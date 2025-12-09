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

static uint64_t PC = 0;
static const uint64_t EXCEPTION_HANDLER_ADDR = 0x8000;
static int branchStallCounter = 0;

Simulator::Instruction nop(StageStatus status) {
    Simulator::Instruction inst;
    inst.instruction = 0x00000013;
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
    return SUCCESS;
}

// Helpers for forwarding detection
static bool dependsOn(const Simulator::Instruction& consumer, const Simulator::Instruction& producer) {
    if (!producer.writesRd || producer.rd == 0) return false;
    return (consumer.readsRs1 && consumer.rs1 == producer.rd) ||
           (consumer.readsRs2 && consumer.rs2 == producer.rd);
}

static uint64_t forwardValue(const Simulator::Instruction& inst, const Simulator::Instruction& memSrc,
                             const Simulator::Instruction& wbSrc, uint64_t orig, bool isRs1) {
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

Status runCycles(uint64_t cycles) {
    uint64_t count = 0;
    auto status = SUCCESS;
    PipeState pipeState{};

    while (cycles == 0 || count < cycles) {
        pipeState.cycle = cycleCount;
        count++;
        cycleCount++;

        PipelineInfo old = pipelineInfo;
        PipelineInfo next{nop(BUBBLE), nop(BUBBLE), nop(BUBBLE), nop(BUBBLE), nop(BUBBLE)};

        // Decrement outstanding miss counters
        if (iMissActive && iMissRemaining > 0) iMissRemaining--;
        if (dMissActive && dMissRemaining > 0) dMissRemaining--;

        bool iMissStall = iMissActive && iMissRemaining > 0;
        bool dMissStall = dMissActive && dMissRemaining > 0;

        // Writeback stage (old MEM -> WB)
        next.wbInst = simulator->simWB(old.memInst);
        if (next.wbInst.isHalt) {
            pipelineInfo = next;
            status = HALT;
            break;
        }

        // Exception detection
        bool illegalTrap = (old.idInst.status == NORMAL && !old.idInst.isNop && !old.idInst.isHalt &&
                            !old.idInst.isLegal);
        bool memTrap = (old.memInst.status == NORMAL && old.memInst.memException);
        bool exceptionTrap = illegalTrap || memTrap;

        if (memTrap) {
            // Squash younger, redirect PC
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

        // Hazard detection
        bool loadUseHazard = false;
        if (old.exInst.readsMem && old.exInst.writesRd && old.exInst.rd != 0) {
            if (old.idInst.readsRs1 && old.idInst.rs1 == old.exInst.rd) loadUseHazard = true;
            if (old.idInst.readsRs2 && old.idInst.rs2 == old.exInst.rd && !old.idInst.writesMem)
                loadUseHazard = true;
        }
        if (loadUseHazard) loadUseStalls++;
        bool branchInID = (old.idInst.opcode == OP_BRANCH || old.idInst.opcode == OP_JALR);
        if (branchStallCounter > 0) branchStallCounter--;
        bool branchStall = branchStallCounter > 0;
        if (!branchStall && branchInID) {
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

        bool stallIDEX = loadUseHazard || branchStall || dMissStall || dMissActive;

        // MEM stage movement and D-cache miss handling
        if (dMissStall) {
            next.memInst = old.memInst;
        } else if (dMissActive && dMissRemaining == 0) {
            // Miss has completed, perform memory access now
            auto completed = simulator->simMEM(old.memInst);
            dMissActive = false;
            next.memInst = completed;
        } else {
            // Bring instruction from EX into MEM if available
            auto exInst = old.exInst;
            // Forward store data (rs2) from later stages
            if (exInst.writesMem) {
                exInst.op2Val = forwardValue(exInst, old.memInst, old.wbInst, exInst.op2Val, false);
            }
            exInst = simulator->simEX(exInst);

            bool accessMem = exInst.readsMem || exInst.writesMem;
            if (accessMem && exInst.status == NORMAL && exInst.isLegal && !exInst.isNop) {
                bool hit = dCache->access(exInst.memAddress, exInst.writesMem ? CACHE_WRITE : CACHE_READ);
                if (!hit) {
                    dMissActive = true;
                    dMissRemaining = static_cast<int64_t>(dCache->config.missLatency);
                    next.memInst = exInst;  // hold in MEM
                } else {
                    exInst = simulator->simMEM(exInst);
                    next.memInst = exInst;
                }
            } else {
                next.memInst = simulator->simMEM(exInst);
            }
        }

        // EX stage advancement (ID -> EX)
        if (!stallIDEX && !exceptionTrap) {
            auto idInst = old.idInst;
            if (idInst.readsRs1)
                idInst.op1Val = forwardValue(idInst, old.memInst, old.wbInst, idInst.op1Val, true);
            if (idInst.readsRs2)
                idInst.op2Val = forwardValue(idInst, old.memInst, old.wbInst, idInst.op2Val, false);
            next.exInst = simulator->simEX(idInst);
        } else {
            next.exInst = nop(BUBBLE);
        }

        // ID stage advancement (IF -> ID)
        bool allowIDAdvance = !stallIDEX && !exceptionTrap && !iMissStall && !dMissStall && !iMissActive;
        if (illegalTrap) allowIDAdvance = false;

        // Branch redirect bookkeeping
        bool branchTaken = false;
        uint64_t branchTarget = 0;

        if (allowIDAdvance) {
            auto ifInst = old.ifInst;
            ifInst = simulator->simID(ifInst);

            if (ifInst.isLegal && !ifInst.isNop && !ifInst.isHalt &&
                (ifInst.opcode == OP_BRANCH || ifInst.opcode == OP_JAL || ifInst.opcode == OP_JALR)) {
                // Compute branch target in ID with forwarding
                if (ifInst.readsRs1)
                    ifInst.op1Val = forwardValue(ifInst, old.memInst, old.wbInst, ifInst.op1Val, true);
                if (ifInst.readsRs2)
                    ifInst.op2Val = forwardValue(ifInst, old.memInst, old.wbInst, ifInst.op2Val, false);
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

        // IF stage fetch
        bool fetchBlocked = stallIDEX || exceptionTrap || iMissStall || dMissStall || iMissActive;
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
                fetched.status = NORMAL;
                next.ifInst = fetched;
                // Do not advance PC until miss resolves
            } else {
                fetched.status = (fetched.opcode == OP_BRANCH || fetched.opcode == OP_JAL ||
                                  fetched.opcode == OP_JALR)
                                     ? SPECULATIVE
                                     : NORMAL;
                next.ifInst = fetched;
                PC += 4;
            }
        } else {
            next.ifInst = old.ifInst;
        }

        // Resolve branch redirect after fetch decision
        if (branchTaken) {
            PC = branchTarget;
            next.ifInst = nop(SQUASHED);
        }

        // Handle illegal instruction trap
        if (illegalTrap) {
            next.idInst = nop(SQUASHED);
            next.exInst = nop(SQUASHED);
            next.ifInst = nop(SQUASHED);
            PC = EXCEPTION_HANDLER_ADDR;
        }

        // Release I-cache miss when counter expired
        if (iMissActive && !iMissStall) {
            iMissActive = false;
            PC += 4;
        }

        pipelineInfo = next;
    }

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
