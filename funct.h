#pragma once
#include <string>

#include "Utilities.h"
#include "simulator.h"

// init the simulator and all info
Status initSimulator(MemoryStore* memory, const std::string& output_name);

// run the simulator for a certain number of instructions
Status runInstructions(uint64_t instructions);

// run till halt (call runInstructions() with instructions == 1 each time) until
// status tells you to HALT or ERROR out
Status runTillHalt();

// dump the state of the simulator
Status finalizeSimulator();
