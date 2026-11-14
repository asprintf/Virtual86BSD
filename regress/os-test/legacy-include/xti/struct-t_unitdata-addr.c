/*[SUSv2]*/
#include <xti.h>
void foo(struct t_unitdata* bar)
{
	struct netbuf *qux = &bar->addr;
	(void) qux;
}
int main(void) { return 0; }
