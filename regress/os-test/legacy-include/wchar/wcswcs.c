/*[SUSv3]*/
#include <wchar.h>
#ifdef wcswcs
#undef wcswcs
#endif
wchar_t *(*foo)(const wchar_t *, const wchar_t *) = wcswcs;
int main(void) { return 0; }
