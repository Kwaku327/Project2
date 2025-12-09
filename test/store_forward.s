.section .text
.globl _start
_start:
    la   t0, buf
    lw   t1, 0(t0)        # load value
    sw   t1, 4(t0)        # store uses forwarded load result (WB->MEM)
    lw   t2, 4(t0)        # read back stored value
    .word 0xfeedfeed

.section .data
buf:
    .word 7
    .word 0
