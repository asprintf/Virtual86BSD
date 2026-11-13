/*[SUSv4]*/
#include <pthread.h>
#ifdef pthread_setconcurrency
#undef pthread_setconcurrency
#endif
int (*foo)(int) = pthread_setconcurrency;
int main(void) { return 0; }
