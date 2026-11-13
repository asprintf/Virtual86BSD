/*[SUSv2]*/
#include <arpa/inet.h>
#ifdef inet_lnaof
#undef inet_lnaof
#endif
in_addr_t (*foo)(struct in_addr) = inet_lnaof;
int main(void) { return 0; }
