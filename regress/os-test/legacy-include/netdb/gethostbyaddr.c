/*[SUSv3]*/
#include <netdb.h>
#ifdef gethostbyaddr
#undef gethostbyaddr
#endif
struct hostent *(*foo)(const void *, socklen_t, int) = gethostbyaddr;
int main(void) { return 0; }
