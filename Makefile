# Makefile

# Build targets:
# make sim_cycle # build sim_cycle
# make sim_funct # build sim_funct
# make all # build sim_funct, sim_cycle and all tests
# make tests # build all assembly tests
# make clean $ removes sim_cycle, sim_funct, and all .bin and .elf files in test/

# Note: If you're having trouble getting the assembler and objcopy executables to work,
# you might need to mark those files as executables using 'chmod +x filename'

# This Makefile checks if it's being run on one of the Nobel machines
ALLOWED_HOSTS = davisson.princeton.edu compton.princeton.edu
CURRENT_HOST := $(shell hostname)

HOST_OK := $(filter $(CURRENT_HOST),$(ALLOWED_HOSTS))
ifeq ($(HOST_OK),)
    $(error This project is highly recommended to be completed on Nobel)
endif

# Compiler settings
CC = g++
# Note: All builds will contain debug information
CFLAGS = --std=c++14 -Wall -g -pedantic -O2

# Source and header files
SIM_FUNCT_SRC = sim_funct.cpp funct.cpp simulator.cpp MemoryStore.cpp Utilities.cpp
SIM_CYCLE_SRC = sim_cycle.cpp cycle.cpp cache.cpp simulator.cpp MemoryStore.cpp Utilities.cpp
SIM_FUNCT_SRCS = $(addprefix src/, $(SIM_FUNCT_SRC))
SIM_CYCLE_SRCS = $(addprefix src/, $(SIM_CYCLE_SRC))
COMMON_HDRS = $(wildcard src/*.h)

ASSEMBLY_TESTS = $(wildcard test/*.s)
ASSEMBLY_TARGETS = $(ASSEMBLY_TESTS:.s=.bin)

ASSEMBLER = bin/riscv64-elf-as
OBJCOPY = bin/riscv64-elf-objcopy

# Main targets
all: sim_funct sim_cycle tests

sim_funct: $(SIM_FUNCT_SRCS) $(COMMON_HDRS)
	$(CC) $(CFLAGS) -o sim_funct $(SIM_FUNCT_SRCS)

sim_cycle: $(SIM_CYCLE_SRCS) $(COMMON_HDRS)
	$(CC) $(CFLAGS) -o sim_cycle $(SIM_CYCLE_SRCS)

# Test targets
tests: $(ASSEMBLY_TARGETS)

$(ASSEMBLY_TARGETS) : test/%.bin : test/%.s
	$(ASSEMBLER) test/$*.s -o test/$*.elf
	$(OBJCOPY) test/$*.elf -j .text -O binary test/$*.bin

# Clean function
clean:
	rm -f sim_funct sim_cycle
	rm -f test/*.bin test/*.elf

# Phony targets
.PHONY: all debug tests clean

# To dump elf:
# riscv64-unknown-elf-objdump -D -j .text -M no-aliases *.elf