/*[SUSv3]*/
/* Test whether makecontext and swapcontext switch between contexts with arguments. */

#include <stdlib.h>
#include <ucontext.h>

#include "../basic.h"

static ucontext_t main_ctx, f_ctx;
static volatile long got[8];
static volatile int steps;

static void f(long a, long b, long c, long d, long e, long g, long h, long i)
{
	got[0] = a; got[1] = b; got[2] = c; got[3] = d;
	got[4] = e; got[5] = g; got[6] = h; got[7] = i;
	steps++;
	swapcontext(&f_ctx, &main_ctx);
	steps++;
	/* returns: uc_link, main_ctx, resumes */
}

int main(void)
{
	char* stack = malloc(65536);
	if ( !stack )
		err(1, "malloc");
	if ( getcontext(&f_ctx) != 0 )
		err(1, "getcontext");
	f_ctx.uc_stack.ss_sp = stack;
	f_ctx.uc_stack.ss_size = 65536;
	f_ctx.uc_link = &main_ctx;
	makecontext(&f_ctx, (void (*)(void)) f, 8, 1L, 2L, 3L, 4L, 5L, 6L, 7L, 8L);
	if ( swapcontext(&main_ctx, &f_ctx) != 0 )
		err(1, "swapcontext");
	if ( steps != 1 )
		errx(1, "f did not run once before it swapped back");
	for ( int i = 0; i < 8; i++ )
		if ( got[i] != i + 1 )
			errx(1, "f's argument %d was %ld", i + 1, got[i]);
	if ( swapcontext(&main_ctx, &f_ctx) != 0 )
		err(1, "swapcontext");
	if ( steps != 2 )
		errx(1, "f did not go on and return to uc_link");
	return 0;
}
