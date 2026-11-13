/*[SUSv2]*/
#include <re_comp.h>
#ifdef re_exec
#undef re_exec
#endif
int (*foo)(const char *) = re_exec;
int main(void) { return 0; }
