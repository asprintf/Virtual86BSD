/*[SUSv3]*/
#include <unistd.h>
#ifdef usleep
#undef usleep
#endif
int (*foo)(useconds_t) = usleep;
int main(void) { return 0; }
