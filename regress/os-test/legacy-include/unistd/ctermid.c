/*[SUSv2]*/
#include <unistd.h>
#ifdef ctermid
#undef ctermid
#endif
char *(*foo)(char *) = ctermid;
int main(void) { return 0; }
