/*[SUSv4]*/
#include <stropts.h>
void foo(struct strrecvfd* bar)
{
	gid_t *qux = &bar->gid;
	(void) qux;
}
int main(void) { return 0; }
