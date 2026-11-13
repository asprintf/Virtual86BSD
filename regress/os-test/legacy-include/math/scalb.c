/*[SUSv3]*/
#include <math.h>
#ifdef scalb
#undef scalb
#endif
double (*foo)(double, double) = scalb;
int main(void) { return 0; }
