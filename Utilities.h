#pragma once
#include <arpa/inet.h>
#include <errno.h>
#include <inttypes.h>

#include <fstream>
#include <iomanip>
#include <iostream>
#include <string>
#include <unordered_map>

#define NUM_REGS 32

// Utilities macro, they are very useful for debugging
// Check sim_cycle.cpp to see how to use them!
#define LOG_INFO \
    "[INFO] (" << __FILE__ << ":" << __LINE__ << "): "
#define LOG_ERROR \
    "[ERROR] (" << __FILE__ << ":" << __LINE__ << "): "
#define LOG_VAR(var) \
    #var << ": " << var << " "

enum Status { SUCCESS = 0, ERROR = 1, HALT = 2 };

// Printing code...
enum OPCODES {
    // R-type opcodes
    OP_INT     = 0b0110011, // Integer ALU instructions add, sub, sll, slt, sltu, xor, srl, sra, or, and
    OP_INTW    = 0b0111011, // Integer ALU instructions on 32-bit words addw, subw, sllw, srlw, sraw
    // I-type opcodes
    OP_LOAD    = 0b0000011, // Load instructions lb, lh, lw, ld, lbu, lhu, lwu
    OP_INTIMM  = 0b0010011, // Integer ALU immediate instructions addi, slli, slti, sltiu, xori, srli, srai, ori, andi
    OP_INTIMMW = 0b0011011, // Integer ALU immediate instructions on 32-bit words addiw, slliw, srliw, sraiw
    OP_JALR    = 0b1100111, // jalr
    // S-type opcodes
    OP_STORE   = 0b0100011, // Store instructions sb, sh, sw, sd
    // B-type opcodes
    OP_BRANCH  = 0b1100011, // Branch instructions beq, bne, blt, bge, bltu, bgeu
    // U-type opcodes
    OP_AUIPC   = 0b0010111, // auipc
    OP_LUI     = 0b0110111, // lui
    // J-type opcodes
    OP_JAL     = 0b1101111, // jal
};

enum FUNCT3 {
    // For integer ALU instructions
    FUNCT3_ADD  = 0b000, // add
    FUNCT3_SLL  = 0b001, // shift left logical
    FUNCT3_SLT  = 0b010, // set less than
    FUNCT3_SLTU = 0b011, // set less than unsigned
    FUNCT3_XOR  = 0b100, // xor
    FUNCT3_SR   = 0b101, // shift right
    FUNCT3_OR   = 0b110, // or
    FUNCT3_AND  = 0b111, // and
    // For load/store instructions
    FUNCT3_B    = 0b000, // load/store byte
    FUNCT3_H    = 0b001, // load/store halfword
    FUNCT3_W    = 0b010, // load/store word
    FUNCT3_D    = 0b011, // load/store doubleword
    // For load instructions
    FUNCT3_BU   = 0b100, // load byte unsigned
    FUNCT3_HU   = 0b101, // load halfword unsigned
    FUNCT3_WU   = 0b110, // load word unsigned
    // For branch instructions
    FUNCT3_BEQ  = 0b000, // branch if equal
    FUNCT3_BNE  = 0b001, // branch if not equal
    FUNCT3_BLT  = 0b100, // branch if less than
    FUNCT3_BGE  = 0b101, // branch if greater than or equal
    FUNCT3_BLTU = 0b110, // branch if less than unsigned
    FUNCT3_BGEU = 0b111, // branch if greater than or equal unsigned
};

enum RI_FUNCT7 {
    // for R type add/sub instruction
    FUNCT7_ADD     = 0b0000000, // add
    FUNCT7_SUB     = 0b0100000, // sub

    FUNCT7_LOGICAL = 0b0000000, // logical shift
    FUNCT7_ARITH   = 0b0100000, // arithmetic shift
};

enum SR_UPPER_IMM12 {
    // For shift right instructions
    UPPERIMM_LOGICAL = 0b000000, // shift logical
    UPPERIMM_ARITH   = 0b010000, // shift arithmetic
};

static const std::string regNames[NUM_REGS] = {
    "zero", "ra", "sp", "gp", "tp", "t0", "t1", "t2",
    "s0", "s1", "a0", "a1", "a2", "a3", "a4", "a5",
    "a6", "a7", "s2", "s3", "s4", "s5", "s6", "s7", "s8",
    "s9", "s10", "s11", "t3", "t4", "t5", "t6"
};

enum StageStatus {
    NORMAL = 0,
    BUBBLE,
    IDLE,
    SPECULATIVE,
    SQUASHED,
};

static const std::unordered_map<StageStatus, std::string> stageStatusStr = {
    {BUBBLE, " (bubble) "},
    {IDLE, " (idle) "},
    {NORMAL, " "},
    {SPECULATIVE, " (spcu) "},
    {SQUASHED, " (squashed) "}
};

struct PipeState {
    uint64_t cycle;
    StageStatus ifStatus;
    uint64_t ifPC;
    StageStatus idStatus;
    uint64_t idInstr;
    StageStatus exStatus;
    uint64_t exInstr;
    StageStatus memStatus;
    uint64_t memInstr;
    StageStatus wbStatus;
    uint64_t wbInstr;
};

struct SimulationStats {
    uint64_t dynamicInstructions;
    uint64_t totalCycles;
    uint64_t icHits;
    uint64_t icMisses;
    uint64_t dcHits;
    uint64_t dcMisses;
    uint64_t loadUseStalls;
};

// extract specific bits [start, end] from a 32 bit instruction
uint64_t extractBits(uint64_t instruction, int start, int end);

// sign extend imm to a 32 bit unsigned int
uint32_t sext32(uint32_t imm, int signBit);

// sign extend imm to a 64 bit unsigned int
uint64_t sext64(uint64_t imm, int signBit);

// Implemented in UtilityFunctions.o
Status dumpPipeState(PipeState& state, const std::string& base_output_name);
Status dumpSimStats(SimulationStats& stats, const std::string& base_output_name);

// handle output file names
inline std::string getBaseFilename(const char* inputPath) {
    std::string path(inputPath);
    size_t end = path.rfind('.');

    if (end != std::string::npos) {
        return path.substr(0, end);
    } else {
        return path;
    }
}
