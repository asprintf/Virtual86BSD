/*[SUSv2]*/
#include <arpa/inet.h>
#ifdef inet_makeaddr
#undef inet_makeaddr
#endif
struct in_addr (*foo)(in_addr_t, in_addr_t) = inet_makeaddr;
int main(void) { return 0; }
