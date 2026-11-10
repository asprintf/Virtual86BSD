/* k_disk.c - disks as macOS names them: /dev/disk0 (block) and /dev/rdisk0
 * (raw, character), /dev/disk0s1 and /dev/rdisk0s1 for its first partition,
 * which the MBR (or the GPT behind a protective MBR) of the disk says.  The
 * block and raw devices show the same sectors; a partition's sector n is the
 * disk's sector start + n; the raw ones take only whole sectors; the
 * names of Linux's disks and of the old console are gone.  Reads only (the
 * disk is the running system's).  Prints "k_disk ok" or FAIL lines. */
#define _POSIX_C_SOURCE 202405L
#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/sysmacros.h>

static int failures;
#define CHECK(c) do { if (!(c)) { printf("FAIL line %d: %s\n", __LINE__, #c); failures++; } } while (0)

static int rd(const char *path, off_t off, void *buf, size_t n)
{
	int fd = open(path, O_RDONLY);
	if (fd < 0)
		return -1;
	ssize_t r = pread(fd, buf, n, off);
	close(fd);
	return r == (ssize_t)n ? 0 : -1;
}

static uint32_t le32(const unsigned char *p) { return p[0] | p[1] << 8 | p[2] << 16 | (uint32_t)p[3] << 24; }

int main(void)
{
	struct stat st;
	static unsigned char a[4096], b[4096], c[4096], mbr[512];

	CHECK(stat("/dev/disk0", &st) == 0 && S_ISBLK(st.st_mode) && major(st.st_rdev) == 8 && minor(st.st_rdev) == 0);
	CHECK(stat("/dev/disk0s1", &st) == 0 && S_ISBLK(st.st_mode) && major(st.st_rdev) == 8 && minor(st.st_rdev) == 1);
	CHECK(stat("/dev/rdisk0", &st) == 0 && S_ISCHR(st.st_mode) && minor(st.st_rdev) == 0);
	CHECK(stat("/dev/rdisk0s1", &st) == 0 && S_ISCHR(st.st_mode) && minor(st.st_rdev) == 1);
	CHECK(access("/dev/sda", F_OK) != 0 && access("/dev/vda", F_OK) != 0);
	CHECK(access("/dev/ttyS0", F_OK) != 0 && access("/dev/tty1", F_OK) != 0);
	CHECK(stat("/dev/tty00", &st) == 0 && S_ISCHR(st.st_mode));
	CHECK(stat("/dev/cua00", &st) == 0 && S_ISCHR(st.st_mode));
	CHECK(stat("/dev/ttyC0", &st) == 0 && S_ISCHR(st.st_mode));

	/* sector 0: the same from both, and a partition table */
	CHECK(rd("/dev/disk0", 0, a, 4096) == 0);
	CHECK(rd("/dev/rdisk0", 0, b, 4096) == 0);
	CHECK(memcmp(a, b, 4096) == 0);
	memcpy(mbr, a, 512);
	CHECK(mbr[510] == 0x55 && mbr[511] == 0xaa);
	const unsigned char *e = mbr + 446; /* the first entry: the root's */
	uint32_t start = le32(e + 8), len = le32(e + 12);
	CHECK(e[4] != 0 && len != 0);

	/* the first partition: sector n of it is sector start + n of the disk */
	if (e[4] != 0xee) {
		CHECK(rd("/dev/disk0", (off_t)start * 512, a, 4096) == 0);
		CHECK(rd("/dev/disk0s1", 0, b, 4096) == 0);
		CHECK(rd("/dev/rdisk0s1", 0, c, 4096) == 0);
		CHECK(memcmp(a, b, 4096) == 0 && memcmp(a, c, 4096) == 0);
		CHECK(rd("/dev/rdisk0", (off_t)start * 512, c, 4096) == 0 && memcmp(a, c, 4096) == 0);
		/* the end of the partition: its last sector, and nothing after */
		off_t size = (off_t)len * 512;
		CHECK(rd("/dev/rdisk0s1", size - 512, c, 512) == 0);
		int fd = open("/dev/rdisk0s1", O_RDONLY);
		CHECK(fd >= 0 && pread(fd, c, 512, size) == 0);
		close(fd);
	} else {
		CHECK(rd("/dev/disk0s1", 0, b, 4096) == 0 && rd("/dev/rdisk0s1", 0, c, 4096) == 0 && memcmp(b, c, 4096) == 0);
	}

	/* the raw device takes whole sectors, from a multiple of the sector */
	int fd = open("/dev/rdisk0", O_RDONLY);
	CHECK(fd >= 0);
	errno = 0;
	CHECK(pread(fd, a, 100, 0) == -1 && errno == EINVAL);
	errno = 0;
	CHECK(pread(fd, a, 512, 1) == -1 && errno == EINVAL);
	close(fd);
	/* ... and a write of that kind changes nothing (nothing is written) */
	fd = open("/dev/rdisk0", O_WRONLY);
	if (fd >= 0) {
		errno = 0;
		CHECK(pwrite(fd, a, 100, 0) == -1 && errno == EINVAL);
		close(fd);
	}
	CHECK(rd("/dev/rdisk0", 0, b, 512) == 0 && memcmp(b, mbr, 512) == 0);

	if (failures == 0)
		printf("k_disk ok\n");
	return failures != 0;
}
