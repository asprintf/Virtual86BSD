/*[SUSv4]*/
#include <wchar.h>
#ifdef iswxdigit
#undef iswxdigit
#endif
int (*foo)(wint_t) = iswxdigit;
int main(void) { return 0; }
