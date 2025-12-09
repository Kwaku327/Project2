#pragma once
#include <inttypes.h>

#include <string>
#include <vector>

// The memory is 64 KB large.
#define MEMORY_SIZE 0x10000

#define BYTE_SHIFT 8
#define BYTE_WIDTH 2
#define WORD_WIDTH 8

// The various sizes at which you can manipulate the memory.
enum MemEntrySize { BYTE_SIZE = 1, HALF_SIZE = 2, WORD_SIZE = 4, DOUBLE_SIZE = 8 };

// A memory abstraction interface. Allows values to be set and retrieved at a number of
// different size granularities. The implementation is also capable of printing out memory
// values over a given address range.
class MemoryStore {
   private:
    uint64_t startAddr;
    std::vector<uint8_t> memArr;

    int getOrSetValue(bool get, uint64_t address, uint64_t& value, MemEntrySize size);

   public:
    MemoryStore(uint64_t startAddr, uint64_t numEntries);
    MemoryStore(uint64_t startAddr, uint64_t numEntries, const char* fileName);
    ~MemoryStore(){};

    int loadFromFile(const char* fileName);
    int getMemValue(uint64_t address, uint64_t& value, MemEntrySize size);
    int setMemValue(uint64_t address, uint64_t value, MemEntrySize size);
    int printMemory(uint64_t startAddress, uint64_t endAddress);
    int printMemArray(uint64_t startAddr, uint64_t endAddr, uint64_t entrySize,
                      uint64_t entriesPerRow, std::ostream& out_stream);
};

// Creates a memory store.
// extern MemoryStore *createMemoryStore();

// Dumps the section of memory relevant for the test.
void dumpMemoryState(MemoryStore* mem, const std::string& base_output_name);

int prepareMemory(MemoryStore* mem);
