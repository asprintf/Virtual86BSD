# Copyright (c) 2026, Joost Orchidee Winter
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are met:
#
# 1. Redistributions of source code must retain the above copyright notice, this
#    list of conditions and the following disclaimer.
#
# 2. Redistributions in binary form must reproduce the above copyright notice,
#    this list of conditions and the following disclaimer in the documentation
#    and/or other materials provided with the distribution.
#
# 3. Neither the name of the copyright holder nor the names of its
#    contributors may be used to endorse or promote products derived from
#    this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
# AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
# DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
# FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
# SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
# CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
# OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

# The time-zone variables, each with a library-internal name used by the
# ISO C functions and a weak POSIX/XSI name (tzname, timezone, daylight),
# which a strictly conforming ISO C program may define for itself.
	.section .rodata.str1.1,"aMS",@progbits,1
.Lutc:	.asciz	"UTC"

	.data
	.align	8
	.globl	__tzname
	.type	__tzname,@object
	.size	__tzname,16
	.weak	tzname
	.type	tzname,@object
	.size	tzname,16
__tzname:
tzname:
	.quad	.Lutc
	.quad	.Lutc

	.bss
	.align	8
	.globl	__timezone
	.type	__timezone,@object
	.size	__timezone,8
	.weak	timezone
	.type	timezone,@object
	.size	timezone,8
__timezone:
timezone:
	.zero	8

	.align	4
	.globl	__daylight
	.type	__daylight,@object
	.size	__daylight,4
	.weak	daylight
	.type	daylight,@object
	.size	daylight,4
__daylight:
daylight:
	.zero	4
	.section .note.GNU-stack,"",@progbits
