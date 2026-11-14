/*[SUSv2]*/
#include <xti.h>
void foo(struct t_info* bar)
{
	t_scalar_t *qux = &bar->flags;
	(void) qux;
}
int main(void) { return 0; }
