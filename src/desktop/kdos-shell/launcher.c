/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   kdos-launcher — Super+D
 *
 *   ╔═ run ════════════════════════════════════╗
 *   ║ gim_                                     ║
 *   ╟──────────────────────────────────────────╢
 *   ║ ▶ GIMP                         [box]     ║
 *   ║   GIMP Script-Fu Console       [box]     ║
 *   ╚══════════════════════════════════════════╝
 *
 * A centred layer-shell overlay drawn as character cells, listing everything
 * on the machine that can be started: ~90 alien apps from the appbox and every
 * native desktop entry, in one list, marked so you can tell which is which but
 * not separated into two places you have to look.
 *
 * IT IS A SEPARATE PROCESS, spawned by Super+D and gone when it exits. That is
 * not laziness about state — a launcher that lives inside the panel process
 * must hold a surface it keeps hidden, and any crash in its filtering takes the
 * panel down with it. Spawning is what the compositor's `spawn` action already
 * does for everything else, and the exit path is `_exit`.
 *
 * Launching goes THROUGH kdos-appbox for alien apps, never around it: the
 * warmup wait, the readiness check, the storage-driver choice and the
 * notification are all in that program, each one a fix for something that broke,
 * and a launcher that exec'd `distrobox enter` directly would reintroduce every
 * one of them.
 */

#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

#include "kwl.h"
#include "shell.h"

#define MAX_ENTRIES 512

struct entry {
	char name[96];		/* what the user reads   */
	char exec[256];		/* argv[0..] as written  */
	char id[96];		/* desktop file id       */
	int alien;		/* lives in a box        */
};

static struct entry entries[MAX_ENTRIES];
static int nentries;
static int order[MAX_ENTRIES];	/* filtered, in match order */
static int nmatch;

/* ── gathering ─────────────────────────────────────────────────────────── */

static int have_id(const char *id)
{
	for (int i = 0; i < nentries; i++)
		if (!strcmp(entries[i].id, id))
			return 1;
	return 0;
}

/*
 * `%f`, `%U` and friends are placeholders for the files an entry was opened
 * with. Launched from here there are none, so they are stripped — passing the
 * literal string `%U` to a program is how you get a browser opening a tab for
 * a file called "%U".
 */
static void strip_field_codes(char *exec)
{
	char *w = exec;
	for (char *r = exec; *r; r++) {
		if (r[0] == '%' && r[1]) {
			r++;		/* skip the code letter too */
			continue;
		}
		*w++ = *r;
	}
	*w = '\0';
	/* A trailing space left by a stripped code is harmless to exec but ugly
	 * in the list, and the list shows the name rather than this anyway. */
	while (w > exec && w[-1] == ' ')
		*--w = '\0';
}

static void add_desktop_dir(const char *dir)
{
	DIR *d = opendir(dir);
	struct dirent *e;
	if (!d)
		return;

	while ((e = readdir(d)) && nentries < MAX_ENTRIES) {
		size_t n = strlen(e->d_name);
		if (n < 9 || strcmp(e->d_name + n - 8, ".desktop"))
			continue;

		char path[1024];
		snprintf(path, sizeof(path), "%s/%s", dir, e->d_name);
		KxdgEntry ent = {0};
		if (kxdg_load(&ent, path, "Desktop Entry") != 0)
			continue;

		const char *type = kxdg_get(&ent, "Type", "");
		const char *name = kxdg_get(&ent, "Name", NULL);
		const char *exec = kxdg_get(&ent, "Exec", NULL);
		if (strcmp(type, "Application") || !name || !exec ||
		    kxdg_bool(&ent, "NoDisplay", 0) ||
		    kxdg_bool(&ent, "Hidden", 0)) {
			kxdg_free(&ent);
			continue;
		}

		struct entry *t = &entries[nentries];
		memset(t, 0, sizeof(*t));
		snprintf(t->id, sizeof(t->id), "%.*s", (int)(n - 8), e->d_name);
		/* The user's own entries are read first and win: a
		 * ~/.local/share entry exists precisely to override the system
		 * one of the same id. */
		if (have_id(t->id)) {
			kxdg_free(&ent);
			continue;
		}
		snprintf(t->name, sizeof(t->name), "%s", name);
		snprintf(t->exec, sizeof(t->exec), "%s", exec);
		strip_field_codes(t->exec);
		/*
		 * An entry whose Exec IS the box launcher is a box app, whatever
		 * the alien-apps table is keyed by. That table's first column is
		 * the SHIM name (`calibre`, `mousepad`) while a desktop id is
		 * upstream's own (`calibre-gui`, `org.xfce.mousepad`), so
		 * matching on the id alone marked [box] on the minority where
		 * the two happen to coincide — Ardour had the tag and calibre,
		 * VSCodium, Mousepad and Foliate did not.
		 */
		if (!strncmp(t->exec, "kdos-appbox run ", 16) ||
		    strstr(t->exec, "/kdos-appbox run "))
			t->alien = 1;
		kxdg_free(&ent);
		nentries++;
	}
	closedir(d);
}

/*
 * /usr/share/kdos/alien-apps is the baked name -> in-box command table, plus
 * the user's runtime additions. An app in there is launched by NAME through
 * kdos-appbox, whatever its desktop entry's Exec says.
 */
static void mark_alien(const char *path)
{
	FILE *f = fopen(path, "r");
	char line[1024];
	if (!f)
		return;
	while (fgets(line, sizeof(line), f)) {
		line[strcspn(line, "\n")] = '\0';
		char *sp = strpbrk(line, " \t");
		if (sp)
			*sp = '\0';
		if (!*line || *line == '#')
			continue;
		for (int i = 0; i < nentries; i++)
			if (!strcmp(entries[i].id, line) ||
			    !strcmp(entries[i].exec, line))
				entries[i].alien = 1;
	}
	fclose(f);
}

static void gather(void)
{
	char path[1024];
	const char *home = getenv("HOME");

	if (home && *home) {
		snprintf(path, sizeof(path), "%s/.local/share/applications", home);
		add_desktop_dir(path);
	}
	add_desktop_dir("/usr/local/share/applications");
	add_desktop_dir("/usr/share/applications");

	mark_alien("/usr/share/kdos/alien-apps");
	if (home && *home) {
		snprintf(path, sizeof(path), "%s/.local/share/kdos/alien-apps",
			 home);
		mark_alien(path);
	}
}

/* ── matching ──────────────────────────────────────────────────────────── */

/*
 * Subsequence match, scored so that a prefix beats a scatter.
 *
 * `gim` should put GIMP first, not "Image Magick" — every character of the
 * query appears in both, and only the position tells them apart. Returns -1
 * for no match, and lower is better.
 */
static int score(const char *hay, const char *needle)
{
	if (!*needle)
		return 0;
	int pos = 0, first = -1, gaps = 0, last = -1;
	for (const char *n = needle; *n; n++) {
		int lc = *n >= 'A' && *n <= 'Z' ? *n + 32 : *n;
		const char *h = hay + pos;
		for (; *h; h++) {
			int hc = *h >= 'A' && *h <= 'Z' ? *h + 32 : *h;
			if (hc == lc)
				break;
		}
		if (!*h)
			return -1;
		int at = (int)(h - hay);
		if (first < 0)
			first = at;
		if (last >= 0 && at != last + 1)
			gaps++;
		last = at;
		pos = at + 1;
	}
	return first * 4 + gaps;
}

static void filter(const char *query)
{
	int scores[MAX_ENTRIES];
	nmatch = 0;
	for (int i = 0; i < nentries; i++) {
		int s = score(entries[i].name, query);
		if (s < 0)
			s = score(entries[i].id, query);
		if (s < 0)
			continue;
		scores[nmatch] = s;
		order[nmatch++] = i;
	}
	/* Insertion sort: nmatch is a few hundred at worst and this runs on a
	 * keystroke, where the constant factor matters more than the order. */
	for (int i = 1; i < nmatch; i++) {
		int si = scores[i], oi = order[i], j = i - 1;
		while (j >= 0 && scores[j] > si) {
			scores[j + 1] = scores[j];
			order[j + 1] = order[j];
			j--;
		}
		scores[j + 1] = si;
		order[j + 1] = oi;
	}
}

/* ── launching ─────────────────────────────────────────────────────────── */

static void launch(const struct entry *e)
{
	char buf[256];
	const char *argv[32];
	int n = 0;

	if (e->alien) {
		/* Through kdos-appbox, never around it — see the header. */
		argv[n++] = "kdos-appbox";
		argv[n++] = "run";
		argv[n++] = e->id;
	} else {
		snprintf(buf, sizeof(buf), "%s", e->exec);
		char *save = NULL;
		for (char *tok = strtok_r(buf, " \t", &save);
		     tok && n < 30; tok = strtok_r(NULL, " \t", &save))
			argv[n++] = tok;
	}
	if (!n)
		return;
	argv[n] = NULL;

	/*
	 * Double fork, so the launched program is init's child and not ours —
	 * this process is about to exit and would otherwise orphan it into
	 * whatever reaps the launcher.
	 */
	pid_t p = fork();
	if (p == 0) {
		if (fork() == 0) {
			setsid();
			execvp(argv[0], (char *const *)argv);
			_exit(127);
		}
		_exit(0);
	}
	if (p > 0) {
		int st;
		waitpid(p, &st, 0);
	}
}

/* ── drawing ───────────────────────────────────────────────────────────── */

static void draw(const char *query, int sel, int top)
{
	int w = ktui_w, h = ktui_h;
	if (w < 10 || h < 3)
		return;

	ktui_draw_fill(krect(0, 0, w, h), KT_SURFACE);
	ktui_draw_box(krect(0, 0, w, h), " run ", KT_ACCENT, KT_SURFACE, 1);

	/* The query line, with a block for a caret — the cell grid has no
	 * hardware cursor to place. */
	char line[256];
	snprintf(line, sizeof(line), "%s%s", query, ktui_glyph[KT_G_FULL]);
	ktui_draw_text(2, 1, w - 4, line, KT_TEXT, KT_SURFACE, KT_A_NONE);
	ktui_draw_hline(1, 2, w - 2, KT_G_HL, KT_DIM, KT_SURFACE);

	int rows = h - 4;
	for (int i = 0; i < rows && top + i < nmatch; i++) {
		const struct entry *e = &entries[order[top + i]];
		int y = 3 + i;
		int is_sel = top + i == sel;
		/*
		 * The colours are ALREADY swapped for the selected row, so it
		 * must not also carry KT_A_REVERSE: the attribute swaps fg and
		 * bg again, and the row came out as accent-on-background text
		 * sitting inside an accent bar — every glyph cell punched back
		 * to the background colour. Swap the slots or set the
		 * attribute, never both.
		 */
		int fg = is_sel ? KT_SURFACE : KT_TEXT;
		int bg = is_sel ? KT_ACCENT : KT_SURFACE;

		if (is_sel)
			ktui_draw_fill(krect(1, y, w - 2, 1), bg);
		ktui_draw_text(2, y, w - 12, e->name, fg, bg, KT_A_NONE);
		/* The mark is the honest part of this list: an alien app costs
		 * a container start on first launch, and the user is entitled
		 * to know which ones those are before clicking. */
		if (e->alien)
			ktui_draw_text_right(0, y, w - 2, "[box]",
					     is_sel ? fg : KT_DIM, bg,
					     KT_A_NONE);
	}

	if (nmatch == 0)
		ktui_draw_text(2, 3, w - 4, "no match", KT_DIM, KT_SURFACE,
			       KT_A_NONE);

	ktui_draw_flush();
}

/* ── main ──────────────────────────────────────────────────────────────── */

int launcher_main(int argc, char **argv)
{
	const char *font = NULL;
	for (int i = 1; i < argc; i++) {
		if (!strcmp(argv[i], "--font") && i + 1 < argc)
			font = argv[++i];
		else {
			fprintf(stderr, "usage: kdos-launcher [--font NAME]\n");
			return 2;
		}
	}

	KwlConfig cfg = {
		.role = KWL_ROLE_OVERLAY,
		.cols = 64,
		.rows = 18,
		.app_id = "kdos-launcher",
		.font = font,
		.keyboard = 1,
		/* A menu, not a dialog: clicking elsewhere closes it. */
		.dismiss_on_unfocus = 1,
	};

	sh_theme_from_cache();
	if (kwl_init(&cfg) != 0) {
		fprintf(stderr, "kdos-launcher: no compositor or no "
				"layer-shell\n");
		return 1;
	}
	ktui_draw_init();

	gather();

	char query[128] = {0};
	int qlen = 0, sel = 0, top = 0;
	filter(query);

	while (!kwl_should_close()) {
		/* Keep the selection on screen and inside the list — every one
		 * of these is reachable by typing until the match set shrinks
		 * under the cursor. */
		if (sel >= nmatch)
			sel = nmatch ? nmatch - 1 : 0;
		if (sel < 0)
			sel = 0;
		int rows = ktui_h - 4;
		if (rows < 1)
			rows = 1;
		if (sel < top)
			top = sel;
		if (sel >= top + rows)
			top = sel - rows + 1;

		draw(query, sel, top);

		KtuiEvent ev;
		if (!ktui_backend()->poll_event(&ev, 1000)) {
			if (ktui_resized) {
				ktui_resized = 0;
				ktui_draw_resize();
				ktui_draw_invalidate();
			}
			continue;
		}
		/*
		 * Full mouse, on the same contract as kdos-menu: hover selects
		 * (plain movement arrives as KT_MP_DRAG — libkwl's spelling),
		 * a left press launches, the wheel scrolls, a right press
		 * closes. A launcher opened by a keystroke is still a list of
		 * things on a screen, and a list you cannot click reads as a
		 * broken one.
		 */
		if (ev.type == KT_EVT_MOUSE) {
			int row = ev.my - 3 + top;
			int on_row = ev.my >= 3 && row >= 0 && row < nmatch;
			if (ev.press == KT_MP_DRAG) {
				if (on_row)
					sel = row;
				continue;
			}
			if (ev.press != KT_MP_PRESS)
				continue;
			if (ev.btn == KT_MB_WHEEL_UP) {
				sel--;
				continue;
			}
			if (ev.btn == KT_MB_WHEEL_DOWN) {
				sel++;
				continue;
			}
			if (ev.btn == KT_MB_RIGHT)
				goto done;
			if (ev.btn != KT_MB_LEFT || !on_row)
				continue;
			launch(&entries[order[row]]);
			goto done;
		}
		if (ev.type != KT_EVT_KEY)
			continue;

		switch (ev.key) {
		case KT_K_ESC:
			goto done;
		case KT_K_ENTER:
			if (sel < nmatch)
				launch(&entries[order[sel]]);
			goto done;
		case KT_K_UP:
			sel--;
			break;
		case KT_K_DOWN:
			sel++;
			break;
		case KT_K_BACKSPACE:
			if (qlen > 0)
				query[--qlen] = '\0';
			filter(query);
			sel = 0;
			top = 0;
			break;
		default:
			/* Printable ASCII only. The match is a byte
			 * subsequence, so accepting multi-byte input here would
			 * compare halves of a codepoint against whole ones. */
			if (ev.key >= 0x20 && ev.key < 0x7f &&
			    qlen < (int)sizeof(query) - 1) {
				query[qlen++] = (char)ev.key;
				query[qlen] = '\0';
				filter(query);
				sel = 0;
				top = 0;
			}
			break;
		}
	}

done:
	kwl_shutdown();
	return 0;
}
