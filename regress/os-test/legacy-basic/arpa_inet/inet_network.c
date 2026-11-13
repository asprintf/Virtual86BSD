/*[SUSv2]*/
/* Test whether inet_network converts a network number in dot notation. */

#include <arpa/inet.h>

#include "../basic.h"

int main(void)
{
	if ( inet_network("10") != 10 )
		errx(1, "inet_network(\"10\") was %#x", inet_network("10"));
	if ( inet_network("128.2") != 0x8002 )
		errx(1, "inet_network(\"128.2\") was %#x", inet_network("128.2"));
	if ( inet_network("0x7f.0.0.1") != 0x7f000001 )
		errx(1, "inet_network(\"0x7f.0.0.1\") was %#x", inet_network("0x7f.0.0.1"));
	if ( inet_network("1.2.3.4.5") != (in_addr_t) -1 || inet_network("x") != (in_addr_t) -1 )
		errx(1, "inet_network of a bad string was not (in_addr_t) -1");
	return 0;
}
