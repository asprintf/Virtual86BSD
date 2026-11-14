/*[SUSv2]*/
#include <xti.h>
void foo(struct t_uderr* bar)
{
	struct netbuf *qux = &bar->opt;
	(void) qux;
}
int main(void) { return 0; }
