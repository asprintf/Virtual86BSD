/* k_umount.c - umount(2) (kern/vfs_subr.c do_umount): a file system that
   nothing uses can be unmounted, one that is used cannot.  As root.
 *
 * What is checked: a fresh tmpfs unmounts; after files were made and closed
 * it still does; it is busy (EBUSY) while a process has its directory as
 * the working directory, while a file in it is open, while another file
 * system is mounted below it, and for /; each is
 * free again once the use ends (the other process has exited, the file is
 * closed); a shared mapping of a tmpfs file does not hold it; an
 * unlinked file that is still open holds it; mounting on a mount stacks and
 * the top one unmounts first; EINVAL for a directory that is no mount, ENOENT,
 * EPERM for who is not root. */
#define _DEFAULT_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

static int failures;
#define CHECK(c) do { if (!(c)) { printf("FAIL line %d: %s (errno %d)\n", __LINE__, #c, errno); failures++; } } while (0)

static char dir[64], sub[80];

static int mnt(const char *where) { return mount("tmpfs", where, "tmpfs", 0, 0); }

int main(void)
{
	int fd, st;
	void *p;
	pid_t kid;
	int pfd[2];

	if (geteuid() != 0) { printf("k_umount: run as root\n"); return 1; }
	snprintf(dir, sizeof dir, "/tmp/k_umount.%d", (int)getpid());
	snprintf(sub, sizeof sub, "%s/m", dir);
	CHECK(mkdir(dir, 0700) == 0);

	/* a fresh file system unmounts */
	CHECK(mnt(dir) == 0);
	CHECK(umount(dir) == 0);
	CHECK(umount(dir) == -1 && errno == EINVAL); /* no longer a mount */

	/* files made and closed do not hold it */
	CHECK(mnt(dir) == 0);
	{
		char f[96];
		snprintf(f, sizeof f, "%s/f", dir);
		fd = open(f, O_CREAT | O_RDWR, 0600);
		CHECK(fd >= 0 && write(fd, "x", 1) == 1);
		close(fd);
		snprintf(f, sizeof f, "%s/d", dir);
		CHECK(mkdir(f, 0700) == 0);
	}
	CHECK(umount(dir) == 0);

	/* an open file */
	CHECK(mnt(dir) == 0);
	{
		char f[96];
		snprintf(f, sizeof f, "%s/f", dir);
		fd = open(f, O_CREAT | O_RDWR, 0600);
		CHECK(fd >= 0);
		CHECK(umount(dir) == -1 && errno == EBUSY);
		/* an unlinked file that is open holds it too */
		CHECK(unlink(f) == 0);
		CHECK(umount(dir) == -1 && errno == EBUSY);
		close(fd);
		CHECK(umount(dir) == 0);
	}

	/* a shared mapping of a tmpfs file holds the file's pages, not the file: the
	   file system can go, and the mapping stays good (the pages are counted) */
	CHECK(mnt(dir) == 0);
	{
		char f[96];
		snprintf(f, sizeof f, "%s/f", dir);
		fd = open(f, O_CREAT | O_RDWR, 0600);
		CHECK(fd >= 0 && ftruncate(fd, 4096) == 0);
		p = mmap(0, 4096, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
		CHECK(p != MAP_FAILED);
		strcpy(p, "still here");
		close(fd);
		CHECK(umount(dir) == 0);
		CHECK(strcmp(p, "still here") == 0);
		strcpy(p, "and writable");
		CHECK(munmap(p, 4096) == 0);
	}

	/* a working directory, of this process and of another one */
	CHECK(mnt(dir) == 0);
	CHECK(chdir(dir) == 0);
	CHECK(umount(dir) == -1 && errno == EBUSY);
	CHECK(chdir("/") == 0);
	CHECK(umount(dir) == 0);
	CHECK(mnt(dir) == 0);
	CHECK(pipe(pfd) == 0);
	kid = fork();
	if (kid == 0) {
		char c;
		close(pfd[1]);
		if (chdir(dir)) _exit(2);
		read(pfd[0], &c, 1); /* until the parent lets it go */
		_exit(0);
	}
	close(pfd[0]);
	usleep(300000);
	CHECK(umount(dir) == -1 && errno == EBUSY);
	close(pfd[1]);
	CHECK(waitpid(kid, &st, 0) == kid && WIFEXITED(st) && WEXITSTATUS(st) == 0);
	CHECK(umount(dir) == 0);

	/* a file system mounted below it; the inner one goes first */
	CHECK(mnt(dir) == 0);
	CHECK(mkdir(sub, 0700) == 0);
	CHECK(mnt(sub) == 0);
	CHECK(umount(dir) == -1 && errno == EBUSY);
	CHECK(umount(sub) == 0);
	CHECK(umount(dir) == 0);

	/* mounting on a mount stacks; the top one comes off first, and what it
	   covered is back */
	CHECK(mnt(dir) == 0);
	{
		char f[96];
		snprintf(f, sizeof f, "%s/below", dir);
		fd = open(f, O_CREAT | O_RDWR, 0600);
		CHECK(fd >= 0);
		close(fd);
		CHECK(mnt(dir) == 0);
		CHECK(access(f, F_OK) == -1 && errno == ENOENT); /* covered */
		CHECK(umount(dir) == 0);
		CHECK(access(f, F_OK) == 0);
		CHECK(umount(dir) == 0);
	}

	/* the others */
	CHECK(umount("/") == -1 && errno == EBUSY);
	CHECK(umount("/nonexistent/dir") == -1 && errno == ENOENT);
	CHECK(umount(dir) == -1 && errno == EINVAL);
	kid = fork();
	if (kid == 0) {
		if (setgid(65534) || setuid(65534)) _exit(2);
		_exit(umount("/proc") == -1 && errno == EPERM ? 0 : 1);
	}
	CHECK(waitpid(kid, &st, 0) == kid && WIFEXITED(st) && WEXITSTATUS(st) == 0);

	rmdir(dir);
	if (failures) { printf("k_umount: %d FAILED\n", failures); return 1; }
	printf("k_umount: ok\n");
	return 0;
}
