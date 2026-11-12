/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   kdos-theme — KColorScheme files, and the greeter's kdeglobals
 *
 *   kde-colors <outdir> [accent]        KDOS<Title>.colors, one per accent,
 *                                       and KDOSBox.colors with no accent
 *   greeter-globals <file> [accent]     merge the colour block into <file>
 *   greeter-globals --sddm <outdir> [accent]
 *                                       <outdir>/<accent>/{kdeglobals,
 *                                       plasmarc,kcminputrc}
 *
 * ONE ACCENT, ONE SCHEME NAME. `plasma-apply-colorscheme` answers "already
 * set" for the name in force and applies nothing, so a single scheme
 * rewritten per accent could never be re-applied; eight names can.
 *
 * THE GREETER'S BLOCK IS ITS OWN FILE'S, NOT A SESSION'S. Processes outside a
 * Plasma session — the greeter, a pkexec dialog, early autostart — read
 * kdeglobals through the XDG config directories only and never see the
 * session's kdedefaults layer, so a colour scheme that reaches them has to be
 * written into a kdeglobals they read: the distribution default for the
 * default accent, and the sddm user's own for the system accent.
 * ---------------------------------
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "kdos-theme.h"

/* The [ColorEffects:*] amounts are Breeze's. Disabled text fades 65% into its
 * background, which is what marks a control as unavailable — WCAG exempts an
 * inactive component from the contrast floor for exactly that reason, and
 * the roles the floor does cover are written unfaded above. The inactive
 * effect is off, so an unfocused window keeps the scheme's colours. */
static void effects(KthIni *ini, const KthPalette *p)
{
	KbBuf c = {0};
	KthGroup *g;

	kth_rgb(&c, p->disabled);
	g = kth_ini_group(ini, "ColorEffects:Disabled", 1);
	kth_ini_set(g, "Color", c.p);
	kth_ini_set(g, "ColorAmount", "0");
	kth_ini_set(g, "ColorEffect", "0");
	kth_ini_set(g, "ContrastAmount", "0.65");
	kth_ini_set(g, "ContrastEffect", "1");
	kth_ini_set(g, "IntensityAmount", "0.1");
	kth_ini_set(g, "IntensityEffect", "2");

	c.n = 0;
	kth_rgb(&c, p->inactive);
	g = kth_ini_group(ini, "ColorEffects:Inactive", 1);
	kth_ini_set(g, "ChangeSelectionColor", "true");
	kth_ini_set(g, "Color", c.p);
	kth_ini_set(g, "ColorAmount", "0.025");
	kth_ini_set(g, "ColorEffect", "2");
	kth_ini_set(g, "ContrastAmount", "0.1");
	kth_ini_set(g, "ContrastEffect", "2");
	kth_ini_set(g, "Enable", "false");
	kth_ini_set(g, "IntensityAmount", "0");
	kth_ini_set(g, "IntensityEffect", "0");
	kb_buf_free(&c);
}

/* Each set's keys in the order KDE's own schemes list them, so a diff against
 * Breeze's file lines up. */
static const int ROLE_ORDER[KTH_NROLE] = {
	KTH_BG_ALT, KTH_BG, KTH_DECO_FOCUS, KTH_DECO_HOVER,
	KTH_FG_ACTIVE, KTH_FG_INACTIVE, KTH_FG_LINK, KTH_FG_NEGATIVE,
	KTH_FG_NEUTRAL, KTH_FG, KTH_FG_POSITIVE, KTH_FG_VISITED,
};

static void sets(KthIni *ini, const KthPalette *p)
{
	/* Written in the byte order of their group names, the order KConfig
	 * itself writes a file in, so the Colors KCM saving this scheme back
	 * produces the same layout. */
	static const int SET_ORDER[KTH_NSET] = {
		KTH_BUTTON, KTH_COMPLEMENTARY, KTH_HEADER, KTH_HEADER_INACTIVE,
		KTH_SELECTION, KTH_TOOLTIP, KTH_VIEW, KTH_WINDOW,
	};
	KbBuf c = {0};

	for (int i = 0; i < KTH_NSET; i++) {
		int s = SET_ORDER[i];
		KthGroup *g = kth_ini_group(ini, kth_set_group[s], 1);

		for (int k = 0; k < KTH_NROLE; k++) {
			int r = ROLE_ORDER[k];

			c.n = 0;
			kth_rgb(&c, p->set[s][r]);
			kth_ini_set(g, kth_role_key[r], c.p);
		}
	}
	kb_buf_free(&c);
}

static void wm(KthIni *ini, const KthPalette *p)
{
	KthGroup *g = kth_ini_group(ini, "WM", 1);
	KbBuf c = {0};

	for (int k = 0; k < KTH_NWM; k++) {
		c.n = 0;
		kth_rgb(&c, p->wm[k]);
		kth_ini_set(g, kth_wm_key[k], c.p);
	}
	kb_buf_free(&c);
}

static void scheme_id(const KcolScheme *sc, char *out, size_t n)
{
	snprintf(out, n, "KDOS%s", kth_title(sc));
}

/* The group to the end of the file, the rest in their order: a scheme file
 * lists its groups in the byte order of their names, and [WM] sorts after
 * the [General] and [KDE] that are added once the colours are in. */
static void move_last(KthIni *ini, const char *name)
{
	for (int i = 0; i < ini->ng - 1; i++) {
		if (strcmp(ini->g[i].name, name))
			continue;
		KthGroup g = ini->g[i];

		memmove(&ini->g[i], &ini->g[i + 1],
			(size_t)(ini->ng - 1 - i) * sizeof(*ini->g));
		ini->g[ini->ng - 1] = g;
		return;
	}
}

/* ── the assertion, before anything is written ────────────────────────── */

typedef struct {
	const char *scheme;
	int bad;
} Gate;

static void gate_row(void *ctx, const char *group, const char *role, int kind,
		     int a, int b, int floor, int ok)
{
	Gate *gt = ctx;

	if (ok)
		return;
	gt->bad++;
	if (a < 0)
		kb_warn("%s [%s] %s is missing", gt->scheme, group, role);
	else if (kind == KTH_ROW_CHROMA)
		kb_warn("%s [%s] %s: chroma %d.%02d against a ceiling of "
			"%d.%02d", gt->scheme, group, role, a / 100, a % 100,
			floor / 100, floor % 100);
	else
		kb_warn("%s [%s] %s: %d.%02d and %d.%02d against a floor of "
			"%d.%02d", gt->scheme, group, role, a / 100, a % 100,
			b / 100, b % 100, floor / 100, floor % 100);
}

static void assert_palette(const KcolScheme *sc, const KthPalette *p)
{
	char id[64];
	Gate gt = { id, 0 };

	scheme_id(sc, id, sizeof(id));
	kth_check(p, gate_row, &gt);
	if (gt.bad)
		kb_die("%s fails %d contrast floor%s; nothing was written", id,
		       gt.bad, gt.bad == 1 ? "" : "s");
}

static void groups(const KcolScheme *sc, const KthPalette *p, KthIni *ini)
{
	assert_palette(sc, p);
	effects(ini, p);
	sets(ini, p);
	wm(ini, p);
}

void kth_colour_groups(const KcolScheme *sc, KthIni *ini)
{
	KthPalette p;

	kth_palette(sc, &p);
	groups(sc, &p, ini);
}

/* ── the neutral box ──────────────────────────────────────────────────── */

/* The grey a colour measures the same against black as: equal relative
 * luminance, so every contrast ratio between two greys is the ratio between
 * the colours they came from, to kcol_contrast's precision. */
static uint32_t grey_of(uint32_t c)
{
	int want = kcol_contrast(c, 0), best = 0, bd = -1;

	for (int g = 0; g < 256; g++) {
		uint32_t v = (uint32_t)g * 0x010101u;
		int d = abs(kcol_contrast(v, 0) - want);

		if (bd < 0 || d < bd) {
			bd = d;
			best = g;
		}
	}
	return (uint32_t)best * 0x010101u;
}

/*
 * KDOSBOX, THE TITLE BAR OF A BOX WITH NO ACCENT. kdos-appbox writes
 * `decocolor=KDOSBox` into the box's KWin rule, and KWin ignores a decocolor
 * naming a scheme that is not installed, so without this file such a box
 * has no tint at all and reads as host software. It is the default accent
 * with every colour taken to its grey of equal luminance — neutral, never
 * mistaken for an accent a person picked, and holding the default's floors —
 * and its title bars sit at KTH_BOX_HEADER_MIX and KTH_BOX_INACTIVE_MIX
 * instead of an accent's 7% and `deep`: the default accent is itself nearly
 * grey, so at an accent's own mix the box's title bar measures 1.0:1 against
 * the host's. box_apart() holds both of its title bars at BOX_APART against
 * both of every accent's. The urgent colour keeps its hue, because an error
 * is still an error.
 */
static void box_scheme(KcolScheme *out)
{
	const KcolScheme *d = kcol_default();

	*out = *d;
	out->name = "box";
	out->label = "BOX";
	out->theme_name = "KDOS-Box";
	out->primary = grey_of(d->primary);
	out->dim = grey_of(d->dim);
	out->secondary = grey_of(d->secondary);
	out->deep = grey_of(d->deep);
	out->text = grey_of(d->text);
	out->variant = grey_of(d->variant);
	out->pdark = grey_of(d->pdark);
	out->backdrop = grey_of(d->backdrop);
}

/* How far KDOSBox's title bars stand from every accent's, as kcol_contrast
 * measures: the least that reads as a different window at a glance. */
#define BOX_APART	150

static void box_apart(const KthPalette *box)
{
	static const int BG[] = { KTH_WM_ACTIVE_BG, KTH_WM_INACTIVE_BG };
	int bad = 0;

	for (int i = 0; i < kcol_nscheme; i++) {
		KthPalette p;

		kth_palette(&kcol_schemes[i], &p);
		for (int a = 0; a < 2; a++)
			for (int b = 0; b < 2; b++) {
				int r = kcol_contrast(box->wm[BG[a]], p.wm[BG[b]]);

				if (r >= BOX_APART)
					continue;
				kb_warn("KDOSBox [WM] %s measures %d.%02d against "
					"KDOS%s's %s, under %d.%02d",
					kth_wm_key[BG[a]], r / 100, r % 100,
					kth_title(&kcol_schemes[i]),
					kth_wm_key[BG[b]], BOX_APART / 100,
					BOX_APART % 100);
				bad++;
			}
	}
	if (bad)
		kb_die("KDOSBox's title bar reads as an accent's; nothing was "
		       "written");
}

/* ── kde-colors ───────────────────────────────────────────────────────── */

int kth_kde_colors(const char *outdir, const char *accent)
{
	const KcolScheme *pick[33];
	KcolScheme box;
	int n = kth_pick(accent, pick, 32), nbox = 0;

	if (!accent) {
		box_scheme(&box);
		pick[n++] = &box;
		nbox = 1;
	}

	for (int i = 0; i < n; i++) {
		const KcolScheme *sc = pick[i];
		char id[64], file[80], contrast[8];
		KthIni ini;
		KthPalette p;
		KbBuf name = {0}, out = {0};

		memset(&ini, 0, sizeof(ini));
		scheme_id(sc, id, sizeof(id));
		if (nbox && i == n - 1) {
			kth_palette_titled(sc, KTH_BOX_HEADER_MIX,
					   KTH_BOX_INACTIVE_MIX, &p);
			box_apart(&p);
		} else {
			kth_palette(sc, &p);
		}
		groups(sc, &p, &ini);

		KthGroup *g = kth_ini_group(&ini, "General", 1);

		kb_buf_printf(&name, "KDOS %s", kth_title(sc));
		kth_ini_set(g, "ColorScheme", id);
		kth_ini_set(g, "Name", name.p);
		kth_ini_set(g, "shadeSortColumn", "true");

		g = kth_ini_group(&ini, "KDE", 1);
		snprintf(contrast, sizeof(contrast), "%d", p.contrast);
		kth_ini_set(g, "contrast", contrast);
		move_last(&ini, "WM");

		kth_ini_write(&ini, &out);
		snprintf(file, sizeof(file), "%s.colors", id);
		kth_emit(outdir, file, &out);

		kb_buf_free(&name);
		kb_buf_free(&out);
		kth_ini_free(&ini);
	}
	return 0;
}

/* ── greeter-globals ──────────────────────────────────────────────────── */

/* What the merge owns: every [Colors:*] and [ColorEffects:*] group whole, the
 * six colour keys of [WM], [General] ColorScheme and [KDE] contrast. Owning a group means
 * replacing it, so a key an older scheme had and this one has not cannot
 * survive the switch. [WM] is shared — it carries the title-bar font too —
 * so there only the colours are KDOS's. */
static int owned_group(const char *name)
{
	return !strncmp(name, "Colors:", 7) ||
	       !strncmp(name, "ColorEffects:", 13);
}

static void merge(KthIni *dst, const KcolScheme *sc)
{
	KthIni block;
	char id[64];

	memset(&block, 0, sizeof(block));
	kth_colour_groups(sc, &block);
	scheme_id(sc, id, sizeof(id));

	for (int i = 0; i < dst->ng;) {
		if (!owned_group(dst->g[i].name)) {
			i++;
			continue;
		}
		/* A copy, because the drop frees the name it is matching. */
		char *name = kb_strdup(dst->g[i].name);

		kth_ini_drop(dst, name);
		free(name);
	}
	for (int i = 0; i < dst->ng; i++)
		if (!strcmp(dst->g[i].name, "WM"))
			for (int k = 0; k < KTH_NWM; k++)
				kth_ini_unset(&dst->g[i], kth_wm_key[k]);

	/* [WM] is made, when the file has none, BEFORE the groups that are
	 * replaced whole: a second merge finds it where the first left it and
	 * appends the replaced groups after it again, so the two runs write
	 * the same bytes. */
	kth_ini_group(dst, "WM", 1);

	KthGroup *gen = kth_ini_group(dst, "General", 1);

	kth_ini_set(gen, "ColorScheme", id);
	/* KColorScheme derives the Window shades at [KDE] contrast and reads
	 * KDE's default when the key is absent, which is not the contrast the
	 * shades were held at. [KDE] is shared, so only this key is KDOS's. */
	char contrast[8];

	snprintf(contrast, sizeof(contrast), "%d", KTH_CONTRAST);
	kth_ini_set(kth_ini_group(dst, "KDE", 1), "contrast", contrast);
	/* The hash startplasma compares against the installed scheme file; a
	 * stale one makes a session re-apply a scheme it did not choose. This
	 * block is not that file, so it carries no hash at all. */
	kth_ini_unset(gen, "ColorSchemeHash");

	for (int i = 0; i < block.ng; i++) {
		KthGroup *to = kth_ini_group(dst, block.g[i].name, 1);

		for (int j = 0; j < block.g[i].nline; j++) {
			const char *l = block.g[i].line[j];
			const char *eq = strchr(l, '=');
			char key[64];

			if (!eq || (size_t)(eq - l) >= sizeof(key))
				continue;
			memcpy(key, l, (size_t)(eq - l));
			key[eq - l] = 0;
			kth_ini_set(to, key, eq + 1);
		}
	}
	kth_ini_free(&block);
}

int kth_greeter_merge(const char *file, const char *accent)
{
	const KcolScheme *sc = accent ? kcol_find(accent) : kcol_default();

	if (!sc)
		kb_die("unknown accent '%s' (try: kdos-theme accents)", accent);
	sc = kth_dark_partner(sc);

	int to_stdout = !strcmp(file, "-");
	KthIni ini;
	KbBuf out = {0};

	memset(&ini, 0, sizeof(ini));
	if (!to_stdout && kth_ini_load(&ini, file) < 0)
		kb_die("cannot read %s", file);
	merge(&ini, sc);
	kth_ini_write(&ini, &out);

	if (to_stdout) {
		fwrite(out.p, 1, out.n, stdout);
	} else {
		char *dir = kb_strdup(file);
		char *slash = strrchr(dir, '/');
		const char *base = slash ? slash + 1 : file;

		if (slash)
			*slash = 0;
		kth_emit(slash ? (dir[0] ? dir : "/") : ".", base, &out);
		free(dir);
	}
	kb_buf_free(&out);
	kth_ini_free(&ini);
	return 0;
}

/*
 * The three files sddm-kcm's "Apply Plasma Settings" copies into the sddm
 * user's ~/.config, one set per accent, so the accent step of kdos-powerd and
 * sddm-kcm write the same files and neither has to generate anything as root.
 * Beside them, `background.png` is a symlink to the dark picture of the
 * greeter's accent at the largest size its wallpaper package carries: the
 * file the accent step copies to the greeter's background, named here so the
 * one place that knows which accent the greeter shows is this one.
 *
 * Fonts are not here: the greeter's face comes from sddm.conf's `[Theme]
 * Font=`, and a second statement of it would be a second thing to keep in
 * step.
 */
int kth_greeter_sddm(const char *outdir, const char *accent)
{
	const KcolScheme *pick[32];
	int n = kth_pick(accent, pick, 32);

	for (int i = 0; i < n; i++) {
		const KcolScheme *g = kth_dark_partner(pick[i]);
		char *dir = kb_path_join(outdir, pick[i]->name);
		KthIni ini;
		KbBuf b = {0}, lnf = {0};

		memset(&ini, 0, sizeof(ini));
		merge(&ini, g);
		kb_buf_printf(&lnf, "org.kdos.%s.desktop", g->name);
		kth_ini_set(kth_ini_group(&ini, "Icons", 1), "Theme", "KDOS");
		KthGroup *kde = kth_ini_group(&ini, "KDE", 1);

		kth_ini_set(kde, "LookAndFeelPackage", lnf.p);
		kth_ini_set(kde, "widgetStyle", "Breeze");
		kth_ini_write(&ini, &b);
		kth_emit(dir, "kdeglobals", &b);
		kth_ini_free(&ini);

		b.n = 0;
		kb_buf_str(&b, "[Theme]\nname=kdos\n");
		kth_emit(dir, "plasmarc", &b);

		b.n = 0;
		kb_buf_str(&b, "[Mouse]\ncursorSize=24\ncursorTheme=breeze_cursors\n");
		kth_emit(dir, "kcminputrc", &b);

		KthSize big = kth_wallpaper_largest();
		char *link = kb_path_join(dir, "background.png");

		b.n = 0;
		kb_buf_printf(&b, "/usr/share/wallpapers/%s/contents/images_dark/%dx%d.png",
			      g->theme_name, big.w, big.h);
		unlink(link);
		if (symlink(b.p, link) < 0)
			kb_die("cannot link %s", link);
		free(link);

		kb_buf_free(&b);
		kb_buf_free(&lnf);
		free(dir);
	}
	return 0;
}
