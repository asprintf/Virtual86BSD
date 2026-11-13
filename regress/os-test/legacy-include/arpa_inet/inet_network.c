/*[SUSv2]*/
#include <arpa/inet.h>
#ifdef inet_network
#undef inet_network
#endif
in_addr_t (*foo)(const char *) = inet_network;
int main(void) { return 0; }
