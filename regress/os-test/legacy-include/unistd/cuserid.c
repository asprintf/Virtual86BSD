/*[SUSv2]*/
#include <unistd.h>
#ifdef cuserid
#undef cuserid
#endif
char *(*foo)(char *) = cuserid;
int main(void) { return 0; }
