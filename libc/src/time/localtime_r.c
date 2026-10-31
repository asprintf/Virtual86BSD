#include <time.h>
#include "time_impl.h"

struct tm *localtime_r(const time_t *__RESTRICT t, struct tm *__RESTRICT tm) { return __localtime_r(t, tm); }
struct tm *gmtime_r(const time_t *__RESTRICT t, struct tm *__RESTRICT tm) { return __gmtime_r(t, tm); }
