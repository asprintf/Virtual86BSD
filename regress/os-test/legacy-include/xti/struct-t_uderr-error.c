/*[SUSv2]*/
#include <xti.h>
void foo(struct t_uderr* bar)
{
	t_scalar_t *qux = &bar->error;
	(void) qux;
}
int main(void) { return 0; }
