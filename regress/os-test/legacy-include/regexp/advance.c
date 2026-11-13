/*[SUSv2]*/
#define INIT char *sp = instring;
#define GETC() (*sp++)
#define PEEKC() (*sp)
#define UNGETC(c) ((void) (c), (void) --sp)
#define RETURN(c) return (c);
#define ERROR(c) return (void) (c), (char *) 0;
#include <regexp.h>
#ifdef advance
#undef advance
#endif
int (*foo)(const char *, const char *) = advance;
int main(void) { return 0; }
