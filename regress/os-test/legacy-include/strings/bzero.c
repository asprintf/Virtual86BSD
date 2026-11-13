/*[SUSv3]*/
#include <strings.h>
#ifdef bzero
#undef bzero
#endif
void (*foo)(void *, size_t) = bzero;
int main(void) { return 0; }
