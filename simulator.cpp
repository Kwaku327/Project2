#include "simulator.h"

#include <stdio.h>
#include <iostream>
#include <stdexcept>
using namespace std;

#define EXCEPTION_HANDLER 0x8000

Simulator::Simulator() {
    // Initialize member variables
    memory = nullptr;
    regData.reg = {};
    din = 0;
}

Simulator::~Simulator() {
    if (memory) delete memory;
}

// dump registers and memory
void Simulator::dumpRegMem(const std::string& output_name) {
    dumpRegisterState(regData.reg, output_name);
    dumpMemoryState(memory, output_name);
}


// Get raw instruction bits from memory
Simulator::Instruction Simulator::simFetch(uint64_t PC, MemoryStore *myMem) {
    // fetch current instruction
    uint64_t instruction;
    myMem->getMemValue(PC, instruction, WORD_SIZE);
    instruction = (uint32_t)instruction;

    Instruction inst;
    inst.PC = PC;
    inst.instruction = instruction;
    return inst;
}

// Determine instruction opcode, funct, reg names (but not calculate all imms)
Simulator::Instruction Simulator::simDecode(Instruction inst) {
    inst.opcode = extractBits(inst.instruction, 6, 0);
    inst.rd     = extractBits(inst.instruction, 11, 7);
    inst.funct3 = extractBits(inst.instruction, 14, 12);
    inst.rs1    = extractBits(inst.instruction, 19, 15);
    inst.rs2    = extractBits(inst.instruction, 24, 20);
    inst.funct7 = extractBits(inst.instruction, 31, 25);

    inst.isLegal = true; // assume legal unless proven otherwise

    if (inst.instruction == 0xfeedfeed) {
        inst.isHalt = true;
        return inst; // halt instruction
    }
    if (inst.instruction == 0x00000013) {
        inst.isNop = true;
        return inst; // NOP instruction
    }

    switch (inst.opcode) {
        case OP_INT:
            if ((inst.funct3 == FUNCT3_ADD && (inst.funct7 == FUNCT7_ADD || inst.funct7 == FUNCT7_SUB)) || 
                inst.funct3 == FUNCT3_SLL || inst.funct3 == FUNCT3_SLT || inst.funct3 == FUNCT3_SLTU ||
                inst.funct3 == FUNCT3_XOR || (inst.funct3 == FUNCT3_SR && (inst.funct7 >> 1 == UPPERIMM_ARITH || 
                inst.funct7 >> 1 == UPPERIMM_LOGICAL)) || inst.funct3 == FUNCT3_OR || inst.funct3 == FUNCT3_AND) {
                inst.doesArithLogic = true;
                inst.writesRd = true;
                inst.readsRs1 = true;
                inst.readsRs2 = true;
            } else {
                inst.isLegal = false;
            }
            break;
        case OP_INTW:
            if ((inst.funct3 == FUNCT3_ADD && (inst.funct7 == FUNCT7_ADD || inst.funct7 == FUNCT7_SUB)) || 
                inst.funct3 == FUNCT3_SLL || (inst.funct3 == FUNCT3_SR && (inst.funct7 == FUNCT7_ARITH || 
                inst.funct7 == FUNCT7_LOGICAL))) {
                inst.doesArithLogic = true;
                inst.writesRd = true;
                inst.readsRs1 = true;
                inst.readsRs2 = true;
            } else {
                inst.isLegal = false;
            }
            break;
        case OP_LOAD:
            if (inst.funct3 == FUNCT3_B || inst.funct3 == FUNCT3_H ||
                inst.funct3 == FUNCT3_W || inst.funct3 == FUNCT3_D ||
                inst.funct3 == FUNCT3_BU || inst.funct3 == FUNCT3_HU ||
                inst.funct3 == FUNCT3_WU) {
                inst.readsRs1 = true;
                inst.writesRd = true;
                inst.readsMem = true;
            } else {
                inst.isLegal = false;
            }
            break;
        case OP_INTIMM:
            if (inst.funct3 == FUNCT3_ADD || inst.funct3 == FUNCT3_SLL || inst.funct3 == FUNCT3_SLT || 
                inst.funct3 == FUNCT3_SLTU || inst.funct3 == FUNCT3_XOR || (inst.funct3 == FUNCT3_SR &&
                (inst.funct7 >> 1 == UPPERIMM_ARITH || inst.funct7 >> 1 == UPPERIMM_LOGICAL)) ||
                inst.funct3 == FUNCT3_OR || inst.funct3 == FUNCT3_AND) {
                inst.doesArithLogic = true;
                inst.writesRd = true;
                inst.readsRs1 = true;
            } else {
                inst.isLegal = false;
            }
            break;
        case OP_INTIMMW:
            if (inst.funct3 == FUNCT3_ADD || inst.funct3 == FUNCT3_SLL ||
                (inst.funct3 == FUNCT3_SR && (inst.funct7 == FUNCT7_ARITH || 
                inst.funct7 == FUNCT7_LOGICAL))) {
                inst.doesArithLogic = true;
                inst.writesRd = true;
                inst.readsRs1 = true;
            } else {
                inst.isLegal = false;
            }
            break;
        case OP_JALR:
            inst.isLegal = true;
            inst.doesArithLogic = true;
            inst.writesRd = true;
            inst.readsRs1 = true;
            break;
        case OP_STORE:
            if (inst.funct3 == FUNCT3_B || inst.funct3 == FUNCT3_H ||
                inst.funct3 == FUNCT3_W || inst.funct3 == FUNCT3_D) {
                inst.readsRs1 = true;
                inst.readsRs2 = true;
                inst.writesMem = true;
            } else {
                inst.isLegal = false;
            }
            break;
        case OP_BRANCH:
            if (inst.funct3 == FUNCT3_BEQ || inst.funct3 == FUNCT3_BNE ||
                inst.funct3 == FUNCT3_BLT || inst.funct3 == FUNCT3_BGE ||
                inst.funct3 == FUNCT3_BLTU || inst.funct3 == FUNCT3_BGEU) {
                inst.readsRs1 = true;
                inst.readsRs2 = true;
            } else {
                inst.isLegal = false;
            }
            break;
        case OP_AUIPC:
        case OP_LUI:
        case OP_JAL:
            inst.isLegal = true;
            inst.doesArithLogic = true;
            inst.writesRd = true;
            break;
        default:
            inst.isLegal = false;
    }
    return inst;
}

// Collect operands whether reg or imm for arith or addr gen
Simulator::Instruction Simulator::simOperandCollection(Instruction inst, REGS regData) {
    regData.registers[0] = 0;
    if (inst.readsRs1) {
        inst.op1Val = regData.registers[inst.rs1];
    }
    if (inst.readsRs2) {
        inst.op2Val = regData.registers[inst.rs2];
    }
    return inst;
}

// Resolve next PC whether +4 or branch/jump target taken/not taken
Simulator::Instruction Simulator::simNextPCResolution(Instruction inst) {

    uint64_t imm5   = inst.rd;
    uint64_t imm7   = inst.funct7;
    uint64_t imm12  = extractBits(inst.instruction, 31, 20);
    uint64_t imm20  = extractBits(inst.instruction, 31, 12);
    uint64_t branchTarget = inst.PC + sext64(
        extractBits(imm7, 6, 6) << 12 |
        extractBits(imm7, 5, 0) << 5 |
        extractBits(imm5, 4, 1) << 1 |
        extractBits(imm5, 0, 0) << 11,
        12); // B-type immediate 
    uint64_t jalTarget = inst.PC + sext64(
        extractBits(imm20, 19, 19) << 20 |
        extractBits(imm20, 18, 9) << 1 |
        extractBits(imm20, 8, 8) << 11 |
        extractBits(imm20, 7, 0) << 12,
        20); // J-type immediate

    switch (inst.opcode) {
        case OP_JALR:
            inst.nextPC = (inst.op1Val + sext64(imm12, 11)) & ~1ULL;
            break;
        case OP_BRANCH:
            inst.nextPC = inst.PC + 4;
            switch (inst.funct3) {
                case FUNCT3_BEQ:
                    if (inst.op1Val == inst.op2Val) {
                        inst.nextPC = branchTarget;
                    }
                    break;
                case FUNCT3_BNE:
                    if (inst.op1Val != inst.op2Val) {
                        inst.nextPC = branchTarget;
                    }
                    break;
                case FUNCT3_BLT:
                    if ((int64_t)inst.op1Val < (int64_t)inst.op2Val) {
                        inst.nextPC = branchTarget;
                    }
                    break;
                case FUNCT3_BGE:
                    if ((int64_t)inst.op1Val >= (int64_t)inst.op2Val) {
                        inst.nextPC = branchTarget;
                    }
                    break;
                case FUNCT3_BLTU:
                    if (inst.op1Val < inst.op2Val) {
                        inst.nextPC = branchTarget;
                    }
                    break;
                case FUNCT3_BGEU:
                    if (inst.op1Val >= inst.op2Val) {
                        inst.nextPC = branchTarget;
                    }
                    break;
            }
            break;
        case OP_JAL:
            inst.nextPC = jalTarget;
            break;
        default:
            inst.nextPC = inst.PC + 4;
    }

    return inst;
}

// Perform arithmetic operations
Simulator::Instruction Simulator::simArithLogic(Instruction inst) {
    uint64_t imm12  = extractBits(inst.instruction, 31, 20);
    uint64_t upperImm12 = extractBits(inst.instruction, 31, 26);
    uint64_t imm20  = extractBits(inst.instruction, 31, 12);
    
    if (inst.opcode == OP_INT && (
        inst.funct3 == FUNCT3_SLL || inst.funct3 == FUNCT3_SR)) {
        // For SLL and SR, only low 6 bits of rs2 are considered in
        // RV64I, so we mask it to 6 bits
        inst.op2Val &= 0x3F;
    } else if (inst.opcode == OP_INTW && (
        inst.funct3 == FUNCT3_SLL || inst.funct3 == FUNCT3_SR)) {
        // For SLLW and SRW, only low 5 bits of rs2 are considered in
        // RV64I, so we mask it to 5 bits
        inst.op2Val &= 0x1F;
    }

    switch (inst.opcode) {
        case OP_INT:
            switch (inst.funct3) {
                case FUNCT3_ADD:
                    if (inst.funct7 == FUNCT7_ADD) {
                        inst.arithResult = inst.op1Val + inst.op2Val;
                    } else if (inst.funct7 == FUNCT7_SUB) {
                        inst.arithResult = inst.op1Val - inst.op2Val;
                    }
                    break;
                case FUNCT3_SLL:
                    inst.arithResult = inst.op1Val << inst.op2Val;
                    break;
                case FUNCT3_SLT:
                    inst.arithResult = (int64_t)inst.op1Val < (int64_t)inst.op2Val;
                    break;
                case FUNCT3_SLTU:
                    inst.arithResult = inst.op1Val < inst.op2Val;
                    break;
                case FUNCT3_XOR:
                    inst.arithResult = inst.op1Val ^ inst.op2Val;
                    break;
                case FUNCT3_SR:
                    if (upperImm12 == FUNCT7_LOGICAL) {
                        inst.arithResult = inst.op1Val >> inst.op2Val;
                    } else if (upperImm12 == UPPERIMM_ARITH) {
                        inst.arithResult = (int64_t)inst.op1Val >> inst.op2Val;
                    }
                    break;
                case FUNCT3_OR:
                    inst.arithResult = inst.op1Val | inst.op2Val;
                    break;
                case FUNCT3_AND:
                    inst.arithResult = inst.op1Val & inst.op2Val;
                    break;
                default:
                    fprintf(stderr, "\tIllegal funct3\n");
            }
            break;
        case OP_INTW:
            switch (inst.funct3) {
                case FUNCT3_ADD:
                    if (inst.funct7 == FUNCT7_ADD) {
                        inst.arithResult = sext64((uint32_t)inst.op1Val + (uint32_t)inst.op2Val, 31);
                    } else if (inst.funct7 == FUNCT7_SUB) {
                        inst.arithResult = sext64((uint32_t)inst.op1Val - (uint32_t)inst.op2Val, 31);
                    }
                    break;
                case FUNCT3_SLL:
                    inst.arithResult = sext64((uint32_t)inst.op1Val << (uint32_t)inst.op2Val, 31);
                    break;
                case FUNCT3_SR:
                    if (upperImm12 == FUNCT7_LOGICAL) {
                        inst.arithResult = sext64((uint32_t)inst.op1Val >> (uint32_t)inst.op2Val, 31);
                    } else if (upperImm12 == UPPERIMM_ARITH) {
                        inst.arithResult = sext64((int32_t)inst.op1Val >> (uint32_t)inst.op2Val, 31);
                    }
                    break;
            }
            break;
        case OP_INTIMM:
            switch (inst.funct3) {
                case FUNCT3_ADD:
                    inst.arithResult = inst.op1Val + sext64(imm12, 11);
                    break;
                case FUNCT3_SLL:
                    inst.arithResult = inst.op1Val << (imm12 & 0x3F);
                    break;
                case FUNCT3_SLT:
                    inst.arithResult = (int64_t)inst.op1Val < (int64_t)sext64(imm12, 11);
                    break;
                case FUNCT3_SLTU:
                    inst.arithResult = inst.op1Val < sext64(imm12, 11);
                    break;
                case FUNCT3_XOR:
                    inst.arithResult = inst.op1Val ^ sext64(imm12, 11);
                    break;
                case FUNCT3_SR:
                    if (upperImm12 == UPPERIMM_LOGICAL) {
                        inst.arithResult = inst.op1Val >> (imm12 & 0x3F);
                    } else if (upperImm12 == UPPERIMM_ARITH) {
                        inst.arithResult = (int64_t)inst.op1Val >> (imm12 & 0x3F);
                    }
                    break;
                case FUNCT3_OR:
                    inst.arithResult = inst.op1Val | sext64(imm12, 11);
                    break;
                case FUNCT3_AND:
                    inst.arithResult = inst.op1Val & sext64(imm12, 11);
                    break;
            }
            break;
        case OP_INTIMMW:
            switch (inst.funct3) {
                case FUNCT3_ADD:
                    inst.arithResult = sext64((uint32_t)inst.op1Val + (uint32_t)sext32(imm12, 11), 31);
                    break;
                case FUNCT3_SLL:
                    inst.arithResult = sext64((uint32_t)inst.op1Val << (uint32_t)(imm12 & 0x1F), 31);
                    break;
                case FUNCT3_SR:
                    if (upperImm12 == UPPERIMM_LOGICAL) {
                        inst.arithResult = sext64((uint32_t)inst.op1Val >> (uint32_t)(imm12 & 0x1F), 31);
                    } else if (upperImm12 == UPPERIMM_ARITH) {
                        inst.arithResult = sext64((int32_t)inst.op1Val >> (uint32_t)(imm12 & 0x1F), 31);
                    }
                    break;
            }
            break;
        case OP_JALR:
            inst.arithResult = inst.PC + 4;
            break;
        case OP_AUIPC:
            inst.arithResult = inst.PC + sext64(imm20 << 12, 31);
            break;
        case OP_LUI:
            inst.arithResult = sext64(imm20 << 12, 31);
            break;
        case OP_JAL:
            inst.arithResult = inst.PC + 4;
            break;
    }

    return inst;
}

// Generate memory address for load/store instructions
Simulator::Instruction Simulator::simAddrGen(Instruction inst) {
    uint64_t imm5   = inst.rd;
    uint64_t imm7   = inst.funct7;
    uint64_t imm12  = extractBits(inst.instruction, 31, 20);
    int64_t storeImm = sext64((imm7 << 5) | imm5, 11); // S-type immediate
    
    if (inst.readsMem) {
        inst.memAddress = inst.op1Val + sext64(imm12, 11);
    } else if (inst.writesMem) {
        inst.memAddress = inst.op1Val + storeImm;
    }

    return inst;
}

// Perform memory access for load/store instructions
Simulator::Instruction Simulator::simMemAccess(Instruction inst, MemoryStore *myMem) {
    MemEntrySize size = (inst.funct3 == FUNCT3_B || inst.funct3 == FUNCT3_BU) ? BYTE_SIZE :
                    (inst.funct3 == FUNCT3_H || inst.funct3 == FUNCT3_HU) ? HALF_SIZE :
                    (inst.funct3 == FUNCT3_W || inst.funct3 == FUNCT3_WU) ? WORD_SIZE : DOUBLE_SIZE;

    if (inst.readsMem) {
        uint64_t value;
        myMem->getMemValue(inst.memAddress, value, size);
        
        if (inst.funct3 == FUNCT3_B || inst.funct3 == FUNCT3_H || inst.funct3 == FUNCT3_W) {
            inst.memResult = sext64(value, size * 8 - 1);
        } else {
            inst.memResult = value;
        }
    } else if (inst.writesMem) {
        myMem->setMemValue(inst.memAddress, inst.op2Val, size);
    }

    return inst;
}

// Write back results to registers
Simulator::Instruction Simulator::simCommit(Instruction inst, REGS &regData) {
    if (inst.readsMem) {
        regData.registers[inst.rd] = inst.memResult;
    } else {
        regData.registers[inst.rd] = inst.arithResult;
    }
    return inst;
}

// TODO complete the following pipeline stage simulation functions
// You may find it useful to call functional simulation functions above

Simulator::Instruction Simulator::simIF(uint64_t PC) {
    throw std::runtime_error("simIF not implemented yet"); // TODO implement IF 
}

Simulator::Instruction Simulator::simID(Simulator::Instruction inst) {
    throw std::runtime_error("simID not implemented yet"); // TODO implement ID
}

Simulator::Instruction Simulator::simEX(Simulator::Instruction inst) {
    throw std::runtime_error("simEX not implemented yet"); // TODO implement EX
}

Simulator::Instruction Simulator::simMEM(Simulator::Instruction inst) {
    throw std::runtime_error("simMEM not implemented yet"); // TODO implement MEM
}

Simulator::Instruction Simulator::simWB(Simulator::Instruction inst) {
    throw std::runtime_error("simWB not implemented yet"); // TODO implement WB
}


// Simulate the whole instruction using functions above
Simulator::Instruction Simulator::simInstruction(uint64_t PC) {
    // Implementation moved from .cpp to .h for illustration
    Instruction inst = simFetch(PC, memory);
    inst = simDecode(inst);
    inst.instructionID = din++;
    if (!inst.isLegal || inst.isHalt) return inst;
    inst = simOperandCollection(inst, regData);
    inst = simNextPCResolution(inst);
    if (inst.doesArithLogic) inst = simArithLogic(inst);
    if (inst.readsMem || inst.writesMem) {
        inst = simAddrGen(inst);
        inst = simMemAccess(inst, memory);
    }
    if (inst.writesRd) inst = simCommit(inst, regData);
    PC = inst.nextPC;
    return inst;
}