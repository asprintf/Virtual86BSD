/*[SUSv2]*/
#include <arpa/inet.h>
#ifdef inet_netof
#undef inet_netof
#endif
in_addr_t (*foo)(struct in_addr) = inet_netof;
int main(void) { return 0; }
