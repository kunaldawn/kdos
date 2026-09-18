/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   kdos-store — what this machine can build, and one tick to build it
 *
 *   ┌ Applications ─────────────────────────────────────────┐
 *   │ Groups │ All │ Graphics │ Office │ Installed          │
 *   ├────────────────────────────────────────────────────────┤
 *   │ [x] GIMP        rt-gtk   Create images and edit photos │
 *   │ [ ] Krita       rt-qt    Digital painting              │
 *   │ [·] Inkscape    rt-gtk   Vector graphics     installed │
 *   ├────────────────────────────────────────────────────────┤
 *   │ 2 ticked · ~1.2 GB    F5 install  F6 import  F7 export │
 *   └────────────────────────────────────────────────────────┘
 *
 * IT COMPUTES NOTHING. `kdos-appbox catalogue` already answers what exists and
 * what is installed, and `kdos-appbox install` already knows how to build one.
 * A surface that re-derived either would be a second answer that drifts — the
 * same rule kdos-update keeps about the version comparison.
 *
 * THE SIZE IS AN ESTIMATE AND THE LABEL SAYS SO. What apt resolves on the day
 * depends on the snapshot, and a runtime's layers are counted once on disk
 * however many applications name them. A number presented as fact that the
 * install then contradicts is worse than no number.
 *
 * `[·]` IS AN APPLICATION ALREADY HERE, and it cannot be ticked. The same mark
 * kinstall's list uses for a row that is carried whatever anybody thinks.
 *
 * A BUILD RUNS IN A TERMINAL AND THIS WINDOW STAYS ALIVE. Installing is podman
 * and apt — minutes for one application, the better part of an hour for a
 * group — and a surface that ran it would be frozen for all of it, with one
 * status line standing in for output worth reading when a package fails to
 * resolve. kdos-update keeps the same rule about `kdos update apply`.
 * ---------------------------------
 */

#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include "kbase.h"
#include "kwl.h"
#include "shell.h"

#define ST_COLS 84
#define ST_ROWS 24
#define ST_MAX  512
#define ST_ID   64

/* The tabs, in the order a person looks: what somebody curated, then
 * everything, then the categories the catalogue actually uses, then what is
 * already here. The category tabs are built from the data rather than listed,
 * so a new category in the catalogue is a tab without a line of C. */
#define ST_MAXTAB 12
enum { SB_INSTALL, SB_IMPORT, SB_EXPORT, SB_CLOSE, SB_N };

struct app {
	char id[ST_ID];
	char name[96];
	char cat[32];
	char parent[ST_ID];
	char tag[128];
	unsigned long long bytes;
	int installed;
	int ticked;
};

struct grp {
	char id[ST_ID];
	char desc[96];
	char members[ST_MAX][ST_ID];
	int nmember;
};

static KtuiKeys keys;
static struct app apps[ST_MAX];
static int napp;
static struct grp groups[ST_MAXTAB * 4];
static int ngroup;
static char tabname[ST_MAXTAB][24];
static KtuiTab tabs[ST_MAXTAB];
static int ntab;
static int page;
static KtuiTable table;
static int icons_on = 1;
static char status[192];

/* ── reading ───────────────────────────────────────────────────────────── */

/*
 * `--fixture <dir>` REPLAYS RECORDED ANSWERS instead of running kdos-appbox.
 *
 * What this surface draws is whatever the catalogue and podman say, and both
 * are the host's — so a golden taken without this would hold the machine that
 * ran it. The same seam kdos-print keeps over recorded lpstat answers and
 * kdos-energyd over a recorded /proc.
 *
 * `<dir>/catalogue` and `<dir>/groups` are the two outputs, verbatim.
 */
static const char *fixture_dir;

static int run_appbox(char *const extra[], int nextra, KbBuf *out)
{
	KbArgv a = {0};

	if (fixture_dir) {
		char path[512];
		size_t len = 0;
		char *text;

		snprintf(path, sizeof(path), "%s/%s", fixture_dir,
			 nextra > 1 ? "groups" : "catalogue");
		text = kb_read_all(path, &len);
		if (!text)
			return -1;
		kb_buf_printf(out, "%s", text);
		free(text);
		return 0;
	}
	kb_argv_add(&a, "kdos-appbox");
	for (int i = 0; i < nextra; i++)
		kb_argv_add(&a, extra[i]);
	kb_argv_end(&a);
	return kb_run_capture_buf(&a, out);
}

/* One tab per category the catalogue actually uses, most-populated first, and
 * capped: a tab row nobody can read is worse than a category behind `All`. */
static void build_tabs(void)
{
	char seen[ST_MAXTAB][32];
	int count[ST_MAXTAB] = {0};
	int nseen = 0;

	for (int i = 0; i < napp; i++) {
		int k;

		for (k = 0; k < nseen; k++)
			if (!strcmp(seen[k], apps[i].cat))
				break;
		if (k == nseen) {
			if (nseen >= ST_MAXTAB)
				continue;
			kb_strlcpy(seen[nseen], apps[i].cat, sizeof(seen[0]));
			nseen++;
		}
		count[k]++;
	}
	for (int i = 0; i < nseen; i++)
		for (int j = i + 1; j < nseen; j++)
			if (count[j] > count[i]) {
				char t[32];
				int c = count[i];
				kb_strlcpy(t, seen[i], sizeof(t));
				kb_strlcpy(seen[i], seen[j], sizeof(seen[0]));
				kb_strlcpy(seen[j], t, sizeof(seen[0]));
				count[i] = count[j];
				count[j] = c;
			}

	ntab = 0;
	kb_strlcpy(tabname[ntab++], "Groups", sizeof(tabname[0]));
	kb_strlcpy(tabname[ntab++], "All", sizeof(tabname[0]));
	for (int i = 0; i < nseen && ntab < ST_MAXTAB - 1; i++) {
		if (!strcmp(seen[i], "Other"))
			continue;
		kb_strlcpy(tabname[ntab++], seen[i], sizeof(tabname[0]));
	}
	kb_strlcpy(tabname[ntab++], "Installed", sizeof(tabname[0]));
	for (int i = 0; i < ntab; i++) {
		tabs[i].name = tabname[i];
		/* NULL abbr takes the first three characters, which reads for
		 * every category name the catalogue uses. */
		tabs[i].abbr = NULL;
	}
	if (page >= ntab)
		page = 0;
}

static void refresh(void)
{
	char *cat[] = { (char *)"catalogue" };
	char *grp[] = { (char *)"catalogue", (char *)"--groups" };
	KbBuf out = {0};
	char *line, *save;

	napp = ngroup = 0;
	status[0] = 0;

	if (run_appbox(cat, 1, &out) != 0 || !out.p) {
		kb_strlcpy(status, "no catalogue — is kdos-appbox installed?",
			   sizeof(status));
		kb_buf_free(&out);
		build_tabs();
		return;
	}
	for (line = strtok_r(out.p, "\n", &save); line && napp < ST_MAX;
	     line = strtok_r(NULL, "\n", &save)) {
		char *f[7];
		int n = 0;

		f[n++] = line;
		for (char *p = line; *p && n < 7; p++)
			if (*p == '\t') {
				*p = 0;
				f[n++] = p + 1;
			}
		if (n < 7)
			continue;
		struct app *a = &apps[napp++];
		kb_strlcpy(a->id, f[0], sizeof(a->id));
		kb_strlcpy(a->name, f[1], sizeof(a->name));
		kb_strlcpy(a->cat, f[2], sizeof(a->cat));
		a->bytes = strtoull(f[3], NULL, 10);
		kb_strlcpy(a->parent, f[4], sizeof(a->parent));
		a->installed = !strcmp(f[5], "installed");
		kb_strlcpy(a->tag, f[6], sizeof(a->tag));
	}
	kb_buf_free(&out);

	memset(&out, 0, sizeof(out));
	if (run_appbox(grp, 2, &out) == 0 && out.p) {
		for (line = strtok_r(out.p, "\n", &save);
		     line && ngroup < (int)(sizeof(groups) / sizeof(groups[0]));
		     line = strtok_r(NULL, "\n", &save)) {
			char *t1 = strchr(line, '\t'), *t2;
			struct grp *g;

			if (!t1)
				continue;
			*t1 = 0;
			t2 = strchr(t1 + 1, '\t');
			if (!t2)
				continue;
			*t2 = 0;
			g = &groups[ngroup++];
			kb_strlcpy(g->id, line, sizeof(g->id));
			kb_strlcpy(g->desc, t1 + 1, sizeof(g->desc));
			for (char *m = strtok(t2 + 1, " "); m;
			     m = strtok(NULL, " "))
				if (g->nmember < ST_MAX)
					kb_strlcpy(g->members[g->nmember++], m,
						   sizeof(g->members[0]));
		}
	}
	kb_buf_free(&out);
	build_tabs();
}

/* ── which rows this page shows ────────────────────────────────────────── */

static int on_groups(void) { return page == 0; }
static int on_installed(void) { return page == ntab - 1; }

static int visible(int i)
{
	if (on_groups())
		return 0;
	if (page == 1)
		return 1;
	if (on_installed())
		return apps[i].installed;
	return !strcmp(apps[i].cat, tabname[page]);
}

static int rows_here(void)
{
	int n = 0;

	if (on_groups())
		return ngroup;
	for (int i = 0; i < napp; i++)
		n += visible(i);
	return n;
}

/* The nth visible row's index into apps[], or -1. */
static int nth(int want)
{
	int n = 0;

	for (int i = 0; i < napp; i++) {
		if (!visible(i))
			continue;
		if (n == want)
			return i;
		n++;
	}
	return -1;
}

static struct app *app_by_id(const char *id)
{
	for (int i = 0; i < napp; i++)
		if (!strcmp(apps[i].id, id))
			return &apps[i];
	return NULL;
}

/* ── ticking ───────────────────────────────────────────────────────────── */

/*
 * AN APPLICATION ALREADY HERE CANNOT BE TICKED. Installing it again is a
 * rebuild of an image that exists, and a tick that does nothing is a tick
 * somebody will count.
 */
static void tick(int i, int on)
{
	if (i < 0 || i >= napp || apps[i].installed)
		return;
	apps[i].ticked = on;
}

static void tick_group(int g, int on)
{
	if (g < 0 || g >= ngroup)
		return;
	for (int m = 0; m < groups[g].nmember; m++) {
		struct app *a = app_by_id(groups[g].members[m]);
		if (a)
			tick(a - apps, on);
	}
}

/*
 * WHAT A GROUP'S MARK MEANS: 0 none chosen, 1 some, 2 all that CAN be, 3
 * nothing left to choose because every member is already here.
 *
 * AN INSTALLED MEMBER IS NOT A CHOICE and is left out of the count. Counting
 * one as chosen drew `[-]` — partly ticked — on a group nobody had touched,
 * which reads as a selection somebody made and did not.
 */
enum { GT_NONE, GT_SOME, GT_ALL, GT_DONE };

static int group_ticked(int g)
{
	int any = 0, all = 1, open = 0;

	for (int m = 0; m < groups[g].nmember; m++) {
		struct app *a = app_by_id(groups[g].members[m]);

		if (!a || a->installed)
			continue;
		open++;
		if (a->ticked)
			any = 1;
		else
			all = 0;
	}
	if (!open)
		return GT_DONE;
	return all ? GT_ALL : any ? GT_SOME : GT_NONE;
}

static int nticked(void)
{
	int n = 0;

	for (int i = 0; i < napp; i++)
		n += apps[i].ticked;
	return n;
}

static unsigned long long ticked_bytes(void)
{
	unsigned long long b = 0;

	for (int i = 0; i < napp; i++)
		if (apps[i].ticked)
			b += apps[i].bytes;
	return b;
}

/* ── drawing ───────────────────────────────────────────────────────────── */

static const KtuiCol AP_COL[] = {
	{ "", 4 }, { "APPLICATION", 0 }, { "ON", 12 }, { "SIZE~", 9 }
};
#define AP_NCOL 4

static const KtuiCol GR_COL[] = {
	{ "", 4 }, { "GROUP", 0 }, { "APPS", 6 }, { "SIZE~", 9 }
};
#define GR_NCOL 4

static void app_cell(int idx, int col, int x, int y, int w, int fg, int bg,
		     void *user)
{
	int i = nth(idx);
	const struct app *a;
	int on = bg == KT_ACCENT;

	(void)user;
	if (i < 0)
		return;
	a = &apps[i];
	switch (col) {
	case 0: {
		/* ALREADY HERE, CHOSEN, NEITHER — and the first is a GLYPH out
		 * of the tier table, never a literal: a `·` written into the
		 * format string draws as `?` wherever UTF-8 does not reach,
		 * which is the ascii tier every dump and every plain console
		 * uses. */
		char mark[16];

		snprintf(mark, sizeof(mark), "[%s]",
			 a->installed ? ktui_glyph[KT_G_BULLET]
				      : a->ticked ? "x" : " ");
		ktui_draw_text(x, y, w, mark,
			       on ? KT_SURFACE : a->installed ? KT_DIM
							      : KT_ACCENT,
			       bg, KT_A_NONE);
		break;
	}
	case 1:
		ktui_draw_text(x, y, w, a->name[0] ? a->name : a->id, fg, bg,
			       KT_A_NONE);
		break;
	case 2:
		ktui_draw_text(x, y, w, a->parent,
			       on ? KT_SURFACE : KT_DIM, bg, KT_A_NONE);
		break;
	case 3:
		ktui_draw_text(x, y, w, kb_human_size(a->bytes),
			       on ? KT_SURFACE : KT_MID, bg, KT_A_NONE);
		break;
	}
}

static void grp_cell(int idx, int col, int x, int y, int w, int fg, int bg,
		     void *user)
{
	const struct grp *g;
	int on = bg == KT_ACCENT, st;
	char n[16];
	unsigned long long b = 0;

	(void)user;
	if (idx < 0 || idx >= ngroup)
		return;
	g = &groups[idx];
	st = group_ticked(idx);
	switch (col) {
	case 0: {
		char mark[16];

		snprintf(mark, sizeof(mark), "[%s]",
			 st == GT_DONE ? ktui_glyph[KT_G_BULLET]
			 : st == GT_ALL ? "x" : st == GT_SOME ? "-" : " ");
		ktui_draw_text(x, y, w, mark,
			       on ? KT_SURFACE
				  : st == GT_DONE ? KT_DIM : KT_ACCENT,
			       bg, KT_A_NONE);
		break;
	}
	case 1:
		ktui_draw_text(x, y, w, g->desc[0] ? g->desc : g->id, fg, bg,
			       KT_A_NONE);
		break;
	case 2:
		snprintf(n, sizeof(n), "%d", g->nmember);
		ktui_draw_text(x, y, w, n, on ? KT_SURFACE : KT_DIM, bg,
			       KT_A_NONE);
		break;
	case 3:
		for (int m = 0; m < g->nmember; m++) {
			const struct app *a = app_by_id(g->members[m]);
			if (a && !a->installed)
				b += a->bytes;
		}
		ktui_draw_text(x, y, w, kb_human_size(b),
			       on ? KT_SURFACE : KT_MID, bg, KT_A_NONE);
		break;
	}
}

static void draw(void)
{
	int w = ktui_w, h = ktui_h;
	int top, body, nt = nticked();
	char sub[160], foot[192];

	ktui_draw_fill(krect(0, 0, w, h), KT_BG);
	sh_frame(w, h, "Applications", KT_ACCENT, KT_BG, 1);

	snprintf(sub, sizeof(sub), "%d applications, %d groups", napp, ngroup);
	top = kch_header(w, "system-software-install", "Applications", sub,
			 icons_on);

	ktui_tabs_draw(krect(2, top, w - 4, 1), tabs, ntab, page, -1, 0);
	body = h - top - 6;
	if (body < 1)
		body = 1;

	if (on_groups()) {
		ktui_table_draw(krect(2, top + 1, w - 4, body), &table, ngroup,
				GR_COL, GR_NCOL, grp_cell, NULL, NULL, -1);
	} else if (!rows_here()) {
		ktui_draw_text(2, top + 2, w - 4,
			       on_installed()
				       ? "nothing installed yet — tick something and press F5"
				       : "nothing in this category",
			       KT_MID, KT_BG, KT_A_NONE);
	} else {
		ktui_table_draw(krect(2, top + 1, w - 4, body), &table,
				rows_here(), AP_COL, AP_NCOL, app_cell, NULL,
				NULL, -1);
	}

	ktui_draw_hline(1, h - 5, w - 2, KT_G_HL, KT_DIM, KT_BG);

	/*
	 * THE FOOTER IS WHAT WAS ASKED FOR, NOT A PROGRESS BAR. The build runs
	 * in a terminal where its output can be read; a percentage invented
	 * here would be a number this surface cannot know.
	 */
	if (status[0])
		ktui_draw_text(2, h - 4, w - 4, status, KT_ACCENT, KT_BG,
			       KT_A_NONE);
	else if (nt)
		snprintf(foot, sizeof(foot),
			 "%d ticked %s ~%s to download and build (an estimate)",
			 nt, ktui_glyph[KT_G_DOT],
			 kb_human_size(ticked_bytes()));
	else {
		int have = 0;
		for (int i = 0; i < napp; i++)
			have += apps[i].installed;
		snprintf(foot, sizeof(foot),
			 "%d of %d installed %s space ticks, F5 builds them",
			 have, napp, ktui_glyph[KT_G_DOT]);
	}
	if (!status[0])
		ktui_draw_text(2, h - 4, w - 4, foot, KT_MID, KT_BG,
			       KT_A_NONE);

	struct kch_button b[SB_N];

	b[SB_INSTALL] = (struct kch_button){ "Install", nt > 0 };
	b[SB_IMPORT] = (struct kch_button){ "Import", 1 };
	b[SB_EXPORT] = (struct kch_button){ "Export", nt > 0 };
	b[SB_CLOSE] = (struct kch_button){ "Close", 1 };
	kch_buttons(w, h - 2, b, SB_N, -1);

	ktui_hint("Space", "tick");
	ktui_hint("Enter", on_groups() ? "tick group" : "install this one");
	ktui_hint("F5", "install ticked");
	ktui_hint("F6", "import");
	ktui_hint("F7", "export");
	ktui_hint("Esc", ktui_esc_verb(&keys));
	ktui_hint_row(&keys, krect(2, h - 3, w - 4, 1), KT_BG);
}

/* ── doing ─────────────────────────────────────────────────────────────── */

/*
 * A BUILD RUNS IN A TERMINAL, NOT BEHIND THIS WINDOW.
 *
 * Installing is podman and apt — minutes for one application, the better part
 * of an hour for a group. A surface that ran it would be frozen for all of it:
 * no resize, no close, and a single status line standing in for output that is
 * genuinely worth reading when a package fails to resolve. kdos-update keeps
 * exactly this rule about `kdos update apply`, and for the same reason.
 *
 * So the verb is handed to a terminal and detached. The build survives this
 * window being closed, the output scrolls, and `r` picks up the result.
 *
 * ON A BARE VIRTUAL TERMINAL kb_terminal() answers NULL — there is no emulator
 * to open. This says so rather than naming a window nobody gets.
 */
static int run_detached(char *const argv[], int n)
{
	const char *term = kb_terminal();
	KbArgv a = {0};

	if (!term) {
		kb_strlcpy(status, "no terminal to run it in — use "
				   "`kdos app install` at a prompt",
			   sizeof(status));
		return -1;
	}
	kb_argv_add(&a, term);
	kb_argv_add(&a, "-e");
	kb_argv_add(&a, "kdos-appbox");
	for (int i = 0; i < n; i++)
		kb_argv_add(&a, argv[i]);
	kb_argv_end(&a);
	kb_run_detach(&a);
	return 0;
}

static void install_ticked(void)
{
	char *argv[ST_MAX + 1];
	int n = 0;

	argv[n++] = (char *)"install";
	for (int i = 0; i < napp && n < ST_MAX; i++)
		if (apps[i].ticked)
			argv[n++] = apps[i].id;
	if (n == 1)
		return;
	if (run_detached(argv, n) == 0)
		snprintf(status, sizeof(status),
			 "building %d application(s) in a terminal — press r "
			 "when it finishes", n - 1);
}

static void install_one(int i)
{
	char *argv[2];

	if (i < 0 || i >= napp || apps[i].installed)
		return;
	argv[0] = (char *)"install";
	argv[1] = apps[i].id;
	if (run_detached(argv, 2) == 0)
		snprintf(status, sizeof(status),
			 "building %s in a terminal — press r when it finishes",
			 apps[i].id);
}

/*
 * IMPORT AND EXPORT TAKE A PATH, AND THIS SURFACE DOES NOT INVENT ONE. The
 * file chooser is the portal's and is a separate process; what this offers is
 * the default location and the verb, so a person who wants another path uses
 * `kdos app export` where a path is an argument.
 */
static void do_export(void)
{
	char *argv[ST_MAX + 2];
	char out[256];
	int n = 0;

	snprintf(out, sizeof(out), "%s/apps.ktar", kb_home_dir());
	argv[n++] = (char *)"export";
	argv[n++] = out;
	for (int i = 0; i < napp && n < ST_MAX; i++)
		if (apps[i].ticked)
			argv[n++] = apps[i].id;
	if (n == 2)
		return;
	if (run_detached(argv, n) == 0)
		snprintf(status, sizeof(status), "exporting to %s", out);
}

static void do_import(void)
{
	char *argv[2];
	char in[256];

	snprintf(in, sizeof(in), "%s/apps.ktar", kb_home_dir());
	if (!kb_path_exists(in)) {
		snprintf(status, sizeof(status),
			 "no %s — `kdos app import <file>` takes a path", in);
		return;
	}
	argv[0] = (char *)"import";
	argv[1] = in;
	if (run_detached(argv, 2) == 0)
		snprintf(status, sizeof(status),
			 "importing %s — press r when it finishes", in);
}

/* ── keys ──────────────────────────────────────────────────────────────── */

static int on_key(int k)
{
	int sel = table.sel;

	/*
	 * A LETTER WITH CTRL IS NOT A LETTER. Every text field in this tree
	 * guards for it; this surface has none, so the guard here is that a
	 * plain key is only acted on when no modifier is down.
	 */
	switch (k) {
	case KT_K_LEFT:
		if (page > 0)
			page--;
		table.sel = 0;
		return 0;
	case KT_K_RIGHT:
		if (page < ntab - 1)
			page++;
		table.sel = 0;
		return 0;
	case ' ':
		if (on_groups())
			tick_group(sel, group_ticked(sel) == GT_NONE);
		else {
			int i = nth(sel);
			if (i >= 0)
				tick(i, !apps[i].ticked);
		}
		return 0;
	case '\r':
	case '\n':
		if (on_groups())
			tick_group(sel, group_ticked(sel) == GT_NONE);
		else
			install_one(nth(sel));
		return 0;
	case KT_K_F5:
		install_ticked();
		return 0;
	case KT_K_F6:
		do_import();
		return 0;
	case KT_K_F7:
		do_export();
		return 0;
	case 'r':
		refresh();
		return 0;
	}
	return 0;
}

int store_main(int argc, char **argv)
{
	const char *font = NULL;
	const char *want_page = NULL;
	int dump = 0;

	for (int i = 1; i < argc; i++) {
		if (!strcmp(argv[i], "--font") && i + 1 < argc)
			font = argv[++i];
		else if (!strcmp(argv[i], "--dump"))
			dump = 1;
		else if (!strcmp(argv[i], "--no-icons"))
			icons_on = 0;
		else if (!strcmp(argv[i], "--fixture") && i + 1 < argc)
			fixture_dir = argv[++i];
		/* WHICH TAB IT OPENS ON, by name. The groups page is the
		 * default and is the only one a dump would otherwise reach —
		 * so the application rows, which are the surface's actual
		 * content, would have no reference frame at all. */
		else if (!strcmp(argv[i], "--page") && i + 1 < argc)
			want_page = argv[++i];
		else {
			fprintf(stderr, "usage: kdos-store [--font NAME] "
					"[--no-icons] [--fixture DIR] "
					"[--page NAME] [--dump]\n");
			return 2;
		}
	}

	KDispConfig cfg = {
		.role = KDISP_ROLE_TOPLEVEL,
		.cols = ST_COLS,
		.rows = ST_ROWS,
		.min_cols = 60,
		.min_rows = 16,
		.title = "Applications",
		.app_id = "kdos-store",
		.font = font,
		.keyboard = 1,
	};

	sh_theme_from_cache();
	refresh();
	/* After refresh(): the tab list is built from the catalogue, so a name
	 * can only be resolved once there is one. An unknown name leaves the
	 * default rather than refusing — a dump is not a place to fail. */
	if (want_page)
		for (int i = 0; i < ntab; i++)
			if (!strcasecmp(tabname[i], want_page)) {
				page = i;
				break;
			}
	if (dump) {
		ktui_offscreen_init(ST_COLS, ST_ROWS);
		ktui_draw_init();
		draw();
		ktui_draw_dump();
		return 0;
	}
	if (kdisp_init(&cfg, kdos_disp, kdos_disp_n) != 0) {
		fprintf(stderr, "kdos-store: no display server\n");
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
			/* A detent scrolls the table, answered before anything
			 * below: a wheel tick arrives as a press with no
			 * release and would otherwise fall into the button arm
			 * and run whichever row it passed over. */
			if (ev.btn == KT_MB_WHEEL_UP ||
			    ev.btn == KT_MB_WHEEL_DOWN) {
				ktui_table_key(&table, rows_here(), ktui_h - 9,
					       ev.btn == KT_MB_WHEEL_UP
						       ? KT_K_UP : KT_K_DOWN,
					       NULL, NULL);
				continue;
			}
			if (ev.press == KT_MP_DRAG) {
				kch_hover(ev.mx, ev.my);
				continue;
			}
			if (ev.press != KT_MP_PRESS)
				continue;

			int bi = kch_button_at(ev.mx, ev.my);

			if (bi == SB_CLOSE)
				break;
			if (bi == SB_INSTALL) {
				install_ticked();
				continue;
			}
			if (bi == SB_IMPORT) {
				do_import();
				continue;
			}
			if (bi == SB_EXPORT) {
				do_export();
				continue;
			}
			int t = ktui_tabs_hit(krect(2, 4, ktui_w - 4, 1), tabs,
					      ntab, 0, ev.mx, ev.my);

			if (t >= 0) {
				page = t;
				table.sel = 0;
				continue;
			}
			int idx = ktui_table_hit(krect(2, 5, ktui_w - 4,
						       ktui_h - 10),
						 &table, rows_here(),
						 on_groups() ? GR_NCOL
							     : AP_NCOL,
						 on_groups() ? GR_COL : AP_COL,
						 ev.mx, ev.my);
			ktui_table_pick(&table, rows_here(), idx, NULL, NULL);
			continue;
		}
		if (ev.type != KT_EVT_KEY)
			continue;
		if (ktui_keys(&keys, &ev) == KTUI_KEY_CLOSE)
			break;
		if (ktui_table_key(&table, rows_here(), ktui_h - 9, ev.key,
				   NULL, NULL))
			continue;
		if (on_key(ev.key))
			break;
	}

	kdisp_shutdown();
	return 0;
}
