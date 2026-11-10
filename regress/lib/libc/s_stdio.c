/* Streams: files, buffering, seeking, memory streams, popen, exit flush. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/wait.h>
#include <signal.h>
#include "out.h"

static char path[64], path2[64];

static int readfile(const char *p, char *buf, size_t n)
{
	int fd = open(p, O_RDONLY), k;
	if (fd < 0) return -1;
	k = (int)read(fd, buf, n - 1);
	close(fd);
	buf[k > 0 ? k : 0] = 0;
	return k;
}

int main(void)
{
	FILE *f, *g;
	char buf[4096], *line = 0, *ms = 0;
	size_t cap = 0, msize = 0;
	ssize_t n;
	int i, c, st;
	pid_t pid;

	snprintf(path, sizeof path, "/tmp/libc-stdio.%d", (int)getpid());
	snprintf(path2, sizeof path2, "%s.renamed", path);

	/* write, read back, modes */
	f = fopen(path, "w");
	CHECK(f != 0);
	CHECK(fputs("line one\n", f) >= 0 && fprintf(f, "%d %s\n", 2, "two") == 6 && fputc('3', f) == '3');
	CHECK(fwrite("\nfour\n", 1, 6, f) == 6);
	CHECK(fclose(f) == 0);
	CHECK(readfile(path, buf, sizeof buf) == 22 && !strcmp(buf, "line one\n2 two\n3\nfour\n"));
	CHECK(fopen(path, "wx") == 0 && errno == EEXIST);
	CHECK(fopen(path, "q") == 0 && errno == EINVAL);
	CHECK(fopen("/nonexistent/x", "r") == 0 && errno == ENOENT);
	f = fopen(path, "a");
	CHECK(f && fputs("five\n", f) >= 0 && fclose(f) == 0);
	f = fopen(path, "re");
	CHECK(f && (fcntl(fileno(f), F_GETFD) & FD_CLOEXEC));
	CHECK(fgets(buf, sizeof buf, f) && !strcmp(buf, "line one\n"));
	CHECK(fgets(buf, 4, f) && !strcmp(buf, "2 t"));
	CHECK(fgetc(f) == 'w' && ungetc('W', f) == 'W' && fgetc(f) == 'W');
	CHECK(ftell(f) == 13);
	CHECK((n = getline(&line, &cap, f)) == 2 && !strcmp(line, "o\n"));
	CHECK((n = getdelim(&line, &cap, 'u', f)) == 5 && !strcmp(line, "3\nfou"));
	CHECK(fseek(f, -5, SEEK_END) == 0 && fgets(buf, sizeof buf, f) && !strcmp(buf, "five\n"));
	CHECK(fgetc(f) == EOF && feof(f) && !ferror(f));
	CHECK(getline(&line, &cap, f) == -1);
	clearerr(f);
	CHECK(!feof(f));
	rewind(f);
	CHECK(fread(buf, 1, 4, f) == 4 && !memcmp(buf, "line", 4));
	{ fpos_t pos; CHECK(fgetpos(f, &pos) == 0); fgetc(f); CHECK(fsetpos(f, &pos) == 0 && fgetc(f) == ' '); }
	CHECK(fread(buf, 5, 100, f) == 4 && feof(f));   /* 22 bytes left -> 4 whole items of 5 */
	CHECK(fputc('x', f) == EOF && ferror(f));       /* read-only stream */
	CHECK(fclose(f) == 0);
	free(line);

	/* r+ and switching between reading and writing */
	f = fopen(path, "r+");
	CHECK(f && fgetc(f) == 'l');
	CHECK(fseek(f, 0, SEEK_CUR) == 0 && fputs("INE", f) >= 0);
	CHECK(fflush(f) == 0 && fseek(f, 0, SEEK_SET) == 0 && fgets(buf, sizeof buf, f) && !strcmp(buf, "lINE one\n"));
	CHECK(fclose(f) == 0);
	CHECK(rename(path, path2) == 0 && remove(path) == -1 && errno == ENOENT);

	/* setvbuf, unbuffered and line-buffered output */
	{
		int fds[2];
		CHECK(pipe(fds) == 0);
		g = fdopen(fds[1], "w");
		CHECK(g && setvbuf(g, 0, _IOLBF, 0) == 0);
		fputs("partial", g);
		fcntl(fds[0], F_SETFL, O_NONBLOCK);
		CHECK(read(fds[0], buf, sizeof buf) == -1 && errno == EAGAIN);   /* still buffered */
		fputs(" line\n", g);
		CHECK(read(fds[0], buf, sizeof buf) == 13);
		CHECK(setvbuf(g, 0, _IONBF, 0) == 0 && fputc('z', g) == 'z' && read(fds[0], buf, 10) == 1);
		CHECK(fclose(g) == 0);
		close(fds[0]);
		CHECK(fdopen(fds[0], "r") == 0 && errno == EBADF);
	}

	/* tmpfile, freopen, fdopen mode check */
	f = tmpfile();
	CHECK(f && fputs("temporary", f) >= 0 && fseek(f, 3, SEEK_SET) == 0 && fgetc(f) == 'p');
	CHECK(fclose(f) == 0);
	{ char t[L_tmpnam]; CHECK(tmpnam(t) && !strncmp(t, "/tmp/", 5)); }
	f = fopen(path2, "r");
	CHECK(f && freopen(0, "r", f) == f && fgetc(f) == 'l');
	CHECK(freopen(path2, "a", f) == f && fputs("appended\n", f) >= 0 && fclose(f) == 0);
	CHECK(readfile(path2, buf, sizeof buf) > 0 && strstr(buf, "five\nappended\n"));
	{ int fd = open(path2, O_RDONLY); CHECK(fdopen(fd, "w") == 0 && errno == EINVAL); close(fd); }

	/* fmemopen */
	{
		char mem[16] = "abcdef";
		f = fmemopen(mem, sizeof mem, "r");
		CHECK(f && fgetc(f) == 'a' && fseek(f, 4, SEEK_SET) == 0 && fgetc(f) == 'e');
		CHECK(fclose(f) == 0);
		f = fmemopen(mem, sizeof mem, "w");
		CHECK(f && fprintf(f, "%d+%d", 12, 34) == 5 && fflush(f) == 0 && !strcmp(mem, "12+34"));
		CHECK(ftell(f) == 5 && fclose(f) == 0);
		f = fmemopen(0, 32, "w+");
		CHECK(f && fputs("scratch", f) >= 0 && fseek(f, 0, SEEK_SET) == 0 && fgets(buf, 20, f) && !strcmp(buf, "scratch"));
		CHECK(fclose(f) == 0);
		CHECK(fmemopen(mem, 0, "r") == 0 && errno == EINVAL);
	}

	/* open_memstream */
	f = open_memstream(&ms, &msize);
	CHECK(f != 0);
	for (i = 0; i < 1000; i++) fprintf(f, "%04d,", i);
	CHECK(fflush(f) == 0 && msize == 5000 && !strncmp(ms, "0000,0001,", 10) && ms[5000] == 0);
	CHECK(fseek(f, 2, SEEK_SET) == 0 && fputs("XY", f) >= 0 && fclose(f) == 0 && msize == 4 && !strncmp(ms, "00XY", 4));
	free(ms);

	/* asprintf, dprintf */
	{
		char *s = 0;
		CHECK(asprintf(&s, "%s-%05.1f", "pi", 3.14159) == 8 && !strcmp(s, "pi-003.1"));
		free(s);
		CHECK(dprintf(-1, "x") == -1);
	}

	/* popen, pclose */
	f = popen("echo popen-ok; exit 3", "r");
	CHECK(f && fgets(buf, sizeof buf, f) && !strcmp(buf, "popen-ok\n"));
	st = f ? pclose(f) : -1;
	CHECK(WIFEXITED(st) && WEXITSTATUS(st) == 3);
	f = popen("cat > /dev/null", "w");
	CHECK(f && fputs("ignored\n", f) >= 0 && pclose(f) == 0);
	CHECK(popen("true", "x") == 0 && errno == EINVAL);

	/* exit() flushes; _exit() does not; perror */
	{
		int fds[2];
		CHECK(pipe(fds) == 0);
		pid = fork();
		if (!pid) {
			dup2(fds[1], 1); dup2(fds[1], 2); close(fds[0]); close(fds[1]);
			setvbuf(stdout, 0, _IOFBF, 0);
			printf("flushed at exit;");
			errno = ENOENT;
			perror("perror");
			exit(0);
		}
		close(fds[1]);
		n = 0;
		while ((i = (int)read(fds[0], buf + n, sizeof buf - 1 - (size_t)n)) > 0) n += i;
		buf[n] = 0;
		close(fds[0]);
		waitpid(pid, &st, 0);
		CHECK(!strcmp(buf, "perror: No such file or directory\nflushed at exit;"));
		CHECK(pipe(fds) == 0);
		pid = fork();
		if (!pid) {
			dup2(fds[1], 1); close(fds[0]); close(fds[1]);
			setvbuf(stdout, 0, _IOFBF, 0);
			printf("lost");
			_exit(0);
		}
		close(fds[1]);
		CHECK(read(fds[0], buf, sizeof buf) == 0);
		close(fds[0]);
		waitpid(pid, &st, 0);
	}

	/* locking */
	flockfile(stdout);
	CHECK(ftrylockfile(stdout) == 0);   /* recursive for the owner */
	funlockfile(stdout);
	funlockfile(stdout);
	CHECK(putc_unlocked('x', stdout) == 'x' && fflush(stdout) == 0);   /* a stray x in the output */

	/* sscanf/ssprintf odds and ends */
	{
		char c3[4] = "";
		CHECK(sscanf("  ", "%3c", c3) == EOF);
		CHECK(sscanf("", "%d", &i) == EOF && sscanf("x", "%d", &i) == 0);
		CHECK(sscanf("12abc", "%d%n", &i, &st) == 1 && i == 12 && st == 2);
	}

	/* after a write error, output goes on (only the indicator stays set) */
	{
		int p1[2], p2[2];
		char rb[64];
		void (*old)(int) = signal(SIGPIPE, SIG_IGN);
		CHECK(pipe(p1) == 0 && pipe(p2) == 0);
		close(p1[0]);
		g = fdopen(p1[1], "w");
		CHECK(g && fputs("lost", g) >= 0 && fflush(g) == EOF && ferror(g));
		CHECK(dup2(p2[1], p1[1]) == p1[1]);
		CHECK(fprintf(g, "%d", 123) == 3 && fputs("x", g) >= 0 && fputc('w', g) == 'w');
		CHECK(fflush(g) == 0 && ferror(g));
		CHECK(read(p2[0], rb, sizeof rb) == 5 && !memcmp(rb, "123xw", 5));
		fclose(g);
		close(p2[0]); close(p2[1]);
		signal(SIGPIPE, old);
	}
	/* "a" (not "a+") starts at the end, as glibc: ftell gives the size */
	f = fopen(path2, "w");
	CHECK(f && fputs("hello\n", f) >= 0 && fclose(f) == 0);
	f = fopen(path2, "a");
	CHECK(f && ftell(f) == 6 && fputs("x\n", f) >= 0 && ftell(f) == 8 && fclose(f) == 0);
	f = fopen(path2, "a+");
	CHECK(f && ftell(f) == 0 && fgetc(f) == 'h' && fclose(f) == 0);
	CHECK((f = freopen(path2, "a", stdin)) && ftell(f) == 8);
	unlink(path2);
	os(failures ? "stdio FAILED\n" : "stdio ok\n");
	oflush();
	return failures != 0;
}
