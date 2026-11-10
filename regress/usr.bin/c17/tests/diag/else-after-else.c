/* expect error: #else without #if */
#if 1
#else
#else
#endif
int x;
