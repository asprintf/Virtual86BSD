/*[SUSv2]*/
#include <math.h>
#ifdef gamma
#undef gamma
#endif
double (*foo)(double) = gamma;
int main(void) { return 0; }
