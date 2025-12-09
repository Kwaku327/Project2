#include "MemoryStore.h"

#include <cassert>
#include <fstream>
#include <iomanip>
#include <iostream>

#include "Utilities.h"

MemoryStore::MemoryStore(uint64_t startAddr, uint64_t numEntries)
    : startAddr(startAddr) {
    // fill memory array with 0s
    memArr.resize(numEntries);
    std::fill(memArr.begin(), memArr.end(), 0);

    // If we can't initialise memory appropriately, don't return a
    // MemoryStore at all.
    assert((prepareMemory(this) == 0));
}

MemoryStore::MemoryStore(uint64_t startAddr, uint64_t numEntries, const char *fileName)
    : startAddr(startAddr) {
    // fill memory array with 0s
    memArr.resize(numEntries);
    std::fill(memArr.begin(), memArr.end(), 0);

    // If we can't initialise memory appropriately, don't return a
    // MemoryStore at all.
    assert((prepareMemory(this) == 0));

    loadFromFile(fileName);
}

int prepareMemory(MemoryStore *mem) {
    std::ifstream initMem;
    initMem.open("init_mem_image", std::ios::in);

    // For tests that don't require such an initial memory image, nothing is done.
    while (initMem && mem) {
        uint32_t curVal = 0;
        uint32_t addr = 0;

        initMem >> std::hex >> addr;
        initMem >> std::hex >> curVal;

        int ret = mem->setMemValue(addr, curVal, WORD_SIZE);

        if (ret) {
            std::cout << LOG_ERROR << "Could not set initial memory value!" << std::endl;
            return -EINVAL;
        }
    }

    return 0;
}

int MemoryStore::getOrSetValue(bool get, uint64_t address, uint64_t &value, MemEntrySize size) {
    uint64_t byteSize = static_cast<uint64_t>(size);

    switch (size) {
        case BYTE_SIZE:
        case HALF_SIZE:
        case WORD_SIZE:
        case DOUBLE_SIZE:
            break;
        default:
            std::cerr << LOG_ERROR << "Invalid size passed, cannot read/write memory" << std::endl;
            return -EINVAL;
    }

    uint64_t relativeAddr = address - startAddr;
    try {
        if (get) {
            value = 0;
            for (uint64_t i = 0; i < byteSize; ++i) {
                value |= ((uint64_t)memArr.at(relativeAddr + i) << (i * 8));
            }
        } else {
            for (uint64_t i = 0; i < byteSize; ++i) {
                memArr.at(relativeAddr + i) = (value >> (i * 8)) & 0xFF;
            }
        }
    } catch (const std::out_of_range &e) {
        std::cerr << LOG_ERROR << "Access violation at address 0x" << std::hex << address << std::endl;
        return -EINVAL;
    }

    return 0;
}

int MemoryStore::getMemValue(uint64_t address, uint64_t &value, MemEntrySize size) {
    return getOrSetValue(true, address, value, size);
}

int MemoryStore::setMemValue(uint64_t address, uint64_t value, MemEntrySize size) {
    return getOrSetValue(false, address, value, size);
}

int MemoryStore::loadFromFile(const char *fileName) {
    // Open instruction file
    std::ifstream infile(fileName, std::ios::binary | std::ios::in);

    if (infile) {
        // Get length of the file and read instruction file into a buffer
        infile.seekg(0, std::ios::end);
        int length = infile.tellg();
        infile.seekg(0, std::ios::beg);

        char *buf = new char[length];
        infile.read(buf, length);
        infile.close();

        // Initialize memory store with buffer contents
        int memLength = length / sizeof(buf[0]);
        int i;
        for (i = 0; i < memLength; i++) {
            this->setMemValue(i * BYTE_SIZE, buf[i], BYTE_SIZE);
        }
        return SUCCESS;
    } else {
        std::cerr << LOG_ERROR << "Unable to open memory file " << fileName << std::endl;
        return ERROR;
    }
}

int MemoryStore::printMemArray(uint64_t startAddr, uint64_t endAddr, uint64_t entrySize,
                               uint64_t entriesPerRow, std::ostream &out_stream) {
    // Validate the entry size
    switch (entrySize) {
        case BYTE_SIZE:
        case HALF_SIZE:
        case WORD_SIZE:
        case DOUBLE_SIZE:
            break;
        default:
            std::cerr << LOG_ERROR << "Invalid print size passed, cannot print memory" << std::endl;
            return -EINVAL;
    }

    uint64_t relStart = startAddr - this->startAddr;
    uint64_t relEnd = endAddr - this->startAddr;
    uint64_t curAddr = startAddr;

    try {
        while (relStart < relEnd) {
            out_stream << "0x" << std::hex << std::setfill('0') << std::setw(WORD_WIDTH) << curAddr << ": ";
            for (uint64_t i = 0; i < entriesPerRow; i++) {
                if (relStart < relEnd) {
                    out_stream << "0x";
                    for (int j = 0; j < (int)(entrySize); j++) {
                        out_stream << std::hex << std::setfill('0') << std::setw(BYTE_WIDTH)
                                   << (uint64_t)(memArr.at(relStart + j));
                    }
                    relStart += entrySize;
                    out_stream << " ";
                } else {
                    out_stream << std::endl;
                    return 0;
                }
            }

            out_stream << std::endl;
            curAddr += (uint64_t)(entrySize)*entriesPerRow;
        }
    } catch (const std::out_of_range &e) {
        std::cerr << LOG_ERROR << "Access violation at address 0x" << std::hex << curAddr << std::endl;
        return -EINVAL;
    }

    return 0;
}

int MemoryStore::printMemory(uint64_t startAddr, uint64_t endAddress) {
    return printMemArray(startAddr, endAddress, WORD_SIZE, 5, std::cout);
}

void dumpMemoryState(MemoryStore *mem, const std::string &base_output_name) {
    uint64_t startAddr;
    uint64_t endAddr;
    startAddr = 0;
    endAddr = 0x1f4;  // 500

    std::ifstream memRange;
    memRange.open("print_mem_range", std::ios::in);
    // For tests that don't specify a memory range to print out, the default is used.
    if (memRange) {
        memRange >> std::hex >> startAddr;
        memRange >> std::hex >> endAddr;
    }
    std::ofstream mem_out(base_output_name + "_mem_state.out");

    if (mem_out) {
        mem_out << "---------------------" << std::endl;
        mem_out << "Begin Memory State" << std::endl;
        mem_out << "---------------------" << std::endl;
        mem->printMemArray(startAddr, endAddr, WORD_SIZE, 5, mem_out);
        mem_out << "---------------------" << std::endl;
        mem_out << "End Memory State" << std::endl;
        mem_out << "---------------------" << std::endl;
    } else {
        std::cerr << LOG_ERROR << "Could not create memory state dump file" << std::endl;
    }
}
