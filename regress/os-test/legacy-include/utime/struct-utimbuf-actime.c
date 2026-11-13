/*[SUSv4]*/
#include <utime.h>
void foo(struct utimbuf* bar)
{
	time_t *qux = &bar->actime;
	(void) qux;
}
int main(void) { return 0; }
