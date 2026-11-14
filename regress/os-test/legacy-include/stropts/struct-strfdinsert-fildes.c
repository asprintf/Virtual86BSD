/*[SUSv4]*/
#include <stropts.h>
void foo(struct strfdinsert* bar)
{
	int *qux = &bar->fildes;
	(void) qux;
}
int main(void) { return 0; }
