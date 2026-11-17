/*
 * Copyright (c) 2026, asprintf
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this
 *    list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the copyright holder nor the names of its
 *    contributors may be used to endorse or promote products derived from
 *    this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/* testinit.c - the first /sbin/init of the FreeBSD kernel switch: writes to
   the FreeBSD system calls directly (no libc), prints one line to the
   console and powers off.  Build: clang -static -nostdlib -ffreestanding. */

#define SYS_exit	1
#define SYS_write	4
#define SYS_open	5
#define SYS_getpid	20
#define SYS_sync	36
#define SYS_ioctl	54
#define SYS_reboot	55

#define O_RDWR		2
#define TIOCDRAIN	0x2000745eUL	/* wait until the tty output is out */
#define RB_POWEROFF	0x4000

/* amd64: number in rax, arguments in rdi rsi rdx r10 r8 r9; CF set on error
   with the positive errno in rax */
static long
sys3(long n, long a, long b, long c)
{
	long r;
	register long r10 __asm__("r10") = 0;

	__asm__ volatile("syscall; jnc 1f; neg %%rax; 1:"
	    : "=a"(r), "+D"(a), "+S"(b), "+d"(c), "+r"(r10)
	    : "0"(n)
	    : "rcx", "r11", "r8", "r9", "memory", "cc");
	return r;
}

static long
slen(const char *s)
{
	long n = 0;

	while (s[n])
		n++;
	return n;
}

static long fd = 1;

static void
puts1(const char *s)
{
	sys3(SYS_write, fd, (long)s, slen(s));
}

static void
putnum(long v)
{
	char b[24];
	int i = sizeof b - 1;

	b[i] = 0;
	do
		b[--i] = '0' + v % 10;
	while ((v /= 10) != 0);
	puts1(b + i);
}

void
_start(void)
{
	/* the kernel starts init with no descriptors open */
	fd = sys3(SYS_open, (long)"/dev/console", O_RDWR, 0);
	puts1("V86BSD init: hello from /sbin/init, pid ");
	putnum(sys3(SYS_getpid, 0, 0, 0));
	puts1("\n");
	sys3(SYS_ioctl, fd, TIOCDRAIN, 0);
	sys3(SYS_sync, 0, 0, 0);
	sys3(SYS_reboot, RB_POWEROFF, 0, 0);
	for (;;)
		sys3(SYS_exit, 0, 0, 0);
}

/* the ELF note that brands the binary FreeBSD (its desc is __FreeBSD_version) */
__asm__(".section .note.tag,\"a\",@note\n"
	".balign 4\n"
	".long 8, 4, 1\n"
	".asciz \"FreeBSD\"\n"
	".balign 4\n"
	".long 1502000\n"
	".previous\n");
