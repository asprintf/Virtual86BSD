/*[SUSv2]*/
#include <xti.h>
void foo(struct t_linger* bar)
{
	t_scalar_t *qux = &bar->l_onoff;
	(void) qux;
}
int main(void) { return 0; }
