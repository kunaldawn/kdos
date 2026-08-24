/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   kdos-notify — the notification centre
 *
 *   ╔══════════════════════════════════════════════════════════╗
 *   ║▓▓ Notifications                                        ▓▓║
 *   ║▓▓ 6 kept · 2 new                                       ▓▓║
 *   ╟──────────────────────────────────────────────────────────╢
 *   ║  Today                                                   ║
 *   ║ ▸ 14:32  firefox-esr   Download finished — report.pdf    ║
 *   ║   14:07  Transmission  Torrent complete                  ║
 *   ╟──────────────────────────────────────────────────────────╢
 *   ║ 2 cleared      [ Clear All ] [ Do Not Disturb ] [ Close ]║
 *   ╚══════════════════════════════════════════════════════════╝
 *
 * A NOTIFICATION THAT EXPIRED IS NOT A NOTIFICATION THAT WAS READ. kdos-notifyd
 * threw every toast away the moment its five seconds were up, so anything that
 * arrived while the screen was locked, while another workspace was up, or while
 * somebody was looking at the other half of the screen was simply gone. On this
 * distro that lands harder than elsewhere: every fat application runs inside a
 * container, and a boxed app's notification is often the ONLY thing that says
 * the work it was doing has finished.
 *
 * THE DAEMON OWNS THE LIST AND THIS DRAWS IT — kdos-clip's split, which is the
 * one this desktop already has. A short connection per request over
 * `$XDG_RUNTIME_DIR/kdos-notify.sock`, so this front end is an ordinary program
 * that can be run by hand, scripted or replaced, and the panel can ask for one
 * number once a second without either process knowing anything about the
 * other's drawing.
 *
 * ANCHORED MEANS POPUP; CENTRED MEANS WINDOW — the rule the four device
 * managers keep, and for the same reason: opened from the bar this is the
 * bar's own popup and is dismissed by a click elsewhere, while typed by name
 * it is the application and stays up.
 * ---------------------------------
 */

#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>

#include "kicon.h"
#include "kwl.h"
#include "shell.h"

#define NT_COLS 72
#define NT_ROWS 22
#define NT_MAX 64

struct nrow {
	int idx;			/* the daemon's index, for `forget` */
	time_t when;
	int urgent;
	char app[64];
	char summary[128];
	char body[192];
};

static struct nrow rows[NT_MAX];
static int nrows;
static int sel, top, sel_follow = 1;
static int dnd;
static char status[128];
/* The daemon is not answering. That is an EMPTY STATE — the middle of the
 * window, where somebody is already looking — and not a status line squeezed
 * between a button bar and the border. */
static int daemon_down;
static int icons_on = 1;
static int popup;
/* Where the draw put the first list row. Recorded rather than recomputed: the
 * header band moved every list in this shell down by three rows, and deriving
 * that origin twice is how a click lands on the row above the one under the
 * pointer — kdos-net's lesson, and it is a notification being dismissed here
 * rather than the wrong wifi being joined. */
static int list_y0 = 4;

/* Which button the pointer/keyboard is on — the shared bar's own hit map does
 * the pointer half; this is the list's. */
/*
 * FOUR, and Open is deliberately not among them: Enter opens the selected
 * entry and so does a click on the row that is already selected — kdos-pick's
 * rule — and a fifth button ate the whole footer, which is where the status
 * line lives. The bar drops from the right, so Close goes before Clear All.
 */
enum { NB_FORGET, NB_CLEAR, NB_DND, NB_CLOSE, NB_N };
#define NB_OPEN (-2)			/* an action, never a button */

/* ── the daemon ────────────────────────────────────────────────────────── */

/*
 * One request, one answer. The daemon is in the same session and answers in
 * microseconds; a surface that is up for as long as somebody is reading it can
 * afford a blocking round trip, which is exactly kdos-clip's reasoning.
 */
static int ask(const char *req, char *out, size_t n)
{
	const char *run = getenv("XDG_RUNTIME_DIR");
	char path[256];
	struct sockaddr_un addr = { .sun_family = AF_UNIX };
	struct timeval tv = { .tv_sec = 1, .tv_usec = 0 };
	size_t got = 0;
	ssize_t r;
	int fd;

	if (out)
		out[0] = '\0';
	if (!run || !*run)
		return -1;
	snprintf(path, sizeof(path), "%s/kdos-notify.sock", run);
	fd = socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0);
	if (fd < 0)
		return -1;
	/* An explicit precision: sun_path is 108 bytes and `path` is 256, and
	 * the gate treats a possible truncation as an error. A runtime dir
	 * that long is not one. */
	snprintf(addr.sun_path, sizeof(addr.sun_path), "%.*s",
		 (int)sizeof(addr.sun_path) - 1, path);
	setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
	setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
	if (connect(fd, (struct sockaddr *)&addr, sizeof(addr)) != 0) {
		close(fd);
		return -1;
	}
	if (write(fd, req, strlen(req)) < 0 || write(fd, "\n", 1) < 0) {
		close(fd);
		return -1;
	}
	shutdown(fd, SHUT_WR);
	while (out && got + 1 < n && (r = read(fd, out + got, n - got - 1)) > 0)
		got += (size_t)r;
	if (out)
		out[got] = '\0';
	close(fd);
	return 0;
}

/* One tab-separated field into `dst`, advancing `p`. */
static const char *field(const char *p, char *dst, size_t n)
{
	size_t i = 0;

	while (*p && *p != '\t' && *p != '\n') {
		if (i + 1 < n)
			dst[i++] = *p;
		p++;
	}
	dst[i] = '\0';
	return *p == '\t' ? p + 1 : p;
}

static void reload(void)
{
	char buf[16384];
	const char *p = buf;

	nrows = 0;
	daemon_down = 0;
	if (ask("list", buf, sizeof(buf)) != 0) {
		daemon_down = 1;
		return;
	}
	while (*p && nrows < NT_MAX) {
		char idx[16], when[24], urg[8];
		struct nrow *r = &rows[nrows];

		if (!strncmp(p, "ok", 2) && (p[2] == '\n' || !p[2]))
			break;
		p = field(p, idx, sizeof(idx));
		p = field(p, when, sizeof(when));
		p = field(p, urg, sizeof(urg));
		p = field(p, r->app, sizeof(r->app));
		p = field(p, r->summary, sizeof(r->summary));
		p = field(p, r->body, sizeof(r->body));
		while (*p == '\n')
			p++;
		if (!idx[0])
			continue;
		r->idx = atoi(idx);
		r->when = (time_t)atoll(when);
		r->urgent = atoi(urg);
		nrows++;
	}
	if (sel >= nrows)
		sel = nrows ? nrows - 1 : 0;

	char st[64];
	if (ask("count", st, sizeof(st)) == 0) {
		int a = 0, b = 0, d = 0;

		if (sscanf(st, "%d %d %d", &a, &b, &d) >= 3)
			dnd = d;
	}
}

/* ── drawing ───────────────────────────────────────────────────────────── */

/*
 * WHEN, IN THE WORDS A PERSON USES. A notification centre is read to answer
 * "what did I miss", and `1755438720` and even `2026-08-17 14:32` are both
 * answers to a different question. Today is a clock, yesterday says so, and
 * anything older is a date — which is also why the column is a fixed six
 * cells: a list whose first column changes width is a list that does not line
 * up.
 */
static void when_text(time_t t, char *out, size_t n)
{
	time_t now = time(NULL);
	struct tm tm, nt;

	localtime_r(&t, &tm);
	localtime_r(&now, &nt);
	if (tm.tm_year == nt.tm_year && tm.tm_yday == nt.tm_yday)
		snprintf(out, n, "%02d:%02d", tm.tm_hour, tm.tm_min);
	else if (tm.tm_year == nt.tm_year && tm.tm_yday == nt.tm_yday - 1)
		snprintf(out, n, "%s", "yest.");
	else
		snprintf(out, n, "%d/%d", tm.tm_mday, tm.tm_mon + 1);
}

static void draw_frame(void)
{
	int w = ktui_w, h = ktui_h;
	char sub[128];

	if (w < 24 || h < 8)
		return;

	ktui_draw_fill(krect(0, 0, w, h), KT_BG);
	ktui_draw_box(krect(0, 0, w, h), NULL, KT_ACCENT, KT_BG, 1);

	/* The header answers the question the window is opened to ask, which
	 * is "is there anything I have not seen" — the same rule the device
	 * managers' subject line keeps. */
	if (nrows)
		snprintf(sub, sizeof(sub), "%d kept%s", nrows,
			 dnd ? "   Do Not Disturb is ON" : "");
	else
		snprintf(sub, sizeof(sub), "nothing has arrived yet%s",
			 dnd ? "   Do Not Disturb is ON" : "");
	/* The moon is Do Not Disturb — see panel.c: `user-busy` is in a Papirus
	 * context this theme does not vendor, so it draws nothing at all. */
	int y0 = kch_header(w, dnd ? "weather-clear-night"
					 : "dialog-information",
				  "Notifications", sub, icons_on);

	list_y0 = y0;

	int rows_vis = h - y0 - 2;
	if (rows_vis < 1)
		rows_vis = 1;

	int follow = sel_follow;

	sel_follow = 0;
	kch_list_clamp(&top, sel, nrows, rows_vis, follow);

	int rw = nrows > rows_vis ? w - 3 : w - 2;
	for (int i = 0; i < rows_vis && top + i < nrows; i++) {
		const struct nrow *r = &rows[top + i];
		int y = y0 + i;
		int on = top + i == sel;
		int fg = on ? KT_SURFACE : KT_TEXT;
		int bg = on ? KT_ACCENT : KT_BG;
		char when[16], line[320];

		ktui_draw_fill(krect(1, y, rw, 1), bg);
		when_text(r->when, when, sizeof(when));
		ktui_draw_text(2, y, 5, when,
			       on ? fg : (r->urgent ? KT_ERR : KT_MID), bg,
			       KT_A_NONE);
		/* The application's own name is what makes a list of thirty
		 * notifications scannable — it is the column the eye picks a
		 * row out by. */
		ktui_draw_text(8, y, 13, r->app[0] ? r->app : "?",
			       on ? fg : KT_MID, bg, KT_A_NONE);
		/* Summary and body on ONE row, joined by a dash: the centre is
		 * a list, and a two-row entry halves how much of the last hour
		 * fits on the screen. The whole of both is one Enter away. */
		/* The separator comes from the GLYPH TABLE, so it is a bullet
		 * under fcft, a bullet on tty1 and an asterisk in a golden —
		 * an em dash would be a `?` on two of those three. */
		if (r->body[0])
			snprintf(line, sizeof(line), "%s %s %s", r->summary,
				 ktui_glyph[KT_G_BULLET], r->body);
		else
			snprintf(line, sizeof(line), "%s", r->summary);
		ktui_draw_text(22, y, rw - 22, line, fg, bg, KT_A_NONE);
	}
	if (!nrows)
		ktui_draw_text(2, y0, w - 4,
			       daemon_down
				       ? "kdos-notifyd is not running - no "
					 "notifications are being kept"
				       : "Notifications that come and go are "
					 "kept here.",
			       daemon_down ? KT_WARN : KT_MID, KT_BG,
			       KT_A_NONE);
	kch_scrollbar(0, w - 2, y0, rows_vis, nrows, top, KT_BG);

	/* ── the footer ── */
	struct kch_button b[NB_N];

	b[NB_FORGET] = (struct kch_button){ "Forget", nrows > 0 };
	b[NB_CLEAR] = (struct kch_button){ "Clear All", nrows > 0 };
	b[NB_DND] = (struct kch_button){ dnd ? "Allow Toasts"
					    : "Do Not Disturb", 1 };
	b[NB_CLOSE] = (struct kch_button){ "Close", 1 };
	int bx = kch_buttons(w, h - 2, b, NB_N, -1);
	int room = bx - 3;
	static const char HINT[] = "Enter open   f forget   c clear   Esc";

	/* A MESSAGE takes whatever room is left; a HINT is drawn whole or not
	 * at all — half a sentence against the start of another is worse than
	 * an empty half-row. The rule the device managers already keep. */
	if (room > 0 &&
	    (status[0] ? room >= 8 : room >= (int)ktui_utf8_width(HINT)))
		ktui_draw_text(2, h - 2, room, status[0] ? status : HINT,
			       status[0] ? KT_WARN : KT_MID, KT_BG, KT_A_NONE);
	ktui_draw_flush();
}

/* ── acting ────────────────────────────────────────────────────────────── */

static void act(int which)
{
	char req[32];

	switch (which) {
	case NB_OPEN:
		if (sel < nrows) {
			snprintf(req, sizeof(req), "open %d", rows[sel].idx);
			ask(req, NULL, 0);
			snprintf(status, sizeof(status), "%s",
				 "asked the desktop to open it");
		}
		break;
	case NB_FORGET:
		if (sel < nrows) {
			snprintf(req, sizeof(req), "forget %d", rows[sel].idx);
			ask(req, NULL, 0);
			reload();
		}
		break;
	case NB_CLEAR:
		ask("clear", NULL, 0);
		reload();
		snprintf(status, sizeof(status), "%s", "cleared");
		break;
	case NB_DND:
		ask("dnd toggle", NULL, 0);
		reload();
		snprintf(status, sizeof(status), "%s",
			 dnd ? "toasts are silenced — they are still kept here"
			     : "toasts are shown again");
		break;
	default:
		break;
	}
}

int notify_main(int argc, char **argv)
{
	const char *font = NULL;
	int at_x = -1, at_y = 0, at_bottom = 0, dump = 0;

	for (int i = 1; i < argc; i++) {
		if (!strcmp(argv[i], "--at") && i + 2 < argc) {
			at_x = atoi(argv[++i]);
			at_y = atoi(argv[++i]);
		} else if (!strcmp(argv[i], "--at-bottom") && i + 2 < argc) {
			at_x = atoi(argv[++i]);
			at_y = atoi(argv[++i]);
			at_bottom = 1;
		} else if (!strcmp(argv[i], "--font") && i + 1 < argc) {
			font = argv[++i];
		} else if (!strcmp(argv[i], "--no-icons")) {
			icons_on = 0;
		} else if (!strcmp(argv[i], "--dump")) {
			dump = 1;
		} else if (!strcmp(argv[i], "--dnd")) {
			/* The switch without the window: what a keybinding or
			 * a script wants, and what the panel's middle click
			 * uses. Prints the new state. */
			char out[16] = "";

			ask("dnd toggle", out, sizeof(out));
			printf("%s", out[0] ? out : "0\n");
			return 0;
		} else {
			fprintf(stderr, "usage: kdos-notify [--at X Y] "
					"[--at-bottom X Y] [--dnd] "
					"[--font NAME] [--no-icons] [--dump]\n");
			return 2;
		}
	}

	popup = at_x >= 0;

	if (dump) {
		sh_theme_from_cache();
		icons_on = 0;
		reload();
		ktui_offscreen_init(NT_COLS, NT_ROWS);
		draw_frame();
		ktui_draw_dump();
		return 0;
	}

	KwlConfig cfg = {
		.role = KWL_ROLE_OVERLAY,
		.cols = popup ? 56 : NT_COLS,
		.rows = popup ? 18 : NT_ROWS,
		.corner = !popup	? KWL_CORNER_CENTER
			  : at_bottom	? KWL_CORNER_BOTTOM_LEFT
					: KWL_CORNER_TOP_LEFT,
		.margin_x = popup ? at_x : 0,
		.margin_y = popup ? at_y : 0,
		.app_id = "kdos-notify",
		.font = font,
		.keyboard = 1,
		/* The bar's own popup dismisses on a click elsewhere; the
		 * application does not. One flag decides both. */
		.dismiss_on_unfocus = popup,
	};

	sh_theme_from_cache();
	if (kwl_init(&cfg) != 0) {
		fprintf(stderr, "kdos-notify: no compositor or no layer-shell\n");
		return 1;
	}
	if (icons_on)
		kicon_init(kwl_cell_w(), kwl_cell_h(), kwl_scale());
	ktui_draw_init();
	/* The bar's own body, so a popup over the taskbar is the
	 * same surface the taskbar is — see kch_px_popup(). */
	kch_px_popup(KT_BG);

	reload();
	/* OPENING IT IS READING IT. The badge counts what has arrived since
	 * somebody last looked, and this is that moment — cleared here and by
	 * nothing else, because a count that cleared itself on a timer is a
	 * count nobody trusts. */
	ask("seen", NULL, 0);

	while (!kwl_should_close()) {
		sh_theme_poll();
		draw_frame();

		KtuiEvent ev;
		if (!ktui_backend()->poll_event(&ev, 1000)) {
			if (ktui_resized) {
				ktui_resized = 0;
				ktui_draw_resize();
				ktui_draw_invalidate();
			}
			/* Something may have arrived while this is up. */
			reload();
			continue;
		}

		if (ev.type == KT_EVT_MOUSE) {
			int y0 = list_y0;
			int rows_vis = ktui_h - y0 - 2;
			int idx = top + ev.my - y0;
			int in_list = ev.my >= y0 && ev.my < y0 + rows_vis &&
				      idx >= 0 && idx < nrows;

			if (ev.press == KT_MP_DRAG) {
				/* THE BAR IS A CONTROL — see kch_scrollbar.
				 * A drag is a press that is still down, and
				 * Wayland says nothing about that, so the
				 * grab is what remembers it. */
				int bt = kch_scrollbar_drag(ev.my);

				if (bt >= 0) {
					top = bt;
					sel_follow = 0;
					continue;
				}
				if (in_list) {
					sel = idx;
					sel_follow = 1;
				}
				kch_hover(ev.mx, ev.my);
				continue;
			}
			if (ev.press == KT_MP_RELEASE) {
				kch_scrollbar_release();
				continue;
			}
			if (ev.press != KT_MP_PRESS)
				continue;
			if (ev.btn == KT_MB_LEFT) {
				int bt = kch_scrollbar_press(0, ev.mx,
							     ev.my);

				if (bt >= 0) {
					top = bt;
					sel_follow = 0;
					continue;
				}
			}
			if (ev.btn == KT_MB_WHEEL_UP ||
			    ev.btn == KT_MB_WHEEL_DOWN) {
				int up = ev.btn == KT_MB_WHEEL_UP;

				if (!kch_list_wheel(up, &top, nrows, rows_vis)) {
					sel += up ? -1 : 1;
					if (sel < 0)
						sel = 0;
					if (sel >= nrows)
						sel = nrows ? nrows - 1 : 0;
					sel_follow = 1;
				}
				continue;
			}
			if (ev.btn == KT_MB_RIGHT)
				break;
			if (ev.btn != KT_MB_LEFT)
				continue;

			int bi = kch_button_at(ev.mx, ev.my);

			if (bi == NB_CLOSE)
				break;
			if (bi >= 0) {
				act(bi);
				continue;
			}
			/* A click on a row selects it; a click on the row that
			 * is already selected opens it — kdos-pick's rule, so
			 * one hand learns one thing. */
			if (in_list) {
				if (idx == sel)
					act(NB_OPEN);
				sel = idx;
				sel_follow = 1;
			}
			continue;
		}

		if (ev.type != KT_EVT_KEY)
			continue;
		sel_follow = 1;
		switch (ev.key) {
		case KT_K_ESC:
			goto done;
		case KT_K_UP:
			if (sel > 0)
				sel--;
			break;
		case KT_K_DOWN:
			if (sel + 1 < nrows)
				sel++;
			break;
		case KT_K_HOME:
			sel = 0;
			break;
		case KT_K_END:
			sel = nrows ? nrows - 1 : 0;
			break;
		case KT_K_ENTER:
			act(NB_OPEN);
			break;
		case 'f':
		case KT_K_DEL:
			act(NB_FORGET);
			break;
		case 'c':
			act(NB_CLEAR);
			break;
		case 'n':
			act(NB_DND);
			break;
		case 'r':
			reload();
			break;
		default:
			break;
		}
	}
done:
	kicon_finish();
	kwl_shutdown();
	return 0;
}
