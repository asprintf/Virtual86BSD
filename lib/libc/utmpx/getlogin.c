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

#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include <pwd.h>
#include <limits.h>
#include "utx_impl.h"

/* The login name of the session: the USER_PROCESS entry in the user
 * accounting database for the controlling terminal (as found on standard
 * input, output or error); failing that, the kernel's audit login uid. */
int getlogin_r(char *name, size_t size)
{
	char tty[64], *line, buf[1024];
	unsigned char r[UTX_RECSIZE];
	struct utmpx u;
	struct passwd pw, *res;
	int i, fd, e = errno, found = 0;
	unsigned long uid = 0;
	ssize_t n;
	size_t l;

	for (i = 0; i < 3; i++)
		if (!ttyname_r(i, tty, sizeof tty)) break;
	if (i < 3 && (fd = __utx_open(O_RDONLY)) >= 0) {
		line = strncmp(tty, "/dev/", 5) ? tty : tty + 5;
		while (read(fd, r, sizeof r) == (ssize_t)sizeof r) {
			__utx_decode(&u, r);
			if (u.ut_type == USER_PROCESS && !strncmp(u.ut_line, line, sizeof u.ut_line) && u.ut_user[0]) {
				found = 1;
				break;
			}
		}
		close(fd);
		if (found) {
			l = strnlen(u.ut_user, sizeof u.ut_user);
			if (l >= size) return ERANGE;
			memcpy(name, u.ut_user, l);
			name[l] = 0;
			errno = e;
			return 0;
		}
	}
	if ((fd = open("/proc/self/loginuid", O_RDONLY | O_CLOEXEC)) >= 0) {
		n = read(fd, buf, sizeof buf - 1);
		close(fd);
		if (n > 0) {
			buf[n] = 0;
			for (i = 0; buf[i] >= '0' && buf[i] <= '9'; i++) uid = uid * 10 + (unsigned)(buf[i] - '0');
			if (i && uid != 0xffffffffUL && !getpwuid_r((uid_t)uid, &pw, buf, sizeof buf, &res) && res) {
				l = strlen(pw.pw_name);
				if (l >= size) return ERANGE;
				memcpy(name, pw.pw_name, l + 1);
				errno = e;
				return 0;
			}
		}
	}
	errno = e;
	return ENXIO;
}

char *getlogin(void)
{
	static char name[LOGIN_NAME_MAX];
	int r = getlogin_r(name, sizeof name);
	if (r) { errno = r; return 0; }
	return name;
}
