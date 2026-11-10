#ifndef _ARPA_INET_H
#define _ARPA_INET_H
#include <bits/features.h>
#include <netinet/in.h>

__BEGIN_DECLS
uint32_t htonl(uint32_t);
uint16_t htons(uint16_t);
uint32_t ntohl(uint32_t);
uint16_t ntohs(uint16_t);
in_addr_t inet_addr(const char *);
char *inet_ntoa(struct in_addr);
const char *inet_ntop(int, const void *__RESTRICT, char *__RESTRICT, socklen_t);
int inet_pton(int, const char *__RESTRICT, void *__RESTRICT);
__END_DECLS

#endif
