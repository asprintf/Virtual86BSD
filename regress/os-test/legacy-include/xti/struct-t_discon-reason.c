/*[SUSv2]*/
#include <xti.h>
void foo(struct t_discon* bar)
{
	int *qux = &bar->reason;
	(void) qux;
}
int main(void) { return 0; }
