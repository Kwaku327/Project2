#include "cycle.h"

#include <iostream>
#include <memory>
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
static uint64_t loadUseStalls = 0;

static uint64_t PC = 0;
static const uint64_t EXCEPTION_HANDLER_ADDR = 0x8000;
static int branchStallCounter = 0;
static const uint64_t EXCEPTION_HANDLER_ADDR = 0x8000;
static int branchStallCounter = 0;

Simulator::Instruction nop(StageStatus status) {
    Simulator::Instruction inst;
    inst.instruction = 0x00000013;
    inst.isLegal = true;
    inst.isNop = true;
    inst.status = status;
    return inst;
    Simulator::Instruction inst;
    inst.instruction = 0x00000013;
    inst.isLegal = true;
    inst.isNop = true;
    inst.status = status;
    return inst;
}

struct PipelineInfo {
struct PipelineInfo {
    Simulator::Instruction ifInst = nop(IDLE);
    Simulator::Instruction idInst = nop(IDLE);
    Simulator::Instruction exInst = nop(IDLE);
    Simulator::Instruction memInst = nop(IDLE);
    Simulator::Instruction wbInst = nop(IDLE);
};

static PipelineInfo pipelineInfo;
};

static PipelineInfo pipelineInfo;

// Cache miss tracking
static bool iMissActive = false;
static int64_t iMissRemaining = 0;
static bool dMissActive = false;
static int64_t dMissRemaining = 0;
// Cache miss tracking
static bool iMissActive = false;
static int64_t iMissRemaining = 0;
static bool dMissActive = false;
static int64_t dMissRemaining = 0;

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
    branchStallCounter = 0;
    iMissActive = dMissActive = false;
    iMissRemaining = dMissRemaining = 0;
    pipelineInfo = {};
    pipelineInfo.ifInst = nop(IDLE);
    pipelineInfo.idInst = nop(IDLE);
    pipelineInfo.exInst = nop(IDLE);
    pipelineInfo.memInst = nop(IDLE);
    pipelineInfo.wbInst = nop(IDLE);
    return SUCCESS;
}

// Helpers for forwarding detection
static bool dependsOn(const Simulator::Instruction& consumer, const Simulator::Instruction& producer) {
    if (!producer.writesRd || producer.rd == 0) return false;
    return (consumer.readsRs1 && consumer.rs1 == producer.rd) ||
           (consumer.readsRs2 && consumer.rs2 == producer.rd);
}

static uint64_t forwardValue(const Simulator::Instruction& inst,
                             const Simulator::Instruction& exSrc,
                             const Simulator::Instruction& memSrc,
                             const Simulator::Instruction& wbSrc,
                             uint64_t orig,
                             bool isRs1) {
    auto matches = [&](const Simulator::Instruction& src) {
        return src.writesRd && src.rd != 0 &&
               ((isRs1 && inst.rs1 == src.rd) || (!isRs1 && inst.rs2 == src.rd));
    };
    if (matches(exSrc)) {
        return exSrc.readsMem ? exSrc.memResult : exSrc.arithResult;
    }
    if (matches(memSrc)) {
        return memSrc.readsMem ? memSrc.memResult : memSrc.arithResult;
    }
    if (matches(wbSrc)) {
        return wbSrc.readsMem ? wbSrc.memResult : wbSrc.arithResult;
    }
    return orig;
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

        // Outstanding miss counters tick down
        if (iMissActive && iMissRemaining > 0) iMissRemaining--;
        if (dMissActive && dMissRemaining > 0) dMissRemaining--;

        bool iMissStall = iMissActive && iMissRemaining > 0;
        bool dMissStall = dMissActive && dMissRemaining > 0;

        // Writeback
        next.wbInst = simulator->simWB(old.memInst);
        if (next.wbInst.isHalt) {
            pipelineInfo = next;
            status = HALT;
            break;
        }

        // Exception checks
        bool illegalTrap =
            (old.idInst.status == NORMAL && !old.idInst.isNop && !old.idInst.isHalt && !old.idInst.isLegal);
        bool memTrap = (old.memInst.status == NORMAL && old.memInst.memException);
        if (memTrap) {
            // Flush younger and redirect
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

        // Branch stall bookkeeping
        if (branchStallCounter > 0) branchStallCounter--;

        // Hazards: load-use
        bool loadUseHazard = false;
        if (old.exInst.readsMem && old.exInst.writesRd && old.exInst.rd != 0) {
            if (old.idInst.readsRs1 && old.idInst.rs1 == old.exInst.rd) loadUseHazard = true;
            if (old.idInst.readsRs2 && old.idInst.rs2 == old.exInst.rd && !old.idInst.writesMem)
                loadUseHazard = true;
        }
        if (loadUseHazard) loadUseStalls++;

        // Branch stall detection (branch/jalr need operand forwarding)
        bool branchStall = branchStallCounter > 0;
        bool idIsBranch = (old.idInst.opcode == OP_BRANCH || old.idInst.opcode == OP_JALR);
        if (!branchStall && idIsBranch) {
            if (dependsOn(old.idInst, old.exInst)) {
                branchStallCounter = old.exInst.readsMem ? 2 : 1;
                branchStall = true;
                if (old.exInst.readsMem) loadUseStalls++;
            } else if (dependsOn(old.idInst, old.memInst) && old.memInst.readsMem) {
                branchStallCounter = 1;
                branchStall = true;
                loadUseStalls++;
            }
        }

        // MEM stage
        if (dMissActive) {
            if (dMissRemaining == 0) {
                // Miss resolved this cycle
                next.memInst = simulator->simMEM(old.memInst);
                dMissActive = false;
            } else {
                next.memInst = old.memInst;
            }
        } else {
            auto memCandidate = old.exInst;
            if (memCandidate.writesMem) {
                memCandidate.op2Val =
                    forwardValue(memCandidate, old.exInst, old.memInst, old.wbInst, memCandidate.op2Val, false);
            }

            bool startMiss = false;
            if (memCandidate.status == NORMAL && memCandidate.isLegal &&
                (memCandidate.readsMem || memCandidate.writesMem)) {
                bool hit =
                    dCache->access(memCandidate.memAddress, memCandidate.writesMem ? CACHE_WRITE : CACHE_READ);
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

        bool pipelineStall = loadUseHazard || branchStall || dMissStall || dMissActive;

        // EX stage (only when not stalled by hazards/mem miss)
        if (!pipelineStall && !illegalTrap) {
            auto idInst = old.idInst;
            if (idInst.readsRs1)
                idInst.op1Val = forwardValue(idInst, old.exInst, old.memInst, old.wbInst, idInst.op1Val, true);
            if (idInst.readsRs2)
                idInst.op2Val = forwardValue(idInst, old.exInst, old.memInst, old.wbInst, idInst.op2Val, false);
            next.exInst = simulator->simEX(idInst);
        } else {
            next.exInst = nop(BUBBLE);
        }

        // ID stage (consume IF unless stalled)
        bool allowID = !pipelineStall && !iMissStall && !iMissActive;
        bool branchTaken = false;
        uint64_t branchTarget = 0;
        if (allowID) {
            auto ifInst = simulator->simID(old.ifInst);
            // Forward for branch/jalr resolution
            if (ifInst.isLegal && (ifInst.opcode == OP_BRANCH || ifInst.opcode == OP_JALR ||
                                   ifInst.opcode == OP_JAL)) {
                if (ifInst.readsRs1)
                    ifInst.op1Val = forwardValue(ifInst, old.exInst, old.memInst, old.wbInst, ifInst.op1Val, true);
                if (ifInst.readsRs2)
                    ifInst.op2Val = forwardValue(ifInst, old.exInst, old.memInst, old.wbInst, ifInst.op2Val, false);
                ifInst = simulator->simNextPCResolution(ifInst);
                if (ifInst.nextPC != ifInst.PC + 4) {
                    branchTaken = true;
                    branchTarget = ifInst.nextPC;
                }
            }
            next.idInst = ifInst;
        } else {
            next.idInst = old.idInst;
        }

        // IF stage
        bool fetchBlocked = pipelineStall || iMissStall || dMissStall || iMissActive;
        if (!fetchBlocked) {
            auto fetched = simulator->simIF(PC);
            bool iMiss = false;
            if (fetched.status != IDLE) {
                bool hit = iCache->access(PC, CACHE_READ);
                iMiss = !hit;
            }
            if (iMiss) {
                iMissActive = true;
                iMissRemaining = static_cast<int64_t>(iCache->config.missLatency);
                next.ifInst = fetched;
            } else {
                bool isCtrl = (fetched.opcode == OP_BRANCH || fetched.opcode == OP_JAL || fetched.opcode == OP_JALR);
                fetched.status = isCtrl ? SPECULATIVE : NORMAL;
                next.ifInst = fetched;
                PC += 4;
            }
        } else {
            next.ifInst = old.ifInst;
        }

        // Branch redirect
        if (branchTaken) {
            PC = branchTarget;
            iMissActive = false;
            iMissRemaining = 0;
            next.ifInst = nop(SQUASHED);
        } else if (next.ifInst.status == SPECULATIVE) {
            next.ifInst.status = NORMAL;
        }

        if (illegalTrap) {
            next.idInst = nop(SQUASHED);
            next.exInst = nop(SQUASHED);
            next.ifInst = nop(SQUASHED);
            PC = EXCEPTION_HANDLER_ADDR;
        }

        // I-cache miss completes
        if (iMissActive && !iMissStall) {
            iMissActive = false;
            PC += 4;
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
