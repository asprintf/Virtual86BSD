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

/* inet_lnaof, inet_makeaddr, inet_netof, inet_network: SUSv2 (XNS); classful, as 4.3BSD */
#define _DEFAULT_SOURCE
#include <arpa/inet.h>
#include <ctype.h>
#include <stdlib.h>

#define CLASSA(i) (((i) & 0x80000000u) == 0)
#define CLASSB(i) (((i) & 0xc0000000u) == 0x80000000u)

in_addr_t inet_lnaof(struct in_addr in)
{
	uint32_t i = ntohl(in.s_addr);
	if (CLASSA(i))
		return i & 0x00ffffff;
	if (CLASSB(i))
		return i & 0x0000ffff;
	return i & 0x000000ff;
}

in_addr_t inet_netof(struct in_addr in)
{
	uint32_t i = ntohl(in.s_addr);
	if (CLASSA(i))
		return i >> 24;
	if (CLASSB(i))
		return i >> 16;
	return i >> 8;
}

struct in_addr inet_makeaddr(in_addr_t net, in_addr_t lna)
{
	struct in_addr a;
	uint32_t i;
	if (net < 128)
		i = (net << 24) | (lna & 0x00ffffff);
	else if (net < 65536)
		i = (net << 16) | (lna & 0x0000ffff);
	else if (net < 16777216)
		i = (net << 8) | (lna & 0x000000ff);
	else
		i = net | lna;
	a.s_addr = htonl(i);
	return a;
}

in_addr_t inet_network(const char *cp)
{
	uint32_t v = 0;
	int parts = 0;
	for (;;) {
		char *end;
		unsigned long p;
		if (!isdigit((unsigned char)*cp))
			return (in_addr_t)-1;
		p = strtoul(cp, &end, 0);
		if (end == cp || p > 255 || ++parts > 4)
			return (in_addr_t)-1;
		v = (v << 8) | (uint32_t)p;
		cp = end;
		if (*cp == 0)
			return v;
		if (*cp++ != '.')
			return (in_addr_t)-1;
	}
}
