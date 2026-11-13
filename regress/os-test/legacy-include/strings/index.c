/*[SUSv3]*/
#include <strings.h>
#ifdef index
#undef index
#endif
char *(*foo)(const char *, int) = index;
int main(void) { return 0; }
