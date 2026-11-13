/*[SUSv4]*/
#include <unistd.h>
#ifdef setpgrp
#undef setpgrp
#endif
pid_t (*foo)(void) = setpgrp;
int main(void) { return 0; }
