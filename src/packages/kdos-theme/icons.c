/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   kdos-theme — the icon theme
 *
 * Lays out the KDOS icon theme from the vendored Papirus artwork in art/.
 * No rasteriser and no SVG parser: Papirus is flat single-fill artwork, so a
 * palette remap is a substitution over the text of the file.
 *
 * Symlinks are Papirus's alias mechanism and are recreated as symlinks; only
 * real files are read and rewritten.
 * ---------------------------------
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "kdos-theme.h"

static const struct {
	const char *dir;
	const char *context;
} CONTEXTS[] = {
	{ "actions", "Actions" },
	{ "categories", "Categories" },
	{ "devices", "Devices" },
	{ "emblems", "Emblems" },
	{ "mimetypes", "MimeTypes" },
	{ "panel", "Status" },
	{ "places", "Places" },
	{ "status", "Status" },
};
#define NCTX ((int)(sizeof(CONTEXTS) / sizeof(CONTEXTS[0])))

static const char *context_of(const char *dir)
{
	for (int i = 0; i < NCTX; i++)
		if (!strcmp(CONTEXTS[i].dir, dir))
			return CONTEXTS[i].context;
	return NULL;
}

/*
 * Papirus's apps/ is deliberately not vendored (see vendor.py), so without
 * this the theme has no Applications context at all and every app icon on the
 * desktop — including COSMIC's own — comes from Cosmic or hicolor untinted,
 * which is what "the KDOS icon theme is not being used" looks like.
 *
 *   Cosmic  <size>/apps   COSMIC's stock application icons
 *   hicolor <size>/apps   but ONLY com.system76. — 06_packaging installs the
 *                         ALIEN apps' icons into that same directory, and a
 *                         phosphor Firefox logo is vandalism, not theming.
 *
 * Swept across EVERY size directory, not just scalable/: COSMIC's own app
 * icons are SVGs filed under fixed sizes (com.system76.CosmicFiles lives at
 * 24x24, 128x128 and 256x256 and nowhere else), which is why the dock's Files
 * and Settings buttons stayed stock when only scalable/ was read. The largest
 * variant of each name wins and is written to scalable/apps, since they are
 * all SVG anyway.
 */
static const struct {
	const char *root;
	const char *prefix;
} APP_SOURCES[] = {
	{ "/usr/share/icons/Cosmic", NULL },
	{ "/usr/share/icons/hicolor", "com.system76." },
};

/*
 * KDOS's own marks, which are not in the vendored artwork. Kept here rather
 * than in the kpkgbuild so that `kdos theme <accent>` — which re-runs this
 * against $HOME — produces a complete theme on its own.
 *
 * The dock's app-library button shows the TARGET's icon
 * (com.system76.CosmicAppLibrary), not the applet's own, so both names have to
 * become the tux.
 */
static const struct {
	const char *stem;
	const char *names[3];
} MARK_SETS[] = {
	{ "tux", { "com.system76.CosmicAppLibrary",
		   "com.system76.CosmicPanelAppButton", NULL } },
	{ "logo", { "distributor-logo-kdos", "start-here", NULL } },
};
#define NMARKSET ((int)(sizeof(MARK_SETS) / sizeof(MARK_SETS[0])))

/* "16x16" -> 16, "scalable" and anything else -> -1. */
static int size_of(const char *name)
{
	if (name[0] < '0' || name[0] > '9')
		return -1;
	int v = 0;
	const char *p = name;
	for (; *p >= '0' && *p <= '9'; p++)
		v = v * 10 + (*p - '0');
	return v;
}

/* ──────────────────────────────────────────────────────────────────────── */

static void retint_file(const char *src, const char *dst, const KcolScheme *sc)
{
	size_t len = 0;
	char *data = kb_read_all(src, &len);
	if (!data)
		return;
	size_t out = 0;
	char *tinted = kcol_retint_text(data, len, sc, &out);
	kb_write_all(dst, tinted, out);
	free(data);
	free(tinted);
}

static void recolor_tree(const char *art, const char *out, const KcolScheme *sc,
			 int *nfiles, int *nlinks)
{
	char **sizes = kb_listdir(art, NULL);
	for (char **sz = sizes; sz && *sz; sz++) {
		char *sd = kb_path_join(art, *sz);
		if (!kb_is_dir(sd)) {
			free(sd);
			continue;
		}
		char **ctxs = kb_listdir(sd, NULL);
		for (char **cx = ctxs; cx && *cx; cx++) {
			if (!context_of(*cx))
				continue;
			char *sc_dir = kb_path_join(sd, *cx);
			char *o1 = kb_path_join(out, *sz);
			char *od = kb_path_join(o1, *cx);
			kb_mkdir_p(od);

			char **files = kb_listdir(sc_dir, NULL);
			for (char **f = files; f && *f; f++) {
				char *s = kb_path_join(sc_dir, *f);
				char *d = kb_path_join(od, *f);
				if (kb_is_link(s)) {
					char target[512];
					ssize_t n = readlink(s, target,
							     sizeof(target) - 1);
					if (n > 0) {
						target[n] = 0;
						unlink(d);
						if (symlink(target, d) == 0)
							(*nlinks)++;
					}
				} else {
					retint_file(s, d, sc);
					(*nfiles)++;
				}
				free(s);
				free(d);
			}
			kb_strv_free(files);
			free(sc_dir);
			free(o1);
			free(od);
		}
		kb_strv_free(ctxs);
		free(sd);
	}
	kb_strv_free(sizes);
}

/* ──────────────────────────────────────────────────────────────────────── */

#define MAX_APPS 512

typedef struct {
	char name[128];
	char src[512];
	int rank;
} AppIcon;

static AppIcon apps[MAX_APPS];
static int napps;

static void app_offer(const char *name, const char *src, int rank)
{
	for (int i = 0; i < napps; i++) {
		if (strcmp(apps[i].name, name))
			continue;
		if (apps[i].rank < rank) {
			apps[i].rank = rank;
			kb_strlcpy(apps[i].src, src, sizeof(apps[i].src));
		}
		return;
	}
	if (napps >= MAX_APPS)
		return;
	kb_strlcpy(apps[napps].name, name, sizeof(apps[0].name));
	kb_strlcpy(apps[napps].src, src, sizeof(apps[0].src));
	apps[napps].rank = rank;
	napps++;
}

static int ends_with(const char *s, const char *suffix)
{
	size_t n = strlen(s), m = strlen(suffix);
	return n >= m && !strcmp(s + n - m, suffix);
}

static int recolor_apps(const char *out, const KcolScheme *sc)
{
	napps = 0;

	for (size_t r = 0; r < sizeof(APP_SOURCES) / sizeof(APP_SOURCES[0]); r++) {
		const char *root = APP_SOURCES[r].root;
		const char *prefix = APP_SOURCES[r].prefix;
		if (!kb_is_dir(root))
			continue;

		char **sizes = kb_listdir(root, NULL);
		for (char **sz = sizes; sz && *sz; sz++) {
			char *s1 = kb_path_join(root, *sz);
			char *d = kb_path_join(s1, "apps");
			free(s1);
			if (!kb_is_dir(d)) {
				free(d);
				continue;
			}
			int rank;
			if (!strcmp(*sz, "scalable")) {
				rank = 1 << 20;
			} else {
				rank = size_of(*sz);
				if (rank < 0) {
					free(d);
					continue;
				}
			}
			char **names = kb_listdir(d, NULL);
			for (char **n = names; n && *n; n++) {
				if (!ends_with(*n, ".svg") ||
				    ends_with(*n, "-symbolic.svg"))
					continue;
				if (prefix && strncmp(*n, prefix, strlen(prefix)))
					continue;
				char *src = kb_path_join(d, *n);
				app_offer(*n, src, rank);
				free(src);
			}
			kb_strv_free(names);
			free(d);
		}
		kb_strv_free(sizes);
	}

	char *o1 = kb_path_join(out, "scalable");
	char *od = kb_path_join(o1, "apps");
	kb_mkdir_p(od);
	for (int i = 0; i < napps; i++) {
		char *dst = kb_path_join(od, apps[i].name);
		retint_file(apps[i].src, dst, sc);
		free(dst);
	}
	free(o1);
	free(od);
	return napps;
}

/* ──────────────────────────────────────────────────────────────────────── */

static int install_marks(const char *marks, const char *out)
{
	int n = 0;
	if (!kb_is_dir(marks))
		return 0;

	for (int m = 0; m < NMARKSET; m++) {
		const char *stem = MARK_SETS[m].stem;
		size_t slen = strlen(stem);
		char **files = kb_listdir(marks, NULL);
		for (char **f = files; f && *f; f++) {
			if (strncmp(*f, stem, slen) || (*f)[slen] != '-' ||
			    !ends_with(*f, ".png"))
				continue;
			char size[16];
			size_t taillen = strlen(*f) - slen - 1 - 4;
			if (taillen == 0 || taillen >= sizeof(size))
				continue;
			memcpy(size, *f + slen + 1, taillen);
			size[taillen] = 0;
			int ok = 1;
			for (size_t k = 0; k < taillen; k++)
				if (size[k] < '0' || size[k] > '9')
					ok = 0;
			if (!ok)
				continue;

			char dir[64];
			snprintf(dir, sizeof(dir), "%sx%s", size, size);
			char *o1 = kb_path_join(out, dir);
			char *od = kb_path_join(o1, "apps");
			kb_mkdir_p(od);
			char *src = kb_path_join(marks, *f);
			for (int i = 0; MARK_SETS[m].names[i]; i++) {
				char leaf[160];
				snprintf(leaf, sizeof(leaf), "%s.png",
					 MARK_SETS[m].names[i]);
				char *dst = kb_path_join(od, leaf);
				if (kb_copy_file(src, dst) == 0)
					n++;
				free(dst);
			}
			free(src);
			free(o1);
			free(od);
		}
		kb_strv_free(files);
	}

	/* recolor_apps just wrote COSMIC's own SVG under these names, and a
	 * scalable icon beats a fixed-size PNG at any requested size — so the
	 * stock grid button would win over the tux. Drop them. */
	char *o1 = kb_path_join(out, "scalable");
	char *od = kb_path_join(o1, "apps");
	for (int m = 0; m < NMARKSET; m++) {
		for (int i = 0; MARK_SETS[m].names[i]; i++) {
			char leaf[160];
			snprintf(leaf, sizeof(leaf), "%s.svg",
				 MARK_SETS[m].names[i]);
			char *p = kb_path_join(od, leaf);
			unlink(p);
			free(p);
		}
	}
	free(o1);
	free(od);
	return n;
}

/* ──────────────────────────────────────────────────────────────────────── */

/*
 * Directories are read back off the GENERATED tree rather than tracked by
 * hand: the marks alone create eight <size>/apps directories, and an icon in
 * a directory index.theme does not list is invisible.
 */
static int write_index(const char *out)
{
	KbBuf dirs = {0}, body = {0}, full = {0};
	int n = 0;

	char **entries = kb_listdir(out, NULL);
	for (char **e = entries; e && *e; e++) {
		char *ed = kb_path_join(out, *e);
		if (!kb_is_dir(ed)) {
			free(ed);
			continue;
		}
		int size = strcmp(*e, "scalable") ? size_of(*e) : 0;
		if (size < 0) {
			free(ed);
			continue;
		}
		char **ctxs = kb_listdir(ed, NULL);
		for (char **c = ctxs; c && *c; c++) {
			char *cd = kb_path_join(ed, *c);
			int isdir = kb_is_dir(cd);
			free(cd);
			if (!isdir)
				continue;

			const char *ctx = !strcmp(*c, "apps") ? "Applications"
							      : context_of(*c);
			if (!ctx)
				ctx = "Actions";

			if (n)
				kb_buf_str(&dirs, ",");
			kb_buf_printf(&dirs, "%s/%s", *e, *c);

			kb_buf_printf(&body, "[%s/%s]\nContext=%s\n", *e, *c, ctx);
			if (size)
				kb_buf_printf(&body, "Size=%d\nType=Fixed\n\n",
					      size);
			else
				kb_buf_str(&body,
					   "Size=64\nMinSize=8\nMaxSize=512\n"
					   "Type=Scalable\n\n");
			n++;
		}
		kb_strv_free(ctxs);
		free(ed);
	}
	kb_strv_free(entries);

	kb_buf_str(&full,
		   "[Icon Theme]\n"
		   "Name=KDOS\n"
		   "Comment=KDOS phosphor icon theme\n"
		   /* Cosmic still supplies the symbolic set the shell tints
		    * itself; hicolor supplies the alien apps' icons, installed
		    * there by 06_packaging/01_appbox.sh. */
		   "Inherits=Cosmic,Pop,hicolor\n"
		   "Directories=");
	kb_buf_add(&full, dirs.p ? dirs.p : "", dirs.n);
	kb_buf_str(&full, "\n\n");
	kb_buf_add(&full, body.p ? body.p : "", body.n);

	/* Blocks are separated by a blank line, but the file ends with exactly
	 * one newline — the line list was joined with "\n" and terminated by a
	 * single empty string, so the last block contributes no blank line. */
	while (full.n && full.p[full.n - 1] == '\n')
		full.n--;
	full.p[full.n] = 0;
	kb_buf_str(&full, "\n");

	char *idx = kb_path_join(out, "index.theme");
	kb_write_all(idx, full.p, full.n);
	free(idx);

	kb_buf_free(&dirs);
	kb_buf_free(&body);
	kb_buf_free(&full);
	return n;
}

int gen_icons(const char *art, const char *marks, const char *out,
	      const KcolScheme *sc)
{
	int files = 0, links = 0;

	if (!kb_is_dir(art))
		kb_die("no icon artwork at %s", art);

	kb_rmtree(out);
	kb_mkdir_p(out);
	recolor_tree(art, out, sc, &files, &links);

	int napp = recolor_apps(out, sc);
	int nmark = install_marks(marks, out);	/* after apps: the tux wins */
	int ndirs = write_index(out);

	printf("kdos-icons: %d icons, %d aliases, %d apps, %d marks, "
	       "%d directories -> %s\n", files, links, napp, nmark, ndirs, out);
	return 0;
}
