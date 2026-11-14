/*[SUSv4]*/
#include <stropts.h>
void foo(struct strrecvfd* bar)
{
	int *qux = &bar->fd;
	(void) qux;
}
int main(void) { return 0; }
