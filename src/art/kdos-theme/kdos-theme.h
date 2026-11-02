/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   kdos-theme — the identity kit's generators, in C
 *
 * Every colour KDOS hands to KDE is written by this program at BUILD TIME,
 * from libkcolor's KCOL_SCHEMES and from nothing else: the KColorScheme files,
 * the Konsole schemes and profiles, the qt5ct palettes, the wallpapers and the
 * greeter's palette. A colour literal in any of those files is a second copy
 * of the palette, and the copy is the one that goes stale when an accent
 * moves.
 *
 * The KDE role mapping is decided ONCE, in roles.c, and every writer expands
 * the same KthPalette: the .colors file, the greeter's kdeglobals and the Qt 5
 * palette cannot disagree about what a selected row looks like, because one
 * function says.
 *
 * Symbols shared between the files carry `kth_`; libkbase owns `kb_` and
 * libkcolor `kcol_`, and this program links both.
 * ---------------------------------
 */

#ifndef KDOS_THEME_H
#define KDOS_THEME_H

#include <stddef.h>
#include <stdint.h>

#include "kbase.h"
#include "kcolor.h"

/* ────────────────────────────────────────────────────────────────────────
 * Names
 *
 * One accent is three spellings and each is derived from the table's
 * `theme_name`, never typed: `KDOS<Title>` is the colour scheme,
 * `KDOS-<Title>` the Konsole scheme, the qt5ct file and the wallpaper
 * package, `KDOS <Title>` what a person reads. A title typed out beside the
 * table is a ninth accent the day somebody renames one.
 * ──────────────────────────────────────────────────────────────────────── */

/* "Bone": `theme_name` past its "KDOS-". Dies on a row that does not carry
 * the prefix, because every name built from it would then be wrong. */
const char *kth_title(const KcolScheme *sc);

/* A scheme is light when its ground is: the ground is what decides which icon
 * theme and which wallpaper variant can sit on it. */
int kth_is_light(const KcolScheme *sc);

/* The scheme a surface that can only be dark shows for `sc`: `sc` itself when
 * it is dark, else the default when the default is dark, else the first dark
 * row. The greeter reads no day/night key, so it always shows this one. */
const KcolScheme *kth_dark_partner(const KcolScheme *sc);

/* The scheme whose ground a light composition is drawn on: the first light
 * row. */
const KcolScheme *kth_light_scheme(void);

/* The scheme `name` names, or every scheme when `name` is NULL. Dies on a
 * name the table does not have, so a misspelt accent in a build.sh fails the
 * build instead of shipping seven. Returns the count written into `out`. */
int kth_pick(const char *name, const KcolScheme **out, int cap);

/* ────────────────────────────────────────────────────────────────────────
 * The KDE role mapping
 * ──────────────────────────────────────────────────────────────────────── */

enum {
	KTH_VIEW,
	KTH_WINDOW,
	KTH_BUTTON,
	KTH_SELECTION,
	KTH_TOOLTIP,
	KTH_COMPLEMENTARY,
	KTH_HEADER,
	KTH_HEADER_INACTIVE,
	KTH_NSET
};

enum {
	KTH_BG,
	KTH_BG_ALT,
	KTH_DECO_FOCUS,
	KTH_DECO_HOVER,
	KTH_FG,
	KTH_FG_INACTIVE,
	KTH_FG_ACTIVE,
	KTH_FG_LINK,
	KTH_FG_VISITED,
	KTH_FG_NEGATIVE,
	KTH_FG_NEUTRAL,
	KTH_FG_POSITIVE,
	KTH_NROLE
};

enum {
	KTH_WM_ACTIVE_BG,
	KTH_WM_ACTIVE_BLEND,
	KTH_WM_ACTIVE_FG,
	KTH_WM_INACTIVE_BG,
	KTH_WM_INACTIVE_BLEND,
	KTH_WM_INACTIVE_FG,
	KTH_NWM
};

/* The group each set is written under, in KConfig's own spelling between the
 * outer brackets: the nested group `[Colors:Header][Inactive]` is
 * "Colors:Header][Inactive". */
extern const char *const kth_set_group[KTH_NSET];
extern const char *const kth_role_key[KTH_NROLE];
extern const char *const kth_wm_key[KTH_NWM];

/* The floor a role is held to against BOTH backgrounds of its set, x100 as
 * kcol_contrast measures: 450 for text (WCAG AA), 300 for a decoration (WCAG
 * 1.4.11, non-text), 0 for the backgrounds themselves. */
int kth_role_floor(int role);

/* A colour a file did not carry. Above 24 bits, so no colour collides. */
#define KTH_MISSING 0x01000000u

typedef struct {
	uint32_t set[KTH_NSET][KTH_NROLE];
	uint32_t wm[KTH_NWM];
	uint32_t disabled;	/* [ColorEffects:Disabled] Color */
	uint32_t inactive;	/* [ColorEffects:Inactive] Color */
} KthPalette;

void kth_palette(const KcolScheme *sc, KthPalette *p);

/* ────────────────────────────────────────────────────────────────────────
 * The contrast assertion
 *
 * One table of checks, run in two places: over the palette in memory before
 * `kde-colors` writes a byte, so a scheme that fails is never installed, and
 * over files on disk by `contrast`, so what is installed is what was
 * measured. Each check calls `row` once; the return is how many failed. A
 * role or a set a file does not carry is a failure, never a skip: a check
 * that can be passed by omission is not a check.
 * ──────────────────────────────────────────────────────────────────────── */

typedef void (*KthRow)(void *ctx, const char *group, const char *role,
		       int vs_bg, int vs_alt, int floor, int ok);

int kth_check(const KthPalette *p, KthRow row, void *ctx);

/* "R,G,B", KColorScheme's format. A hex string in a .colors file reads as
 * black, so nothing here writes one. */
void kth_rgb(KbBuf *b, uint32_t c);

/* ────────────────────────────────────────────────────────────────────────
 * A KConfig-shaped INI file
 *
 * Read whole, kept as the lines it was read as, and written back byte for
 * byte except where a caller replaced something. The greeter's kdeglobals is
 * MERGED into a file other programs write too, and a writer that normalised
 * what it does not own would rewrite their keys on every run.
 * ──────────────────────────────────────────────────────────────────────── */

typedef struct {
	char *name;	/* between the outer brackets; "" before any header */
	char **line;	/* the group's lines as read, header excluded         */
	int nline, cap;
	int fresh;	/* created since the file was read                    */
} KthGroup;

typedef struct {
	KthGroup *g;
	int ng, cap;
} KthIni;

/* An absent file is an empty one: 0 either way, -1 only when the file exists
 * and cannot be read. */
int kth_ini_load(KthIni *ini, const char *path);
void kth_ini_free(KthIni *ini);

KthGroup *kth_ini_group(KthIni *ini, const char *name, int create);
/* The value of `key=` in `g`, or NULL; the last assignment wins, as KConfig
 * reads it. A localised (`key[de]=`) or flagged (`key[$e]=`) spelling is a
 * different key and never matches. */
const char *kth_ini_get(const KthGroup *g, const char *key);
void kth_ini_set(KthGroup *g, const char *key, const char *val);
void kth_ini_unset(KthGroup *g, const char *key);
void kth_ini_drop(KthIni *ini, const char *name);

/* Every group as it was read, line for line; a group created since gets one
 * blank line before its header. Writing what was read gives the bytes read
 * (plus a final newline), and a merge run twice gives the same bytes twice. */
void kth_ini_write(const KthIni *ini, KbBuf *out);

/* ────────────────────────────────────────────────────────────────────────
 * Output
 * ──────────────────────────────────────────────────────────────────────── */

/* `b` into `dir`/`name`, making `dir` first, written beside and renamed over.
 * Dies on failure: these run inside a build.sh, and a generator that warns
 * and carries on installs a package with a file missing. */
void kth_emit(const char *dir, const char *name, const KbBuf *b);

/* ────────────────────────────────────────────────────────────────────────
 * The generators. `accent` NULL means all eight.
 * ──────────────────────────────────────────────────────────────────────── */

int kth_kde_colors(const char *outdir, const char *accent);
int kth_konsole(const char *outdir, const char *accent);
int kth_qt5ct(const char *outdir, const char *accent);
int kth_greeter_merge(const char *file, const char *accent);
int kth_greeter_sddm(const char *outdir, const char *accent);
int kth_contrast(int nfile, char **files, int quiet);

/* The colour block the greeter's kdeglobals carries, for `sc` as it is (the
 * caller has already applied kth_dark_partner where it must). */
void kth_colour_groups(const KcolScheme *sc, KthIni *ini);

typedef struct {
	int w, h;
} KthSize;

int kth_wallpaper(const char *outdir, const char *accent, const char *src,
		  const KthSize *sizes, int nsize);

/* The largest size a wallpaper package carries by default: the picture the
 * greeter is given, because it is scaled down to every screen and never up. */
KthSize kth_wallpaper_largest(void);

#endif /* KDOS_THEME_H */
