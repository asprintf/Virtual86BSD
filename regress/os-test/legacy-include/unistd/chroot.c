/*[SUSv2]*/
#include <unistd.h>
#ifdef chroot
#undef chroot
#endif
int (*foo)(const char *) = chroot;
int main(void) { return 0; }
