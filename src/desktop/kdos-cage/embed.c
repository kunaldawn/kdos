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

#include <errno.h>
#include <fcntl.h>
#include <linux/input-event-codes.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <unistd.h>

#include <wlr/types/wlr_buffer.h>
#include <wlr/types/wlr_output.h>
#include <wlr/util/log.h>

#include "embed.h"
#include "kembed.h"
#include "output.h"
#include "seat.h"
#include "server.h"

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
 * A new mapping, because the size changed. The OLD one is unmapped only after
 * the parent has been told about the new one: the parent may still be reading
 * the frame it was last told about, and pulling the memory out from under it is
 * a fault in a process that did nothing wrong.
 */
static bool remap(struct cg_embed *e, int w, int h)
{
	size_t stride = (size_t)w * 4;
	size_t slot = stride * (size_t)h;
	size_t total = slot * KEMBED_SLOTS;

	if (w <= 0 || h <= 0 || total == 0)
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
	};

	if (!send_msg(e, &m, fd)) {
		munmap(map, total);
		close(fd);
		return false;
	}
	close(fd);

	if (e->map)
		munmap(e->map, e->map_len);
	e->map = map;
	e->map_len = total;
	e->slot_len = slot;
	e->stride = stride;
	e->width = w;
	e->height = h;
	e->slot = 0;
	return true;
}

/*
 * The output, at the size the parent asked for. A window resize IS an output
 * resize, so the guest reconfigures exactly the way it would on any compositor
 * — there is no second notion of "the window is smaller than the output".
 */
static void set_size(struct cg_server *server, int w, int h)
{
	struct cg_output *output;

	wl_list_for_each (output, &server->outputs, link) {
		struct wlr_output_state state;

		wlr_output_state_init(&state);
		wlr_output_state_set_custom_mode(&state, w, h, 0);
		wlr_output_commit_state(output->wlr_output, &state);
		wlr_output_state_finish(&state);
		break;	/* embedded is one window and therefore one output */
	}
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
		KembedMsg m;
		ssize_t n = recv(e->fd, &m, sizeof(m), MSG_DONTWAIT);

		if (n < 0) {
			if (errno == EINTR)
				continue;
			return 0;	/* EAGAIN: drained */
		}
		if (n == 0) {
			server_terminate(server);
			return 0;
		}
		/* A short or mistyped message is a peer speaking something
		 * else. There is exactly one peer and it is our parent, so
		 * this is a bug rather than an attack — and either way the
		 * answer is to stop rather than to guess. */
		if (n != (ssize_t)sizeof(m) || m.magic != KEMBED_MAGIC) {
			wlr_log(WLR_ERROR, "kembed: malformed message");
			server_terminate(server);
			return 0;
		}

		switch (m.op) {
		case KEMBED_SIZE:
			if (m.a > 0 && m.b > 0 &&
			    (m.a != e->width || m.b != e->height))
				set_size(server, m.a, m.b);
			break;
		case KEMBED_KEY:
			seat_embed_key(server->seat, (uint32_t)m.a, m.b != 0);
			break;
		case KEMBED_MOTION:
			seat_embed_motion(server->seat, m.a, m.b, m.e);
			break;
		case KEMBED_BUTTON:
			seat_embed_motion(server->seat, m.a, m.b, m.e);
			seat_embed_button(server->seat, (uint32_t)m.c,
					  m.d != 0, m.e);
			break;
		case KEMBED_AXIS:
			seat_embed_motion(server->seat, m.a, m.b, m.e);
			seat_embed_axis(server->seat, m.c, m.e);
			break;
		case KEMBED_FOCUS:
			e->focused = m.a != 0;
			break;
		case KEMBED_SLEEP:
			/*
			 * MINIMISED MEANS STOP RENDERING. A guest drawing
			 * frames nobody is composited into is a guest spending
			 * a core on nothing — which on a battery is the whole
			 * difference between a window and a wasted process.
			 */
			e->asleep = m.a != 0;
			break;
		case KEMBED_CLOSE:
			server_terminate(server);
			return 0;
		default:
			break;
		}
	}
}

bool embed_init(struct cg_server *server, int fd, int width, int height)
{
	struct cg_embed *e = &server->embed;

	e->fd = fd;
	e->slot = 0;

	/*
	 * THE SIZE IS ASSERTED HERE, not assumed. The output was created at it,
	 * so this is normally a no-op — but "the output is the size the parent
	 * asked for" is the invariant every frame below depends on, and it
	 * costs one commit to make it true however the output came to exist.
	 */
	set_size(server, width, height);

	if (!remap(e, width, height))
		return false;

	e->source = wl_event_loop_add_fd(wl_display_get_event_loop(server->wl_display),
					 fd, WL_EVENT_READABLE, handle_readable,
					 server);
	if (!e->source)
		return false;

	KembedMsg m = { .magic = KEMBED_MAGIC, .op = KEMBED_HELLO };

	e->active = true;
	e->focused = true;
	return send_msg(e, &m, -1);
}

void embed_finish(struct cg_server *server)
{
	struct cg_embed *e = &server->embed;

	if (!e->active)
		return;

	KembedMsg m = { .magic = KEMBED_MAGIC, .op = KEMBED_GONE };

	send_msg(e, &m, -1);
	if (e->source)
		wl_event_source_remove(e->source);
	if (e->map)
		munmap(e->map, e->map_len);
	e->map = NULL;
	e->source = NULL;
	e->active = false;
}

bool embed_active(struct cg_server *server)
{
	return server->embed.active;
}

bool embed_asleep(struct cg_server *server)
{
	return server->embed.active && server->embed.asleep;
}

void embed_publish(struct cg_server *server, struct wlr_buffer *buffer,
		   const pixman_region32_t *damage)
{
	struct cg_embed *e = &server->embed;
	void *data = NULL;
	uint32_t format = 0;
	size_t stride = 0;

	if (!e->active || !e->map || !buffer)
		return;

	if (buffer->width != e->width || buffer->height != e->height) {
		/* The output resized and this is the first frame at the new
		 * size: the mapping follows the buffer, not the request. */
		if (!remap(e, buffer->width, buffer->height))
			return;
	}

	if (!wlr_buffer_begin_data_ptr_access(buffer, WLR_BUFFER_DATA_PTR_ACCESS_READ,
					      &data, &format, &stride))
		return;

	/*
	 * 32 BITS PER PIXEL AND NOTHING ELSE. The pixman renderer on a headless
	 * output gives XRGB8888 or ARGB8888; anything else is a wlroots that
	 * chose a format this was not written for, and copying it as if it were
	 * one of those would put garbage on a screen rather than fail.
	 */
	if (stride < (size_t)e->width * 4) {
		wlr_buffer_end_data_ptr_access(buffer);
		return;
	}

	int next = (e->slot + 1) % KEMBED_SLOTS;
	uint8_t *dst = (uint8_t *)e->map + (size_t)next * e->slot_len;
	const uint8_t *src = data;

	/*
	 * ROW BY ROW, because the renderer's stride need not equal the width —
	 * copying it as one block would put the padding on the screen as a
	 * diagonal smear, which reads as a decoder fault and is not one.
	 *
	 * The WHOLE frame is copied even when the damage is one row: the two
	 * slots alternate, so the half being written is a frame behind and the
	 * undamaged part of it is stale. Damage bounds what the PARENT has to
	 * re-send, which is where it costs something.
	 */
	for (int y = 0; y < e->height; y++)
		memcpy(dst + (size_t)y * e->stride, src + (size_t)y * stride,
		       (size_t)e->width * 4);

	wlr_buffer_end_data_ptr_access(buffer);

	e->slot = next;

	pixman_box32_t box = { 0, 0, e->width, e->height };

	if (damage && pixman_region32_not_empty(damage)) {
		pixman_box32_t *ext = pixman_region32_extents((pixman_region32_t *)damage);

		box = *ext;
	}

	KembedMsg m = {
		.magic = KEMBED_MAGIC,
		.op = KEMBED_FRAME,
		.a = next,
		.b = box.x1,
		.c = box.y1,
		.d = box.x2 - box.x1,
		.e = (uint32_t)(box.y2 - box.y1),
	};

	send_msg(e, &m, -1);
}
