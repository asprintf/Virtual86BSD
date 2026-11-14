/*[SUSv2]*/
#include <xti.h>
void foo(struct thrpt* bar)
{
	struct reqvalue *qux = &bar->avgthrpt;
	(void) qux;
}
int main(void) { return 0; }
