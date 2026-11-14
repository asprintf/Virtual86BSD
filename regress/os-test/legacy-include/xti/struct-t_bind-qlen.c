/*[SUSv2]*/
#include <xti.h>
void foo(struct t_bind* bar)
{
	unsigned int *qux = &bar->qlen;
	(void) qux;
}
int main(void) { return 0; }
