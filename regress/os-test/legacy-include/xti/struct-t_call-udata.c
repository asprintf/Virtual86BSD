/*[SUSv2]*/
#include <xti.h>
void foo(struct t_call* bar)
{
	struct netbuf *qux = &bar->udata;
	(void) qux;
}
int main(void) { return 0; }
