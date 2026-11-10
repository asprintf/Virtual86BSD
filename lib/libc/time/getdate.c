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

/* getdate(): parse a date with the templates in the file named by
 * DATEMSK, then complete it as XSH getdate describes. */
#include <time.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <limits.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include "time_impl.h"
#include "strptime_impl.h"

static int err;
int *__getdate_err_location(void) { return &err; }

static char *readall(int fd, size_t *len)
{
	char *buf = 0, *nb;
	size_t cap = 0, n = 0;
	for (;;) {
		ssize_t r;
		if (n + 1 >= cap) {
			cap = cap ? 2 * cap : 4096;
			if (!(nb = realloc(buf, cap))) { free(buf); err = 6; return 0; }
			buf = nb;
		}
		r = read(fd, buf + n, cap - n - 1);
		if (r < 0) { free(buf); err = 5; return 0; }
		if (!r) break;
		n += (size_t)r;
	}
	buf[n] = 0;
	*len = n;
	return buf;
}

struct tm *getdate(const char *str)
{
	static struct tm res;
	const char *path = getenv("DATEMSK");
	char *data, *line, *next;
	struct stat st;
	size_t len;
	int fd;

	if (!path || !*path) { err = 1; return 0; }
	if ((fd = open(path, O_RDONLY | O_CLOEXEC)) < 0) { err = 2; return 0; }
	if (fstat(fd, &st)) { close(fd); err = 3; return 0; }
	if (!S_ISREG(st.st_mode)) { close(fd); err = 4; return 0; }
	data = readall(fd, &len);
	close(fd);
	if (!data) return 0;

	for (line = data; line && *line; line = next) {
		struct tm tm, now;
		time_t t = time(0);
		int have;
		const char *end;
		next = strchr(line, '\n');
		if (next) *next++ = 0;
		if (!*line) continue;
		__localtime_r(&t, &now);
		memset(&tm, 0, sizeof tm);
		tm.tm_isdst = -1;
		tm.tm_year = INT_MIN;   /* "not given" markers */
		tm.tm_mon = tm.tm_mday = tm.tm_hour = tm.tm_min = tm.tm_sec = tm.tm_wday = -1;
		while (isspace((unsigned char)*str)) str++;
		end = __strptime(str, line, &tm, &have, 1);
		if (!end) continue;
		while (isspace((unsigned char)*end)) end++;
		if (*end) continue;

		/* complete the date */
		{
			int date = have & (HAVE_YEAR | HAVE_MON | HAVE_MDAY | HAVE_WDAY | HAVE_YDAY);
			int timeset = have & (HAVE_HOUR | HAVE_MIN | HAVE_SEC);
			if (!timeset) { tm.tm_hour = now.tm_hour; tm.tm_min = now.tm_min; tm.tm_sec = now.tm_sec; }
			else {
				if (!(have & HAVE_HOUR)) tm.tm_hour = now.tm_hour;
				if (!(have & HAVE_MIN)) tm.tm_min = 0;
				if (!(have & HAVE_SEC)) tm.tm_sec = 0;
			}
			if (!(have & HAVE_YEAR)) tm.tm_year = now.tm_year;
			if (!date) {
				/* only a time: the first such hour from now on */
				tm.tm_mon = now.tm_mon; tm.tm_mday = now.tm_mday;
				if (tm.tm_hour < now.tm_hour ||
				    (tm.tm_hour == now.tm_hour && (have & HAVE_MIN) && tm.tm_min < now.tm_min))
					tm.tm_mday++;
			} else if ((have & HAVE_WDAY) && !(have & (HAVE_MON | HAVE_MDAY | HAVE_YEAR))) {
				tm.tm_mon = now.tm_mon;
				tm.tm_mday = now.tm_mday + (tm.tm_wday - now.tm_wday + 7) % 7;
			} else if ((have & HAVE_MON) && !(have & HAVE_YEAR)) {
				if (!(have & HAVE_MDAY)) tm.tm_mday = 1;
				if (tm.tm_mon < now.tm_mon) tm.tm_year++;
			} else {
				if (!(have & HAVE_MON)) tm.tm_mon = (have & HAVE_YDAY) ? 0 : now.tm_mon;
				if (!(have & HAVE_MDAY)) tm.tm_mday = (have & HAVE_YDAY) ? tm.tm_yday + 1 : (have & HAVE_MON) ? 1 : now.tm_mday;
			}
		}
		{
			struct tm chk = tm;
			int isdst = tm.tm_isdst;
			mktime(&chk);
			/* invalid input: a date that does not exist (e.g. February 30) */
			if ((have & HAVE_MDAY) && chk.tm_mday != tm.tm_mday) {
				free(data); err = 8; return 0;
			}
			if ((have & HAVE_ZONE) && isdst >= 0 && chk.tm_isdst != isdst) { free(data); err = 8; return 0; }
			res = chk;
		}
		free(data);
		return &res;
	}
	free(data);
	err = 7;
	return 0;
}
