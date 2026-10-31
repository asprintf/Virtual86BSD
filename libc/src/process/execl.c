#include <unistd.h>
#include <stdarg.h>
#include "libc.h"

int __execvpe(const char *, char *const [], char *const []);

/* Count the arguments up to and including the terminating null pointer. */
#define COUNT(ap, first, n) do { va_start(ap, first); n = 1; \
	while (va_arg(ap, const char *)) { n++; } va_end(ap); } while (0)

int execl(const char *path, const char *arg, ...)
{
	va_list ap;
	int n, i;
	COUNT(ap, arg, n);
	{
		char *argv[n + 1];
		va_start(ap, arg);
		argv[0] = (char *)arg;
		for (i = 1; i <= n; i++) argv[i] = va_arg(ap, char *);
		va_end(ap);
		return execve(path, argv, __environ);
	}
}

int execlp(const char *file, const char *arg, ...)
{
	va_list ap;
	int n, i;
	COUNT(ap, arg, n);
	{
		char *argv[n + 1];
		va_start(ap, arg);
		argv[0] = (char *)arg;
		for (i = 1; i <= n; i++) argv[i] = va_arg(ap, char *);
		va_end(ap);
		return __execvpe(file, argv, __environ);
	}
}

int execle(const char *path, const char *arg, ...)
{
	va_list ap;
	int n, i;
	char **envp;
	COUNT(ap, arg, n);
	{
		char *argv[n + 1];
		va_start(ap, arg);
		argv[0] = (char *)arg;
		for (i = 1; i <= n; i++) argv[i] = va_arg(ap, char *);
		envp = va_arg(ap, char **);
		va_end(ap);
		return execve(path, argv, envp);
	}
}
