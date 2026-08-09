/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   kdos-theme — the GTK stylesheet
 *
 * Lays out the KDOS GTK theme from the vendored adw-gtk3 stylesheet. Two
 * passes, and the split matters:
 *
 *   1. Every literal colour in the stylesheet is hue-mapped by FAMILY.
 *      Lightness is left alone: adw-gtk3's contrast steps are tuned, and
 *      re-deriving them produces a worse theme than keeping them. This pass is
 *      the safety net — it catches the 45-colour GNOME ramp, the `--blue-3`
 *      style custom properties in :root, and the stray hex in helper classes,
 *      so nothing anywhere stays blue.
 *
 *   2. The ~48 semantic names (window/view/headerbar/sidebar/card/popover/
 *      dialog/accent/destructive/success/warning/error) are then overwritten
 *      outright. These are what a widget actually resolves, so this is where
 *      the KDOS look is decided, and it is deliberately not left to a hue
 *      rotation of somebody else's grey.
 *
 * Fill colours use the DIM accent, never the full-intensity one: a 100%-filled
 * GIMP opacity slider painted #39ff14 is an unreadable neon block.
 * ---------------------------------
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "kdos-theme.h"

#define MAX_NAMES 64

typedef struct {
	const char *name;
	char value[48];
} Named;

static Named names[MAX_NAMES];
static int nnames;

static void put(const char *name, const char *value)
{
	if (nnames >= MAX_NAMES)
		kb_die("colour name table is full");
	names[nnames].name = name;
	kb_strlcpy(names[nnames].value, value, sizeof(names[0].value));
	nnames++;
}

static const char *hx(uint32_t v)
{
	static char buf[8][8];
	static int slot;
	char *b = buf[slot = (slot + 1) & 7];
	b[0] = '#';
	kcol_format(v, b + 1);
	return b;
}

static const char *find_name(const char *name)
{
	for (int i = 0; i < nnames; i++)
		if (!strcmp(names[i].name, name))
			return names[i].value;
	return NULL;
}

/* adw's own surface steps (#1d1d20 view, #222226 window, #2e2e32 headerbar,
 * #36363a popover) are grey and much lighter than KDOS wants, so they are
 * replaced outright rather than shifted. */
static void build_names(const KcolScheme *sc)
{
	uint32_t d = sc->deep, t = sc->text, p = sc->primary;
	uint32_t acc = sc->pdark, sec = sc->secondary, urg = sc->urgent;
	uint32_t white = 0xffffff;

	uint32_t view = d;
	uint32_t window = sc->variant;
	uint32_t header = kcol_mixf(d, t, 0.07);
	uint32_t side_backdrop = kcol_mixf(d, t, 0.04);
	uint32_t dialog = kcol_mixf(d, t, 0.11);
	uint32_t thumb = kcol_mixf(d, t, 0.13);
	uint32_t on_accent = kcol_mixf(white, t, 0.35);

	char card[48];
	snprintf(card, sizeof(card), "alpha(%s, 0.07)", hx(t));

	nnames = 0;
	put("accent_bg_color", hx(acc));
	put("accent_fg_color", hx(on_accent));
	put("accent_color", hx(p));
	put("success_bg_color", hx(acc));
	put("success_fg_color", hx(on_accent));
	put("success_color", hx(p));
	put("warning_bg_color", hx(kcol_mixf(sec, d, 0.25)));
	put("warning_fg_color", hx(d));
	put("warning_color", hx(sec));
	put("destructive_bg_color", hx(kcol_mixf(urg, d, 0.2)));
	put("destructive_fg_color", hx(kcol_mixf(white, urg, 0.15)));
	put("destructive_color", hx(urg));
	put("error_bg_color", hx(kcol_mixf(urg, d, 0.2)));
	put("error_fg_color", hx(kcol_mixf(white, urg, 0.15)));
	put("error_color", hx(urg));

	put("window_bg_color", hx(window));
	put("window_fg_color", hx(t));
	put("view_bg_color", hx(view));
	put("view_fg_color", hx(t));
	put("headerbar_bg_color", hx(header));
	put("headerbar_fg_color", hx(t));
	put("headerbar_border_color", hx(t));
	put("headerbar_backdrop_color", hx(window));
	put("sidebar_bg_color", hx(header));
	put("sidebar_fg_color", hx(t));
	put("sidebar_backdrop_color", hx(side_backdrop));
	put("secondary_sidebar_bg_color", hx(header));
	put("secondary_sidebar_fg_color", hx(t));
	put("secondary_sidebar_backdrop_color", hx(side_backdrop));
	put("card_bg_color", card);
	put("card_fg_color", hx(t));
	put("dialog_bg_color", hx(dialog));
	put("dialog_fg_color", hx(t));
	put("popover_bg_color", hx(dialog));
	put("popover_fg_color", hx(t));
	put("thumbnail_bg_color", hx(thumb));
	put("thumbnail_fg_color", hx(t));
	put("panel_bg_color", hx(d));
	put("panel_fg_color", hx(t));

	put("theme_bg_color", hx(window));
	put("theme_fg_color", hx(t));
	put("theme_base_color", hx(view));
	put("theme_text_color", hx(t));
	put("theme_selected_bg_color", hx(acc));
	put("theme_selected_fg_color", hx(on_accent));
	put("borders", hx(kcol_mixf(window, t, 0.22)));
	put("unfocused_borders", hx(kcol_mixf(window, t, 0.14)));
	put("wm_highlight", hx(header));
	put("content_view_bg", hx(view));
	put("text_view_bg", hx(view));
}

/* ──────────────────────────────────────────────────────────────────────── */

static int name_char(char c)
{
	return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
	       (c >= '0' && c <= '9') || c == '_';
}

static int var_char(char c)
{
	return (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-';
}

static int is_space(char c)
{
	return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f' ||
	       c == '\v';
}

/*
 * The end of a `\s*$` in MULTILINE. `$` matches at end of input or just
 * before a newline, and `\s*` is GREEDY — so a definition sitting at the end
 * of the file swallows the file's final newline, and one followed by a blank
 * line swallows that newline too. Reproducing that is not pedantry: without
 * it every generated stylesheet differs from the python one by a byte.
 *
 * Returns the end of the match, or (size_t)-1 if `\s*$` cannot succeed here.
 */
static size_t greedy_ws_eol(const char *in, size_t len, size_t from)
{
	size_t j = from, best = (size_t)-1;
	for (;;) {
		if (j == len || in[j] == '\n')
			best = j;
		if (j < len && is_space(in[j])) {
			j++;
			continue;
		}
		break;
	}
	return best;
}

/*
 * `^@define-color\s+([A-Za-z0-9_]+)\s+.*?;\s*$` with re.MULTILINE. The `.*?`
 * is non-greedy and cannot cross a newline, so the value ends at the FIRST
 * semicolon on the line that leaves only whitespace to a line end.
 */
static char *rewrite_defines(const char *in, size_t len, size_t *outlen)
{
	static const char TAG[] = "@define-color";
	const size_t taglen = sizeof(TAG) - 1;

	KbBuf b = {0};
	size_t i = 0;

	while (i < len) {
		int at_line_start = (i == 0 || in[i - 1] == '\n');
		if (!at_line_start || i + taglen >= len ||
		    memcmp(in + i, TAG, taglen) || !is_space(in[i + taglen])) {
			kb_buf_add(&b, in + i, 1);
			i++;
			continue;
		}

		size_t eol = i;
		while (eol < len && in[eol] != '\n')
			eol++;

		size_t k = i + taglen;
		while (k < eol && is_space(in[k]))
			k++;
		size_t ns = k;
		while (k < eol && name_char(in[k]))
			k++;
		size_t nlen = k - ns;
		if (!nlen || k >= eol || !is_space(in[k])) {
			kb_buf_add(&b, in + i, 1);
			i++;
			continue;
		}

		size_t end = (size_t)-1;
		for (size_t s = k; s < eol; s++) {
			if (in[s] != ';')
				continue;
			size_t e = greedy_ws_eol(in, len, s + 1);
			if (e != (size_t)-1) {
				end = e;
				break;
			}
		}
		if (end == (size_t)-1) {
			kb_buf_add(&b, in + i, 1);
			i++;
			continue;
		}

		char name[64];
		const char *v = NULL;
		if (nlen < sizeof(name)) {
			memcpy(name, in + ns, nlen);
			name[nlen] = 0;
			v = find_name(name);
		}

		if (v)
			kb_buf_printf(&b, "@define-color %s %s;", name, v);
		else
			kb_buf_add(&b, in + i, end - i);
		i = end;
	}

	if (outlen)
		*outlen = b.n;
	return b.p;
}

/*
 * `--<name>\s*:\s*[^;]+;` — libadwaita's GTK 4.20 custom properties carry the
 * same palette under kebab-case names, and a widget may resolve either
 * spelling. Missing these leaves a GTK4 app half-recoloured.
 */
static char *rewrite_vars(const char *in, size_t len, size_t *outlen)
{
	KbBuf b = {0};
	size_t i = 0;

	while (i < len) {
		if (!(in[i] == '-' && i + 1 < len && in[i + 1] == '-')) {
			kb_buf_add(&b, in + i, 1);
			i++;
			continue;
		}

		size_t k = i + 2, ns = k;
		while (k < len && var_char(in[k]))
			k++;
		size_t nlen = k - ns;
		size_t after_name = k;
		while (k < len && is_space(in[k]))
			k++;
		if (!nlen || k >= len || in[k] != ':') {
			kb_buf_add(&b, in + i, 1);
			i++;
			continue;
		}
		k++;
		while (k < len && is_space(in[k]))
			k++;
		size_t vs = k;
		while (k < len && in[k] != ';')
			k++;
		if (k >= len || k == vs) {
			kb_buf_add(&b, in + i, 1);
			i++;
			continue;
		}
		(void)after_name;

		char name[64];
		if (nlen >= sizeof(name)) {
			kb_buf_add(&b, in + i, 1);
			i++;
			continue;
		}
		memcpy(name, in + ns, nlen);
		name[nlen] = 0;

		char under[64];
		for (size_t j = 0; j <= nlen; j++)
			under[j] = name[j] == '-' ? '_' : name[j];

		const char *v = find_name(under);
		if (v) {
			char rep[160];
			int m = snprintf(rep, sizeof(rep), "--%s: %s;", name, v);
			kb_buf_add(&b, rep, (size_t)m);
		} else {
			kb_buf_add(&b, in + i, k + 1 - i);
		}
		i = k + 1;
	}

	if (outlen)
		*outlen = b.n;
	return b.p;
}

/* ──────────────────────────────────────────────────────────────────────── */

static int copy_assets(const char *src, const char *out, const char *sub)
{
	char *sd = kb_path_join(src, sub);
	char *sa = kb_path_join(sd, "assets");
	int n = 0;

	if (kb_is_dir(sa)) {
		char *od = kb_path_join(out, sub);
		char *oa = kb_path_join(od, "assets");
		kb_mkdir_p(oa);
		char **names = kb_listdir(sa, NULL);
		for (char **p = names; p && *p; p++) {
			char *s = kb_path_join(sa, *p);
			char *d = kb_path_join(oa, *p);
			if (kb_copy_file(s, d) == 0)
				n++;
			free(s);
			free(d);
		}
		kb_strv_free(names);
		free(od);
		free(oa);
	}
	free(sd);
	free(sa);
	return n;
}

static const char INDEX[] =
	"[Desktop Entry]\n"
	"Type=X-GNOME-Metatheme\n"
	"Name=KDOS\n"
	"Comment=KDOS phosphor theme (adw-gtk3, recoloured)\n"
	"Encoding=UTF-8\n"
	"\n"
	"[X-GNOME-Metatheme]\n"
	"GtkTheme=KDOS\n"
	"MetacityTheme=KDOS\n"
	"IconTheme=KDOS\n"
	"CursorTheme=KDOS-cursors\n"
	"ButtonLayout=:close\n";

/* GTK3 picks gtk.css normally and gtk-dark.css when the app asks for dark;
 * upstream's dark variant ships the two byte-identical, and KDOS has no light
 * mode, so one source fills both names. */
static const struct {
	const char *sub;
	const char *name;
	const char *aliases[3];
} SHEETS[] = {
	{ "gtk-3.0", "gtk-dark.css", { "gtk.css", "gtk-dark.css", NULL } },
	{ "gtk-4.0", "gtk.css", { "gtk.css", "gtk-dark.css", NULL } },
	{ "gtk-4.0", "libadwaita.css", { "libadwaita.css", NULL, NULL } },
	{ "gtk-4.0", "libadwaita-tweaks.css",
	  { "libadwaita-tweaks.css", NULL, NULL } },
};

int gen_gtk(const char *src, const char *out, const KcolScheme *sc)
{
	if (!kb_is_dir(src))
		kb_die("no stylesheet source at %s", src);

	build_names(sc);

	kb_rmtree(out);
	kb_mkdir_p(out);
	copy_assets(src, out, "gtk-3.0");
	copy_assets(src, out, "gtk-4.0");

	int written = 0;
	for (size_t i = 0; i < sizeof(SHEETS) / sizeof(SHEETS[0]); i++) {
		char *sd = kb_path_join(src, SHEETS[i].sub);
		char *sp = kb_path_join(sd, SHEETS[i].name);
		size_t len = 0;
		char *data = kb_read_all(sp, &len);
		if (!data)
			kb_die("cannot read %s", sp);
		free(sd);
		free(sp);

		size_t l1 = 0, l2 = 0, l3 = 0;
		char *s1 = kcol_retint_text(data, len, sc, &l1);
		char *s2 = rewrite_defines(s1, l1, &l2);
		char *s3 = rewrite_vars(s2, l2, &l3);
		free(data);
		free(s1);
		free(s2);

		char *od = kb_path_join(out, SHEETS[i].sub);
		kb_mkdir_p(od);
		for (int a = 0; a < 3 && SHEETS[i].aliases[a]; a++) {
			char *dst = kb_path_join(od, SHEETS[i].aliases[a]);
			if (kb_write_all(dst, s3, l3) < 0)
				kb_die("cannot write %s", dst);
			free(dst);
			written++;
		}
		free(od);
		free(s3);
	}

	char *idx = kb_path_join(out, "index.theme");
	kb_write_all(idx, INDEX, sizeof(INDEX) - 1);
	free(idx);

	printf("kdos-gtk-theme: %d stylesheets -> %s\n", written, out);
	return 0;
}
