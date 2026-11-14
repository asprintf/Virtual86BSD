/*[SUSv4]*/
#include <stropts.h>
void foo(struct strpeek* bar)
{
	struct strbuf *qux = &bar->databuf;
	(void) qux;
}
int main(void) { return 0; }
