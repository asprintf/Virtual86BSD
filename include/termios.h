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

#ifndef _TERMIOS_H
#define _TERMIOS_H
#include <bits/features.h>
#define __NEED_pid_t
#define __NEED_struct_winsize
#include <bits/alltypes.h>

typedef unsigned char cc_t;
typedef unsigned int  speed_t;
typedef unsigned int  tcflag_t;

#define NCCS 20

/* The kernel's layout (sys/sys/_termios.h). */
struct termios {
	tcflag_t c_iflag;
	tcflag_t c_oflag;
	tcflag_t c_cflag;
	tcflag_t c_lflag;
	cc_t c_cc[NCCS];
	speed_t __c_ispeed;
	speed_t __c_ospeed;
};

#define VEOF     0
#define VEOL     1
#define VERASE   3
#define VKILL    5
#define VINTR    8
#define VQUIT    9
#define VSUSP    10
#define VSTART   12
#define VSTOP    13
#define VMIN     16
#define VTIME    17
#if __EXT_VISIBLE
#define VEOL2    2
#define VWERASE  4
#define VREPRINT 6
#define VLNEXT   14
#define VDISCARD 15
#endif

#define IGNBRK  0x0001
#define BRKINT  0x0002
#define IGNPAR  0x0004
#define PARMRK  0x0008
#define INPCK   0x0010
#define ISTRIP  0x0020
#define INLCR   0x0040
#define IGNCR   0x0080
#define ICRNL   0x0100
#define IXON    0x0200
#define IXOFF   0x0400
#define IXANY   0x0800
#define IMAXBEL 0x2000
#define IUTF8   0x4000
#if __XSI_OB5
#define IUCLC   0x8000          /* SUSv2 (LEGACY): upper to lower case; the kernel ignores it */
#endif

#define OPOST  0x0001
#if __XSI_OB5
#define OLCUC  0x00100000       /* SUSv2 (LEGACY): lower to upper case; the kernel ignores it */
#endif
#if __XSI_VISIBLE
/* (the kernel has ONLCR, TAB3, OCRNL, ONOCR and ONLRET; the delay bits
 * and OFILL, OFDEL are accepted and ignored) */
#define ONLCR  0x0002
#define OCRNL  0x0010
#define ONOCR  0x0020
#define ONLRET 0x0040
#define OFILL  0x00004000
#define OFDEL  0x00008000
#define NLDLY  0x00000100
#define NL0    0x00000000
#define NL1    0x00000100
#define CRDLY  0x00000600
#define CR0    0x00000000
#define CR1    0x00000200
#define CR2    0x00000400
#define CR3    0x00000600
#define TABDLY 0x00010004
#define TAB0   0x00000000
#define TAB1   0x00010000
#define TAB2   0x00000000
#define TAB3   0x00000004
#define BSDLY  0x00000800
#define BS0    0x00000000
#define BS1    0x00000800
#define VTDLY  0x00001000
#define VT0    0x00000000
#define VT1    0x00001000
#define FFDLY  0x00002000
#define FF0    0x00000000
#define FF1    0x00002000
#endif

/* speeds are the numbers */
#define B0      0
#define B50     50
#define B75     75
#define B110    110
#define B134    134
#define B150    150
#define B200    200
#define B300    300
#define B600    600
#define B1200   1200
#define B1800   1800
#define B2400   2400
#define B4800   4800
#define B9600   9600
#define B19200  19200
#define B38400  38400
#define B57600  57600
#define B115200 115200
#define B230400 230400
#define B460800 460800

#define CSIZE  0x0300
#define CS5    0x0000
#define CS6    0x0100
#define CS7    0x0200
#define CS8    0x0300
#define CSTOPB 0x0400
#define CREAD  0x0800
#define PARENB 0x1000
#define PARODD 0x2000
#define HUPCL  0x4000
#define CLOCAL 0x8000

#define ISIG    0x0080
#define ICANON  0x0100
#define ECHO    0x0008
#define ECHOE   0x0002
#define ECHOK   0x0004
#define ECHONL  0x0010
#define NOFLSH  0x80000000
#define TOSTOP  0x00400000
#if __EXT_VISIBLE
#define ECHOCTL 0x0040
#define ECHOPRT 0x0020
#define ECHOKE  0x0001
#endif
#define IEXTEN  0x0400
#if __XSI_OB5
#define XCASE   0x00001000      /* SUSv2 (LEGACY): canonical upper/lower case; the kernel ignores it */
#endif

#define TCSANOW   0
#define TCSADRAIN 1
#define TCSAFLUSH 2

#define TCIFLUSH  1
#define TCOFLUSH  2
#define TCIOFLUSH 3

#define TCOOFF 1
#define TCOON  2
#define TCIOFF 3
#define TCION  4

__BEGIN_DECLS
speed_t cfgetispeed(const struct termios *);
speed_t cfgetospeed(const struct termios *);
int     cfsetispeed(struct termios *, speed_t);
int     cfsetospeed(struct termios *, speed_t);
int     tcdrain(int);
int     tcflow(int, int);
int     tcflush(int, int);
int     tcgetattr(int, struct termios *);
pid_t   tcgetsid(int);
int     tcgetwinsize(int, struct winsize *);
int     tcsendbreak(int, int);
int     tcsetattr(int, int, const struct termios *);
int     tcsetwinsize(int, const struct winsize *);
__END_DECLS
#endif
