/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   KD's Homebrew Linux Distro
 * ---------------------------------
 */

/* See term.h. */

#include <errno.h>
#include <poll.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "kcon.h"	/* kcon_impl */
#ifndef KDOS_TERM_CONSOLE_ONLY
#include "kwl.h"	/* kwl_impl — naming these is what links each one in */
#endif
#include "kxdg.h"
#include "term.h"

#ifdef HAVE_KIMG
#include <pixman.h>
#endif

/*
 * See the declaration: naming kwl_impl is what links Wayland into this binary,
 * and naming kcon_impl is what makes the same one a console surface.
 *
 * KDOS_TERM_CONSOLE_ONLY drops the Wayland half, which is what the self-test
 * builds: the state machine, the frame and the image path are the whole of
 * what a `--dump` exercises, and they must be asserted on a bare host rather
 * than only where fcft and wayland-client happen to exist. The SHIPPED binary
 * is the one without it — a define that has to be present for the desktop to
 * work would be a define somebody forgets.
 */
#ifdef KDOS_TERM_CONSOLE_ONLY
const KDispImpl *const kdos_disp[] = { &kcon_impl };
const int kdos_disp_n = 1;
#else
const KDispImpl *const kdos_disp[] = { &kcon_impl, &kwl_impl };
const int kdos_disp_n = 2;
#endif

/* Zeroed: `selecting` is what says whether a drag is in progress, so the
 * sentinel the cell coordinates used to carry is not needed. */
Term T;

static volatile sig_atomic_t g_reload;
static char g_title[128] = "Terminal";

static const char USAGE[] =
"usage: kdos-term [options] [-e command [args...]]\n"
"\n"
"  -e, --exec CMD     run CMD instead of the shell; everything after it is\n"
"                     its argument vector\n"
"      --title TEXT   the window title before the program sets one\n"
"  -D, --working-directory DIR\n"
"                     start the program in DIR\n"
"      --font NAME    fontconfig name; overrides term.conf\n"
"      --tty          draw on the terminal this was started from\n"
"      --dump WxH     run to completion offscreen and write the cells\n"
"  -h, --help\n"
"\n"
"Configuration is ~/.config/kdos/term.conf.\n";

/*
 * The accent, from the one word `kdos theme` writes. No colours are read: the
 * palette is compiled in through libkcolor and the state file names which of
 * its schemes is in force.
 */
static void theme_from_cache(void)
{
	char path[512], name[64];
	const char *cache = getenv("XDG_CACHE_HOME");
	const char *home = getenv("HOME");

	if (cache && *cache)
		snprintf(path, sizeof(path), "%s/kdos/theme", cache);
	else if (home && *home)
		snprintf(path, sizeof(path), "%s/.cache/kdos/theme", home);
	else
		return;

	char *d = kb_read_all(path, NULL);

	if (!d)
		return;		/* absent: ktui_theme_set already defaulted */

	char *nl = strchr(d, '\n');

	if (nl)
		*nl = 0;
	kb_strlcpy(name, d, sizeof(name));
	free(d);
	if (*name)
		ktui_theme_set(name);
}

/*
 * The default disposition for SIGHUP is death, so a program on
 * reload_session()'s list that does not handle it is a program `kdos theme`
 * KILLS. A flag rather than the work itself: a handler that reparsed a file
 * would be allocating inside a signal.
 */
static void on_hup(int sig)
{
	(void)sig;
	g_reload = 1;
}

/* OSC 0 and OSC 2, which is how a program names its own window. Only the
 * undecorated frame can show it: an xdg-toplevel's title was set at
 * initialisation and libkdisp has no path to change it. */
/*
 * A CHILD PUT SOMETHING ON THE CLIPBOARD, through OSC 52. It goes wherever
 * this program's display server puts a selection — the compositor's data
 * device under Wayland, the session's own buffer on the console.
 */
static void on_clip(struct kvt_vte *vte, const char *text, size_t len,
		    int primary, void *data)
{
	(void)vte;
	(void)data;
	kdisp_copy(text, len, primary);
}

static void on_osc(struct kvt_vte *vte, const char *u8, size_t len, void *data)
{
	(void)vte;
	(void)data;

	if (len > 2 && (!strncmp(u8, "0;", 2) || !strncmp(u8, "2;", 2)))
		kb_strlcpy(g_title, u8 + 2, sizeof(g_title));
}

#ifdef HAVE_KIMG
/*
 * WHERE A SPRITE'S PIXELS COME FROM when this program is a console surface.
 * libkcon links no pixel library and must not; it asks for the bytes through
 * this and puts them on the wire, and the display on the other end scales them
 * to whatever a cell is there.
 */
static int sprite_bits(const void *pix, const uint32_t **argb, int *w, int *h,
		       int *stride_px, void *user)
{
	pixman_image_t *img = (pixman_image_t *)pix;

	(void)user;
	if (!img)
		return -1;
	*argb = pixman_image_get_data(img);
	*w = pixman_image_get_width(img);
	*h = pixman_image_get_height(img);
	*stride_px = pixman_image_get_stride(img) / 4;
	return *argb && *w > 0 && *h > 0 ? 0 : -1;
}
#endif

/* ── drawing ───────────────────────────────────────────────────────────── */

/*
 * WHOEVER OWNS THE FRAME DRAWS IT. Under kdos-comp the compositor is already
 * drawing one round the outside, so a box here would be a second frame inside
 * the first with the title written twice. On a tty and under `--dump` nothing
 * else is drawing one, and then this is the only frame there is.
 */
static void inner(int *x, int *y, int *w, int *h)
{
	if (kdisp_decorated()) {
		*x = 0;
		*y = 0;
		*w = ktui_w;
		*h = ktui_h;
		return;
	}
	char t[132];

	snprintf(t, sizeof(t), " %s ", g_title);
	ktui_draw_box(krect(0, 0, ktui_w, ktui_h), t, KT_ACCENT, KT_BG, 1);
	*x = 1;
	*y = 1;
	*w = ktui_w - 2;
	*h = ktui_h - 2;
}

static void draw(void)
{
	static KtuiCell *buf;
	static int bufn;
	int x, y, w, h;

	ktui_draw_clear();
	inner(&x, &y, &w, &h);
	if (w <= 0 || h <= 0)
		return;

	if (w * h > bufn) {
		KtuiCell *nb = realloc(buf, (size_t)w * h * sizeof(*buf));

		if (!nb)
			return;
		buf = nb;
		bufn = w * h;
	}

	kvt_term_render(T.t, buf, w, h);

	for (int r = 0; r < h; r++)
		for (int c = 0; c < w; c++) {
			const KtuiCell *cl = &buf[r * w + c];

			ktui_draw_cell(x + c, y + r, cl->ch, cl->fg, cl->bg,
				       cl->attr);
		}

	/*
	 * The cursor is the SCREEN's, offset into the frame. Drawn only while
	 * the child is alive: a block sitting under the exit message reads as
	 * a prompt waiting for input that nothing will ever receive.
	 */
	if (kvt_term_alive(T.t)) {
		struct kvt_screen *sc = kvt_term_screen(T.t);
		unsigned cx = kvt_screen_get_cursor_x(sc);
		unsigned cy = kvt_screen_get_cursor_y(sc);

		if ((int)cx < w && (int)cy < h)
			ktui_draw_cursor(x + (int)cx, y + (int)cy);
	}
}

/* ── the terminal's own chords ─────────────────────────────────────────── */

/*
 * FOUR, AND NO MORE THAN FOUR. Every chord this program claims is a chord no
 * program running inside it can ever use, and a terminal that ate Ctrl+Shift+K
 * is a terminal somebody's editor is broken in.
 *
 * Ctrl+Shift is the prefix because a bare Ctrl chord belongs to the child.
 */
static int chord(const KtuiEvent *ev)
{
	if (!(ev->mods & KT_MOD_CTRL) || !(ev->mods & KT_MOD_SHIFT))
		return 0;

	switch (ev->key) {
	case 'C': case 'c': case 0x03: {
		/* THE CLIPBOARD, not the primary selection: a drag already put
		 * the selection there, and a copy that only wrote the same
		 * place would be a copy that did nothing. */
		char *text = NULL;

		if (kvt_screen_selection_copy(kvt_term_screen(T.t), &text) >= 0
		    && text) {
			kdisp_copy(text, strlen(text), 0);
			free(text);
		}
		return 1;
	}
	case 'V': case 'v': case 0x16:
		/* The backend started the receive when it saw the key; the
		 * bytes arrive on a later pass and term_paste_pending() is
		 * what writes them. */
		return 1;
	default:
		break;
	}
	return 0;
}

static int scroll_chord(const KtuiEvent *ev)
{
	if (!(ev->mods & KT_MOD_SHIFT))
		return 0;
	if (ev->key == KT_K_PGUP) {
		kvt_term_scroll(T.t, -(T.rows / 2));
		return 1;
	}
	if (ev->key == KT_K_PGDN) {
		kvt_term_scroll(T.t, T.rows / 2);
		return 1;
	}
	return 0;
}

/* ── the loop ──────────────────────────────────────────────────────────── */

static void resize_to_frame(void)
{
	int x, y, w, h;

	ktui_draw_resize();
	ktui_draw_invalidate();
	inner(&x, &y, &w, &h);
	if (w < 1 || h < 1 || (w == T.cols && h == T.rows))
		return;
	T.cols = w;
	T.rows = h;
	kvt_term_resize(T.t, w, h);
}

/*
 * Run the child to completion and consume everything it wrote, then draw one
 * frame. What makes a dump reproducible: a frame taken while a program is
 * still writing is a different frame every time it is taken.
 */
static void settle(void)
{
	for (int spin = 0; spin < 4000; spin++) {
		struct pollfd p = { kvt_term_fd(T.t), POLLIN, 0 };
		int live = kvt_term_alive(T.t);

		kvt_term_pump(T.t);
		if (p.fd < 0)
			return;
		if (poll(&p, 1, live ? 5 : 1) <= 0 && !live) {
			kvt_term_pump(T.t);
			return;
		}
	}
}

int main(int argc, char **argv)
{
	const char *title = NULL, *font = NULL, *cwd = NULL;
	int tty = 0, dump_w = 0, dump_h = 0;
	const char *av[64];
	int nav = 0;

	kb_set_progname("kdos-term");
	term_conf_load();

	for (int i = 1; i < argc; i++) {
		const char *a = argv[i];

		if (!strcmp(a, "-h") || !strcmp(a, "--help")) {
			fputs(USAGE, stdout);
			return 0;
		} else if ((!strcmp(a, "-e") || !strcmp(a, "--exec")) &&
			   i + 1 < argc) {
			/* EVERYTHING AFTER -e IS THE CHILD'S, which is what -e
			 * means in every terminal there has ever been. */
			for (int j = i + 1; j < argc && nav < 63; j++)
				av[nav++] = argv[j];
			break;
		} else if (!strcmp(a, "--title") && i + 1 < argc) {
			title = argv[++i];
		} else if ((!strcmp(a, "-D") ||
			    !strcmp(a, "--working-directory")) && i + 1 < argc) {
			cwd = argv[++i];
		} else if (!strcmp(a, "--font") && i + 1 < argc) {
			font = argv[++i];
		} else if (!strcmp(a, "--tty")) {
			tty = 1;
		} else if (!strcmp(a, "--dump") && i + 1 < argc) {
			if (sscanf(argv[++i], "%dx%d", &dump_w, &dump_h) != 2 ||
			    dump_w < 4 || dump_h < 2) {
				fprintf(stderr,
					"kdos-term: --dump wants COLSxROWS\n");
				return 1;
			}
		} else if (!strcmp(a, "--")) {
			for (int j = i + 1; j < argc && nav < 63; j++)
				av[nav++] = argv[j];
			break;
		} else {
			fprintf(stderr, "kdos-term: unknown option '%s'\n", a);
			return 1;
		}
	}

	/*
	 * NO SHELL AND NO system(). The vector is built here and executed
	 * directly, because $SHELL and term.conf are both strings somebody
	 * else wrote — split the way a desktop entry is, by libkxdg.
	 */
	char store[1024];

	if (!nav) {
		const char *sh = *TC.shell ? TC.shell : getenv("SHELL");

		if (!sh || !*sh)
			sh = "/bin/sh";
		nav = kxdg_exec_split(sh, NULL, 0, store, sizeof(store), av, 63);
		if (nav <= 0) {
			fprintf(stderr, "kdos-term: cannot read '%s'\n", sh);
			return 1;
		}
	}
	av[nav] = NULL;

	if (title)
		kb_strlcpy(g_title, title, sizeof(g_title));
	if (!font && *TC.font)
		font = TC.font;

	theme_from_cache();

	if (dump_w) {
		if (ktui_offscreen_init(dump_w, dump_h) != 0) {
			fprintf(stderr, "kdos-term: cannot draw offscreen\n");
			return 1;
		}
	} else if (tty) {
		ktui_backend_set(NULL);	/* NULL selects the built-in tty */
		if (ktui_term_init(1) != 0) {
			fprintf(stderr, "kdos-term: no terminal\n");
			return 1;
		}
	} else {
		KDispConfig cfg = {
			.role = KDISP_ROLE_TOPLEVEL,
			.title = g_title,
			.app_id = "kdos-term",
			.font = font,
			.keyboard = 1,
			.cols = TC.cols,
			.rows = TC.rows,
		};

		if (kdisp_init(&cfg, kdos_disp, kdos_disp_n) != 0) {
			fprintf(stderr,
				"kdos-term: no display — try --tty\n");
			return 1;
		}
	}
	ktui_draw_init();

	int x, y, w, h;

	inner(&x, &y, &w, &h);
	T.cols = w > 0 ? w : TC.cols;
	T.rows = h > 0 ? h : TC.rows;

	/* Before the fork, so the shell and anything it starts are in the
	 * directory the caller named. After the configuration and the theme,
	 * which are read from absolute paths and are already loaded. */
	if (cwd && chdir(cwd) < 0)
		fprintf(stderr, "kdos-term: cannot enter %s: %s\n", cwd,
			strerror(errno));

	T.t = kvt_term_open(av, T.cols, T.rows);
	if (!T.t) {
		fprintf(stderr, "kdos-term: cannot open a pty\n");
		return 1;
	}
	kvt_term_scrollback(T.t, (unsigned)TC.scrollback);
	kvt_term_osc_cb(T.t, on_osc, NULL);
	kvt_term_clip_cb(T.t, on_clip, NULL);

#ifdef HAVE_KIMG
	kcon_set_sprite_bits(sprite_bits, NULL);
#endif
	term_pic_init();

	if (dump_w) {
		settle();
		draw();
		ktui_draw_dump();
		term_pic_shutdown();
		kvt_term_close(T.t);
		return 0;
	}

	signal(SIGHUP, on_hup);

	int status = 0;

	for (;;) {
		if (!tty && kdisp_should_close())
			break;
		if (g_reload) {
			g_reload = 0;
			term_conf_load();
			theme_from_cache();
			ktui_draw_invalidate();
		}
		if (ktui_resized) {
			ktui_resized = 0;
			resize_to_frame();
		}

		kvt_term_pump(T.t);
		term_paste_pending();

		/*
		 * THE CHILD IS GONE AND ITS LAST OUTPUT IS ON THE SCREEN. One
		 * more draw so the exit message is visible, then out: a window
		 * that stayed open on a dead shell is a window with no way to
		 * type into it.
		 */
		if (!kvt_term_alive(T.t)) {
			status = kvt_term_status(T.t);
			draw();
			ktui_draw_flush();
			break;
		}

		draw();
		ktui_draw_flush();

		struct pollfd p[2];
		int n = 0;
		int dfd = tty ? -1 : kdisp_fd();

		if (dfd >= 0) {
			p[n].fd = dfd;
			p[n].events = POLLIN;
			p[n].revents = 0;
			n++;
		}
		p[n].fd = kvt_term_fd(T.t);
		p[n].events = POLLIN;
		p[n].revents = 0;
		n++;

		/*
		 * THE ANIMATION DECIDES THE WAIT. A terminal with nothing
		 * moving in it wakes ten times a second for the cursor blink
		 * and no more; one with a picture playing wakes when its next
		 * frame is due, which is what makes the frame rate the
		 * picture's rather than the loop's.
		 */
		int anim = term_pic_tick();
		int wait = anim >= 0 && anim < 100 ? anim : 100;

		if (poll(p, (nfds_t)n, wait) < 0)
			continue;

		KtuiEvent ev;

		while (ktui_backend()->poll_event(&ev, 0)) {
			if (ev.type == KT_EVT_RESIZE) {
				resize_to_frame();
				continue;
			}
			if (ev.type == KT_EVT_MOUSE) {
				int fx, fy, fw, fh;
				KtuiEvent in = ev;

				inner(&fx, &fy, &fw, &fh);
				in.mx -= fx;
				in.my -= fy;
				if (in.mx < 0 || in.my < 0 ||
				    in.mx >= fw || in.my >= fh)
					continue;
				term_mouse(&in);
				continue;
			}
			if (ev.type != KT_EVT_KEY)
				continue;
			if (chord(&ev) || scroll_chord(&ev))
				continue;
			term_key(&ev);
		}
	}

	term_pic_shutdown();
	kvt_term_close(T.t);
	if (tty)
		ktui_term_shutdown();
	else
		kdisp_shutdown();

	/* The child's status is this program's: a terminal opened to run one
	 * command is a wrapper round it, and a caller that checks the exit
	 * code must see the command's. */
	return status < 0 ? 0 : status;
}
