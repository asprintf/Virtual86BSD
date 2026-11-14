/*[SUSv4]*/
#include <stropts.h>
#ifdef getpmsg
#undef getpmsg
#endif
int (*foo)(int, struct strbuf *restrict, struct strbuf *restrict, int *restrict, int *restrict) = getpmsg;
int main(void) { return 0; }
