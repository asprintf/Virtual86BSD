#ifndef _PWD_H
#define _PWD_H
#include <bits/features.h>
#define __NEED_uid_t
#define __NEED_gid_t
#define __NEED_size_t
#include <bits/alltypes.h>

struct passwd {
	char *pw_name;
	char *pw_passwd;
	uid_t pw_uid;
	gid_t pw_gid;
	char *pw_gecos;
	char *pw_dir;
	char *pw_shell;
};

__BEGIN_DECLS
struct passwd *getpwnam(const char *);
int            getpwnam_r(const char *, struct passwd *, char *, size_t, struct passwd **);
struct passwd *getpwuid(uid_t);
int            getpwuid_r(uid_t, struct passwd *, char *, size_t, struct passwd **);
#if __XSI_VISIBLE
void           endpwent(void);
struct passwd *getpwent(void);
void           setpwent(void);
#endif
__END_DECLS
#endif
