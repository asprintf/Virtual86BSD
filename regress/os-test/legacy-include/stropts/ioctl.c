/*[SUSv4]*/
#include <stropts.h>
#ifdef ioctl
#undef ioctl
#endif
int (*foo)(int, unsigned long, ...) = ioctl;
int main(void) { return 0; }
