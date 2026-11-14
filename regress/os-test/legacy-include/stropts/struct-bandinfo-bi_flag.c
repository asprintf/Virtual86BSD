/*[SUSv4]*/
#include <stropts.h>
void foo(struct bandinfo* bar)
{
	int *qux = &bar->bi_flag;
	(void) qux;
}
int main(void) { return 0; }
