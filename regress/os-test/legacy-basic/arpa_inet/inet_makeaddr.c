/*[SUSv2]*/
/* Test whether inet_makeaddr, inet_netof and inet_lnaof follow the address classes. */

#include <arpa/inet.h>

#include "../basic.h"

int main(void)
{
	struct in_addr a = inet_makeaddr(10, 0x010203);
	if ( ntohl(a.s_addr) != 0x0a010203 )
		errx(1, "inet_makeaddr(10, 0x010203) was %#x", ntohl(a.s_addr));
	a = inet_makeaddr(0x8002, 0x0304);
	if ( ntohl(a.s_addr) != 0x80020304 )
		errx(1, "inet_makeaddr(0x8002, 0x0304) was %#x", ntohl(a.s_addr));
	a.s_addr = htonl(0x80020304);	/* class B */
	if ( inet_netof(a) != 0x8002 || inet_lnaof(a) != 0x0304 )
		errx(1, "class B: inet_netof %#x, inet_lnaof %#x", inet_netof(a), inet_lnaof(a));
	a.s_addr = htonl(0xc0a80105);	/* class C */
	if ( inet_netof(a) != 0xc0a801 || inet_lnaof(a) != 5 )
		errx(1, "class C: inet_netof %#x, inet_lnaof %#x", inet_netof(a), inet_lnaof(a));
	a.s_addr = htonl(0x0a010203);	/* class A */
	if ( inet_netof(a) != 10 || inet_lnaof(a) != 0x010203 )
		errx(1, "class A: inet_netof %#x, inet_lnaof %#x", inet_netof(a), inet_lnaof(a));
	return 0;
}
