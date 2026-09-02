/* libkcon — the wire: buffers, framing and connections. See kcon.h. */

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <fcntl.h>
#include <unistd.h>

#include "kcon.h"

#define HDR_BYTES 8		/* u32 len, u16 op, u16 flags */

/* ── buffers ─────────────────────────────────────────────────────────────
 *
 * `err` latches. A caller writes a whole message and tests once at the end,
 * rather than checking every field and getting it wrong on the one that
 * matters.
 * ──────────────────────────────────────────────────────────────────────── */

static int reserve(KconBuf *b, size_t n)
{
	if (b->err)
		return -1;
	if (b->len + n <= b->cap)
		return 0;

	size_t want = b->cap ? b->cap * 2 : 256;

	while (want < b->len + n)
		want *= 2;

	/* Refused here rather than at the socket: a message this large is a
	 * caller bug, and growing to it first would make the bug expensive. */
	if (want > KCON_MAX_PAYLOAD) {
		b->err = -EMSGSIZE;
		return -1;
	}

	unsigned char *p = realloc(b->b, want);

	if (!p) {
		b->err = -ENOMEM;
		return -1;
	}

	b->b = p;
	b->cap = want;
	return 0;
}

void kcon_buf_free(KconBuf *b)
{
	free(b->b);
	memset(b, 0, sizeof(*b));
}

void kcon_buf_reset(KconBuf *b)
{
	b->len = 0;
	b->err = 0;
}

int kcon_put_u8(KconBuf *b, uint8_t v)
{
	if (reserve(b, 1))
		return -1;
	b->b[b->len++] = v;
	return 0;
}

int kcon_put_u16(KconBuf *b, uint16_t v)
{
	if (reserve(b, 2))
		return -1;
	b->b[b->len++] = (unsigned char)(v & 0xff);
	b->b[b->len++] = (unsigned char)(v >> 8);
	return 0;
}

int kcon_put_u32(KconBuf *b, uint32_t v)
{
	if (reserve(b, 4))
		return -1;
	for (int i = 0; i < 4; i++)
		b->b[b->len++] = (unsigned char)((v >> (i * 8)) & 0xff);
	return 0;
}

int kcon_put_i32(KconBuf *b, int32_t v)
{
	/* Through the unsigned form on purpose: a negative shifted right is
	 * implementation-defined, and coordinates are routinely negative. */
	return kcon_put_u32(b, (uint32_t)v);
}

int kcon_put_bytes(KconBuf *b, const void *p, size_t n)
{
	if (n > KCON_MAX_PAYLOAD) {
		b->err = -EMSGSIZE;
		return -1;
	}
	if (!n)
		return 0;
	if (reserve(b, n))
		return -1;
	memcpy(b->b + b->len, p, n);
	b->len += n;
	return 0;
}

int kcon_put_blob(KconBuf *b, const void *p, size_t n)
{
	if (n > KCON_MAX_PAYLOAD) {
		b->err = -EMSGSIZE;
		return -1;
	}
	if (kcon_put_u32(b, (uint32_t)n))
		return -1;
	if (!n)
		return 0;
	if (reserve(b, n))
		return -1;
	memcpy(b->b + b->len, p, n);
	b->len += n;
	return 0;
}

int kcon_put_str(KconBuf *b, const char *s)
{
	return kcon_put_blob(b, s ? s : "", s ? strlen(s) : 0);
}

/* ── reading ─────────────────────────────────────────────────────────── */

void kcon_rd_init(KconRd *r, const void *p, size_t n)
{
	r->b = p;
	r->len = n;
	r->pos = 0;
	r->err = 0;
}

size_t kcon_rd_left(const KconRd *r)
{
	if (r->err || r->pos > r->len)
		return 0;
	return r->len - r->pos;
}

static int take(KconRd *r, size_t n)
{
	if (r->err)
		return -1;
	if (r->pos + n > r->len) {
		r->err = -EBADMSG;
		return -1;
	}
	return 0;
}

uint8_t kcon_get_u8(KconRd *r)
{
	if (take(r, 1))
		return 0;
	return r->b[r->pos++];
}

uint16_t kcon_get_u16(KconRd *r)
{
	if (take(r, 2))
		return 0;
	uint16_t v = (uint16_t)r->b[r->pos] | ((uint16_t)r->b[r->pos + 1] << 8);

	r->pos += 2;
	return v;
}

uint32_t kcon_get_u32(KconRd *r)
{
	if (take(r, 4))
		return 0;
	uint32_t v = 0;

	for (int i = 0; i < 4; i++)
		v |= (uint32_t)r->b[r->pos + i] << (i * 8);
	r->pos += 4;
	return v;
}

int32_t kcon_get_i32(KconRd *r)
{
	return (int32_t)kcon_get_u32(r);
}

const void *kcon_get_blob(KconRd *r, size_t n)
{
	if (take(r, n))
		return NULL;
	const void *p = r->b + r->pos;

	r->pos += n;
	return p;
}

const char *kcon_get_str(KconRd *r)
{
	uint32_t n = kcon_get_u32(r);

	if (r->err || n > r->len - r->pos) {
		r->err = -EBADMSG;
		return "";
	}

	/*
	 * ONE SCRATCH BUFFER FOR EVERY CALL. The payload's bytes are not
	 * terminated where the string ends, so it cannot be handed back in
	 * place; a caller reading several strings has to copy each before it
	 * reads the next, or every pointer it kept names the last one.
	 */
	static char scratch[1024];
	size_t k = n < sizeof(scratch) - 1 ? n : sizeof(scratch) - 1;

	memcpy(scratch, r->b + r->pos, k);
	scratch[k] = '\0';
	r->pos += n;
	return scratch;
}

/* ── cells ───────────────────────────────────────────────────────────── */

int kcon_put_run(KconBuf *b, uint16_t x, uint16_t y, const KtuiCell *cells,
		 uint16_t n)
{
	if (kcon_put_u16(b, x) || kcon_put_u16(b, y) || kcon_put_u16(b, n))
		return -1;

	for (uint16_t i = 0; i < n; i++) {
		if (kcon_put_u32(b, cells[i].ch) ||
		    kcon_put_u8(b, cells[i].fg) ||
		    kcon_put_u8(b, cells[i].bg) ||
		    kcon_put_u8(b, cells[i].attr) ||
		    kcon_put_u8(b, 0))		/* reserved, keeps it 8 bytes */
			return -1;
	}

	return 0;
}

int kcon_get_run(KconRd *r, uint16_t *x, uint16_t *y, KtuiCell *out,
		 uint16_t max)
{
	*x = kcon_get_u16(r);
	*y = kcon_get_u16(r);

	uint16_t n = kcon_get_u16(r);

	if (r->err)
		return -1;
	/* A count the payload cannot hold is a truncated or hostile message,
	 * and is refused before anything is written into `out`. */
	if (n > max || (size_t)n * KCON_CELL_BYTES > r->len - r->pos) {
		r->err = -EBADMSG;
		return -1;
	}

	for (uint16_t i = 0; i < n; i++) {
		out[i].ch = kcon_get_u32(r);
		out[i].fg = kcon_get_u8(r);
		out[i].bg = kcon_get_u8(r);
		out[i].attr = kcon_get_u8(r);
		(void)kcon_get_u8(r);
	}

	return r->err ? -1 : (int)n;
}

/* ── connections ─────────────────────────────────────────────────────── */

struct KconConn {
	int fd;
	int dead;

	unsigned char *out;
	size_t out_len, out_cap, out_off;

	unsigned char *in;
	size_t in_len, in_cap;
	/* The message handed out last time, still sitting at the front of the
	 * buffer. It is dropped at the START of the next receive, which is what
	 * keeps a returned payload valid while the caller reads it. */
	size_t in_hold;
};

KconConn *kcon_conn_new(int fd)
{
	KconConn *c = calloc(1, sizeof(*c));

	if (!c)
		return NULL;

	c->fd = fd;
	/* Non-blocking both ways: nothing holding a display may ever wait on a
	 * peer, and a client that blocks writing to a wedged server hangs. */
	int fl = fcntl(fd, F_GETFL, 0);

	if (fl >= 0)
		fcntl(fd, F_SETFL, fl | O_NONBLOCK);
	return c;
}

void kcon_conn_free(KconConn *c)
{
	if (!c)
		return;
	if (c->fd >= 0)
		close(c->fd);
	free(c->out);
	free(c->in);
	free(c);
}

int kcon_conn_fd(const KconConn *c)
{
	return c ? c->fd : -1;
}

int kcon_conn_dead(const KconConn *c)
{
	return !c || c->dead;
}

static int out_reserve(KconConn *c, size_t n)
{
	/* Compact first: the offset is how far the writer got last time, and
	 * a long-lived connection would otherwise grow without bound. */
	if (c->out_off && c->out_off == c->out_len)
		c->out_len = c->out_off = 0;
	else if (c->out_off > 65536) {
		memmove(c->out, c->out + c->out_off, c->out_len - c->out_off);
		c->out_len -= c->out_off;
		c->out_off = 0;
	}

	if (c->out_len + n <= c->out_cap)
		return 0;

	size_t want = c->out_cap ? c->out_cap * 2 : 4096;

	while (want < c->out_len + n)
		want *= 2;

	if (want > KCON_MAX_QUEUE) {
		/* THE PEER STOPPED READING. Dropping it is the only answer
		 * that does not stop everything else. */
		c->dead = 1;
		return -1;
	}

	unsigned char *p = realloc(c->out, want);

	if (!p) {
		c->dead = 1;
		return -1;
	}

	c->out = p;
	c->out_cap = want;
	return 0;
}

int kcon_send(KconConn *c, uint16_t op, const KconBuf *payload)
{
	if (!c || c->dead)
		return -1;

	size_t n = payload ? payload->len : 0;

	if (payload && payload->err)
		return -1;
	if (n > KCON_MAX_PAYLOAD)
		return -1;
	if (out_reserve(c, n + HDR_BYTES))
		return -1;

	unsigned char *h = c->out + c->out_len;

	for (int i = 0; i < 4; i++)
		h[i] = (unsigned char)((n >> (i * 8)) & 0xff);
	h[4] = (unsigned char)(op & 0xff);
	h[5] = (unsigned char)(op >> 8);
	h[6] = 0;
	h[7] = 0;
	c->out_len += HDR_BYTES;

	if (n) {
		memcpy(c->out + c->out_len, payload->b, n);
		c->out_len += n;
	}

	return kcon_flush(c) < 0 ? -1 : 0;
}

int kcon_flush(KconConn *c)
{
	if (!c || c->dead)
		return -1;

	while (c->out_off < c->out_len) {
		/*
		 * MSG_NOSIGNAL, NOT write(). A display that goes away leaves a
		 * socket whose next write raises SIGPIPE, and the default
		 * disposition is death — so a surface would be killed by its
		 * display disappearing instead of noticing and closing. A
		 * library cannot fix that by changing the process's signal
		 * handling; it has to not ask for the signal.
		 */
		ssize_t w = send(c->fd, c->out + c->out_off,
				 c->out_len - c->out_off, MSG_NOSIGNAL);

		if (w > 0) {
			c->out_off += (size_t)w;
			continue;
		}
		if (w < 0 && (errno == EAGAIN || errno == EWOULDBLOCK))
			return 1;	/* still queued; poll for writability */
		if (w < 0 && errno == EINTR)
			continue;
		c->dead = 1;
		return -1;
	}

	c->out_len = c->out_off = 0;
	return 0;
}

static int in_reserve(KconConn *c, size_t n)
{
	if (c->in_len + n <= c->in_cap)
		return 0;

	size_t want = c->in_cap ? c->in_cap * 2 : 4096;

	while (want < c->in_len + n)
		want *= 2;
	if (want > KCON_MAX_PAYLOAD + HDR_BYTES * 2)
		want = KCON_MAX_PAYLOAD + HDR_BYTES * 2;

	unsigned char *p = realloc(c->in, want);

	if (!p) {
		c->dead = 1;
		return -1;
	}

	c->in = p;
	c->in_cap = want;
	return 0;
}

int kcon_recv(KconConn *c, KconMsg *out)
{
	if (!c || c->dead)
		return -1;

	/* Drop what the caller was shown last time, now that it cannot be
	 * looking at it any more. */
	if (c->in_hold) {
		memmove(c->in, c->in + c->in_hold, c->in_len - c->in_hold);
		c->in_len -= c->in_hold;
		c->in_hold = 0;
	}

	for (;;) {
		if (c->in_len >= HDR_BYTES) {
			uint32_t len = 0;

			for (int i = 0; i < 4; i++)
				len |= (uint32_t)c->in[i] << (i * 8);

			/*
			 * REFUSED AT THE HEADER, before a byte is reserved for
			 * it. A length field is an allocation request from a
			 * peer, and this is the only place to say no cheaply.
			 */
			if (len > KCON_MAX_PAYLOAD) {
				c->dead = 1;
				return -1;
			}

			if (c->in_len >= HDR_BYTES + len) {
				out->op = (uint16_t)c->in[4] |
					  ((uint16_t)c->in[5] << 8);
				out->flags = (uint16_t)c->in[6] |
					     ((uint16_t)c->in[7] << 8);
				out->payload = c->in + HDR_BYTES;
				out->len = len;

				/*
				 * NOT consumed here. `out->payload` points into
				 * this buffer, and compacting it now would hand
				 * the caller the next message's bytes under the
				 * pointer it was just given.
				 */
				c->in_hold = HDR_BYTES + len;
				return 1;
			}
		}

		if (in_reserve(c, 4096))
			return -1;

		ssize_t r = read(c->fd, c->in + c->in_len,
				 c->in_cap - c->in_len);

		if (r > 0) {
			c->in_len += (size_t)r;
			continue;
		}
		if (r == 0) {
			c->dead = 1;	/* the peer closed */
			return -1;
		}
		if (errno == EAGAIN || errno == EWOULDBLOCK)
			return 0;	/* nothing more has arrived yet */
		if (errno == EINTR)
			continue;
		c->dead = 1;
		return -1;
	}
}
