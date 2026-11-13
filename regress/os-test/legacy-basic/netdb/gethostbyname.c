/*[SUSv3]*/
/* Test whether gethostbyname finds localhost. */

#include <arpa/inet.h>
#include <netdb.h>
#include <string.h>

#include "../basic.h"

int main(void)
{
	struct hostent* h = gethostbyname("localhost");
	if ( !h )
		errx(1, "gethostbyname(\"localhost\") failed: h_errno %d", h_errno);
	if ( h->h_addrtype != AF_INET || h->h_length != 4 )
		errx(1, "gethostbyname's entry is not AF_INET with 4 bytes");
	int found = 0;
	for ( char** a = h->h_addr_list; *a; a++ )
	{
		struct in_addr in;
		memcpy(&in, *a, 4);
		found |= ntohl(in.s_addr) == 0x7f000001;
	}
	if ( !found )
		errx(1, "localhost's addresses do not include 127.0.0.1");
	return 0;
}
