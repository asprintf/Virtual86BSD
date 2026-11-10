/* <sys/sysmacros.h> -- not POSIX: device number encoding (Linux). */
#ifndef _SYS_SYSMACROS_H
#define _SYS_SYSMACROS_H
#define major(d) ((unsigned)((((d) >> 31 >> 1) & 0xfffff000) | (((d) >> 8) & 0x00000fff)))
#define minor(d) ((unsigned)((((d) >> 12) & 0xffffff00) | ((d) & 0x000000ff)))
#define makedev(x, y) ( \
	(((x) & 0xfffff000ULL) << 32) | (((x) & 0x00000fffULL) << 8) | \
	(((y) & 0xffffff00ULL) << 12) | (((y) & 0x000000ffULL)))
#endif
