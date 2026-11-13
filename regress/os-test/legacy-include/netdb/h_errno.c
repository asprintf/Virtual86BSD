/*[SUSv3]*/
#include <netdb.h>
void foo(void)
{
	int *qux = &h_errno;
	h_errno = *qux;
}
int main(void) { return 0; }
