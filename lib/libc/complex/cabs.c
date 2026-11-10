#include "complex_impl.h"

long double cabsl(long double _Complex z) { return hypotl(__cre(z), __cim(z)); }
double cabs(double _Complex z) { return hypot(__cre(z), __cim(z)); }
float cabsf(float _Complex z) { return hypotf(__cre(z), __cim(z)); }

long double cargl(long double _Complex z) { return atan2l(__cim(z), __cre(z)); }
double carg(double _Complex z) { return atan2(__cim(z), __cre(z)); }
float cargf(float _Complex z) { return atan2f(__cim(z), __cre(z)); }
