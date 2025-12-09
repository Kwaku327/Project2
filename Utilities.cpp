#include "Utilities.h"

#include <arpa/inet.h>
#include <errno.h>
#include <inttypes.h>

#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>

static std::string getOpString(uint64_t opcode, uint64_t funct3, uint64_t funct7) {
    std::string prefix = "", body = "", suffix = "";
    switch (opcode) {
        case OP_INTIMM:
            suffix = "i";
        case OP_INT:
            switch (funct3) {
                case FUNCT3_ADD:
                    if (opcode == OP_INT) {
                        switch (funct7) {
                            case FUNCT7_ADD:
                                body = "add";
                                break;
                            case FUNCT7_SUB:
                                body = "sub";
                                break;
                            default:
                                return "ILLEGAL";
                        }
                    } else {
                        body = "add"; // There is no subi
                    }
                    break;
                case FUNCT3_SLL:
                    body = "sll";
                    break;
                case FUNCT3_SLT:
                    body = "slt";
                    break;
                case FUNCT3_SLTU:
                    body = "sltu";
                    break;
                case FUNCT3_XOR:
                    body = "xor";
                    break;
                case FUNCT3_SR:
                    switch (funct7 >> 1) {
                        case UPPERIMM_LOGICAL:
                            body = "srl";
                            break;
                        case UPPERIMM_ARITH:
                            body = "sra";
                            break;
                        default:
                            return "ILLEGAL";
                    }
                    break;
                case FUNCT3_OR:
                    body = "or";
                    break;
                case FUNCT3_AND:
                    body = "and";
                    break;
                default:
                    return "ILLEGAL";
            }
            break;
        case OP_INTIMMW:
            suffix = "i";
        case OP_INTW:
            suffix += "w";
            switch (funct3) {
                case FUNCT3_ADD:
                    switch (funct7) {
                        case FUNCT7_ADD:
                            body = "add";
                            break;
                        case FUNCT7_SUB:
                            body = "sub";
                            break;
                        default:
                            return "ILLEGAL";
                    }
                    break;
                case FUNCT3_SLL:
                    body = "sll";
                    break;
                case FUNCT3_SR:
                    switch (funct7) {
                        case FUNCT7_LOGICAL:
                            body = "srl";
                            break;
                        case FUNCT7_ARITH:
                            body = "sra";
                            break;
                        default:
                            return "ILLEGAL";
                    }
                    break;
                default:
                    return "ILLEGAL";
            }
            break;
        case OP_LOAD:
            prefix = "l";
            switch (funct3) {
                case FUNCT3_B:
                    body = "b";
                    break;
                case FUNCT3_H:
                    body = "h";
                    break;
                case FUNCT3_W:
                    body = "w";
                    break;
                case FUNCT3_D:
                    body = "d";
                    break;
                case FUNCT3_BU:
                    body = "bu";
                    break;
                case FUNCT3_HU:
                    body = "hu";
                    break;
                case FUNCT3_WU:
                    body = "wu";
                    break;
                default:
                    return "ILLEGAL";
            }
            break;
        case OP_STORE:
            prefix = "s";
            switch (funct3) {
                case FUNCT3_B:
                    body = "b";
                    break;
                case FUNCT3_H:
                    body = "h";
                    break;
                case FUNCT3_W:
                    body = "w";
                    break;
                case FUNCT3_D:
                    body = "d";
                    break;
                default:
                    return "ILLEGAL";
            }
            break;
        case OP_BRANCH:
            prefix = "b";
            switch (funct3) {
                case FUNCT3_BEQ:
                    body = "eq";
                    break;
                case FUNCT3_BNE:
                    body = "ne";
                    break;
                case FUNCT3_BLT:
                    body = "lt";
                    break;
                case FUNCT3_BGE:
                    body = "ge";
                    break;
                case FUNCT3_BLTU:
                    body = "ltu";
                    break;
                case FUNCT3_BGEU:
                    body = "geu";
                    break;
                default:
                    return "ILLEGAL";
            }
            break;
        case OP_JAL:
            return "jal";
        case OP_JALR:
            return "jalr";
        case OP_LUI:
            return "lui";
        case OP_AUIPC:
            return "auipc";
        default:
            return "ILLEGAL";
    }

    return prefix + body + suffix;
}

// extract specific bits [start, end] from a 32 bit instruction
uint64_t extractBits(uint64_t instruction, int start, int end) {
    int bitsToExtract = start - end + 1;
    uint64_t mask = (1 << bitsToExtract) - 1;
    uint64_t clipped = instruction >> end;
    return clipped & mask;
}

// sign extend imm to a 32 bit unsigned int
uint32_t sext32(uint32_t imm, int signBit) {
    return (imm & (1 << signBit)) ? imm | (~0U << (signBit + 1)) : imm;
}

// sign extend imm to a 64 bit unsigned int
uint64_t sext64(uint64_t imm, int signBit) {
    return (imm & (1ULL << signBit)) ? imm | (~0ULL << (signBit + 1)) : imm;
}

static void handleIAndR(uint64_t curInst, std::ostream &out_stream) {
    uint64_t opcode = extractBits(curInst, 6, 0);
    uint64_t rd = extractBits(curInst, 11, 7);
    uint64_t funct3 = extractBits(curInst, 14, 12);
    uint64_t rs1 = extractBits(curInst, 19, 15);
    uint64_t rs2 = extractBits(curInst, 24, 20);
    uint64_t funct7 = extractBits(curInst, 31, 25);

    uint64_t imm12  = extractBits(curInst, 31, 20);
    int64_t sextImm12 = sext64(imm12, 11);

    std::string opcodeStr = getOpString(opcode, funct3, funct7);

    if (opcodeStr == "ILLEGAL") {
        out_stream << " ILLEGAL";
        return;
    }

    std::ostringstream sb;
    if (opcode == OP_INT || opcode == OP_INTW) {
        sb << " " << getOpString(opcode, funct3, funct7) << " " 
        << regNames[rd] << ", " << regNames[rs1] << ", " << regNames[rs2];
    } else {
        sb << " " << getOpString(opcode, funct3, funct7) << " " 
        << regNames[rd] << ", " << regNames[rs1] << ", ";
        if (funct3 == FUNCT3_SLL || funct3 == FUNCT3_SR) {
            // For shift right, we need to handle the immediate differently
            sb << (imm12 & 0x3F);
        } else {
            sb << sextImm12;
        }
    }

    out_stream << sb.str();
}

static void handleLAndS(uint64_t curInst, std::ostream &out_stream) {
    uint64_t opcode = extractBits(curInst, 6, 0);
    uint64_t rd = extractBits(curInst, 11, 7);
    uint64_t funct3 = extractBits(curInst, 14, 12);
    uint64_t rs1 = extractBits(curInst, 19, 15);
    uint64_t rs2 = extractBits(curInst, 24, 20);

    uint64_t imm5 = extractBits(curInst, 11, 7);
    uint64_t imm7 = extractBits(curInst, 31, 25);

    uint64_t imm12 = extractBits(curInst, 31, 20);
    int64_t sextImm12 = sext64(imm12, 11);

    int64_t storeImm = sext64((imm7 << 5) | imm5, 11);

    std::string opcodeStr = getOpString(opcode, funct3, 0);

    if (opcodeStr == "ILLEGAL") {
        out_stream << " ILLEGAL";
        return;
    }

    std::ostringstream sb;
    if (opcode == OP_LOAD) {
        sb << " " << opcodeStr << " " << regNames[rd] << ", " 
           << sextImm12 << "(" << regNames[rs1] << ")";
    } else {
        sb << " " << opcodeStr << " " 
           << regNames[rs2] << ", " << storeImm 
           << "(" << regNames[rs1] << ")";
    }

    out_stream << sb.str();
}

static void handleBranch(uint64_t curInst, std::ostream &out_stream) {
    uint64_t opcode = extractBits(curInst, 6, 0);
    uint64_t rs1 = extractBits(curInst, 19, 15);
    uint64_t rs2 = extractBits(curInst, 24, 20);
    uint64_t funct3 = extractBits(curInst, 14, 12);

    uint64_t imm7 = extractBits(curInst, 31, 25);
    uint64_t imm5 = extractBits(curInst, 11, 7);

    int64_t branchImm = sext64(
        extractBits(imm7, 6, 6) << 12 |
        extractBits(imm7, 5, 0) << 5 |
        extractBits(imm5, 4, 1) << 1 |
        extractBits(imm5, 0, 0) << 11,
        12); // B-type immediate 

    std::string opcodeStr = getOpString(opcode, funct3, 0);

    if (opcodeStr == "ILLEGAL") {
        out_stream << " ILLEGAL";
        return;
    }

    std::ostringstream sb;
    sb << " " << opcodeStr << " " 
       << regNames[rs1] << ", " << regNames[rs2] 
       << ", " << branchImm;

    out_stream << sb.str();
}

static void handleSpecial(uint64_t curInst, std::ostream &out_stream) {
    uint64_t opcode = extractBits(curInst, 6, 0);
    uint64_t rd = extractBits(curInst, 11, 7);
    uint64_t imm20 = extractBits(curInst, 31, 12);
    int64_t sextUpper = sext64(imm20 << 12, 19);

    int64_t jalImm = sext64(
        extractBits(imm20, 19, 19) << 20 |
        extractBits(imm20, 18, 9) << 1 |
        extractBits(imm20, 8, 8) << 11 |
        extractBits(imm20, 7, 0) << 12,
        20); // J-type immediate

    std::string opcodeStr = getOpString(opcode, 0, 0);

    if (opcodeStr == "ILLEGAL") {
        out_stream << " ILLEGAL";
        return;
    }

    std::ostringstream sb;
    if (opcode == OP_JAL) {
        sb << " " << opcodeStr << " " << regNames[rd] << ", " 
           << jalImm;
    } else {
        sb << " " << opcodeStr << " " << regNames[rd] 
           << ", " << sextUpper;
    }

    out_stream << sb.str();
}


static void printIFPC(uint64_t pc, StageStatus status, std::ostream &pipeState) {
    std::ostringstream sb;
    sb << " Inst at 0x" << std::hex << pc << stageStatusStr.at(status);
    pipeState << std::left << std::setw(25) << sb.str();
}

static void printInstr(uint32_t curInst, StageStatus status, std::ostream &pipeState) {
    std::ostringstream sb;
    if (curInst == 0xfeedfeed) {
        sb << " HALT" << stageStatusStr.at(status);
        pipeState << std::left << std::setw(25) << sb.str();
        return;
    // } else if (curInst == 0xdeefdeef) {
    //     pipeState << std::left << std::setw(25) << " UNKNOWN ";
    //     return;
    } else if (curInst == 0x00000013) {
        sb << " NOP" << stageStatusStr.at(status);
        pipeState << std::left << std::setw(25) << sb.str();
        return;
    }

    uint64_t opcode = extractBits(curInst, 6,  0);

    switch (opcode) {
        case OP_INT:
        case OP_INTW:
        case OP_INTIMM:
        case OP_INTIMMW:
        case OP_JALR:
            handleIAndR(curInst, sb);
            break;
        case OP_LOAD:
        case OP_STORE:
            handleLAndS(curInst, sb);
            break;
        case OP_BRANCH:
            handleBranch(curInst, sb);
            break;
        case OP_JAL:
        case OP_LUI:
        case OP_AUIPC:
            handleSpecial(curInst, sb);
            break;
        default:
            // Illegal instruction. Trigger an exception.
            // Note: Since we catch illegal instructions here, the "handle"
            // instructions don't need to check for illegal instructions.
            // except for the case with a 0 opcode and illegal function.
            sb << " ILLEGAL";
    }
    sb << stageStatusStr.at(status);
    pipeState << std::left << std::setw(25) << sb.str();
}

Status dumpPipeState(PipeState &state, const std::string &base_output_name) {
    static auto fileInit = false;
    auto fileOp = std::ios::app;
    if (!fileInit) {
        fileOp = std::ios::out;
        fileInit = true;
    }
    std::ofstream pipe_out(base_output_name + "_pipe_state.out", fileOp);

    if (pipe_out) {
        pipe_out << "Cycle: " << std::setw(8) << state.cycle << "\t|";
        pipe_out << "|";
        printIFPC(state.ifPC, state.ifStatus, pipe_out);
        pipe_out << "|";
        printInstr(state.idInstr, state.idStatus, pipe_out);
        pipe_out << "|";
        printInstr(state.exInstr, state.exStatus, pipe_out);
        pipe_out << "|";
        printInstr(state.memInstr, state.memStatus, pipe_out);
        pipe_out << "|";
        printInstr(state.wbInstr, state.wbStatus, pipe_out);
        pipe_out << "|" << std::endl;
        return SUCCESS;
    } else {
        std::cerr << LOG_ERROR << "Could not open pipe state file!" << std::endl;
        return ERROR;
    }
}

Status dumpSimStats(SimulationStats &stats, const std::string &base_output_name) {
    std::ofstream simStats(base_output_name + "_sim_stats.out");

    if (simStats) {
        simStats << std::left << std::setw(23) << "Dynamic instructions: "<< stats.dynamicInstructions << std::endl;
        simStats << std::left << std::setw(23) << "Total cycles: "        << stats.totalCycles << std::endl;
        simStats << std::left << std::setw(23) << "I-cache hits: "        << stats.icHits << std::endl;
        simStats << std::left << std::setw(23) << "I-cache misses: "      << stats.icMisses << std::endl;
        simStats << std::left << std::setw(23) << "D-cache hits: "        << stats.dcHits << std::endl;
        simStats << std::left << std::setw(23) << "D-cache misses: "      << stats.dcMisses << std::endl;
        simStats << std::left << std::setw(23) << "Load-use stalls: "     << stats.loadUseStalls << std::endl;
        return SUCCESS;
    } else {
        std::cerr << LOG_ERROR << "Could not open sim stats file!" << std::endl;
        return ERROR;
    }
}
