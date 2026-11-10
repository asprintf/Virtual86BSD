/*
 * Copyright (c) 2026, Joost Orchidee Winter
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

#include <termios.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include "syscall.h"

#define TCGETS     0x5401
#define TCSETS     0x5402
#define TCSBRK     0x5409
#define TCXONC     0x540A
#define TCFLSH     0x540B
#define TIOCGPGRP  0x540F
#define TIOCSPGRP  0x5410
#define TIOCGWINSZ 0x5413
#define TIOCSWINSZ 0x5414
#define TIOCGSID   0x5429
#define CBAUD      0010017

int tcgetattr(int fd, struct termios *t)
{
	/* the kernel fills 19 of the NCCS control characters: the rest is
	 * zeroed (as glibc), so that termios structures can be compared */
	memset(t, 0, sizeof *t);
	if (__sys3(SYS_ioctl, fd, TCGETS, t) < 0) return -1;
	t->__c_ispeed = t->__c_ospeed = t->c_cflag & CBAUD;
	return 0;
}

int tcsetattr(int fd, int act, const struct termios *t)
{
	if ((unsigned)act > TCSAFLUSH) { errno = EINVAL; return -1; }
	return (int)__sys3(SYS_ioctl, fd, TCSETS + act, t);
}

int tcsendbreak(int fd, int dur) { (void)dur; return (int)__sys3(SYS_ioctl, fd, TCSBRK, 0); }
int tcdrain(int fd) { return (int)__sys_cp3(SYS_ioctl, fd, TCSBRK, 1); }
int tcflush(int fd, int q) { return (int)__sys3(SYS_ioctl, fd, TCFLSH, q); }
int tcflow(int fd, int a) { return (int)__sys3(SYS_ioctl, fd, TCXONC, a); }

pid_t tcgetsid(int fd)
{
	int sid;
	if (__sys3(SYS_ioctl, fd, TIOCGSID, &sid) < 0) return -1;
	return sid;
}

pid_t tcgetpgrp(int fd)
{
	int pg;
	if (__sys3(SYS_ioctl, fd, TIOCGPGRP, &pg) < 0) return -1;
	return pg;
}

int tcsetpgrp(int fd, pid_t pg)
{
	int p = pg;
	return (int)__sys3(SYS_ioctl, fd, TIOCSPGRP, &p);
}

int tcgetwinsize(int fd, struct winsize *w) { return (int)__sys3(SYS_ioctl, fd, TIOCGWINSZ, w); }
int tcsetwinsize(int fd, const struct winsize *w) { return (int)__sys3(SYS_ioctl, fd, TIOCSWINSZ, w); }

speed_t cfgetospeed(const struct termios *t) { return t->c_cflag & CBAUD; }
speed_t cfgetispeed(const struct termios *t) { return cfgetospeed(t); }

int cfsetospeed(struct termios *t, speed_t s)
{
	if (s & ~(speed_t)CBAUD) { errno = EINVAL; return -1; }
	t->c_cflag = (t->c_cflag & ~(tcflag_t)CBAUD) | s;
	return 0;
}

/* Linux has one speed for both directions; 0 means "same as output". */
int cfsetispeed(struct termios *t, speed_t s) { return s ? cfsetospeed(t, s) : 0; }
