/* getnameinfo: /etc/hosts, then the DNS (PTR); /etc/services. */
#include <arpa/inet.h>
#include <net/if.h>
#include <stdio.h>
#include <string.h>
#include "netdb_impl.h"

#define FLAGS (NI_NUMERICHOST | NI_NUMERICSERV | NI_NOFQDN | NI_NAMEREQD | NI_DGRAM | NI_NUMERICSCOPE)

int getnameinfo(const struct sockaddr *__RESTRICT sa, socklen_t sl, char *__RESTRICT host, socklen_t hl,
                char *__RESTRICT serv, socklen_t svl, int flags)
{
	const unsigned char *a;
	unsigned scope = 0;
	int family, port;
	char buf[256], num[INET6_ADDRSTRLEN + IF_NAMESIZE + 2];

	if (flags & ~FLAGS)
		return EAI_BADFLAGS;
	if (!sa)
		return EAI_FAMILY;
	family = sa->sa_family;
	if (family == AF_INET && sl >= sizeof(struct sockaddr_in)) {
		const struct sockaddr_in *s = (const struct sockaddr_in *)sa;
		a = (const unsigned char *)&s->sin_addr;
		port = ntohs(s->sin_port);
	} else if (family == AF_INET6 && sl >= sizeof(struct sockaddr_in6)) {
		const struct sockaddr_in6 *s = (const struct sockaddr_in6 *)sa;
		a = s->sin6_addr.s6_addr;
		port = ntohs(s->sin6_port);
		scope = s->sin6_scope_id;
	} else
		return EAI_FAMILY;

	if (host && hl) {
		int found = 0;
		if (!(flags & NI_NUMERICHOST)) {
			found = __hosts_reverse(a, family, buf, sizeof buf);
			if (!found) {
				int r = __dns_reverse(a, family, buf, sizeof buf);
				if (r < 0 && (flags & NI_NAMEREQD))
					return r;
				found = r > 0;
			}
			if (found && (flags & NI_NOFQDN)) {
				char *dot = strchr(buf, '.');
				if (dot)
					*dot = 0;
			}
		}
		if (!found) {
			if (flags & NI_NAMEREQD)
				return EAI_NONAME;
			inet_ntop(family, a, num, sizeof num);
			if (scope) {
				char ifn[IF_NAMESIZE];
				size_t n = strlen(num);
				int linklocal = family == AF_INET6 && (IN6_IS_ADDR_LINKLOCAL(a) || IN6_IS_ADDR_MC_LINKLOCAL(a));
				if (!(flags & NI_NUMERICSCOPE) && linklocal && if_indextoname(scope, ifn))
					snprintf(num + n, sizeof num - n, "%%%s", ifn);
				else
					snprintf(num + n, sizeof num - n, "%%%u", scope);
			}
			strcpy(buf, num);
		}
		if (strlen(buf) >= hl)
			return EAI_OVERFLOW;
		strcpy(host, buf);
	}
	if (serv && svl) {
		int found = 0;
		if (!(flags & NI_NUMERICSERV)) {
			struct servent *se = getservbyport(htons((unsigned short)port), flags & NI_DGRAM ? "udp" : "tcp");
			if (se && strlen(se->s_name) < sizeof buf) {
				strcpy(buf, se->s_name);
				found = 1;
			}
		}
		if (!found)
			snprintf(buf, sizeof buf, "%d", port);
		if (strlen(buf) >= svl)
			return EAI_OVERFLOW;
		strcpy(serv, buf);
	}
	if ((!host || !hl) && (!serv || !svl))
		return EAI_NONAME;
	return 0;
}
