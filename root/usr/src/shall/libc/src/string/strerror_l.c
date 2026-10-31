#include <string.h>
#include <errno.h>

const char *__strerror_msg(int);
char *__strerror_unknown(int, char *);

char *strerror_l(int e, locale_t loc)
{
	(void)loc;
	return strerror(e);
}
