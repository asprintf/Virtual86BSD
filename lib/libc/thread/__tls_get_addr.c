/* __tls_get_addr: the address of a thread-local variable of module v[0]
 * at offset v[1] (the general-dynamic TLS model of shared objects). */
#include "pthread_impl.h"
#include "libc.h"

void *__tls_get_addr(unsigned long *v)
{
	unsigned long *dtv = __pthread_self()->dtv;
	if (v[0] <= dtv[0] && dtv[v[0]])
		return (char *)dtv[v[0]] + v[1];
	return __libc.tls_get_new(v);   /* a dlopen'ed module, first use */
}
