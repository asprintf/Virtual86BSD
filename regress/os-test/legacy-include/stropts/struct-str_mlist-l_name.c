/*[SUSv4]*/
#include <stropts.h>
void foo(struct str_mlist* bar)
{
	char *qux = bar->l_name;
	(void) qux;
}
int main(void) { return 0; }
