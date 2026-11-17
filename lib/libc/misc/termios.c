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
#include <time.h>
#include "syscall.h"

#include <sys/ioctl.h>

int tcgetattr(int fd, struct termios *t)
{
	return (int)__sys3(SYS_ioctl, fd, TIOCGETA, t);
}

int tcsetattr(int fd, int act, const struct termios *t)
{
	static const unsigned long req[3] = { TIOCSETA, TIOCSETAW, TIOCSETAF };
	if ((unsigned)act > TCSAFLUSH) { errno = EINVAL; return -1; }
	return (int)__sys3(SYS_ioctl, fd, req[act], t);
}

int tcsendbreak(int fd, int dur)
{
	struct timespec ts = { 0, 400000000L };   /* 0.4 s */
	(void)dur;
	if (__sys3(SYS_ioctl, fd, TIOCSBRK, 0) < 0) return -1;
	__syscall2(SYS_nanosleep, &ts, 0);
	return (int)__sys3(SYS_ioctl, fd, TIOCCBRK, 0);
}

int tcdrain(int fd) { return (int)__sys_cp3(SYS_ioctl, fd, TIOCDRAIN, 0); }

int tcflush(int fd, int q)
{
	int what;
	switch (q) {
	case TCIFLUSH: what = 1 /* FREAD */; break;
	case TCOFLUSH: what = 2 /* FWRITE */; break;
	case TCIOFLUSH: what = 3; break;
	default: errno = EINVAL; return -1;
	}
	return (int)__sys3(SYS_ioctl, fd, TIOCFLUSH, &what);
}

int tcflow(int fd, int a)
{
	struct termios t;
	char c;
	switch (a) {
	case TCOOFF: return (int)__sys3(SYS_ioctl, fd, TIOCSTOP, 0);
	case TCOON: return (int)__sys3(SYS_ioctl, fd, TIOCSTART, 0);
	case TCIOFF: case TCION:
		if (tcgetattr(fd, &t) < 0) return -1;
		c = (char)t.c_cc[a == TCIOFF ? VSTOP : VSTART];
		if (c == (char)0xff) return 0;
		return __syscall3(SYS_write, fd, &c, 1) == 1 ? 0 : -1;
	default: errno = EINVAL; return -1;
	}
}

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

speed_t cfgetospeed(const struct termios *t) { return t->__c_ospeed; }
speed_t cfgetispeed(const struct termios *t) { return t->__c_ispeed; }
int cfsetospeed(struct termios *t, speed_t s) { t->__c_ospeed = s; return 0; }
int cfsetispeed(struct termios *t, speed_t s) { t->__c_ispeed = s ? s : t->__c_ospeed; return 0; }
