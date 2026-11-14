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

/* <stropts.h>: SUSv2 to SUSv4 (STREAMS) */
#ifndef _STROPTS_H
#define _STROPTS_H
#include <bits/features.h>

#if __XSI_OB
#define __NEED_uid_t
#define __NEED_gid_t
#include <bits/alltypes.h>
#include <bits/xtitypes.h>
#include <sys/ioctl.h>

#define __SID ('S' << 8)

#define I_NREAD     (__SID | 1)
#define I_PUSH      (__SID | 2)
#define I_POP       (__SID | 3)
#define I_LOOK      (__SID | 4)
#define I_FLUSH     (__SID | 5)
#define I_SRDOPT    (__SID | 6)
#define I_GRDOPT    (__SID | 7)
#define I_STR       (__SID | 8)
#define I_SETSIG    (__SID | 9)
#define I_GETSIG    (__SID | 10)
#define I_FIND      (__SID | 11)
#define I_LINK      (__SID | 12)
#define I_UNLINK    (__SID | 13)
#define I_RECVFD    (__SID | 14)
#define I_PEEK      (__SID | 15)
#define I_FDINSERT  (__SID | 16)
#define I_SENDFD    (__SID | 17)
#define I_SWROPT    (__SID | 19)
#define I_GWROPT    (__SID | 20)
#define I_LIST      (__SID | 21)
#define I_PLINK     (__SID | 22)
#define I_PUNLINK   (__SID | 23)
#define I_FLUSHBAND (__SID | 28)
#define I_CKBAND    (__SID | 29)
#define I_GETBAND   (__SID | 30)
#define I_ATMARK    (__SID | 31)
#define I_SETCLTIME (__SID | 32)
#define I_GETCLTIME (__SID | 33)
#define I_CANPUT    (__SID | 34)

#define FMNAMESZ 8

#define FLUSHR  0x01
#define FLUSHW  0x02
#define FLUSHRW 0x03
#define FLUSHBAND 0x04

#define S_INPUT   0x0001
#define S_HIPRI   0x0002
#define S_OUTPUT  0x0004
#define S_MSG     0x0008
#define S_ERROR   0x0010
#define S_HANGUP  0x0020
#define S_RDNORM  0x0040
#define S_WRNORM  S_OUTPUT
#define S_RDBAND  0x0080
#define S_WRBAND  0x0100
#define S_BANDURG 0x0200

#define RS_HIPRI 0x01

#define RNORM     0x01
#define RMSGD     0x02
#define RMSGN     0x04
#define RPROTDAT  0x08
#define RPROTDIS  0x10
#define RPROTNORM 0x20

#define SNDZERO 0x01

#define ANYMARK  0x01
#define LASTMARK 0x02

#define MUXID_ALL (-1)

#define MSG_HIPRI 0x01
#define MSG_ANY   0x02
#define MSG_BAND  0x04

#define MORECTL  1
#define MOREDATA 2

struct strbuf {
	int maxlen;
	int len;
	char *buf;
};

struct bandinfo {
	unsigned char bi_pri;
	int bi_flag;
};

struct strpeek {
	struct strbuf ctlbuf;
	struct strbuf databuf;
	t_uscalar_t flags;
};

struct strfdinsert {
	struct strbuf ctlbuf;
	struct strbuf databuf;
	t_uscalar_t flags;
	int fildes;
	int offset;
};

struct strioctl {
	int ic_cmd;
	int ic_timout;
	int ic_len;
	char *ic_dp;
};

struct strrecvfd {
	int fd;
	uid_t uid;
	gid_t gid;
	char fill[8];
};

struct str_mlist {
	char l_name[FMNAMESZ + 1];
};

struct str_list {
	int sl_nmods;
	struct str_mlist *sl_modlist;
};

__BEGIN_DECLS
int isastream(int);
int getmsg(int, struct strbuf *__RESTRICT, struct strbuf *__RESTRICT, int *__RESTRICT);
int getpmsg(int, struct strbuf *__RESTRICT, struct strbuf *__RESTRICT, int *__RESTRICT, int *__RESTRICT);
int putmsg(int, const struct strbuf *, const struct strbuf *, int);
int putpmsg(int, const struct strbuf *, const struct strbuf *, int, int);
int fattach(int, const char *);
int fdetach(const char *);
__END_DECLS
#endif
#endif
