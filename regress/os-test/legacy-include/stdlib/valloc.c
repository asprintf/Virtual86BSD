/*[SUSv2]*/
#include <stdlib.h>
#ifdef valloc
#undef valloc
#endif
void *(*foo)(size_t) = valloc;
int main(void) { return 0; }
