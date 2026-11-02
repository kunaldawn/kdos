/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   http.c — one request to a server on this machine, never blocking
 *
 * kiwix-serve, Kolibri and llama-server listen on 127.0.0.1 (boot-and-init.md,
 * "The local servers"). This is the whole client: the caller's request sent
 * as written (a GET, or ask.c's POST with its body), HTTP/1.0 so the server
 * closes when it is done and no chunked body ever arrives, a connect that
 * cannot block, and an answer read until the server closes or the cap is
 * reached.
 *
 * LOOPBACK ONLY, BY CONSTRUCTION. The address is 127.0.0.1 in this file and
 * nowhere a caller can change; the port is the only thing a caller picks.
 * The palette fetches nothing from any network.
 *
 * THE MAIN LOOP DRIVES IT (main.c): each ask is a descriptor in its poll set,
 * and a Match that asked is answered when every ask is done or its deadline
 * passes, whichever comes first. A server that hangs costs one deadline.
 * ---------------------------------
 */

#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#include "runner.h"

#define RN_HTTP_CAP (512 * 1024)

int rn_http_start(RnAsk *a)
{
	struct sockaddr_in sa = { 0 };

	a->fd = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, 0);
	if (a->fd < 0) {
		a->done = -1;
		return -1;
	}
	sa.sin_family = AF_INET;
	sa.sin_port = htons((unsigned short)a->port);
	sa.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
	a->reqlen = strlen(a->req);
	a->sent = 0;
	if (connect(a->fd, (struct sockaddr *)&sa, sizeof(sa)) != 0 &&
	    errno != EINPROGRESS) {
		/* Nothing listening is the common case: the server starts
		 * only when there is a library for it to serve. */
		close(a->fd);
		a->fd = -1;
		a->done = -1;
	}
	return 0;
}

short rn_http_events(const RnAsk *a)
{
	if (a->done || a->fd < 0)
		return 0;
	return a->sent < a->reqlen ? POLLOUT : POLLIN;
}

static void finish(RnAsk *a, int how)
{
	if (a->fd >= 0)
		close(a->fd);
	a->fd = -1;
	a->done = how;
}

void rn_http_step(RnAsk *a, short revents)
{
	if (a->done || a->fd < 0)
		return;
	if (revents & (POLLERR | POLLNVAL)) {
		finish(a, -1);
		return;
	}
	if (a->sent < a->reqlen) {
		ssize_t w;

		if (!(revents & POLLOUT))
			return;
		w = send(a->fd, a->req + a->sent, a->reqlen - a->sent,
			 MSG_NOSIGNAL);
		if (w < 0) {
			if (errno != EAGAIN && errno != EINTR)
				finish(a, -1);
			return;
		}
		a->sent += (size_t)w;
		return;
	}
	if (!(revents & (POLLIN | POLLHUP)))
		return;
	for (;;) {
		char buf[8192];
		ssize_t r = recv(a->fd, buf, sizeof(buf), 0);

		if (r < 0) {
			if (errno == EINTR)
				continue;
			if (errno != EAGAIN)
				finish(a, -1);
			return;
		}
		if (r == 0) {
			finish(a, 1);
			return;
		}
		if (a->resp.n + (size_t)r > RN_HTTP_CAP) {
			finish(a, -1);
			return;
		}
		kb_buf_add(&a->resp, buf, (size_t)r);
	}
}

const char *rn_http_body(const RnAsk *a)
{
	const char *p, *body;

	if (a->done != 1 || !a->resp.p)
		return NULL;
	/* kb_buf_add keeps the buffer NUL-terminated past its length. */
	p = a->resp.p;
	if (strncmp(p, "HTTP/1.", 7) || strlen(p) < 12 || strncmp(p + 9, "200", 3))
		return NULL;
	body = strstr(p, "\r\n\r\n");
	return body ? body + 4 : NULL;
}

void rn_http_free(RnAsk *a)
{
	while (a) {
		RnAsk *n = a->next;

		if (a->fd >= 0)
			close(a->fd);
		kb_buf_free(&a->resp);
		free(a);
		a = n;
	}
}

void rn_urlenc(KbBuf *b, const char *s, int keep_slash)
{
	static const char hex[] = "0123456789ABCDEF";

	for (const unsigned char *p = (const unsigned char *)s; *p; p++) {
		if ((*p >= 'a' && *p <= 'z') || (*p >= 'A' && *p <= 'Z') ||
		    (*p >= '0' && *p <= '9') || *p == '-' || *p == '_' ||
		    *p == '.' || *p == '~' || (keep_slash && *p == '/')) {
			kb_buf_add(b, p, 1);
		} else {
			char e[3] = { '%', hex[*p >> 4], hex[*p & 15] };

			kb_buf_add(b, e, 3);
		}
	}
}
