#pragma once
#include <string>

#include "cache.h"
#include "Utilities.h"
#include "simulator.h"

// init the simulator and all info
Status initSimulator(CacheConfig& icConfig, CacheConfig& dcConfig, MemoryStore* memory,
                     const std::string& output_name);

// run the simulator for a certain number of cycles
Status runCycles(uint64_t cycles);

// run till halt (call runCycles() with cycles == 1 each time) until
// status tells you to HALT or ERROR out
Status runTillHalt();

// dump the state of the simulator
Status finalizeSimulator();