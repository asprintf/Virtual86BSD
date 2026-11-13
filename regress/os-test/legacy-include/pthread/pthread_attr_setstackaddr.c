/*[SUSv3]*/
#include <pthread.h>
#ifdef pthread_attr_setstackaddr
#undef pthread_attr_setstackaddr
#endif
int (*foo)(pthread_attr_t *, void *) = pthread_attr_setstackaddr;
int main(void) { return 0; }
