/*[SUSv4]*/
#include <stropts.h>
void foo(struct strbuf* bar)
{
	int *qux = &bar->maxlen;
	(void) qux;
}
int main(void) { return 0; }
