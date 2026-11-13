/*[SUSv2]*/
#include <libgen.h>
#ifdef regcmp
#undef regcmp
#endif
char *(*foo)(const char *, ...) = regcmp;
int main(void) { return 0; }
