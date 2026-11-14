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

/* t_errno, t_error, t_strerror (XTI, SUSv2) */
#define _DEFAULT_SOURCE
#include <xti.h>
#include <errno.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static pthread_key_t key;
static pthread_once_t once = PTHREAD_ONCE_INIT;
static int fallback;

static void mkkey(void)
{
	pthread_key_create(&key, free);
}

int *_t_errno(void)
{
	int *p;
	pthread_once(&once, mkkey);
	p = pthread_getspecific(key);
	if (!p) {
		p = calloc(1, sizeof *p);
		if (!p) return &fallback;
		pthread_setspecific(key, p);
	}
	return p;
}

static const char *const msgs[] = {
	0,
	"incorrect addr format", "incorrect option format", "incorrect permissions",
	"illegal transport fd", "couldn't allocate addr", "out of state",
	"bad call sequence number", "system error", "event requires attention",
	"illegal amount of data", "buffer not large enough", "flow control",
	"no data", "discon_ind not found on queue", "unitdata error not found",
	"bad flags", "no ord rel found on queue", "primitive/action not supported",
	"state is in process of changing", "unsupported struct-type requested",
	"invalid transport provider name", "qlen is zero", "address in use",
	"outstanding connection indications", "transport provider mismatch",
	"resfd specified to accept w/qlen >0", "resfd not bound to same addr as fd",
	"incoming connection queue full", "XTI protocol error",
};

const char *t_strerror(int err)
{
	static char buf[48];
	if (err > 0 && err < (int)(sizeof msgs / sizeof *msgs)) return msgs[err];
	snprintf(buf, sizeof buf, "%d: error unknown", err);
	return buf;
}

int t_error(const char *msg)
{
	int e = errno, te = t_errno;
	if (msg && *msg) fprintf(stderr, "%s: ", msg);
	fputs(t_strerror(te), stderr);
	if (te == TSYSERR) fprintf(stderr, ": %s", strerror(e));
	fputc('\n', stderr);
	return 0;
}
