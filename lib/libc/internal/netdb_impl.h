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

/* Internal: name resolution (getaddrinfo, getnameinfo and their
 * helpers): /etc/hosts, /etc/services, the DNS. */
#ifndef _NETDB_IMPL_H
#define _NETDB_IMPL_H
#include <netdb.h>

struct __address {
	int family;
	unsigned scope;
	unsigned char addr[16];
	int sortkey;
};

struct __service {
	unsigned short port;
	unsigned char proto, socktype;
};

#define MAXADDRS 48
#define MAXSERVS 3

/* the files; the paths may be changed (by tests) */
extern const char *__hosts_path, *__services_path, *__resolv_conf_path;

/* a line's fields, separated by blanks, up to a '#' */
int __netdb_fields(char *line, char **f, int max);

/* the addresses of name (case-insensitive) in the hosts file: their
 * number, 0, or -1 when the file is missing; canon: the official name */
int __hosts_lookup(struct __address *buf, int max, char canon[256], const char *name, int family);
/* the name of an address in the hosts file: 1, or 0 */
int __hosts_reverse(const unsigned char *addr, int family, char *name, size_t n);

/* the DNS: the number of addresses, or EAI_NONAME, EAI_AGAIN, EAI_FAIL */
int __dns_lookup(struct __address *buf, int max, char canon[256], const char *name, int family);
/* PTR: 1, 0 (no name), or EAI_AGAIN/EAI_FAIL */
int __dns_reverse(const unsigned char *addr, int family, char *name, size_t n);

/* the port of a service name for "tcp" or "udp": the port (host
 * order), or -1 */
int __services_lookup(const char *name, const char *proto);

/* inet_aton: the old numeric forms (a, a.b, a.b.c, a.b.c.d): 1, or 0 */
struct in_addr;
int __inet_aton(const char *, struct in_addr *);

#endif
