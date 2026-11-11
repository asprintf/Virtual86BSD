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

/* halt, reboot - stop the system (one program, acting by its name).
 *
 *   halt [-npq]
 *   reboot [-nq]
 *
 * The usual way is to ask init(8) (SIGUSR1 halt, SIGUSR2 power off, SIGINT
 * reboot): it hangs up the sessions, runs /etc/rc.shutdown, ends every
 * process and then stops the machine, which writes the file systems.  -p
 * (halt only) powers the machine off.  -q is quick: init is not asked and
 * reboot(2) is called at once (the file systems are still written); -n does
 * not write them and so also calls reboot(2) at once, since init cannot be
 * told.  Root only. */
#define _POSIX_C_SOURCE 202405L
#define _XOPEN_SOURCE 800
#define _DEFAULT_SOURCE
#include <errno.h>
#include <libgen.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/reboot.h>
#include <unistd.h>

int main(int argc, char *argv[])
{
	char *prog = basename(argv[0]);
	int isreboot = !strcmp(prog, "reboot"), ch, nflag = 0, pflag = 0, qflag = 0;
	int howto, sig;
	const char *opts = isreboot ? "nq" : "npq";

	while ((ch = getopt(argc, argv, opts)) != -1) {
		switch (ch) {
		case 'n': nflag = 1; break;
		case 'p': pflag = 1; break;
		case 'q': qflag = 1; break;
		default:
			fprintf(stderr, isreboot ? "usage: reboot [-nq]\n" : "usage: halt [-npq]\n");
			return 1;
		}
	}
	if (argc != optind) {
		fprintf(stderr, isreboot ? "usage: reboot [-nq]\n" : "usage: halt [-npq]\n");
		return 1;
	}
	if (geteuid() != 0) {
		fprintf(stderr, "%s: you must be root\n", prog);
		return 1;
	}
	if (isreboot) {
		howto = RB_AUTOBOOT;
		sig = SIGINT;
	} else if (pflag) {
		howto = RB_HALT | RB_POWERDOWN;
		sig = SIGUSR2;
	} else {
		howto = RB_HALT;
		sig = SIGUSR1;
	}
	if (nflag)
		howto |= RB_NOSYNC;
	if (qflag || nflag) {
		if (!nflag)
			sync();
		if (reboot(howto) == -1) {
			fprintf(stderr, "%s: reboot: %s\n", prog, strerror(errno));
			return 1;
		}
		return 0;
	}
	if (kill(1, sig) == -1) {
		fprintf(stderr, "%s: cannot signal init: %s\n", prog, strerror(errno));
		return 1;
	}
	return 0;
}
