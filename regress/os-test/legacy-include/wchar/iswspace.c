/*[SUSv4]*/
#include <wchar.h>
#ifdef iswspace
#undef iswspace
#endif
int (*foo)(wint_t) = iswspace;
int main(void) { return 0; }
