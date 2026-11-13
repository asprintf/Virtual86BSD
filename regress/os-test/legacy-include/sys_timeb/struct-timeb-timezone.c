/*[SUSv3]*/
#include <sys/timeb.h>
void foo(struct timeb* bar)
{
	short *qux = &bar->timezone;
	(void) qux;
}
int main(void) { return 0; }
