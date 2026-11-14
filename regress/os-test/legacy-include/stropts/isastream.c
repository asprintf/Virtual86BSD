/*[SUSv4]*/
#include <stropts.h>
#ifdef isastream
#undef isastream
#endif
int (*foo)(int) = isastream;
int main(void) { return 0; }
