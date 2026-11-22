/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   kdos-mdnsd — answer DNS questions about .local names from Avahi
 *
 * musl has no NSS, so no nss-mdns module can hook getaddrinfo(): every host
 * lookup is a DNS query to /etc/resolv.conf's 127.0.0.1, NetworkManager's
 * dnsmasq. /etc/NetworkManager/dnsmasq.d/kdos-mdns.conf forwards the `local`
 * domain to this daemon on 127.0.0.1:5380, and it asks avahi-daemon over
 * the simple protocol on /run/avahi-daemon/socket:
 *
 *   RESOLVE-HOSTNAME-IPV4 <name>   ->  + <if> <proto> <name> <address>
 *   RESOLVE-HOSTNAME-IPV6 <name>   ->  -<errno> <message>
 *
 * WHAT IT ANSWERS. An A or AAAA question for a name ending in `.local` gets
 * Avahi's address as one record at a TTL of ten seconds. A miss — a `-`
 * reply, no reply in three seconds, an IPv6 link-local address, which a DNS
 * record cannot scope to an interface — is NOERROR with no answer and an SOA
 * for `local` whose minimum is ten seconds, so dnsmasq caches it. It is never
 * NXDOMAIN: musl's getaddrinfo() asks A and AAAA together and fails the whole
 * lookup on an NXDOMAIN to either, so an IPv4-only host would resolve to
 * nothing. Any other type under .local is the same empty answer, and a name
 * outside .local is REFUSED. A miss of one family once the other has
 * answered waits half a second more rather than the full three: Avahi's cache
 * holds every address record the responder sent with the first answer.
 *
 * Each question has its own non-blocking connection to Avahi and poll()
 * waits on all of them, so a name that never answers holds up nothing else;
 * at most MD_MAX are open, and a question past that is SERVFAIL. A question
 * Avahi cannot be reached for, or whose connection Avahi ends before a reply
 * line, is SERVFAIL too: "try again", not "no host", which dnsmasq would
 * cache. MD_MAX stays below avahi-daemon's own 50 simple-protocol clients,
 * which it refuses by closing the connection.
 *
 * It binds the port as root from 45_mdns.sh and drops to `nobody` before it
 * reads a packet. `--answer QUERY REPLY` runs one query file through the
 * parser and the answer builder against a recorded Avahi reply line and
 * writes the response packet to stdout, which is what testing/selftest.sh
 * drives.
 */

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <grp.h>
#include <netinet/in.h>
#include <poll.h>
#include <pwd.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/prctl.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <time.h>
#include <unistd.h>

#include "kbase.h"

#define MD_PORT     5380
#define MD_SOCKET   "/run/avahi-daemon/socket"
#define MD_USER     "nobody"
#define MD_MAX      48		/* questions in flight; under Avahi's 50 clients */
#define MD_WAIT_MS  3000	/* a name Avahi has not answered by then is a miss */
#define MD_PEER_MS  500		/* the other family's grace once one answered */
#define MD_TTL      10

#define T_A    1
#define T_SOA  6
#define T_AAAA 28

enum {
	MD_DROP,	/* not a question: no answer at all */
	MD_FORMERR,
	MD_NOTIMP,
	MD_REFUSED,
	MD_NODATA,	/* under .local, nothing to say */
	MD_ASK,		/* an A or AAAA question for Avahi */
};

struct md_query {
	uint16_t id, flags, qtype;
	int has_q;			/* the question parsed and is echoed */
	unsigned char qwire[256];	/* the name, uncompressed */
	size_t qwlen;
	size_t local_off;		/* where the `local` label starts */
	char name[256];			/* dotted, no trailing dot */
};

static long long now_ms(void)
{
	struct timespec ts;
	clock_gettime(CLOCK_MONOTONIC, &ts);
	return (long long)ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}

static uint16_t rd16(const unsigned char *p)
{
	return (uint16_t)(p[0] << 8 | p[1]);
}

static void wr16(unsigned char *p, unsigned v)
{
	p[0] = (unsigned char)(v >> 8);
	p[1] = (unsigned char)v;
}

static void wr32(unsigned char *p, unsigned long v)
{
	wr16(p, (unsigned)(v >> 16) & 0xffff);
	wr16(p + 2, (unsigned)v & 0xffff);
}

/* A byte Avahi is asked about: letters, digits, '-', '_' and UTF-8. Anything
 * else — a space or newline above all — could split the command line. */
static int name_byte_ok(unsigned char c)
{
	return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
	       (c >= '0' && c <= '9') || c == '-' || c == '_' || c >= 0x80;
}

/* The question's name at *pos into q. A compression pointer must point
 * before the last one followed, so every jump goes strictly backwards and a
 * loop cannot form; a label or name past the packet, or past 255 bytes, is
 * malformed. Returns -1 malformed, 0 well formed, 1 well formed but holding a
 * byte name_byte_ok() refuses. */
static int md_name(const unsigned char *p, size_t n, size_t *pos,
		   struct md_query *q)
{
	size_t at = *pos, limit = *pos, wl = 0, nl = 0;
	int jumped = 0, odd = 0;

	for (;;) {
		if (at >= n)
			return -1;
		unsigned c = p[at];
		if ((c & 0xc0) == 0xc0) {
			if (at + 1 >= n)
				return -1;
			size_t to = (size_t)(c & 0x3f) << 8 | p[at + 1];
			if (to >= limit)
				return -1;
			if (!jumped)
				*pos = at + 2;
			jumped = 1;
			limit = to;
			at = to;
			continue;
		}
		if (c & 0xc0)
			return -1;
		if (c == 0) {
			if (wl + 1 > sizeof q->qwire)
				return -1;
			q->qwire[wl++] = 0;
			if (!jumped)
				*pos = at + 1;
			break;
		}
		if (at + 1 + c > n || wl + 1 + c + 1 > sizeof q->qwire)
			return -1;
		q->local_off = wl;
		q->qwire[wl++] = (unsigned char)c;
		if (nl)
			q->name[nl++] = '.';
		for (unsigned i = 0; i < c; i++) {
			unsigned char b = p[at + 1 + i];
			if (!name_byte_ok(b))
				odd = 1;
			q->qwire[wl++] = b;
			q->name[nl++] = (char)b;
		}
		at += 1 + c;
	}
	q->name[nl] = 0;
	q->qwlen = wl;
	return odd;
}

/* Reads one packet into q and says what it is owed. */
static int md_parse(const unsigned char *p, size_t n, struct md_query *q)
{
	memset(q, 0, sizeof *q);
	if (n < 12)
		return MD_DROP;
	q->id = rd16(p);
	q->flags = rd16(p + 2);
	if (q->flags & 0x8000)
		return MD_DROP;
	if ((q->flags >> 11) & 0xf)
		return MD_NOTIMP;
	if (rd16(p + 4) != 1)
		return MD_FORMERR;

	size_t pos = 12;
	int r = md_name(p, n, &pos, q);
	if (r < 0 || pos + 4 > n)
		return MD_FORMERR;
	q->qtype = rd16(p + pos);
	q->has_q = 1;

	/* The last label is `local` and at least one label stands before it. */
	if ((rd16(p + pos + 2) & 0x7fff) != 1 || q->local_off == 0 ||
	    q->qwire[q->local_off] != 5 ||
	    strncasecmp((const char *)q->qwire + q->local_off + 1, "local", 5))
		return MD_REFUSED;
	if (r)
		return MD_REFUSED;
	if (q->qtype == T_A || q->qtype == T_AAAA)
		return MD_ASK;
	return MD_NODATA;
}

/* The response to q: rcode, and for MD_ASK an address of q's family or NULL
 * for a miss. Returns its length; the buffer must hold 512 bytes. */
static size_t md_answer(const struct md_query *q, int verdict,
			const unsigned char *addr, unsigned char *out)
{
	static const unsigned rcode[] = {
		[MD_FORMERR] = 1, [MD_NOTIMP] = 4, [MD_REFUSED] = 5,
	};
	unsigned rc = 0;
	if (verdict < 0)
		rc = 2;		/* SERVFAIL */
	else if (verdict < (int)(sizeof rcode / sizeof *rcode))
		rc = rcode[verdict];
	size_t o = 12;

	memset(out, 0, 12);
	wr16(out, q->id);
	wr16(out + 2, 0x8000 | 0x0400 | (q->flags & 0x0100) | 0x0080 | rc);
	if (q->has_q) {
		wr16(out + 4, 1);
		memcpy(out + o, q->qwire, q->qwlen);
		o += q->qwlen;
		wr16(out + o, q->qtype);
		wr16(out + o + 2, 1);
		o += 4;
	}
	if (rc)
		return o;

	if (verdict == MD_ASK && addr) {
		unsigned len = q->qtype == T_A ? 4 : 16;
		wr16(out + 6, 1);
		wr16(out + o, 0xc00c);
		wr16(out + o + 2, q->qtype);
		wr16(out + o + 4, 1);
		wr32(out + o + 6, MD_TTL);
		wr16(out + o + 10, len);
		memcpy(out + o + 12, addr, len);
		return o + 12 + len;
	}

	/* No answer: an SOA for `local` gives dnsmasq a negative TTL. */
	static const unsigned char mname[] = "\x09localhost";
	static const unsigned char rname[] = "\x0ahostmaster";
	unsigned zone = 0xc000 | (unsigned)(12 + q->local_off);
	unsigned rdlen = sizeof mname + sizeof rname - 1 + 2 + 20;
	wr16(out + 8, 1);
	wr16(out + o, zone);
	wr16(out + o + 2, T_SOA);
	wr16(out + o + 4, 1);
	wr32(out + o + 6, MD_TTL);
	wr16(out + o + 10, rdlen);
	o += 12;
	memcpy(out + o, mname, sizeof mname);	/* with its root label */
	o += sizeof mname;
	memcpy(out + o, rname, sizeof rname - 1);
	o += sizeof rname - 1;
	wr16(out + o, zone);
	o += 2;
	wr32(out + o, 1);
	wr32(out + o + 4, 3600);
	wr32(out + o + 8, 600);
	wr32(out + o + 12, 86400);
	wr32(out + o + 16, MD_TTL);
	return o + 20;
}

/* Avahi's reply line for q's family into addr. 1 an address, 0 a miss. */
static int md_reply(const struct md_query *q, const char *line,
		    unsigned char *addr)
{
	char host[256], text[64];
	int ifi, proto;

	if (sscanf(line, "+ %d %d %255s %63s", &ifi, &proto, host, text) != 4)
		return 0;
	if (q->qtype == T_A)
		return inet_pton(AF_INET, text, addr) == 1;
	if (inet_pton(AF_INET6, text, addr) != 1)
		return 0;
	return !(addr[0] == 0xfe && (addr[1] & 0xc0) == 0x80);
}

/* --- the daemon --------------------------------------------------------- */

struct md_pend {
	int fd;			/* -1: the slot is free */
	int reading;
	struct sockaddr_in peer;
	struct md_query q;
	char cmd[300];
	size_t cmdlen, cmdoff;
	char rbuf[512];
	size_t rlen;
	long long deadline;
};

static struct md_pend pend[MD_MAX];
static int udp = -1;
static const char *avahi_path = MD_SOCKET;

static void send_answer(const struct sockaddr_in *peer,
			const struct md_query *q, int verdict,
			const unsigned char *addr)
{
	unsigned char out[512];
	size_t len = md_answer(q, verdict, addr, out);
	sendto(udp, out, len, MSG_DONTWAIT, (const struct sockaddr *)peer,
	       sizeof *peer);
}

/* verdict MD_ASK with addr NULL is a miss, -1 is SERVFAIL. */
static void finish(struct md_pend *e, int verdict, const unsigned char *addr)
{
	send_answer(&e->peer, &e->q, verdict, addr);
	close(e->fd);
	e->fd = -1;
}

/* One family answered: the other family's question for the same name has
 * MD_PEER_MS left, not the rest of its three seconds. */
static void hurry_peers(const struct md_pend *done)
{
	long long by = now_ms() + MD_PEER_MS;
	for (int i = 0; i < MD_MAX; i++) {
		struct md_pend *e = &pend[i];
		if (e->fd < 0 || e == done || e->q.qtype == done->q.qtype ||
		    strcasecmp(e->q.name, done->q.name))
			continue;
		if (e->deadline > by)
			e->deadline = by;
	}
}

static void start_question(const struct sockaddr_in *peer,
			   const struct md_query *q)
{
	struct md_pend *e = NULL;
	for (int i = 0; i < MD_MAX && !e; i++)
		if (pend[i].fd < 0)
			e = &pend[i];
	if (!e) {
		send_answer(peer, q, -1, NULL);
		return;
	}

	int fd = socket(AF_UNIX, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, 0);
	struct sockaddr_un sa = { .sun_family = AF_UNIX };
	kb_strlcpy(sa.sun_path, avahi_path, sizeof sa.sun_path);
	if (fd < 0 || (connect(fd, (struct sockaddr *)&sa, sizeof sa) < 0 &&
		       errno != EINPROGRESS)) {
		if (fd >= 0)
			close(fd);
		send_answer(peer, q, -1, NULL);
		return;
	}
	e->fd = fd;
	e->reading = 0;
	e->peer = *peer;
	e->q = *q;
	e->cmdlen = (size_t)snprintf(e->cmd, sizeof e->cmd,
				     "RESOLVE-HOSTNAME-IPV%c %s\n",
				     q->qtype == T_A ? '4' : '6', q->name);
	e->cmdoff = 0;
	e->rlen = 0;
	e->deadline = now_ms() + MD_WAIT_MS;
}

static void on_writable(struct md_pend *e)
{
	ssize_t w = send(e->fd, e->cmd + e->cmdoff, e->cmdlen - e->cmdoff,
			 MSG_NOSIGNAL | MSG_DONTWAIT);
	if (w < 0) {
		if (errno == EAGAIN || errno == EINTR)
			return;
		finish(e, -1, NULL);
		return;
	}
	e->cmdoff += (size_t)w;
	if (e->cmdoff == e->cmdlen)
		e->reading = 1;
}

static void on_readable(struct md_pend *e)
{
	ssize_t r = recv(e->fd, e->rbuf + e->rlen, sizeof e->rbuf - 1 - e->rlen,
			 MSG_DONTWAIT);
	if (r < 0 && (errno == EAGAIN || errno == EINTR))
		return;
	if (r <= 0) {
		finish(e, -1, NULL);
		return;
	}
	e->rlen += (size_t)r;
	e->rbuf[e->rlen] = 0;
	if (!strchr(e->rbuf, '\n') && e->rlen < sizeof e->rbuf - 1)
		return;

	unsigned char addr[16];
	if (md_reply(&e->q, e->rbuf, addr)) {
		hurry_peers(e);
		finish(e, MD_ASK, addr);
	} else {
		finish(e, MD_ASK, NULL);
	}
}

static void on_packet(void)
{
	unsigned char in[1500];
	struct sockaddr_in peer;
	socklen_t plen = sizeof peer;
	ssize_t n = recvfrom(udp, in, sizeof in, MSG_DONTWAIT,
			     (struct sockaddr *)&peer, &plen);
	if (n < 0 || plen != sizeof peer || peer.sin_family != AF_INET)
		return;

	struct md_query q;
	int v = md_parse(in, (size_t)n, &q);
	if (v == MD_DROP)
		return;
	if (v == MD_ASK)
		start_question(&peer, &q);
	else
		send_answer(&peer, &q, v, NULL);
}

static void drop_to(const char *user)
{
	if (getuid() != 0 && geteuid() != 0)
		return;
	struct passwd *pw = getpwnam(user);
	if (!pw)
		kb_die("no account %s to run as", user);
	if (setgroups(0, NULL) < 0 || setgid(pw->pw_gid) < 0 ||
	    setuid(pw->pw_uid) < 0)
		kb_die("cannot become %s: %s", user, strerror(errno));
	if (setuid(0) == 0)
		kb_die("still able to become root after dropping to %s", user);
	prctl(PR_SET_NO_NEW_PRIVS, 1, 0, 0, 0);
}

static int serve(int port, const char *user)
{
	udp = socket(AF_INET, SOCK_DGRAM | SOCK_CLOEXEC, 0);
	if (udp < 0)
		kb_die("socket: %s", strerror(errno));
	struct sockaddr_in sa = {
		.sin_family = AF_INET,
		.sin_port = htons((uint16_t)port),
		.sin_addr.s_addr = htonl(INADDR_LOOPBACK),
	};
	if (bind(udp, (struct sockaddr *)&sa, sizeof sa) < 0)
		kb_die("bind 127.0.0.1:%d: %s", port, strerror(errno));
	socklen_t sl = sizeof sa;
	getsockname(udp, (struct sockaddr *)&sa, &sl);
	drop_to(user);
	signal(SIGPIPE, SIG_IGN);
	fprintf(stderr, "kdos-mdnsd: listening on 127.0.0.1:%d, asking %s\n",
		ntohs(sa.sin_port), avahi_path);

	for (int i = 0; i < MD_MAX; i++)
		pend[i].fd = -1;

	for (;;) {
		struct pollfd pf[MD_MAX + 1];
		int map[MD_MAX + 1], np = 1;
		long long now = now_ms(), next = -1;

		pf[0] = (struct pollfd){ .fd = udp, .events = POLLIN };
		for (int i = 0; i < MD_MAX; i++) {
			struct md_pend *e = &pend[i];
			if (e->fd < 0)
				continue;
			if (e->deadline <= now) {
				finish(e, MD_ASK, NULL);
				continue;
			}
			if (next < 0 || e->deadline < next)
				next = e->deadline;
			pf[np] = (struct pollfd){
				.fd = e->fd,
				.events = e->reading ? POLLIN : POLLOUT,
			};
			map[np++] = i;
		}
		int timeout = next < 0 ? -1 : (int)(next - now);
		if (poll(pf, (nfds_t)np, timeout) < 0) {
			if (errno == EINTR)
				continue;
			kb_die("poll: %s", strerror(errno));
		}
		for (int k = 1; k < np; k++) {
			struct md_pend *e = &pend[map[k]];
			if (e->fd < 0 || !pf[k].revents)
				continue;
			if (!e->reading && (pf[k].revents & (POLLOUT | POLLERR | POLLHUP)))
				on_writable(e);
			else if (e->reading)
				on_readable(e);
		}
		if (pf[0].revents & POLLIN)
			on_packet();
	}
}

/* --answer QUERY REPLY: QUERY is a packet file, REPLY Avahi's line, empty for
 * no reply. Exit 3 for a packet that gets no answer at all. */
static int answer_file(const char *path, const char *reply)
{
	size_t n = 0;
	char *in = kb_read_all(path, &n);
	if (!in)
		kb_die("cannot read %s", path);

	struct md_query q;
	int v = md_parse((const unsigned char *)in, n, &q);
	free(in);
	if (v == MD_DROP) {
		fprintf(stderr, "kdos-mdnsd: not a question; no answer\n");
		return 3;
	}
	unsigned char out[512], addr[16];
	int hit = v == MD_ASK && md_reply(&q, reply, addr);
	size_t len = md_answer(&q, v, hit ? addr : NULL, out);
	return fwrite(out, 1, len, stdout) == len ? 0 : 1;
}

int main(int argc, char **argv)
{
	int port = MD_PORT;
	const char *user = MD_USER;

	kb_set_progname("kdos-mdnsd");
	for (int i = 1; i < argc; i++) {
		if (!strcmp(argv[i], "--answer") && i + 2 < argc)
			return answer_file(argv[i + 1], argv[i + 2]);
		if (!strcmp(argv[i], "--port") && i + 1 < argc)
			port = atoi(argv[++i]);
		else if (!strcmp(argv[i], "--socket") && i + 1 < argc)
			avahi_path = argv[++i];
		else if (!strcmp(argv[i], "--user") && i + 1 < argc)
			user = argv[++i];
		else {
			fprintf(stderr,
				"usage: kdos-mdnsd [--port N] [--socket PATH] [--user NAME]\n"
				"       kdos-mdnsd --answer QUERY-FILE AVAHI-REPLY\n");
			return 2;
		}
	}
	if (port < 0 || port > 65535)
		kb_die("bad port %d", port);
	return serve(port, user);
}
