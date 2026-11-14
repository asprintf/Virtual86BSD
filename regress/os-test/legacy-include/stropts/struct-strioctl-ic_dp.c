/*[SUSv4]*/
#include <stropts.h>
void foo(struct strioctl* bar)
{
	char **qux = &bar->ic_dp;
	(void) qux;
}
int main(void) { return 0; }
