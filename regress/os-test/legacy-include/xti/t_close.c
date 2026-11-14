/*[SUSv2]*/
#include <xti.h>
#ifdef t_close
#undef t_close
#endif
int (*foo)(int) = t_close;
int main(void) { return 0; }
