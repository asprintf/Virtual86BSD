/*[SUSv4]*/
#include <stropts.h>
void foo(struct strbuf* bar)
{
	int *qux = &bar->len;
	(void) qux;
}
int main(void) { return 0; }
