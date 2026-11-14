/*[SUSv2]*/
#include <xti.h>
void foo(struct rate* bar)
{
	t_scalar_t *qux = &bar->minacceptvalue;
	(void) qux;
}
int main(void) { return 0; }
