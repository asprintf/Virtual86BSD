/*[SUSv2]*/
#include <sys/wait.h>
#ifdef wait3
#undef wait3
#endif
pid_t (*foo)(int *, int, struct rusage *) = wait3;
int main(void) { return 0; }
