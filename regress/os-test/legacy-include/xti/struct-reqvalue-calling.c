/*[SUSv2]*/
#include <xti.h>
void foo(struct reqvalue* bar)
{
	struct rate *qux = &bar->calling;
	(void) qux;
}
int main(void) { return 0; }
