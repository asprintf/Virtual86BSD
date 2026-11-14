/*[SUSv2]*/
#include <xti.h>
void foo(struct netbuf* bar)
{
	void **qux = &bar->buf;
	(void) qux;
}
int main(void) { return 0; }
