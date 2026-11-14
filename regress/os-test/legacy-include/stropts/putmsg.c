/*[SUSv4]*/
#include <stropts.h>
#ifdef putmsg
#undef putmsg
#endif
int (*foo)(int, const struct strbuf *, const struct strbuf *, int) = putmsg;
int main(void) { return 0; }
