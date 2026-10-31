#include <dirent.h>
#include <unistd.h>
#include "dir_impl.h"
#include "../internal/lock.h"

void seekdir(DIR *d, long off)
{
	LOCK(&d->lock);
	d->tell = lseek(d->fd, off, SEEK_SET);
	d->pos = d->end = 0;
	UNLOCK(&d->lock);
}

long telldir(DIR *d) { return (long)d->tell; }
