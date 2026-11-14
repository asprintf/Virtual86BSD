/*[SUSv2]*/
#include <xti.h>
void foo(struct netbuf* bar)
{
	unsigned int *qux = &bar->len;
	(void) qux;
}
int main(void) { return 0; }
