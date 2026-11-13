/*[SUSv3]*/
#include <unistd.h>
#ifdef vfork
#undef vfork
#endif
pid_t (*foo)(void) = vfork;
int main(void) { return 0; }
