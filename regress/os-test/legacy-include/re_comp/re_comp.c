/*[SUSv2]*/
#include <re_comp.h>
#ifdef re_comp
#undef re_comp
#endif
char *(*foo)(const char *) = re_comp;
int main(void) { return 0; }
