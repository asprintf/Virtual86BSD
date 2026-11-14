/*[SUSv2]*/
#include <xti.h>
void foo(struct transdel* bar)
{
	struct reqvalue *qux = &bar->avgdel;
	(void) qux;
}
int main(void) { return 0; }
