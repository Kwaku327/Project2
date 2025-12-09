#pragma once

#include <string>

#include "Utilities.h"
#include "MemoryStore.h"
#include "RegisterInfo.h"

class Simulator {
   private:
    union REGS {
        RegisterInfo reg;
        uint64_t registers[32]{0};
    };

    // Registers
    union REGS regData;
    // memory component
    MemoryStore* memory;

    // Arch states and statistics
    uint64_t din;  // Dynamic instruction number

   public:
    Simulator();
    ~Simulator();

    struct Instruction {
        // known by IF
        uint64_t PC = 0;
        uint64_t instruction = 0;    // raw instruction encoding

        // known by ID
        bool     isHalt = false;
        bool     isLegal = false;
        bool     isNop = false;

        bool     readsMem = false;
        bool     writesMem = false;
        bool     doesArithLogic = false;
        bool     writesRd = false;
        bool     readsRs1 = false;
        bool     readsRs2 = false;

        uint64_t opcode = 0;
        uint64_t funct3 = 0;
        uint64_t funct7 = 0;
        uint64_t rd = 0;
        uint64_t rs1 = 0;
        uint64_t rs2 = 0;

        uint64_t nextPC = 0;

        uint64_t op1Val = 0;
        uint64_t op2Val = 0;


        // known by EX
        uint64_t arithResult = 0;
        uint64_t memAddress = 0;

        // known by MEM
        bool     memException = false;
        uint64_t memResult = 0;
        uint64_t valToWrite = 0;

        // known by WB
        uint64_t instructionID = 0;  // din of the instruction

        // Used for stage status tracking in cycle
        StageStatus status = NORMAL;
    };

    // getters and setters
    auto getDin() { return din; }
    auto getMemory() { return memory; }

    void setMemory(MemoryStore* mem) { memory = mem; }

    // Simulate by functionality (project 1)
    Instruction simFetch(uint64_t PC, MemoryStore *myMem);
    Instruction simDecode(Instruction inst);
    Instruction simOperandCollection(Instruction inst, REGS regData);
    Instruction simNextPCResolution(Instruction inst);
    Instruction simArithLogic(Instruction inst);
    Instruction simAddrGen(Instruction inst);
    Instruction simMemAccess(Instruction inst, MemoryStore *myMem);
    Instruction simCommit(Instruction inst, REGS &regData);

    // Simulate an instruction functionally in a single step
    Instruction simInstruction(uint64_t PC);

    // Simulate pipeline stages (project 2 TODO)
    Instruction simIF(uint64_t PC);
    Instruction simID(Instruction inst);
    Instruction simEX(Instruction inst);
    Instruction simMEM(Instruction inst);
    Instruction simWB(Instruction inst);

    // Helper function to dump registers and memory
    void dumpRegMem(const std::string& output_name);
};
