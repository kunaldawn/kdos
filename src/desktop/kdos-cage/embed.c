/*
 * kdos-cage --embed — the guest's pixels, out to the cell desktop.
 *
 * See cage.c's header for what this mode is. The whole mechanism is that
 * wlroots' HEADLESS backend renders into a buffer in memory rather than onto a
 * screen, and the PIXMAN renderer can be asked for a pointer to those bytes.
 * Nothing is scanned out; the parent takes the bytes and puts them on whatever
 * display it has.
 *
 * TWO FRAMES IN ONE MAPPING. The child renders into the half the parent is not
 * reading and then flips, so a frame is never half old and half new. Single
 * buffering would tear on every commit, and on a photograph that reads as the
 * compositor being broken rather than as the timing artefact it is.
 *
 * ONE OUTPUT, ONE MAPPING AND ONE `win` PER MAPPED TOPLEVEL. A scene output
 * renders only what is inside its own layout box, so toplevels placed in
 * disjoint boxes reach the parent as separate pictures — which is what lets an
 * application that maps five windows be five windows. Sharing one output
 * composites them into one framebuffer before the parent ever sees them, and
 * nothing downstream can take them apart again. It is also where the work goes:
 * a window nobody is looking at has its own scene output and produces no frames
 * at all.
 *
 * THE DESCRIPTOR IS THE ONE THING THIS CHANNEL HAS THAT THE PUBLISHED ONES
 * MUST NOT. It is passed once per size, from child to parent, over a socketpair
 * inherited across the fork — never over a path anything can connect to. That
 * is what keeps the surface and view protocols forwardable over ssh.
 */

/* memfd_create. Guarded: the self-test's compile gate puts the flag on the
 * command line, and an unconditional define collides with it under -Werror. */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <drm_fourcc.h>
#include <errno.h>
#include <fcntl.h>
#include <linux/dma-buf.h>
#include <linux/input-event-codes.h>
#include <poll.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>

#include <wlr/backend/headless.h>
#include <wlr/render/allocator.h>
#include <wlr/render/dmabuf.h>
#include <wlr/render/drm_format_set.h>
#include <wlr/render/pass.h>
#include <wlr/render/wlr_renderer.h>
#include <wlr/types/wlr_buffer.h>
#include <wlr/types/wlr_output.h>
#include <wlr/util/log.h>

#include "clipboard.h"
#include "embed.h"
#include "kembed.h"
#include "output.h"
#include "seat.h"
#include "server.h"
#include "view.h"
#if CAGE_HAS_XWAYLAND
#include "xwayland.h"
#endif

/*
 * HOW MANY BOXES ONE FRAME'S DAMAGE MAY BE CUT INTO before its bounding box is
 * the cheaper answer. The parent rounds every box out to the blocks it
 * touches — at most sixteen cells square, and smaller where one that size
 * would not fit a message — so pieces beyond a handful land in blocks another
 * piece already named — and each one still costs a message. Sixteen is the
 * same order as the rectangle list a KMS dirty-rectangle commit carries.
 */
#define EMBED_MAX_DAMAGE 16

/*
 * HOW LONG A PUBLISH WAITS FOR THE GPU to have finished the frame it is about
 * to read, in milliseconds.
 *
 * Bounded and never infinite: the wait happens inside the cage's event loop, so
 * a card that never signals would otherwise stop the guest's input and every
 * message to the parent for as long as it stays wedged. Longer than this and
 * that stall is longer; shorter and a frame a loaded card legitimately spent
 * this long on is dropped — and the scene has already subtracted its damage, so
 * the parent keeps the stale block until the client draws over it again. Well
 * past any single frame a working card takes and short enough that a broken one
 * costs a window that crawls rather than a session that hangs.
 */
#define EMBED_FENCE_MS 100

/*
 * THE LARGEST KEYMAP THE PARENT MAY HAND OVER, bytes. Far above any real xkb
 * text layout, which is tens of kilobytes, and a bound rather than a trust:
 * the length arrives in the message and is what this process maps.
 */
#define EMBED_KEYMAP_MAX (256u << 10)

/*
 * DECLARED HERE BECAUSE WLROOTS DOES NOT INSTALL IT. The symbol is exported
 * from the library; only the header is private, so the alternative to this
 * line is a patch against wlroots that has to be rewritten every release.
 * See embed_allocator() for why nothing else will do.
 */
struct wlr_allocator *wlr_udmabuf_allocator_create(void);

/*
 * THE ONE ALLOCATOR WHOSE BUFFERS A GPU CAN DRAW INTO AND THIS PROCESS CAN
 * STILL READ.
 *
 * An embedded cage's whole output is bytes it hands to its parent, so a buffer
 * it cannot read is a black window. With the software renderer that is free:
 * `wlr_allocator_autocreate()` picks shm, whose buffers give a pointer
 * directly. With a hardware renderer it is not — autocreate sees the
 * renderer's DRM descriptor and picks gbm, and a gbm buffer offers neither a
 * pointer nor an shm handle, so every frame would be rendered and none could
 * be published. Its own udmabuf branch is unreachable here, because it is
 * guarded on there being no DRM descriptor at all.
 *
 * A udmabuf buffer is both: a DMA-BUF the renderer can bind and a file the
 * kernel backs with ordinary memory, which is what makes one mapping serve
 * both ends. It needs /dev/udmabuf, so this can fail on a kernel or a
 * permission that does not have it — and the caller then has the software
 * renderer to fall back to, which is why this returns NULL rather than dying.
 *
 * AN ALLOCATOR RETURNED HERE IS NOT A PATH THAT WORKS. Whether a driver will
 * import one of these buffers is answered by the driver, at bind time, and
 * some refuse; the caller proves the renderer, the allocator and an output
 * together before building on them. See embed_render_path_works() in cage.c.
 */
struct wlr_allocator *embed_allocator(struct wlr_backend *backend,
				      struct wlr_renderer *renderer)
{
	struct wlr_allocator *a;

	if (!(renderer->render_buffer_caps & WLR_BUFFER_CAP_DATA_PTR)) {
		a = wlr_udmabuf_allocator_create();
		if (a)
			return a;
		wlr_log(WLR_ERROR, "embed: no udmabuf allocator — a hardware "
				   "renderer's frames would be unreadable");
		return NULL;
	}
	return wlr_allocator_autocreate(backend, renderer);
}

/*
 * THE KERNEL'S CPU-ACCESS BRACKET AROUND A DMA-BUF MAPPING.
 *
 * A START without its matching END leaves the buffer marked as being read by
 * the CPU, and the next START is then unbalanced — so every path out of the
 * read runs the END. EINTR is retried: a signal arriving between the two halves
 * would otherwise drop one of them.
 */
static bool dmabuf_sync(int fd, uint64_t flags)
{
	struct dma_buf_sync s = { .flags = flags };
	int r;

	do {
		r = ioctl(fd, DMA_BUF_IOCTL_SYNC, &s);
	} while (r == -1 && errno == EINTR);

	return r == 0;
}

static bool send_msg(struct cg_embed *e, const KembedMsg *m, int fd)
{
	struct iovec iov = { .iov_base = (void *)m, .iov_len = sizeof(*m) };
	struct msghdr hdr = { .msg_iov = &iov, .msg_iovlen = 1 };
	union {
		char buf[CMSG_SPACE(sizeof(int))];
		struct cmsghdr align;
	} u;

	if (fd >= 0) {
		memset(&u, 0, sizeof(u));
		hdr.msg_control = u.buf;
		hdr.msg_controllen = sizeof(u.buf);

		struct cmsghdr *c = CMSG_FIRSTHDR(&hdr);

		c->cmsg_level = SOL_SOCKET;
		c->cmsg_type = SCM_RIGHTS;
		c->cmsg_len = CMSG_LEN(sizeof(int));
		memcpy(CMSG_DATA(c), &fd, sizeof(int));
	}

	while (sendmsg(e->fd, &hdr, MSG_NOSIGNAL) < 0) {
		if (errno == EINTR)
			continue;
		return false;
	}
	return true;
}

/*
 * ONE DATAGRAM, ITS TAIL AND ANY DESCRIPTOR IT CARRIED.
 *
 * The buffer is the struct PLUS KEMBED_TAIL_MAX, because a message may be
 * longer than the struct — a MIME type follows a drag the way a name follows
 * a title — and a datagram that does not fit the buffer is truncated by the
 * kernel with no error anywhere. A receive of exactly the struct would take a
 * truncated tail for a whole message and would drop every descriptor the
 * kernel attached, one leak per keymap. The tail, where an op has one, is at
 * `buf + sizeof(KembedMsg)` and is as long as the return value says.
 *
 * `*fd` is -1 unless the message carried one, and it is the CALLER's to
 * close — including on an op that does not want it, because a descriptor the
 * kernel queued and nobody closed is a descriptor leaked.
 */
static ssize_t recv_msg(struct cg_embed *e, void *buf, size_t cap, int *fd)
{
	struct iovec iov = { .iov_base = buf, .iov_len = cap };
	union {
		char buf[CMSG_SPACE(sizeof(int))];
		struct cmsghdr align;
	} u;
	struct msghdr hdr = {
		.msg_iov = &iov,
		.msg_iovlen = 1,
		.msg_control = u.buf,
		.msg_controllen = sizeof(u.buf),
	};
	ssize_t n;

	*fd = -1;
	memset(&u, 0, sizeof(u));

	n = recvmsg(e->fd, &hdr, MSG_DONTWAIT | MSG_CMSG_CLOEXEC);
	if (n < 0)
		return n;

	/* THE FIRST HEADER AND ONLY IT. No op on this channel carries more than
	 * one descriptor, so a second would be a peer speaking something else —
	 * and there is exactly one peer, our parent. */
	struct cmsghdr *c = CMSG_FIRSTHDR(&hdr);

	if (c && c->cmsg_level == SOL_SOCKET && c->cmsg_type == SCM_RIGHTS &&
	    c->cmsg_len == CMSG_LEN(sizeof(int)))
		memcpy(fd, CMSG_DATA(c), sizeof(int));

	return n;
}

/*
 * HOW MUCH OF A NAME FITS WITHOUT SPLITTING A CHARACTER. A byte cut is a lead
 * byte with no continuation, which the parent's grid can only draw as a
 * replacement mark — so the cut lands on a sequence boundary or the name loses
 * the whole last character.
 */
static size_t utf8_trim(const char *s, size_t cap)
{
	size_t i = 0, whole = 0;

	while (s[i]) {
		unsigned char c = (unsigned char)s[i];
		size_t n = c < 0x80		? 1
			   : (c & 0xe0) == 0xc0 ? 2
			   : (c & 0xf0) == 0xe0 ? 3
			   : (c & 0xf8) == 0xf0 ? 4
						: 1;

		if (i + n > cap)
			break;
		i += n;
		whole = i;
	}
	return whole;
}

/*
 * A MESSAGE WITH A STRING AFTER IT, in one datagram.
 *
 * The socket is SOCK_SEQPACKET, so the kernel frames the whole send and the
 * receiver's own buffer decides where the string stops: there is no length
 * field to keep in step with the bytes and no codec to keep in step with the
 * struct. Cut to KEMBED_TITLE_MAX here because that is what the parent takes —
 * a longer name would be cut there instead, one copy later.
 */
static bool send_text(struct cg_embed *e, const KembedMsg *m, const char *text)
{
	char tail[KEMBED_TITLE_MAX];
	size_t len = text ? utf8_trim(text, sizeof(tail) - 1) : 0;
	struct iovec iov[2] = {
		{ .iov_base = (void *)m, .iov_len = sizeof(*m) },
		{ .iov_base = tail, .iov_len = len + 1 },
	};
	struct msghdr hdr = { .msg_iov = iov, .msg_iovlen = 2 };

	if (len)
		memcpy(tail, text, len);
	tail[len] = '\0';

	while (sendmsg(e->fd, &hdr, MSG_NOSIGNAL) < 0) {
		if (errno == EINTR)
			continue;
		return false;
	}
	return true;
}

/*
 * HOW MANY PIXELS THIS WINDOW SPENDS ON ONE OF THE GUEST'S LOGICAL ONES.
 *
 * THE OUTPUT IS WHERE IT IS KEPT AND THERE IS NO SECOND COPY. wlroots already
 * stores a scale per output, every wlroots call that converts between the two
 * coordinate systems reads it from there, and a copy on this side would be the
 * one the conversions below disagreed with.
 *
 * EVERY NUMBER ON THE CHANNEL IS IN PIXELS AND EVERY NUMBER INSIDE THE CAGE IS
 * LOGICAL. The framebuffer, the output mode and the blocks the parent cuts are
 * pixels; the layout, the cursor, the scene hit test and a client's own
 * geometry are logical. So a coordinate crossing this boundary is multiplied
 * coming out of the cage and divided going in — and one that is not is a
 * pointer that lands at half the distance it was aimed at, or a window the
 * parent sizes to half of what the guest asked for.
 *
 * A WINDOW WITH NO OUTPUT YET IS AT 1, which is what the parent's own arithmetic
 * assumes until it has said otherwise.
 */
static double win_scale(const struct cg_view *view)
{
	double s = view && view->win.out ? view->win.out->wlr_output->scale : 1.0;

	return s >= 1.0 ? s : 1.0;
}

/*
 * A new mapping for one window, because its size changed. The OLD one is
 * unmapped only after the parent has been told about the new one: the parent
 * may still be reading the frame it was last told about, and pulling the memory
 * out from under it is a fault in a process that did nothing wrong.
 *
 * ANNOUNCED UNDER THE WINDOW IT BELONGS TO, so a flip of it names that window
 * and a window that did not redraw costs no message at all.
 */
static bool remap(struct cg_view *view, int w, int h)
{
	struct cg_embed *e = &view->server->embed;
	struct cg_win *win = &view->win;
	size_t stride = (size_t)w * 4;
	size_t slot = stride * (size_t)h;
	size_t total = slot * KEMBED_SLOTS;

	if (w <= 0 || h <= 0 || total == 0 || !win->win)
		return false;

	int fd = memfd_create("kdos-embed", MFD_CLOEXEC);

	if (fd < 0) {
		wlr_log_errno(WLR_ERROR, "memfd_create");
		return false;
	}
	if (ftruncate(fd, (off_t)total) != 0) {
		wlr_log_errno(WLR_ERROR, "ftruncate");
		close(fd);
		return false;
	}

	void *map = mmap(NULL, total, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);

	if (map == MAP_FAILED) {
		wlr_log_errno(WLR_ERROR, "mmap");
		close(fd);
		return false;
	}

	KembedMsg m = {
		.magic = KEMBED_MAGIC,
		.op = KEMBED_BUF,
		.a = w,
		.b = h,
		.c = (int32_t)stride,
		.d = (int32_t)slot,
		.win = win->win,
	};

	if (!send_msg(e, &m, fd)) {
		munmap(map, total);
		close(fd);
		return false;
	}
	close(fd);

	if (win->map)
		munmap(win->map, win->map_len);
	win->map = map;
	win->map_len = total;
	win->slot_len = slot;
	win->stride = stride;
	win->width = w;
	win->height = h;
	win->slot = 0;
	win->map_blank = true;
	return true;
}

/*
 * WHAT EACH WINDOW'S SURFACE WAS LAST REPORTED AS, in pixels, with 0 in `win`
 * for a free row.
 *
 * KEPT BY THE CHANNEL AND NOT DERIVED FROM A FRAME, because a report is about
 * what has been SAID and no pixel can say it: the framebuffer published for a
 * window is always that window's OUTPUT, whatever the guest committed inside
 * it. The parent rounds a report up to whole cells, which leaves nearly every
 * window a few pixels short of its own output for good — so without the record
 * a disagreement that is already settled costs a message on every frame, for
 * the life of every such window.
 *
 * AS MANY ROWS AS THE GRID HAS SPANS, which is the most windows this process
 * can have at once, and a row is released with its window. An id is never
 * reused, so a row that outlived its window could not be mistaken for another
 * window's — releasing it is what keeps the table from filling up instead.
 */
static struct {
	uint32_t win;
	int w, h;
} surf_told[CG_EMBED_WINS];

static void surface_forget(uint32_t win)
{
	if (!win)
		return;
	for (int i = 0; i < CG_EMBED_WINS; i++)
		if (surf_told[i].win == win)
			surf_told[i].win = 0;
}

/* The row this window's last report is in, claiming a free one if it has
 * none. -1 when every row is taken, which is a window that goes on reporting
 * rather than one that reports wrongly. */
static int surface_row(uint32_t win)
{
	int free_row = -1;

	for (int i = 0; i < CG_EMBED_WINS; i++) {
		if (surf_told[i].win == win)
			return i;
		if (!surf_told[i].win && free_row < 0)
			free_row = i;
	}
	if (free_row >= 0) {
		surf_told[free_row].win = win;
		surf_told[free_row].w = 0;
		surf_told[free_row].h = 0;
	}
	return free_row;
}

/*
 * WHAT AN X11 GUEST HAS ASKED ITS WINDOW TO BE, in pixels, with NULL in `view`
 * for a free row.
 *
 * A MANAGED X11 TOPLEVEL CANNOT DISAGREE WITH ITS OUTPUT, so nothing in a frame
 * can carry this. The window manager is the X server's redirection target: the
 * geometry of a managed window is whatever wlr_xwayland_surface_configure()
 * last set on it, Xwayland attaches a buffer of exactly that geometry, and the
 * committed surface is therefore this end's own number coming back on every
 * frame the client will ever publish. A client states a size of its own exactly
 * once, as a ConfigureRequest, and wlroots drops it on the floor unless
 * something listens — so this row IS the X11 half of the report, and a report
 * derived from an X11 surface instead would be one that can never fire.
 *
 * AS MANY ROWS AS THE GRID HAS SPANS, which is the most windows this process
 * can have at once. The row is taken when the window opens and released when it
 * closes, which is what keeps the listener from outliving the surface it is
 * hung on — a signal emitted into a freed row is this process gone.
 */
#if CAGE_HAS_XWAYLAND
/*
 * THE TWO BITS OF A ConfigureRequest THAT NAME A SIZE, spelled here rather
 * than taken from xcb: the cage links no xcb and these are X protocol
 * constants that cannot move. wlroots hands the mask through as
 * xcb_config_window_t on wlr_xwayland_surface_configure_event.
 */
#define CG_X11_CFG_WIDTH  (1u << 2)
#define CG_X11_CFG_HEIGHT (1u << 3)

struct cg_x11_ask {
	struct wl_listener request_configure;
	struct cg_view *view;
	int w, h;
};

static struct cg_x11_ask x11_ask[CG_EMBED_WINS];

static struct cg_x11_ask *x11_ask_of(const struct cg_view *view)
{
	for (int i = 0; i < CG_EMBED_WINS; i++)
		if (x11_ask[i].view == view)
			return &x11_ask[i];
	return NULL;
}
#endif

/*
 * THE SIZE THIS GUEST WANTS ITS WINDOW TO BE, or false for "it is not saying".
 *
 * A window rendered between being told a size and answering it still measures
 * the size it is about to stop being, and a report of that is one the parent
 * honours by putting the window back where it was — a drag that fights the
 * pointer for its whole length. Only a settled answer leaves here.
 *
 * TWO SHELLS AND TWO SOURCES. xdg-shell acknowledges a configure, so an empty
 * configure list — and no configure still waiting on the idle that sends it —
 * is the client having answered every size it was given, and its committed
 * geometry is then a size it chose. X11 neither acknowledges nor disagrees: the
 * X server resizes a managed window to whatever the window manager configured
 * and Xwayland commits a buffer of exactly that, so the only size an X11 client
 * can ever state is the ConfigureRequest x11_ask holds. A request has nothing
 * in flight behind it and so needs no settling — testing an X11 surface for one
 * is testing this end's own number against itself, which is true when there is
 * nothing to report and false when there is.
 */
static bool surface_wanted(struct cg_view *view, int *width, int *height)
{
	if (!view->wlr_surface)
		return false;

#if CAGE_HAS_XWAYLAND
	if (view->type == CAGE_XWAYLAND_VIEW) {
		const struct cg_x11_ask *ask = x11_ask_of(view);

		if (!ask || ask->w < 1 || ask->h < 1)
			return false;
		/*
		 * AN X11 WISH IS IN LOGICAL PIXELS TOO. X has no scale factor,
		 * so an Xwayland client's ConfigureRequest names the layout's
		 * own units; the parent asks in real ones, and a request
		 * forwarded unconverted asks it for half the window.
		 */
		*width = (int)(ask->w * win_scale(view) + 0.5);
		*height = (int)(ask->h * win_scale(view) + 0.5);
		return true;
	}
#endif

	struct wlr_xdg_toplevel *top =
		wlr_xdg_toplevel_try_from_wlr_surface(view->wlr_surface);

	if (top && (top->base->configure_idle ||
		    !wl_list_empty(&top->base->configure_list)))
		return false;

	embed_natural_size(view, width, height);
	return *width >= 1 && *height >= 1;
}

/*
 * THE GUEST'S WINDOW IS NOT THE SIZE OF ITS OUTPUT, SO THE PARENT IS TOLD.
 *
 * NOTHING ELSE CAN TELL IT. The output is the parent's own choice and the
 * framebuffer is the output, so a guest that commits a smaller window is
 * composited at the output's top left over this process's background — the
 * parent sees a frame of exactly the size it asked for with a band of the
 * scheme's darkest slot down two sides of it — and a guest that commits a
 * larger one is cut off at the output's edge with no error anywhere. A dialog
 * that will not be stretched and a window with a minimum of its own are both
 * ordinary, so both would be permanent.
 *
 * A SIZE OF NONE IS NOT A DISAGREEMENT. surface_wanted() answers false for a
 * guest that has stated no size and for a geometry this end could not make an
 * output at, and a window placed at a size no output can carry is a window
 * drawn at one size and rendered at another.
 *
 * AND AGREEMENT CLEARS THE RECORD, so the next disagreement is news again —
 * which is what lets a parent that ignored a report (the window is tiled, the
 * work area has no room) hear it once more the next time it moves the window.
 */
static void surface_report(struct cg_view *view)
{
	struct cg_win *win = &view->win;
	struct wlr_output *out;
	int w = 0, h = 0, row;

	if (!win->win || !win->out)
		return;
	out = win->out->wlr_output;
	if (!surface_wanted(view, &w, &h))
		return;
	if (w == out->width && h == out->height) {
		surface_forget(win->win);
		return;
	}

	row = surface_row(win->win);
	if (row >= 0 && surf_told[row].w == w && surf_told[row].h == h)
		return;

	KembedMsg m = { .magic = KEMBED_MAGIC, .op = KEMBED_SURFACE, .a = w,
			.b = h, .win = win->win };

	if (!send_msg(&view->server->embed, &m, -1) || row < 0)
		return;
	surf_told[row].w = w;
	surf_told[row].h = h;
}

#if CAGE_HAS_XWAYLAND
/*
 * AN X11 GUEST HAS ASKED FOR A SIZE, WHICH IS THE ONLY SIZE IT WILL EVER STATE.
 *
 * THE REQUEST IS RECORDED AND REPORTED, NEVER HONOURED HERE. The parent owns
 * every window rectangle: a cage that resized its own output on a client's word
 * would draw the guest at a size the session's frame is not, and the session's
 * cap on a run of guest-driven resizes — a toolkit that answers every size with
 * another demand — would have nothing left to cap.
 *
 * AND THE REPORT IS PUSHED, NOT LEFT FOR A FRAME. A client that asks to shrink
 * has nothing new to draw, so the frame that would carry the report past
 * embed_publish() is a frame that is never coming.
 *
 * ICCCM WANTS AN ANSWER EVEN WHEN THE ANSWER IS NO. A redirected
 * ConfigureRequest never reaches the X server, so a client whose request is
 * neither granted nor acknowledged waits on a ConfigureNotify that nothing will
 * send and sits unresized and unpainted for good. Re-asserting the geometry the
 * window already has IS that acknowledgement: wlr_xwayland_surface_configure()
 * emits the synthetic notify ICCCM 4.1.5 asks for whenever the size it is given
 * is the size already set.
 *
 * A SIZE THIS END COULD NOT MAKE AN OUTPUT AT IS NOT RECORDED, so the window
 * keeps the last size the guest asked for that the parent could actually grant.
 */
static void handle_x11_request_configure(struct wl_listener *listener,
					 void *data)
{
	struct cg_x11_ask *ask =
		wl_container_of(listener, ask, request_configure);
	struct wlr_xwayland_surface_configure_event *ev = data;
	struct wlr_xwayland_surface *xs = ev->surface;

	/*
	 * A REQUEST THAT NAMES NO SIZE IS NOT A SIZE. The event carries the
	 * window's current geometry in the fields the mask does not select, so
	 * a client asking only to MOVE would otherwise store this end's own
	 * output size as the guest's wish and the parent would be told the
	 * guest wants exactly what it already has.
	 */
	if ((ev->mask & (CG_X11_CFG_WIDTH | CG_X11_CFG_HEIGHT)) &&
	    ev->width >= 1 && ev->height >= 1 && ev->width <= CG_EMBED_SPAN &&
	    ev->height <= CG_EMBED_SPAN) {
		ask->w = ev->width;
		ask->h = ev->height;
	}

	wlr_xwayland_surface_configure(xs, xs->x, xs->y, xs->width, xs->height);

	if (ask->view)
		surface_report(ask->view);
}
#endif

/*
 * THIS WINDOW'S X11 GUEST IS LISTENED TO, from the open until the close.
 *
 * THE ROW IS THE LISTENER, so taking one and dropping one are the only two
 * places the signal is joined and left: a listener still on a destroyed
 * surface's signal list is a wl_list_remove() through freed memory, and one
 * dropped while the window is still open is a guest whose every size request
 * goes nowhere. Both calls are safe for a view that is not an X11 one and for a
 * window that was never opened, which is what lets the open and close paths
 * carry them unconditionally.
 */
static void x11_ask_watch(struct cg_view *view)
{
#if CAGE_HAS_XWAYLAND
	struct wlr_xwayland_surface *xs;
	struct cg_x11_ask *ask;

	if (view->type != CAGE_XWAYLAND_VIEW || x11_ask_of(view))
		return;
	xs = xwayland_view_from_view(view)->xwayland_surface;
	if (!xs)
		return;
	ask = x11_ask_of(NULL);		/* a free row carries no view */
	if (!ask)
		return;

	ask->view = view;
	ask->w = 0;
	ask->h = 0;
	ask->request_configure.notify = handle_x11_request_configure;
	wl_signal_add(&xs->events.request_configure, &ask->request_configure);
#else
	(void)view;
#endif
}

/*
 * THIS GUEST'S WISH IS SPENT. The row keeps the listener and the view — only
 * the size is dropped — because the window is still open and its next request
 * must still be heard.
 */
static void x11_ask_clear(struct cg_view *view)
{
#if CAGE_HAS_XWAYLAND
	struct cg_x11_ask *ask = x11_ask_of(view);

	if (!ask)
		return;
	ask->w = 0;
	ask->h = 0;
#else
	(void)view;
#endif
}

static void x11_ask_drop(struct cg_view *view)
{
#if CAGE_HAS_XWAYLAND
	struct cg_x11_ask *ask = x11_ask_of(view);

	if (!ask)
		return;
	wl_list_remove(&ask->request_configure.link);
	ask->view = NULL;
	ask->w = 0;
	ask->h = 0;
#else
	(void)view;
#endif
}

void embed_set_size(struct cg_view *view, int w, int h)
{
	struct wlr_output_state state;

	/* A SIZE PAST THE SPAN IS REFUSED AND NOT CLAMPED. Clamping would tell
	 * the guest it is one size while the parent scales its frames as
	 * another; refusing leaves the window at the size it has, which the
	 * parent's own resize gate already knows how to wait out. */
	if (!view->win.out || w < 1 || h < 1 || w > CG_EMBED_SPAN ||
	    h > CG_EMBED_SPAN)
		return;

	wlr_output_state_init(&state);
	wlr_output_state_set_custom_mode(&state, w, h, 0);
	wlr_output_commit_state(view->win.out->wlr_output, &state);
	wlr_output_state_finish(&state);

	/* A NEW OUTPUT IS A NEW QUESTION. What the guest last reported was
	 * about the output it had; the next frame measures the guest against
	 * this one, and a report the parent could not act on then is one it
	 * can act on now. */
	surface_forget(view->win.win);

	/*
	 * AND THE PARENT SETTING THE SIZE IS WHAT ANSWERS AN X11 ASK. A
	 * ConfigureRequest is a one-shot wish, not a standing state: an X11
	 * guest is measured off the row rather than off its buffer, so a row
	 * left filled is re-reported on the very next frame and the parent
	 * undoes its own resize for as long as the window lives. Dropping it
	 * here leaves the window where the parent put it, and a guest that
	 * still wants another size has a fresh request to say so with.
	 */
	x11_ask_clear(view);
}

/*
 * HOW MANY PIXELS THE DESKTOP SPENDS ON ONE OF THE GUEST'S LOGICAL ONES.
 *
 * NOTHING ABOUT THE FRAME MOVES. The mode is untouched, so the output stays the
 * pixel size the parent asked for, the swapchain is the same shape, no new
 * mapping is announced and the blocks the parent cuts out of the framebuffer
 * are the cells it already chose. What changes is the LOGICAL size wlroots
 * derives from the mode, which is what the toolkit lays its window out in and
 * what it multiplies its own drawing by — so a scale of 2 is a guest that draws
 * everything twice as large into exactly the frame it was already filling. This
 * is the whole reason the scale can be raised on a live window at all.
 *
 * A WHOLE NUMBER AND A SMALL ONE. wlroots divides the mode by the scale and
 * TRUNCATES, so a fraction leaves a strip of this process's background down the
 * edge of the window for as long as the window lives; the parent only ever
 * names a scale that divides its own cell, and this refuses anything outside
 * the range a console can plausibly be at rather than trusting it.
 *
 * THE VIEW IS RE-PLACED BY THE LAYOUT AND NOT FROM HERE. Committing the scale
 * changes the output's logical box, which the output layout answers with its
 * own change event, which is where every view is sized to the box it sits in —
 * a resize from here would be the second one and they would disagree.
 */
#define EMBED_SCALE_MAX 4

static void embed_set_scale(struct cg_view *view, int scale)
{
	struct wlr_output_state state;

	if (!view || !view->win.out || scale < 1 || scale > EMBED_SCALE_MAX)
		return;
	if (view->win.out->wlr_output->scale == (float)scale)
		return;

	wlr_output_state_init(&state);
	wlr_output_state_set_scale(&state, (float)scale);
	wlr_output_commit_state(view->win.out->wlr_output, &state);
	wlr_output_state_finish(&state);

	/* A NEW SCALE IS A NEW QUESTION, exactly as a new mode is: what the
	 * guest last reported was measured against the logical box it had. */
	surface_forget(view->win.win);
}

/*
 * THE LAYOUT THE PERSON IS TYPING ON, out of the descriptor it arrived in.
 *
 * Mapped rather than read: the descriptor is sealed and the length is in the
 * message, so it is one call with no copy and no partial read to loop over. A
 * text a compiler is handed must end in a terminator, so one that does not is
 * refused rather than compiled — and the guest keeps the layout it has, which
 * is a wrong layout and not a dead keyboard.
 */
static void take_keymap(struct cg_server *server, const KembedMsg *m, int fd)
{
	if (m->b != KEMBED_KEYMAP_XKB_V1 || m->a <= 0 ||
	    (size_t)m->a > EMBED_KEYMAP_MAX)
		return;

	size_t len = (size_t)m->a;
	char *text = mmap(NULL, len, PROT_READ, MAP_PRIVATE, fd, 0);

	if (text == MAP_FAILED) {
		wlr_log_errno(WLR_ERROR, "kembed: keymap mmap");
		return;
	}
	if (text[len - 1] == '\0')
		seat_embed_keymap(server->seat, text, len);
	else
		wlr_log(WLR_ERROR, "kembed: a keymap with no terminator");
	munmap(text, len);
}

/*
 * THAT WINDOW IS FULLSCREEN NOW, OR IT IS NOT.
 *
 * The parent owns the window model, so Super+f is decided there and the
 * output follows the window — but a guest that is never told keeps the
 * layout it drew for a window, which is Firefox keeping its toolbars over a
 * full screen and a player that was put back drawing fullscreen chrome inside
 * a small one. The toplevel `win` names is the one told, so a fullscreen asked
 * of the image window does not reshape the dialog in front of it.
 */
static void set_fullscreen_state(struct cg_view *view, bool fullscreen)
{
	if (!view->wlr_surface)
		return;

	struct wlr_xdg_toplevel *top =
		wlr_xdg_toplevel_try_from_wlr_surface(view->wlr_surface);

	if (top) {
		wlr_xdg_toplevel_set_fullscreen(top, fullscreen);
		return;
	}
#if CAGE_HAS_XWAYLAND
	struct wlr_xwayland_surface *xs =
		wlr_xwayland_surface_try_from_wlr_surface(view->wlr_surface);

	if (xs)
		wlr_xwayland_surface_set_fullscreen(xs, fullscreen);
#endif
}

struct cg_view *embed_view_from_win(struct cg_server *server, uint32_t win)
{
	struct cg_view *view;

	if (!win) {
		wl_list_for_each (view, &server->views, link)
			if (view->win.win)
				return view;
		return NULL;
	}
	wl_list_for_each (view, &server->views, link)
		if (view->win.win == win)
			return view;
	return NULL;
}

void embed_natural_size(struct cg_view *view, int *width, int *height)
{
	int w = 0, h = 0;

	view->impl->get_geometry(view, &w, &h);
	if ((w <= 0 || h <= 0) && view->wlr_surface) {
		w = view->wlr_surface->current.width;
		h = view->wlr_surface->current.height;
	}

	/*
	 * Zero is "none" on the wire and the parent has a rule for it; a number
	 * the parent would place a window at and then be refused when it asked
	 * for it is a window at one size being drawn at another.
	 *
	 * AND IT LEAVES HERE IN PIXELS. A client states its geometry in logical
	 * pixels, the parent places windows and cuts blocks in real ones, and
	 * this is the one place a guest's own idea of its size crosses over —
	 * both the natural size in KEMBED_OPEN and the report in KEMBED_SURFACE
	 * are taken from here, so a window at scale 2 reported unconverted is a
	 * window the parent sizes to half of what the guest asked for.
	 */
	double sc = win_scale(view);

	w = (int)(w * sc + 0.5);
	h = (int)(h * sc + 0.5);

	/*
	 * A SIZE THIS END CANNOT MAKE AN OUTPUT AT IS NO ANSWER AT ALL, and the
	 * test is on the pixels because the output is made of those.
	 */
	if (w < 1 || h < 1 || w > CG_EMBED_SPAN || h > CG_EMBED_SPAN) {
		w = 0;
		h = 0;
	}
	*width = w;
	*height = h;
}

/*
 * WHAT KIND OF WINDOW THIS IS, in KEMBED_OPEN's `d`.
 *
 * DIALOG is the only one a Wayland toplevel can report, because xdg-shell has
 * no modal, no utility and no splash to read — so it comes from naming an
 * owner. X11 carries all three and an Xwayland guest is where they arrive; see
 * kembed.h.
 */
static uint32_t win_roles(struct cg_view *view)
{
	uint32_t role = 0;

	if (view_get_parent(view))
		role |= KEMBED_ROLE_DIALOG;

#if CAGE_HAS_XWAYLAND
	if (view->type == CAGE_XWAYLAND_VIEW) {
		const struct wlr_xwayland_surface *xs =
			xwayland_view_from_view(view)->xwayland_surface;

		if (xs->modal)
			role |= KEMBED_ROLE_MODAL;
		if (wlr_xwayland_surface_has_window_type(
			    xs, WLR_XWAYLAND_NET_WM_WINDOW_TYPE_DIALOG))
			role |= KEMBED_ROLE_DIALOG;
		if (wlr_xwayland_surface_has_window_type(
			    xs, WLR_XWAYLAND_NET_WM_WINDOW_TYPE_UTILITY) ||
		    wlr_xwayland_surface_has_window_type(
			    xs, WLR_XWAYLAND_NET_WM_WINDOW_TYPE_TOOLBAR))
			role |= KEMBED_ROLE_UTILITY;
		if (wlr_xwayland_surface_has_window_type(
			    xs, WLR_XWAYLAND_NET_WM_WINDOW_TYPE_SPLASH))
			role |= KEMBED_ROLE_SPLASH;
	}
#endif
	return role;
}

bool embed_win_is_ordinary(struct cg_view *view)
{
	return win_roles(view) == 0;
}

void embed_open_window(struct cg_view *view)
{
	struct cg_embed *e = &view->server->embed;
	struct cg_view *parent;
	int w = 0, h = 0;

	/*
	 * A TOPLEVEL WITH NO OUTPUT IS NOT ANNOUNCED. The grid holds
	 * CG_EMBED_WINS windows and output_claim() refuses past that, and a
	 * window the parent was told about but that can never carry a frame is
	 * a taskbar row and a frame on the desktop with nothing behind it.
	 */
	if (!e->active || view->win.win || !view->win.out)
		return;

	embed_natural_size(view, &w, &h);
	parent = view_get_parent(view);

	/*
	 * NEVER REUSED, EVER. The parent keys a window on (channel, win) and
	 * drops an op naming one it does not have; an id handed out twice is a
	 * frame drawn into whichever window claimed it first.
	 */
	view->win.win = ++e->next_win;
	view->win.owner = parent ? parent->win.win : 0;
	x11_ask_watch(view);

	KembedMsg m = {
		.magic = KEMBED_MAGIC,
		.op = KEMBED_OPEN,
		.a = w,
		.b = h,
		.c = (int32_t)view->win.owner,
		.d = (int32_t)win_roles(view),
		.win = view->win.win,
	};

	send_text(e, &m, view->impl->get_title(view));

	/* AND WHAT IT ASKED FOR BEFORE IT HAD A NAME. A client may request
	 * fullscreen at its initial commit, where there is no `win` to carry
	 * it; the request is held on the window and goes out the moment there
	 * is one. */
	if (view->win.want_fullscreen) {
		view->win.want_fullscreen = false;
		embed_set_fullscreen(view, true);
	}
}

void embed_close_window(struct cg_view *view)
{
	struct cg_embed *e = &view->server->embed;

	if (e->kbd == view)
		e->kbd = NULL;

	x11_ask_drop(view);
	surface_forget(view->win.win);

	if (view->win.map) {
		munmap(view->win.map, view->win.map_len);
		view->win.map = NULL;
		view->win.map_len = 0;
	}
	if (!view->win.win)
		return;

	if (e->active) {
		KembedMsg m = { .magic = KEMBED_MAGIC, .op = KEMBED_CLOSE_WIN,
				.win = view->win.win };

		send_msg(e, &m, -1);
	}
	view->win.win = 0;
	view->win.owner = 0;
	view->win.asleep = false;
}

static int handle_readable(int fd, uint32_t mask, void *data)
{
	struct cg_server *server = data;
	struct cg_embed *e = &server->embed;

	(void)fd;

	if (mask & (WL_EVENT_HANGUP | WL_EVENT_ERROR)) {
		/* The parent is gone. A compositor holding a guest nobody can
		 * see is a process nobody can reach either. */
		server_terminate(server);
		return 0;
	}

	for (;;) {
		char buf[sizeof(KembedMsg) + KEMBED_TAIL_MAX];
		KembedMsg m;
		int msgfd = -1;
		ssize_t n = recv_msg(e, buf, sizeof(buf), &msgfd);

		if (n < 0) {
			if (errno == EINTR)
				continue;
			return 0;	/* EAGAIN: drained */
		}
		if (n == 0) {
			server_terminate(server);
			return 0;
		}
		/* A message shorter than the struct, or mistyped, is a peer
		 * speaking something else. There is exactly one peer and it is
		 * our parent, so this is a bug rather than an attack — and
		 * either way the answer is to stop rather than to guess. A
		 * LONGER one is ordinary: the tail is where a name and a MIME
		 * type travel. */
		if (n >= (ssize_t)sizeof(m))
			memcpy(&m, buf, sizeof(m));
		if (n < (ssize_t)sizeof(m) || m.magic != KEMBED_MAGIC) {
			if (msgfd >= 0)
				close(msgfd);
			wlr_log(WLR_ERROR, "kembed: malformed message");
			server_terminate(server);
			return 0;
		}

		/*
		 * THE WINDOW THE OP IS ABOUT, resolved once. Every op below
		 * that names a window drops silently when it names one this
		 * end does not have: a toplevel can go while a message about it
		 * is still in flight, and guessing at another window is a key
		 * typed into the wrong one. The descriptor such a message
		 * carried is still closed, by the sweep at the end of the loop.
		 */
		struct cg_view *w = embed_view_from_win(server, m.win);

		/*
		 * PIXELS COME IN AND LOGICAL UNITS GO TO THE SEAT. The parent
		 * aims at the framebuffer, wlroots aims at the layout, and the
		 * two differ by exactly this whenever the console is dense —
		 * an unconverted position at scale 2 lands at half the distance
		 * from the window's corner that the person pointed at.
		 */
		double sc = win_scale(w);

		switch (m.op) {
		case KEMBED_SCALE:
			if (w)
				embed_set_scale(w, m.a);
			break;
		case KEMBED_SIZE:
			if (w && w->win.out &&
			    (m.a != w->win.out->wlr_output->width ||
			     m.b != w->win.out->wlr_output->height))
				embed_set_size(w, m.a, m.b);
			break;
		case KEMBED_KEYMAP:
			if (msgfd >= 0)
				take_keymap(server, &m, msgfd);
			break;
		case KEMBED_CLIP_SET:
			clipboard_take(server, &m, msgfd);
			break;
		case KEMBED_MODS:
			seat_embed_mods(server->seat, (uint32_t)m.a,
					(uint32_t)m.b, (uint32_t)m.c,
					(uint32_t)m.d);
			break;
		case KEMBED_KEY:
			/*
			 * A KEY GOES WHERE THE KEYBOARD IS, which is the window
			 * the parent named in the last KEMBED_FOCUS. There is
			 * one keyboard focus per seat and the parent owns it,
			 * so a key needs no routing of its own — and a key
			 * routed by `win` instead would reach a window the seat
			 * has not entered, where wlroots drops it.
			 */
			seat_embed_key(server->seat, (uint32_t)m.a, m.b != 0,
				       m.e);
			break;
		case KEMBED_MOTION:
			if (w)
				seat_embed_motion(server->seat, w, m.a / sc,
						  m.b / sc, m.e);
			break;
		case KEMBED_REL:
			/*
			 * 1/256 OF A PIXEL, which is what the fraction in the
			 * message is: a device delta small enough to be lost
			 * to a whole number is exactly the slow, precise
			 * motion a three-dimensional editor is aimed with.
			 */
			seat_embed_rel(server->seat, m.a / 256.0 / sc,
				       m.b / 256.0 / sc, m.c / 256.0 / sc,
				       m.d / 256.0 / sc, m.e);
			break;
		case KEMBED_BUTTON:
			if (w) {
				seat_embed_motion(server->seat, w, m.a / sc,
						  m.b / sc, m.e);
				seat_embed_button(server->seat, (uint32_t)m.c,
						  m.d != 0, m.e);
			}
			break;
		case KEMBED_AXIS:
			/*
			 * NO POSITION HERE, and none is wanted: a scroll is
			 * not a place. The parent sends the motion first
			 * whenever the pointer moved, so a detent over a
			 * frame button is a scroll and not a click on it.
			 */
			seat_embed_axis(server->seat, m.a / 256.0 / sc, m.b, m.c,
					m.d & 0xff,
					(m.d & (int32_t)KEMBED_AXIS_INVERTED) != 0,
					m.e);
			break;
		case KEMBED_LEAVE:
			seat_embed_leave(server->seat);
			break;
		case KEMBED_FOCUS:
			/*
			 * THE FIELD IS SET BEFORE THE SEAT IS TOLD. A pointer
			 * grab may activate only while the window it belongs to
			 * has the keyboard, and that test reads this field.
			 *
			 * A FOCUS NAMING A WINDOW THIS END DOES NOT HAVE IS
			 * DROPPED, like any other op that names one: the two
			 * ends are out of step, and the keyboard is left where
			 * it is rather than taken from a window that still has
			 * keys down.
			 */
			if (!m.a) {
				e->kbd = NULL;
				seat_embed_focus(server->seat, NULL);
			} else if (w) {
				e->kbd = w;
				seat_embed_focus(server->seat, w);
			}
			break;
		case KEMBED_FULLSCREEN_SET:
			/*
			 * NAMING A WINDOW, ALWAYS. The parent re-offers this
			 * state every turn until the window it is for has an
			 * id, so a box launched into a window the console
			 * already has fullscreen is told the moment its first
			 * toplevel maps and nothing has to be held here.
			 */
			if (w)
				set_fullscreen_state(w, m.a != 0);
			break;
		case KEMBED_SLEEP:
			/*
			 * A WINDOW NOBODY CAN SEE STOPS RENDERING —
			 * minimised, hidden, on another workspace, behind the
			 * lock or under the saver. A guest drawing
			 * frames nobody is composited into is a guest spending
			 * a core on nothing — which on a battery is the whole
			 * difference between a window and a wasted process.
			 * Per window: a dock behind another workspace stops
			 * while the image window in front of the person does
			 * not.
			 */
			if (w)
				w->win.asleep = m.a != 0;
			break;
		case KEMBED_CLOSE:
			/*
			 * THE GUEST IS ASKED, NOT SHOT. Terminating the
			 * display takes the application down with it, and an
			 * application that was never told to quit never got to
			 * offer the dialog that saves the work in it. The
			 * deadline and the escalation are the parent's, because
			 * the parent owns the process and this end owns only
			 * the protocol.
			 *
			 * A NAMED WINDOW IS THE ONE ASKED, and only it: a
			 * person clicking the X on an export dialog has not
			 * asked the application to quit. Zero is every
			 * toplevel, and with nothing mapped there is nobody to
			 * ask — that case is the immediate one.
			 */
			if (m.win) {
				if (w)
					w->impl->close(w);
				break;
			}
			if (wl_list_empty(&server->views)) {
				server_terminate(server);
				return 0;
			}
			{
				struct cg_view *view, *tmp;

				wl_list_for_each_safe (view, tmp,
						       &server->views, link)
					view->impl->close(view);
			}
			break;
		default:
			break;
		}

		/*
		 * WHATEVER THE OP DID WITH IT, THE DESCRIPTOR IS CLOSED HERE.
		 * An op that wanted one has finished with it by now — the
		 * keymap is compiled inside the call — and an op that did not
		 * want one still had it queued by the kernel. One left open
		 * per message is a cage that runs out of descriptors.
		 */
		if (msgfd >= 0)
			close(msgfd);
	}
}

bool embed_init(struct cg_server *server, int fd)
{
	struct cg_embed *e = &server->embed;

	/*
	 * NO MAPPING AND NO SIZE ARE SET HERE, because there is no window yet.
	 * Every mapping belongs to a toplevel and is announced under its `win`,
	 * and one made before any toplevel exists would be a mapping the parent
	 * could not attach to anything. The anchor output already carries the
	 * size --embed named.
	 */
	e->fd = fd;
	e->next_win = 0;

	e->source = wl_event_loop_add_fd(wl_display_get_event_loop(server->wl_display),
					 fd, WL_EVENT_READABLE, handle_readable,
					 server);
	if (!e->source)
		return false;

	KembedMsg m = { .magic = KEMBED_MAGIC, .op = KEMBED_HELLO };

	e->active = true;
	return send_msg(e, &m, -1);
}

void embed_finish(struct cg_server *server)
{
	struct cg_embed *e = &server->embed;
	struct cg_view *view;

	if (!e->active)
		return;

	KembedMsg m = { .magic = KEMBED_MAGIC, .op = KEMBED_GONE };

	send_msg(e, &m, -1);
	if (e->source)
		wl_event_source_remove(e->source);

	/*
	 * EVERY WINDOW'S MAPPING, not one. The channel ends here and the parent
	 * retires every window of it on the message above; a mapping left
	 * behind is this process's own memory and nobody else's.
	 */
	wl_list_for_each (view, &server->views, link) {
		if (view->win.map) {
			munmap(view->win.map, view->win.map_len);
			view->win.map = NULL;
			view->win.map_len = 0;
		}
		view->win.win = 0;
	}

	e->kbd = NULL;
	e->source = NULL;
	e->active = false;
}

void embed_set_title(struct cg_view *view, const char *title)
{
	struct cg_embed *e = &view->server->embed;
	KembedMsg m = { .magic = KEMBED_MAGIC, .op = KEMBED_TITLE,
			.win = view->win.win };

	if (!e->active || !view->win.win || !title || !*title)
		return;
	send_text(e, &m, title);
}

void embed_set_fullscreen(struct cg_view *view, bool fullscreen)
{
	struct cg_embed *e = &view->server->embed;
	KembedMsg m = { .magic = KEMBED_MAGIC, .op = KEMBED_FULLSCREEN,
			.a = fullscreen ? 1 : 0, .win = view->win.win };

	if (!e->active)
		return;
	if (!view->win.win) {
		view->win.want_fullscreen = fullscreen;
		return;
	}
	send_msg(e, &m, -1);
}

/*
 * SOMETHING IN HERE IS PLAYING, so the screen must not blank.
 *
 * The guest's idle inhibitor reaches this process' own idle notifier, which
 * this process never reads because it has no idle policy: the saver, the lock
 * and the DPMS clock are all the parent's. Without this message a video in a
 * boxed player is watched for five minutes and then covered by the saver,
 * with nothing the application can do about it — and the graphical session
 * honours the identical protocol.
 */
void embed_set_inhibit(struct cg_view *view, bool inhibited)
{
	struct cg_embed *e;
	KembedMsg m = { .magic = KEMBED_MAGIC, .op = KEMBED_INHIBIT,
			.a = inhibited ? 1 : 0 };

	if (!view)
		return;
	e = &view->server->embed;
	if (!e->active || !view->win.win)
		return;
	m.win = view->win.win;
	send_msg(e, &m, -1);
}

/*
 * THE GUEST TOOK THE POINTER, or gave it back.
 *
 * A game and a three-dimensional editor lock or confine the pointer and read
 * motion as a delta. The parent draws the arrow and decides which window is
 * hovered, so it is the parent that has to stop doing both — a console that
 * went on moving its own pointer over a locked guest would hover and raise
 * windows behind the person's back. The hint is where the guest asked the
 * pointer to be left when the grab ends.
 */
void embed_set_grab(struct cg_view *view, int grab, bool have_hint,
		    int hint_x, int hint_y)
{
	struct cg_embed *e;
	KembedMsg m = { .magic = KEMBED_MAGIC, .op = KEMBED_GRAB, .a = grab,
			.d = have_hint ? (int32_t)KEMBED_GRAB_HINT : 0 };

	if (!view)
		return;
	/* THE HINT IS A PLACE AND LEAVES IN PIXELS, like every other place on
	 * this channel: the guest asked in its own logical units and the parent
	 * puts the arrow down in the framebuffer's. */
	m.b = (int32_t)(hint_x * win_scale(view) + 0.5);
	m.c = (int32_t)(hint_y * win_scale(view) + 0.5);
	e = &view->server->embed;
	if (!e->active || !view->win.win)
		return;
	m.win = view->win.win;
	send_msg(e, &m, -1);
}

bool embed_active(struct cg_server *server)
{
	return server->embed.active;
}

bool embed_asleep(const struct cg_view *view)
{
	return view->server->embed.active && view->win.asleep;
}

/* Where one buffer's pixels are, and what has to be undone to let go of them.
 * `mapped` is MAP_FAILED unless the shm road was taken, and that is what
 * frame_unmap() reads to tell the two roads apart. */
struct frame_map {
	void *data;
	uint32_t format;
	size_t stride;
	void *mapped;
	size_t maplen;
	int dmafd;
	bool synced;
};

/*
 * THE ONLY TWO PIXEL FORMATS THIS CAGE COPIES. The parent reads a sprite block
 * as four bytes a pixel in this channel order and has no field to be told
 * otherwise, so a buffer in anything else is copied by nobody: the frame is
 * dropped and the window keeps the one before it, which is a stall a person
 * can see and report, where a copy at the wrong depth is a smear that reads as
 * a decoder fault.
 */
static bool frame_is_argb32(const struct frame_map *fm)
{
	return fm->format == DRM_FORMAT_XRGB8888 ||
	       fm->format == DRM_FORMAT_ARGB8888;
}

/*
 * A RENDERED BUFFER, OPEN FOR READING, BY WHICHEVER ROAD THE ALLOCATOR LEFT.
 *
 * A direct pointer is what the shm allocator offers and it is free, so it is
 * asked for first. A udmabuf buffer — which is what an allocator has to hand
 * out for a GPU renderer to draw into memory this process can still read —
 * implements `get_shm` and `get_dmabuf` and NOT data-ptr access, so a cage
 * that knew only the first road would publish nothing at all the moment the
 * renderer stopped being the software one. The window would be permanently
 * black, with every other part of the mechanism working.
 *
 * Mapped per frame on that road rather than cached, because the swapchain owns
 * the buffer and may free it between frames; the cost is two syscalls and the
 * page table for one frame, against a readback through the GPU, which is the
 * only other way to reach those bytes.
 *
 * THE CARD MAY STILL BE WRITING THESE PAGES. A udmabuf buffer hands out two
 * descriptors onto one piece of memory: the memfd mapped here and a DMA-BUF
 * beside it, and the DMA-BUF is the only one the kernel will synchronise on. A
 * gles2 pass on a headless output ends in a bare glFlush() — wlroots allocates
 * a signal timeline only for a backend with a DRM descriptor and the headless
 * backend has none — so with nothing waited here the copy races the renderer
 * and a busy card tears inside a block, worst exactly when it is busiest.
 *
 * POLLIN ON A DMA-BUF WAITS FOR THE WRITE FENCE THE DRIVER ATTACHED, AND A
 * DRIVER NEED NOT ATTACH ONE. A udmabuf buffer whose reservation is empty
 * polls readable the moment it is asked, so a readable poll bounds the wait
 * rather than promising the write is done; it is worth taking because the
 * drivers that do attach a fence are the ones whose frame tears without it.
 * The ioctl bracket is the cache maintenance a CPU mapping of memory a device
 * wrote needs, and it is what makes the bytes read back the ones the GPU put
 * there. A buffer with no DMA-BUF handle has no GPU writer and needs neither.
 *
 * `skip_on_stall` says a fence that has not signalled within the deadline is a
 * reason to give up on this buffer; without it the read runs unwaited, which
 * is what a caller with nothing to lose by a torn read wants. A poll that
 * fails outright is not a deadline and never stalls the caller.
 *
 * FALSE MEANS NOTHING WAS OPENED and frame_unmap() must not be called: there
 * is no bracket to close and no data-ptr access to end.
 */
static bool frame_map(struct wlr_buffer *buffer, bool skip_on_stall,
		      struct frame_map *fm)
{
	struct wlr_shm_attributes shm = { .fd = -1 };
	struct wlr_dmabuf_attributes dma = { .n_planes = 0 };

	*fm = (struct frame_map){ .mapped = MAP_FAILED, .dmafd = -1 };

	if (wlr_buffer_begin_data_ptr_access(buffer,
					     WLR_BUFFER_DATA_PTR_ACCESS_READ,
					     &fm->data, &fm->format,
					     &fm->stride))
		return true;

	if (!wlr_buffer_get_shm(buffer, &shm) || shm.fd < 0)
		return false;
	/* Signed in the attributes and used as sizes here, so the negatives
	 * are refused before the arithmetic rather than after it, where they
	 * are enormous. */
	if (shm.stride < 0 || shm.offset < 0 ||
	    (size_t)shm.stride < (size_t)buffer->width * 4)
		return false;
	fm->maplen = (size_t)shm.offset +
		     (size_t)shm.stride * (size_t)buffer->height;
	fm->mapped = mmap(NULL, fm->maplen, PROT_READ, MAP_SHARED, shm.fd, 0);
	if (fm->mapped == MAP_FAILED)
		return false;
	fm->data = (uint8_t *)fm->mapped + shm.offset;
	fm->stride = (size_t)shm.stride;
	fm->format = shm.format;

	if (wlr_buffer_get_dmabuf(buffer, &dma) && dma.n_planes > 0)
		fm->dmafd = dma.fd[0];
	if (fm->dmafd >= 0) {
		struct pollfd pfd = { .fd = fm->dmafd, .events = POLLIN };
		int r;

		do {
			r = poll(&pfd, 1, EMBED_FENCE_MS);
		} while (r == -1 && errno == EINTR);

		if (r == 0 && skip_on_stall) {
			munmap(fm->mapped, fm->maplen);
			fm->mapped = MAP_FAILED;
			return false;
		}
		fm->synced = dmabuf_sync(fm->dmafd, DMA_BUF_SYNC_START |
						    DMA_BUF_SYNC_READ);
	}
	return true;
}

/*
 * ONE WAY OUT OF THE READ, so the kernel's bracket is closed and the mapping
 * released whatever the frame turned out to be. Only ever after a frame_map()
 * that returned true.
 */
static void frame_unmap(struct wlr_buffer *buffer, struct frame_map *fm)
{
	if (fm->synced)
		dmabuf_sync(fm->dmafd, DMA_BUF_SYNC_END | DMA_BUF_SYNC_READ);
	if (fm->mapped != MAP_FAILED)
		munmap(fm->mapped, fm->maplen);
	else
		wlr_buffer_end_data_ptr_access(buffer);
}

void embed_publish(struct cg_view *view, struct wlr_buffer *buffer,
		   const pixman_region32_t *damage)
{
	struct cg_embed *e;
	struct cg_win *win;

	if (!view || !buffer)
		return;

	e = &view->server->embed;
	win = &view->win;

	/*
	 * NOTHING IS PUBLISHED UNTIL THE WINDOW HAS BEEN OPENED.
	 *
	 * The scene's background rectangle is created with the server, so the
	 * first headless frame is a whole window of the scheme's darkest slot
	 * and it goes out within milliseconds of the fork — long before a
	 * container has finished coming up and the guest exists at all. The
	 * parent cannot tell that black from a black an application drew, so
	 * the window reads as a program that started and then did nothing. With
	 * no frame at all the parent knows it is still waiting, and says so.
	 * KEMBED_OPEN is also what the parent needs before it can attach a
	 * frame to anything: a frame for a window that has not opened is
	 * dropped there rather than guessed at.
	 */
	if (!e->active || !win->win)
		return;

	/*
	 * AND THE SIZE THE GUEST CHOSE FOR ITSELF IS CHECKED AHEAD OF THE
	 * DAMAGE GATE. A guest that has finished resizing has nothing more to
	 * draw, so the frame that carries its last pixels may also be the last
	 * frame it ever publishes — a check behind a gate that drops a frame
	 * with nothing new in it would wait for a frame that is not coming.
	 */
	surface_report(view);

	/*
	 * AN EMPTY DAMAGE REGION IS NOTHING TO SEND, NOT EVERYTHING.
	 *
	 * wlr_scene_output_build_state() ALWAYS sets the damage field, and the
	 * scene subtracts what it committed afterwards — so a frame with
	 * nothing new in it arrives here as a region that is present and
	 * empty. Reading that as "no damage information" and falling through
	 * to the whole-window box below told the parent that every pixel had
	 * changed, on every tick the headless output produced. The parent then
	 * re-cut and re-sent every block of the window: about two megabytes a
	 * frame for a half-screen guest, which overruns the view connection's
	 * queue, and a view that stops reading is a view the session drops —
	 * the screen freezes on whatever it last flipped.
	 *
	 * The slot is NOT advanced either: a flip with nothing behind it costs
	 * the parent a whole-window resend the next time it does have damage.
	 */
	if (damage && !pixman_region32_not_empty(damage))
		return;

	if (!win->map || buffer->width != win->width ||
	    buffer->height != win->height) {
		/* THE MAPPING FOLLOWS THE BUFFER AND NOT THE REQUEST. This is
		 * the first frame of a window, or the first at a new size after
		 * the output was resized. */
		if (!remap(view, buffer->width, buffer->height))
			return;
	}

	/*
	 * A FENCE THAT DOES NOT SIGNAL SKIPS THE FRAME, BUT ONLY WHERE THERE
	 * IS A WHOLE FRAME TO KEEP: the slot is not flipped, so the parent
	 * goes on showing the one before it. Into a mapping that has never
	 * carried a frame there is nothing to keep — the parent is holding
	 * zeroed pages and showing none of them — so the copy runs unwaited
	 * instead. A torn first frame is corrected by the next one the guest
	 * draws; a skipped one is not corrected at all, because the scene has
	 * already subtracted its damage and a guest with nothing to redraw
	 * never asks for another frame. The window would stay blank for as
	 * long as it is open.
	 */
	struct frame_map fm;

	if (!frame_map(buffer, !win->map_blank, &fm))
		return;

	int next = (win->slot + 1) % KEMBED_SLOTS;
	bool copied = false;

	/*
	 * 32 BITS PER PIXEL AND NOTHING ELSE. Both renderers reach this copy —
	 * pixman on a headless output, and gles2 wherever the readback probe
	 * kept the card — and both give XRGB8888 or ARGB8888 on the buffers
	 * embed_allocator() hands out. Anything else is a renderer that chose a
	 * format this was not written for, and copying it as if it were one of
	 * those would put garbage on a screen rather than fail.
	 */
	if (frame_is_argb32(&fm) && fm.stride >= (size_t)win->width * 4) {
		uint8_t *dst = (uint8_t *)win->map + (size_t)next * win->slot_len;
		const uint8_t *src = fm.data;

		/*
		 * ROW BY ROW, because the renderer's stride need not equal the
		 * width — copying it as one block would put the padding on the
		 * screen as a diagonal smear, which reads as a decoder fault
		 * and is not one.
		 *
		 * The WHOLE frame is copied even when the damage is one row:
		 * the two slots alternate, so the half being written is a
		 * frame behind and the undamaged part of it is stale. Damage
		 * bounds what the PARENT has to re-send, which is where it
		 * costs something.
		 */
		for (int y = 0; y < win->height; y++)
			memcpy(dst + (size_t)y * win->stride,
			       src + (size_t)y * fm.stride,
			       (size_t)win->width * 4);
		copied = true;
	}

	frame_unmap(buffer, &fm);

	if (!copied)
		return;

	win->slot = next;
	win->map_blank = false;

	/*
	 * THE REGION'S OWN BOXES, NOT THE ONE THAT CONTAINS THEM.
	 *
	 * The parent rounds each box out to the blocks it touches and re-cuts
	 * every one, so the difference between the boxes and their bounding
	 * box is the difference between sending what changed and sending the
	 * window. A page scrolled with a clock ticking in the title bar is two
	 * small rectangles at opposite corners whose extents are everything.
	 *
	 * CAPPED, and past the cap the extents are the honest answer: a region
	 * cut into more pieces than this costs more in messages than it saves
	 * in blocks, because a block is at most sixteen cells square and most
	 * of the pieces land in the same ones.
	 */
	pixman_box32_t whole = { 0, 0, win->width, win->height };
	const pixman_box32_t *boxes = &whole;
	int nbox = 1;

	if (damage) {
		int n = 0;
		const pixman_box32_t *b =
			pixman_region32_rectangles((pixman_region32_t *)damage,
						   &n);

		if (n > 0 && n <= EMBED_MAX_DAMAGE) {
			boxes = b;
			nbox = n;
		} else if (n > EMBED_MAX_DAMAGE) {
			whole = *pixman_region32_extents(
				(pixman_region32_t *)damage);
		}
	}

	for (int i = 0; i < nbox; i++) {
		KembedMsg m = {
			.magic = KEMBED_MAGIC,
			.op = i ? KEMBED_DAMAGE : KEMBED_FRAME,
			.a = next,
			.b = boxes[i].x1,
			.c = boxes[i].y1,
			.d = boxes[i].x2 - boxes[i].x1,
			.e = (uint32_t)(boxes[i].y2 - boxes[i].y1),
			.win = win->win,
		};

		send_msg(e, &m, -1);
	}
}

/*
 * WHAT THE PROBE'S FRAME IS PAINTED WITH, and how long the readback is given.
 *
 * Fully saturated channels and nothing between them: every renderer path this
 * cage can be handed leaves 0.0 at 0 and 1.0 at 0xff whatever transfer curve
 * it applies, so an exact comparison stays exact and a colour with a middle
 * component would not.
 *
 * A gles2 pass into a udmabuf buffer ends in glFlush and not glFinish, and the
 * poll in frame_map() waits only for a fence the driver attached — a udmabuf
 * with none polls readable at once — so the pixels may arrive a moment after
 * the submit returns. The read is retried rather than taken once, and a try
 * that fails outright spends a try and not the probe. The whole probe is
 * therefore bounded by TRIES × (EMBED_FENCE_MS + GAP), under a second, paid
 * once at the start of one cage by a card that is never going to write them.
 */
#define EMBED_PROBE_RED   0x00ff0000u
#define EMBED_PROBE_GREEN 0x0000ff00u
#define EMBED_PROBE_TRIES 8
#define EMBED_PROBE_GAP_MS 5

static uint32_t probe_px(const struct frame_map *fm, int x, int y)
{
	const uint8_t *row = (const uint8_t *)fm->data + (size_t)y * fm->stride;
	uint32_t px;

	memcpy(&px, row + (size_t)x * 4, sizeof(px));
	/* The X byte of an XRGB8888 pixel is whatever the renderer left in
	 * it, so only the colour is compared. */
	return px & 0x00ffffffu;
}

/*
 * THE FOUR CORNERS OF THE PROBE FRAME, against what was painted into it.
 *
 * Three red and one green, in whichever corner the green lands: a renderer's
 * buffer coordinates run from the top left, but a readback that came back
 * mirrored would still be a readback that works, and failing it would cost a
 * working card its hardware path. What no arrangement survives is a corner
 * that is neither colour — the zeroed page of a buffer the GPU never wrote,
 * or a row read at the wrong stride.
 *
 * The format is checked here and not assumed from the allocation: the two
 * constants above are laid out for the pixel embed_publish() copies, so a
 * buffer the renderer handed back in anything else is a path this cage could
 * not publish from even if the pixels did arrive.
 */
static bool probe_frame_painted(const struct frame_map *fm, int w, int h)
{
	static const int corner[4][2] = { { 0, 0 }, { 1, 0 }, { 0, 1 }, { 1, 1 } };
	int greens = 0;

	if (!frame_is_argb32(fm) || fm->stride < (size_t)w * 4)
		return false;

	for (int i = 0; i < 4; i++) {
		uint32_t px = probe_px(fm, corner[i][0] ? w - 1 : 0,
				       corner[i][1] ? h - 1 : 0);

		if (px == EMBED_PROBE_GREEN)
			greens++;
		else if (px != EMBED_PROBE_RED)
			return false;
	}
	return greens == 1;
}

/*
 * ONE RENDERED FRAME, READ BACK THE WAY A PUBLISHED ONE IS.
 *
 * A render pass that reports success is not a render pass whose pixels this
 * process can reach. The buffers an embedded cage draws into are udmabuf ones
 * — memory the kernel backs and the card imports — and a driver is free to
 * accept the import, satisfy every GL call against it and put the result
 * somewhere else entirely. Nothing in wlroots checks; the allocator, the
 * renderer, the output and the publish all report success, the parent receives
 * a full frame of blocks every tick, and the window shows the desk behind it
 * for the life of the process.
 *
 * So the probe paints a frame whose every pixel is known and reads it back
 * through frame_map() — the same road, the same fence and the same bracket
 * embed_publish() uses. Pixels that come back are a card kept; pixels that do
 * not are a card dropped for the software renderer, which gives this guest a
 * picture and costs it hardware GL and hardware video decode — the guest's own
 * Mesa included, because the cage that advertises no dmabuf leaves it wl_shm.
 * THIS FINDS THE MACHINE, IT DOES NOT REPAIR IT: nothing here makes a driver's
 * frames reachable, and the choice is only between a renderer whose frames can
 * be read and a window that stays the colour of the desk.
 *
 * ITS OWN BUFFER AND NOT AN OUTPUT'S, because an output's swapchain belongs to
 * a renderer the caller may be about to throw away, and because the buffer has
 * to be held open across the readback — which is exactly what a committed
 * output will not allow.
 */
bool embed_readback_works(struct cg_server *server)
{
	struct wlr_drm_format fmt = { .format = DRM_FORMAT_XRGB8888 };
	int w = server->embed.first_w, h = server->embed.first_h;
	struct wlr_buffer *buf;
	struct wlr_render_pass *pass;
	bool painted = false;

	if (w < 2 || h < 2)
		return false;

	buf = wlr_allocator_create_buffer(server->allocator, w, h, &fmt);
	if (!buf) {
		wlr_log(WLR_ERROR, "embed: the allocator cannot make a "
				   "%dx%d XRGB8888 buffer", w, h);
		return false;
	}

	pass = wlr_renderer_begin_buffer_pass(server->renderer, buf, NULL);
	if (!pass) {
		wlr_log(WLR_ERROR, "embed: the renderer cannot bind a buffer "
				   "the allocator made");
		wlr_buffer_drop(buf);
		return false;
	}

	/* The whole buffer, then one quadrant of it: a fill alone cannot tell
	 * a frame that arrived from a frame the card happened to clear. */
	wlr_render_pass_add_rect(pass, &(struct wlr_render_rect_options){
		.box = { .x = 0, .y = 0, .width = w, .height = h },
		.color = { .r = 1.0f, .g = 0.0f, .b = 0.0f, .a = 1.0f },
		.blend_mode = WLR_RENDER_BLEND_MODE_NONE,
	});
	wlr_render_pass_add_rect(pass, &(struct wlr_render_rect_options){
		.box = { .x = w / 2, .y = h / 2,
			 .width = w - w / 2, .height = h - h / 2 },
		.color = { .r = 0.0f, .g = 1.0f, .b = 0.0f, .a = 1.0f },
		.blend_mode = WLR_RENDER_BLEND_MODE_NONE,
	});

	if (!wlr_render_pass_submit(pass)) {
		wlr_log(WLR_ERROR, "embed: the renderer refused a render pass "
				   "into a buffer this cage can read");
		wlr_buffer_drop(buf);
		return false;
	}

	for (int attempt = 0; attempt < EMBED_PROBE_TRIES && !painted;
	     attempt++) {
		struct frame_map fm;
		struct timespec gap = {
			.tv_nsec = EMBED_PROBE_GAP_MS * 1000L * 1000L,
		};

		if (attempt)
			nanosleep(&gap, NULL);
		/* A MAP THAT FAILED SPENDS ONE ATTEMPT AND NOT THE PROBE. The
		 * skip_on_stall road is taken here, so an unsignalled fence
		 * returns false — and treating that as the answer would hand a
		 * card that was merely slow on its first frame to llvmpipe for
		 * the life of the cage. */
		if (!frame_map(buf, true, &fm))
			continue;
		painted = probe_frame_painted(&fm, w, h);
		frame_unmap(buf, &fm);
	}

	wlr_buffer_drop(buf);
	return painted;
}
