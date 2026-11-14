/*[SUSv2]*/
#include <xti.h>
void foo(struct t_opthdr* bar)
{
	t_uscalar_t *qux = &bar->len;
	(void) qux;
}
int main(void) { return 0; }
