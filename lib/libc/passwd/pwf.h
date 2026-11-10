/* Internal: parsing of /etc/passwd and /etc/group lines. Only the files
 * are consulted (no name-service switch). */
#ifndef _INTERNAL_PWF_H
#define _INTERNAL_PWF_H
#include <pwd.h>
#include <grp.h>
#include <stdio.h>

#define PASSWD_FILE "/etc/passwd"
#define GROUP_FILE  "/etc/group"

/* Split a line in place. Return 0 on success, -1 for a malformed line.
 * __parse_gr stores up to *nmem member pointers in *mem, growing it with
 * realloc; returns -2 if that fails. */
int __parse_pw(char *line, struct passwd *);
int __parse_gr(char *line, struct group *, char ***mem, size_t *nmem);

/* Search the file: by name if name, else by id. 0 or an error number;
 * *res is null when there is no match. */
int __getpw_r(const char *name, uid_t, struct passwd *, char *, size_t, struct passwd **);
int __getgr_r(const char *name, gid_t, struct group *, char *, size_t, struct group **);

#endif
