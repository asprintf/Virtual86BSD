/*[SUSv4]*/
#include <wchar.h>
#ifdef iswpunct
#undef iswpunct
#endif
int (*foo)(wint_t) = iswpunct;
int main(void) { return 0; }
