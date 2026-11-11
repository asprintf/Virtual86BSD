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

/* getpass.c - getpass(3): read a password from the terminal without echo.
   The prompt is written to /dev/tty and the answer read from it (when there
   is no terminal: stderr and stdin).  At most PASS_MAX characters are kept,
   the rest of the line is dropped; end of file with nothing read gives "".
   The terminal's state is put back before the function returns, also when
   a signal comes in the meantime: the signal is held until then and sent
   again to this process. */
#define _DEFAULT_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>

#define PASS_LEN 128

static volatile sig_atomic_t caught;

static void on_signal(int sig)
{
	caught = sig;
}

char *getpass(const char *prompt)
{
	static char buf[PASS_LEN + 1];
	static const int sigs[] = { SIGINT, SIGQUIT, SIGTSTP, SIGHUP, SIGTERM };
	struct sigaction sa, old[sizeof sigs / sizeof sigs[0]];
	struct termios t, saved;
	int in, out, own = 0, changed = 0, e = 0;
	size_t i, n = 0;
	char c;
	ssize_t r;

	in = out = open("/dev/tty", O_RDWR | O_NOCTTY | O_CLOEXEC);
	if (in >= 0) {
		own = 1;
	} else {
		in = STDIN_FILENO;
		out = STDERR_FILENO;
	}
	caught = 0;
	memset(&sa, 0, sizeof sa);
	sa.sa_handler = on_signal;
	sigemptyset(&sa.sa_mask);
	for (i = 0; i < sizeof sigs / sizeof sigs[0]; i++)
		sigaction(sigs[i], &sa, &old[i]);

	if (tcgetattr(in, &saved) == 0) {
		t = saved;
		t.c_lflag &= ~(tcflag_t)(ECHO | ECHONL);
		if (tcsetattr(in, TCSAFLUSH, &t) == 0)
			changed = 1;
	}
	if (prompt)
		write(out, prompt, strlen(prompt));
	while (!caught) {
		r = read(in, &c, 1);
		if (r < 0 && errno == EINTR)
			continue;
		if (r <= 0)
			break;
		if (c == '\n')
			break;
		if (n < PASS_LEN)
			buf[n++] = c;
	}
	e = errno;
	buf[n] = 0;
	if (changed) {
		write(out, "\n", 1); /* the user's newline was not echoed */
		tcsetattr(in, TCSAFLUSH, &saved);
	}
	for (i = 0; i < sizeof sigs / sizeof sigs[0]; i++)
		sigaction(sigs[i], &old[i], NULL);
	if (own)
		close(in);
	if (caught)
		raise(caught);
	errno = e;
	return buf;
}
