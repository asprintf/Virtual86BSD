/*[SUSv4]*/
#include <stropts.h>
void foo(struct strioctl* bar)
{
	int *qux = &bar->ic_cmd;
	(void) qux;
}
int main(void) { return 0; }
