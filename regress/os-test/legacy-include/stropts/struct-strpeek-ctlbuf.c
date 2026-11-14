/*[SUSv4]*/
#include <stropts.h>
void foo(struct strpeek* bar)
{
	struct strbuf *qux = &bar->ctlbuf;
	(void) qux;
}
int main(void) { return 0; }
