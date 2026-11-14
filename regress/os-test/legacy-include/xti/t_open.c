/*[SUSv2]*/
#include <xti.h>
#ifdef t_open
#undef t_open
#endif
int (*foo)(const char *, int, struct t_info *) = t_open;
int main(void) { return 0; }
