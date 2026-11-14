/*[SUSv4]*/
#include <stropts.h>
void foo(struct str_list* bar)
{
	int *qux = &bar->sl_nmods;
	(void) qux;
}
int main(void) { return 0; }
