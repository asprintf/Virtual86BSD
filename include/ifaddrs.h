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

/* <ifaddrs.h>: the addresses of the interfaces (not POSIX; as the BSDs). */
#ifndef _IFADDRS_H
#define _IFADDRS_H
#include <sys/socket.h>

struct ifaddrs {
	struct ifaddrs *ifa_next;
	char *ifa_name;                /* "em0" */
	unsigned int ifa_flags;        /* IFF_* of <net/if.h> */
	struct sockaddr *ifa_addr;     /* AF_INET, or AF_LINK (struct sockaddr_dl) */
	struct sockaddr *ifa_netmask;  /* AF_INET only */
	struct sockaddr *ifa_dstaddr;  /* the broadcast address (no point-to-point links) */
	void *ifa_data;                /* (unused) */
};
#define ifa_broadaddr ifa_dstaddr

__BEGIN_DECLS
int getifaddrs(struct ifaddrs **);
void freeifaddrs(struct ifaddrs *);
__END_DECLS

#endif
