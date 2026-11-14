/*[SUSv4]*/
#include <stropts.h>
#ifdef getmsg
#undef getmsg
#endif
int (*foo)(int, struct strbuf *restrict, struct strbuf *restrict, int *restrict) = getmsg;
int main(void) { return 0; }
