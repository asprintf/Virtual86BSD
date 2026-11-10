#include <signal.h>

int __sigaction(int, const struct sigaction *, struct sigaction *);

int sigaction(int sig, const struct sigaction *__RESTRICT sa, struct sigaction *__RESTRICT old)
{
	return __sigaction(sig, sa, old);
}
