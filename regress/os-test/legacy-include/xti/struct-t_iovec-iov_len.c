/*[SUSv2]*/
#include <xti.h>
void foo(struct t_iovec* bar)
{
	size_t *qux = &bar->iov_len;
	(void) qux;
}
int main(void) { return 0; }
