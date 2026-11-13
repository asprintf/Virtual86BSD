/*[SUSv2]*/
#define INIT char *sp = instring;
#define GETC() (*sp++)
#define PEEKC() (*sp)
#define UNGETC(c) ((void) (c), (void) --sp)
#define RETURN(c) return (c);
#define ERROR(c) return (void) (c), (char *) 0;
#include <regexp.h>
#ifdef step
#undef step
#endif
int (*foo)(const char *, const char *) = step;
int main(void) { return 0; }
