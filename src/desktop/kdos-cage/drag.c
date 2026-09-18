/*
 * kdos-cage --embed — a drag, across the private channel.
 *
 * THE SESSION OWNS THE DRAG AND THIS CAGE PLAYS IT INTO THE GUEST. On the cell
 * desktop the pointer belongs to the console: it hit-tests the windows, it
 * draws the arrow and it holds the payload from the pick-up to the release, so
 * a drag that crossed a boxed application would end as cancelled unless
 * something inside the cage turned those five messages back into the
 * `wl_data_device` events a toolkit understands. That is this file. Both
 * directions are here — KEMBED_DRAG_ENTER through KEMBED_DROP are replayed on
 * this compositor's seat, and a drag the GUEST begins is read out and offered
 * up in KEMBED_DRAG_OFFER so the session can carry it to another window.
 *
 * TEXT AND FILE NAMES, WHICH IS WHAT THE SESSION CARRIES. `text/plain` and
 * `text/uri-list` are the two types kdos-con accepts at all; a guest dragging
 * an image out has begun a drag this desktop has nowhere to put, and the offer
 * is simply not made rather than made with bytes nothing can read.
 *
 * THE DRAG IS A REAL POINTER GRAB, NOT A SYNTHESISED EVENT STREAM. wlroots'
 * drag machinery decides which surface has the offer, sends the enter, motion
 * and leave, and refuses the drop where the guest declined the type — none of
 * which can be faked from outside without reimplementing it. So the bridge
 * creates a `wlr_drag`, starts a pointer grab with it, and drives that grab:
 * the press that arms it is swallowed by the grab itself and never reaches the
 * guest as a click, and the release is what wlroots reads as the drop.
 *
 * THE PAYLOAD ARRIVES AT THE DROP AND NOT AT THE ENTER, because that is what
 * the session sends: a drag crossing six windows would otherwise hand its bytes
 * to all six. A guest that asks for the data before then — a toolkit that
 * previews what is being dragged — is not refused and is not answered either:
 * its pipe is held until the drop fills it, and closed unanswered if the drag
 * ends without one. A refusal would read to the guest as an empty file.
 *
 * AND THE PAYLOAD OUTLIVES THE DRAG. wlroots tears the drag down in the same
 * call that sends the drop, while the OFFER stays in the guest's hands —
 * because a dropped drag is exactly when a toolkit asks for the bytes. They
 * belong to the SOURCE's lifetime, which ends when the offer does.
 *
 * NOTHING IS TRANSFERRED WHILE THIS PROCESS WAITS FOR IT. Every transfer is a
 * file descriptor on the event loop, for the reason clipboard.c states: a guest
 * that asks for the payload and never reads it would otherwise stop the loop
 * that also pumps frames.
 */

/* memfd_create and pipe2. Guarded: the compile gate puts the flag on the
 * command line, and an unconditional define collides with it under -Werror. */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <errno.h>
#include <fcntl.h>
#include <linux/input-event-codes.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <unistd.h>

#include <wayland-server-core.h>
#include <wlr/types/wlr_data_device.h>
#include <wlr/types/wlr_seat.h>
#include <wlr/util/log.h>

#include "drag.h"
#include "kembed.h"
#include "seat.h"
#include "server.h"
#include "view.h"

/*
 * THE TWO TYPES THIS DESKTOP CARRIES, in preference order. kdos-con's
 * `on_drag_start` refuses everything else outright, so offering a third here
 * would be a drag the session drops on the floor with the guest believing it
 * began.
 */
static const char *const drag_mime[] = {
	"text/uri-list",
	"text/plain",
};

/* How much is taken from a guest's pipe in one call. See CLIP_CHUNK. */
#define DRAG_CHUNK 4096

/* The source this compositor installs for a drag the PARENT is carrying. */
struct drag_source {
	struct wlr_data_source base;
};

/* A guest asked for the payload: the pipe it is owed, and how far along. */
struct drag_write {
	struct wl_list link;
	struct wl_event_source *source;
	int fd;
	size_t sent;
};

/* The guest began a drag, and this is the transfer reading what it holds. */
struct drag_read {
	struct wl_event_source *source;
	struct wl_listener src_destroy;
	int fd;
	char mime[64];
	char *buf;
	size_t len;
};

static const struct wlr_data_source_impl drag_src_impl;

/*
 * ONE CAGE IS ONE PROCESS AND ONE CHANNEL, so the bridge rides beside the
 * server rather than inside it — the same shape clipboard.c uses, and for the
 * same reason.
 */
static struct {
	struct cg_server *server;
	struct wl_listener start_drag;
	bool live;

	/* The parent's drag, being played into the guest. */
	struct wlr_drag *in;
	struct wl_listener in_destroy;
	struct drag_source *src;
	struct cg_view *over;		/* the window it was last aimed at  */
	char *data;			/* the payload, once the drop lands */
	size_t len;
	bool dropped;
	struct wl_list writes;		/* pipes waiting for it, and after  */

	/* The guest's drag, being read out. */
	struct drag_read *out;
} dg;

static struct wlr_seat *the_seat(void)
{
	return dg.server->seat->seat;
}

static struct wl_event_loop *the_loop(void)
{
	return wl_display_get_event_loop(dg.server->wl_display);
}

/* ── the payload, out to the guest ───────────────────────────────────── */

/*
 * THE LOOP CLOSES THE DESCRIPTOR IT WAS GIVEN — wl_event_source_remove() closes
 * the fd itself, and a close beside it lands on whatever number was opened
 * next. See clipboard.c's write_drop().
 */
static void write_drop(struct drag_write *w)
{
	if (w->source)
		wl_event_source_remove(w->source);
	else
		close(w->fd);
	wl_list_remove(&w->link);
	free(w);
}

/* True while there is more to give; false when the transfer is over or the
 * reader went away. */
static bool write_step(struct drag_write *w)
{
	while (w->sent < dg.len) {
		ssize_t n = write(w->fd, dg.data + w->sent, dg.len - w->sent);

		if (n > 0) {
			w->sent += (size_t)n;
			continue;
		}
		if (n < 0 && errno == EINTR)
			continue;
		if (n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK))
			return true;
		return false;
	}
	return false;
}

static int write_writable(int fd, uint32_t mask, void *data)
{
	struct drag_write *w = data;

	(void)fd;
	(void)mask;
	if (!write_step(w))
		write_drop(w);
	return 0;
}

/* Arm a held pipe now that there are bytes for it. */
static void write_arm(struct drag_write *w)
{
	if (w->source)
		return;
	if (!write_step(w)) {
		write_drop(w);
		return;
	}
	w->source = wl_event_loop_add_fd(the_loop(), w->fd, WL_EVENT_WRITABLE,
					 write_writable, w);
	if (!w->source)
		write_drop(w);
}

/*
 * A GUEST ASKED FOR WHAT IS BEING DRAGGED. `fd` BECOMES OURS the moment wlroots
 * hands it over, so every path out of here either closes it or gives it to the
 * loop, which closes it.
 *
 * ASKED BEFORE THE DROP, THE PIPE IS HELD. The session sends the bytes with
 * KEMBED_DROP and not a message earlier, and a transfer answered with nothing
 * reads to the guest as a drag carrying an empty file — which is worse than one
 * that takes a moment.
 */
static void drag_src_send(struct wlr_data_source *source, const char *mime,
			  int32_t fd)
{
	struct drag_write *w;
	int fl;

	(void)source;
	(void)mime;
	if (fd < 0)
		return;
	/*
	 * A BLOCKING DESCRIPTOR IS NOT A TRANSFER, IT IS A STALL — see
	 * clipboard.c's pour(). write() on one can never answer EAGAIN, so a
	 * guest that asks and does not drain would stop the loop that pumps
	 * frames.
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
	w->fd = fd;
	wl_list_insert(&dg.writes, &w->link);
	if (dg.dropped)
		write_arm(w);
}

/*
 * THE SOURCE IS WHAT THE PAYLOAD BELONGS TO, AND NOT THE DRAG.
 *
 * wlroots tears the drag down the moment the button comes up — the drop is sent
 * and `drag_destroy` runs in the same call — while the OFFER stays in the
 * guest's hands, because a dropped drag is exactly the point at which a toolkit
 * calls `wl_data_offer.receive`. Bytes freed with the drag would leave that
 * transfer holding a pipe nothing will ever write to, which reads to the guest
 * as a drop carrying an empty file. They are freed here instead, when the
 * source dies and nothing can ask for them again.
 */
static void drag_src_destroy(struct wlr_data_source *source)
{
	struct drag_source *s = wl_container_of(source, s, base);
	struct drag_write *w, *tmp;

	if (dg.src == s) {
		dg.src = NULL;
		wl_list_for_each_safe (w, tmp, &dg.writes, link)
			write_drop(w);
		free(dg.data);
		dg.data = NULL;
		dg.len = 0;
		dg.dropped = false;
	}
	free(s);
}

/*
 * `dnd_finish` AND `dnd_action` MUST BOTH EXIST OR THERE IS NO DROP.
 *
 * wlroots reads a source with no `dnd_finish` as one that cannot take part in a
 * drag at all and destroys it on the release; and a target's `set_actions` is
 * delivered through `dnd_action`, which is half of the test the drop itself is
 * gated on. A source missing either is a drag that always reads as cancelled,
 * which is the shape this bridge exists to fix.
 */
static void drag_src_accept(struct wlr_data_source *source, uint32_t serial,
			    const char *mime)
{
	(void)source;
	(void)serial;
	(void)mime;
}

static void drag_src_dnd_drop(struct wlr_data_source *source)
{
	(void)source;
}

/*
 * NOTHING HERE, AND NOT BECAUSE THERE IS NOTHING TO DO.
 *
 * Its PRESENCE is what the drop is gated on — wlroots reads a source without a
 * `dnd_finish` as one that cannot take part in a drag at all and destroys it on
 * the release — and its BODY must not destroy the source: wlroots does that
 * itself when the offer goes, and a destroy from inside this callback frees the
 * offer underneath the very call that is running, which then uses it.
 */
static void drag_src_dnd_finish(struct wlr_data_source *source)
{
	(void)source;
}

static void drag_src_dnd_action(struct wlr_data_source *source,
				enum wl_data_device_manager_dnd_action action)
{
	(void)source;
	(void)action;
}

static const struct wlr_data_source_impl drag_src_impl = {
	.accept = drag_src_accept,
	.send = drag_src_send,
	.destroy = drag_src_destroy,
	.dnd_drop = drag_src_dnd_drop,
	.dnd_finish = drag_src_dnd_finish,
	.dnd_action = drag_src_dnd_action,
};

/*
 * THE NAME IS DUPLICATED BEFORE IT IS FILED, and an entry left NULL by a failed
 * copy is one every reader of the list walks into — including wlroots' own.
 *
 * THE SESSION'S TYPE FIRST AND THE OTHER BESIDE IT. A drag carrying a file name
 * is `text/uri-list` to a file manager and a line of text to an editor, and a
 * toolkit picks whichever it can take; offering only the one the session named
 * would make an editor refuse a drop it can perfectly well accept.
 */
static bool mimes_add(struct wl_array *mimes, const char *first)
{
	for (size_t i = 0; i <= sizeof(drag_mime) / sizeof(drag_mime[0]); i++) {
		const char *name = i ? drag_mime[i - 1] : first;
		char *dup;
		char **p;

		if (i && !strcmp(name, first))
			continue;	/* already offered, at the head */
		dup = strdup(name);
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

/* ── the parent's drag, in ───────────────────────────────────────────── */

/*
 * THE DRAG WENT AWAY — because the guest refused the type, because the surface
 * it was over was destroyed, because the release dropped it, or because this
 * bridge ended it.
 *
 * THE PAYLOAD IS NOT FREED HERE. It outlives the drag by design; see
 * drag_src_destroy(). What is dropped is everything that can only be answered
 * while a drag is running — the pipes waiting for a payload that is no longer
 * coming, which without this leave a guest blocked on a read for as long as it
 * lives — and only where the drag ended WITHOUT a drop.
 */
static void in_forget(void)
{
	wl_list_remove(&dg.in_destroy.link);
	wl_list_init(&dg.in_destroy.link);
	dg.in = NULL;
	dg.over = NULL;

	if (!dg.dropped) {
		struct drag_write *w, *tmp;

		wl_list_for_each_safe (w, tmp, &dg.writes, link)
			write_drop(w);
		free(dg.data);
		dg.data = NULL;
		dg.len = 0;
	}
}

static void in_destroy(struct wl_listener *listener, void *data)
{
	(void)listener;
	(void)data;
	in_forget();
}

void drag_leave(struct cg_server *server)
{
	(void)server;
	if (!dg.live || !dg.in)
		return;
	/*
	 * THE SOURCE IS DESTROYED AND THE DRAG FOLLOWS IT. wlroots ends a drag
	 * whose source died, which sends the guest the `wl_data_device.leave`
	 * this message means — cancelling the grab directly would leave the
	 * offer in the guest's hands with nothing behind it.
	 */
	if (dg.src)
		wlr_data_source_destroy(&dg.src->base);
	else
		in_forget();
}

void drag_enter(struct cg_server *server, struct cg_view *view, double x,
		double y, const char *mime, uint32_t time_msec)
{
	struct wlr_seat *seat;
	struct wlr_seat_client *client;
	struct drag_source *s;
	struct wlr_drag *drag;
	uint32_t serial;

	if (!dg.live || !view || !view->wlr_surface || !mime || !*mime)
		return;
	/* A second enter with no leave between is the parent having moved the
	 * drag to another window: end the old one first, or the guest holds two
	 * offers and the seat holds two grabs. */
	if (dg.in)
		drag_leave(server);

	seat = the_seat();
	/*
	 * THE DRAG IS ORIGINATED FROM THE GUEST'S OWN SEAT CLIENT, because
	 * wlroots requires one and this compositor has no client of its own to
	 * name. It is only the origin: the offer goes to whichever client the
	 * pointer is over, which inside a cage is the same one.
	 */
	client = wlr_seat_client_for_wl_client(
		seat, wl_resource_get_client(view->wlr_surface->resource));
	if (!client)
		return;

	s = calloc(1, sizeof(*s));
	if (!s)
		return;
	wlr_data_source_init(&s->base, &drag_src_impl);
	if (!mimes_add(&s->base.mime_types, mime)) {
		wlr_data_source_destroy(&s->base);
		return;
	}

	drag = wlr_drag_create(client, &s->base, NULL);
	if (!drag) {
		wlr_data_source_destroy(&s->base);
		return;
	}

	dg.src = s;
	dg.in = drag;
	dg.over = view;
	dg.dropped = false;
	dg.in_destroy.notify = in_destroy;
	wl_signal_add(&drag->events.destroy, &dg.in_destroy);

	serial = wl_display_next_serial(server->wl_display);
	wlr_seat_start_pointer_drag(seat, drag, serial);
	/*
	 * THE PRESS THAT ARMS THE RELEASE, and the grab is already installed so
	 * it never reaches the guest as a click. wlroots reads a drop as "the
	 * button the grab started with came back up", and a grab with no button
	 * recorded can never see one — the release would destroy the drag as a
	 * cancel instead.
	 */
	wlr_seat_pointer_notify_button(seat, time_msec, BTN_LEFT,
				       WL_POINTER_BUTTON_STATE_PRESSED);
	seat_embed_drag_motion(server->seat, view, x, y, time_msec);
}

void drag_motion(struct cg_server *server, struct cg_view *view, double x,
		 double y, uint32_t time_msec)
{
	if (!dg.live || !dg.in || !view)
		return;
	dg.over = view;
	seat_embed_drag_motion(server->seat, view, x, y, time_msec);
}

/*
 * THE BYTES, OUT OF THE DESCRIPTOR THEY ARRIVED IN. The descriptor stays the
 * CALLER'S — the dispatch that read the message closes every one it received —
 * so this maps and copies within the call.
 */
static bool take_payload(const KembedMsg *m, int fd)
{
	size_t len = m->c > 0 ? (size_t)m->c : 0;
	struct stat st;
	void *map;

	free(dg.data);
	dg.data = NULL;
	dg.len = 0;

	if (fd < 0 || !len || len > KEMBED_CLIP_MAX)
		return false;
	/*
	 * THE LENGTH IS CHECKED AGAINST THE DESCRIPTOR, not taken from the
	 * message. A peer that names more than it sent would have this process
	 * map past the end of the file and fault on the read.
	 */
	if (fstat(fd, &st) != 0 || (size_t)st.st_size < len)
		return false;

	map = mmap(NULL, len, PROT_READ, MAP_PRIVATE, fd, 0);
	if (map == MAP_FAILED)
		return false;
	dg.data = malloc(len);
	if (dg.data) {
		memcpy(dg.data, map, len);
		dg.len = len;
	}
	munmap(map, len);
	return dg.data != NULL;
}

void drag_drop(struct cg_server *server, struct cg_view *view, double x,
	       double y, const KembedMsg *m, int fd, uint32_t time_msec)
{
	struct drag_write *w, *tmp;

	if (!dg.live || !dg.in)
		return;
	if (!take_payload(m, fd)) {
		/* Nothing to give. A drop with no payload is a drag that ended
		 * without one, which is what a cancel already means. */
		drag_leave(server);
		return;
	}
	dg.dropped = true;

	if (view)
		seat_embed_drag_motion(server->seat, view, x, y, time_msec);
	/* Anything that asked early is owed the bytes now. */
	wl_list_for_each_safe (w, tmp, &dg.writes, link)
		write_arm(w);
	/*
	 * AND THE RELEASE IS THE DROP. wlroots sends `wl_data_device.drop` if
	 * the guest accepted a type and set an action, and destroys the drag as
	 * a cancel if it did not — which is the guest refusing the payload and
	 * is the right answer for it.
	 */
	wlr_seat_pointer_notify_button(the_seat(), time_msec, BTN_LEFT,
				       WL_POINTER_BUTTON_STATE_RELEASED);
}

/* ── the guest's drag, out ───────────────────────────────────────────── */

static bool send_offer(size_t len, int fd, const char *mime)
{
	KembedMsg m = {
		.magic = KEMBED_MAGIC,
		.op = KEMBED_DRAG_OFFER,
		.b = (int32_t)len,
	};
	char buf[sizeof(m) + 64];
	size_t n = strlen(mime) + 1;
	struct iovec iov;
	struct msghdr hdr = { .msg_iov = &iov, .msg_iovlen = 1 };
	union {
		char buf[CMSG_SPACE(sizeof(int))];
		struct cmsghdr align;
	} u;
	struct cmsghdr *c;

	if (dg.server->embed.fd < 0 || fd < 0 || n > 64)
		return false;

	/* THE TYPE FOLLOWS THE STRUCT, with the datagram boundary as its end —
	 * the socket is SOCK_SEQPACKET, so no length field is needed. */
	memcpy(buf, &m, sizeof(m));
	memcpy(buf + sizeof(m), mime, n);
	iov.iov_base = buf;
	iov.iov_len = sizeof(m) + n;

	memset(&u, 0, sizeof(u));
	hdr.msg_control = u.buf;
	hdr.msg_controllen = sizeof(u.buf);
	c = CMSG_FIRSTHDR(&hdr);
	c->cmsg_level = SOL_SOCKET;
	c->cmsg_type = SCM_RIGHTS;
	c->cmsg_len = CMSG_LEN(sizeof(int));
	memcpy(CMSG_DATA(c), &fd, sizeof(fd));

	while (sendmsg(dg.server->embed.fd, &hdr, MSG_NOSIGNAL) < 0) {
		if (errno == EINTR)
			continue;
		return false;
	}
	return true;
}

/*
 * OUT, IN A SEALED DESCRIPTOR — the same carrier the selection uses, and for
 * the same reasons. See clipboard.c's offer_out().
 */
static void offer_out(const char *text, size_t len, const char *mime)
{
	int fd;

	if (!dg.live || !len || dg.server->embed.fd < 0)
		return;

	fd = memfd_create("kdos-drag", MFD_CLOEXEC | MFD_ALLOW_SEALING);
	if (fd < 0) {
		wlr_log_errno(WLR_ERROR, "drag: memfd_create");
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
		wlr_log_errno(WLR_ERROR, "drag: write");
		close(fd);
		return;
	}
	if (fcntl(fd, F_ADD_SEALS,
		  F_SEAL_SHRINK | F_SEAL_GROW | F_SEAL_WRITE) != 0) {
		wlr_log_errno(WLR_ERROR, "drag: seal");
		close(fd);
		return;
	}
	send_offer(len, fd, mime);
	close(fd);
}

static void read_drop(struct drag_read *r)
{
	if (dg.out == r)
		dg.out = NULL;
	wl_list_remove(&r->src_destroy.link);
	if (r->source)
		wl_event_source_remove(r->source);
	else
		close(r->fd);
	free(r->buf);
	free(r);
}

/*
 * THE GUEST'S PAYLOAD IS COMPLETE — offer it to the session, which from here
 * owns the drag and will play it back into whichever window it is released
 * over, this one included.
 *
 * A TRANSFER THAT PRODUCED NOTHING IS NOT A DRAG. A guest that offered a type
 * and wrote no bytes has begun something the session cannot carry, and an offer
 * of nothing would be a pointer the person cannot put down.
 */
static void read_done(struct drag_read *r)
{
	if (r->len)
		offer_out(r->buf, r->len, r->mime);
	read_drop(r);
}

static int read_readable(int fd, uint32_t mask, void *data)
{
	struct drag_read *r = data;

	(void)mask;
	for (;;) {
		char buf[DRAG_CHUNK];
		size_t room = KEMBED_CLIP_MAX - r->len;
		ssize_t n;
		char *grown;

		if (!room) {
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
		grown = realloc(r->buf, r->len + (size_t)n);
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
 * THE SOURCE WENT AWAY WHILE IT WAS BEING READ — the guest cancelled its own
 * drag, or exited. The listener removes itself here, because wlroots requires a
 * source's destroy signal to be empty by the time it returns.
 */
static void read_src_destroy(struct wl_listener *listener, void *data)
{
	struct drag_read *r = wl_container_of(listener, r, src_destroy);

	(void)data;
	read_drop(r);
}

static const char *pick_mime(const struct wl_array *mimes)
{
	for (size_t i = 0; i < sizeof(drag_mime) / sizeof(drag_mime[0]); i++) {
		char **p;

		wl_array_for_each (p, mimes)
			if (*p && !strcmp(*p, drag_mime[i]))
				return drag_mime[i];
	}
	return NULL;
}

/*
 * A GUEST BEGAN A DRAG.
 *
 * READ AT THE START AND NOT AT THE RELEASE, because the session has to be
 * carrying something from the moment the pointer leaves the window: a drag
 * whose payload were fetched at the drop would need a round trip through
 * another process with a person's hand already off the button. It is the same
 * reason clipboard.c reads a copy when it is made.
 *
 * A DRAG THIS BRIDGE ITSELF STARTED IS NOT READ BACK. The parent is already
 * carrying those bytes, and offering them up again would be the session
 * replacing its own drag with a copy of it at the moment the pointer entered a
 * guest.
 */
static void handle_start_drag(struct wl_listener *listener, void *data)
{
	struct wlr_drag *drag = data;
	struct drag_read *r;
	const char *mime;
	int p[2];

	(void)listener;
	if (!dg.live || !drag->source)
		return;
	if (drag->source->impl == &drag_src_impl)
		return;
	mime = pick_mime(&drag->source->mime_types);
	if (!mime)
		return;		/* something this desktop cannot carry */

	if (dg.out)
		read_drop(dg.out);

	if (pipe2(p, O_CLOEXEC | O_NONBLOCK) != 0) {
		wlr_log_errno(WLR_ERROR, "drag: pipe2");
		return;
	}
	r = calloc(1, sizeof(*r));
	if (!r) {
		close(p[0]);
		close(p[1]);
		return;
	}
	r->fd = p[0];
	snprintf(r->mime, sizeof(r->mime), "%s", mime);
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
	dg.out = r;
	wl_signal_add(&drag->source->events.destroy, &r->src_destroy);
	/* THE WRITE END BECOMES THE GUEST'S AND WLROOTS CLOSES IT: a source's
	 * send hands the descriptor over and lets go of it. */
	wlr_data_source_send(drag->source, mime, p[1]);
}

/* ── setup ───────────────────────────────────────────────────────────── */

void drag_init(struct cg_server *server)
{
	wl_list_init(&dg.writes);
	wl_list_init(&dg.in_destroy.link);
	if (!server->embed.embedded)
		return;

	dg.server = server;
	dg.live = true;
	dg.start_drag.notify = handle_start_drag;
	wl_signal_add(&server->seat->seat->events.start_drag, &dg.start_drag);
}

void drag_finish(struct cg_server *server)
{
	(void)server;
	if (!dg.live)
		return;
	dg.live = false;
	wl_list_remove(&dg.start_drag.link);
	if (dg.out)
		read_drop(dg.out);
	/*
	 * THE SOURCE IS WHAT OWNS THE PAYLOAD, so ending the source is what
	 * frees it — see drag_src_destroy(). Destroying it also ends the drag
	 * and with it the pointer grab, which is what this call is for: a
	 * compositor shutting down with a grab still installed is one whose
	 * seat outlives its own teardown.
	 */
	if (dg.src)
		wlr_data_source_destroy(&dg.src->base);
	else
		in_forget();
	dg.server = NULL;
}
