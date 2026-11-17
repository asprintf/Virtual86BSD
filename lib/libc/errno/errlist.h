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

/* errno number, symbol, message (FreeBSD's numbers and messages; the four
 * STREAMS errors above ELAST are never returned by the kernel). */
E(1, EPERM, "Operation not permitted")
E(2, ENOENT, "No such file or directory")
E(3, ESRCH, "No such process")
E(4, EINTR, "Interrupted system call")
E(5, EIO, "Input/output error")
E(6, ENXIO, "Device not configured")
E(7, E2BIG, "Argument list too long")
E(8, ENOEXEC, "Exec format error")
E(9, EBADF, "Bad file descriptor")
E(10, ECHILD, "No child processes")
E(11, EDEADLK, "Resource deadlock avoided")
E(12, ENOMEM, "Cannot allocate memory")
E(13, EACCES, "Permission denied")
E(14, EFAULT, "Bad address")
E(15, ENOTBLK, "Block device required")
E(16, EBUSY, "Device busy")
E(17, EEXIST, "File exists")
E(18, EXDEV, "Cross-device link")
E(19, ENODEV, "Operation not supported by device")
E(20, ENOTDIR, "Not a directory")
E(21, EISDIR, "Is a directory")
E(22, EINVAL, "Invalid argument")
E(23, ENFILE, "Too many open files in system")
E(24, EMFILE, "Too many open files")
E(25, ENOTTY, "Inappropriate ioctl for device")
E(26, ETXTBSY, "Text file busy")
E(27, EFBIG, "File too large")
E(28, ENOSPC, "No space left on device")
E(29, ESPIPE, "Illegal seek")
E(30, EROFS, "Read-only file system")
E(31, EMLINK, "Too many links")
E(32, EPIPE, "Broken pipe")
E(33, EDOM, "Numerical argument out of domain")
E(34, ERANGE, "Result too large")
E(35, EAGAIN, "Resource temporarily unavailable")
E(36, EINPROGRESS, "Operation now in progress")
E(37, EALREADY, "Operation already in progress")
E(38, ENOTSOCK, "Socket operation on non-socket")
E(39, EDESTADDRREQ, "Destination address required")
E(40, EMSGSIZE, "Message too long")
E(41, EPROTOTYPE, "Protocol wrong type for socket")
E(42, ENOPROTOOPT, "Protocol not available")
E(43, EPROTONOSUPPORT, "Protocol not supported")
E(44, ESOCKTNOSUPPORT, "Socket type not supported")
E(45, EOPNOTSUPP, "Operation not supported")
E(46, EPFNOSUPPORT, "Protocol family not supported")
E(47, EAFNOSUPPORT, "Address family not supported by protocol family")
E(48, EADDRINUSE, "Address already in use")
E(49, EADDRNOTAVAIL, "Can't assign requested address")
E(50, ENETDOWN, "Network is down")
E(51, ENETUNREACH, "Network is unreachable")
E(52, ENETRESET, "Network dropped connection on reset")
E(53, ECONNABORTED, "Software caused connection abort")
E(54, ECONNRESET, "Connection reset by peer")
E(55, ENOBUFS, "No buffer space available")
E(56, EISCONN, "Socket is already connected")
E(57, ENOTCONN, "Socket is not connected")
E(58, ESHUTDOWN, "Can't send after socket shutdown")
E(59, ETOOMANYREFS, "Too many references: can't splice")
E(60, ETIMEDOUT, "Operation timed out")
E(61, ECONNREFUSED, "Connection refused")
E(62, ELOOP, "Too many levels of symbolic links")
E(63, ENAMETOOLONG, "File name too long")
E(64, EHOSTDOWN, "Host is down")
E(65, EHOSTUNREACH, "No route to host")
E(66, ENOTEMPTY, "Directory not empty")
E(67, EPROCLIM, "Too many processes")
E(68, EUSERS, "Too many users")
E(69, EDQUOT, "Disc quota exceeded")
E(70, ESTALE, "Stale NFS file handle")
E(71, EREMOTE, "Too many levels of remote in path")
E(72, EBADRPC, "RPC struct is bad")
E(73, ERPCMISMATCH, "RPC version wrong")
E(74, EPROGUNAVAIL, "RPC prog. not avail")
E(75, EPROGMISMATCH, "Program version wrong")
E(76, EPROCUNAVAIL, "Bad procedure for program")
E(77, ENOLCK, "No locks available")
E(78, ENOSYS, "Function not implemented")
E(79, EFTYPE, "Inappropriate file type or format")
E(80, EAUTH, "Authentication error")
E(81, ENEEDAUTH, "Need authenticator")
E(82, EIDRM, "Identifier removed")
E(83, ENOMSG, "No message of desired type")
E(84, EOVERFLOW, "Value too large to be stored in data type")
E(85, ECANCELED, "Operation canceled")
E(86, EILSEQ, "Illegal byte sequence")
E(87, ENOATTR, "Attribute not found")
E(88, EDOOFUS, "Programming error")
E(89, EBADMSG, "Bad message")
E(90, EMULTIHOP, "Multihop attempted")
E(91, ENOLINK, "Link has been severed")
E(92, EPROTO, "Protocol error")
E(93, ENOTCAPABLE, "Capabilities insufficient")
E(94, ECAPMODE, "Not permitted in capability mode")
E(95, ENOTRECOVERABLE, "State not recoverable")
E(96, EOWNERDEAD, "Previous owner died")
E(97, EINTEGRITY, "Integrity check failed")
E(98, ENODATA, "No data available")
E(99, ENOSR, "Out of streams resources")
E(100, ENOSTR, "Device not a stream")
E(101, ETIME, "Timer expired")
