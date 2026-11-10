/*
 * Copyright (c) 2026, Joost Orchidee Winter
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this
 *    list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the copyright holder nor the names of its
 *    contributors may be used to endorse or promote products derived from
 *    this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

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
