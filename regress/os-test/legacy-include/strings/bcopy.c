/*[SUSv3]*/
#include <strings.h>
#ifdef bcopy
#undef bcopy
#endif
void (*foo)(const void *, void *, size_t) = bcopy;
int main(void) { return 0; }
