/* Self-checking: directory streams, posix_getdents, utmpx, getlogin. */
#define _XOPEN_SOURCE 800
#include <dirent.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <utmpx.h>
#include <pwd.h>
#include <sys/stat.h>
#include "out.h"

#define NF 300

int main(void)
{
	char dir[64] = "/tmp/libc-sdirent.XXXXXX", name[32], seen[NF], cwd[4096];
	_Alignas(8) char gbuf[4096];
	DIR *d;
	struct dirent *de, ent, *res;
	long pos[NF + 2], at;
	int i, k, n, fd, dots = 0, total;
	ssize_t got;

	CHECK(mkdtemp(dir) != 0);
	CHECK(getcwd(cwd, sizeof cwd) != 0);
	CHECK(chdir(dir) == 0);
	for (i = 0; i < NF; i++) {
		strcpy(name, "file-"); name[5] = (char)('a' + i / 26 % 26); name[6] = (char)('a' + i % 26);
		name[7] = (char)('0' + i / 676); name[8] = 0;
		fd = open(name, O_CREAT | O_WRONLY, 0644); CHECK(fd >= 0); close(fd);
	}
	CHECK(mkdir("sub", 0755) == 0);
	CHECK(symlink("x", "link") == 0);

	/* readdir sees every entry once; telldir/seekdir come back */
	memset(seen, 0, sizeof seen);
	d = opendir("."); CHECK(d != 0);
	CHECK(dirfd(d) >= 0 && (fcntl(dirfd(d), F_GETFD) & FD_CLOEXEC));
	n = 0;
	for (;;) {
		at = telldir(d);
		if (!(de = readdir(d))) break;
		pos[n < NF + 2 ? n : NF + 1] = at;
		if (!strcmp(de->d_name, ".") || !strcmp(de->d_name, "..")) dots++;
		else if (!strncmp(de->d_name, "file-", 5)) {
			k = (de->d_name[5] - 'a') * 26 + (de->d_name[6] - 'a');
			CHECK(k >= 0 && k < NF && !seen[k]); if (k >= 0 && k < NF) seen[k] = 1;
		}
		n++;
	}
	CHECK(n == NF + 4 && dots == 2);
	for (i = 0; i < NF; i++) if (!seen[i]) { CHECK(seen[i]); break; }
	/* seek back to entry 100 and re-read the same name */
	seekdir(d, pos[100]); de = readdir(d); CHECK(de != 0);
	strcpy(name, de->d_name);
	seekdir(d, pos[10]); readdir(d); seekdir(d, pos[100]);
	de = readdir(d); CHECK(de && !strcmp(de->d_name, name));
	rewinddir(d); total = 0;
	while (readdir(d)) total++;
	CHECK(total == n);
	rewinddir(d);
	CHECK(readdir_r(d, &ent, &res) == 0 && res == &ent && ent.d_name[0]);
	while (!readdir_r(d, &ent, &res) && res);
	CHECK(res == 0);
	CHECK(closedir(d) == 0);

	/* fdopendir */
	fd = open(".", O_RDONLY | O_DIRECTORY); d = fdopendir(fd); CHECK(d && dirfd(d) == fd);
	total = 0; while (readdir(d)) total++; CHECK(total == n);
	closedir(d);
	CHECK(fcntl(fd, F_GETFD) == -1 && errno == EBADF);
	fd = open("file-aa0", O_RDONLY); CHECK(fdopendir(fd) == 0 && errno == ENOTDIR); close(fd);
	CHECK(opendir("file-aa0") == 0 && errno == ENOTDIR);
	CHECK(opendir("nonexistent") == 0 && errno == ENOENT);

	/* posix_getdents with types */
	fd = open(".", O_RDONLY | O_DIRECTORY);
	CHECK(posix_getdents(fd, gbuf, sizeof gbuf, 1) == -1 && errno == EINVAL);
	total = 0; k = 0;
	while ((got = posix_getdents(fd, gbuf, sizeof gbuf, 0)) > 0) {
		char *p = gbuf;
		while (p < gbuf + got) {
			struct posix_dent *pd = (struct posix_dent *)(void *)p;
			CHECK(((unsigned long)p & 7) == 0 && pd->d_reclen > 0);
			if (!strcmp(pd->d_name, "sub")) k += pd->d_type == DT_DIR || pd->d_type == DT_UNKNOWN;
			if (!strcmp(pd->d_name, "link")) k += pd->d_type == DT_LNK || pd->d_type == DT_UNKNOWN;
			if (!strcmp(pd->d_name, "file-aa0")) k += pd->d_type == DT_REG || pd->d_type == DT_UNKNOWN;
			total++;
			p += pd->d_reclen;
		}
	}
	CHECK(got == 0 && total == n && k == 3);
	close(fd);
	CHECK(posix_getdents(-1, gbuf, sizeof gbuf, 0) == -1 && errno == EBADF);

	/* a removed directory reads as empty */
	d = opendir("sub"); CHECK(rmdir("sub") == 0);
	errno = 0; CHECK(readdir(d) == 0 && errno == 0);
	closedir(d);

	/* utmpx: only where we may create the database and it does not exist */
	{
		struct stat st;
		if (geteuid() == 0 && stat("/run/utmp", &st) < 0 && stat("/var/run/utmp", &st) < 0 &&
		    stat("/run", &st) == 0 && (fd = open("/run/utmp", O_CREAT | O_EXCL | O_RDWR, 0664)) >= 0) {
			struct utmpx u, *up;
			char login[64];
			close(fd);
			setutxent();
			CHECK(getutxent() == 0);
			memset(&u, 0, sizeof u);
			u.ut_type = BOOT_TIME; u.ut_tv.tv_sec = 1700000000;
			CHECK(pututxline(&u) != 0);
			memset(&u, 0, sizeof u);
			u.ut_type = USER_PROCESS; u.ut_pid = 4242; strcpy(u.ut_line, "pts/77");
			strcpy(u.ut_id, "ts/7"); strcpy(u.ut_user, "tester"); strcpy(u.ut_host, "far.away");
			u.ut_tv.tv_sec = 2200000000L; u.ut_tv.tv_usec = 5;
			CHECK(pututxline(&u) != 0);
			u.ut_pid = 4243;                       /* replaces the entry with the same id */
			CHECK(pututxline(&u) != 0);
			CHECK(stat("/run/utmp", &st) == 0 && st.st_size == 2 * 384);
			setutxent();
			up = getutxent(); CHECK(up && up->ut_type == BOOT_TIME && up->ut_tv.tv_sec == 1700000000);
			up = getutxent(); CHECK(up && up->ut_type == USER_PROCESS && up->ut_pid == 4243 &&
			      !strcmp(up->ut_user, "tester") && !strcmp(up->ut_host, "far.away") &&
			      up->ut_tv.tv_sec == 2200000000L && up->ut_tv.tv_usec == 5);
			CHECK(getutxent() == 0);
			setutxent();
			memset(&u, 0, sizeof u); strcpy(u.ut_line, "pts/77");
			up = getutxline(&u); CHECK(up && up->ut_pid == 4243);
			setutxent();
			memset(&u, 0, sizeof u); u.ut_type = DEAD_PROCESS; strcpy(u.ut_id, "ts/7");
			up = getutxid(&u); CHECK(up && up->ut_type == USER_PROCESS);
			u = *up; u.ut_type = DEAD_PROCESS;
			CHECK(pututxline(&u) != 0);
			setutxent();
			memset(&u, 0, sizeof u); strcpy(u.ut_line, "pts/77");
			CHECK(getutxline(&u) == 0);
			endutxent();
			CHECK(unlink("/run/utmp") == 0);
			(void)login;
		}
	}
	/* getlogin: either a name that exists, or an error */
	{
		char *l = getlogin();
		CHECK(l == 0 ? errno != 0 : getpwnam(l) != 0);
		if (l) { char b[2]; CHECK(getlogin_r(b, sizeof b) == ERANGE || strlen(l) < 2); }
	}

	CHECK(chdir(cwd) == 0);
	{ char cmd[100] = "rm -rf "; strcat(cmd, dir); CHECK(system(cmd) == 0); }
	os(failures ? "failures\n" : "ok\n");
	oflush();
	return failures != 0;
}
