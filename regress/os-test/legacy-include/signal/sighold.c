/*[SUSv4]*/
#include <signal.h>
#ifdef sighold
#undef sighold
#endif
int (*foo)(int) = sighold;
int main(void) { return 0; }
