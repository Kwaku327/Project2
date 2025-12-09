// Sample cache implementation with random hit return
// TODO: Modify this file to model an LRU cache as in the project description

#include "cache.h"
#include <random>

using namespace std;

// Random generator for cache hit/miss simulation
static std::mt19937 generator(42);  // Fixed seed for deterministic results
std::uniform_real_distribution<double> distribution(0.0, 1.0);

// Constructor definition
Cache::Cache(CacheConfig configParam, CacheDataType cacheType) : config(configParam) {
    // Here you can initialize other cache-specific attributes
    // For instance, if you had cache tables or other structures, initialize them here
}

// Access method definition
bool Cache::access(uint64_t address, CacheOperation readWrite) {
    // For simplicity, we're using a random boolean to simulate cache hit/miss
    bool hit = distribution(generator) < 0.20;  // random 20% hit for a strange cache
    hits += hit;
    misses += !hit;
    return hit;
}

// debug: dump information as you needed, here are some examples
Status Cache::dump(const std::string& base_output_name) {
    ofstream cache_out(base_output_name + "_cache_state.out");
    if (cache_out) {
        cache_out << "---------------------" << endl;
        cache_out << "Begin Register Values" << endl;
        cache_out << "---------------------" << endl;
        cache_out << "Cache Configuration:" << std::endl;
        cache_out << "Size: " << config.cacheSize << " bytes" << std::endl;
        cache_out << "Block Size: " << config.blockSize << " bytes" << std::endl;
        cache_out << "Ways: " << (config.ways == 1) << std::endl;
        cache_out << "Miss Latency: " << config.missLatency << " cycles" << std::endl;
        cache_out << "---------------------" << endl;
        cache_out << "End Register Values" << endl;
        cache_out << "---------------------" << endl;
        return SUCCESS;
    } else {
        cerr << LOG_ERROR << "Could not create cache state dump file" << endl;
        return ERROR;
    }
}
