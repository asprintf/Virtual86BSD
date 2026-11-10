#ifndef _DEVCTL_H
#define _DEVCTL_H
#include <bits/features.h>
#define __NEED_size_t
#include <bits/alltypes.h>

__BEGIN_DECLS
int posix_devctl(int, int, void *__RESTRICT, size_t, int *__RESTRICT);
__END_DECLS

#endif
