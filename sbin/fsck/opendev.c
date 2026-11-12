/*	$OpenBSD: opendev.c,v 1.18 2025/08/25 14:59:13 claudio Exp $	*/

/*
 * Copyright (c) 2000, Todd C. Miller.  All rights reserved.
 * Copyright (c) 1996, Jason Downs.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR(S) ``AS IS'' AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR(S) BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * Adapted for V86BSD from OpenBSD lib/libutil/opendev.c (revision 1.18).
 * Changes: no disk map (duid) lookup and no raw partition letter: V86BSD
 *   names a partition disk0s1, so a short name is /dev/rdisk0s1
 *   (/dev/disk0s1 with OPENDEV_BLCK); OPENDEV_PART is accepted and ignored;
 *   kept beside fsck(8) (not in libc: only OpenBSD has opendev()); otherwise
 *   as OpenBSD.
 */

#define _DEFAULT_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <paths.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "opendev.h"

/*
 * This routine is a generic rewrite of the original code found in
 * disklabel(8).
 */
int
opendev(const char *path, int oflags, int dflags, char **realpath)
{
	static char namebuf[PATH_MAX];
	char *prefix;
	int fd, ret;

	/* Initial state */
	fd = -1;
	errno = ENOENT;

	if (dflags & OPENDEV_BLCK)
		prefix = "";			/* block device */
	else
		prefix = "r";			/* character device */

	if (strchr(path, '/')) {
		strlcpy(namebuf, path, sizeof(namebuf));
		fd = open(namebuf, oflags);
	} else {
		ret = snprintf(namebuf, sizeof(namebuf), "%s%s%s",
		    _PATH_DEV, prefix, path);
		if (ret < 0 || (size_t)ret >= sizeof(namebuf))
			errno = ENAMETOOLONG;
		else
			fd = open(namebuf, oflags);
	}
	if (realpath)
		*realpath = namebuf;

	return (fd);
}
