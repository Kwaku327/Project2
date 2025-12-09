_start:
	li   t0, 60         # t0 = &array[0]
	li   t5, 108        # t5 = &size
	lw   t5, 0(t5)      # t5 = size
	li   t2, 1          # t2 = 1
	sw   t2, 0(t0)      # array[0] = 1
	sw   t2, 4(t0)      # array[1] = 1
	addi t1, t5, -2     # t1 = size-2

loop:
	lw   t3, 0(t0)      # t3 = array[n]
	lw   t4, 4(t0)      # t4 = array[n+1]
	add  t2, t3, t4     # t2 = t3 + t4
	sw   t2, 8(t0)      # array[n+2] = t2
	addi t0, t0, 4      # t0++
	addi t1, t1, -1     # t1--
	bgtz t1, loop       # if t1 > 0 goto loop

.word 0xfeedfeed

array:	.space 48		# 12 words for Fibonacci numbers
size:	.word 12


