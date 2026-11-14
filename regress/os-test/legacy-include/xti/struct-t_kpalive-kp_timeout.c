/*[SUSv2]*/
#include <xti.h>
void foo(struct t_kpalive* bar)
{
	t_scalar_t *qux = &bar->kp_timeout;
	(void) qux;
}
int main(void) { return 0; }
