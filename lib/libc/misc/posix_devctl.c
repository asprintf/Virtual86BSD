/* posix_devctl on ioctl: dcmd is the ioctl request and dev_data_ptr its
 * argument; the ioctl's non-negative result goes to *dev_info_ptr.  When
 * the request encodes a direction and a size (_IOC), nbyte must cover
 * that size (EINVAL otherwise); nbyte 0 (obsolescent) means "as the
 * command needs". */
#include <devctl.h>
#include <errno.h>
#include "syscall.h"

int posix_devctl(int fd, int cmd, void *__RESTRICT data, size_t nbyte, int *__RESTRICT info)
{
	unsigned c = (unsigned)cmd, dir = c >> 30, size = c >> 16 & 0x3fff;
	long r;
	if (data && nbyte && dir && size > nbyte)
		return EINVAL;
	r = __syscall3(SYS_ioctl, fd, c, data);
	if (r < 0)
		return (int)-r;
	if (info)
		*info = (int)r;
	return 0;
}
