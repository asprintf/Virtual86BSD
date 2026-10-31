#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include <limits.h>
#include "syscall.h"
#include "libc.h"

int execve(const char *p, char *const argv[], char *const envp[])
{
	return (int)__sys3(SYS_execve, p, argv, envp);
}

int execv(const char *p, char *const argv[])
{
	return execve(p, argv, __environ);
}

int fexecve(int fd, char *const argv[], char *const envp[])
{
	return (int)__sys5(SYS_execveat, fd, "", argv, envp, 0x1000 /* AT_EMPTY_PATH */);
}

/* Run a file that is not an executable binary as a shell script. */
static int exec_script(const char *path, char *const argv[], char *const envp[])
{
	int n = 0;
	while (argv[n]) n++;
	{
		const char *nargv[n + 3];
		int i;
		nargv[0] = "sh";
		nargv[1] = path;
		for (i = 1; i <= n; i++) nargv[i + 1] = argv[i];
		if (n == 0) nargv[2] = 0;
		return execve("/bin/sh", (char *const *)nargv, envp);
	}
}

int __execvpe(const char *file, char *const argv[], char *const envp[])
{
	const char *path, *p, *z;
	size_t l, k;
	int seen_eacces = 0;

	if (!*file) { errno = ENOENT; return -1; }
	if (strchr(file, '/')) {
		execve(file, argv, envp);
		if (errno == ENOEXEC) exec_script(file, argv, envp);
		return -1;
	}
	path = 0;
	{
		char **e;
		for (e = __environ; e && *e; e++)
			if (!strncmp(*e, "PATH=", 5)) { path = *e + 5; break; }
	}
	if (!path) path = "/usr/local/bin:/bin:/usr/bin";
	k = strnlen(file, NAME_MAX + 1);
	if (k > NAME_MAX) { errno = ENAMETOOLONG; return -1; }
	for (p = path; ; p = z + 1) {
		char b[PATH_MAX];
		z = strchr(p, ':');
		if (!z) z = p + strlen(p);
		l = (size_t)(z - p);
		if (l + k + 2 <= sizeof b) {
			memcpy(b, p, l);
			b[l] = '/';
			if (!l) b[0] = '.';  /* empty element: current directory */
			memcpy(b + (l ? l + 1 : 2), file, k + 1);
			if (!l) b[1] = '/';
			execve(b, argv, envp);
			switch (errno) {
			case EACCES: seen_eacces = 1;
			case ENOENT: case ENOTDIR: break;
			case ENOEXEC: exec_script(b, argv, envp); return -1;
			default: return -1;
			}
		}
		if (!*z) break;
	}
	if (seen_eacces) errno = EACCES;
	return -1;
}

int execvp(const char *file, char *const argv[])
{
	return __execvpe(file, argv, __environ);
}
