# Copyright 2017 Wade Lawrence Hennessey
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#   http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

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


	
