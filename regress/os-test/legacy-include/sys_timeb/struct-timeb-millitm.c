/*[SUSv3]*/
#include <sys/timeb.h>
void foo(struct timeb* bar)
{
	unsigned short *qux = &bar->millitm;
	(void) qux;
}
int main(void) { return 0; }
