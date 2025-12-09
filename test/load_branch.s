.section .text
.globl _start
_start:
    la   t0, flag
    lw   t1, 0(t0)        # load value
    beq  t1, zero, done   # branch depends on loaded value (not taken)
    addi t2, zero, 2
done:
    .word 0xfeedfeed

.section .data
flag:
    .word 1
