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

/* atexit() and at_quick_exit(): unlimited registrations (ATEXIT_MAX is
 * effectively INT_MAX); the first 32 need no allocation. */
#include <stdlib.h>
#include "libc.h"
#include "lock.h"

#define BLK 32

struct fl {
	struct fl *next;
	void (*f[BLK])(void);
};

struct list {
	struct fl builtin, *head;
	int slot;
	volatile int lock;
};

static struct list on_exit_list, on_quick_list;

static int add(struct list *l, void (*func)(void))
{
	LOCK(&l->lock);
	if (!l->head) l->head = &l->builtin;
	if (l->slot == BLK) {
		struct fl *n = calloc(1, sizeof *n);
		if (!n) { UNLOCK(&l->lock); return -1; }
		n->next = l->head;
		l->head = n;
		l->slot = 0;
	}
	l->head->f[l->slot++] = func;
	UNLOCK(&l->lock);
	return 0;
}

/* Call in reverse order of registration; handlers may register more. */
static void run(struct list *l)
{
	LOCK(&l->lock);
	for (; l->head; l->head = l->head->next, l->slot = BLK) {
		while (l->slot > 0) {
			void (*func)(void) = l->head->f[--l->slot];
			UNLOCK(&l->lock);
			func();
			LOCK(&l->lock);
		}
	}
	UNLOCK(&l->lock);
}

int atexit(void (*func)(void)) { return add(&on_exit_list, func); }
int at_quick_exit(void (*func)(void)) { return add(&on_quick_list, func); }
void __funcs_on_exit(void) { run(&on_exit_list); }

_Noreturn void quick_exit(int status)
{
	run(&on_quick_list);
	_Exit(status);
}
