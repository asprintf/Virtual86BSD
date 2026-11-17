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

/* dmesg - show the kernel's messages: what the system printed since it
 * started (the last 64 KiB of it), read from the kernel's log buffer
 * (sysctl kern.msgbuf, a string).  Nothing is taken from the buffer: syslogd(8)
 * and dmesg can both show it. */
#define _POSIX_C_SOURCE 202405L
#define _XOPEN_SOURCE 800
#define _DEFAULT_SOURCE
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/sysctl.h>
#include <unistd.h>

int main(int argc, char *argv[])
{
	size_t len = 0;
	char *buf;

	(void)argv;
	if (argc != 1) {
		fprintf(stderr, "usage: dmesg\n");
		return 1;
	}
	if (sysctlbyname("kern.msgbuf", NULL, &len, NULL, 0) == -1 || len == 0) {
		fprintf(stderr, "dmesg: kern.msgbuf: %s\n", strerror(errno));
		return 1;
	}
	len += 4096;	/* the buffer grows while it is read */
	buf = malloc(len + 1);
	if (!buf) {
		fprintf(stderr, "dmesg: %s\n", strerror(errno));
		return 1;
	}
	if (sysctlbyname("kern.msgbuf", buf, &len, NULL, 0) == -1) {
		fprintf(stderr, "dmesg: kern.msgbuf: %s\n", strerror(errno));
		return 1;
	}
	if (len > 0 && buf[len - 1] == '\0')
		len--;
	if (len == 0)
		return 0;
	if (fwrite(buf, 1, len, stdout) != len)
		return 1;
	if (buf[len - 1] != '\n')
		putchar('\n');
	return fflush(stdout) == 0 ? 0 : 1;
}
