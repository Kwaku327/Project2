#include "funct.h"

#include <iostream>

#include "cache.h"
#include "Utilities.h"
#include "simulator.h"

static Simulator* simulator = nullptr;
static std::string output;
static uint64_t PC = 0;

// initialize the simulator
Status initSimulator(MemoryStore* mem, const std::string& output_name) {
    output = output_name;
    simulator = new Simulator();
    simulator->setMemory(mem);
    return SUCCESS;
}

// run the simulator for a certain number of intructions
// return SUCCESS if count of executed instructions == desired intructions.
// return HALT if the simulator halts on 0xfeedfeed
Status runInstructions(uint64_t instructions) {
    uint64_t numInstructions = 0;
    auto status = SUCCESS;

    while (instructions == 0 || numInstructions < instructions) {

        Simulator::Instruction inst = simulator->simInstruction(PC);

        numInstructions += 1;
        PC = inst.nextPC;

        if (inst.isHalt) {
            status = HALT;
            break;
        }

        if (!inst.isLegal) {
            status = ERROR;
            break;
        }
    }
    return status;
}

// run till halt (call runInstructions() with instructions == 1 each time) until
// status tells you to HALT or ERROR out
Status runTillHalt() {
    Status status;
    while (true) {
        status = static_cast<Status>(runInstructions(1));
        if (status == HALT || status == ERROR) break;
    }
    return status;
}

// dump the stats of the simulator
Status finalizeSimulator() {
    simulator->dumpRegMem(output);
    SimulationStats stats{simulator->getDin(), 0,};
    dumpSimStats(stats, output);
    return SUCCESS;
}
