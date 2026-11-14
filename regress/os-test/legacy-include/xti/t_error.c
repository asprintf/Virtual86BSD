/*[SUSv2]*/
#include <xti.h>
#ifdef t_error
#undef t_error
#endif
int (*foo)(const char *) = t_error;
int main(void) { return 0; }
