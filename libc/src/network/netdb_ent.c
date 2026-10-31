/* The network databases as POSIX enumerates them: hosts, networks,
 * protocols, services from /etc/hosts, /etc/networks, /etc/protocols
 * and /etc/services.  The results are in static storage. */
#include <arpa/inet.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "netdb_impl.h"

#define MAXALIASES 35

struct db {
	const char *const *path;
	FILE *f;
	int stayopen;
	char line[1024];
	char *fl[MAXALIASES + 3];
	int n;
};

static const char *networks_path = "/etc/networks", *protocols_path = "/etc/protocols";
static struct db hosts = {&__hosts_path}, nets = {&networks_path}, protos = {&protocols_path},
                 servs = {&__services_path};

static void db_set(struct db *d, int stay)
{
	if (d->f)
		rewind(d->f);
	else
		d->f = fopen(*d->path, "re");
	d->stayopen |= stay;
}

static void db_end(struct db *d)
{
	if (d->f)
		fclose(d->f);
	d->f = 0;
	d->stayopen = 0;
}

/* the next line with at least `min' fields: 1, or 0 */
static int db_next(struct db *d, int min)
{
	if (!d->f && !(d->f = fopen(*d->path, "re")))
		return 0;
	while (fgets(d->line, sizeof d->line, d->f)) {
		d->n = __netdb_fields(d->line, d->fl, MAXALIASES + 2);
		if (d->n >= min) {
			d->fl[d->n] = 0;
			return 1;
		}
	}
	return 0;
}

/* ---- hosts ---- */
static struct hostent he;
static unsigned char he_addr[16];
static char *he_list[2];

void sethostent(int stay) { db_set(&hosts, stay); }
void endhostent(void) { db_end(&hosts); }

struct hostent *gethostent(void)
{
	while (db_next(&hosts, 2)) {
		char buf[64], *pct;
		int fam = AF_INET;
		if (inet_pton(AF_INET, hosts.fl[0], he_addr) != 1) {
			if (strlen(hosts.fl[0]) >= sizeof buf)
				continue;
			strcpy(buf, hosts.fl[0]);
			if ((pct = strchr(buf, '%')))
				*pct = 0;
			if (inet_pton(AF_INET6, buf, he_addr) != 1)
				continue;
			fam = AF_INET6;
		}
		he.h_name = hosts.fl[1];
		he.h_aliases = hosts.fl + 2;
		he.h_addrtype = fam;
		he.h_length = fam == AF_INET ? 4 : 16;
		he_list[0] = (char *)he_addr;
		he_list[1] = 0;
		he.h_addr_list = he_list;
		return &he;
	}
	return 0;
}

/* ---- networks ---- */
static struct netent ne;

/* "a.b.c.d" with fewer parts meaning the low ones: 169.254 = 0xa9fe */
static int network(const char *s, uint32_t *v)
{
	uint32_t r = 0;
	int parts = 0;
	char *end;
	for (;;) {
		unsigned long x = strtoul(s, &end, 0);
		if (end == s || x > 255 || parts == 4)
			return 0;
		r = r << 8 | (uint32_t)x;
		parts++;
		if (*end != '.')
			break;
		s = end + 1;
	}
	if (*end)
		return 0;
	*v = r;
	return 1;
}

void setnetent(int stay) { db_set(&nets, stay); }
void endnetent(void) { db_end(&nets); }

struct netent *getnetent(void)
{
	while (db_next(&nets, 2)) {
		if (!network(nets.fl[1], &ne.n_net))
			continue;
		ne.n_name = nets.fl[0];
		nets.fl[1] = nets.fl[0];
		ne.n_aliases = nets.fl + 2;
		ne.n_addrtype = AF_INET;
		return &ne;
	}
	return 0;
}

struct netent *getnetbyname(const char *name)
{
	struct netent *n;
	char **a;
	setnetent(0);
	while ((n = getnetent())) {
		if (!strcmp(n->n_name, name))
			break;
		for (a = n->n_aliases; *a && strcmp(*a, name); a++)
			;
		if (*a)
			break;
	}
	if (!nets.stayopen)
		endnetent();
	return n;
}

struct netent *getnetbyaddr(uint32_t net, int type)
{
	struct netent *n;
	setnetent(0);
	while ((n = getnetent()) && !(n->n_net == net && n->n_addrtype == type))
		;
	if (!nets.stayopen)
		endnetent();
	return n;
}

/* ---- protocols ---- */
static struct protoent pe;

void setprotoent(int stay) { db_set(&protos, stay); }
void endprotoent(void) { db_end(&protos); }

struct protoent *getprotoent(void)
{
	if (!db_next(&protos, 2))
		return 0;
	pe.p_name = protos.fl[0];
	pe.p_proto = atoi(protos.fl[1]);
	protos.fl[1] = protos.fl[0];
	pe.p_aliases = protos.fl + 2;
	return &pe;
}

struct protoent *getprotobyname(const char *name)
{
	struct protoent *p;
	char **a;
	setprotoent(0);
	while ((p = getprotoent())) {
		if (!strcmp(p->p_name, name))
			break;
		for (a = p->p_aliases; *a && strcmp(*a, name); a++)
			;
		if (*a)
			break;
	}
	if (!protos.stayopen)
		endprotoent();
	return p;
}

struct protoent *getprotobynumber(int num)
{
	struct protoent *p;
	setprotoent(0);
	while ((p = getprotoent()) && p->p_proto != num)
		;
	if (!protos.stayopen)
		endprotoent();
	return p;
}

/* ---- services ---- */
static struct servent se;

void setservent(int stay) { db_set(&servs, stay); }
void endservent(void) { db_end(&servs); }

struct servent *getservent(void)
{
	char *slash;
	while (db_next(&servs, 2)) {
		if (!(slash = strchr(servs.fl[1], '/')))
			continue;
		*slash = 0;
		se.s_name = servs.fl[0];
		se.s_port = htons((uint16_t)atoi(servs.fl[1]));
		se.s_proto = slash + 1;
		se.s_aliases = servs.fl + 2;
		return &se;
	}
	return 0;
}

struct servent *getservbyname(const char *name, const char *proto)
{
	struct servent *s;
	char **a;
	setservent(0);
	while ((s = getservent())) {
		if (proto && strcmp(s->s_proto, proto))
			continue;
		if (!strcmp(s->s_name, name))
			break;
		for (a = s->s_aliases; *a && strcmp(*a, name); a++)
			;
		if (*a)
			break;
	}
	if (!servs.stayopen)
		endservent();
	return s;
}

struct servent *getservbyport(int port, const char *proto)
{
	struct servent *s;
	setservent(0);
	while ((s = getservent()) && !(s->s_port == port && (!proto || !strcmp(s->s_proto, proto))))
		;
	if (!servs.stayopen)
		endservent();
	return s;
}
