/*[SUSv4]*/
#include <wchar.h>
#ifdef iswlower
#undef iswlower
#endif
int (*foo)(wint_t) = iswlower;
int main(void) { return 0; }
