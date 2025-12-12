#include "cycle.h"

#include <iostream>
#include <string>

#include "Utilities.h"
#include "cache.h"
#include "simulator.h"

static Simulator* simulator = nullptr;
static Cache* iCache = nullptr;
static Cache* dCache = nullptr;
static std::string output;

static uint64_t cycleCount = 0;
static uint64_t loadUseStalls = 0;
static uint64_t PC = 0;

static const uint64_t EXCEPTION_HANDLER_ADDR = 0x8000;

// Cache miss tracking
static bool iMissActive = false;
static int64_t iMissRemaining = 0;
static bool dMissActive = false;
static int64_t dMissRemaining = 0;

Simulator::Instruction nop(StageStatus status) {
    Simulator::Instruction inst;
    inst.instruction = 0x00000013;  // addi x0, x0, 0
    inst.isLegal = true;
    inst.isNop = true;
    inst.status = status;
    return inst;
}

struct PipelineInfo {
    Simulator::Instruction ifInst = nop(IDLE);
    Simulator::Instruction idInst = nop(IDLE);
    Simulator::Instruction exInst = nop(IDLE);
    Simulator::Instruction memInst = nop(IDLE);
    Simulator::Instruction wbInst = nop(IDLE);
};

static PipelineInfo pipelineInfo;

// Check if instruction is valid (not bubble/squashed/idle)
static bool isValidInst(const Simulator::Instruction& inst) {
    return inst.status != SQUASHED && inst.status != BUBBLE && inst.status != IDLE;
}

// Helpers for forwarding detection
static uint64_t forwardValue(const Simulator::Instruction& inst,
                             const Simulator::Instruction& exSrc,
                             const Simulator::Instruction& memSrc,
                             const Simulator::Instruction& wbSrc,
                             uint64_t orig,
                             bool isRs1) {
    uint64_t targetReg = isRs1 ? inst.rs1 : inst.rs2;

    // EX/MEM forwarding (highest priority for non-load)
    if (exSrc.writesRd && exSrc.rd != 0 && exSrc.rd == targetReg && isValidInst(exSrc) &&
        !exSrc.readsMem) {
        return exSrc.arithResult;
    }

    // MEM/WB forwarding
    if (memSrc.writesRd && memSrc.rd != 0 && memSrc.rd == targetReg && isValidInst(memSrc)) {
        return memSrc.readsMem ? memSrc.memResult : memSrc.arithResult;
    }

    // WB forwarding (lowest priority)
    if (wbSrc.writesRd && wbSrc.rd != 0 && wbSrc.rd == targetReg && isValidInst(wbSrc)) {
        return wbSrc.readsMem ? wbSrc.memResult : wbSrc.arithResult;
    }

    return orig;
}

Status initSimulator(CacheConfig& iCacheConfig, CacheConfig& dCacheConfig, MemoryStore* mem,
                     const std::string& output_name) {
    output = output_name;
    simulator = new Simulator();
    simulator->setMemory(mem);
    iCache = new Cache(iCacheConfig, I_CACHE);
    dCache = new Cache(dCacheConfig, D_CACHE);

    PC = 0;
    cycleCount = 0;
    loadUseStalls = 0;
    iMissActive = dMissActive = false;
    iMissRemaining = dMissRemaining = 0;

    pipelineInfo = {};
    pipelineInfo.ifInst = nop(IDLE);
    pipelineInfo.ifInst.PC = 0;
    pipelineInfo.idInst = nop(IDLE);
    pipelineInfo.exInst = nop(IDLE);
    pipelineInfo.memInst = nop(IDLE);
    pipelineInfo.wbInst = nop(IDLE);
    return SUCCESS;
}

Status runCycles(uint64_t cycles) {
    uint64_t executed = 0;
    Status status = SUCCESS;

    while (cycles == 0 || executed < cycles) {
        executed++;

        // Dump pipe state at the beginning of each cycle
        PipeState pipeState{};
        pipeState.cycle = cycleCount;
        pipeState.ifPC = pipelineInfo.ifInst.PC;
        pipeState.ifStatus = pipelineInfo.ifInst.status;
        pipeState.idInstr = pipelineInfo.idInst.instruction;
        pipeState.idStatus = pipelineInfo.idInst.status;
        pipeState.exInstr = pipelineInfo.exInst.instruction;
        pipeState.exStatus = pipelineInfo.exInst.status;
        pipeState.memInstr = pipelineInfo.memInst.instruction;
        pipeState.memStatus = pipelineInfo.memInst.status;
        pipeState.wbInstr = pipelineInfo.wbInst.instruction;
        pipeState.wbStatus = pipelineInfo.wbInst.status;
        dumpPipeState(pipeState, output);

        cycleCount++;

        PipelineInfo old = pipelineInfo;
        PipelineInfo next{nop(BUBBLE), nop(BUBBLE), nop(BUBBLE), nop(BUBBLE), nop(BUBBLE)};

        // Decrement cache miss counters at start of cycle
        if (iMissActive && iMissRemaining > 0) iMissRemaining--;
        if (dMissActive && dMissRemaining > 0) dMissRemaining--;

        // ===== WB Stage =====
        next.wbInst = simulator->simWB(old.memInst);
        if (next.wbInst.isHalt && isValidInst(next.wbInst)) {
            pipelineInfo = next;
            status = HALT;
            break;
        }

        // ===== Detect D-cache stall =====
        bool dMissStall = dMissActive && dMissRemaining > 0;

        // ===== Load-use hazard detection =====
        // Load in EX, dependent instruction in ID
        bool loadUseHazard = false;
        if (old.exInst.readsMem && old.exInst.writesRd && old.exInst.rd != 0 &&
            isValidInst(old.exInst)) {
            if (isValidInst(old.idInst) && !old.idInst.isNop && !old.idInst.isHalt) {
                if ((old.idInst.readsRs1 && old.idInst.rs1 == old.exInst.rd) ||
                    (old.idInst.readsRs2 && old.idInst.rs2 == old.exInst.rd)) {
                    loadUseHazard = true;
                }
            }
        }

        // Count load-use stalls (only when not already stalled by d-cache)
        if (loadUseHazard && !dMissStall) {
            loadUseStalls++;
        }

        bool pipelineStall = loadUseHazard || dMissStall;

        // ===== Exception Detection =====
        // Illegal instruction in ID
        bool illegalTrap = false;
        if (isValidInst(old.idInst) && !old.idInst.isNop && !old.idInst.isHalt &&
            !old.idInst.isLegal) {
            illegalTrap = true;
        }

        // Memory exception in MEM
        bool memTrap = isValidInst(old.memInst) && old.memInst.memException;

        // ===== Handle Memory Exception =====
        if (memTrap) {
            next.memInst = nop(SQUASHED);
            next.exInst = nop(SQUASHED);
            next.idInst = nop(SQUASHED);
            next.ifInst = nop(SQUASHED);
            next.ifInst.PC = EXCEPTION_HANDLER_ADDR;
            PC = EXCEPTION_HANDLER_ADDR;
            iMissActive = dMissActive = false;
            iMissRemaining = dMissRemaining = 0;
            pipelineInfo = next;
            continue;
        }

        // ===== MEM Stage =====
        if (dMissActive) {
            if (dMissRemaining == 0) {
                // D-cache miss resolved
                next.memInst = simulator->simMEM(old.memInst);
                dMissActive = false;
            } else {
                // Still waiting
                next.memInst = old.memInst;
            }
        } else {
            auto memCandidate = old.exInst;

            // Store data forwarding from MEM/WB
            if (memCandidate.writesMem && isValidInst(memCandidate)) {
                // Forward from WB stage for store data
                if (old.wbInst.writesRd && old.wbInst.rd != 0 && old.wbInst.rd == memCandidate.rs2 &&
                    isValidInst(old.wbInst)) {
                    memCandidate.op2Val =
                        old.wbInst.readsMem ? old.wbInst.memResult : old.wbInst.arithResult;
                }
            }

            bool startDMiss = false;
            if (isValidInst(memCandidate) && memCandidate.isLegal &&
                (memCandidate.readsMem || memCandidate.writesMem)) {
                bool hit = dCache->access(memCandidate.memAddress,
                                         memCandidate.writesMem ? CACHE_WRITE : CACHE_READ);
                if (!hit) {
                    startDMiss = true;
                    dMissActive = true;
                    dMissRemaining = static_cast<int64_t>(dCache->config.missLatency);
                    next.memInst = memCandidate;
                }
            }

            if (!startDMiss) {
                next.memInst = simulator->simMEM(memCandidate);
            }
        }

        // ===== EX Stage =====
        if (!pipelineStall && !illegalTrap) {
            auto idInst = old.idInst;

            // Apply forwarding for EX stage
            if (isValidInst(idInst) && !idInst.isNop && !idInst.isHalt) {
                if (idInst.readsRs1) {
                    idInst.op1Val =
                        forwardValue(idInst, old.exInst, old.memInst, old.wbInst, idInst.op1Val, true);
                }
                if (idInst.readsRs2) {
                    idInst.op2Val =
                        forwardValue(idInst, old.exInst, old.memInst, old.wbInst, idInst.op2Val, false);
                }
            }

            next.exInst = simulator->simEX(idInst);
        } else {
            next.exInst = nop(BUBBLE);
        }

        // ===== ID Stage and Branch Resolution =====
        bool branchTaken = false;
        uint64_t branchTarget = 0;
        bool iStall = iMissActive && iMissRemaining > 0;

        if (!pipelineStall && !iStall) {
            auto ifInst = old.ifInst;

            if (isValidInst(ifInst)) {
                ifInst = simulator->simID(ifInst);

                // Handle speculative status - clear when entering ID
                if (ifInst.status == SPECULATIVE) {
                    ifInst.status = NORMAL;
                }

                // Branch/Jump resolution in ID
                if (ifInst.isLegal && !ifInst.isNop && !ifInst.isHalt &&
                    (ifInst.opcode == OP_BRANCH || ifInst.opcode == OP_JALR ||
                     ifInst.opcode == OP_JAL)) {

                    // Apply forwarding for branch operands
                    if (ifInst.readsRs1) {
                        ifInst.op1Val =
                            forwardValue(ifInst, old.exInst, old.memInst, old.wbInst, ifInst.op1Val, true);
                    }
                    if (ifInst.readsRs2) {
                        ifInst.op2Val =
                            forwardValue(ifInst, old.exInst, old.memInst, old.wbInst, ifInst.op2Val, false);
                    }

                    ifInst = simulator->simNextPCResolution(ifInst);

                    if (ifInst.nextPC != ifInst.PC + 4) {
                        branchTaken = true;
                        branchTarget = ifInst.nextPC;
                    }
                    ifInst.status = NORMAL;
                }
            }
            next.idInst = ifInst;
        } else {
            next.idInst = old.idInst;
        }

        // ===== IF Stage =====
        bool fetchBlocked = pipelineStall || dMissStall;

        if (!fetchBlocked) {
            if (iMissActive) {
                if (iMissRemaining == 0) {
                    // I-cache miss just resolved
                    auto fetched = simulator->simIF(PC);

                    bool parentCtrl =
                        (old.idInst.opcode == OP_BRANCH || old.idInst.opcode == OP_JALR ||
                         old.idInst.opcode == OP_JAL) &&
                        isValidInst(old.idInst);
                    fetched.status = parentCtrl ? SPECULATIVE : NORMAL;
                    next.ifInst = fetched;
                    PC = PC + 4;
                    iMissActive = false;
                } else {
                    // Still waiting for I-cache
                    next.ifInst = old.ifInst;
                    next.ifInst.status = BUBBLE;
                }
            } else {
                // Try to fetch
                uint64_t fetchPC = PC;

                bool hit = iCache->access(fetchPC, CACHE_READ);
                if (!hit) {
                    // Start I-cache miss
                    iMissActive = true;
                    iMissRemaining = static_cast<int64_t>(iCache->config.missLatency);
                    next.ifInst = old.ifInst;
                    next.ifInst.status = BUBBLE;
                    next.ifInst.PC = fetchPC;
                } else {
                    // I-cache hit - fetch succeeds
                    auto fetched = simulator->simIF(fetchPC);

                    bool parentCtrl =
                        (old.idInst.opcode == OP_BRANCH || old.idInst.opcode == OP_JALR ||
                         old.idInst.opcode == OP_JAL) &&
                        isValidInst(old.idInst);
                    fetched.status = parentCtrl ? SPECULATIVE : NORMAL;
                    next.ifInst = fetched;
                    PC = fetchPC + 4;
                }
            }
        } else {
            next.ifInst = old.ifInst;
        }

        // ===== Handle Branch Taken =====
        if (branchTaken) {
            PC = branchTarget;
            next.ifInst = nop(SQUASHED);
            next.ifInst.PC = branchTarget;
            // Cancel I-cache miss on wrong path
            iMissActive = false;
            iMissRemaining = 0;
        }

        // ===== Handle Illegal Instruction =====
        if (illegalTrap) {
            next.idInst = nop(SQUASHED);
            next.exInst = nop(SQUASHED);
            next.ifInst = nop(SQUASHED);
            next.ifInst.PC = EXCEPTION_HANDLER_ADDR;
            PC = EXCEPTION_HANDLER_ADDR;
            iMissActive = false;
            iMissRemaining = 0;
        }

        pipelineInfo = next;
    }

    return status;
}

Status runTillHalt() {
    Status status;
    while (true) {
        status = static_cast<Status>(runCycles(1));
        if (status == HALT) break;
    }
    return status;
}

Status finalizeSimulator() {
    simulator->dumpRegMem(output);
    SimulationStats stats{simulator->getDin(),
                          cycleCount,
                          iCache->getHits(),
                          iCache->getMisses(),
                          dCache->getHits(),
                          dCache->getMisses(),
                          loadUseStalls};
    dumpSimStats(stats, output);
    return SUCCESS;
}
