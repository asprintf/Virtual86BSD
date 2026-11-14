/*[SUSv4]*/
/* Test whether ftw visits every object once, a directory before its
   descendants, follows links, and stops on a non-zero return. */

#define _XOPEN_SOURCE 500
#include <errno.h>
#include <ftw.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "../basic.h"

static char root[64];

static struct { char path[256]; int flag; int isdir, isreg; } seen[32];
static int nseen, stopat, stopval;

static int fn(const char* path, const struct stat* st, int flag)
{
	if ( nseen == 32 )
		errx(1, "too many objects");
	snprintf(seen[nseen].path, sizeof(seen[nseen].path), "%s", path);
	seen[nseen].flag = flag;
	seen[nseen].isdir = flag != FTW_NS && S_ISDIR(st->st_mode);
	seen[nseen].isreg = flag != FTW_NS && S_ISREG(st->st_mode);
	nseen++;
	return nseen == stopat ? stopval : 0;
}

static void make(const char* rel, mode_t mode, const char* target)
{
	char path[128];
	snprintf(path, sizeof(path), "%s/%s", root, rel);
	if ( target )
	{
		if ( symlink(target, path) < 0 )
			err(1, "symlink %s", path);
	}
	else if ( mode & S_IFDIR )
	{
		if ( mkdir(path, mode & 07777) < 0 )
			err(1, "mkdir %s", path);
	}
	else
	{
		FILE* f = fopen(path, "w");
		if ( !f || fclose(f) )
			err(1, "create %s", path);
	}
}

static int find(const char* rel)
{
	char path[128];
	int i, n = -1;
	snprintf(path, sizeof(path), "%s%s%s", root, *rel ? "/" : "", rel);
	for ( i = 0; i < nseen; i++ )
		if ( !strcmp(seen[i].path, path) )
		{
			if ( n >= 0 )
				errx(1, "%s visited twice", path);
			n = i;
		}
	return n;
}

static void expect(const char* rel, int flag)
{
	int i = find(rel);
	if ( i < 0 )
		errx(1, "%s not visited", rel);
	if ( seen[i].flag != flag )
		errx(1, "%s has flag %d, not %d", rel, seen[i].flag, flag);
	if ( flag == FTW_D && !seen[i].isdir )
		errx(1, "%s: stat is not a directory", rel);
	if ( flag == FTW_F && !seen[i].isreg )
		errx(1, "%s: stat is not a regular file", rel);
}

static void before(const char* dir, const char* below)
{
	int i = find(dir), j = find(below);
	if ( i < 0 || j < 0 || i > j )
		errx(1, "%s not visited before %s", dir, below);
}

static void cleanup(void)
{
	char cmd[256];
	snprintf(cmd, sizeof(cmd), "chmod -R u+rwx '%s' 2>/dev/null; rm -rf '%s'", root, root);
	if ( system(cmd) )
		warn("cleanup");
}

int main(void)
{
	int r;
	strcpy(root, "/tmp/lb.ftw.XXXXXX");
	if ( !mkdtemp(root) )
		err(1, "mkdtemp");
	atexit(cleanup);
	make("a", 0, NULL);
	make("d", S_IFDIR | 0755, NULL);
	make("d/b", 0, NULL);
	make("d/sub", S_IFDIR | 0755, NULL);
	make("d/sub/c", 0, NULL);
	make("lnk", 0, "d");
	make("dang", 0, "nowhere");

	if ( (r = ftw(root, fn, 5)) != 0 )
		errx(1, "ftw returned %d", r);
	if ( seen[0].flag != FTW_D || strcmp(seen[0].path, root) != 0 )
		errx(1, "the first object is not the starting directory");
	expect("", FTW_D);
	expect("a", FTW_F);
	expect("d", FTW_D);
	expect("d/b", FTW_F);
	expect("d/sub", FTW_D);
	expect("d/sub/c", FTW_F);
	/* a link is followed as by stat() */
	expect("lnk", FTW_D);
	expect("lnk/b", FTW_F);
	expect("lnk/sub", FTW_D);
	expect("lnk/sub/c", FTW_F);
	/* a link whose stat() failed: FTW_SL or FTW_NS */
	r = find("dang");
	if ( r < 0 || (seen[r].flag != FTW_SL && seen[r].flag != FTW_NS) )
		errx(1, "the dangling link is not FTW_SL or FTW_NS");
	if ( nseen != 11 )
		errx(1, "%d objects visited, not 11", nseen);
	before("d", "d/b");
	before("d", "d/sub");
	before("d/sub", "d/sub/c");
	before("lnk", "lnk/sub");
	before("lnk/sub", "lnk/sub/c");

	/* one descriptor for each level is enough */
	nseen = 0;
	if ( ftw(root, fn, 1) != 0 || nseen != 11 )
		errx(1, "ftw with ndirs 1 visited %d objects", nseen);

	/* a non-zero return stops the walk and is the result */
	nseen = 0; stopat = 3; stopval = 42;
	if ( (r = ftw(root, fn, 5)) != 42 || nseen != 3 )
		errx(1, "stop on 42: ftw returned %d after %d calls", r, nseen);
	nseen = 0; stopat = 0;

	/* errors */
	char path[128];
	snprintf(path, sizeof(path), "%s/a/x", root);
	errno = 0;
	if ( ftw(path, fn, 5) != -1 || errno != ENOTDIR )
		errx(1, "ftw below a file: %s", strerror(errno));
	snprintf(path, sizeof(path), "%s/none", root);
	errno = 0;
	if ( ftw(path, fn, 5) != -1 || errno != ENOENT )
		errx(1, "ftw on a missing name: %s", strerror(errno));
	errno = 0;
	if ( ftw("", fn, 5) != -1 || errno != ENOENT )
		errx(1, "ftw on an empty path: %s", strerror(errno));
	/* ndirs 0 is invalid: EINVAL, or a walk all the same */
	nseen = 0;
	errno = 0;
	r = ftw(root, fn, 0);
	if ( r == -1 ? errno != EINVAL : (r != 0 || nseen != 11) )
		errx(1, "ftw with ndirs 0 returned %d, errno %d, %d objects", r, errno, nseen);

	/* permissions (root is not subject to them) */
	if ( geteuid() != 0 )
	{
		char dir[128];
		snprintf(dir, sizeof(dir), "%s/d/sub", root);
		if ( chmod(dir, 0) < 0 )
			err(1, "chmod");
		nseen = 0;
		if ( ftw(root, fn, 5) != 0 )
			err(1, "ftw with an unreadable directory");
		expect("d/sub", FTW_DNR);
		if ( find("d/sub/c") >= 0 )
			errx(1, "ftw went below an unreadable directory");
		if ( chmod(dir, 0444) < 0 )
			err(1, "chmod");
		nseen = 0;
		if ( ftw(root, fn, 5) != 0 )
			err(1, "ftw with a directory that cannot be searched");
		expect("d/sub", FTW_D);
		expect("d/sub/c", FTW_NS);
	}
	return 0;
}
