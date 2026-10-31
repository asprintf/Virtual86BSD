#ifndef _GRP_H
#define _GRP_H
#include <bits/features.h>
#define __NEED_gid_t
#define __NEED_size_t
#include <bits/alltypes.h>

struct group {
	char *gr_name;
	char *gr_passwd;
	gid_t gr_gid;
	char **gr_mem;
};

__BEGIN_DECLS
struct group  *getgrgid(gid_t);
int            getgrgid_r(gid_t, struct group *, char *, size_t, struct group **);
struct group  *getgrnam(const char *);
int            getgrnam_r(const char *, struct group *, char *, size_t, struct group **);
#if __XSI_VISIBLE
void           endgrent(void);
struct group  *getgrent(void);
void           setgrent(void);
#endif
#if __EXT_VISIBLE
/* Not POSIX (a privileged operation); as on other systems. */
int            setgroups(size_t, const gid_t *);
#endif
__END_DECLS
#endif
