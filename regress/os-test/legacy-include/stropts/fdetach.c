/*[SUSv4]*/
#include <stropts.h>
#ifdef fdetach
#undef fdetach
#endif
int (*foo)(const char *) = fdetach;
int main(void) { return 0; }
