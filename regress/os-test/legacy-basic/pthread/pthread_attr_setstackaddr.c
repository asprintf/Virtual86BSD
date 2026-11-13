/*[SUSv3]*/
/* Test whether a thread runs on the stack given by pthread_attr_setstackaddr. */

#include <pthread.h>
#include <stdint.h>
#include <stdlib.h>
#include <sys/mman.h>

#include "../basic.h"

#define SIZE (256 * 1024)

/* which end stackaddr is, is not given: it is the middle of 2 * SIZE */
static void* thread(void* arg)
{
	char here;
	char* base = arg;
	return (void*)(uintptr_t)(&here >= base && &here < base + 2 * SIZE);
}

int main(void)
{
	char* stack = mmap(NULL, 2 * SIZE, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	if ( stack == MAP_FAILED )
		err(1, "mmap");
	pthread_attr_t attr;
	pthread_attr_init(&attr);
	pthread_attr_setstacksize(&attr, SIZE);
	if ( pthread_attr_setstackaddr(&attr, stack + SIZE) != 0 )
		errx(1, "pthread_attr_setstackaddr failed");
	void* got;
	if ( pthread_attr_getstackaddr(&attr, &got) != 0 || got != stack + SIZE )
		errx(1, "pthread_attr_getstackaddr did not give the address set");
	pthread_t t;
	void* in;
	if ( pthread_create(&t, &attr, thread, stack) != 0 )
		errx(1, "pthread_create failed");
	pthread_join(t, &in);
	if ( !in )
		errx(1, "the thread did not run on the stack given");
	return 0;
}
