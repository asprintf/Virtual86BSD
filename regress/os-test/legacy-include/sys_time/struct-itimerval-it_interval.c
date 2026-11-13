/*[SUSv4]*/
#include <sys/time.h>
void foo(struct itimerval* bar)
{
	struct timeval *qux = &bar->it_interval;
	(void) qux;
}
int main(void) { return 0; }
