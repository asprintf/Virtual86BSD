/*[SUSv3]*/
#include <ucontext.h>
void foo(ucontext_t* bar)
{
	stack_t *qux = &bar->uc_stack;
	(void) qux;
}
int main(void) { return 0; }
