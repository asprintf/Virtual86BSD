/*[SUSv4]*/
#include <stropts.h>
#ifdef fattach
#undef fattach
#endif
int (*foo)(int, const char *) = fattach;
int main(void) { return 0; }
