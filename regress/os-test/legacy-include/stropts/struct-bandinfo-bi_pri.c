/*[SUSv4]*/
#include <stropts.h>
void foo(struct bandinfo* bar)
{
	unsigned char *qux = &bar->bi_pri;
	(void) qux;
}
int main(void) { return 0; }
