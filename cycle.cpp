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

// Helpers for forwarding detection (only from EX/MEM and MEM/WB)
static uint64_t forwardValue(const Simulator::Instruction& inst,
                             const Simulator::Instruction& memSrc,
                             const Simulator::Instruction& wbSrc,
                             uint64_t orig,
                             bool isRs1) {
    auto matches = [&](const Simulator::Instruction& src) {
        return src.writesRd && src.rd != 0 &&
               ((isRs1 && inst.rs1 == src.rd) || (!isRs1 && inst.rs2 == src.rd));
    };

    if (matches(memSrc)) {
        return memSrc.readsMem ? memSrc.memResult : memSrc.arithResult;
    }
    if (matches(wbSrc)) {
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
    pipelineInfo.ifInst = nop(NORMAL);
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

        if (iMissActive && iMissRemaining > 0) iMissRemaining--;
        if (dMissActive && dMissRemaining > 0) dMissRemaining--;
        if (iMissActive && iMissRemaining == 0) iMissActive = false;

        // Writeback happens first in conceptual pipeline
        next.wbInst = simulator->simWB(old.memInst);
        if (next.wbInst.isHalt) {
            pipelineInfo = next;
            status = HALT;
            break;
        }

        // Exceptions: illegal in ID, memory in MEM
        bool illegalTrap =
            (old.idInst.status == NORMAL && !old.idInst.isNop && !old.idInst.isHalt &&
             !old.idInst.isLegal);
        bool memTrap = (old.memInst.status == NORMAL && old.memInst.memException);
        if (memTrap) {
            next.memInst = nop(SQUASHED);
            next.exInst = nop(SQUASHED);
            next.idInst = nop(SQUASHED);
            next.ifInst = nop(SQUASHED);
            PC = EXCEPTION_HANDLER_ADDR;
            iMissActive = dMissActive = false;
            iMissRemaining = dMissRemaining = 0;
            pipelineInfo = next;
            continue;
        }

        // Hazards (kinda quick + dirty check)
        bool loadUseHazard = (old.exInst.readsMem && old.exInst.writesRd && old.exInst.rd != 0 &&
                              ((old.idInst.readsRs1 && old.idInst.rs1 == old.exInst.rd) ||
                               (old.idInst.readsRs2 && old.idInst.rs2 == old.exInst.rd)));

        if (loadUseHazard) loadUseStalls++;

        bool dMissStall = dMissActive && dMissRemaining > 0;
        bool pipelineStall = loadUseHazard || dMissStall || dMissActive;

        // MEM stage
        if (dMissActive) {
            if (dMissRemaining == 0) {
                next.memInst = simulator->simMEM(old.memInst);
                dMissActive = false;
            } else {
                next.memInst = old.memInst;
            }
        } else {
            auto memCandidate = old.exInst;
            if (memCandidate.writesMem) {
                // store data might need late forwarding, hope thats ok
                memCandidate.op2Val =
                    forwardValue(memCandidate, old.memInst, old.wbInst, memCandidate.op2Val, false);
            }

            bool startMiss = false;
            if (memCandidate.status == NORMAL && memCandidate.isLegal &&
                (memCandidate.readsMem || memCandidate.writesMem)) {
                bool hit =
                    dCache->access(memCandidate.memAddress,
                                   memCandidate.writesMem ? CACHE_WRITE : CACHE_READ);
                if (!hit) {
                    startMiss = true;
                    dMissActive = true;
                    dMissRemaining = static_cast<int64_t>(dCache->config.missLatency);
                    next.memInst = memCandidate;
                }
            }

            if (!startMiss) {
                next.memInst = simulator->simMEM(memCandidate);
            }
        }

        // EX stage
        if (!pipelineStall && !illegalTrap) {
            auto idInst = old.idInst;
            if (idInst.readsRs1)
                idInst.op1Val =
                    forwardValue(idInst, old.memInst, old.wbInst, idInst.op1Val, true);
            if (idInst.readsRs2)
                idInst.op2Val =
                    forwardValue(idInst, old.memInst, old.wbInst, idInst.op2Val, false);
            if (idInst.opcode == OP_BRANCH) {
                next.exInst = nop(BUBBLE);
            } else {
                next.exInst = simulator->simEX(idInst);
            }
        } else {
            next.exInst = nop(BUBBLE);
        }

        // ID stage
        bool iStall = iMissActive && iMissRemaining > 0;
        bool allowID = !pipelineStall && !iStall;
        bool branchTaken = false;
        bool ctrlResolved = false;
        uint64_t branchTarget = 0;

        if (allowID) {
            auto ifInst = simulator->simID(old.ifInst);
            if (ifInst.status == SPECULATIVE) {
                ifInst.status = NORMAL;  // branch that made it speculative is done now
            }

            if (ifInst.isLegal && (ifInst.opcode == OP_BRANCH || ifInst.opcode == OP_JALR ||
                                   ifInst.opcode == OP_JAL)) {
                if (ifInst.readsRs1)
                    ifInst.op1Val =
                        forwardValue(ifInst, old.memInst, old.wbInst, ifInst.op1Val, true);
                if (ifInst.readsRs2)
                    ifInst.op2Val =
                        forwardValue(ifInst, old.memInst, old.wbInst, ifInst.op2Val, false);
                ifInst = simulator->simNextPCResolution(ifInst);
                ctrlResolved = true;
                if (ifInst.nextPC != ifInst.PC + 4) {
                    branchTaken = true;
                    branchTarget = ifInst.nextPC;
                }
                ifInst.status = NORMAL;
            }
            next.idInst = ifInst;
        } else {
            next.idInst = old.idInst;
        }

        // IF stage
        bool fetchBlocked = pipelineStall || iStall || dMissStall;
        if (!fetchBlocked) {
            uint64_t fetchPC = PC;
            auto fetched = simulator->simIF(fetchPC);
            bool startMiss = false;
            bool parentCtrl = (old.idInst.opcode == OP_BRANCH || old.idInst.opcode == OP_JALR ||
                               old.idInst.opcode == OP_JAL);

            if (fetched.status != IDLE) {
                bool hit = iCache->access(fetchPC, CACHE_READ);
                if (!hit) {
                    startMiss = true;
                    iMissActive = true;
                    iMissRemaining = static_cast<int64_t>(iCache->config.missLatency);
                    fetched.status = parentCtrl ? SPECULATIVE : fetched.status;
                    next.ifInst = fetched;
                    PC = fetchPC + 4;
                }
            }

            if (!startMiss) {
                fetched.status = parentCtrl ? SPECULATIVE : NORMAL;
                next.ifInst = fetched;
                PC = fetchPC + 4;
            }
        } else {
            next.ifInst = old.ifInst;
        }

        // Branch redirect or speculation cleanup
        if (branchTaken) {
            PC = branchTarget;
            next.ifInst = nop(SQUASHED);
            iMissActive = false;
            iMissRemaining = 0;
        } else if (ctrlResolved && next.ifInst.status == SPECULATIVE) {
            next.ifInst.status = NORMAL;
        }

        if (illegalTrap) {
            next.idInst = nop(SQUASHED);
            next.exInst = nop(SQUASHED);
            next.ifInst = nop(SQUASHED);
            PC = EXCEPTION_HANDLER_ADDR;
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
