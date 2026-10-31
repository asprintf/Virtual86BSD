#ifndef _SYS_UIO_H
#define _SYS_UIO_H
#include <bits/features.h>
#define __NEED_size_t
#define __NEED_ssize_t
#define __NEED_struct_iovec
#include <bits/alltypes.h>

__BEGIN_DECLS
ssize_t readv(int, const struct iovec *, int);
ssize_t writev(int, const struct iovec *, int);
__END_DECLS
#endif
