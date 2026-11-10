/* Internal: file type and mode bits (<sys/stat.h>, <fcntl.h>, <spawn.h>). */
#ifndef _BITS_STAT_MODE_H
#define _BITS_STAT_MODE_H
#define S_IFMT   0170000
#define S_IFSOCK 0140000
#define S_IFLNK  0120000
#define S_IFREG  0100000
#define S_IFBLK  0060000
#define S_IFDIR  0040000
#define S_IFCHR  0020000
#define S_IFIFO  0010000
#define S_ISUID  04000
#define S_ISGID  02000
#define S_ISVTX  01000
#define S_IRWXU  0700
#define S_IRUSR  0400
#define S_IWUSR  0200
#define S_IXUSR  0100
#define S_IRWXG  070
#define S_IRGRP  040
#define S_IWGRP  020
#define S_IXGRP  010
#define S_IRWXO  07
#define S_IROTH  04
#define S_IWOTH  02
#define S_IXOTH  01
#endif
