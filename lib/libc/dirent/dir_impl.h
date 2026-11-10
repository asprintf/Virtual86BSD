/* Internal: directory streams. */
#ifndef _INTERNAL_DIR_H
#define _INTERNAL_DIR_H
#include <dirent.h>

struct __dirstream {
	int fd;
	volatile int lock;
	size_t pos, end;            /* bytes of buf used / filled */
	off_t tell;                 /* offset after the last entry returned */
	_Alignas(8) char buf[2048];
};

#endif
