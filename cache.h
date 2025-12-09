#pragma once
#include <inttypes.h>
#include <iostream>
#include <vector>
#include "Utilities.h"

struct CacheConfig {
    // Cache size in bytes.
    uint64_t cacheSize;
    // Cache block size in bytes.
    uint64_t blockSize;
    // Type of cache: set associativity 
    uint64_t ways;
    // Additional miss latency in cycles.
    uint64_t missLatency;
    // debug: Overload << operator to allow easy printing of CacheConfig
    friend std::ostream& operator<<(std::ostream& os, const CacheConfig& config) {
        os << "CacheConfig { " << config.cacheSize << ", " << config.blockSize << ", "
           << config.ways << ", " << config.missLatency << " }";
        return os;
    }
};

enum CacheDataType { I_CACHE = false, D_CACHE = true };
enum CacheOperation { CACHE_READ = false, CACHE_WRITE = true };

class Cache {
private:
    uint64_t hits, misses;    
    CacheDataType type;

public:
    CacheConfig config;
    // Constructor to initialize the cache parameters
    Cache(CacheConfig configParam, CacheDataType cacheType);

    /** Access methods for reading/writing
     * @return true for hit and false for miss
     * @param
     *      address: memory address
     *      readWrite: true for read operation and false for write operation
     */
    bool access(uint64_t address, CacheOperation readWrite);

    // debug: dump information as you needed
    Status dump(const std::string& base_output_name);

    // TODO: You may add more methods and fields as needed

    uint64_t getHits() { return hits; }
    uint64_t getMisses() { return misses; }
};
