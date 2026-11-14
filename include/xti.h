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

/* <xti.h>: SUSv2 (X/Open Transport Interface, XNS Issue 5) */
#ifndef _XTI_H
#define _XTI_H
#include <bits/features.h>

#if __XSI_OB5
#define __NEED_size_t
#include <bits/alltypes.h>
#include <bits/xtitypes.h>

#define TBADADDR      1
#define TBADOPT       2
#define TACCES        3
#define TBADF         4
#define TNOADDR       5
#define TOUTSTATE     6
#define TBADSEQ       7
#define TSYSERR       8
#define TLOOK         9
#define TBADDATA      10
#define TBUFOVFLW     11
#define TFLOW         12
#define TNODATA       13
#define TNODIS        14
#define TNOUDERR      15
#define TBADFLAG      16
#define TNOREL        17
#define TNOTSUPPORT   18
#define TSTATECHNG    19
#define TNOSTRUCTYPE  20
#define TBADNAME      21
#define TBADQLEN      22
#define TADDRBUSY     23
#define TINDOUT       24
#define TPROVMISMATCH 25
#define TRESQLEN      26
#define TRESADDR      27
#define TQFULL        28
#define TPROTO        29

#define T_LISTEN     0x0001
#define T_CONNECT    0x0002
#define T_DATA       0x0004
#define T_EXDATA     0x0008
#define T_DISCONNECT 0x0010
#define T_UDERR      0x0040
#define T_ORDREL     0x0080
#define T_GODATA     0x0100
#define T_GOEXDATA   0x0200

#define T_MORE        0x001
#define T_EXPEDITED   0x002
#define T_PUSH        0x004
#define T_NEGOTIATE   0x004
#define T_CHECK       0x008
#define T_DEFAULT     0x010
#define T_SUCCESS     0x020
#define T_FAILURE     0x040
#define T_CURRENT     0x080
#define T_PARTSUCCESS 0x100
#define T_READONLY    0x200
#define T_NOTSUPPORT  0x400

__BEGIN_DECLS
int *_t_errno(void);
__END_DECLS
#define t_errno (*(_t_errno()))

#define T_IOV_MAX 16

struct t_iovec {
	void *iov_base;
	size_t iov_len;
};

struct t_info {
	t_scalar_t addr;
	t_scalar_t options;
	t_scalar_t tsdu;
	t_scalar_t etsdu;
	t_scalar_t connect;
	t_scalar_t discon;
	t_scalar_t servtype;
	t_scalar_t flags;
};

#define T_COTS     01
#define T_COTS_ORD 02
#define T_CLTS     03

#define T_SENDZERO   0x001
#define T_ORDRELDATA 0x002

struct netbuf {
	unsigned int maxlen;
	unsigned int len;
	void *buf;
};

struct t_opthdr {
	t_uscalar_t len;
	t_uscalar_t level;
	t_uscalar_t name;
	t_uscalar_t status;
};

struct t_bind {
	struct netbuf addr;
	unsigned int qlen;
};

struct t_optmgmt {
	struct netbuf opt;
	t_scalar_t flags;
};

struct t_discon {
	struct netbuf udata;
	int reason;
	int sequence;
};

struct t_call {
	struct netbuf addr;
	struct netbuf opt;
	struct netbuf udata;
	int sequence;
};

struct t_unitdata {
	struct netbuf addr;
	struct netbuf opt;
	struct netbuf udata;
};

struct t_uderr {
	struct netbuf addr;
	struct netbuf opt;
	t_scalar_t error;
};

#define T_BIND      1
#define T_OPTMGMT   2
#define T_CALL      3
#define T_DIS       4
#define T_UNITDATA  5
#define T_UDERROR   6
#define T_INFO      7

#define T_ADDR  0x01
#define T_OPT   0x02
#define T_UDATA 0x04
#define T_ALL   0xffff

#define T_UNBND    1
#define T_IDLE     2
#define T_OUTCON   3
#define T_INCON    4
#define T_DATAXFER 5
#define T_OUTREL   6
#define T_INREL    7

#define T_YES        1
#define T_NO         0
#define T_NULL       0
#define T_ABSREQ     0x8000
#define T_INFINITE   (-1)
#define T_INVALID    (-2)

#define _SC_T_IOV_MAX 1

#define T_UNSPEC  (~0 - 2)
#define T_ALLOPT  0
#define T_ALIGN(p) (((t_uscalar_t)(p) + (sizeof (t_scalar_t) - 1)) & ~(sizeof (t_scalar_t) - 1))
#define T_OPT_DATA(tohp) ((unsigned char *)(tohp) + T_ALIGN(sizeof (struct t_opthdr)))
#define T_OPT_NEXTHDR(pbuf, buflen, popt) \
	(((char *)(popt) + T_ALIGN((popt)->len) < (char *)(pbuf) + (buflen)) ? \
	(struct t_opthdr *)((char *)(popt) + T_ALIGN((popt)->len)) : (struct t_opthdr *)0)
#define T_OPT_FIRSTHDR(nbp) \
	(((nbp)->buf != 0 && (nbp)->len >= sizeof (struct t_opthdr)) ? \
	(struct t_opthdr *)(nbp)->buf : (struct t_opthdr *)0)
#define T_OPT_NXTHDR(nbp, tohp) T_OPT_NEXTHDR((nbp)->buf, (nbp)->len, (tohp))

#define XTI_GENERIC  0xffff

#define XTI_DEBUG    0x0001
#define XTI_LINGER   0x0080
#define XTI_RCVBUF   0x1002
#define XTI_RCVLOWAT 0x1004
#define XTI_SNDBUF   0x1001
#define XTI_SNDLOWAT 0x1003

struct t_linger {
	t_scalar_t l_onoff;
	t_scalar_t l_linger;
};

#define T_CLASS0 0
#define T_CLASS1 1
#define T_CLASS2 2
#define T_CLASS3 3
#define T_CLASS4 4

#define T_PRITOP  0
#define T_PRIHIGH 1
#define T_PRIMID  2
#define T_PRILOW  3
#define T_PRIDFLT 4

#define T_NOPROTECT      1
#define T_PASSIVEPROTECT 2
#define T_ACTIVEPROTECT  4

#define T_LTPDUDFLT 128

struct rate {
	t_scalar_t targetvalue;
	t_scalar_t minacceptvalue;
};

struct reqvalue {
	struct rate called;
	struct rate calling;
};

struct thrpt {
	struct reqvalue maxthrpt;
	struct reqvalue avgthrpt;
};

struct transdel {
	struct reqvalue maxdel;
	struct reqvalue avgdel;
};

#define T_ISO_TP 0x0100
#define ISO_TP   0x0100

#define T_TCO_THROUGHPUT     0x0001
#define TCO_THROUGHPUT       0x0001
#define T_TCO_TRANSDEL       0x0002
#define TCO_TRANSDEL         0x0002
#define T_TCO_RESERRORRATE   0x0003
#define TCO_RESERRORRATE     0x0003
#define T_TCO_TRANSFFAILPROB 0x0004
#define TCO_TRANSFFAILPROB   0x0004
#define T_TCO_ESTFAILPROB    0x0005
#define TCO_ESTFAILPROB      0x0005
#define T_TCO_RELFAILPROB    0x0006
#define TCO_RELFAILPROB      0x0006
#define T_TCO_ESTDELAY       0x0007
#define TCO_ESTDELAY         0x0007
#define T_TCO_RELDELAY       0x0008
#define TCO_RELDELAY         0x0008
#define T_TCO_CONNRESIL      0x0009
#define TCO_CONNRESIL        0x0009
#define T_TCO_PROTECTION     0x000a
#define TCO_PROTECTION       0x000a
#define T_TCO_PRIORITY       0x000b
#define TCO_PRIORITY         0x000b
#define T_TCO_EXPD           0x000c
#define TCO_EXPD             0x000c
#define T_TCL_TRANSDEL       0x000d
#define TCL_TRANSDEL         0x000d
#define T_TCL_RESERRORRATE   T_TCO_RESERRORRATE
#define TCL_RESERRORRATE     T_TCO_RESERRORRATE
#define T_TCL_PROTECTION     T_TCO_PROTECTION
#define TCL_PROTECTION       T_TCO_PROTECTION
#define T_TCL_PRIORITY       T_TCO_PRIORITY
#define TCL_PRIORITY         T_TCO_PRIORITY

#define T_TCO_LTPDU      0x0100
#define TCO_LTPDU        0x0100
#define T_TCO_ACKTIME    0x0200
#define TCO_ACKTIME      0x0200
#define T_TCO_REASTIME   0x0300
#define TCO_REASTIME     0x0300
#define T_TCO_EXTFORM    0x0400
#define TCO_EXTFORM      0x0400
#define T_TCO_FLOWCTRL   0x0500
#define TCO_FLOWCTRL     0x0500
#define T_TCO_CHECKSUM   0x0600
#define TCO_CHECKSUM     0x0600
#define T_TCO_NETEXP     0x0700
#define TCO_NETEXP       0x0700
#define T_TCO_NETRECPTCF 0x0800
#define TCO_NETRECPTCF   0x0800
#define T_TCO_PREFCLASS  0x0900
#define TCO_PREFCLASS    0x0900
#define T_TCO_ALTCLASS1  0x0a00
#define TCO_ALTCLASS1    0x0a00
#define T_TCO_ALTCLASS2  0x0b00
#define TCO_ALTCLASS2    0x0b00
#define T_TCO_ALTCLASS3  0x0c00
#define TCO_ALTCLASS3    0x0c00
#define T_TCO_ALTCLASS4  0x0d00
#define TCO_ALTCLASS4    0x0d00
#define T_TCL_CHECKSUM   T_TCO_CHECKSUM
#define TCL_CHECKSUM     T_TCO_CHECKSUM

#define T_INET_TCP 0x6
#define INET_TCP   0x6

/* TCP_NODELAY (1) and TCP_MAXSEG (2) are those of <netinet/tcp.h> */
#include <netinet/tcp.h>
#define T_TCP_NODELAY   0x1
#define T_TCP_MAXSEG    0x2
#define T_TCP_KEEPALIVE 0x8
#define TCP_KEEPALIVE   0x8

struct t_kpalive {
	t_scalar_t kp_onoff;
	t_scalar_t kp_timeout;
};

#define T_GARBAGE 0x02

#define T_INET_UDP 0x11
#define INET_UDP   0x11

#define T_UDP_CHECKSUM T_TCO_CHECKSUM
#define UDP_CHECKSUM   T_TCO_CHECKSUM

#define T_INET_IP 0x0
#define INET_IP   0x0

/* the LEGACY IP_OPTIONS, IP_TOS, IP_TTL, IP_REUSEADDR, IP_DONTROUTE and
   IP_BROADCAST are not defined: <netinet/in.h> has IP_TOS and IP_TTL with
   other values (the socket options) */
#define T_IP_OPTIONS   0x1
#define T_IP_TOS       0x2
#define T_IP_TTL       0x3
#define T_IP_REUSEADDR 0x4
#define T_IP_DONTROUTE 0x10
#define T_IP_BROADCAST 0x20

#define T_ROUTINE       0
#define T_PRIORITY      1
#define T_IMMEDIATE     2
#define T_FLASH         3
#define T_OVERRIDEFLASH 4
#define T_CRITIC_ECP    5
#define T_INETCONTROL   6
#define T_NETCONTROL    7

#define T_NOTOS   0
#define T_LDELAY  (1 << 4)
#define T_HITHRPT (1 << 3)
#define T_HIREL   (1 << 2)
#define T_LOCOST  (1 << 1)
#define SET_TOS(prec, tos) ((0x7 & (prec)) << 5 | (0x1c & (tos)))

__BEGIN_DECLS
int t_accept(int, int, const struct t_call *);
void *t_alloc(int, int, int);
int t_bind(int, const struct t_bind *, struct t_bind *);
int t_close(int);
int t_connect(int, const struct t_call *, struct t_call *);
int t_error(const char *);
int t_free(void *, int);
int t_getinfo(int, struct t_info *);
int t_getprotaddr(int, struct t_bind *, struct t_bind *);
int t_getstate(int);
int t_listen(int, struct t_call *);
int t_look(int);
int t_open(const char *, int, struct t_info *);
int t_optmgmt(int, const struct t_optmgmt *, struct t_optmgmt *);
int t_rcv(int, void *, unsigned int, int *);
int t_rcvconnect(int, struct t_call *);
int t_rcvdis(int, struct t_discon *);
int t_rcvrel(int);
int t_rcvreldata(int, struct t_discon *);
int t_rcvudata(int, struct t_unitdata *, int *);
int t_rcvuderr(int, struct t_uderr *);
int t_rcvv(int, struct t_iovec *, unsigned int, int *);
int t_rcvvudata(int, struct t_unitdata *, struct t_iovec *, unsigned int, int *);
int t_snd(int, void *, unsigned int, int);
int t_snddis(int, const struct t_call *);
int t_sndrel(int);
int t_sndreldata(int, struct t_discon *);
int t_sndudata(int, const struct t_unitdata *);
int t_sndv(int, const struct t_iovec *, unsigned int, int);
int t_sndvudata(int, struct t_unitdata *, struct t_iovec *, unsigned int);
const char *t_strerror(int);
int t_sync(int);
int t_sysconf(int);
int t_unbind(int);
__END_DECLS
#endif
#endif
