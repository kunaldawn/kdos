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

/*
 * The index of a page by its id. An unattended install jumps straight to the
 * install page and used to do it by a literal 8 — which is right until a page
 * is added in front of it, and then an unattended install lands on the wrong
 * screen with the child already forked. -1 when there is no such page.
 */
int page_index(const char *id)
{
	for (int i = 0; i < ki_npages; i++)
		if (!strcmp(ki_pages[i]->id, id))
			return i;
	return -1;
}

void page_goto(int n)
{
	if (n < 0 || n >= ki_npages)
		return;
	ki_page = n;
	ktui_focus_set(0);
	page_scroll = 0;
	page_overflow = 0;
	last_focus = -1;
	nav_err[0] = 0;
	if (ki_pages[n]->enter)
		ki_pages[n]->enter();
	ktui_draw_invalidate();
}

void nav_next(void)
{
	Page *p = ki_pages[ki_page];
	if (p->validate && p->validate(nav_err, sizeof(nav_err))) {
		nav_err_at = kb_now_s();
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
	ktui_draw_fill(krect(0, 0, ktui_w, 1), KT_ACCENT);

	char left[80];
	snprintf(left, sizeof(left), " KDOS INSTALLER %s ", KI_VERSION);
	ktui_draw_text(1, 0, ktui_w, left, KT_BG, KT_ACCENT, 0);

	char right[96];
	time_t t = time(NULL);
	struct tm tm;
	localtime_r(&t, &tm);
	snprintf(right, sizeof(right), "%s  %d/%d  %02d:%02d ",
		 ki_pages[ki_page]->title, ki_page + 1, ki_npages, tm.tm_hour,
		 tm.tm_min);
	int rw = ktui_utf8_width(right);
	if (rw + (int)strlen(left) + 2 < ktui_w)
		ktui_draw_text(ktui_w - rw - 1, 0, rw, right, KT_BG, KT_ACCENT, 0);

	if (cfg.dry_run) {
		const char *dr = " DRY RUN ";
		ktui_draw_text((ktui_w - 9) / 2, 0, 9, dr, KT_ACCENT, KT_ERR, 0);
	}
}

static void draw_sidebar(KRect r)
{
	ktui_draw_vline(r.x + r.w, r.y, r.h, KT_G_VL, KT_DIM, KT_BG);

	int y = r.y;
	for (int i = 0; i < ki_npages; i++) {
		if (y >= r.y + r.h)
			break;
		int state = i < ki_page ? 2 : i == ki_page ? 1 : 0;
		const char *mark = state == 2 ? ktui_glyph[KT_G_SQUARE]
					      : state == 1 ? ktui_glyph[KT_G_RIGHT]
							   : ktui_glyph[KT_G_DOT];
		int fg = state == 1 ? KT_BG : state == 2 ? KT_MID : KT_DIM;
		int bg = state == 1 ? KT_ACCENT : KT_BG;

		ktui_draw_fill(krect(r.x, y, r.w, 1), bg);
		ktui_draw_textf(r.x + 1, y, r.w - 1, fg, bg, 0, "%s %d %s", mark, i + 1,
			   ki_pages[i]->title);

		/* Clicking a finished stage walks back to it; clicking ahead
		 * does nothing, because the pages ahead have not been
		 * validated and their state would be a guess.
		 *
		 * The rows register as chrome rather than calling ktui_id():
		 * the sidebar is drawn BEFORE the page, so claiming real focus
		 * ids here would push every control on every page ten places
		 * down the ring and leave the Tab order starting on a
		 * decoration. */
		ktui_hit_chrome(krect(r.x, y, r.w, 1), i);
		if (ktui_chrome_clicked(i) && i < ki_page &&
		    !ki_pages[ki_page]->hide_nav)
			page_goto(i);
		y++;
	}

	y = r.y + r.h - 5;
	if (y > ki_npages + r.y) {
		ktui_draw_hline(r.x, y++, r.w - 1, KT_G_HL, KT_DIM, KT_BG);
		ktui_draw_textf(r.x + 1, y++, r.w - 2, KT_DIM, KT_BG, 0, "accent  %s",
			   ktui_theme->name);
		ktui_draw_textf(r.x + 1, y++, r.w - 2, KT_DIM, KT_BG, 0, "mouse   %s",
			   (ktui_caps & KT_CAP_MOUSE)
				   ? ((ktui_caps & KT_CAP_LINUXVT) ? "evdev" : "sgr")
				   : "off");
		ktui_draw_textf(r.x + 1, y++, r.w - 2, KT_DIM, KT_BG, 0, "colour  %s",
			   (ktui_caps & KT_CAP_TRUECOLOR) ? "24-bit"
			   : (ktui_caps & KT_CAP_256)     ? "256"
			   : (ktui_caps & KT_CAP_LINUXVT) ? "vt palette"
						       : "ansi");
	}
}

static void draw_nav(int y)
{
	Page *p = ki_pages[ki_page];

	ktui_draw_hline(0, y - 1, ktui_w, KT_G_HL, KT_DIM, KT_BG);

	if (nav_err[0] && kb_now_s() - nav_err_at < 8.0)
		ktui_draw_textf(1, y, ktui_w - 40, KT_ERR, KT_BG, 0, "! %s", nav_err);
	else if (p->subtitle)
		ktui_draw_text(1, y, ktui_w - 40, p->subtitle, KT_DIM, KT_BG, 0);

	if (p->hide_nav)
		return;

	int bx = ktui_w - 34;
	if (ktui_button(krect(bx, y, 14, 1), "Back", ki_page > 0, 0))
		nav_back();
	/* The page before the summary says "Review", not "Next" — the label is
	 * the last chance to signal that the questionnaire is over. */
	if (ktui_button(krect(bx + 16, y, 16, 1),
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

	ktui_draw_fill(krect(0, y, ktui_w, 1), KT_SURFACE);
	ktui_draw_text(1, y, ktui_w - 2, hint, KT_MID, KT_SURFACE, 0);
}

static void draw_help(void)
{
	int w = 60, h = 20;
	if (w > ktui_w - 4)
		w = ktui_w - 4;
	if (h > ktui_h - 4)
		h = ktui_h - 4;
	int x = (ktui_w - w) / 2, y = (ktui_h - h) / 2;

	KRect r = krect(x, y, w, h);
	ktui_draw_fill(r, KT_SURFACE);
	ktui_draw_shadow(r);
	ktui_draw_box(r, "KEYS", KT_ACCENT, KT_SURFACE, 1);

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
		ktui_draw_text(x + 3, ry, 20, rows[i][0], KT_ACCENT, KT_SURFACE, 0);
		ktui_draw_text(x + 22, ry, w - 25, rows[i][1], KT_TEXT, KT_SURFACE, 0);
	}
}

/* ──────────────────────────────────────────────────────────────────────── */

static void quit_confirmed(void)
{
	ki_quit = 1;
}

static void global_keys(KtuiEvent *ev)
{
	if (ev->type != KT_EVT_KEY)
		return;

	if (ev->key == KT_K_F1) {
		ki_help = !ki_help;
		ktui_consume();
		return;
	}
	if (ki_help) {
		if (ev->key == KT_K_ESC || ev->key == KT_K_ENTER || ev->key == ' ') {
			ki_help = 0;
			ktui_consume();
		}
		return;
	}
	if ((ev->mods & KT_MOD_CTRL) && (ev->key == 'q' || ev->key == 'c')) {
		ktui_consume();
		if (inst.running)
			ktui_modal_confirm("Quit",
				      "The install is still running.\n"
				      "Stopping now leaves the target half written.",
				      "Stop and quit", "Keep going", quit_confirmed);
		else
			ktui_modal_confirm("Quit", "Leave the installer?", "Quit",
				      "Stay", quit_confirmed);
		return;
	}
	if (ki_pages[ki_page]->hide_nav)
		return;
	if ((ev->mods & KT_MOD_ALT) && ev->key == KT_K_RIGHT) {
		nav_next();
		ktui_consume();
	} else if ((ev->mods & KT_MOD_ALT) && ev->key == KT_K_LEFT) {
		nav_back();
		ktui_consume();
	} else if (ev->key == KT_K_ESC) {
		nav_back();
		ktui_consume();
	}
}

static void render(KtuiEvent *ev)
{
	if (ktui_w < MIN_W || ktui_h < MIN_H) {
		ktui_toosmall("KDOS INSTALLER", MIN_W, MIN_H);
		return;
	}

	ktui_draw_clear();
	ktui_frame_begin(ev);

	if (ktui_modal_active()) {
		/* A modal owns the keyboard completely — controls underneath
		 * must not be reachable by Tab while it is up. */
		ktui_modal_event(ev);
		draw_header();
		ktui_modal_draw();
	} else if (ki_help) {
		global_keys(ev);
		draw_header();
		draw_help();
	} else {
		global_keys(ev);

		Page *p = ki_pages[ki_page];
		if (p->event && ev->type != KT_EVT_NONE && !ktui_consumed() &&
		    p->event(ev))
			ktui_consume();

		draw_header();

		int content_y = 2;
		int content_h = ktui_h - 4 - content_y + 1;
		int sb = ktui_w >= 78 ? SIDEBAR_W : 0;

		if (sb)
			draw_sidebar(krect(0, content_y, sb, content_h));

		KRect body = krect(sb ? sb + 3 : 1, content_y,
				 ktui_w - (sb ? sb + 4 : 2), content_h);

		/* The page draws at its natural height, shifted by the scroll
		 * and clipped to the pane. The console this ships on is 25
		 * rows; without this, the pages that do not fit would lose
		 * their last question with no sign that it was ever there. */
		page_scroll += ktui_wheel_take(body) * 2;
		if (!ktui_consumed() && ev->type == KT_EVT_KEY && page_overflow) {
			if (ev->key == KT_K_PGDN) {
				page_scroll += body.h - 2;
				ktui_consume();
			} else if (ev->key == KT_K_PGUP) {
				page_scroll -= body.h - 2;
				ktui_consume();
			}
		}
		if (page_scroll < 0)
			page_scroll = 0;

		ktui_draw_clip(body);
		ktui_extent_reset(body.y - 1);
		p->draw(krect(body.x, body.y - page_scroll, body.w, body.h));
		int used = ktui_extent() - (body.y - page_scroll) + 1;
		ktui_draw_clip_none();

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
		KRect fr;
		if (ktui_focus_rect(&fr) && ktui_focus_get() != last_focus) {
			if (fr.y < body.y)
				page_scroll -= body.y - fr.y;
			else if (fr.y + fr.h > body.y + body.h)
				page_scroll += fr.y + fr.h - (body.y + body.h);
			if (page_scroll < 0)
				page_scroll = 0;
		}

		last_focus = ktui_focus_get();

		if (page_overflow) {
			ktui_scrollbar(krect(ktui_w - 1, body.y, 1, body.h), used,
				     body.h, page_scroll);
			if (page_scroll < maxscroll)
				ktui_draw_text(ktui_w - 1, body.y + body.h - 1, 1,
					  ktui_glyph[KT_G_DOWN], KT_ACCENT, KT_BG, 0);
		}

		draw_nav(ktui_h - 2);
	}

	draw_hints(ktui_h - 1);
	ktui_frame_end();

	int px, py;
	if (ktui_input_mouse_visible(&px, &py))
		ktui_draw_cursor(px, py);

	ktui_draw_flush();
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
	       "  --dump probe      what the installer sees on this machine\n"
	       "  --dump plan       the steps these answers would run\n"
	       "  --json            render --dump as JSON instead of text\n"
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
	int want_mouse = 1, unattended = 0, json = 0;
	const char *save_to = NULL, *dump = NULL;

	/* An allocation failure has to drop the terminal before it prints, or
	 * the message lands in the alternate screen and dies with it. */
	kb_set_progname("kinstall");
	kb_set_oom_handler(ktui_term_shutdown);

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
		} else if (!strcmp(a, "--dump") && i + 1 < argc) {
			dump = argv[++i];
		} else if (!strcmp(a, "--json")) {
			json = 1;
		} else if (!strcmp(a, "--unattended")) {
			unattended = 1;
		} else if (!strcmp(a, "--dry-run")) {
			cfg.dry_run = 1;
		} else if (!strcmp(a, "--no-mouse")) {
			want_mouse = 0;
		} else if (!strcmp(a, "--ascii")) {
			setenv("KDOS_ASCII", "1", 1);
		} else if (!strcmp(a, "--theme") && i + 1 < argc) {
			kb_strlcpy(cfg.theme, argv[++i], sizeof(cfg.theme));
		} else {
			fprintf(stderr, "kinstall: unknown option %s\n", a);
			usage();
			return 1;
		}
	}

	if (save_to)
		return conf_save(save_to) == 0 ? 0 : 1;

	/* Before the terminal, before the probe's own progress line: a dump is
	 * stdout and only stdout. */
	if (dump)
		return ki_dump(dump, json);

	if (ktui_theme_set(cfg.theme) < 0)
		ktui_theme_set("phosphor");

	fputs("kinstall: measuring the live system...\n", stderr);
	probe_system();
	probe_disks();

	ktui_term_init(want_mouse);
	ktui_draw_init();
	ktui_input_init(want_mouse);

	inst.fd = -1;
	inst.logfd = 0;

	if (unattended) {
		/* The wizard reaches the Applications page on its way past;
		 * an unattended install does not, and would carry no packs at
		 * all with an answer file that named some. */
		ki_packs_enter();
		install_plan();
		install_start(0);
		ki_page = page_index("install");
	} else {
		page_goto(0);
	}

	while (!ki_quit) {
		KtuiEvent ev;
		memset(&ev, 0, sizeof(ev));
		int timeout = inst.running ? 60 : 250;
		ktui_input_next(&ev, timeout);
		install_pump();
		render(&ev);

		/*
		 * AN UNATTENDED RUN ENDS BY ITSELF, WHICHEVER WAY IT WENT.
		 * There is nobody to press Reboot and nobody to press
		 * anything else either, so a finished install that only knew
		 * how to reboot sat on its own last screen for ever — which
		 * is a scripted install that never returns, and the one
		 * outcome "unattended" must not have. It still shows the
		 * finished screen for a beat, so a watcher sees what happened
		 * rather than a machine that blinked and restarted.
		 *
		 * THE FAILED CASE COUNTS AS ENDED. `inst.done` is only set by
		 * a run that reached the last step, so a test for it alone
		 * leaves exactly the install that went wrong spinning on its
		 * own error screen — which is the run somebody most needs the
		 * exit status of.
		 */
		if (unattended && !inst.running && (inst.done || inst.failed)) {
			static double at;
			if (!at)
				at = kb_now_s();
			else if (kb_now_s() - at > 5.0) {
				if (cfg.reboot_after) {
					ktui_term_shutdown();
					execlp("reboot", "reboot", NULL);
					_exit(0);
				}
				ki_quit = 1;
			}
		}
	}

	if (inst.running)
		install_abort();

	ktui_input_shutdown();
	ktui_term_shutdown();
	/* The exit status is the answer a script reads: an install that failed
	 * and returned 0 is worse than one that never returned. */
	return unattended && inst.failed ? 1 : 0;
}
