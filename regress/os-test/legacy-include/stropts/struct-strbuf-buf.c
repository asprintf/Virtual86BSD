/*[SUSv4]*/
#include <stropts.h>
void foo(struct strbuf* bar)
{
	char **qux = &bar->buf;
	(void) qux;
}
int main(void) { return 0; }
