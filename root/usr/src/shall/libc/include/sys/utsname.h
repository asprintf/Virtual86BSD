#ifndef _SYS_UTSNAME_H
#define _SYS_UTSNAME_H
#include <bits/features.h>

/* The kernel's layout: six fields of 65 bytes. */
struct utsname {
	char sysname[65];
	char nodename[65];
	char release[65];
	char version[65];
	char machine[65];
	char __domainname[65];
};

__BEGIN_DECLS
int uname(struct utsname *);
__END_DECLS
#endif
