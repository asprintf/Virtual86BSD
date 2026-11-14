/*[SUSv4]*/
#include <stropts.h>
void foo(struct str_list* bar)
{
	struct str_mlist **qux = &bar->sl_modlist;
	(void) qux;
}
int main(void) { return 0; }
