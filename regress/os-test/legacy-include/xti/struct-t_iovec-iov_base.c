/*[SUSv2]*/
#include <xti.h>
void foo(struct t_iovec* bar)
{
	void **qux = &bar->iov_base;
	(void) qux;
}
int main(void) { return 0; }
