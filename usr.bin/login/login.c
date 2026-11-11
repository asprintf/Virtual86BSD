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

/* login - sign in (the login(1) of the BSDs, reduced).
 *
 *   login [-fp] [--] [name]
 *
 * getty runs it as `login -- name' on the terminal it set up.  With no name
 * it asks (`login: '); then, unless the account's password is empty, the
 * password (`Password:', no echo) is checked with crypt(3) against the hash
 * in /etc/master.passwd.  A wrong name or password sleeps two seconds and
 * says `Login incorrect'; ten failures end login (getty starts again).
 * An empty password field is an account that needs none; `*' or `!' one that
 * nobody can enter.  A name that does not exist costs the same time as a
 * wrong password.
 *
 * Then, as root: root is only let in on a line that /etc/ttys marks secure; a
 * user is turned away (with the text of /etc/nologin) while that file exists;
 * an expired account is refused.  The login is recorded in utmpx (so `who'
 * shows it; init marks it dead when the session ends), the terminal becomes
 * the user's (owner, group tty, mode 0620), /etc/motd is shown unless
 * ~/.hushlogin exists, the groups, group and user are set, the environment is
 * made (HOME SHELL USER LOGNAME PATH, TERM kept; -p keeps all of it) and the
 * shell is run with the name `-sh', in the home directory (or /, with a
 * note, if there is none).
 *
 * -f: the user is already known (root only; used by su and the like): no
 * password.  Not there: wtmp and lastlog (no records of past logins),
 * login classes, accounting. */
#define _POSIX_C_SOURCE 202405L
#define _XOPEN_SOURCE 800
#define _DEFAULT_SOURCE
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <grp.h>
#include <limits.h>
#include <mpasswd.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>
#include <utmpx.h>

#define MAXTRIES 10
#define FAIL_DELAY 2
#define TIMEOUT 300
#define NAME_MAX_LEN 32
#define PATH_NOLOGIN "/etc/nologin"
#define PATH_MOTD "/etc/motd"
#define PATH_TTYS "/etc/ttys"
#define DEFAULT_PATH "/usr/bin:/bin:/usr/sbin:/sbin:/usr/local/bin"
#define DEFAULT_SHELL "/bin/sh"

extern char **environ;

static void usage(void)
{
	fprintf(stderr, "usage: login [-fp] [--] [name]\n");
	exit(1);
}

static void timed_out(int sig)
{
	(void)sig;
	static const char m[] = "\r\nLogin timed out after 300 seconds\r\n";
	(void)!write(1, m, sizeof m - 1);
	_exit(1);
}

/* write a file to the terminal; 1 if it was there */
static int cat(const char *file)
{
	char buf[1024];
	ssize_t n;
	int fd = open(file, O_RDONLY);
	if (fd < 0)
		return 0;
	while ((n = read(fd, buf, sizeof buf)) > 0)
		(void)!write(1, buf, (size_t)n);
	close(fd);
	return 1;
}

/* is the line `secure' in /etc/ttys (a root login is allowed there)?  No
   /etc/ttys: yes (nothing says otherwise). */
static int secure_line(const char *line)
{
	FILE *f = fopen(PATH_TTYS, "r");
	char buf[512];
	int found = 0, secure = 0;
	if (!f)
		return 1;
	while (fgets(buf, sizeof buf, f)) {
		char *p = buf, *q, *save = NULL, *w;
		size_t l = strlen(line);
		while (*p == ' ' || *p == '\t')
			p++;
		if (*p == '#' || strncmp(p, line, l) || (p[l] != ' ' && p[l] != '\t'))
			continue;
		found = 1;
		/* after the quoted command: type, on|off, flags */
		q = strchr(p, '"');
		q = q ? strchr(q + 1, '"') : NULL;
		for (w = strtok_r(q ? q + 1 : p + l, " \t\n", &save); w; w = strtok_r(NULL, " \t\n", &save))
			if (!strcmp(w, "secure"))
				secure = 1;
		break;
	}
	fclose(f);
	return found && secure;
}

static char *read_name(void)
{
	static char name[128];
	size_t l;
	fputs("login: ", stdout);
	fflush(stdout);
	if (!fgets(name, sizeof name, stdin))
		exit(0);
	l = strlen(name);
	if (l && name[l - 1] == '\n')
		name[--l] = 0;
	return name;
}

/* a password was right: 1.  The time taken does not tell whether the user
   exists: for one that does not, a hash is made all the same. */
static int check(int exists, const struct spw *pw, const char *pass)
{
	static const char dummy[] = "$6$loginlogin$0000000000000000000000000000000000000000000000000000000000000000000000000000000000000";
	const char *want = exists ? pw->hash : dummy;
	char *got = crypt(pass, want);
	int ok = exists && got && want[0] != '*' && want[0] != '!' && strcmp(got, want) == 0;
	return ok;
}

static void bad_name(void)
{
	sleep(FAIL_DELAY);
	puts("Login incorrect");
}

int main(int argc, char *argv[])
{
	int ch, fflag = 0, pflag = 0, tries = 0, exists;
	char *name = NULL, *tty, *ttyshort, home[PATH_MAX], shell[PATH_MAX];
	struct spw pw;
	struct utmpx ut;
	struct stat st;
	gid_t groups[NGROUPS_MAX < 64 ? NGROUPS_MAX : 64];
	int ngroups = 0;
	struct group *gr;
	gid_t ttygid;
	char *envp[16], *shellargv[2], e_home[PATH_MAX + 8], e_shell[PATH_MAX + 8], e_user[80], e_logname[80], e_path[] = "PATH=" DEFAULT_PATH,
	    e_term[128];
	int ne = 0;
	const char *term;
	struct timeval tv;

	signal(SIGALRM, timed_out);
	alarm(TIMEOUT);
	signal(SIGQUIT, SIG_IGN);
	signal(SIGINT, SIG_IGN);
	while ((ch = getopt(argc, argv, "fp")) != -1) {
		switch (ch) {
		case 'f': fflag = 1; break;
		case 'p': pflag = 1; break;
		default: usage();
		}
	}
	argc -= optind;
	argv += optind;
	if (argc > 1)
		usage();
	if (argc == 1)
		name = argv[0];
	if (fflag && (!name || getuid() != 0)) {
		fprintf(stderr, "login: -f needs a name and is for root\n");
		return 1;
	}
	if (!(tty = ttyname(0))) {
		fprintf(stderr, "login: not a terminal\n");
		return 1;
	}
	ttyshort = strncmp(tty, "/dev/", 5) ? tty : tty + 5;

	for (;;) {
		char *pass;
		int ok;
		if (!name)
			name = read_name();
		if (!name[0]) {
			name = NULL;
			if (++tries >= MAXTRIES)
				return 1;
			continue;
		}
		if (name[0] == '-' || strlen(name) > NAME_MAX_LEN || strchr(name, ':') || strchr(name, '/')) {
			bad_name();
			name = NULL;
			if (++tries >= MAXTRIES)
				return 1;
			continue;
		}
		exists = spw_lookup(PATH_MASTER_PASSWD, name, &pw) == 1;
		if (fflag) {
			ok = exists;
		} else if (exists && pw.hash[0] == 0) {
			ok = 1; /* no password */
		} else {
			pass = getpass("Password:");
			ok = check(exists, &pw, pass ? pass : "");
			if (pass)
				memset(pass, 0, strlen(pass));
		}
		if (ok && pw.uid == 0 && !secure_line(ttyshort)) {
			fprintf(stdout, "login: root login refused on %s\n", ttyshort);
			ok = 0;
		}
		if (ok)
			break;
		bad_name();
		fflag = 0; /* -f is for the name it was given */
		name = NULL;
		if (++tries >= MAXTRIES)
			return 1;
	}
	alarm(0);

	if (pw.uid != 0 && stat(PATH_NOLOGIN, &st) == 0) {
		if (!cat(PATH_NOLOGIN))
			puts("The system is going down.");
		return 0;
	}
	if (pw.expire && time(NULL) > pw.expire) {
		puts("Sorry -- your account has expired.");
		return 1;
	}
	snprintf(home, sizeof home, "%s", pw.dir[0] ? pw.dir : "/");
	snprintf(shell, sizeof shell, "%s", pw.shell[0] ? pw.shell : DEFAULT_SHELL);

	/* the record that who shows; init marks it dead when this session's process ends */
	memset(&ut, 0, sizeof ut);
	ut.ut_type = USER_PROCESS;
	ut.ut_pid = getpid();
	snprintf(ut.ut_line, sizeof ut.ut_line, "%s", ttyshort);
	{
		size_t l = strlen(ttyshort);
		memcpy(ut.ut_id, ttyshort + (l > sizeof ut.ut_id ? l - sizeof ut.ut_id : 0), l > sizeof ut.ut_id ? sizeof ut.ut_id : l);
	}
	snprintf(ut.ut_user, sizeof ut.ut_user, "%s", name);
	gettimeofday(&tv, NULL);
	ut.ut_tv.tv_sec = tv.tv_sec;
	ut.ut_tv.tv_usec = tv.tv_usec;
	setutxent();
	pututxline(&ut);
	endutxent();

	/* the terminal is the user's */
	ttygid = pw.gid;
	if ((gr = getgrnam("tty")))
		ttygid = gr->gr_gid;
	fchown(0, (uid_t)pw.uid, ttygid);
	fchmod(0, 0620);

	/* the groups: the user's own, and those that list the user */
	groups[ngroups++] = (gid_t)pw.gid;
	setgrent();
	while ((gr = getgrent()) && ngroups < (int)(sizeof groups / sizeof groups[0])) {
		char **m;
		if (gr->gr_gid == (gid_t)pw.gid)
			continue;
		for (m = gr->gr_mem; m && *m; m++)
			if (!strcmp(*m, name)) {
				groups[ngroups++] = gr->gr_gid;
				break;
			}
	}
	endgrent();

	umask(022);
	if (chdir(home) < 0) {
		printf("No home directory %s!\n", home);
		if (chdir("/") < 0)
			return 1;
		strcpy(home, "/");
		puts("Logging in with home = \"/\".");
	}
	{
		char hush[PATH_MAX + 16];
		snprintf(hush, sizeof hush, "%s/.hushlogin", home);
		if (access(hush, F_OK) != 0)
			cat(PATH_MOTD);
	}

	/* the environment */
	term = getenv("TERM");
	if (pflag) {
		setenv("HOME", home, 1);
		setenv("SHELL", shell, 1);
		setenv("USER", name, 1);
		setenv("LOGNAME", name, 1);
	} else {
		snprintf(e_home, sizeof e_home, "HOME=%s", home);
		snprintf(e_shell, sizeof e_shell, "SHELL=%s", shell);
		snprintf(e_user, sizeof e_user, "USER=%s", name);
		snprintf(e_logname, sizeof e_logname, "LOGNAME=%s", name);
		envp[ne++] = e_home;
		envp[ne++] = e_shell;
		envp[ne++] = e_user;
		envp[ne++] = e_logname;
		envp[ne++] = e_path;
		if (term) {
			snprintf(e_term, sizeof e_term, "TERM=%s", term);
			envp[ne++] = e_term;
		}
		envp[ne] = NULL;
	}

	if (setgroups((size_t)ngroups, groups) < 0 || setgid((gid_t)pw.gid) < 0 || setuid((uid_t)pw.uid) < 0) {
		fprintf(stderr, "login: cannot become %s: %s\n", name, strerror(errno));
		return 1;
	}
	signal(SIGINT, SIG_DFL);
	signal(SIGQUIT, SIG_DFL);
	signal(SIGALRM, SIG_DFL);
	{
		char *base = strrchr(shell, '/');
		static char arg0[PATH_MAX];
		snprintf(arg0, sizeof arg0, "-%s", base ? base + 1 : shell);
		shellargv[0] = arg0;
		shellargv[1] = NULL;
	}
	execve(shell, shellargv, pflag ? environ : envp);
	fprintf(stderr, "login: no shell: %s\n", strerror(errno));
	return 1;
}
