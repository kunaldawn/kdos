/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   kdos-openwith — open with…, and always use this
 *
 *   ┌ Open with ───────────────────────────────────┐
 *   │ report.odt   ·   application/vnd.oasis...    │
 *   ├──────────────────────────────────────────────┤
 *   │▸LibreOffice Writer            [box] default  │
 *   │ Text Editor                                  │
 *   │ Other command…                               │
 *   ├──────────────────────────────────────────────┤
 *   │ [x] Always use this application              │
 *   │ Enter open  d default  Esc  [ Cancel ][ Open ]│
 *   └──────────────────────────────────────────────┘
 *
 * On the distro whose `kdos-appbox open` owns the whole resolution chain,
 * changing what opens a .pdf meant hand-editing mimeapps.list. This is the
 * surface for that, and it is deliberately the SAME chain read backwards: the
 * candidates come from the files that decide, so a handler this list offers is
 * a handler `kdos-appbox open` will actually pick once it is written down.
 *
 * THE GLOB RULE IS MIRRORED, NOT REINVENTED. `open.c` in kdos-appbox resolves a
 * path with /usr/share/mime/globs and the LONGEST matching suffix wins — get
 * that backwards and every .tar.gz opens in a decompressor. That function is
 * copied here rather than shared, for the reason pick.c copies asciicmd.c's
 * parser: kdos-appbox is a different binary and kdos-shell cannot link it. If
 * one of the two ever changes, they both do.
 *
 * FIELD CODES ARE SUBSTITUTED, NEVER STRIPPED. `%f` IS the file; dropping it
 * opens the application with an empty document, which looks exactly like the
 * chooser having done nothing. sh_strip_field_codes() is the LAUNCHER's rule
 * (there is no file), and using it here would be the bug.
 * ---------------------------------
 */

#include <dirent.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/wait.h>
#include <unistd.h>

#include "kbase.h"
#include "kwl.h"
#include "kxdg.h"
#include "shell.h"

#define OW_MAX_CANDS 64
#define OW_MIME_MAX 128

struct ow_cand {
	char id[160];		/* the desktop file id, with .desktop      */
	char name[96];
	char exec[256];
	int terminal;
	int alien;		/* launched through kdos-appbox            */
	int is_default;		/* what mimeapps.list already says         */
};

static struct ow_cand cands[OW_MAX_CANDS];
static int ncands;
/* The viewport follows the SELECTION only when the selection is what moved —
 * see kch_list_wheel. A clamp that followed unconditionally would undo a page
 * scroll on the very next frame. */
static int sel, top, sel_follow = 1;

static char ow_path[1024];	/* the file, or "" for --mime               */
static char ow_mime[OW_MIME_MAX];
static int set_default;		/* the "always use this" checkbox           */
static char note[160];

/* The "Other command…" row is the last one and is not a candidate. */
static int nrows(void) { return ncands + 1; }
static int row_is_other(int i) { return i == ncands; }

/* Editing state for that row, the same one-line editor pick.c uses. */
static int editing;
static char edit_buf[512];

/* ── path -> MIME ──────────────────────────────────────────────────────── */

/*
 * libkxdg owns this. It is the same table `kdos-appbox open` resolves against
 * and the same longest-suffix rule, so the chooser and the opener cannot
 * disagree about what a file is — and the glob table's path is a compile-time
 * define there, which is what lets a fixture stand in for the compiled
 * database that exists only on a booted target.
 */
static void mime_for_path(const char *path, char *out, size_t n)
{
	kxdg_mime_for_path(path, out, n);
}

/* ── the search path ───────────────────────────────────────────────────── */

static int ow_data_dirs(char out[][512], int max)
{
	const char *home = getenv("XDG_DATA_HOME");
	const char *dirs = getenv("XDG_DATA_DIRS");
	int n = 0;

	if (home && *home)
		snprintf(out[n++], 512, "%s", home);
	else
		snprintf(out[n++], 512, "%.480s/.local/share", kb_home_dir());

	if (!dirs || !*dirs)
		dirs = "/usr/local/share:/usr/share";
	for (const char *p = dirs; *p && n < max;) {
		const char *sep = strchr(p, ':');
		size_t len = sep ? (size_t)(sep - p) : strlen(p);
		while (len > 1 && p[len - 1] == '/')
			len--;
		if (len && len < 512) {
			memcpy(out[n], p, len);
			out[n][len] = '\0';
			n++;
		}
		if (!sep)
			break;
		p = sep + 1;
	}
	return n;
}

static int desktop_find(const char *id, char *out, size_t n)
{
	char dirs[16][512];
	int nd = ow_data_dirs(dirs, 16);

	if (!id || !*id || strchr(id, '/'))
		return 0;
	for (int i = 0; i < nd; i++) {
		snprintf(out, n, "%.400s/applications/%.150s", dirs[i], id);
		if (kb_path_exists(out))
			return 1;
	}
	return 0;
}

static void mimeapps_path(char *out, size_t n)
{
	const char *cfg = getenv("XDG_CONFIG_HOME");

	if (cfg && *cfg)
		snprintf(out, n, "%.500s/mimeapps.list", cfg);
	else
		snprintf(out, n, "%.500s/.config/mimeapps.list", kb_home_dir());
}

/* ── candidates ────────────────────────────────────────────────────────── */

static struct ow_cand *cand_find(const char *id)
{
	for (int i = 0; i < ncands; i++)
		if (!strcmp(cands[i].id, id))
			return &cands[i];
	return NULL;
}

/*
 * One candidate, by desktop id. An entry that is Hidden or has no Exec cannot
 * open anything and is dropped; NoDisplay is dropped too — it means "not an
 * application the user chooses", which is exactly what this list is — EXCEPT
 * when it is already the recorded default, because a chooser that cannot show
 * you what is currently in force cannot be used to change it.
 */
static void cand_add(const char *id, int is_default)
{
	char path[700];
	KxdgEntry e;

	if (ncands >= OW_MAX_CANDS || !id || !*id)
		return;
	struct ow_cand *have = cand_find(id);
	if (have) {
		have->is_default |= is_default;
		return;
	}
	if (!desktop_find(id, path, sizeof(path)))
		return;
	if (kxdg_load(&e, path, "Desktop Entry") != 0)
		return;

	const char *exec = kxdg_get(&e, "Exec", NULL);
	const char *name = kxdg_get(&e, "Name", NULL);
	if (!exec || !*exec || kxdg_bool(&e, "Hidden", 0) ||
	    (!is_default && kxdg_bool(&e, "NoDisplay", 0))) {
		kxdg_free(&e);
		return;
	}

	struct ow_cand *c = &cands[ncands++];
	memset(c, 0, sizeof(*c));
	snprintf(c->id, sizeof(c->id), "%s", id);
	snprintf(c->name, sizeof(c->name), "%s", name && *name ? name : id);
	snprintf(c->exec, sizeof(c->exec), "%s", exec);
	c->terminal = kxdg_bool(&e, "Terminal", 0);
	/* The launcher's rule: an entry whose Exec IS the box launcher is a box
	 * app whatever the alien-apps table is keyed by. */
	c->alien = !strncmp(c->exec, "kdos-appbox run ", 16) ||
		   strstr(c->exec, "/kdos-appbox run ") != NULL;
	c->is_default = is_default;
	kxdg_free(&e);
}

/* A `;`-separated preference list, in order. */
static void cand_add_list(const char *list, int is_default)
{
	char buf[2048];

	if (!list)
		return;
	kb_strlcpy(buf, list, sizeof(buf));
	for (char *p = strtok(buf, ";"); p; p = strtok(NULL, ";")) {
		while (*p == ' ')
			p++;
		if (!*p)
			continue;
		cand_add(p, is_default);
		/* Only the FIRST of a Default Applications list is the default:
		 * the rest are the fallbacks that kick in when it is missing. */
		is_default = 0;
	}
}

static void add_from_section(const char *path, const char *section,
			     const char *mime, int is_default)
{
	KxdgEntry e;

	if (kxdg_load(&e, path, section) != 0)
		return;
	cand_add_list(kxdg_get(&e, mime, NULL), is_default);
	kxdg_free(&e);
}

/*
 * Every entry in the tree whose MimeType claims the type. This is the pass that
 * makes the chooser a CHOOSER rather than a list of what is already wired up —
 * without it, "open this .odt in GIMP instead" has nothing to pick from.
 * A directory scan per data dir, which is what kdos-menu already does per
 * launch, and this runs once.
 */
static int mimetype_claims(const char *list, const char *mime)
{
	size_t n = strlen(mime);

	for (const char *p = list; p && *p;) {
		const char *end = strchr(p, ';');
		size_t len = end ? (size_t)(end - p) : strlen(p);
		while (len && (p[len - 1] == ' ' || p[len - 1] == '\t'))
			len--;
		if (len == n && !strncasecmp(p, mime, n))
			return 1;
		if (!end)
			break;
		p = end + 1;
	}
	return 0;
}

static void scan_for_mime(const char *dir, const char *mime)
{
	DIR *d = opendir(dir);
	struct dirent *de;

	if (!d)
		return;
	while ((de = readdir(d)) && ncands < OW_MAX_CANDS) {
		char path[1200];
		KxdgEntry e;
		size_t len = strlen(de->d_name);
		if (len < 9 || strcmp(de->d_name + len - 8, ".desktop"))
			continue;
		snprintf(path, sizeof(path), "%.600s/%.500s", dir, de->d_name);
		if (kxdg_load(&e, path, "Desktop Entry") != 0)
			continue;
		const char *mt = kxdg_get(&e, "MimeType", NULL);
		int hit = mt && mimetype_claims(mt, mime);
		kxdg_free(&e);
		if (hit)
			cand_add(de->d_name, 0);
	}
	closedir(d);
}

static void gather(const char *mime)
{
	char dirs[16][512], path[700];
	int nd = ow_data_dirs(dirs, 16);

	/* In the order `kdos-appbox open` consults them, so the first row is
	 * the handler that would open the file right now. */
	mimeapps_path(path, sizeof(path));
	add_from_section(path, "Default Applications", mime, 1);
	add_from_section(path, "Added Associations", mime, 0);
	add_from_section("/etc/xdg/mimeapps.list", "Default Applications", mime,
			 ncands == 0);
	for (int i = 0; i < nd; i++) {
		snprintf(path, sizeof(path), "%.500s/applications/mimeinfo.cache",
			 dirs[i]);
		add_from_section(path, "MIME Cache", mime, 0);
	}
	for (int i = 0; i < nd; i++) {
		snprintf(path, sizeof(path), "%.500s/applications", dirs[i]);
		scan_for_mime(path, mime);
	}
	for (int i = 0; i < ncands; i++)
		if (cands[i].is_default)
			sel = i;
}

/* ── writing the default ───────────────────────────────────────────────── */

/*
 * `[Default Applications]` in ~/.config/mimeapps.list, atomically, preserving
 * every other section, comment and blank line.
 *
 * That preservation is the whole job: the file also carries the user's
 * [Added Associations] and whatever a boxed application wrote into it, and a
 * chooser that rewrote it wholesale would silently drop associations nobody
 * asked it to touch. Temp + fsync + rename, because a half-written
 * mimeapps.list is a machine where nothing opens anything.
 */
static int write_default(const char *mime, const char *id)
{
	char path[700], tmp[720], dir[700];
	char *old;
	KbBuf out = {0};
	int in_sec = 0, wrote = 0, have_sec = 0, rc = -1;

	mimeapps_path(path, sizeof(path));
	kb_strlcpy(dir, path, sizeof(dir));
	char *slash = strrchr(dir, '/');
	if (slash) {
		*slash = '\0';
		kb_mkdir_p(dir);
	}

	old = kb_read_all(path, NULL);
	/* A file that EXISTS and did not read is not an empty one — kb_read_all
	 * answers NULL to ENOENT and to EACCES alike, and preserving every other
	 * section means nothing if an unreadable one is replaced wholesale. */
	if (!old && access(path, F_OK) == 0)
		return -1;
	for (char *line = old, *next; line && *line; line = next) {
		char *nl = strchr(line, '\n');
		next = nl ? nl + 1 : line + strlen(line);
		size_t n = (size_t)(next - line);

		if (line[0] == '[') {
			/* Leaving our section without having written the key:
			 * it goes in at the end of it, before the next header. */
			if (in_sec && !wrote) {
				kb_buf_printf(&out, "%s=%s\n", mime, id);
				wrote = 1;
			}
			in_sec = !strncmp(line, "[Default Applications]", 22);
			have_sec |= in_sec;
			kb_buf_add(&out, line, n);
			continue;
		}
		if (in_sec) {
			const char *t = line;
			size_t ml = strlen(mime);
			while (*t == ' ' || *t == '\t')
				t++;
			if (!strncmp(t, mime, ml)) {
				const char *e = t + ml;
				while (*e == ' ' || *e == '\t')
					e++;
				if (*e == '=') {
					if (!wrote) {
						kb_buf_printf(&out, "%s=%s\n",
							      mime, id);
						wrote = 1;
					}
					continue;	/* the old line goes */
				}
			}
		}
		kb_buf_add(&out, line, n);
	}
	if (in_sec && !wrote) {
		/* A file whose last line has no newline would otherwise get the
		 * key glued onto the end of it. */
		if (out.n && out.p[out.n - 1] != '\n')
			kb_buf_add(&out, "\n", 1);
		kb_buf_printf(&out, "%s=%s\n", mime, id);
		wrote = 1;
	}
	if (!have_sec) {
		if (out.n && out.p[out.n - 1] != '\n')
			kb_buf_add(&out, "\n", 1);
		kb_buf_printf(&out, "[Default Applications]\n%s=%s\n", mime, id);
	}

	snprintf(tmp, sizeof(tmp), "%s.tmp", path);
	FILE *f = fopen(tmp, "w");
	if (f) {
		int ok = out.n == 0 || fwrite(out.p, 1, out.n, f) == out.n;
		if (ok && fflush(f) == 0 && fsync(fileno(f)) == 0 &&
		    fclose(f) == 0) {
			f = NULL;
			if (rename(tmp, path) == 0) {
				rc = 0;
				/* The rename is what makes it visible, and it
				 * is the directory entry that has to be on
				 * disk — the step people leave out. */
				if (slash) {
					int d = open(dir, O_RDONLY);
					if (d >= 0) {
						fsync(d);
						close(d);
					}
				}
			}
		}
		if (f)
			fclose(f);
		if (rc != 0)
			remove(tmp);
	}
	kb_buf_free(&out);
	free(old);
	return rc;
}

/* ── launching ─────────────────────────────────────────────────────────── */

/*
 * The Exec line, with the field codes SUBSTITUTED — open.c's rule, and the
 * difference between a chooser and a launcher. %f/%u take the path, %F/%U the
 * same one path (this chooser opens one thing), %% is a literal percent, and
 * %i/%c/%k are dropped: an icon, a name and the entry's own path, none of
 * which a caller has to supply.
 */
static int build_argv(const char *exec, int terminal, const char *file,
		      char *buf, size_t bufsz, const char **argv, int max)
{
	int n = 0;

	kb_strlcpy(buf, exec, bufsz);
	if (terminal && n + 2 < max) {
		argv[n++] = "foot";
		argv[n++] = "-e";
	}
	for (char *w = strtok(buf, " \t"); w && n < max - 1;
	     w = strtok(NULL, " \t")) {
		if (w[0] != '%' || !w[1] || w[2]) {
			argv[n++] = w;
			continue;
		}
		switch (w[1]) {
		case 'f':
		case 'u':
		case 'F':
		case 'U':
			if (file && *file)
				argv[n++] = file;
			break;
		case '%':
			argv[n++] = "%";
			break;
		default:
			break;
		}
	}
	argv[n] = NULL;
	return n;
}

/*
 * Double fork: the opened program is init's child, not ours — this process is
 * about to exit and would otherwise orphan it into whatever reaps the chooser.
 */
static void spawn_argv(const char **argv)
{
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

/* Returns 0 when something was launched. */
static int open_with(const struct ow_cand *c)
{
	char buf[512];
	const char *argv[64];

	if (!build_argv(c->exec, c->terminal, ow_path[0] ? ow_path : NULL, buf,
			sizeof(buf), argv, 64)) {
		snprintf(note, sizeof(note), "%.60s has an empty Exec line",
			 c->name);
		return -1;
	}
	if (set_default && write_default(ow_mime, c->id) != 0)
		snprintf(note, sizeof(note), "could not write mimeapps.list");
	spawn_argv(argv);
	return 0;
}

/* The "Other command…" row: what was typed, plus the path. No shell — the
 * words are split here and exec'd, so a command with a `;` in it is one
 * program with a funny argument rather than two programs. */
static int open_command(const char *cmd)
{
	char buf[512];
	const char *argv[64];
	int n = 0;

	kb_strlcpy(buf, cmd, sizeof(buf));
	for (char *w = strtok(buf, " \t"); w && n < 62; w = strtok(NULL, " \t"))
		argv[n++] = w;
	if (!n) {
		snprintf(note, sizeof(note), "nothing to run");
		return -1;
	}
	/* A bare command line cannot become a default handler: mimeapps.list
	 * records a desktop ENTRY, and inventing one here would put a file in
	 * the user's applications directory they never asked for. */
	if (set_default)
		fprintf(stderr, "kdos-openwith: a plain command cannot be made "
				"the default for %s — write a .desktop entry\n",
			ow_mime);
	if (ow_path[0])
		argv[n++] = ow_path;
	argv[n] = NULL;
	spawn_argv(argv);
	return 0;
}

/* ── drawing ───────────────────────────────────────────────────────────── */

static int btn_ok_x, btn_ok_end, btn_cancel_x, btn_cancel_end, btn_row;
static int check_row;

static void draw(void)
{
	int w = ktui_w, h = ktui_h;
	int list_top = 3;
	int list_rows = h - list_top - 3;

	if (list_rows < 1)
		list_rows = 1;

	ktui_draw_fill(krect(0, 0, w, h), KT_SURFACE);
	sh_frame(w, h, " Open with ", KT_ACCENT, KT_SURFACE, 0);

	/* What is being opened, and what it was decided to be. The path is
	 * elided from the LEFT — the end of it is the part that identifies it. */
	char head[1200];
	if (ow_path[0])
		snprintf(head, sizeof(head), "%s   %s   %s", ow_path,
			 ktui_glyph[KT_G_BULLET], ow_mime);
	else
		snprintf(head, sizeof(head), "%s", ow_mime);
	int hw = w - 4;
	if (hw < 1)
		hw = 1;
	const char *shown = head;
	if ((int)strlen(head) > hw)
		shown = head + strlen(head) - (size_t)hw;
	ktui_draw_text(2, 1, hw, shown, KT_MID, KT_SURFACE, KT_A_NONE);
	ktui_draw_hline(1, 2, w - 2, KT_G_HL, KT_DIM, KT_SURFACE);

	for (int i = 0; i < list_rows; i++) {
		int idx = top + i;
		if (idx >= nrows())
			break;
		int on = idx == sel;
		int fg = on ? KT_SURFACE : KT_TEXT;
		int bg = on ? KT_ACCENT : KT_SURFACE;
		int y = list_top + i;

		ktui_draw_fill(krect(1, y, w - 2, 1), bg);
		if (row_is_other(idx)) {
			/* The ellipsis comes from the glyph table: the console
			 * font has one, and a tier without it gets "..." rather
			 * than a blank cell where the promise of a prompt was. */
			char other[32];
			snprintf(other, sizeof(other), "Other command%s",
				 ktui_glyph[KT_G_ELLIPSIS]);
			ktui_draw_text(2, y, w - 4, other, on ? fg : KT_MID, bg,
				       KT_A_NONE);
			continue;
		}
		const struct ow_cand *c = &cands[idx];
		ktui_draw_text(2, y, w - 20, c->name, fg, bg, KT_A_NONE);
		/* Two marks, right to left: `default` is what happens today,
		 * `[box]` is what it costs (a container start on first launch).
		 * Both are facts the person choosing is entitled to. */
		int rx = w - 2;
		if (c->is_default) {
			ktui_draw_text_right(0, y, rx, "default",
					     on ? fg : KT_WARN, bg, KT_A_NONE);
			rx -= 8;
		}
		if (c->alien)
			ktui_draw_text_right(0, y, rx, "[box]", on ? fg : KT_DIM,
					     bg, KT_A_NONE);
	}

	/*
	 * ONE COLUMN THAT SAYS THERE IS MORE, on the frame's own right edge —
	 * see kch_scrollbar. It matters more since the wheel started
	 * moving the PAGE rather than the cursor: without it the content
	 * slides for no visible reason.
	 */
	kch_scrollbar(0, w - 1, list_top, list_rows, nrows(), top,
			  KT_SURFACE);
	/* On `!ncands`, never `!nrows()`: the Other row is always there, so the
	 * list is never empty and this said nothing on the one machine that
	 * needed it — a type outside the appbox's mimeinfo.cache showed a lone
	 * "Other command…" and no reason for it. It goes UNDER that row. */
	if (!ncands && list_rows > 1)
		ktui_draw_text(2, list_top + 1, w - 4,
			       "nothing on this machine claims this type",
			       KT_DIM, KT_SURFACE, KT_A_NONE);

	check_row = h - 3;
	if (editing) {
		char line[600];
		snprintf(line, sizeof(line), "Command: %s", edit_buf);
		const char *el = line;
		if ((int)strlen(line) > w - 4)
			el = line + strlen(line) - (size_t)(w - 4);
		ktui_draw_text(2, check_row, w - 4, el, KT_TEXT, KT_SURFACE,
			       KT_A_UNDERLINE);
	} else {
		char line[96];
		snprintf(line, sizeof(line), "[%s] Always use this application",
			 set_default ? "x" : " ");
		ktui_draw_text(2, check_row, w - 4, line,
			       set_default ? KT_ACCENT : KT_DIM, KT_SURFACE,
			       KT_A_NONE);
	}

	if (note[0])
		ktui_draw_text(2, h - 2, w - 26, note, KT_WARN, KT_SURFACE,
			       KT_A_NONE);
	else
		ktui_draw_text(2, h - 2, w - 26,
			       editing ? "Enter run   Esc cancel"
				       : "Enter open   d default   Esc cancel",
			       KT_DIM, KT_SURFACE, KT_A_NONE);

	/* The same two buttons pick.c carries, in the same place: this dialog
	 * arrives in front of a person holding a mouse. */
	const char *ok = " Open ";
	int okw = ktui_utf8_width(ok) + 2, cw = 10;
	btn_ok_x = w - 2 - okw;
	btn_ok_end = btn_ok_x + okw;
	btn_cancel_x = btn_ok_x - 1 - cw;
	btn_cancel_end = btn_cancel_x + cw;
	btn_row = h - 2;
	if (btn_cancel_x > 26) {
		ktui_draw_text(btn_cancel_x, btn_row, cw, "[ Cancel ]", KT_TEXT,
			       KT_SURFACE, KT_A_NONE);
		ktui_draw_text(btn_ok_x, btn_row, 1, "[", KT_DIM, KT_SURFACE,
			       KT_A_NONE);
		ktui_draw_text(btn_ok_x + 1, btn_row, okw - 2, ok, KT_SURFACE,
			       KT_ACCENT, KT_A_NONE);
		ktui_draw_text(btn_ok_end - 1, btn_row, 1, "]", KT_DIM,
			       KT_SURFACE, KT_A_NONE);
	} else {
		btn_ok_x = btn_ok_end = btn_cancel_x = btn_cancel_end = 0;
	}
	ktui_draw_flush();
}

/* `--dump-cells` — one line per painted cell, colours included. The
 * backend is cells.c's; these two are what the size flag writes. */
static int cap_w = 64, cap_h = 18;

/* ── main ──────────────────────────────────────────────────────────────── */

/* Enter, the Open button, and a click on an already-selected row. Returns 1
 * when the chooser has answered and should close. */
static int activate(void)
{
	if (sel < 0 || sel >= nrows())
		return 0;
	if (row_is_other(sel)) {
		if (!editing) {
			editing = 1;
			edit_buf[0] = '\0';
			note[0] = '\0';
			return 0;
		}
		return open_command(edit_buf) == 0;
	}
	return open_with(&cands[sel]) == 0;
}

int openwith_main(int argc, char **argv)
{
	const char *font = NULL;
	const char *mime_arg = NULL;
	const char *file_arg = NULL;
	int dump = 0, cells = 0, print = 0;

	for (int i = 1; i < argc; i++) {
		if (!strcmp(argv[i], "--font") && i + 1 < argc)
			font = argv[++i];
		else if (!strcmp(argv[i], "--mime") && i + 1 < argc)
			mime_arg = argv[++i];
		else if (!strcmp(argv[i], "--print"))
			print = 1;
		else if (!strcmp(argv[i], "--dump"))
			dump = 1;
		else if (!strcmp(argv[i], "--dump-cells"))
			cells = 1;
		/* A golden frame is named for its size, so the harness has to
		 * be able to ask for one. */
		else if (!strcmp(argv[i], "--dump-size") && i + 1 < argc) {
			int dw, dh;
			if (sscanf(argv[++i], "%dx%d", &dw, &dh) == 2 &&
			    dw > 23 && dh > 5) {
				cap_w = dw;
				cap_h = dh;
			}
		} else if (argv[i][0] != '-' && !file_arg) {
			file_arg = argv[i];
		} else {
			fprintf(stderr,
				"usage: kdos-openwith <path> | --mime <type>\n"
				"                     [--print] "
				"[--dump|--dump-cells] [--dump-size WxH]\n"
				"                     [--font NAME]\n");
			return 2;
		}
	}

	if (file_arg) {
		/* Absolute, because the handler is spawned with setsid() from a
		 * process that is about to exit and nothing guarantees it keeps
		 * this working directory. */
		if (file_arg[0] == '/') {
			snprintf(ow_path, sizeof(ow_path), "%s", file_arg);
		} else {
			char cwd[512];
			if (getcwd(cwd, sizeof(cwd)))
				snprintf(ow_path, sizeof(ow_path), "%s/%s", cwd,
					 file_arg);
			else
				snprintf(ow_path, sizeof(ow_path), "%s", file_arg);
		}
		mime_for_path(ow_path, ow_mime, sizeof(ow_mime));
	} else if (mime_arg) {
		snprintf(ow_mime, sizeof(ow_mime), "%s", mime_arg);
	} else {
		fprintf(stderr, "kdos-openwith: a path or --mime <type>\n");
		return 2;
	}

	gather(ow_mime);

	if (print) {
		/* Resolves and prints, launching nothing — open.c's --print,
		 * and what a test can assert on. */
		printf("mime\t%s\n", ow_mime);
		for (int i = 0; i < ncands; i++) {
			if (cands[i].is_default)
				printf("default\t%s\n", cands[i].id);
			printf("entry\t%s\t%s\n", cands[i].id, cands[i].name);
		}
		return ncands ? 0 : 1;
	}

	if (dump || cells) {
		sh_theme_from_cache();
		if (cells) {
			ktui_backend_set(sh_cells_backend(cap_w, cap_h));
			ktui_draw_init();
			draw();
			return 0;
		}
		ktui_offscreen_init(cap_w, cap_h);
		draw();
		ktui_draw_dump();
		return 0;
	}

	KwlConfig cfg = {
		/*
		 * ANCHORED MEANS POPUP; CENTRED MEANS A WINDOW — and a window
		 * is an xdg TOPLEVEL, not a layer surface. Layer-shell has no
		 * move and no resize in the protocol at all, so every native
		 * app on this desktop was a rectangle nailed to the screen
		 * while every boxed one could be dragged and pulled about. A
		 * toplevel also gets the compositor's own frame, which is the
		 * other half of it: the decoration then MATCHES an alien app's
		 * because it IS an alien app's.
		 */
		.role = KWL_ROLE_TOPLEVEL,
		.cols = 64,
		.rows = 18,
		/* The SSD shows this: a toplevel with no title gets an
		 * empty titlebar, which is a frame that says nothing. */
		.title = "Open with",
		.app_id = "kdos-openwith",
		.font = font,
		.keyboard = 1,
	};

	sh_theme_from_cache();
	if (kwl_init(&cfg) != 0) {
		fprintf(stderr, "kdos-openwith: no compositor or no layer-shell\n");
		return 2;
	}
	ktui_draw_init();
	/* The bar's own body, so a popup over the taskbar is the
	 * same surface the taskbar is — see kch_px_popup(). */
	kch_px_popup(KT_SURFACE);

	int rc = 1;
	while (!kwl_should_close()) {
		/* Follow a live `kdos theme <accent>`; see sh_theme_poll(). */
		sh_theme_poll();
		int list_rows = ktui_h - 6;
		if (list_rows < 1)
			list_rows = 1;
		kch_list_clamp(&top, sel, nrows(), list_rows, sel_follow);
		sel_follow = 0;

		draw();

		KtuiEvent ev;
		if (!ktui_backend()->poll_event(&ev, 1000)) {
			if (ktui_resized) {
				ktui_resized = 0;
				ktui_draw_resize();
				ktui_draw_invalidate();
			}
			continue;
		}

		/* The shell's one mouse contract: motion selects, left press
		 * activates, wheel scrolls, right press closes. */
		if (ev.type == KT_EVT_MOUSE) {
			int row = ev.my - 3 + top;
			int on_row = ev.my >= 3 && ev.my < ktui_h - 3 &&
				     row >= 0 && row < nrows();
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
				if (on_row && !editing) {
					sel = row;
					sel_follow = 1;
				}
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
				int lr = ktui_h - 6 > 0 ? ktui_h - 6 : 1;
				if (!kch_list_wheel(up, &top, nrows(), lr)) {
					sel += up ? -1 : 1;
					sel_follow = 1;
				}
			} else if (ev.btn == KT_MB_RIGHT) {
				break;
			} else if (ev.btn == KT_MB_LEFT) {
				if (btn_ok_end > btn_ok_x && ev.my == btn_row &&
				    ev.mx >= btn_ok_x && ev.mx < btn_ok_end) {
					if (activate()) {
						rc = 0;
						break;
					}
				} else if (btn_cancel_end > btn_cancel_x &&
					   ev.my == btn_row &&
					   ev.mx >= btn_cancel_x &&
					   ev.mx < btn_cancel_end) {
					break;
				} else if (ev.my == check_row && !editing) {
					set_default = !set_default;
				} else if (on_row && !editing) {
					if (row == sel && activate()) {
						rc = 0;
						break;
					}
					sel = row;
					sel_follow = 1;
				}
			}
			if (sel < 0)
				sel = 0;
			if (sel >= nrows())
				sel = nrows() ? nrows() - 1 : 0;
			continue;
		}
		if (ev.type != KT_EVT_KEY)
			continue;

		/* A key either moves the cursor or refills the list; both want
		 * the viewport to follow. */
		sel_follow = 1;

		if (editing) {
			size_t n = strlen(edit_buf);
			if (ev.key == KT_K_ESC) {
				editing = 0;
			} else if (ev.key == KT_K_ENTER) {
				if (open_command(edit_buf) == 0) {
					rc = 0;
					break;
				}
			} else if (ev.key == KT_K_BACKSPACE) {
				if (n)
					edit_buf[n - 1] = '\0';
			} else if (ev.key >= 0x20 && ev.key < 0x7f &&
				   n + 1 < sizeof(edit_buf)) {
				edit_buf[n] = (char)ev.key;
				edit_buf[n + 1] = '\0';
			}
			continue;
		}

		int page = ktui_h - 6;
		if (page < 1)
			page = 1;

		if (ev.key == KT_K_ESC)
			break;
		if (ev.key == KT_K_ENTER) {
			if (activate()) {
				rc = 0;
				break;
			}
		} else if (ev.key == KT_K_UP) {
			sel--;
		} else if (ev.key == KT_K_DOWN) {
			sel++;
		} else if (ev.key == KT_K_PGUP) {
			sel -= page;
		} else if (ev.key == KT_K_PGDN) {
			sel += page;
		} else if (ev.key == KT_K_HOME) {
			sel = 0;
		} else if (ev.key == KT_K_END) {
			sel = nrows() - 1;
		} else if (ev.key == 'd' || ev.key == ' ') {
			set_default = !set_default;
		}

		if (sel < 0)
			sel = 0;
		if (sel >= nrows())
			sel = nrows() ? nrows() - 1 : 0;
	}

	kwl_shutdown();
	return rc;
}
