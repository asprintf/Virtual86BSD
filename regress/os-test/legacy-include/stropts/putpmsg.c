/*[SUSv4]*/
#include <stropts.h>
#ifdef putpmsg
#undef putpmsg
#endif
int (*foo)(int, const struct strbuf *, const struct strbuf *, int, int) = putpmsg;
int main(void) { return 0; }
