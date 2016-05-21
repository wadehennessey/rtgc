	.text
	
	.align	8
	.global locked_long_or
locked_long_or:
	lock orq	%rsi, (%rdi)
	ret

	.align	8
	.global locked_long_and
locked_long_and:
	lock andq	%rsi, (%rdi)
	ret

	.align	8
	.global locked_long_inc
locked_long_inc:
	lock addq	$1, (%rdi)
	ret
	
	.align 8
.globl locked_byte_or
locked_byte_or:
	lock orb 	%sil, (%rdi)
	ret


	
