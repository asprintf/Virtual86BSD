/*[SUSv2]*/
#include <libgen.h>
#ifdef regex
#undef regex
#endif
char *(*foo)(const char *, const char *, ...) = regex;
int main(void) { return 0; }
