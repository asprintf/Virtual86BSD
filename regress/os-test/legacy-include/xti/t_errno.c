/*[SUSv2]*/
#include <xti.h>
void foo(void)
{
	int *qux = &t_errno;
	t_errno = *qux;
}
int main(void) { return 0; }
