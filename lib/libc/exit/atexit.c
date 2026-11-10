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
