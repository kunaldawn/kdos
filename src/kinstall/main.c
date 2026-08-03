/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   KDOS Installer — chrome and the loop
 * ---------------------------------
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "kinstall.h"

int ki_page;
int ki_quit;
int ki_help;

static char nav_err[160];
static double nav_err_at;
static int page_scroll;
static int page_overflow;
static int last_focus = -1;

#define SIDEBAR_W 22
#define MIN_W 62
#define MIN_H 18

/* ──────────────────────────────────────────────────────────────────────── */

static struct {
	int active;
	int confirm;
	char title[64];
	char msg[512];
	char yes[24], no[24];
	void (*on_yes)(void);
	int saved_focus;
} md;

/* A modal takes the focus ring over completely and hands it back on close,
 * so dismissing a dialog does not silently move the caret on the page. */
static void modal_open(void)
{
	md.saved_focus = ui.focus;
	ui.focus = 0;
}

static void modal_close(void)
{
	md.active = 0;
	ui.focus = md.saved_focus;
}

int modal_active(void)
{
	return md.active;
}

void modal_alert(const char *title, const char *msg)
{
	memset(&md, 0, sizeof(md));
	md.active = 1;
	strlcpy_(md.title, title, sizeof(md.title));
	strlcpy_(md.msg, msg, sizeof(md.msg));
	strlcpy_(md.yes, "OK", sizeof(md.yes));
	modal_open();
}

void modal_confirm(const char *title, const char *msg, const char *yes,
		   const char *no, void (*on_yes)(void))
{
	memset(&md, 0, sizeof(md));
	md.active = 1;
	md.confirm = 1;
	strlcpy_(md.title, title, sizeof(md.title));
	strlcpy_(md.msg, msg, sizeof(md.msg));
	strlcpy_(md.yes, yes, sizeof(md.yes));
	strlcpy_(md.no, no, sizeof(md.no));
	md.on_yes = on_yes;
	modal_open();
}

void modal_draw(void)
{
	if (!md.active)
		return;

	int lines = 1;
	for (const char *p = md.msg; *p; p++)
		if (*p == '\n')
			lines++;

	int w = 56;
	if (w > term_w - 6)
		w = term_w - 6;
	int h = lines + 6;
	int x = (term_w - w) / 2, y = (term_h - h) / 2;

	Rect r = rect(x, y, w, h);
	draw_fill(r, CL_SURFACE);
	draw_shadow(r);
	draw_box(r, md.title, md.confirm ? CL_WARN : CL_ACCENT, CL_SURFACE, 1);

	const char *p = md.msg;
	for (int i = 0; i < lines; i++) {
		const char *nl = strchr(p, '\n');
		char buf[256];
		size_t n = nl ? (size_t)(nl - p) : strlen(p);
		if (n >= sizeof(buf))
			n = sizeof(buf) - 1;
		memcpy(buf, p, n);
		buf[n] = 0;
		draw_text(x + 3, y + 2 + i, w - 6, buf, CL_TEXT, CL_SURFACE, 0);
		if (!nl)
			break;
		p = nl + 1;
	}

	int by = y + h - 2;
	if (md.confirm) {
		if (ui_button(rect(x + w - 40, by, 18, 1), md.yes, 1, 1)) {
			void (*fn)(void) = md.on_yes;
			modal_close();
			if (fn)
				fn();
		}
		if (ui_button(rect(x + w - 21, by, 18, 1), md.no, 1, 0))
			modal_close();
	} else {
		if (ui_button(rect(x + w - 21, by, 18, 1), md.yes, 1, 1))
			modal_close();
	}
}

int modal_event(Event *ev)
{
	if (!md.active)
		return 0;
	if (ev->type == EVT_KEY && ev->key == K_ESC) {
		modal_close();
		return 1;
	}
	return 0;
}

/* ──────────────────────────────────────────────────────────────────────── */

void page_goto(int n)
{
	if (n < 0 || n >= ki_npages)
		return;
	ki_page = n;
	ui.focus = 0;
	page_scroll = 0;
	page_overflow = 0;
	last_focus = -1;
	nav_err[0] = 0;
	if (ki_pages[n]->enter)
		ki_pages[n]->enter();
	draw_invalidate();
}

void nav_next(void)
{
	Page *p = ki_pages[ki_page];
	if (p->validate && p->validate(nav_err, sizeof(nav_err))) {
		nav_err_at = now_s();
		return;
	}
	if (ki_page + 1 < ki_npages)
		page_goto(ki_page + 1);
}

void nav_back(void)
{
	if (ki_page > 0 && !ki_pages[ki_page]->hide_nav)
		page_goto(ki_page - 1);
}

/* ──────────────────────────────────────────────────────────────────────── */

static void draw_header(void)
{
	draw_fill(rect(0, 0, term_w, 1), CL_ACCENT);

	char left[80];
	snprintf(left, sizeof(left), " KDOS INSTALLER %s ", KI_VERSION);
	draw_text(1, 0, term_w, left, CL_BG, CL_ACCENT, 0);

	char right[96];
	time_t t = time(NULL);
	struct tm tm;
	localtime_r(&t, &tm);
	snprintf(right, sizeof(right), "%s  %d/%d  %02d:%02d ",
		 ki_pages[ki_page]->title, ki_page + 1, ki_npages, tm.tm_hour,
		 tm.tm_min);
	int rw = utf8_width(right);
	if (rw + (int)strlen(left) + 2 < term_w)
		draw_text(term_w - rw - 1, 0, rw, right, CL_BG, CL_ACCENT, 0);

	if (cfg.dry_run) {
		const char *dr = " DRY RUN ";
		draw_text((term_w - 9) / 2, 0, 9, dr, CL_ACCENT, CL_ERR, 0);
	}
}

static void draw_sidebar(Rect r)
{
	draw_vline(r.x + r.w, r.y, r.h, G_VL, CL_DIM, CL_BG);

	int y = r.y;
	for (int i = 0; i < ki_npages; i++) {
		if (y >= r.y + r.h)
			break;
		int state = i < ki_page ? 2 : i == ki_page ? 1 : 0;
		const char *mark = state == 2 ? ki_glyph[G_SQUARE]
					      : state == 1 ? ki_glyph[G_RIGHT]
							   : ki_glyph[G_DOT];
		int fg = state == 1 ? CL_BG : state == 2 ? CL_MID : CL_DIM;
		int bg = state == 1 ? CL_ACCENT : CL_BG;

		draw_fill(rect(r.x, y, r.w, 1), bg);
		draw_textf(r.x + 1, y, r.w - 1, fg, bg, 0, "%s %d %s", mark, i + 1,
			   ki_pages[i]->title);

		/* Clicking a finished stage walks back to it; clicking ahead
		 * does nothing, because the pages ahead have not been
		 * validated and their state would be a guess.
		 *
		 * The rows take hit ids out of a reserved range instead of
		 * calling ui_id(): the sidebar is drawn BEFORE the page, so
		 * claiming real focus ids here would push every control on
		 * every page ten places down the ring and leave the Tab order
		 * starting on a decoration. */
		int id = UI_ID_SIDEBAR + i;
		Rect row = rect(r.x, y, r.w, 1);
		ui_hit(row, id);
		if (ui.clicked == id && i < ki_page &&
		    !ki_pages[ki_page]->hide_nav)
			page_goto(i);
		y++;
	}

	y = r.y + r.h - 5;
	if (y > ki_npages + r.y) {
		draw_hline(r.x, y++, r.w - 1, G_HL, CL_DIM, CL_BG);
		draw_textf(r.x + 1, y++, r.w - 2, CL_DIM, CL_BG, 0, "accent  %s",
			   ki_theme->name);
		draw_textf(r.x + 1, y++, r.w - 2, CL_DIM, CL_BG, 0, "mouse   %s",
			   (term_caps & CAP_MOUSE)
				   ? ((term_caps & CAP_LINUXVT) ? "evdev" : "sgr")
				   : "off");
		draw_textf(r.x + 1, y++, r.w - 2, CL_DIM, CL_BG, 0, "colour  %s",
			   (term_caps & CAP_TRUECOLOR) ? "24-bit"
			   : (term_caps & CAP_256)     ? "256"
			   : (term_caps & CAP_LINUXVT) ? "vt palette"
						       : "ansi");
	}
}

static void draw_nav(int y)
{
	Page *p = ki_pages[ki_page];

	draw_hline(0, y - 1, term_w, G_HL, CL_DIM, CL_BG);

	if (nav_err[0] && now_s() - nav_err_at < 8.0)
		draw_textf(1, y, term_w - 40, CL_ERR, CL_BG, 0, "! %s", nav_err);
	else if (p->subtitle)
		draw_text(1, y, term_w - 40, p->subtitle, CL_DIM, CL_BG, 0);

	if (p->hide_nav)
		return;

	int bx = term_w - 34;
	if (ui_button(rect(bx, y, 14, 1), "Back", ki_page > 0, 0))
		nav_back();
	/* The page before the summary says "Review", not "Next" — the label is
	 * the last chance to signal that the questionnaire is over. */
	if (ui_button(rect(bx + 16, y, 16, 1),
		      ki_page == ki_npages - 4 ? "Review" : "Next", 1, 1))
		nav_next();
}

static void draw_hints(int y)
{
	const char *hint;
	if (ki_pages[ki_page]->hide_nav)
		hint = " Tab move   Enter activate   L log   F1 help   ^Q quit ";
	else
		hint = " Tab move   Enter activate   Alt+</> page   F1 help   ^Q quit ";

	draw_fill(rect(0, y, term_w, 1), CL_SURFACE);
	draw_text(1, y, term_w - 2, hint, CL_MID, CL_SURFACE, 0);
}

static void draw_help(void)
{
	int w = 60, h = 20;
	if (w > term_w - 4)
		w = term_w - 4;
	if (h > term_h - 4)
		h = term_h - 4;
	int x = (term_w - w) / 2, y = (term_h - h) / 2;

	Rect r = rect(x, y, w, h);
	draw_fill(r, CL_SURFACE);
	draw_shadow(r);
	draw_box(r, "KEYS", CL_ACCENT, CL_SURFACE, 1);

	static const char *rows[][2] = {
		{ "Tab / Shift-Tab", "move between controls" },
		{ "Arrows", "move inside a list or a field" },
		{ "Enter / Space", "activate, toggle, choose" },
		{ "Alt + Left/Right", "previous / next page" },
		{ "Esc", "back a page, or close this" },
		{ "F1", "this help" },
		{ "L", "full log, on the install page" },
		{ "Ctrl+U", "clear a text field" },
		{ "Ctrl+Q", "quit the installer" },
		{ "", "" },
		{ "mouse", "click anything, wheel scrolls lists" },
		{ "", "on a bare tty the pointer is read" },
		{ "", "straight from /dev/input — no gpm" },
		{ NULL, NULL },
	};

	int ry = y + 2;
	for (int i = 0; rows[i][0] && ry < y + h - 1; i++, ry++) {
		draw_text(x + 3, ry, 20, rows[i][0], CL_ACCENT, CL_SURFACE, 0);
		draw_text(x + 22, ry, w - 25, rows[i][1], CL_TEXT, CL_SURFACE, 0);
	}
}

static void draw_toosmall(void)
{
	draw_clear();
	char m[80];
	snprintf(m, sizeof(m), "terminal is %dx%d, need at least %dx%d", term_w,
		 term_h, MIN_W, MIN_H);
	draw_text(1, term_h / 2 - 1, term_w - 2, "KDOS INSTALLER", CL_ACCENT,
		  CL_BG, 0);
	draw_text(1, term_h / 2, term_w - 2, m, CL_ERR, CL_BG, 0);
	draw_text(1, term_h / 2 + 1, term_w - 2, "resize the window and it will "
						 "come back", CL_MID, CL_BG, 0);
	draw_flush();
}

/* ──────────────────────────────────────────────────────────────────────── */

static void quit_confirmed(void)
{
	ki_quit = 1;
}

static void global_keys(Event *ev)
{
	if (ev->type != EVT_KEY)
		return;

	if (ev->key == K_F1) {
		ki_help = !ki_help;
		ui.consumed = 1;
		return;
	}
	if (ki_help) {
		if (ev->key == K_ESC || ev->key == K_ENTER || ev->key == ' ') {
			ki_help = 0;
			ui.consumed = 1;
		}
		return;
	}
	if ((ev->mods & MOD_CTRL) && (ev->key == 'q' || ev->key == 'c')) {
		ui.consumed = 1;
		if (inst.running)
			modal_confirm("Quit",
				      "The install is still running.\n"
				      "Stopping now leaves the target half written.",
				      "Stop and quit", "Keep going", quit_confirmed);
		else
			modal_confirm("Quit", "Leave the installer?", "Quit",
				      "Stay", quit_confirmed);
		return;
	}
	if (ki_pages[ki_page]->hide_nav)
		return;
	if ((ev->mods & MOD_ALT) && ev->key == K_RIGHT) {
		nav_next();
		ui.consumed = 1;
	} else if ((ev->mods & MOD_ALT) && ev->key == K_LEFT) {
		nav_back();
		ui.consumed = 1;
	} else if (ev->key == K_ESC) {
		nav_back();
		ui.consumed = 1;
	}
}

static void render(Event *ev)
{
	if (term_w < MIN_W || term_h < MIN_H) {
		draw_toosmall();
		return;
	}

	draw_clear();
	ui_frame_begin(ev);

	if (md.active) {
		/* A modal owns the keyboard completely — controls underneath
		 * must not be reachable by Tab while it is up. */
		modal_event(ev);
		draw_header();
		modal_draw();
	} else if (ki_help) {
		global_keys(ev);
		draw_header();
		draw_help();
	} else {
		global_keys(ev);

		Page *p = ki_pages[ki_page];
		if (p->event && ev->type != EVT_NONE && !ui.consumed)
			ui.consumed = p->event(ev);

		draw_header();

		int content_y = 2;
		int content_h = term_h - 4 - content_y + 1;
		int sb = term_w >= 78 ? SIDEBAR_W : 0;

		if (sb)
			draw_sidebar(rect(0, content_y, sb, content_h));

		Rect body = rect(sb ? sb + 3 : 1, content_y,
				 term_w - (sb ? sb + 4 : 2), content_h);

		/* The page draws at its natural height, shifted by the scroll
		 * and clipped to the pane. The console this ships on is 25
		 * rows; without this, the pages that do not fit would lose
		 * their last question with no sign that it was ever there. */
		if (ui.wheel && ui.wheel_id < 0 && rect_hit(body, ui.mx, ui.my)) {
			page_scroll += ui.wheel * 2;
			ui.wheel = 0;
		}
		if (!ui.consumed && ev->type == EVT_KEY && page_overflow) {
			if (ev->key == K_PGDN) {
				page_scroll += body.h - 2;
				ui.consumed = 1;
			} else if (ev->key == K_PGUP) {
				page_scroll -= body.h - 2;
				ui.consumed = 1;
			}
		}
		if (page_scroll < 0)
			page_scroll = 0;

		draw_clip(body);
		draw_maxy = body.y - 1;
		p->draw(rect(body.x, body.y - page_scroll, body.w, body.h));
		int used = draw_maxy - (body.y - page_scroll) + 1;
		draw_clip_none();

		page_overflow = used > body.h;
		int maxscroll = used - body.h;
		if (maxscroll < 0)
			maxscroll = 0;
		if (page_scroll > maxscroll)
			page_scroll = maxscroll;

		/* Follow the Tab key: a control that walked off the pane pulls
		 * the pane after it. Only when the focus actually MOVED —
		 * doing it every frame means a manual PgDn is dragged straight
		 * back by whatever control is still focused at the top. */
		if (ui.focus_seen && ui.focus != last_focus) {
			if (ui.focus_rect.y < body.y)
				page_scroll -= body.y - ui.focus_rect.y;
			else if (ui.focus_rect.y + ui.focus_rect.h > body.y + body.h)
				page_scroll += ui.focus_rect.y + ui.focus_rect.h -
					       (body.y + body.h);
			if (page_scroll < 0)
				page_scroll = 0;
		}

		last_focus = ui.focus;

		if (page_overflow) {
			ui_scrollbar(rect(term_w - 1, body.y, 1, body.h), used,
				     body.h, page_scroll);
			if (page_scroll < maxscroll)
				draw_text(term_w - 1, body.y + body.h - 1, 1,
					  ki_glyph[G_DOWN], CL_ACCENT, CL_BG, 0);
		}

		draw_nav(term_h - 2);
	}

	draw_hints(term_h - 1);
	ui_frame_end();

	int px, py;
	if (input_mouse_visible(&px, &py))
		draw_cursor(px, py);

	draw_flush();
}

/* ──────────────────────────────────────────────────────────────────────── */

static void usage(void)
{
	printf("kinstall %s — the KDOS installer\n\n"
	       "usage: kinstall [options]\n\n"
	       "  --config FILE     read answers from FILE\n"
	       "  --save FILE       write the current answers and exit\n"
	       "  --unattended      skip the wizard and install from --config\n"
	       "  --dry-run         log every command, execute none\n"
	       "  --theme NAME      phosphor | amber | ice | bone\n"
	       "  --no-mouse        keyboard only\n"
	       "  --ascii           box drawing with - | +, for odd terminals\n"
	       "  --version\n"
	       "  --help\n\n"
	       "The log of a run is at /var/log/kinstall.log.\n",
	       KI_VERSION);
}

int main(int argc, char **argv)
{
	int want_mouse = 1, unattended = 0;
	const char *save_to = NULL;

	conf_defaults();

	for (int i = 1; i < argc; i++) {
		const char *a = argv[i];
		if (!strcmp(a, "--help") || !strcmp(a, "-h")) {
			usage();
			return 0;
		} else if (!strcmp(a, "--version")) {
			printf("kinstall %s\n", KI_VERSION);
			return 0;
		} else if (!strcmp(a, "--config") && i + 1 < argc) {
			if (conf_load(argv[++i]) < 0) {
				fprintf(stderr, "kinstall: cannot read %s\n",
					argv[i]);
				return 1;
			}
		} else if (!strcmp(a, "--save") && i + 1 < argc) {
			save_to = argv[++i];
		} else if (!strcmp(a, "--unattended")) {
			unattended = 1;
		} else if (!strcmp(a, "--dry-run")) {
			cfg.dry_run = 1;
		} else if (!strcmp(a, "--no-mouse")) {
			want_mouse = 0;
		} else if (!strcmp(a, "--ascii")) {
			setenv("KDOS_ASCII", "1", 1);
		} else if (!strcmp(a, "--theme") && i + 1 < argc) {
			strlcpy_(cfg.theme, argv[++i], sizeof(cfg.theme));
		} else {
			fprintf(stderr, "kinstall: unknown option %s\n", a);
			usage();
			return 1;
		}
	}

	if (save_to)
		return conf_save(save_to) == 0 ? 0 : 1;

	if (theme_set(cfg.theme) < 0)
		theme_set("phosphor");

	fputs("kinstall: measuring the live system...\n", stderr);
	probe_system();
	probe_disks();

	term_init(want_mouse);
	draw_init();
	input_init(want_mouse);

	inst.fd = -1;
	inst.logfd = 0;

	if (unattended) {
		install_plan();
		install_start(0);
		ki_page = 8;
	} else {
		page_goto(0);
	}

	while (!ki_quit) {
		Event ev;
		memset(&ev, 0, sizeof(ev));
		int timeout = inst.running ? 60 : 250;
		input_next(&ev, timeout);
		install_pump();
		render(&ev);

		/* An unattended run has nobody to press Reboot. It still shows
		 * the finished screen for a beat, so a watcher sees what
		 * happened rather than a machine that blinked and restarted. */
		if (unattended && inst.done && !inst.running && cfg.reboot_after) {
			static double at;
			if (!at)
				at = now_s();
			else if (now_s() - at > 5.0) {
				term_shutdown();
				execlp("reboot", "reboot", NULL);
				_exit(0);
			}
		}
	}

	if (inst.running)
		install_abort();

	input_shutdown();
	term_shutdown();
	return 0;
}
