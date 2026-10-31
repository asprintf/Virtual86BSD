/* <fcntl.h>, <sys/stat.h>, <sys/statvfs.h> and the file functions of <unistd.h>. */
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <sys/wait.h>
#include <sys/uio.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <limits.h>
#include "out.h"

int main(void)
{
	char dir[64] = "/tmp/libc-test.", buf[PATH_MAX], cwd[PATH_MAX];
	char *p;
	int fd, fd2, fds[2], st, n;
	struct stat sa, sb;
	struct statvfs sv;
	struct timespec ts[2];
	struct iovec iov[2];
	pid_t pid;
	mode_t old;

	/* a private directory */
	{ char num[16], *q = num + sizeof num; unsigned u = (unsigned)getpid(); *--q = 0;
	  do *--q = (char)('0' + u % 10); while (u /= 10); strcat(dir, q); }
	CHECK(getcwd(cwd, sizeof cwd) == cwd && cwd[0] == '/');
	old = umask(022);
	CHECK(mkdir(dir, 0777) == 0);
	CHECK(mkdir(dir, 0777) == -1 && errno == EEXIST);
	CHECK(stat(dir, &sa) == 0 && S_ISDIR(sa.st_mode) && (sa.st_mode & 0777) == 0755);
	CHECK(chdir(dir) == 0);
	/* pwrite: at the offset even with O_APPEND (Linux: from 6.9) */
	fd = open("ap", O_RDWR | O_CREAT | O_TRUNC | O_APPEND, 0644);
	CHECK(fd >= 0 && write(fd, "abcdef", 6) == 6 && pwrite(fd, "XY", 2, 1) == 2);
	memset(buf, 0, 8);
	CHECK(pread(fd, buf, 7, 0) == 6 && !strcmp(buf, "aXYdef") && lseek(fd, 0, SEEK_CUR) == 6);
	CHECK(write(fd, "!", 1) == 1 && pread(fd, buf, 7, 0) == 7 && buf[6] == 0x21);
	close(fd);
	CHECK(unlink("ap") == 0);
	/* O_EXEC: execute permission (even for root, some x bit) */
	CHECK((fd = open("nx", O_WRONLY | O_CREAT | O_TRUNC, 0644)) >= 0 && close(fd) == 0);
	errno = 0;
	CHECK(open("nx", O_EXEC) == -1 && errno == EACCES);
	CHECK(chmod("nx", 0755) == 0 && (fd = open("nx", O_EXEC)) >= 0 && close(fd) == 0);
	CHECK((fd = open(".", O_SEARCH)) >= 0 && close(fd) == 0);
	CHECK(unlink("nx") == 0);
	CHECK(getcwd(buf, sizeof buf) && !strcmp(buf, dir));
	CHECK(getcwd(buf, 3) == 0 && errno == ERANGE);
	p = getcwd(0, 0); CHECK(p && !strcmp(p, dir)); free(p);

	/* open, write, read, lseek, pread, pwrite, ftruncate */
	fd = open("f", O_RDWR | O_CREAT | O_EXCL, 0666);
	CHECK(fd >= 0);
	CHECK(open("f", O_RDWR | O_CREAT | O_EXCL, 0666) == -1 && errno == EEXIST);
	CHECK(write(fd, "hello world", 11) == 11);
	CHECK(lseek(fd, 0, SEEK_CUR) == 11);
	CHECK(lseek(fd, 6, SEEK_SET) == 6);
	CHECK(read(fd, buf, sizeof buf) == 5 && !memcmp(buf, "world", 5));
	CHECK(pread(fd, buf, 5, 0) == 5 && !memcmp(buf, "hello", 5));
	CHECK(pwrite(fd, "J", 1, 0) == 1 && lseek(fd, 0, SEEK_CUR) == 11);
	iov[0].iov_base = "ab"; iov[0].iov_len = 2; iov[1].iov_base = "cd"; iov[1].iov_len = 2;
	CHECK(writev(fd, iov, 2) == 4);
	CHECK(fstat(fd, &sa) == 0 && S_ISREG(sa.st_mode) && sa.st_size == 15 && (sa.st_mode & 0777) == 0644);
	CHECK(ftruncate(fd, 5) == 0 && fstat(fd, &sa) == 0 && sa.st_size == 5);
	CHECK(fsync(fd) == 0 && fdatasync(fd) == 0);
	CHECK(fcntl(fd, F_GETFL) == O_RDWR);
	CHECK(fcntl(fd, F_SETFL, O_APPEND) == 0 && (fcntl(fd, F_GETFL) & O_APPEND));
	CHECK(read(-1, buf, 1) == -1 && errno == EBADF);
	CHECK(posix_fallocate(fd, 0, 100) == 0 && fstat(fd, &sa) == 0 && sa.st_size == 100);
	CHECK(posix_fadvise(fd, 0, 0, POSIX_FADV_SEQUENTIAL) == 0);
	CHECK(lockf(fd, F_TLOCK, 0) == 0 && lockf(fd, F_TEST, 0) == 0 && lockf(fd, F_ULOCK, 0) == 0);

	/* descriptor flags, dup family, close-on-exec and close-on-fork */
	CHECK(fcntl(fd, F_GETFD) == 0);
	CHECK(fcntl(fd, F_SETFD, FD_CLOEXEC | FD_CLOFORK) == 0);
	CHECK(fcntl(fd, F_GETFD) == (FD_CLOEXEC | FD_CLOFORK));
	fd2 = dup(fd); CHECK(fd2 > fd && fcntl(fd2, F_GETFD) == 0);
	CHECK(dup2(fd, fd2) == fd2 && fcntl(fd2, F_GETFD) == 0);
	CHECK(dup2(fd2, fd2) == fd2);
	CHECK(dup2(-1, fd2) == -1 && errno == EBADF);
	CHECK(dup3(fd, fd2, O_CLOFORK) == fd2 && fcntl(fd2, F_GETFD) == FD_CLOFORK);
	CHECK(dup3(fd, fd, 0) == -1 && errno == EINVAL);
	CHECK(dup3(fd, fd2, O_APPEND) == -1 && errno == EINVAL);
	n = fcntl(fd, F_DUPFD_CLOFORK, 50); CHECK(n >= 50 && fcntl(n, F_GETFD) == FD_CLOFORK);
	CHECK(close(n) == 0);
	n = fcntl(fd, F_DUPFD_CLOEXEC, 50); CHECK(n >= 50 && fcntl(n, F_GETFD) == FD_CLOEXEC);  /* reused number starts clear */
	CHECK(close(n) == 0 && close(n) == -1 && errno == EBADF);
	fd2 = open("f", O_RDONLY | O_CLOFORK);
	CHECK(fd2 >= 0 && fcntl(fd2, F_GETFD) == FD_CLOFORK);
	CHECK(pipe2(fds, O_CLOFORK | O_CLOEXEC) == 0 && fcntl(fds[0], F_GETFD) == (FD_CLOFORK | FD_CLOEXEC));
	CHECK(pipe2(fds + 0, O_APPEND) == -1 && errno == EINVAL);
	pid = fork();
	if (pid == 0) {
		/* in the child: close-on-fork descriptors are gone, the others stay */
		int ok = fcntl(fd2, F_GETFD) == -1 && errno == EBADF &&
		         fcntl(fds[0], F_GETFD) == -1 && fcntl(fds[1], F_GETFD) == -1 &&
		         fcntl(0, F_GETFD) >= 0 && fcntl(fd, F_GETFD) == -1;
		_exit(ok ? 0 : 1);
	}
	CHECK(pid > 0 && waitpid(pid, &st, 0) == pid && WIFEXITED(st) && WEXITSTATUS(st) == 0);
	CHECK(fcntl(fd2, F_GETFD) == FD_CLOFORK);
	close(fds[0]); close(fds[1]);
	CHECK(close(fd2) == 0 && posix_close(fd, 0) == 0);

	/* pipes */
	CHECK(pipe(fds) == 0);
	CHECK(write(fds[1], "xyz", 3) == 3 && read(fds[0], buf, 10) == 3);
	CHECK(fstat(fds[0], &sa) == 0 && S_ISFIFO(sa.st_mode));
	CHECK(isatty(fds[0]) == 0 && errno == ENOTTY);
	close(fds[0]); close(fds[1]);

	/* links, names, modes, times */
	CHECK(link("f", "g") == 0 && stat("f", &sa) == 0 && stat("g", &sb) == 0 &&
	      sa.st_ino == sb.st_ino && sa.st_nlink == 2);
	CHECK(symlink("f", "s") == 0 && lstat("s", &sa) == 0 && S_ISLNK(sa.st_mode) && sa.st_size == 1);
	CHECK(readlink("s", buf, sizeof buf) == 1 && buf[0] == 'f');
	CHECK(readlink("f", buf, sizeof buf) == -1 && errno == EINVAL);
	CHECK(fstatat(AT_FDCWD, "s", &sa, AT_SYMLINK_NOFOLLOW) == 0 && S_ISLNK(sa.st_mode));
	CHECK(fstatat(AT_FDCWD, "s", &sa, 0) == 0 && S_ISREG(sa.st_mode));
	CHECK(fstatat(AT_FDCWD, "s", &sa, 12345) == -1 && errno == EINVAL);
	CHECK(chmod("f", 0600) == 0 && stat("f", &sa) == 0 && (sa.st_mode & 07777) == 0600);
	CHECK(fchmodat(AT_FDCWD, "f", 0640, 0) == 0 && stat("f", &sa) == 0 && (sa.st_mode & 07777) == 0640);
	CHECK(fchmodat(AT_FDCWD, "s", 0600, AT_SYMLINK_NOFOLLOW) == -1 && errno == EOPNOTSUPP);
	CHECK(access("f", R_OK | W_OK) == 0 && access("nonexistent", F_OK) == -1 && errno == ENOENT);
	CHECK(faccessat(AT_FDCWD, "f", R_OK, AT_EACCESS) == 0);
	ts[0].tv_sec = 1000000000; ts[0].tv_nsec = 5;
	ts[1].tv_sec = 1234567890; ts[1].tv_nsec = 0;
	CHECK(utimensat(AT_FDCWD, "f", ts, 0) == 0 && stat("f", &sa) == 0 &&
	      sa.st_atim.tv_sec == 1000000000 && sa.st_atim.tv_nsec == 5 && sa.st_mtime == 1234567890);
	ts[0].tv_nsec = UTIME_OMIT; ts[1].tv_nsec = UTIME_NOW;
	CHECK(utimensat(AT_FDCWD, "f", ts, 0) == 0 && stat("f", &sa) == 0 &&
	      sa.st_atime == 1000000000 && sa.st_mtime > 1234567890);
	CHECK(mkfifo("p", 0600) == 0 && lstat("p", &sa) == 0 && S_ISFIFO(sa.st_mode));
	CHECK(mkdirat(AT_FDCWD, "d", 0700) == 0);
	fd = open("d", O_RDONLY | O_DIRECTORY);
	CHECK(fd >= 0 && openat(fd, "inner", O_WRONLY | O_CREAT, 0600) >= 0);
	CHECK(open("f", O_RDONLY | O_DIRECTORY) == -1 && errno == ENOTDIR);
	CHECK(open("s", O_RDONLY | O_NOFOLLOW) == -1 && errno == ELOOP);
	CHECK(unlinkat(fd, "inner", 0) == 0 && close(fd) == 0);
	CHECK(rmdir("d") == 0 && unlink("p") == 0 && unlink("s") == 0 && unlink("g") == 0);
	CHECK(rmdir("f") == -1 && errno == ENOTDIR);
	CHECK(truncate("f", 3) == 0 && stat("f", &sa) == 0 && sa.st_size == 3);
	CHECK(statvfs(".", &sv) == 0 && sv.f_bsize > 0 && sv.f_namemax >= 14);
	CHECK(pathconf(".", _PC_NAME_MAX) >= 14 && pathconf(".", _PC_PATH_MAX) == PATH_MAX);
	CHECK(pathconf(".", 9999) == -1 && errno == EINVAL);
	CHECK(unlink("f") == 0 && unlink("f") == -1 && errno == ENOENT);
	fd = open(".", O_SEARCH);
	CHECK(fd >= 0 && chdir("/") == 0 && fchdir(fd) == 0 && getcwd(buf, sizeof buf) && !strcmp(buf, dir));
	close(fd);
	/* deeper than the kernel's getcwd reports: found by walking up */
	{
		char name[101], *big = malloc(16384), *m;
		int k;
		memset(name, 'd', 100);
		name[100] = 0;
		CHECK(chdir(dir) == 0);
		for (k = 0; k < 45; k++)
			if (mkdir(name, 0700) || chdir(name)) break;
		CHECK(k == 45);
		CHECK(big && getcwd(big, 16384) == big && strlen(big) == strlen(dir) + 45 * 101);
		CHECK(big && !strncmp(big, dir, strlen(dir)) && !strcmp(big + strlen(big) - 100, name));
		CHECK(getcwd(buf, sizeof buf) == 0 && errno == ERANGE);
		CHECK((m = getcwd(0, 0)) && !strcmp(m, big));
		free(m);
		free(big);
		for (; k > 0; k--)
			if (chdir("..") || rmdir(name)) break;
		CHECK(k == 0);
		CHECK(chdir(cwd) == 0);
	}
	CHECK(chdir(cwd) == 0 && rmdir(dir) == 0);
	umask(old);
	os(failures ? "files FAILED\n" : "files ok\n");
	oflush();
	return failures != 0;
}
