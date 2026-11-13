/*[SUSv2]*/
#include <signal.h>
void foo(struct sigstack* bar)
{
	void **qux = &bar->ss_sp;
	(void) qux;
}
int main(void) { return 0; }
