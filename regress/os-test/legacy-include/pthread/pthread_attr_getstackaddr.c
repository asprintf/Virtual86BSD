/*[SUSv3]*/
#include <pthread.h>
#ifdef pthread_attr_getstackaddr
#undef pthread_attr_getstackaddr
#endif
int (*foo)(const pthread_attr_t *, void **) = pthread_attr_getstackaddr;
int main(void) { return 0; }
