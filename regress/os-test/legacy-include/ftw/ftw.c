/*[SUSv4]*/
#include <ftw.h>
#ifdef ftw
#undef ftw
#endif
int (*foo)(const char *, int (*)(const char *, const struct stat *, int), int) = ftw;
int main(void) { return 0; }
