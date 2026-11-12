/*	$OpenBSD: newfs.c,v 1.120 2025/09/17 16:07:57 deraadt Exp $	*/
/*	$NetBSD: newfs.c,v 1.20 1996/05/16 07:13:03 thorpej Exp $	*/

/*
 * Copyright (c) 2002 Networks Associates Technology, Inc.
 * All rights reserved.
 *
 * This software was developed for the FreeBSD Project by Marshall
 * Kirk McKusick and Network Associates Laboratories, the Security
 * Research Division of Network Associates, Inc. under DARPA/SPAWAR
 * contract N66001-01-C-8035 ("CBOSS"), as part of the DARPA CHATS
 * research program.
 *
 * Copyright (c) 1983, 1989, 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * Adapted for V86BSD from OpenBSD sbin/newfs/newfs.c (revision 1.120).
 * Changes: the disklabel, partition, memory-file-system (mount_mfs), opendev
 *   and pledge parts removed: the size of the file system is that of the
 *   file or device, newfs refuses a mounted device by reading
 *   /proc/self/mounts; -T and -t dropped; FFS2 is the default (FFS1 is still
 *   written with -O 1, the kernel reads FFS2 only); otherwise as OpenBSD.
 */

#include <sys/param.h>	/* DEV_BSIZE MAXBSIZE */
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/sysctl.h>
#include <sys/resource.h>

#include <ufs/ufs/dinode.h>
#include <ufs/ufs/dir.h>
#include <ufs/ffs/fs.h>

#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <unistd.h>

#define MINIMUM(a, b)	(((a) < (b)) ? (a) : (b))
#define MAXIMUM(a, b)	(((a) > (b)) ? (a) : (b))

void	fatal(const char *fmt, ...)
	    __attribute__((__format__ (printf, 1, 2)))
	    __attribute__((__nonnull__ (1)));
void	usage(void);
void	mkfs(char *, int, int, mode_t, uid_t, gid_t);
int	scan_scaled(char *, long long *);	/* scan_scaled.c */
void	getphysmem(void);
static int ismounted(const char *);

/*
 * The following two constants set the default block and fragment sizes.
 * Both constants must be a power of 2 and meet the following constraints:
 *	MINBSIZE <= DESBLKSIZE <= MAXBSIZE
 *	sectorsize <= DESFRAGSIZE <= DESBLKSIZE
 *	DESBLKSIZE / DESFRAGSIZE <= 8
 */
#define	DFL_FRAGSIZE	2048
#define	DFL_BLKSIZE	16384

/*
 * MAXBLKPG determines the maximum number of data blocks which are
 * placed in a single cylinder group. The default is one indirect
 * block worth of data blocks.
 */
#define MAXBLKPG_FFS1(bsize)	((bsize) / sizeof(int32_t))
#define MAXBLKPG_FFS2(bsize)	((bsize) / sizeof(int64_t))

/*
 * Each file system has a number of inodes statically allocated.
 * We allocate one inode slot per NFPI fragments, expecting this
 * to be far more than we will ever need.
 */
#define	NFPI		4

int	mfs;			/* (mkfs.c: no memory based file system here) */
int	Nflag;			/* run without writing file system */
int	Oflag = 2;		/* 1 = 4.4BSD ffs, 2 = ffs2 */
daddr_t	fssize;			/* file system size in 512-byte blocks */
long long	sectorsize;	/* bytes/sector */
int	fsize = 0;		/* fragment size */
int	bsize = 0;		/* block size */
int	maxfrgspercg = INT_MAX;	/* maximum fragments per cylinder group */
int	minfree = MINFREE;	/* free space threshold */
int	opt = DEFAULTOPT;	/* optimization preference (space or time) */
int	reqopt = -1;		/* opt preference has not been specified */
int	density;		/* number of bytes per inode */
int	maxbpg;			/* maximum blocks per file in a cyl group */
int	avgfilesize = AVFILESIZ;/* expected average file size */
int	avgfilesperdir = AFPDIR;/* expected number of files per directory */
int	quiet = 0;		/* quiet flag */
char	*membase;		/* (mkfs.c: unused without a memory file system) */

extern	char *__progname;

int64_t physmem;

void
getphysmem(void)
{
	int mib[] = { CTL_HW, HW_PHYSMEM64 };
	size_t len = sizeof(physmem);

	if (sysctl(mib, 2, &physmem, &len, NULL, 0) != 0) {
		if (errno != ENOSYS)
			err(1, "can't get physmem");
		physmem = INT64_MAX;	/* a Linux-ABI program: the data limit rules */
	}
}

int
main(int argc, char *argv[])
{
	int ch;
	struct stat st;
	int fsi = -1, oflagset = 0, fso;
	char *s1, *special;
	const char *errstr;
	long long fssize_input = 0;
	int fssize_usebytes = 0;
	int defaultfsize;
	uint64_t nsecs, devsecs;
	off_t devsize;

	getphysmem();

	while ((ch = getopt(argc, argv, "NO:S:b:c:e:f:g:h:i:m:o:qs:")) != -1) {
		switch (ch) {
		case 'N':
			Nflag = 1;
			break;
		case 'O':
			Oflag = strtonum(optarg, 1, 2, &errstr);
			if (errstr)
				fatal("%s: invalid ffs version", optarg);
			oflagset = 1;
			break;
		case 'S':
			if (scan_scaled(optarg, &sectorsize) == -1 ||
			    sectorsize <= 0 || (sectorsize % DEV_BSIZE))
				fatal("sector size invalid: %s", optarg);
			break;
		case 'b':
			bsize = strtonum(optarg, MINBSIZE, MAXBSIZE, &errstr);
			if (errstr)
				fatal("block size is %s: %s", errstr, optarg);
			break;
		case 'c':
			maxfrgspercg = strtonum(optarg, 1, INT_MAX, &errstr);
			if (errstr)
				fatal("fragments per cylinder group is %s: %s",
				    errstr, optarg);
			break;
		case 'e':
			maxbpg = strtonum(optarg, 1, INT_MAX, &errstr);
			if (errstr)
				fatal("blocks per file in a cylinder group is"
				    " %s: %s", errstr, optarg);
			break;
		case 'f':
			fsize = strtonum(optarg, MINBSIZE / MAXFRAG, MAXBSIZE,
			    &errstr);
			if (errstr)
				fatal("fragment size is %s: %s",
				    errstr, optarg);
			break;
		case 'g':
			avgfilesize = strtonum(optarg, 1, INT_MAX, &errstr);
			if (errstr)
				fatal("average file size is %s: %s",
				    errstr, optarg);
			break;
		case 'h':
			avgfilesperdir = strtonum(optarg, 1, INT_MAX, &errstr);
			if (errstr)
				fatal("average files per dir is %s: %s",
				    errstr, optarg);
			break;
		case 'i':
			density = strtonum(optarg, 1, INT_MAX, &errstr);
			if (errstr)
				fatal("bytes per inode is %s: %s",
				    errstr, optarg);
			break;
		case 'm':
			minfree = strtonum(optarg, 0, 99, &errstr);
			if (errstr)
				fatal("free space %% is %s: %s",
				    errstr, optarg);
			break;
		case 'o':
			if (strcmp(optarg, "space") == 0)
				reqopt = opt = FS_OPTSPACE;
			else if (strcmp(optarg, "time") == 0)
				reqopt = opt = FS_OPTTIME;
			else
				fatal("%s: unknown optimization "
				    "preference: use `space' or `time'.",
				    optarg);
			break;
		case 'q':
			quiet = 1;
			break;
		case 's':
			if (scan_scaled(optarg, &fssize_input) == -1 ||
			    fssize_input <= 0)
				fatal("file system size invalid: %s", optarg);
			fssize_usebytes = 0;    /* in case of multiple -s */
			for (s1 = optarg; *s1 != '\0'; s1++)
				if (isalpha((unsigned char)*s1)) {
					fssize_usebytes = 1;
					break;
				}
			break;
		default:
			usage();
		}
	}
	argc -= optind;
	argv += optind;

	if (argc != 1)
		usage();
	special = argv[0];
	if (*special == '\0')
		fatal("empty special file name supplied");

	if (Nflag) {
		fso = -1;
	} else {
		if (ismounted(special))
			fatal("%s is mounted", special);
		fso = open(special, O_WRONLY);
		if (fso == -1)
			fatal("%s: %s", special, strerror(errno));
	}
	fsi = open(special, O_RDONLY);
	if (fsi == -1)
		fatal("%s: %s", special, strerror(errno));
	if (fstat(fsi, &st) == -1)
		fatal("%s: %s", special, strerror(errno));
	if (S_ISBLK(st.st_mode))
		fatal("%s: block device", special);
	if (S_ISREG(st.st_mode))
		devsize = st.st_size;
	else {
		devsize = lseek(fsi, 0, SEEK_END);
		if (devsize == -1)
			fatal("%s: can't find the size: %s", special,
			    strerror(errno));
	}

	if (sectorsize == 0)
		sectorsize = DEV_BSIZE;
	devsecs = devsize / sectorsize;
	if (fssize_usebytes) {
		nsecs = fssize_input / sectorsize;
		if (fssize_input % sectorsize != 0)
			nsecs++;
	} else if (fssize_input == 0)
		nsecs = devsecs;
	else
		nsecs = fssize_input;
	if (nsecs > devsecs)
		fatal("%s: maximum file system size is %llu sectors",
		    special, (unsigned long long)devsecs);
	if (nsecs == 0)
		fatal("%s: no space for a file system", special);

	fssize = nsecs * (sectorsize / DEV_BSIZE);
	if (oflagset == 0 && fssize >= INT_MAX)
		Oflag = 2;	/* FFS2 */
	defaultfsize = fsize == 0;
	if (fsize == 0)
		fsize = MAXIMUM(DFL_FRAGSIZE, sectorsize);
	if (bsize == 0)
		bsize = MINIMUM(DFL_BLKSIZE, 8 * fsize);
	if (density == 0) {
		density = NFPI * fsize;
		/* large sectors lead to fewer inodes due to large fsize,
		   compensate */
		if (defaultfsize && sectorsize > DEV_BSIZE)
			density /= 2;
	}
	if (minfree < MINFREE && opt != FS_OPTSPACE && reqopt == -1) {
		warnx("warning: changing optimization to space "
		    "because minfree is less than %d%%\n", MINFREE);
		opt = FS_OPTSPACE;
	}
	if (maxbpg == 0) {
		if (Oflag <= 1)
			maxbpg = MAXBLKPG_FFS1(bsize);
		else
			maxbpg = MAXBLKPG_FFS2(bsize);
	}

	mkfs(special, fsi, fso, 0, 0, 0);
	if (!Nflag)
		close(fso);
	close(fsi);
	exit(0);
}

/*
 * Is special (or the block device of the character special rdisk0s1) in
 * the table of mounted file systems?
 */
static int
ismounted(const char *special)
{
	FILE *f;
	char line[1024], dev[PATH_MAX], *from, *p;
	const char *s = special;
	int found = 0;

	if ((f = fopen("/proc/self/mounts", "r")) == NULL)
		return 0;
	if (strncmp(s, "/dev/r", 6) == 0)
		s += 6;			/* rdisk0s1 names disk0s1 */
	else if (strncmp(s, "/dev/", 5) == 0)
		s += 5;
	while (fgets(line, sizeof line, f) != NULL) {
		from = line;
		if ((p = strpbrk(from, " \t")) == NULL)
			continue;
		*p = '\0';
		if (strncmp(from, "/dev/", 5) != 0)
			continue;
		snprintf(dev, sizeof dev, "%s", from + 5);
		if (strcmp(dev, s) == 0 ||
		    (dev[0] == 'r' && strcmp(dev + 1, s) == 0)) {
			found = 1;
			break;
		}
	}
	fclose(f);
	return found;
}

void
fatal(const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	if (fcntl(STDERR_FILENO, F_GETFL) == -1) {
		openlog(__progname, LOG_CONS, LOG_DAEMON);
		vsyslog(LOG_ERR, fmt, ap);
		closelog();
	} else {
		vwarnx(fmt, ap);
	}
	va_end(ap);
	exit(1);
	/*NOTREACHED*/
}

void
usage(void)
{
	fprintf(stderr,
	    "usage: %s [-Nq] [-b block-size] "
	    "[-c fragments-per-cylinder-group] [-e maxbpg]\n"
	    "\t[-f frag-size] [-g avgfilesize] [-h avgfpdir] [-i bytes]\n"
	    "\t[-m free-space] [-O filesystem-format] [-o optimization]\n"
	    "\t[-S sector-size] [-s size] special\n",
	    __progname);
	exit(1);
}
