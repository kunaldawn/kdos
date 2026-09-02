/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   kdos-con — a graphical application as a WINDOW
 *
 * A Wayland client's surface is pixels and this desktop composites characters.
 * The compositing that reconciles the two happens in a SEPARATE PROCESS —
 * kdos-cage --embed, one per window — and this file is the parent half of the
 * private channel to it. kdos-con still links no wlroots, no mesa and no pixel
 * library at all: it moves a blob of bytes it never looks at.
 *
 * THE FRAME BECOMES SPRITES, not a rectangle of pixels drawn over the grid.
 * A sprite lives IN A CELL, so a window on top of an embedded one simply
 * overwrites those cells and the occlusion is the z-ordered copy that was
 * already there. A pixel region painted alongside the grid would cover
 * whatever was above it, and every window-model question — stacking, snapping,
 * workspaces — would need a second answer for one kind of window.
 *
 * A PICTURE IS SIXTEEN CELLS SQUARE AT MOST, because that is what the cell's
 * sprite encoding carries, so a window is a grid of blocks and damage is
 * rounded out to the blocks it touches.
 *
 * THE DESCRIPTOR IS THE ONE PLACE ONE APPEARS. It is a socketpair created
 * before the fork and inherited — never a path anything can connect to — which
 * is what keeps the surface and view protocols descriptor-free and therefore
 * forwardable over ssh.
 * ---------------------------------
 */

#include <errno.h>
#include <fcntl.h>
#include <linux/input-event-codes.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#include "con.h"
#include "kbase.h"
#include "kembed.h"

/* One sprite is sixteen cells square at most — the cell encoding's four bits
 * per axis — so a window is a grid of blocks that size. */
#define EM_TILE 16
#define EM_MAX_BLOCKS 256

/*
 * A CELL SIZE FOR A SESSION THAT HAS NOT BEEN TOLD ONE. A view says how many
 * pixels its cells are; a terminal view has no answer, and a guest still has
 * to be given a size. Eight by sixteen is the console font's, so an embedded
 * application rendered for a terminal view has the aspect ratio the characters
 * that will represent it do.
 */
#define EM_CELL_W 8
#define EM_CELL_H 16

/*
 * HOW OFTEN A FRAME MAY BE SENT WHEN NOTHING CAN SHOW PIXELS. Every attached
 * view still receives the picture — a terminal view matches it to characters —
 * but a window of pixels at a compositor's frame rate down an ssh link is a
 * link that does nothing else.
 */
#define EM_SLOW_MS 250

struct Embed {
	Win *win;
	pid_t pid;
	int fd;

	void *map;
	size_t map_len, slot_len;
	int pw, ph;			/* the mapping, in pixels */
	size_t stride;
	int slot;			/* the half holding the current frame */

	int cell_w, cell_h;
	int cols, rows;			/* the size the guest was asked for */
	int bw, bh;			/* blocks across and down */
	int slots[EM_MAX_BLOCKS];	/* session sprite slots, -1 unassigned */

	uint32_t *scratch;		/* one block, contiguous */
	size_t scratch_px;

	int dx0, dy0, dx1, dy1;		/* pending damage, in pixels */
	int have_damage;
	unsigned long long last_ms;

	int gone;			/* the guest exited */
	int focused, asleep;
};

static unsigned long long now_ms(void)
{
	struct timespec ts;

	clock_gettime(CLOCK_MONOTONIC, &ts);
	return (unsigned long long)ts.tv_sec * 1000ull +
	       (unsigned long long)(ts.tv_nsec / 1000000);
}

/* ── the wire ────────────────────────────────────────────────────────── */

static int send_msg(struct Embed *e, unsigned op, int a, int b, int c, int d,
		    unsigned f)
{
	KembedMsg m = { .magic = KEMBED_MAGIC, .op = op, .a = a, .b = b,
			.c = c, .d = d, .e = f };

	if (e->fd < 0)
		return -1;
	while (send(e->fd, &m, sizeof(m), MSG_NOSIGNAL) < 0) {
		if (errno == EINTR)
			continue;
		return -1;
	}
	return 0;
}

/*
 * One message, and the descriptor it may carry. A short read is a peer
 * speaking something else; there is exactly one peer and it is our own child,
 * so the answer is to stop talking to it rather than to guess.
 */
static int recv_msg(struct Embed *e, KembedMsg *m, int *fd)
{
	struct iovec iov = { .iov_base = m, .iov_len = sizeof(*m) };
	struct msghdr hdr = { .msg_iov = &iov, .msg_iovlen = 1 };
	union {
		char buf[CMSG_SPACE(sizeof(int))];
		struct cmsghdr align;
	} u;
	ssize_t n;

	*fd = -1;
	memset(&u, 0, sizeof(u));
	hdr.msg_control = u.buf;
	hdr.msg_controllen = sizeof(u.buf);

	do {
		n = recvmsg(e->fd, &hdr, MSG_DONTWAIT);
	} while (n < 0 && errno == EINTR);

	if (n < 0)
		return errno == EAGAIN || errno == EWOULDBLOCK ? 0 : -1;
	if (n == 0)
		return -1;
	if (n != (ssize_t)sizeof(*m) || m->magic != KEMBED_MAGIC)
		return -1;

	for (struct cmsghdr *c = CMSG_FIRSTHDR(&hdr); c; c = CMSG_NXTHDR(&hdr, c))
		if (c->cmsg_level == SOL_SOCKET && c->cmsg_type == SCM_RIGHTS)
			memcpy(fd, CMSG_DATA(c), sizeof(int));

	return 1;
}

/* ── blocks ──────────────────────────────────────────────────────────── */

/*
 * The whole window is damaged. Used when the geometry changed, when a new
 * mapping arrived and when a view attaches: a view that has never been sent a
 * block draws the fallback mark where the picture should be.
 */
static void damage_all(struct Embed *e)
{
	e->dx0 = 0;
	e->dy0 = 0;
	e->dx1 = e->pw;
	e->dy1 = e->ph;
	e->have_damage = 1;
}

static void damage_add(struct Embed *e, int x, int y, int w, int h)
{
	if (w <= 0 || h <= 0)
		return;
	if (!e->have_damage) {
		e->dx0 = x;
		e->dy0 = y;
		e->dx1 = x + w;
		e->dy1 = y + h;
		e->have_damage = 1;
		return;
	}
	if (x < e->dx0)
		e->dx0 = x;
	if (y < e->dy0)
		e->dy0 = y;
	if (x + w > e->dx1)
		e->dx1 = x + w;
	if (y + h > e->dy1)
		e->dy1 = y + h;
}

/*
 * The blocks a window is cut into, and a session sprite slot for each. The
 * slots come from the server's own rotation, the same one surfaces draw from:
 * a session numbering its own pictures separately would eventually hand a view
 * a number a surface is already using.
 */
static int layout(struct Embed *e, int cols, int rows)
{
	e->cols = cols;
	e->rows = rows;
	e->bw = (cols + EM_TILE - 1) / EM_TILE;
	e->bh = (rows + EM_TILE - 1) / EM_TILE;

	if (e->bw < 1 || e->bh < 1 || e->bw * e->bh > EM_MAX_BLOCKS)
		return -1;

	for (int i = 0; i < e->bw * e->bh; i++)
		if (e->slots[i] < 0)
			e->slots[i] = kcon_server_alloc_slot(S.server);

	size_t px = (size_t)EM_TILE * e->cell_w * EM_TILE * e->cell_h;

	if (px > e->scratch_px) {
		uint32_t *p = realloc(e->scratch, px * 4);

		if (!p)
			return -1;
		e->scratch = p;
		e->scratch_px = px;
	}
	return 0;
}

/*
 * Cut one block out of the mapping and hand it to every attached view. The
 * rows are copied rather than pointed at: the mapping's stride is the whole
 * window's and a sprite's bytes have to be contiguous, and the half being read
 * is the one the child is not writing.
 */
static void send_block(struct Embed *e, int bx, int by)
{
	int cx = bx * EM_TILE, cy = by * EM_TILE;
	int cw = e->cols - cx, ch = e->rows - cy;

	if (cw > EM_TILE)
		cw = EM_TILE;
	if (ch > EM_TILE)
		ch = EM_TILE;
	if (cw < 1 || ch < 1)
		return;

	int px = cx * e->cell_w, py = cy * e->cell_h;
	int pw = cw * e->cell_w, ph = ch * e->cell_h;

	if (px + pw > e->pw || py + ph > e->ph)
		return;

	const uint8_t *base = (const uint8_t *)e->map +
			      (size_t)e->slot * e->slot_len;

	for (int y = 0; y < ph; y++)
		memcpy(e->scratch + (size_t)y * pw,
		       base + (size_t)(py + y) * e->stride + (size_t)px * 4,
		       (size_t)pw * 4);

	int slot = e->slots[by * e->bw + bx];

	if (slot < 0)
		return;

	/*
	 * WHAT A PICTURE LOOKS LIKE WHERE THERE ARE NO PIXELS. Something
	 * rather than nothing: a window that rendered as blank cells is
	 * indistinguishable from one that never drew.
	 */
	uint32_t fb = 0x2593u;

	for (int i = 0; i < kcon_server_view_count(S.server); i++)
		kcon_view_sprite(kcon_server_view_at(S.server, i), slot,
				 cw, ch, fb, e->scratch, pw, ph);
}

/* Does anything attached turn a sprite's bytes into pixels? */
static int any_pixel_view(void)
{
	for (int i = 0; i < kcon_server_view_count(S.server); i++)
		if (kcon_view_caps(kcon_server_view_at(S.server, i)) &
		    KCON_VIEW_PIXELS)
			return 1;
	return 0;
}

static void publish(struct Embed *e)
{
	if (!e->have_damage || !e->map || e->slot < 0 || e->asleep)
		return;

	unsigned long long t = now_ms();

	if (!any_pixel_view() && t - e->last_ms < EM_SLOW_MS)
		return;
	e->last_ms = t;

	int bx0 = e->dx0 / (EM_TILE * e->cell_w);
	int by0 = e->dy0 / (EM_TILE * e->cell_h);
	int bx1 = (e->dx1 + EM_TILE * e->cell_w - 1) / (EM_TILE * e->cell_w);
	int by1 = (e->dy1 + EM_TILE * e->cell_h - 1) / (EM_TILE * e->cell_h);

	if (bx0 < 0)
		bx0 = 0;
	if (by0 < 0)
		by0 = 0;
	if (bx1 > e->bw)
		bx1 = e->bw;
	if (by1 > e->bh)
		by1 = e->bh;

	for (int by = by0; by < by1; by++)
		for (int bx = bx0; bx < bx1; bx++)
			send_block(e, bx, by);

	e->have_damage = 0;
}

/* ── which way a graphical application is shown ──────────────────────── */

/*
 * THE NAME A POLICY IS KEYED ON. A generated launcher runs `kdos-appbox run
 * <app>`, so the interesting word is the third one; anything else is named by
 * its own program.
 */
static const char *guest_name(const char *const argv[])
{
	if (argv[0] && argv[1] && argv[2] && !strcmp(argv[1], "run") &&
	    strstr(argv[0], "kdos-appbox"))
		return argv[2];

	const char *base = strrchr(argv[0], '/');

	return base ? base + 1 : argv[0];
}

/* `display` out of a box profile, or "". The same file `kdos-box profile`
 * writes and the settings surface edits — a second store for one key would be
 * a second place to look when an application comes up on the wrong thing. */
static void profile_display(const char *name, char *out, size_t cap)
{
	char path[512];
	const char *cfg = getenv("XDG_CONFIG_HOME");
	char *data;

	out[0] = '\0';
	if (!name || !*name || strchr(name, '/'))
		return;

	if (cfg && *cfg)
		snprintf(path, sizeof(path), "%.400s/kdos/boxes/%.63s.conf",
			 cfg, name);
	else
		snprintf(path, sizeof(path), "%.400s/.config/kdos/boxes/%.63s.conf",
			 kb_home_dir(), name);

	data = kb_read_all(path, NULL);
	if (!data)
		return;

	for (char *line = data, *next; line && *line; line = next) {
		char *nl = strchr(line, '\n');

		next = nl ? nl + 1 : line + strlen(line);
		if (nl)
			*nl = '\0';
		while (*line == ' ' || *line == '\t')
			line++;
		if (strncmp(line, "display", 7))
			continue;

		char *eq = strchr(line, '=');

		if (!eq)
			continue;
		eq++;
		while (*eq == ' ' || *eq == '\t')
			eq++;
		snprintf(out, cap, "%s", eq);
		break;
	}
	free(data);
}

/*
 * EMBEDDING IS THE DEFAULT and a terminal of its own is the exception, because
 * an embedded guest is composited by pixman on the CPU: fine for an editor, not
 * a way to play a game. Every rule that overrides it says so, so that `kdos
 * doctor` and a person reading a log get the same sentence.
 */
int con_display_mode(const char *const argv[], const char **why)
{
	static char reason[160];
	char disp[64];

	*why = reason;

	if (!argv || !argv[0]) {
		snprintf(reason, sizeof(reason), "there is nothing to run");
		return CON_DISPLAY_VT;
	}

	profile_display(guest_name(argv), disp, sizeof(disp));

	if (!strcmp(disp, "vt")) {
		snprintf(reason, sizeof(reason),
			 "its box profile says display = vt");
		return CON_DISPLAY_VT;
	}
	if (!kcon_conf_bool("embed", 1)) {
		snprintf(reason, sizeof(reason),
			 "con.conf says embed = false");
		return CON_DISPLAY_VT;
	}

	snprintf(reason, sizeof(reason),
		 "embedding is what a graphical application gets");
	return CON_DISPLAY_EMBED;
}

/* ── the child ───────────────────────────────────────────────────────── */

static struct Embed *embeds[32];
static int nembeds;

/* The cell size a guest is rendered at: the primary view's, because that is
 * the display the person is looking at. A second view of a different font
 * rescales the sprite, which is what it already does for every other picture. */
static void cell_size(int *w, int *h)
{
	KconSurface *v = S.server ? kcon_server_view_at(S.server, 0) : NULL;

	*w = v ? kcon_view_cell_w(v) : 0;
	*h = v ? kcon_view_cell_h(v) : 0;
	if (*w < 2 || *h < 2 || *w > 64 || *h > 64) {
		*w = EM_CELL_W;
		*h = EM_CELL_H;
	}
}

Win *embed_open(const char *const argv[], const char *title)
{
	int sv[2];

	if (!argv || !argv[0] || nembeds >= (int)(sizeof(embeds) / sizeof(embeds[0])))
		return NULL;

	struct Embed *e = calloc(1, sizeof(*e));

	if (!e)
		return NULL;
	for (int i = 0; i < EM_MAX_BLOCKS; i++)
		e->slots[i] = -1;
	e->slot = -1;
	e->fd = -1;
	e->focused = 1;

	cell_size(&e->cell_w, &e->cell_h);

	Win *w = calloc(1, sizeof(*w));

	if (!w) {
		free(e);
		return NULL;
	}

	w->kind = WIN_EMBED;
	w->id = ++S.next_id;
	w->workspace = S.workspace;
	w->em = e;
	e->win = w;
	snprintf(w->title, sizeof(w->title), "%s",
		 title && *title ? title : argv[0]);
	snprintf(w->app_id, sizeof(w->app_id), "%s", argv[0]);

	/* Half the workarea, placed by the window model like anything else —
	 * an embedded application is a window and is given a window's size. */
	KwmRect area = win_workarea();

	win_place(w, area.w / 2, area.h / 2);

	if (layout(e, w->geom.w, w->geom.h) != 0) {
		free(w);
		free(e);
		return NULL;
	}

	if (socketpair(AF_UNIX, SOCK_SEQPACKET | SOCK_CLOEXEC, 0, sv) != 0) {
		free(e->scratch);
		free(w);
		free(e);
		return NULL;
	}

	char geom[32];

	snprintf(geom, sizeof(geom), "%dx%d", e->cols * e->cell_w,
		 e->rows * e->cell_h);

	const char *av[KCON_MAX_ARGV + 4];
	int n = 0;

	av[n++] = "kdos-cage";
	av[n++] = "--embed";
	av[n++] = geom;
	av[n++] = "--";
	for (int i = 0; argv[i] && n < (int)(sizeof(av) / sizeof(av[0])) - 1; i++)
		av[n++] = argv[i];
	av[n] = NULL;

	pid_t pid = fork();

	if (pid < 0) {
		close(sv[0]);
		close(sv[1]);
		free(e->scratch);
		free(w);
		free(e);
		return NULL;
	}

	if (pid == 0) {
		int null = open("/dev/null", O_RDWR);

		/* The inherited descriptor, at the number both halves name.
		 * dup2 clears close-on-exec, which is what makes it survive. */
		if (sv[1] != KEMBED_FD) {
			dup2(sv[1], KEMBED_FD);
			close(sv[1]);
		} else {
			fcntl(sv[1], F_SETFD, 0);
		}
		close(sv[0]);

		if (null >= 0) {
			dup2(null, 0);
			dup2(null, 1);
			if (null > 2)
				close(null);
		}

		/*
		 * A US KEYMAP, AND THE REASON IS THE PATH A KEY TAKES. The view
		 * resolved the person's own layout to a character before the
		 * session ever saw it; what goes to the guest is the key that
		 * produces that character on a US keyboard, so the guest has to
		 * be reading one. An application that reads raw scancodes sees
		 * US positions.
		 */
		setenv("XKB_DEFAULT_RULES", "evdev", 1);
		setenv("XKB_DEFAULT_MODEL", "pc105", 1);
		setenv("XKB_DEFAULT_LAYOUT", "us", 1);
		setenv("XKB_DEFAULT_VARIANT", "", 1);
		setenv("XKB_DEFAULT_OPTIONS", "", 1);

		/* The guest is a client of the compositor we are starting, not
		 * of this session and not of anything outside it. */
		unsetenv("KDOS_CON");
		unsetenv("WAYLAND_DISPLAY");
		unsetenv("DISPLAY");

		execvp(av[0], (char *const *)av);
		_exit(127);
	}

	close(sv[1]);
	e->pid = pid;
	e->fd = sv[0];

	w->next = S.wins;
	S.wins = w;
	S.focus = w->id;
	embeds[nembeds++] = e;
	return w;
}

/* ── messages from the child ─────────────────────────────────────────── */

static void take_buf(struct Embed *e, int fd, int w, int h, size_t stride,
		     size_t slot_len)
{
	if (fd < 0)
		return;
	if (w <= 0 || h <= 0 || stride < (size_t)w * 4 ||
	    slot_len < stride * (size_t)h) {
		close(fd);
		return;
	}

	size_t total = slot_len * KEMBED_SLOTS;
	void *map = mmap(NULL, total, PROT_READ, MAP_SHARED, fd, 0);

	close(fd);
	if (map == MAP_FAILED)
		return;

	if (e->map)
		munmap(e->map, e->map_len);
	e->map = map;
	e->map_len = total;
	e->slot_len = slot_len;
	e->stride = stride;
	e->pw = w;
	e->ph = h;
	e->slot = -1;
	damage_all(e);
}

static void drain(struct Embed *e)
{
	for (;;) {
		KembedMsg m;
		int fd = -1;
		int r = recv_msg(e, &m, &fd);

		if (r == 0)
			return;
		if (r < 0) {
			e->gone = 1;
			return;
		}

		switch (m.op) {
		case KEMBED_HELLO:
			break;
		case KEMBED_BUF:
			take_buf(e, fd, m.a, m.b, (size_t)m.c, (size_t)m.d);
			break;
		case KEMBED_FRAME:
			if (fd >= 0)
				close(fd);
			if (m.a < 0 || m.a >= KEMBED_SLOTS || !e->map)
				break;
			e->slot = m.a;
			damage_add(e, m.b, m.c, m.d, (int)m.e);
			break;
		case KEMBED_GONE:
			if (fd >= 0)
				close(fd);
			e->gone = 1;
			break;
		default:
			if (fd >= 0)
				close(fd);
			break;
		}
	}
}

/* ── the session's side of the loop ──────────────────────────────────── */

int embed_fds(int *fds, int max)
{
	int n = 0;

	for (int i = 0; i < nembeds && n < max; i++)
		if (embeds[i]->fd >= 0)
			fds[n++] = embeds[i]->fd;
	return n;
}

void embed_pump(void)
{
	for (int i = 0; i < nembeds; i++) {
		struct Embed *e = embeds[i];
		Win *w = e->win;

		if (e->fd >= 0)
			drain(e);

		if (!w)
			continue;

		/*
		 * THE GUEST IS TOLD WHAT THE WINDOW MODEL DECIDED. Focus so it
		 * knows whether the keyboard is its own; sleep so a minimised
		 * application stops rendering frames nobody composites, which
		 * on a battery is the whole difference between a window and a
		 * wasted process.
		 */
		int want_focus = w->id == S.focus && !S.locked;

		if (want_focus != e->focused) {
			e->focused = want_focus;
			send_msg(e, KEMBED_FOCUS, want_focus, 0, 0, 0, 0);
		}
		if (w->minimised != e->asleep) {
			e->asleep = w->minimised;
			send_msg(e, KEMBED_SLEEP, e->asleep, 0, 0, 0, 0);
			if (!e->asleep)
				damage_all(e);
		}

		publish(e);
	}
}

/*
 * The window's cells changed size, so the guest's output does. A resize IS an
 * output resize there, which is how the application reconfigures the way it
 * would on any compositor.
 */
void embed_resized(Win *w)
{
	struct Embed *e = w ? w->em : NULL;

	if (!e)
		return;
	if (w->geom.w == e->cols && w->geom.h == e->rows)
		return;

	int cw = e->cell_w, chh = e->cell_h;

	cell_size(&cw, &chh);
	e->cell_w = cw;
	e->cell_h = chh;

	if (layout(e, w->geom.w, w->geom.h) != 0)
		return;
	send_msg(e, KEMBED_SIZE, e->cols * e->cell_w, e->rows * e->cell_h,
		 0, 0, 0);
	damage_all(e);
}

/*
 * A VIEW ATTACHED, so every block has to go out again: a view holds the
 * pictures it was sent and a new one was sent none.
 */
void embed_view_attached(void)
{
	for (int i = 0; i < nembeds; i++) {
		damage_all(embeds[i]);
		embeds[i]->last_ms = 0;
	}
}

void embed_close(Win *w)
{
	struct Embed *e = w ? w->em : NULL;

	if (!e)
		return;
	send_msg(e, KEMBED_CLOSE, 0, 0, 0, 0, 0);
	if (e->pid > 0)
		kill(e->pid, SIGTERM);
}

void embed_close_all(void)
{
	for (int i = 0; i < nembeds; i++)
		if (embeds[i]->pid > 0)
			kill(embeds[i]->pid, SIGTERM);
}

/*
 * A guest that exited closes its window. Polled rather than driven by SIGCHLD,
 * for the reason vt_reap is: the session already wakes on a timer, and a
 * handler would be a signal racing the window list.
 */
void embed_reap(void)
{
	for (int i = 0; i < nembeds; i++) {
		struct Embed *e = embeds[i];

		if (e->pid > 0 && waitpid(e->pid, NULL, WNOHANG) == e->pid)
			e->pid = 0, e->gone = 1;
		if (e->gone && e->pid == 0 && e->win)
			win_close(e->win);
	}
}

/* Called from win_close once the window is going for good. */
void embed_free(Win *w)
{
	struct Embed *e = w ? w->em : NULL;

	if (!e)
		return;
	w->em = NULL;

	for (int i = 0; i < nembeds; i++)
		if (embeds[i] == e) {
			embeds[i] = embeds[--nembeds];
			break;
		}

	if (e->fd >= 0)
		close(e->fd);
	if (e->map)
		munmap(e->map, e->map_len);
	free(e->scratch);
	free(e);
}

int embed_alive(const Win *w)
{
	return w && w->em && w->em->pid > 0 && !w->em->gone;
}

/* ── drawing ─────────────────────────────────────────────────────────── */

/*
 * The window's cells ARE the picture: each one names the block covering it and
 * which cell of that block it is. Nothing here touches a pixel — the bytes
 * went to the views as sprites and this is the reference to them.
 */
void embed_draw(const Win *w)
{
	const struct Embed *e = w ? w->em : NULL;

	if (!e)
		return;

	for (int y = 0; y < w->geom.h && y < e->rows; y++)
		for (int x = 0; x < w->geom.w && x < e->cols; x++) {
			int slot = e->slots[(y / EM_TILE) * e->bw +
					    (x / EM_TILE)];
			uint32_t ch;

			if (slot < 0)
				continue;
			ch = KTUI_SPRITE_BASE |
			     ((uint32_t)slot << 8) |
			     ((uint32_t)(y % EM_TILE) << 4) |
			     (uint32_t)(x % EM_TILE);
			ktui_draw_cell(w->geom.x + x, w->geom.y + y, ch,
				       KT_TEXT, KT_BG, 0);
		}
}

/* ── input ───────────────────────────────────────────────────────────── */

/*
 * A CHARACTER BACK TO THE KEY THAT PRODUCES IT, on the US keymap the guest is
 * started with. The view resolved the person's own layout to a character
 * already; this is the other half of that trip, and it is a table rather than
 * a keymap because the session links no xkb and must not.
 */
struct KeyCode {
	int key;
	uint16_t code;
	uint8_t shift;
};

static const struct KeyCode keymap[] = {
	{ 'a', KEY_A, 0 }, { 'b', KEY_B, 0 }, { 'c', KEY_C, 0 },
	{ 'd', KEY_D, 0 }, { 'e', KEY_E, 0 }, { 'f', KEY_F, 0 },
	{ 'g', KEY_G, 0 }, { 'h', KEY_H, 0 }, { 'i', KEY_I, 0 },
	{ 'j', KEY_J, 0 }, { 'k', KEY_K, 0 }, { 'l', KEY_L, 0 },
	{ 'm', KEY_M, 0 }, { 'n', KEY_N, 0 }, { 'o', KEY_O, 0 },
	{ 'p', KEY_P, 0 }, { 'q', KEY_Q, 0 }, { 'r', KEY_R, 0 },
	{ 's', KEY_S, 0 }, { 't', KEY_T, 0 }, { 'u', KEY_U, 0 },
	{ 'v', KEY_V, 0 }, { 'w', KEY_W, 0 }, { 'x', KEY_X, 0 },
	{ 'y', KEY_Y, 0 }, { 'z', KEY_Z, 0 },
	{ 'A', KEY_A, 1 }, { 'B', KEY_B, 1 }, { 'C', KEY_C, 1 },
	{ 'D', KEY_D, 1 }, { 'E', KEY_E, 1 }, { 'F', KEY_F, 1 },
	{ 'G', KEY_G, 1 }, { 'H', KEY_H, 1 }, { 'I', KEY_I, 1 },
	{ 'J', KEY_J, 1 }, { 'K', KEY_K, 1 }, { 'L', KEY_L, 1 },
	{ 'M', KEY_M, 1 }, { 'N', KEY_N, 1 }, { 'O', KEY_O, 1 },
	{ 'P', KEY_P, 1 }, { 'Q', KEY_Q, 1 }, { 'R', KEY_R, 1 },
	{ 'S', KEY_S, 1 }, { 'T', KEY_T, 1 }, { 'U', KEY_U, 1 },
	{ 'V', KEY_V, 1 }, { 'W', KEY_W, 1 }, { 'X', KEY_X, 1 },
	{ 'Y', KEY_Y, 1 }, { 'Z', KEY_Z, 1 },
	{ '1', KEY_1, 0 }, { '2', KEY_2, 0 }, { '3', KEY_3, 0 },
	{ '4', KEY_4, 0 }, { '5', KEY_5, 0 }, { '6', KEY_6, 0 },
	{ '7', KEY_7, 0 }, { '8', KEY_8, 0 }, { '9', KEY_9, 0 },
	{ '0', KEY_0, 0 },
	{ '!', KEY_1, 1 }, { '@', KEY_2, 1 }, { '#', KEY_3, 1 },
	{ '$', KEY_4, 1 }, { '%', KEY_5, 1 }, { '^', KEY_6, 1 },
	{ '&', KEY_7, 1 }, { '*', KEY_8, 1 }, { '(', KEY_9, 1 },
	{ ')', KEY_0, 1 },
	{ ' ', KEY_SPACE, 0 },
	{ '-', KEY_MINUS, 0 }, { '_', KEY_MINUS, 1 },
	{ '=', KEY_EQUAL, 0 }, { '+', KEY_EQUAL, 1 },
	{ '[', KEY_LEFTBRACE, 0 }, { '{', KEY_LEFTBRACE, 1 },
	{ ']', KEY_RIGHTBRACE, 0 }, { '}', KEY_RIGHTBRACE, 1 },
	{ ';', KEY_SEMICOLON, 0 }, { ':', KEY_SEMICOLON, 1 },
	{ '\'', KEY_APOSTROPHE, 0 }, { '"', KEY_APOSTROPHE, 1 },
	{ '`', KEY_GRAVE, 0 }, { '~', KEY_GRAVE, 1 },
	{ '\\', KEY_BACKSLASH, 0 }, { '|', KEY_BACKSLASH, 1 },
	{ ',', KEY_COMMA, 0 }, { '<', KEY_COMMA, 1 },
	{ '.', KEY_DOT, 0 }, { '>', KEY_DOT, 1 },
	{ '/', KEY_SLASH, 0 }, { '?', KEY_SLASH, 1 },
	{ KT_K_ESC, KEY_ESC, 0 },
	{ KT_K_ENTER, KEY_ENTER, 0 },
	{ KT_K_TAB, KEY_TAB, 0 },
	{ KT_K_BTAB, KEY_TAB, 1 },
	{ KT_K_BACKSPACE, KEY_BACKSPACE, 0 },
	{ KT_K_UP, KEY_UP, 0 }, { KT_K_DOWN, KEY_DOWN, 0 },
	{ KT_K_LEFT, KEY_LEFT, 0 }, { KT_K_RIGHT, KEY_RIGHT, 0 },
	{ KT_K_HOME, KEY_HOME, 0 }, { KT_K_END, KEY_END, 0 },
	{ KT_K_PGUP, KEY_PAGEUP, 0 }, { KT_K_PGDN, KEY_PAGEDOWN, 0 },
	{ KT_K_INS, KEY_INSERT, 0 }, { KT_K_DEL, KEY_DELETE, 0 },
	{ KT_K_F1, KEY_F1, 0 }, { KT_K_F2, KEY_F2, 0 },
	{ KT_K_F3, KEY_F3, 0 }, { KT_K_F4, KEY_F4, 0 },
	{ KT_K_F5, KEY_F5, 0 }, { KT_K_F6, KEY_F6, 0 },
	{ KT_K_F7, KEY_F7, 0 }, { KT_K_F8, KEY_F8, 0 },
	{ KT_K_F9, KEY_F9, 0 }, { KT_K_F10, KEY_F10, 0 },
	{ KT_K_F11, KEY_F11, 0 }, { KT_K_F12, KEY_F12, 0 },
};

static void tap(struct Embed *e, uint16_t code, int down)
{
	send_msg(e, KEMBED_KEY, code, down, 0, 0, 0);
}

int embed_key(Win *w, const KtuiEvent *ev)
{
	struct Embed *e = w ? w->em : NULL;

	if (!e || e->fd < 0)
		return 0;

	const struct KeyCode *k = NULL;

	for (size_t i = 0; i < sizeof(keymap) / sizeof(keymap[0]); i++)
		if (keymap[i].key == ev->key) {
			k = &keymap[i];
			break;
		}
	if (!k)
		return 0;

	/*
	 * PRESS AND RELEASE, both here. The view reports a key as one event —
	 * it is a character, not a switch — so a guest that was sent only the
	 * press would hold every key it was ever given down.
	 */
	int shift = k->shift || (ev->mods & KT_MOD_SHIFT);

	if (shift)
		tap(e, KEY_LEFTSHIFT, 1);
	if (ev->mods & KT_MOD_CTRL)
		tap(e, KEY_LEFTCTRL, 1);
	if (ev->mods & KT_MOD_ALT)
		tap(e, KEY_LEFTALT, 1);

	tap(e, k->code, 1);
	tap(e, k->code, 0);

	if (ev->mods & KT_MOD_ALT)
		tap(e, KEY_LEFTALT, 0);
	if (ev->mods & KT_MOD_CTRL)
		tap(e, KEY_LEFTCTRL, 0);
	if (shift)
		tap(e, KEY_LEFTSHIFT, 0);
	return 1;
}

int embed_ptr(Win *w, const KtuiEvent *ev)
{
	struct Embed *e = w ? w->em : NULL;

	if (!e || e->fd < 0)
		return 0;

	/*
	 * THE CELL, PLUS WHERE IN IT. A cell is several pixels wide and a
	 * guest's buttons are smaller than one, so a view that knows its own
	 * pixel geometry says where inside the cell the pointer was; one that
	 * does not means its centre.
	 */
	int cx = ev->mx - w->geom.x, cy = ev->my - w->geom.y;
	int px = cx * e->cell_w + e->cell_w / 2 +
		 ev->subx * e->cell_w / 256;
	int py = cy * e->cell_h + e->cell_h / 2 +
		 ev->suby * e->cell_h / 256;

	if (px < 0)
		px = 0;
	if (py < 0)
		py = 0;
	if (px >= e->pw && e->pw > 0)
		px = e->pw - 1;
	if (py >= e->ph && e->ph > 0)
		py = e->ph - 1;

	unsigned ms = (unsigned)now_ms();

	switch (ev->btn) {
	case KT_MB_LEFT:
	case KT_MB_MIDDLE:
	case KT_MB_RIGHT: {
		static const int btn[] = { BTN_LEFT, BTN_MIDDLE, BTN_RIGHT };

		if (ev->press == KT_MP_DRAG)
			send_msg(e, KEMBED_MOTION, px, py, 0, 0, ms);
		else
			send_msg(e, KEMBED_BUTTON, px, py, btn[ev->btn],
				 ev->press == KT_MP_PRESS, ms);
		break;
	}
	case KT_MB_WHEEL_UP:
		send_msg(e, KEMBED_AXIS, px, py, -1, 0, ms);
		break;
	case KT_MB_WHEEL_DOWN:
		send_msg(e, KEMBED_AXIS, px, py, 1, 0, ms);
		break;
	default:
		send_msg(e, KEMBED_MOTION, px, py, 0, 0, ms);
		break;
	}
	return 1;
}
