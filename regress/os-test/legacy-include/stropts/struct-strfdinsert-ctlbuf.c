/*[SUSv4]*/
#include <stropts.h>
void foo(struct strfdinsert* bar)
{
	struct strbuf *qux = &bar->ctlbuf;
	(void) qux;
}
int main(void) { return 0; }
