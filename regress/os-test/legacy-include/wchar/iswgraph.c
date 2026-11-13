/*[SUSv4]*/
#include <wchar.h>
#ifdef iswgraph
#undef iswgraph
#endif
int (*foo)(wint_t) = iswgraph;
int main(void) { return 0; }
