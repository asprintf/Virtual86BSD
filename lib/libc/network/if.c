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

/* <net/if.h>: interface names and indexes, from the kernel by ioctl;
 * the list of interfaces from /proc/net/dev. */
#include <errno.h>
#include <net/if.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <unistd.h>

struct ifreq_min {               /* struct ifreq: name, then a union */
	char name[IF_NAMESIZE];
	union { int index; char pad[24]; } u;
};

unsigned if_nametoindex(const char *name)
{
	struct ifreq_min r;
	int fd, ok;
	if (strlen(name) >= IF_NAMESIZE)
		return 0;
	memset(&r, 0, sizeof r);
	strcpy(r.name, name);
	if ((fd = socket(AF_UNIX, SOCK_DGRAM | SOCK_CLOEXEC, 0)) < 0)
		return 0;
	ok = ioctl(fd, SIOCGIFINDEX, &r) == 0;
	close(fd);
	return ok ? (unsigned)r.u.index : 0;
}

char *if_indextoname(unsigned index, char *name)
{
	struct ifreq_min r;
	int fd, ok;
	memset(&r, 0, sizeof r);
	r.u.index = (int)index;
	if ((fd = socket(AF_UNIX, SOCK_DGRAM | SOCK_CLOEXEC, 0)) < 0)
		return 0;
	ok = ioctl(fd, SIOCGIFNAME, &r) == 0;
	close(fd);
	if (!ok) {
		if (errno == ENODEV)
			errno = ENXIO;
		return 0;
	}
	r.name[IF_NAMESIZE - 1] = 0;
	return strcpy(name, r.name);
}

struct if_nameindex *if_nameindex(void)
{
	FILE *f = fopen("/proc/net/dev", "re");
	struct if_nameindex *list = 0, *l;
	char line[512], *p, *q;
	size_t n = 0;
	if (!f)
		return 0;
	while (fgets(line, sizeof line, f)) {
		if (!(q = strchr(line, ':')))
			continue;
		*q = 0;
		for (p = line; *p == ' '; p++)
			;
		if (!(l = realloc(list, (n + 2) * sizeof *list)))
			goto fail;
		list = l;
		list[n].if_name = 0;
		if (strlen(p) >= IF_NAMESIZE || !(list[n].if_index = if_nametoindex(p)))
			continue;
		if (!(list[n].if_name = strdup(p)))
			goto fail;
		n++;
	}
	fclose(f);
	if (!list && !(list = malloc(sizeof *list)))
		return 0;
	{       /* in the order of the indexes, as the kernel lists them */
		size_t i, j;
		for (i = 1; i < n; i++)
			for (j = i; j > 0 && list[j - 1].if_index > list[j].if_index; j--) {
				struct if_nameindex t = list[j];
				list[j] = list[j - 1];
				list[j - 1] = t;
			}
	}
	list[n].if_index = 0;
	list[n].if_name = 0;
	return list;
fail:
	fclose(f);
	while (n--)
		free(list[n].if_name);
	free(list);
	errno = ENOBUFS;
	return 0;
}

void if_freenameindex(struct if_nameindex *list)
{
	struct if_nameindex *l;
	for (l = list; l->if_name; l++)
		free(l->if_name);
	free(list);
}
