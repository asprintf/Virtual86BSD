#include <string.h>
#define BITOP(a,b,op) ((a)[(unsigned char)(b) / (8 * sizeof *(a))] op \
	((size_t)1 << ((unsigned char)(b) % (8 * sizeof *(a)))))
size_t strspn(const char *s, const char *c)
{
	const char *a = s;
	size_t byteset[32 / sizeof(size_t)] = { 0 };
	if (!c[0]) return 0;
	if (!c[1]) { for (; *s == *c; s++); return (size_t)(s - a); }
	for (; *c; c++) BITOP(byteset, *c, |=);
	for (; *s && BITOP(byteset, *s, &); s++);
	return (size_t)(s - a);
}
size_t strcspn(const char *s, const char *c)
{
	const char *a = s;
	size_t byteset[32 / sizeof(size_t)] = { 0 };
	char *__strchrnul(const char *, int);
	if (!c[0] || !c[1]) return (size_t)(__strchrnul(s, *c) - a);
	for (; *c; c++) BITOP(byteset, *c, |=);
	for (; *s && !BITOP(byteset, *s, &); s++);
	return (size_t)(s - a);
}
char *strpbrk(const char *s, const char *b)
{
	s += strcspn(s, b);
	return *s ? (char *)s : 0;
}
