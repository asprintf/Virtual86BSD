/*[SUSv4]*/
#include <stropts.h>
void foo(struct strrecvfd* bar)
{
	uid_t *qux = &bar->uid;
	(void) qux;
}
int main(void) { return 0; }
