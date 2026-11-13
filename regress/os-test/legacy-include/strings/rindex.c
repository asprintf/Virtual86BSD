/*[SUSv3]*/
#include <strings.h>
#ifdef rindex
#undef rindex
#endif
char *(*foo)(const char *, int) = rindex;
int main(void) { return 0; }
