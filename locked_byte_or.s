.text
	.align	8
.globl locked_byte_or
locked_byte_or:
	lock orb 	%sil, (%rdi)
	ret

