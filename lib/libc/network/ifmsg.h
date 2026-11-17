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

/* Internal: the interface list of the kernel, sysctl(CTL_NET, PF_ROUTE, 0, 0,
 * NET_RT_IFLISTL, 0): messages with the interfaces (RTM_IFINFO) and their
 * addresses (RTM_NEWADDR), each followed by socket addresses packed in the
 * order of the RTA_ bits. */
#ifndef _NETWORK_IFMSG_H
#define _NETWORK_IFMSG_H
#include <stddef.h>

#define IFM_VERSION 5
#define IFM_RTM_NEWADDR 0xc
#define IFM_RTM_IFINFO 0xe
#define IFM_RTA_NETMASK 0x04
#define IFM_RTA_IFP 0x10
#define IFM_RTA_IFA 0x20
#define IFM_RTA_BRD 0x80
#define IFM_RTAX_MAX 8

/* the part of if_msghdrl and ifa_msghdrl that is the same */
struct ifmsg {
	unsigned short msglen;
	unsigned char version, type;
	int addrs;
	int flags;
	unsigned short index, spare1;
	unsigned short len;        /* the header: the addresses follow it */
};

/* the list in a malloc'ed buffer (freed by the caller), its size in *len; 0 or -1 with errno */
int __ifmsg_list(char **buf, size_t *len);
/* the address in slot bit (an RTA_ value) of a message, or 0 */
const struct sockaddr *__ifmsg_addr(const struct ifmsg *m, int bit);
#endif
