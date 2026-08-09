/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   libkcolor — the KDOS palette, and the arithmetic on it
 *
 * One table for the distro. It used to exist twice — a bash `palette()` in
 * fs/usr/local/bin/kdos and a C `ki_themes[]` in the installer, whose own
 * comment admitted the duplication — and the theme generators each carried
 * their own copy of the colour maths on top of that.
 * ---------------------------------
 */

#ifndef KCOLOR_H
#define KCOLOR_H

#include <stdint.h>

/* ────────────────────────────────────────────────────────────────────────
 * The schemes
 *
 * X-macro so every consumer builds its own table at COMPILE time from these
 * literals — libktui projects them onto its eight VT slots, the theme
 * generators expand them into CSS, SVG and Xcursor. Nobody gets to keep a
 * second copy of the numbers.
 *
 * Field order is the one `kdos theme` has always used:
 *   primary  dim  secondary  urgent  deep  text  variant  pdark  backdrop
 * ──────────────────────────────────────────────────────────────────────── */

#define KCOL_SCHEMES(X)                                                       \
	X(phosphor, "PHOSPHOR", "KDOS-Phosphor",                              \
	  39ff14, 12401f, ffb000, ff3131, 000a03, b8ffc8, 04120a, 1f8f0c, 02120a) \
	X(amber, "AMBER", "KDOS-Amber",                                       \
	  ffb000, 4a2f00, ff6b1a, ff3131, 0d0600, ffd9a0, 160d02, 8a5f00, 140b02) \
	X(ice, "ICE", "KDOS-Ice",                                             \
	  4dd7ff, 123a4a, 7aa2ff, ff4d6d, 00080d, bfeeff, 031420, 1f7fa0, 021a26) \
	X(bone, "BONE", "KDOS-Bone",                                          \
	  f4f0e2, 3a3833, b9b3a1, ff5a5a, 0a0a09, ded9cb, 141412, 8f8b7e, 161512)

#define KCOL_HEX(x) ((uint32_t)0x##x)

typedef struct {
	uint8_t r, g, b;
} KcolRgb;

typedef struct {
	const char *name;	/* phosphor                                */
	const char *label;	/* PHOSPHOR — for a UI                     */
	const char *theme_name;	/* KDOS-Phosphor — for a COSMIC theme file */
	uint32_t primary;
	uint32_t dim;
	uint32_t secondary;
	uint32_t urgent;
	uint32_t deep;
	uint32_t text;
	uint32_t variant;
	uint32_t pdark;
	uint32_t backdrop;
} KcolScheme;

extern const KcolScheme kcol_schemes[];
extern const int kcol_nscheme;

/* NULL when the name is not one of ours. */
const KcolScheme *kcol_find(const char *name);

/* ────────────────────────────────────────────────────────────────────────
 * Conversions
 * ──────────────────────────────────────────────────────────────────────── */

static inline KcolRgb kcol_rgb(uint32_t hex)
{
	KcolRgb c = { (uint8_t)(hex >> 16), (uint8_t)(hex >> 8), (uint8_t)hex };
	return c;
}

/* Accepts "39ff14", "#39ff14", "#39f". Returns -1 if it is not a colour. */
int kcol_parse(const char *s, uint32_t *out);

/* Six lowercase hex digits, no '#'. buf must hold 7 bytes. */
void kcol_format(uint32_t rgb, char *buf);

/* HLS, in python's `colorsys` field order and with its exact arithmetic —
 * the generators these replace were written against it, and the committed
 * artwork has to come out byte-identical. Do not "clean up" the operation
 * order in kcolor.c; it is load-bearing. */
void kcol_to_hls(uint32_t rgb, double *h, double *l, double *s);
uint32_t kcol_from_hls(double h, double l, double s);

/* Linear blend, pct 0..100 of `b` over `a`. Integer maths, matching the
 * `mix_hex` the shell version used, so generated files do not shift by one. */
uint32_t kcol_mix(uint32_t a, uint32_t b, int pct);

/* The float form the stylesheet generator uses: x + (y - x) * t, rounded the
 * way python rounds. NOT interchangeable with kcol_mix — they disagree by a
 * unit on some inputs, and each generated file was written against one. */
uint32_t kcol_mixf(uint32_t a, uint32_t b, double t);

double kcol_luma(uint32_t rgb);	/* 0..1, Rec.601 */

/* ────────────────────────────────────────────────────────────────────────
 * Hue families
 *
 * Papirus colour-codes mimetypes, so flattening every hue onto one accent
 * turns a folder of files into a wall of identical lozenges. A PDF stays red
 * and an audio file stays amber while folders, devices and places go
 * phosphor. genicons and gengtk both classified hues independently before
 * this existed, and they did not agree.
 * ──────────────────────────────────────────────────────────────────────── */

enum {
	KCOL_FAM_NEUTRAL = 0,	/* near-grey — faintly tinted              */
	KCOL_FAM_ACCENT,	/* blue / green / purple                   */
	KCOL_FAM_SECONDARY,	/* yellow / orange / brown                 */
	KCOL_FAM_URGENT		/* red                                     */
};

int kcol_family(uint32_t rgb);

/* Remap one source colour into `sc`, keeping its own lightness and
 * saturation. This is the operation every generator performs. */
uint32_t kcol_remap(const KcolScheme *sc, uint32_t rgb);

/* Rewrite every `#rrggbb` / `#rgb` token in a blob through kcol_remap, and
 * leave everything else byte for byte alone. Both the SVG icons and the GTK
 * stylesheets are recoloured with exactly this and nothing more — there is no
 * SVG parser and no CSS parser anywhere in KDOS.
 *
 * Token recognition matches python's `#([0-9a-fA-F]{6}|[0-9a-fA-F]{3})\b`,
 * trailing word boundary included, so `#39ff14ff` is left alone rather than
 * half-rewritten. Returns a malloc'd NUL-terminated buffer. */
char *kcol_retint_text(const char *in, size_t len, const KcolScheme *sc,
		       size_t *outlen);

#endif /* KCOLOR_H */
