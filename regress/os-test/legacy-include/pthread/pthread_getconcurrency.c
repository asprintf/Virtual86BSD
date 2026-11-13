/*[SUSv4]*/
#include <pthread.h>
#ifdef pthread_getconcurrency
#undef pthread_getconcurrency
#endif
int (*foo)(void) = pthread_getconcurrency;
int main(void) { return 0; }
