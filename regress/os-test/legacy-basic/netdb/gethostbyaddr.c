/*[SUSv3]*/
/* Test whether gethostbyaddr names 127.0.0.1 and copies the address. */

#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <string.h>

#include "../basic.h"

int main(void)
{
	struct in_addr in;
	in.s_addr = htonl(0x7f000001);
	struct hostent* h = gethostbyaddr(&in, sizeof in, AF_INET);
	if ( !h )
		errx(1, "gethostbyaddr(127.0.0.1) failed: h_errno %d", h_errno);
	if ( h->h_addrtype != AF_INET || !h->h_name || !h->h_name[0] )
		errx(1, "gethostbyaddr's entry has no name or type");
	if ( !h->h_addr_list[0] || h->h_addr_list[1] || memcmp(h->h_addr_list[0], &in, 4) )
		errx(1, "gethostbyaddr's h_addr_list is not the one address given");
	return 0;
}
