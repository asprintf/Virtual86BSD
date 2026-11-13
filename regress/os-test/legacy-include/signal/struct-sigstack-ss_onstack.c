/*[SUSv2]*/
#include <signal.h>
void foo(struct sigstack* bar)
{
	int *qux = &bar->ss_onstack;
	(void) qux;
}
int main(void) { return 0; }
