/*[SUSv3]*/
#include <strings.h>
#ifdef bcmp
#undef bcmp
#endif
int (*foo)(const void *, const void *, size_t) = bcmp;
int main(void) { return 0; }
