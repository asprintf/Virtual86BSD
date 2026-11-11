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

/* umount - unmount file systems.
 *
 *   umount [-v] special | node ...
 *   umount -a [-v] [-t type[,type...]]
 *
 * A name is the directory a file system is mounted on or its special file
 * (/dev/disk0s1).  -a unmounts every mount of /proc/self/mounts, the last
 * first, except /, /dev and /proc, which the kernel keeps; -t keeps to the
 * types listed.  A mount that is busy (something below it, a file open)
 * fails with its reason and the rest go on.  -v names each one.  The status
 * is 1 if anything failed. */
#define _POSIX_C_SOURCE 202405L
#define _XOPEN_SOURCE 800
#define _DEFAULT_SOURCE
#include <errno.h>
#include <limits.h>
#include <mnttab.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mount.h>
#include <unistd.h>

static int status, verbose;

static void usage(void)
{
	fprintf(stderr, "usage: umount [-v] special | node ...\n       umount -a [-v] [-t type[,type...]]\n");
	exit(1);
}

static void do_umount(const char *dir)
{
	if (umount(dir) == -1) {
		fprintf(stderr, "umount: %s: %s\n", dir, strerror(errno));
		status = 1;
	} else if (verbose) {
		printf("%s: unmounted\n", dir);
	}
}

int main(int argc, char *argv[])
{
	int ch, all = 0;
	const char *types = NULL;
	const char *const files[] = { "/proc/self/mounts", NULL };
	struct mnt *cur;
	size_t ncur, i;

	while ((ch = getopt(argc, argv, "avt:")) != -1) {
		switch (ch) {
		case 'a': all = 1; break;
		case 'v': verbose = 1; break;
		case 't': types = optarg; break;
		default: usage();
		}
	}
	argc -= optind;
	argv += optind;
	if (all ? argc != 0 : (argc == 0 || types))
		usage();
	if (!mnt_read(files, &cur, &ncur)) {
		fprintf(stderr, "umount: %s: %s\n", files[0], strerror(errno));
		return 1;
	}
	if (all) {
		for (i = ncur; i-- > 0; ) {
			if (!strcmp(cur[i].dir, "/") || !strcmp(cur[i].dir, "/dev") || !strcmp(cur[i].dir, "/proc"))
				continue;
			if (types && !mnt_hasopt(types, cur[i].type))
				continue;
			do_umount(cur[i].dir);
		}
		return status;
	}
	for (; *argv; argv++) {
		char real[PATH_MAX];
		const char *name = *argv, *dir = NULL;
		/* the last mount whose directory or special file is the name (also
		   with the name made absolute: a path with .. or a symbolic link) */
		for (i = ncur; i-- > 0 && !dir; ) {
			if (!strcmp(cur[i].dir, name) || !strcmp(cur[i].fsname, name))
				dir = cur[i].dir;
		}
		if (!dir && realpath(name, real)) {
			for (i = ncur; i-- > 0 && !dir; )
				if (!strcmp(cur[i].dir, real) || !strcmp(cur[i].fsname, real))
					dir = cur[i].dir;
		}
		if (!dir) {
			fprintf(stderr, "umount: %s: not currently mounted\n", name);
			status = 1;
			continue;
		}
		do_umount(dir);
	}
	return status;
}
