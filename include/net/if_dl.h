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

/* <net/if_dl.h>: a link level address (AF_LINK), what getifaddrs() gives
   for an interface's hardware address.  The BSDs' sockaddr_dl without
   sdl_len (a struct sockaddr here has none): sdl_nlen is 0, the name is the
   ifa_name, and the address comes first in sdl_data. */
#ifndef _NET_IF_DL_H_
#define _NET_IF_DL_H_
#include <sys/socket.h>

struct sockaddr_dl {
	sa_family_t sdl_family;  /* AF_LINK */
	unsigned short sdl_index; /* of the interface */
	unsigned char sdl_type;  /* IFT_ETHER, IFT_LOOP (<net/if_types.h>) */
	unsigned char sdl_nlen;  /* the length of the name in sdl_data: 0 */
	unsigned char sdl_alen;  /* the length of the address: 6, 0 for lo0 */
	unsigned char sdl_slen;  /* 0 */
	char sdl_data[12];
};

#define LLADDR(s) ((char *)((s)->sdl_data + (s)->sdl_nlen))

#endif
