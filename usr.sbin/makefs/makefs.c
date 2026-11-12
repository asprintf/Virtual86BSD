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
/*
 * makefs - make an FFS2 image from a directory tree, without root.
 * Written for V86BSD after makefs(8) of NetBSD (the options are those of
 * NetBSD's that are used here, see README.md).
 */

#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "makefs.h"

bool rootowner;
bool verbose;
bool stamped;
time_t stamp;

static void
usage(void)
{
	fprintf(stderr,
	    "usage: makefs [-Uv] [-b free-blocks] [-f free-files] [-M minimum-size]\n"
	    "              [-m maximum-size] [-o fs-option[,fs-option ...]] [-s image-size]\n"
	    "              [-T time] image-file directory\n"
	    "fs options: bsize=N fsize=N density=N minfree=N maxbpg=N avgfilesize=N\n"
	    "            avgfpdir=N extent=N maxbpcg=N optimization=time|space label=NAME\n");
	exit(EXIT_FAILURE);
}

/* a number with an optional % : the percent, or else the amount */
static void
amount(const char *what, char *arg, off_t *abs, int *pc)
{
	size_t len = strlen(arg);

	if (len > 0 && arg[len - 1] == '%') {
		arg[len - 1] = '\0';
		*pc = (int)strsuftoll(what, arg, 0, 99);
	} else
		*abs = (off_t)strsuftoll(what, arg, 0, LLONG_MAX);
}

int
main(int argc, char *argv[])
{
	fsinfo_t f;
	fsnode *root;
	int ch;

	memset(&f, 0, sizeof(f));
	f.fd = -1;
	ffs_prep_opts(&f);
	while ((ch = getopt(argc, argv, "b:f:M:m:o:s:T:Uv")) != -1) {
		switch (ch) {
		case 'b':
			amount("free blocks", optarg, &f.freeblocks, &f.freeblockpc);
			break;
		case 'f':
			amount("free files", optarg, &f.freefiles, &f.freefilepc);
			break;
		case 'M':
			f.minsize = (off_t)strsuftoll("minimum size", optarg, 1, LLONG_MAX);
			break;
		case 'm':
			f.maxsize = (off_t)strsuftoll("maximum size", optarg, 1, LLONG_MAX);
			break;
		case 'o': {
			char *p, *rest = optarg;

			while ((p = strsep(&rest, ",")) != NULL) {
				if (*p == '\0')
					errx(EXIT_FAILURE, "Empty option");
				if (!ffs_parse_opts(p, &f))
					usage();
			}
			break;
		}
		case 's':
			f.minsize = f.maxsize = (off_t)strsuftoll("size", optarg, 1, LLONG_MAX);
			break;
		case 'T': {
			char *end;
			long long t;

			errno = 0;
			t = strtoll(optarg, &end, 0);
			if (end == optarg || *end != '\0' || errno != 0 || t < 0)
				errx(EXIT_FAILURE, "-T wants a time in seconds since the epoch, not `%s'", optarg);
			stamped = true;
			stamp = (time_t)t;
			break;
		}
		case 'U':
			rootowner = true;
			break;
		case 'v':
			verbose = true;
			break;
		default:
			usage();
		}
	}
	argc -= optind;
	argv += optind;
	if (argc != 2)
		usage();

	root = walk_dir(argv[1]);
	ffs_makefs(argv[0], root, &f);
	free_fsnodes(root);
	return EXIT_SUCCESS;
}
