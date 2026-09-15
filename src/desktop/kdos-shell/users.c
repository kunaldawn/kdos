/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   kdos-users — the accounts, and the one thing about them this can change
 *
 *   ┌ Accounts ────────────────────────────────────────────┐
 *   │ USER      UID   NAME              SHELL              │
 *   │ kdos      1000  KDOS User         /bin/bash          │
 *   ├──────────────────────────────────────────────────────┤
 *   │ Groups: wheel video audio input lpadmin              │
 *   │ Autologin: kdos                                      │
 *   └──────────────────────────────────────────────────────┘
 *
 * SPLIT BY PRIVILEGE, AND IT SAYS WHICH SIDE EACH ROW IS ON. Reading
 * `/etc/passwd` and `/etc/group` is anybody's; creating an account, changing
 * a password and editing group membership are root's, and this program does
 * none of them. A surface with an `Add user` button that answered "permission
 * denied" would be worse than one without: it would read as
 * a fault in the machine rather than as the boundary it is.
 *
 * THE ONE THING IT CAN CHANGE is which account tty1 logs in without asking,
 * because `/etc/kdos/con.conf` is a KDOS file and `kdos-powerd` already
 * answers "is this person administering the machine" for the power verbs. It
 * is the same question and it gets the same answer rather than a second one.
 *
 * THE LIST IS `kb_users()`, the same call the greeter makes. Two answers to
 * who may log in would disagree, and the disagreement would be invisible: an
 * account listed here and not offered at the login screen reads as a bug in
 * whichever one was opened second.
 *
 * WHAT IT DELIBERATELY DOES NOT DO. `passwd`, `adduser`, `deluser` and
 * `usermod` are on the image and are what a person changing accounts uses;
 * wrapping them would mean a surface that has to reproduce their prompts,
 * their validation and their failure modes, and would put a root-spawning
 * argument builder in the panel binary for a job done once per machine.
 * ---------------------------------
 */

#define _POSIX_C_SOURCE 200809L
#include <grp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "kbase.h"
#include "kwl.h"
#include "shell.h"

#define US_COLS 74
#define US_ROWS 18
#define US_MAX 64
#define US_GROUPS 256

enum { UB_AUTO, UB_REFRESH, UB_CLOSE, UB_N };

static KtuiKeys keys;
static KbUser users[US_MAX];
static int nuser;
static char groups[US_GROUPS];	/* the selected account's, space separated */
static char autologin[80];
static char status[160];
static KtuiTable tbl;
static int icons_on = 1;

static const char *etc(void)
{
	const char *e = getenv("KDOS_ETC");

	return e && *e ? e : "/etc";
}

/*
 * The groups an account is in, read from `/etc/group` rather than asked of
 * `getgrouplist`: the primary group is in the passwd entry and the rest are
 * membership lists, and this is the same file `kb_user_in_group()` decides
 * authorisation from — so what is drawn here is what the daemons will act on.
 */
static void load_groups(const KbUser *u)
{
	struct group *g;
	size_t used = 0;

	groups[0] = '\0';
	if (!u)
		return;
	setgrent();
	while ((g = getgrent()) != NULL) {
		int in = g->gr_gid == u->gid;

		for (char **m = g->gr_mem; !in && m && *m; m++)
			if (!strcmp(*m, u->name))
				in = 1;
		if (!in)
			continue;

		int k = snprintf(groups + used, sizeof(groups) - used, "%s%s",
				 used ? " " : "", g->gr_name);

		if (k < 0 || (size_t)k >= sizeof(groups) - used)
			break;
		used += (size_t)k;
	}
	endgrent();
}

/*
 * Which account tty1 logs in without asking. `greet = yes` means it asks, so
 * `autologin` is a line that does nothing — reporting its value there would
 * name an account that is not being logged in.
 */
static void load_autologin(void)
{
	char path[320], buf[16384];
	int greet = 0;
	char who[64] = "";

	snprintf(autologin, sizeof(autologin), "%s", "unknown");
	snprintf(path, sizeof(path), "%s/kdos/con.conf", etc());
	if (kb_read_file(path, buf, sizeof(buf)) <= 0)
		return;
	for (char *sp = NULL, *ln = strtok_r(buf, "\n", &sp); ln;
	     ln = strtok_r(NULL, "\n", &sp)) {
		char val[64];

		if (ln[0] == '#')
			continue;
		if (sscanf(ln, "greet = %63s", val) == 1)
			greet = !strcmp(val, "yes");
		else if (sscanf(ln, "autologin = %63s", val) == 1)
			snprintf(who, sizeof(who), "%s", val);
	}
	if (greet)
		snprintf(autologin, sizeof(autologin), "%s",
			 "off — tty1 asks who you are");
	else
		snprintf(autologin, sizeof(autologin), "%s",
			 who[0] ? who : "kdos");
}

static const KbUser *sel_user(void)
{
	return tbl.sel >= 0 && tbl.sel < nuser ? &users[tbl.sel] : NULL;
}

static void refresh(void)
{
	nuser = kb_users(users, US_MAX);
	ktui_table_clamp(&tbl, nuser, ktui_h > 8 ? ktui_h - 8 : 1);
	load_groups(sel_user());
	load_autologin();
}

/*
 * `kdos-power autologin <user>|off` — the same wheel-gated socket every power
 * verb goes through, because "may this person change how the machine logs in"
 * is the question that daemon already answers.
 */
static void toggle_autologin(void)
{
	const KbUser *u = sel_user();
	char buf[256];
	KbArgv a = { 0 };
	int on = strncmp(autologin, "off", 3) != 0;

	if (!u)
		return;
	kb_argv_add(&a, "kdos-power");
	kb_argv_add(&a, "autologin");
	/* Pressed on the account that is already the autologin, it turns it
	 * OFF; on any other account it moves it there. A toggle that could
	 * only ever be turned on would need a second control to undo it. */
	kb_argv_add(&a, on && !strcmp(autologin, u->name) ? "off" : u->name);
	kb_argv_end(&a);

	if (kb_run_capture(&a, buf, sizeof(buf)) != 0 || strncmp(buf, "ok", 2))
		snprintf(status, sizeof(status), "%s",
			 buf[0] ? buf : "kdos-powerd refused it");
	else
		snprintf(status, sizeof(status), "%s", buf);
	refresh();
}

/* ── drawing ───────────────────────────────────────────────────────────── */

static const KtuiCol US_COL[] = {
	{ "USER", 12 }, { "UID", 7 }, { "NAME", 0 }, { "SHELL", 18 }
};
#define US_NCOL 4

static void us_cell(int idx, int col, int x, int y, int w, int fg, int bg,
		    void *user)
{
	const KbUser *u = &users[idx];
	int on = bg == KT_ACCENT;

	(void)user;
	switch (col) {
	case 0:
		ktui_draw_text(x, y, w, u->name, fg, bg, KT_A_NONE);
		break;
	case 1:
		ktui_draw_textf(x, y, w, on ? KT_SURFACE : KT_DIM, bg,
				KT_A_NONE, "%u", (unsigned)u->uid);
		break;
	case 2:
		ktui_draw_text(x, y, w, u->gecos[0] ? u->gecos : "-",
			       on ? KT_SURFACE : u->gecos[0] ? fg : KT_DIM, bg,
			       KT_A_NONE);
		break;
	default:
		ktui_draw_text(x, y, w, u->shell, on ? KT_SURFACE : KT_DIM, bg,
			       KT_A_NONE);
		break;
	}
}

static void draw(void)
{
	int w = ktui_w, h = ktui_h;
	const KbUser *u = sel_user();
	int top, body;

	ktui_draw_fill(krect(0, 0, w, h), KT_BG);
	sh_frame(w, h, "Accounts", KT_ACCENT, KT_BG, 1);
	top = kch_header(w, "system-users", "Accounts",
			 "who may log in — changing one is `passwd` and `adduser`",
			 icons_on);

	/*
	 * SIX ROWS BELOW THE LIST, counted from the bottom: a rule, three
	 * facts, the hint row and the button bar. Counted from the top
	 * instead, the last of them lands under the bar on a short window and
	 * the fact simply is not there.
	 */
	body = h - top - 8;
	if (body < 1)
		body = 1;

	if (!nuser)
		ktui_draw_text(2, top, w - 4,
			       "no account with a uid of 1000 or more and a real shell",
			       KT_ERR, KT_BG, KT_A_NONE);
	else
		ktui_table_draw(krect(2, top, w - 4, body), &tbl, nuser, US_COL,
				US_NCOL, us_cell, NULL, NULL, -1);

	int y = h - 7;

	ktui_draw_hline(1, y, w - 2, KT_G_HL, KT_DIM, KT_BG);
	ktui_kv(2, y + 1, w - 4, "home", u ? u->home : "-", KT_TEXT);
	ktui_kv(2, y + 2, w - 4, "groups", groups[0] ? groups : "-", KT_TEXT);
	/* `tty1 logs in`, NOT `autologin`: there is one autologin and it is the
	 * machine's, so a row labelled with the setting's name sitting under
	 * `home` and `groups` reads as the selected account's own. The key
	 * column is sixteen cells, so a label that says which is a label that
	 * has to fit. */
	ktui_kv(2, y + 3, w - 4, "tty1 logs in", autologin,
		strncmp(autologin, "off", 3) ? KT_WARN : KT_MID);

	struct kch_button b[UB_N];

	b[UB_AUTO] = (struct kch_button){
		u && !strcmp(autologin, u->name) ? "No autologin" : "Autologin",
		u != NULL
	};
	b[UB_REFRESH] = (struct kch_button){ "Refresh", 1 };
	b[UB_CLOSE] = (struct kch_button){ "Close", 1 };

	int bx = kch_buttons(w, h - 2, b, UB_N, -1);
	int room = bx - 3;

	if (status[0] && room > 0)
		ktui_draw_text(2, h - 2, room, status, KT_MID, KT_BG,
			       KT_A_NONE);

	ktui_hint_if(u != NULL, "a", "autologin");
	ktui_hint("r", "refresh");
	ktui_hint("Esc", ktui_esc_verb(&keys));
	ktui_hint_row(&keys, krect(2, h - 3, w - 4, 1), KT_BG);
}

static int on_key(int k)
{
	if (ktui_table_key(&tbl, nuser, ktui_h - 8, k, NULL, NULL)) {
		load_groups(sel_user());
		return 0;
	}
	switch (k) {
	case 'a':
	case KT_K_ENTER:
		toggle_autologin();
		break;
	case 'r':
		refresh();
		break;
	case 'q':
		return 1;
	}
	return 0;
}

int users_main(int argc, char **argv)
{
	const char *font = NULL;
	int dump = 0;

	for (int i = 1; i < argc; i++) {
		if (!strcmp(argv[i], "--font") && i + 1 < argc)
			font = argv[++i];
		else if (!strcmp(argv[i], "--dump"))
			dump = 1;
		else if (!strcmp(argv[i], "--no-icons"))
			icons_on = 0;
		else {
			fprintf(stderr, "usage: kdos-users [--font NAME] "
					"[--no-icons] [--dump]\n");
			return 2;
		}
	}

	KDispConfig cfg = {
		.role = KDISP_ROLE_TOPLEVEL,
		.cols = US_COLS,
		.rows = US_ROWS,
		.min_cols = 56,
		.min_rows = 14,
		.title = "Accounts",
		.app_id = "kdos-users",
		.font = font,
		.keyboard = 1,
	};

	sh_theme_from_cache();
	refresh();
	if (dump) {
		ktui_offscreen_init(US_COLS, US_ROWS);
		ktui_draw_init();
		draw();
		ktui_draw_dump();
		return 0;
	}
	if (kdisp_init(&cfg, kdos_disp, kdos_disp_n) != 0) {
		fprintf(stderr, "kdos-users: no display server\n");
		return 1;
	}
	ktui_draw_init();

	while (!kdisp_should_close()) {
		draw();
		ktui_draw_flush();

		KtuiEvent ev;

		if (!ktui_backend()->poll_event(&ev, 1000)) {
			if (ktui_resized) {
				ktui_resized = 0;
				ktui_draw_resize();
				ktui_draw_invalidate();
			}
			continue;
		}
		if (ev.type == KT_EVT_MOUSE) {
			if (ev.press == KT_MP_DRAG) {
				kch_hover(ev.mx, ev.my);
				continue;
			}
			if (ev.press != KT_MP_PRESS)
				continue;

			int bi = kch_button_at(ev.mx, ev.my);

			if (bi == UB_CLOSE)
				break;
			if (bi == UB_AUTO) {
				toggle_autologin();
				continue;
			}
			if (bi == UB_REFRESH) {
				refresh();
				continue;
			}
			int idx = ktui_table_hit(krect(2, 4, ktui_w - 4,
						       ktui_h - 11),
						 &tbl, nuser, US_NCOL, US_COL,
						 ev.mx, ev.my);
			if (ktui_table_pick(&tbl, nuser, idx, NULL, NULL))
				load_groups(sel_user());
			continue;
		}
		if (ev.type != KT_EVT_KEY)
			continue;
		if (ktui_keys(&keys, &ev) == KTUI_KEY_CLOSE)
			break;
		if (on_key(ev.key))
			break;
	}

	kdisp_shutdown();
	return 0;
}
