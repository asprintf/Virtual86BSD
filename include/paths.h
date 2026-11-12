/*
 * Copyright (c) 2026, asprintf
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

#ifndef _PATHS_H_
#define _PATHS_H_

/* Names of files of the system, as in BSD <paths.h> (those V86BSD has). */

#define _PATH_DEFPATH  "/usr/bin:/bin:/usr/sbin:/sbin"
#define _PATH_STDPATH  "/usr/bin:/bin:/usr/sbin:/sbin"

#define _PATH_BSHELL   "/bin/sh"
#define _PATH_CONSOLE  "/dev/console"
#define _PATH_DEVNULL  "/dev/null"
#define _PATH_KLOG     "/dev/klog"
#define _PATH_LOGCONF  "/etc/syslog.conf"
#define _PATH_NOLOGIN  "/etc/nologin"
#define _PATH_SHELLS   "/etc/shells"
#define _PATH_TTY      "/dev/tty"
#define _PATH_UNIX     "/bsd"
#define _PATH_VI       "/usr/bin/vi"

/* with a trailing slash, mostly used to build file names */
#define _PATH_DEV      "/dev/"
#define _PATH_TMP      "/tmp/"
#define _PATH_VARRUN   "/var/run/"
#define _PATH_VARTMP   "/var/tmp/"

#endif
