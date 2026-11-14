/*[SUSv2]*/
#include <xti.h>
#ifdef t_strerror
#undef t_strerror
#endif
const char *(*foo)(int) = t_strerror;
int main(void) { return 0; }
