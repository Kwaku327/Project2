#include "cache.h"
#include <cmath>

using namespace std;

// Constructor definition
Cache::Cache(CacheConfig configParam, CacheDataType cacheType)
    : hits(0),
      misses(0),
      type(cacheType),
      config(configParam) {
    // Pre-compute basic geometry.
    blockOffsetBits = static_cast<uint64_t>(std::log2(config.blockSize));
    numSets = config.cacheSize / (config.blockSize * config.ways);
    setIndexBits = static_cast<uint64_t>(std::log2(numSets));
    setIndexMask = (1ULL << setIndexBits) - 1;

    sets.resize(numSets, std::vector<Line>(config.ways));
}

// Access method definition
bool Cache::access(uint64_t address, CacheOperation /*readWrite*/) {
    uint64_t setIndex = getSetIndex(address);
    uint64_t tag = getTag(address);

    auto& set = sets.at(setIndex);
    // First, search for a hit.
    for (auto& line : set) {
        if (line.valid && line.tag == tag) {
            hits++;
            line.lastUsed = ++accessCounter;
            return true;
        }
    }

    // Miss path: choose a victim using true LRU (oldest lastUsed or invalid).
    misses++;
    auto* victim = &set.front();
    for (auto& line : set) {
        if (!line.valid) {
            victim = &line;
            break;
        }
        if (line.lastUsed < victim->lastUsed) {
            victim = &line;
        }
    }

    victim->valid = true;
    victim->tag = tag;
    victim->lastUsed = ++accessCounter;
    return false;
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
