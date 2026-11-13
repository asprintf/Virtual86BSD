/*[SUSv2]*/
#include <unistd.h>
#ifdef getpass
#undef getpass
#endif
char *(*foo)(const char *) = getpass;
int main(void) { return 0; }
