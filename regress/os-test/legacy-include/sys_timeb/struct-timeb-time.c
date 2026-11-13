/*[SUSv3]*/
#include <sys/timeb.h>
void foo(struct timeb* bar)
{
	time_t *qux = &bar->time;
	(void) qux;
}
int main(void) { return 0; }
