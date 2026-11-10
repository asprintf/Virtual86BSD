/* pthread_atfork: the handlers, which fork() runs through __atfork_hook
 * (prepare handlers in the reverse order of registration). */
#include <errno.h>
#include <pthread.h>
#include <stdlib.h>
#include "pthread_impl.h"
#include "lock.h"

struct handlers { void (*prepare)(void), (*parent)(void), (*child)(void); };

static struct handlers *list;
static size_t n, cap;
static volatile int lock;

extern void (*__atfork_hook)(int);

static void run(int who)
{
	size_t i;
	if (who < 0) {
		LOCK(&lock);
		for (i = n; i-- > 0;)
			if (list[i].prepare)
				list[i].prepare();
		return;
	}
	if (who)
		lock = 0;      /* the child: a fresh lock */
	for (i = 0; i < n; i++) {
		void (*f)(void) = who ? list[i].child : list[i].parent;
		if (f)
			f();
	}
	if (!who)
		UNLOCK(&lock);
}

int pthread_atfork(void (*prepare)(void), void (*parent)(void), void (*child)(void))
{
	LOCK(&lock);
	if (n == cap) {
		size_t c = cap ? 2 * cap : 8;
		struct handlers *l = realloc(list, c * sizeof *l);
		if (!l) {
			UNLOCK(&lock);
			return ENOMEM;
		}
		list = l;
		cap = c;
	}
	list[n].prepare = prepare;
	list[n].parent = parent;
	list[n].child = child;
	n++;
	__atfork_hook = run;
	UNLOCK(&lock);
	return 0;
}
