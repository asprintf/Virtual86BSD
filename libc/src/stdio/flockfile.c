#include "stdio_impl.h"
int __trylockfile(FILE *);
void flockfile(FILE *f) { __lockfile(f); }
void funlockfile(FILE *f) { __unlockfile(f); }
int ftrylockfile(FILE *f) { return __trylockfile(f); }
