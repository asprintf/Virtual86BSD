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

/* mount - mount file systems (the mount(8) of the BSDs, for what this
   kernel can do).
 *
 *   mount [-t type[,type...]]                 list the mounts
 *   mount -a [-t type[,type...]]              mount what /etc/fstab lists
 *   mount [-ruvw] [-t type] [-o options] special node
 *   mount [-ruvw] [-o options] special | node    (the rest from /etc/fstab)
 *
 * -r mounts read-only (-o ro), -w read-write (-o rw), -u changes the mount
 * that is there (a remount: `mount -uw /' makes / writable); -v prints what
 * was mounted.  The options are ro, rw, noauto (in the fstab: -a skips the
 * entry), update (-u) and defaults; the kernel knows no others, so any other
 * word is an error.  -a skips what is already mounted (or, with -u, changes
 * it), and entries of type swap, sw and xx.  With no type a /dev/ special is
 * ext2 and anything else tmpfs.  The list is the system's table (/proc/self/mounts):
 *     /dev/disk0s1 on / type ext2 (local)
 * The status is 1 if any mount failed. */
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

#define PATH_FSTAB "/etc/fstab"

static int status, verbose;
static const char *const mounts_files[] = { "/proc/self/mounts", NULL };
static const char *const fstab_files[] = { PATH_FSTAB, NULL };

static void usage(void)
{
	fprintf(stderr, "usage: mount [-aruvw] [-t type[,type...]] [-o options] [special | node]\n"
	    "       mount [-ruvw] [-t type] [-o options] special node\n");
	exit(1);
}

/* what the options say: ro is -1 (not said), 0 (rw) or 1 (ro) */
struct want {
	int ro, noauto, update;
};

/* the words of a comma-separated option list; 0, or -1 with a message for
   an option the kernel does not know */
static int parse_opts(const char *opts, struct want *w)
{
	char buf[256], *o, *save;

	if (strlen(opts) >= sizeof buf) {
		fprintf(stderr, "mount: options too long\n");
		return -1;
	}
	strcpy(buf, opts);
	for (o = strtok_r(buf, ",", &save); o; o = strtok_r(NULL, ",", &save)) {
		if (!strcmp(o, "ro")) w->ro = 1;
		else if (!strcmp(o, "rw")) w->ro = 0;
		else if (!strcmp(o, "noauto")) w->noauto = 1;
		else if (!strcmp(o, "update")) w->update = 1;
		else if (!strcmp(o, "defaults") || !strcmp(o, "auto")) ;
		else {
			fprintf(stderr, "mount: option not supported by this kernel: %s\n", o);
			return -1;
		}
	}
	return 0;
}

static int type_ok(const char *types, const char *type)
{
	return !types || mnt_hasopt(types, type);
}

static int load(const char *const *files, struct mnt **l, size_t *n)
{
	if (!mnt_read(files, l, n)) {
		fprintf(stderr, "mount: %s: %s\n", files[0], strerror(errno));
		return -1;
	}
	return 0;
}

/* the last mount (the one that shows) at dir, or whose special file is name */
static const struct mnt *find_mount(const struct mnt *cur, size_t ncur, const char *name, int by_special)
{
	size_t i;
	for (i = ncur; i-- > 0; )
		if (!strcmp(cur[i].dir, name) || (by_special && !strcmp(cur[i].fsname, name)))
			return &cur[i];
	return NULL;
}

/* mount, or with update change the mount at dir; ro as the want says (an
   update that says nothing keeps the mount as it is) */
static void do_mount(const char *special, const char *dir, const char *type, int ro, int update)
{
	unsigned long flags = ro == 1 ? MS_RDONLY : 0;
	int e;

	if (update)
		e = mount(special, dir, NULL, flags | MS_REMOUNT, NULL);
	else
		e = mount(special, dir, type, flags, NULL);
	if (e == -1) {
		fprintf(stderr, "mount: %s: %s\n", dir, strerror(errno));
		status = 1;
	} else if (verbose) {
		printf("%s on %s type %s (%s)\n", special, dir, type ? type : "-", ro == 1 ? "ro" : "rw");
	}
}

int main(int argc, char *argv[])
{
	int ch, all = 0, bad = 0;
	struct want cmd = { -1, 0, 0 };
	const char *types = NULL;
	struct mnt *cur = NULL, *tab = NULL;
	size_t ncur = 0, ntab = 0, i;

	while ((ch = getopt(argc, argv, "aruvwt:o:")) != -1) {
		switch (ch) {
		case 'a': all = 1; break;
		case 'r': cmd.ro = 1; break;
		case 'w': cmd.ro = 0; break;
		case 'u': cmd.update = 1; break;
		case 'v': verbose = 1; break;
		case 't': types = optarg; break;
		case 'o':
			if (parse_opts(optarg, &cmd) == -1)
				bad = 1;
			break;
		default: usage();
		}
	}
	argc -= optind;
	argv += optind;
	if (bad)
		return 1;
	if (load(mounts_files, &cur, &ncur) == -1)
		return 1;

	if (all) {
		if (argc != 0)
			usage();
		if (load(fstab_files, &tab, &ntab) == -1)
			return 1;
		for (i = 0; i < ntab; i++) {
			struct want w = { -1, 0, 0 };
			const struct mnt *at;
			if (!strcmp(tab[i].type, "swap") || !strcmp(tab[i].type, "sw") || !strcmp(tab[i].type, "xx"))
				continue;
			if (!type_ok(types, tab[i].type))
				continue;
			if (parse_opts(tab[i].opts, &w) == -1) {
				status = 1;
				continue;
			}
			if (w.noauto)
				continue;
			if (cmd.ro != -1)
				w.ro = cmd.ro; /* the command line wins */
			at = find_mount(cur, ncur, tab[i].dir, 0);
			if (at && !cmd.update)
				continue;
			do_mount(tab[i].fsname, tab[i].dir, tab[i].type, w.ro, at != NULL);
		}
		return status;
	}

	if (argc == 0) {
		for (i = 0; i < ncur; i++)
			if (type_ok(types, cur[i].type))
				printf("%s on %s type %s (local%s)\n", cur[i].fsname, cur[i].dir, cur[i].type,
				    mnt_hasopt(cur[i].opts, "ro") ? ", read-only" : "");
		return 0;
	}
	if (argc > 2)
		usage();

	if (cmd.update) {
		const struct mnt *m;
		if (argc != 1)
			usage();
		m = find_mount(cur, ncur, argv[0], 1);
		if (!m) {
			fprintf(stderr, "mount: %s: not mounted\n", argv[0]);
			return 1;
		}
		do_mount(m->fsname, m->dir, m->type, cmd.ro != -1 ? cmd.ro : mnt_hasopt(m->opts, "ro"), 1);
		return status;
	}
	if (argc == 1) { /* a directory or special file of the fstab */
		if (load(fstab_files, &tab, &ntab) == -1)
			return 1;
		for (i = 0; i < ntab; i++)
			if (!strcmp(tab[i].dir, argv[0]) || !strcmp(tab[i].fsname, argv[0])) {
				struct want w = { -1, 0, 0 };
				if (parse_opts(tab[i].opts, &w) == -1)
					return 1;
				if (cmd.ro != -1)
					w.ro = cmd.ro;
				do_mount(tab[i].fsname, tab[i].dir, types ? types : tab[i].type, w.ro, 0);
				return status;
			}
		fprintf(stderr, "mount: %s: not in %s\n", argv[0], PATH_FSTAB);
		return 1;
	}
	do_mount(argv[0], argv[1], types ? types : (strncmp(argv[0], "/dev/", 5) == 0 ? "ext2" : "tmpfs"), cmd.ro, 0);
	return status;
}
