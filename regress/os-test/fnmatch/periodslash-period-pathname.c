/* Test whether periods and slashes work properly with FNM_PERIOD and
   FNM_PATHNAME. */

#define FLAGS (FNM_PERIOD | FNM_PATHNAME)
#include "periodslash.c"
