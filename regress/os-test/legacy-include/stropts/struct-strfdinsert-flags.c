/*[SUSv4]*/
#include <stropts.h>
void foo(struct strfdinsert* bar)
{
	t_uscalar_t *qux = &bar->flags;
	(void) qux;
}
int main(void) { return 0; }
