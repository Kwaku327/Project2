.section .text
.globl _start
_start:
    addi t1, zero, 5
    addi t2, zero, 4
    add  t3, t1, t2      # arithmetic result
    beq  t3, t1, skip    # branch depends on previous result (not taken)
    addi t4, zero, 1
skip:
    .word 0xfeedfeed
