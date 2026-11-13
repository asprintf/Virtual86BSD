/*[SUSv3]*/
#include <unistd.h>
#ifdef getwd
#undef getwd
#endif
char *(*foo)(char *) = getwd;
int main(void) { return 0; }
