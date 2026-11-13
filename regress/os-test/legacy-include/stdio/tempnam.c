/*[SUSv4]*/
#include <stdio.h>
#ifdef tempnam
#undef tempnam
#endif
char *(*foo)(const char *, const char *) = tempnam;
int main(void) { return 0; }
