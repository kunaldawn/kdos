/*
 * kdos-cage --embed — the selection, across the private channel.
 *
 * THE SESSION OWNS THE SELECTION AND THIS CAGE HOLDS A COPY OF IT. On the cell
 * desktop every boxed application is a compositor of its own with a seat of its
 * own, so a selection left where a guest put it is one clipboard per
 * application: a copy in the browser that the terminal beside it cannot paste,
 * and a copy in the terminal the browser cannot see. Every window of one guest
 * shares this seat and therefore this clipboard. Both directions of the channel
 * are here — what the guest copies is offered up in KEMBED_CLIP_OFFER, and what
 * KEMBED_CLIP_SET carries is installed on this seat as a source of this
 * compositor's own.
 *
 * TEXT, AND THE TWO NAMES FOR IT. The session's clipboard holds bytes with no
 * type beside them, so the negotiation is the first of
 * `text/plain;charset=utf-8` and `text/plain` the guest offers — which is also
 * what XWayland's own bridge maps UTF8_STRING and STRING to. A guest that
 * offers an image and nothing else has copied something this desktop has
 * nowhere to put: the selection is LEFT AS IT WAS rather than replaced with
 * nothing, because a copy that silently emptied the clipboard is worse than a
 * copy that did not take.
 *
 * NOTHING IS TRANSFERRED WHILE THIS PROCESS WAITS FOR IT. Both halves are file
 * descriptors on the event loop: a guest that offers a mime type and never
 * writes, or that asks for the selection and never reads, would otherwise stop
 * the loop that also pumps frames — the window would freeze because somebody
 * pressed Ctrl+C in it.
 *
 * A KEMBED_CLIP_SET CARRYING THE BYTES THIS CAGE LAST OFFERED CHANGES NOTHING
 * HERE. Every offer is remembered and compared, because a session that mirrors
 * a guest's own copy back at it would otherwise destroy that guest's source and
 * replace it a moment after the copy — which for a toolkit is its copy being
 * cancelled.
 */

/* memfd_create and pipe2. Guarded: the compile gate puts the flag on the
 * command line, and an unconditional define collides with it under -Werror. */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <unistd.h>

#include <wayland-server-core.h>
#include <wlr/types/wlr_data_device.h>
#include <wlr/types/wlr_primary_selection.h>
#include <wlr/types/wlr_seat.h>
#include <wlr/util/log.h>

#include "clipboard.h"
#include "kembed.h"
#include "seat.h"
#include "server.h"

/*
 * THE TWO NAMES FOR TEXT, IN PREFERENCE ORDER. Nearly every guest offers both;
 * one that offers only the second is saying it has no encoding to promise, and
 * the session takes the bytes either way.
 */
static const char *const clip_mime[] = {
	"text/plain;charset=utf-8",
	"text/plain",
};

/*
 * HOW MUCH IS TAKEN FROM A PIPE IN ONE CALL. The transfer is bounded by
 * KEMBED_CLIP_MAX, so this decides only how often the loop comes back: one
 * read for a file name, sixteen for the largest selection there is.
 */
#define CLIP_CHUNK 4096

/* A guest copied, and this is the transfer reading what it copied. */
struct clip_read {
	struct wl_event_source *source;
	struct wl_listener src_destroy;
	int fd;
	int idx;
	char *buf;
	size_t len;
};

/* A guest asked for the session's selection, and this is what it has not
 * taken from the pipe yet. */
struct clip_write {
	struct wl_list link;
	struct wl_event_source *source;
	int fd;
	char *buf;
	size_t len, sent;
};

/* The clipboard source this compositor installs on its own seat. */
struct clip_source {
	struct wlr_data_source base;
};

/* The same for the primary selection, which is a different wlroots type and
 * the same bytes. */
struct clip_prim_source {
	struct wlr_primary_selection_source base;
};

/*
 * ONE CAGE IS ONE PROCESS AND ONE CHANNEL, so the bridge rides beside the
 * server rather than inside it: [0] is the clipboard and [1] the primary
 * selection, which is how kembed's `a` reads on both ops.
 */
static struct {
	struct cg_server *server;
	struct wl_listener set_selection;
	struct wl_listener set_primary;
	struct wl_list writes;
	bool live;
} clip;

static struct {
	char *text;		/* the bytes this cage believes are the
				 * selection — what it last sent out or was
				 * last told, and what it serves the guest */
	size_t len;
	struct clip_read *in;	/* a guest's copy, being read              */
} sel[2];

static struct clip_source *clip_src;		/* ours, on the seat, [0]  */
static struct clip_prim_source *prim_src;	/* ours, on the seat, [1]  */

static const struct wlr_data_source_impl clip_src_impl;
static const struct wlr_primary_selection_source_impl prim_src_impl;

static struct wlr_seat *the_seat(void)
{
	return clip.server->seat->seat;
}

static struct wl_event_loop *the_loop(void)
{
	return wl_display_get_event_loop(clip.server->wl_display);
}

/* ── the wire ────────────────────────────────────────────────────────── */

/*
 * ONE DATAGRAM AND THE DESCRIPTOR WITH IT. `fd` is the caller's and stays the
 * caller's: the kernel copies it into the receiving process, so this end
 * closes its own copy when it has finished with it and not before.
 */
static bool send_offer(int idx, size_t len, int fd)
{
	KembedMsg m = {
		.magic = KEMBED_MAGIC,
		.op = KEMBED_CLIP_OFFER,
		.a = idx,
		.b = (int32_t)len,
	};
	struct iovec iov = { .iov_base = &m, .iov_len = sizeof(m) };
	struct msghdr hdr = { .msg_iov = &iov, .msg_iovlen = 1 };
	union {
		char buf[CMSG_SPACE(sizeof(int))];
		struct cmsghdr align;
	} u;
	struct cmsghdr *c;

	if (clip.server->embed.fd < 0 || fd < 0)
		return false;

	memset(&u, 0, sizeof(u));
	hdr.msg_control = u.buf;
	hdr.msg_controllen = sizeof(u.buf);
	c = CMSG_FIRSTHDR(&hdr);
	c->cmsg_level = SOL_SOCKET;
	c->cmsg_type = SCM_RIGHTS;
	c->cmsg_len = CMSG_LEN(sizeof(int));
	memcpy(CMSG_DATA(c), &fd, sizeof(fd));

	while (sendmsg(clip.server->embed.fd, &hdr, MSG_NOSIGNAL) < 0) {
		if (errno == EINTR)
			continue;
		return false;
	}
	return true;
}

/*
 * OUT, IN A SEALED DESCRIPTOR. The channel already passes one for the frame
 * and one for the keymap; a selection chunked through a 32-byte message would
 * be thousands of datagrams through the loop that pumps frames, and the
 * datagram the parent reads is sized for a name and not for a page of text.
 *
 * SEALED BEFORE IT CROSSES, because the parent maps it at the length it was
 * told: a descriptor that could still be shrunk is one whose reader can be
 * faulted after the fact, and this end has no reason to write to it again.
 */
static void offer_out(int idx, const char *text, size_t len)
{
	int fd;

	if (!clip.live || !len || clip.server->embed.fd < 0)
		return;

	fd = memfd_create("kdos-clip", MFD_CLOEXEC | MFD_ALLOW_SEALING);
	if (fd < 0) {
		wlr_log_errno(WLR_ERROR, "clipboard: memfd_create");
		return;
	}
	for (size_t off = 0; off < len;) {
		ssize_t n = write(fd, text + off, len - off);

		if (n > 0) {
			off += (size_t)n;
			continue;
		}
		if (n < 0 && errno == EINTR)
			continue;
		wlr_log_errno(WLR_ERROR, "clipboard: write");
		close(fd);
		return;
	}
	if (fcntl(fd, F_ADD_SEALS,
		  F_SEAL_SHRINK | F_SEAL_GROW | F_SEAL_WRITE) != 0) {
		wlr_log_errno(WLR_ERROR, "clipboard: seal");
		close(fd);
		return;
	}
	send_offer(idx, len, fd);
	close(fd);
}

/* ── what this cage believes the selection is ────────────────────────── */

/*
 * A CUT THAT DOES NOT SPLIT A CHARACTER. A selection longer than the channel
 * carries is cut, and a cut inside a UTF-8 sequence is a lead byte with no
 * continuation — which a reader draws as a replacement mark or refuses whole.
 */
static size_t utf8_cut(const char *s, size_t len)
{
	size_t n = len;
	unsigned char c;
	size_t need;

	while (n && ((unsigned char)s[n - 1] & 0xc0) == 0x80)
		n--;
	if (!n)
		return 0;

	c = (unsigned char)s[n - 1];
	need = c < 0x80		     ? 1
	       : (c & 0xe0) == 0xc0  ? 2
	       : (c & 0xf0) == 0xe0  ? 3
	       : (c & 0xf8) == 0xf0  ? 4
				     : 1;
	return n - 1 + need <= len ? len : n - 1;
}

static bool sel_is(int idx, const char *text, size_t len)
{
	return sel[idx].len == len && sel[idx].text &&
	       !memcmp(sel[idx].text, text, len);
}

static bool sel_hold(int idx, const char *text, size_t len)
{
	char *copy = NULL;

	if (len) {
		copy = malloc(len);
		if (!copy)
			return false;	/* what is held stands */
		memcpy(copy, text, len);
	}
	free(sel[idx].text);
	sel[idx].text = copy;
	sel[idx].len = copy ? len : 0;
	return true;
}

/* ── the session's selection, out to the guest ───────────────────────── */

/*
 * THE LOOP CLOSES THE DESCRIPTOR IT WAS GIVEN, so this closes it only while
 * the loop has not been handed it: wl_event_source_remove() closes the fd
 * itself, and a close beside it lands on whatever number was opened next.
 */
static void write_drop(struct clip_write *w)
{
	if (w->source)
		wl_event_source_remove(w->source);
	else
		close(w->fd);
	wl_list_remove(&w->link);
	free(w->buf);
	free(w);
}

/*
 * ANSWERS TRUE WHILE THERE IS MORE TO GIVE. A pipe holds a page or two and a
 * selection can be sixty-four kilobytes, so a reader that takes its time is
 * ordinary — and the only alternative to coming back for it is blocking this
 * process on a client's own scheduling.
 */
static bool write_step(struct clip_write *w)
{
	while (w->sent < w->len) {
		ssize_t n = write(w->fd, w->buf + w->sent, w->len - w->sent);

		if (n > 0) {
			w->sent += (size_t)n;
			continue;
		}
		if (n < 0 && errno == EINTR)
			continue;
		if (n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK))
			return true;
		return false;	/* the reader left, or the pipe broke */
	}
	return false;
}

static int write_writable(int fd, uint32_t mask, void *data)
{
	struct clip_write *w = data;

	(void)fd;
	(void)mask;
	if (!write_step(w))
		write_drop(w);
	return 0;
}

/*
 * A PASTE INSIDE THE GUEST, SERVED FROM WHAT THIS CAGE HOLDS.
 *
 * The bytes are copied per transfer rather than pinned, because the session
 * may replace the selection while a slow reader is still taking the last one —
 * and a paste that changed underneath the program doing it is a paste of two
 * different clipboards spliced together. `fd` BECOMES OURS the moment wlroots
 * hands it over, so every path out of here either closes it or gives it to the
 * loop, which closes it.
 */
static void pour(int idx, int fd)
{
	struct clip_write *w;
	int fl;

	if (fd < 0)
		return;
	/*
	 * AN EMPTY SELECTION IS STILL AN ANSWER. A program that asked and
	 * heard nothing cannot tell a slow compositor from an empty
	 * clipboard, and waits for a paste that is never coming.
	 */
	if (!sel[idx].len) {
		close(fd);
		return;
	}

	/*
	 * A BLOCKING DESCRIPTOR IS NOT A TRANSFER, IT IS A STALL. write() on one
	 * can never answer EAGAIN, so a guest that asks for the selection and
	 * does not drain its pipe would stop the event loop that also pumps
	 * frames — the window freezing because somebody pressed Ctrl+V in it. A
	 * paste that produces nothing is what an empty selection already
	 * produces, and it is the only answer that keeps the loop.
	 */
	fl = fcntl(fd, F_GETFL);
	if (fl < 0 || fcntl(fd, F_SETFL, fl | O_NONBLOCK) != 0) {
		close(fd);
		return;
	}

	w = calloc(1, sizeof(*w));
	if (!w) {
		close(fd);
		return;
	}
	w->buf = malloc(sel[idx].len);
	if (!w->buf) {
		free(w);
		close(fd);
		return;
	}
	memcpy(w->buf, sel[idx].text, sel[idx].len);
	w->len = sel[idx].len;
	w->fd = fd;
	wl_list_insert(&clip.writes, &w->link);

	if (!write_step(w)) {
		write_drop(w);
		return;
	}
	w->source = wl_event_loop_add_fd(the_loop(), w->fd, WL_EVENT_WRITABLE,
					 write_writable, w);
	if (!w->source)
		write_drop(w);
}

static void clip_src_send(struct wlr_data_source *source, const char *mime,
			  int32_t fd)
{
	(void)source;
	(void)mime;
	pour(0, fd);
}

static void clip_src_destroy(struct wlr_data_source *source)
{
	struct clip_source *s = wl_container_of(source, s, base);

	if (clip_src == s)
		clip_src = NULL;
	free(s);
}

static const struct wlr_data_source_impl clip_src_impl = {
	.send = clip_src_send,
	.destroy = clip_src_destroy,
};

static void prim_src_send(struct wlr_primary_selection_source *source,
			  const char *mime, int fd)
{
	(void)source;
	(void)mime;
	pour(1, fd);
}

static void prim_src_destroy(struct wlr_primary_selection_source *source)
{
	struct clip_prim_source *s = wl_container_of(source, s, base);

	if (prim_src == s)
		prim_src = NULL;
	free(s);
}

static const struct wlr_primary_selection_source_impl prim_src_impl = {
	.send = prim_src_send,
	.destroy = prim_src_destroy,
};

/*
 * THE NAME IS DUPLICATED BEFORE IT IS FILED. Every entry in the array is freed
 * by the source's destructor, so an entry left NULL by a failed copy is one
 * every reader of the list walks into — including wlroots' own.
 */
static bool mimes_add(struct wl_array *mimes)
{
	for (size_t i = 0; i < sizeof(clip_mime) / sizeof(clip_mime[0]); i++) {
		char *dup = strdup(clip_mime[i]);
		char **p;

		if (!dup)
			return false;
		p = wl_array_add(mimes, sizeof(*p));
		if (!p) {
			free(dup);
			return false;
		}
		*p = dup;
	}
	return true;
}

/*
 * THE SESSION'S SELECTION BECOMES THIS COMPOSITOR'S OWN SOURCE.
 *
 * Installed the moment the session says so and not when a paste asks, because
 * a guest reads the clipboard through the offer it was sent on focus: a cage
 * that waited to be asked would have nothing to send at the moment the guest
 * decides whether its Paste item is usable at all.
 *
 * THE OLD SOURCE IS DESTROYED BY THE SEAT, which is what makes a guest that
 * copied and then saw the session copy elsewhere see its own copy cancelled —
 * the same thing every other Wayland client sees when another takes the
 * clipboard.
 */
static void install(int idx)
{
	struct wlr_seat *seat = the_seat();
	uint32_t serial = wl_display_next_serial(clip.server->wl_display);

	if (idx) {
		struct clip_prim_source *s = calloc(1, sizeof(*s));

		if (!s)
			return;
		wlr_primary_selection_source_init(&s->base, &prim_src_impl);
		if (!mimes_add(&s->base.mime_types)) {
			wlr_primary_selection_source_destroy(&s->base);
			return;
		}
		prim_src = s;
		wlr_seat_set_primary_selection(seat, &s->base, serial);
		return;
	}

	struct clip_source *s = calloc(1, sizeof(*s));

	if (!s)
		return;
	wlr_data_source_init(&s->base, &clip_src_impl);
	if (!mimes_add(&s->base.mime_types)) {
		wlr_data_source_destroy(&s->base);
		return;
	}
	clip_src = s;
	wlr_seat_set_selection(seat, &s->base, serial);
}

/* Is what the seat is offering the copy this bridge put there? */
static bool ours(int idx)
{
	struct wlr_seat *seat = the_seat();

	if (idx)
		return prim_src &&
		       seat->primary_selection_source == &prim_src->base;
	return clip_src && seat->selection_source == &clip_src->base;
}

/* ── the guest's copy, in ────────────────────────────────────────────── */

/* The loop closes the descriptor it was given — see write_drop(). */
static void read_drop(struct clip_read *r)
{
	if (sel[r->idx].in == r)
		sel[r->idx].in = NULL;
	wl_list_remove(&r->src_destroy.link);
	if (r->source)
		wl_event_source_remove(r->source);
	else
		close(r->fd);
	free(r->buf);
	free(r);
}

/*
 * THE COPY IS COMPLETE — offer it, and remember it.
 *
 * A TRANSFER THAT PRODUCED NOTHING IS NOT A COPY. A guest that offered a mime
 * type and then wrote no bytes, or whose source went away mid-transfer, must
 * not empty the session's clipboard: the selection stays what it was.
 */
static void read_done(struct clip_read *r)
{
	int idx = r->idx;

	if (r->len && !sel_is(idx, r->buf, r->len) &&
	    sel_hold(idx, r->buf, r->len))
		offer_out(idx, sel[idx].text, sel[idx].len);
	read_drop(r);
}

static int read_readable(int fd, uint32_t mask, void *data)
{
	struct clip_read *r = data;

	(void)mask;
	for (;;) {
		char buf[CLIP_CHUNK];
		size_t room = KEMBED_CLIP_MAX - r->len;
		ssize_t n;

		if (!room) {
			/*
			 * CUT WHERE THE SESSION WOULD CUT IT, and only here:
			 * the parent's own clipboard truncates at
			 * KEMBED_CLIP_MAX, so reading past it is a copy of
			 * bytes nothing downstream keeps, and the reader
			 * closing early is what tells the guest the transfer
			 * is over. A COMPLETE transfer is handed over byte for
			 * byte — `text/plain` with no charset need not be
			 * UTF-8, and trimming a trailing sequence out of one
			 * that is not would be dropping a character the guest
			 * did copy.
			 */
			r->len = utf8_cut(r->buf, r->len);
			read_done(r);
			return 0;
		}
		if (room > sizeof(buf))
			room = sizeof(buf);

		n = read(fd, buf, room);
		if (n == 0) {
			read_done(r);
			return 0;
		}
		if (n < 0) {
			if (errno == EINTR)
				continue;
			if (errno == EAGAIN || errno == EWOULDBLOCK)
				return 0;
			read_drop(r);
			return 0;
		}

		char *grown = realloc(r->buf, r->len + (size_t)n);

		if (!grown) {
			read_drop(r);
			return 0;
		}
		r->buf = grown;
		memcpy(r->buf + r->len, buf, (size_t)n);
		r->len += (size_t)n;
	}
}

/*
 * THE SOURCE WENT AWAY WHILE IT WAS BEING READ. The guest exited, or replaced
 * its own selection; either way the bytes are partial and a partial paste is
 * worse than none. The listener removes itself here, because wlroots requires
 * a source's destroy signal to be empty by the time it returns.
 */
static void read_src_destroy(struct wl_listener *listener, void *data)
{
	struct clip_read *r = wl_container_of(listener, r, src_destroy);

	(void)data;
	read_drop(r);
}

/*
 * ONE TRANSFER PER SELECTION. A guest that copies twice has already replaced
 * its own source, so the first read is draining a pipe nothing will write to
 * again — the new copy cancels it rather than racing it to the offer.
 *
 * THE WRITE END BECOMES THE GUEST'S AND WLROOTS CLOSES IT: a source's send is
 * defined to hand the descriptor over and let go of it, so closing it here
 * would close a descriptor this process has since opened for something else.
 */
static void read_begin(int idx, const char *mime)
{
	struct wlr_seat *seat = the_seat();
	struct clip_read *r;
	int p[2];

	if (sel[idx].in)
		read_drop(sel[idx].in);

	if (pipe2(p, O_CLOEXEC | O_NONBLOCK) != 0) {
		wlr_log_errno(WLR_ERROR, "clipboard: pipe2");
		return;
	}
	r = calloc(1, sizeof(*r));
	if (!r) {
		close(p[0]);
		close(p[1]);
		return;
	}
	r->fd = p[0];
	r->idx = idx;
	wl_list_init(&r->src_destroy.link);
	r->src_destroy.notify = read_src_destroy;
	r->source = wl_event_loop_add_fd(the_loop(), r->fd, WL_EVENT_READABLE,
					 read_readable, r);
	if (!r->source) {
		close(p[0]);
		close(p[1]);
		free(r);
		return;
	}
	sel[idx].in = r;

	if (idx) {
		wl_signal_add(&seat->primary_selection_source->events.destroy,
			      &r->src_destroy);
		wlr_primary_selection_source_send(
			seat->primary_selection_source, mime, p[1]);
	} else {
		wl_signal_add(&seat->selection_source->events.destroy,
			      &r->src_destroy);
		wlr_data_source_send(seat->selection_source, mime, p[1]);
	}
}

static const char *pick_mime(const struct wl_array *mimes)
{
	for (size_t i = 0; i < sizeof(clip_mime) / sizeof(clip_mime[0]); i++) {
		char **p;

		wl_array_for_each (p, mimes)
			if (*p && !strcmp(*p, clip_mime[i]))
				return clip_mime[i];
	}
	return NULL;
}

/*
 * A GUEST TOOK THE SELECTION, so read what it holds and hand it to the
 * session. The read is started here and not at the session's paste, because
 * the source belongs to the guest: one that has exited, or that has copied
 * something else since, cannot answer a question asked later — and a paste
 * that has to wait for a round trip through another process is a paste that
 * can hang the window doing it.
 *
 * A CLEARED SELECTION IS NOT FORWARDED. A guest's source dies with the
 * program that made it, and a session whose clipboard emptied because a
 * window closed is one that has thrown away the copy a person made in it.
 */
static void handle_set_selection(struct wl_listener *listener, void *data)
{
	struct wlr_data_source *src = the_seat()->selection_source;
	const char *mime;

	(void)listener;
	(void)data;
	/*
	 * A SEAT THAT OFFERS NOTHING CANNOT BE PASTED FROM. A guest's own source
	 * dies with the document window that made it, with the toolkit clearing
	 * the clipboard, or with an Xwayland owner disowning CLIPBOARD; wlroots
	 * then leaves the seat empty. The session's copy is still here, so it
	 * goes back on rather than leaving the guest unable to paste anything
	 * until somebody copies again somewhere else in the session.
	 */
	if (!src) {
		if (sel[0].len)
			install(0);
		return;
	}
	if (src->impl == &clip_src_impl)
		return;		/* the copy this bridge installed */
	mime = pick_mime(&src->mime_types);
	if (!mime)
		return;		/* an image, and nowhere to put it */
	read_begin(0, mime);
}

static void handle_set_primary(struct wl_listener *listener, void *data)
{
	struct wlr_primary_selection_source *src =
		the_seat()->primary_selection_source;
	const char *mime;

	(void)listener;
	(void)data;
	if (!src) {
		if (sel[1].len)
			install(1);
		return;
	}
	if (src->impl == &prim_src_impl)
		return;
	mime = pick_mime(&src->mime_types);
	if (!mime)
		return;
	read_begin(1, mime);
}

void clipboard_take(struct cg_server *server, const KembedMsg *m, int fd)
{
	int idx = m->a ? 1 : 0;
	struct stat st;
	int seals;
	void *map;

	if (!clip.live || server != clip.server)
		return;

	if (m->b < 0 || (size_t)m->b > KEMBED_CLIP_MAX) {
		wlr_log(WLR_ERROR, "clipboard: a selection of an impossible "
				   "length");
		return;
	}

	/*
	 * A CLEARED SELECTION IS A LENGTH OF ZERO, and it clears only what this
	 * bridge installed: a guest's own source is the guest's, and dropping
	 * it here would take a copy away from the program that made it.
	 */
	if (m->b == 0) {
		sel_hold(idx, NULL, 0);
		if (ours(idx)) {
			uint32_t serial =
				wl_display_next_serial(server->wl_display);

			if (idx)
				wlr_seat_set_primary_selection(the_seat(), NULL,
							       serial);
			else
				wlr_seat_set_selection(the_seat(), NULL,
						       serial);
		}
		return;
	}
	if (fd < 0) {
		wlr_log(WLR_ERROR, "clipboard: a selection with no descriptor");
		return;
	}

	size_t len = (size_t)m->b;

	/*
	 * A DESCRIPTOR FROM ANOTHER PROCESS IS MEASURED BEFORE IT IS MAPPED.
	 * The length in the message is a claim; the file is the fact. A
	 * mapping longer than the file faults on the byte past its end, and a
	 * file that can still SHRINK faults after the check — so the seal is
	 * required as well as the size, and anything that is not a plain file
	 * with the room the message promised is dropped rather than read.
	 */
	if (fstat(fd, &st) != 0 || !S_ISREG(st.st_mode) ||
	    (size_t)st.st_size < len) {
		wlr_log(WLR_ERROR, "clipboard: a selection that is not a file "
				   "of the length it claims");
		return;
	}
	seals = fcntl(fd, F_GET_SEALS);
	if (seals < 0 || !(seals & F_SEAL_SHRINK)) {
		wlr_log(WLR_ERROR, "clipboard: a selection that can still be "
				   "shrunk under the mapping");
		return;
	}

	map = mmap(NULL, len, PROT_READ, MAP_PRIVATE, fd, 0);
	if (map == MAP_FAILED) {
		wlr_log_errno(WLR_ERROR, "clipboard: mmap");
		return;
	}
	if (!sel_is(idx, map, len) && sel_hold(idx, map, len))
		install(idx);
	munmap(map, len);
}

void clipboard_init(struct cg_server *server)
{
	if (!server->embed.active || clip.live)
		return;

	clip.server = server;
	wl_list_init(&clip.writes);

	/*
	 * A PIPE WHOSE READER LEFT KILLS THE WRITER, and a pipe has no
	 * MSG_NOSIGNAL: a guest that asks for the selection and exits before
	 * taking it would take this compositor — and the window — with it.
	 *
	 * AN IGNORED DISPOSITION IS INHERITED ACROSS AN EXEC, so this is set
	 * after the guest is forked and no earlier. Every process this
	 * compositor execs from here on carries it — Xwayland, which wlroots
	 * starts lazily on the first X client, included; Xwayland installs the
	 * same disposition itself, so nothing it runs is changed by it.
	 */
	signal(SIGPIPE, SIG_IGN);

	clip.set_selection.notify = handle_set_selection;
	wl_signal_add(&server->seat->seat->events.set_selection,
		      &clip.set_selection);
	clip.set_primary.notify = handle_set_primary;
	wl_signal_add(&server->seat->seat->events.set_primary_selection,
		      &clip.set_primary);
	clip.live = true;
}

void clipboard_finish(struct cg_server *server)
{
	struct clip_write *w, *wtmp;

	if (!clip.live || server != clip.server)
		return;

	clip.live = false;
	wl_list_remove(&clip.set_selection.link);
	wl_list_remove(&clip.set_primary.link);

	for (int i = 0; i < 2; i++) {
		if (sel[i].in)
			read_drop(sel[i].in);
		free(sel[i].text);
		sel[i].text = NULL;
		sel[i].len = 0;
	}
	wl_list_for_each_safe (w, wtmp, &clip.writes, link)
		write_drop(w);

	/*
	 * THE SEAT OUTLIVES THIS, so a source it would call back into has to
	 * go before the bridge does: the display is still up here and the seat
	 * is destroyed several steps later.
	 */
	if (ours(0))
		wlr_seat_set_selection(
			server->seat->seat, NULL,
			wl_display_next_serial(server->wl_display));
	if (ours(1))
		wlr_seat_set_primary_selection(
			server->seat->seat, NULL,
			wl_display_next_serial(server->wl_display));
}
