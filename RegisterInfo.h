#pragma once

#include <fstream>
#include <iomanip>
#include <iostream>

#include "Utilities.h"

#define REG_SIZE 32

#define REG_OUTPUT_WIDTH 8

// A struct that can hold the values of all architectural registers.
struct RegisterInfo
{
    uint64_t zero;
    uint64_t ra;
    uint64_t sp;
    uint64_t gp;
    uint64_t tp;
    uint64_t t0;
    uint64_t t1;
    uint64_t t2;
    uint64_t s0;
    uint64_t s1;
    uint64_t a0;
    uint64_t a1;
    uint64_t a2;
    uint64_t a3;
    uint64_t a4;
    uint64_t a5;
    uint64_t a6;
    uint64_t a7;
    uint64_t s2;
    uint64_t s3;
    uint64_t s4;
    uint64_t s5;
    uint64_t s6;
    uint64_t s7;
    uint64_t s8;
    uint64_t s9;
    uint64_t s10;
    uint64_t s11;
    uint64_t t3;
    uint64_t t4;
    uint64_t t5;
    uint64_t t6;
};

inline Status dumpRegisterState(RegisterInfo& reg, const std::string& base_output_name) {
    std::ofstream reg_out(base_output_name + "_reg_state.out");
    if (reg_out) {
        reg_out << "---------------------" << std::endl;
        reg_out << "Begin Register Values" << std::endl;
        reg_out << "---------------------" << std::endl;
        reg_out << "$ra = 0x" << std::hex << std::setfill('0') << std::setw(REG_OUTPUT_WIDTH) << reg.ra << std::endl;
        reg_out << "$sp = 0x" << std::hex << std::setfill('0') << std::setw(REG_OUTPUT_WIDTH) << reg.sp << std::endl;
        reg_out << "$gp = 0x" << std::hex << std::setfill('0') << std::setw(REG_OUTPUT_WIDTH) << reg.gp << std::endl;
        reg_out << "$tp = 0x" << std::hex << std::setfill('0') << std::setw(REG_OUTPUT_WIDTH) << reg.tp << std::endl;
        reg_out << std::endl;
        reg_out << "$t0 = 0x" << std::hex << std::setfill('0') << std::setw(REG_OUTPUT_WIDTH) << reg.t0 << std::endl;
        reg_out << "$t1 = 0x" << std::hex << std::setfill('0') << std::setw(REG_OUTPUT_WIDTH) << reg.t1 << std::endl;
        reg_out << "$t2 = 0x" << std::hex << std::setfill('0') << std::setw(REG_OUTPUT_WIDTH) << reg.t2 << std::endl;
        reg_out << std::endl;
        reg_out << "$s0 = 0x" << std::hex << std::setfill('0') << std::setw(REG_OUTPUT_WIDTH) << reg.s0 << std::endl;
        reg_out << "$s1 = 0x" << std::hex << std::setfill('0') << std::setw(REG_OUTPUT_WIDTH) << reg.s1 << std::endl;
        reg_out << std::endl;
        reg_out << "$a0 = 0x" << std::hex << std::setfill('0') << std::setw(REG_OUTPUT_WIDTH) << reg.a0 << std::endl;
        reg_out << "$a1 = 0x" << std::hex << std::setfill('0') << std::setw(REG_OUTPUT_WIDTH) << reg.a1 << std::endl;
        reg_out << "$a2 = 0x" << std::hex << std::setfill('0') << std::setw(REG_OUTPUT_WIDTH) << reg.a2 << std::endl;
        reg_out << "$a3 = 0x" << std::hex << std::setfill('0') << std::setw(REG_OUTPUT_WIDTH) << reg.a3 << std::endl;
        reg_out << "$a4 = 0x" << std::hex << std::setfill('0') << std::setw(REG_OUTPUT_WIDTH) << reg.a4 << std::endl;
        reg_out << "$a5 = 0x" << std::hex << std::setfill('0') << std::setw(REG_OUTPUT_WIDTH) << reg.a5 << std::endl;
        reg_out << "$a6 = 0x" << std::hex << std::setfill('0') << std::setw(REG_OUTPUT_WIDTH) << reg.a6 << std::endl;
        reg_out << "$a7 = 0x" << std::hex << std::setfill('0') << std::setw(REG_OUTPUT_WIDTH) << reg.a7 << std::endl;
        reg_out << std::endl;
        reg_out << "$s2 = 0x" << std::hex << std::setfill('0') << std::setw(REG_OUTPUT_WIDTH) << reg.s2 << std::endl;
        reg_out << "$s3 = 0x" << std::hex << std::setfill('0') << std::setw(REG_OUTPUT_WIDTH) << reg.s3 << std::endl;
        reg_out << "$s4 = 0x" << std::hex << std::setfill('0') << std::setw(REG_OUTPUT_WIDTH) << reg.s4 << std::endl;
        reg_out << "$s5 = 0x" << std::hex << std::setfill('0') << std::setw(REG_OUTPUT_WIDTH) << reg.s5 << std::endl;
        reg_out << "$s6 = 0x" << std::hex << std::setfill('0') << std::setw(REG_OUTPUT_WIDTH) << reg.s6 << std::endl;
        reg_out << "$s7 = 0x" << std::hex << std::setfill('0') << std::setw(REG_OUTPUT_WIDTH) << reg.s7 << std::endl;
        reg_out << "$s8 = 0x" << std::hex << std::setfill('0') << std::setw(REG_OUTPUT_WIDTH) << reg.s8 << std::endl;
        reg_out << "$s9 = 0x" << std::hex << std::setfill('0') << std::setw(REG_OUTPUT_WIDTH) << reg.s9 << std::endl;
        reg_out << "$s10 = 0x" << std::hex << std::setfill('0') << std::setw(REG_OUTPUT_WIDTH) << reg.s10 << std::endl;
        reg_out << "$s11 = 0x" << std::hex << std::setfill('0') << std::setw(REG_OUTPUT_WIDTH) << reg.s11 << std::endl;
        reg_out << std::endl;
        reg_out << "$t3 = 0x" << std::hex << std::setfill('0') << std::setw(REG_OUTPUT_WIDTH) << reg.t3 << std::endl;
        reg_out << "$t4 = 0x" << std::hex << std::setfill('0') << std::setw(REG_OUTPUT_WIDTH) << reg.t4 << std::endl;
        reg_out << "$t5 = 0x" << std::hex << std::setfill('0') << std::setw(REG_OUTPUT_WIDTH) << reg.t5 << std::endl;
        reg_out << "$t6 = 0x" << std::hex << std::setfill('0') << std::setw(REG_OUTPUT_WIDTH) << reg.t6 << std::endl;
        reg_out << "---------------------" << std::endl;
        reg_out << "End Register Values" << std::endl;
        reg_out << "---------------------" << std::endl;
        return SUCCESS;
    } else {
        std::cerr << "Could not create register state dump file" << std::endl;
        return ERROR;
    }
}
