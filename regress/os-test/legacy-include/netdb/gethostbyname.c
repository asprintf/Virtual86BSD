/*[SUSv3]*/
#include <netdb.h>
#ifdef gethostbyname
#undef gethostbyname
#endif
struct hostent *(*foo)(const char *) = gethostbyname;
int main(void) { return 0; }
