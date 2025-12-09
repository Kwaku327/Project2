.section .text
.globl _start
_start:
    la   t0, data
    lw   t1, 0(t0)        # load
    addi t1, t1, 1        # uses loaded value (load-use hazard)
    sw   t1, 4(t0)        # store result
    .word 0xfeedfeed

.section .data
data:
    .word 5
    .word 0
