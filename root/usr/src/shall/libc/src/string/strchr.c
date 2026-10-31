#include <string.h>
char *__strchrnul(const char *s, int c)
{
	char ch = (char)c;
	for (; *s && *s != ch; s++);
	return (char *)s;
}
char *strchr(const char *s, int c)
{
	char *r = __strchrnul(s, c);
	return *r == (char)c ? r : 0;
}
