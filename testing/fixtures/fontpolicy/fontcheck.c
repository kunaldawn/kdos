/*
 * THE CELL FONT'S SIZE POLICY, measured.
 *
 * Terminus is a bitmap with strikes up to 32 pixels, and fontconfig draws any
 * other size by scaling the nearest strike — exact at a whole multiple, uneven
 * at a fraction, and not at all within a fifth of it. kcell_font_load() moves
 * a size no strike draws exactly onto `Terminus (TTF)`, the same typeface in
 * outlines, and pins a scalable face's cell to the pixel size asked for. The
 * decisions are arithmetic on fontconfig's answer and on a face's metrics, so
 * they are checked here with no font at all:
 *
 *   - a whole-multiple scale is exact, a fractional one is not, an unscaled
 *     strike is exact within a tenth and short past it;
 *   - the twin's name keeps every property after the family and is never
 *     asked twice, and only a face fcft names as that family is taken;
 *   - a line at most a sixteenth of the size taller, rounded up, is pinned
 *     to the size, and a taller one is not;
 *   - the pinned baseline is the face's ascent scaled into the cell, which is
 *     the bitmap strike's own at the sizes both exist;
 *   - a name moved to another pixel size (a fractional output scale's
 *     device size) loses every size property it carried and keeps the rest.
 *
 * Where Terminus and its twin ARE installed the loads are checked too: 64 is
 * the doubled 32 strike, 36 and 40 are the twin in cells of 18x36 and 20x40,
 * 8 is the twin in 8 rows, the twin named directly is pinned, and a size
 * fontconfig answers with the strike above it (11, 13, 31) is that whole
 * strike, never pinned; and the fractional output scales libkwl draws at —
 * 1.25, 1.5 and 1.75 of the 32 cell — come back as the twin in cells of
 * exactly 20x40, 24x48 and 28x56. Otherwise that half says it was skipped.
 */
#include <stdio.h>
#include <string.h>

#include <fcft/fcft.h>

#include "kcell.h"
#include "kcell_priv.h"

static int bad;

#define CHECK(cond, ...)                                                       \
	do {                                                                   \
		if (!(cond)) {                                                 \
			printf("    FAIL  ");                                  \
			printf(__VA_ARGS__);                                   \
			printf("\n");                                          \
			bad = 1;                                               \
		}                                                              \
	} while (0)

static void at_px(const char *in, int px, const char *want)
{
	char out[128];
	int ok = kcell_name_at_px(in, px, out, sizeof(out));

	if (!want)
		CHECK(!ok, "'%s' at %d should be refused, got '%s'", in, px, out);
	else
		CHECK(ok && !strcmp(out, want), "'%s' at %d: want '%s', got '%s'",
		      in, px, want, ok ? out : "(refused)");
}

static void twin(const char *in, const char *want)
{
	char out[128];
	int ok = kcell_twin_name(in, out, sizeof(out));

	if (!want)
		CHECK(!ok, "twin of '%s' should be refused, got '%s'", in, out);
	else
		CHECK(ok && !strcmp(out, want), "twin of '%s': want '%s', got '%s'",
		      in, want, ok ? out : "(refused)");
}

/* Whether fontconfig will hand fcft this family for this name: a face fcft
 * names as the family asked for, not a substitute. */
static int installed(const char *spec)
{
	const char *names[1] = { spec };
	struct fcft_font *f = fcft_from_name(1, names, NULL);
	int yes = f && kcell_face_named(f->name, spec);

	if (f)
		fcft_destroy(f);
	return yes;
}

int main(void)
{
	char small[8];

	/* The size in a name. */
	CHECK(kcell_name_px("Terminus:pixelsize=64") == 64, "pixelsize=64");
	CHECK(kcell_name_px("Terminus:pixelsize=40:weight=bold") == 40,
	      "pixelsize before another property");
	CHECK(kcell_name_px("Terminus:size=11") == 0, "a point size is not px");
	CHECK(kcell_name_px("Terminus") == 0, "no size");
	CHECK(kcell_name_px(NULL) == 0, "NULL");

	/* Exact or not. */
	CHECK(!kcell_strike_off(32, 1, 32), "the strike at its own size");
	CHECK(!kcell_strike_off(32, 1, 34), "34 on the 32 strike is within a tenth");
	CHECK(kcell_strike_off(32, 1, 36), "36 on the 32 strike is short");
	CHECK(!kcell_strike_off(32, 2, 64), "64 is the 32 strike doubled");
	CHECK(!kcell_strike_off(16, 4, 64), "a four-times multiple is exact");
	CHECK(kcell_strike_off(32, 1.25, 40), "40 is a fractional scale");
	CHECK(kcell_strike_off(32, 1.5, 48), "48 is a fractional scale");
	CHECK(kcell_strike_off(32, 2.25, 72), "72 is a fractional scale");
	CHECK(kcell_strike_off(12, 0.75, 9), "a scale down is fractional");
	CHECK(!kcell_strike_off(0, 1, 40), "no strike reported");
	CHECK(!kcell_strike_off(32, 2, 0), "no size asked");

	/* Short. */
	CHECK(kcell_face_short(32, 40), "32 for 40 is short");
	CHECK(!kcell_face_short(35, 32), "a taller face is not short");
	CHECK(!kcell_face_short(29, 32), "29 for 32 is within a tenth");

	/* The twin's name. */
	twin("Terminus:pixelsize=64", "Terminus (TTF):pixelsize=64");
	twin("Terminus:pixelsize=40:weight=bold",
	     "Terminus (TTF):pixelsize=40:weight=bold");
	twin("Terminus,monospace:pixelsize=40", "Terminus (TTF):pixelsize=40");
	twin("Terminus", "Terminus (TTF)");
	twin("Terminus (TTF):pixelsize=64", NULL);
	twin("terminus (ttf):pixelsize=64", NULL);
	twin(":pixelsize=64", NULL);
	twin("", NULL);
	CHECK(!kcell_twin_name("Terminus:pixelsize=64", small, sizeof(small)),
	      "a twin that does not fit is refused");

	/* Recognising it. */
	CHECK(kcell_face_named("Terminus (TTF)", "Terminus (TTF):pixelsize=64"),
	      "the twin's own name");
	CHECK(kcell_face_named("Terminus (TTF) Bold",
			       "Terminus (TTF):pixelsize=64:weight=bold"),
	      "a styled face of the twin");
	CHECK(kcell_face_named("terminus (ttf)", "Terminus (TTF)"),
	      "case-folded as fontconfig compares");
	CHECK(!kcell_face_named("Noto Sans Regular", "Terminus (TTF):pixelsize=64"),
	      "a substitute is refused");
	CHECK(!kcell_face_named("Terminus (TTF)X", "Terminus (TTF)"),
	      "a longer family is not this one");
	CHECK(!kcell_face_named(NULL, "Terminus (TTF)"), "a face with no name");

	/* Which lines pin: Terminus TTF's, measured, and DejaVu Sans Mono's. */
	CHECK(kcell_pin_fits(7, 2, 8), "8: a line of 9 pins to 8");
	CHECK(kcell_pin_fits(8, 2, 9), "9: a line of 10 pins to 9");
	CHECK(kcell_pin_fits(54, 11, 64), "64: a line of 65 pins to 64");
	CHECK(kcell_pin_fits(67, 1, 64), "64: a line of 68 pins to 64");
	CHECK(!kcell_pin_fits(67, 2, 64), "64: a line of 69 does not");
	CHECK(!kcell_pin_fits(7, 3, 8), "8: a line of 10 does not");
	CHECK(!kcell_pin_fits(60, 16, 64), "DejaVu Sans Mono's 76 at 64 does not");
	CHECK(!kcell_pin_fits(1, 0, 0), "no size, no pin");

	/* A name at another pixel size: every size out, the rest kept. */
	at_px("Terminus:pixelsize=32", 48, "Terminus:pixelsize=48");
	at_px("Terminus:size=12:antialias=false", 40,
	      "Terminus:antialias=false:pixelsize=40");
	at_px("Terminus:pixelsize=32:size=24:weight=bold", 56,
	      "Terminus:weight=bold:pixelsize=56");
	at_px("", 24, "monospace:pixelsize=24");
	at_px(NULL, 24, "monospace:pixelsize=24");
	at_px("Terminus:pixelsize=32", 0, NULL);
	CHECK(!kcell_name_at_px("Terminus", 48, small, sizeof(small)),
	      "a name that does not fit is refused");
	/* The base a scaled size counts from, where the name says it. */
	CHECK(kcell_name_pixelsize("Terminus:pixelsize=32") == 32,
	      "pixelsize=32 is 32");

	/* The pinned baseline, from Terminus TTF's measured metrics. */
	CHECK(kcell_pin_ascent(54, 11, 64) == 53, "64: %d", kcell_pin_ascent(54, 11, 64));
	CHECK(kcell_pin_ascent(40, 9, 48) == 39, "48: %d", kcell_pin_ascent(40, 9, 48));
	CHECK(kcell_pin_ascent(34, 7, 40) == 33, "40: %d", kcell_pin_ascent(34, 7, 40));
	CHECK(kcell_pin_ascent(27, 6, 32) == 26, "32 is the strike's 26");
	CHECK(kcell_pin_ascent(20, 5, 24) == 19, "24 is the strike's 19");
	CHECK(kcell_pin_ascent(90, 1, 10) <= 10, "never below the cell");

	/* The loads, where the fonts are. */
	if (!fcft_init(FCFT_LOG_COLORIZE_NEVER, false, FCFT_LOG_CLASS_NONE)) {
		printf("  fontcheck (arithmetic; loads skipped — no fcft)\n");
		return bad;
	}
	int have_ttf = installed("Terminus (TTF):pixelsize=64");
	int have_pcf = installed("Terminus:pixelsize=32");
	fcft_fini();

	if (have_ttf) {
		CHECK(kcell_font_load("Terminus (TTF):pixelsize=64") == 0,
		      "the twin loads");
		CHECK(kcell_w() == 32 && kcell_h() == 64 && kcell_ascent() == 53,
		      "the twin at 64 is pinned to 32x64 at 53, got %dx%d at %d",
		      kcell_w(), kcell_h(), kcell_ascent());
	}
	if (have_ttf && have_pcf) {
		CHECK(kcell_font_load("Terminus:pixelsize=40") == 0, "40 loads");
		CHECK(kcell_w() == 20 && kcell_h() == 40 && kcell_ascent() == 33,
		      "40 is the twin in 20x40 at 33, got %dx%d at %d",
		      kcell_w(), kcell_h(), kcell_ascent());
		/* 40 alone cannot tell the twin from the strike at 1.25,
		 * which is also 20x40 at 33; 36 can, where fontconfig does
		 * not scale and the strike stays 16x32. */
		CHECK(kcell_font_load("Terminus:pixelsize=36") == 0, "36 loads");
		CHECK(kcell_w() == 18 && kcell_h() == 36,
		      "36 is the twin in 18x36, got %dx%d", kcell_w(), kcell_h());
		CHECK(kcell_font_load("Terminus:pixelsize=64") == 0, "64 loads");
		CHECK(kcell_w() == 32 && kcell_h() == 64 && kcell_ascent() == 52,
		      "64 is the 32 strike doubled, 32x64 at 52, got %dx%d at %d",
		      kcell_w(), kcell_h(), kcell_ascent());
		CHECK(kcell_font_load("Terminus:pixelsize=8") == 0, "8 loads");
		CHECK(kcell_h() == 8,
		      "8 is the twin pinned to 8 rows, got %dx%d", kcell_w(),
		      kcell_h());
		CHECK(kcell_font_load("Terminus:pixelsize=32") == 0, "32 loads");
		CHECK(kcell_w() == 16 && kcell_h() == 32 && kcell_ascent() == 26,
		      "32 is the strike, 16x32 at 26, got %dx%d at %d",
		      kcell_w(), kcell_h(), kcell_ascent());
		/* A bitmap is never pinned: where fontconfig answers with the
		 * strike above the asked size, the cell is that strike, since
		 * its ink reaches its last row. */
		CHECK(kcell_font_load("Terminus:pixelsize=31") == 0, "31 loads");
		CHECK(kcell_w() == 16 && kcell_h() == 32 && kcell_ascent() == 26,
		      "31 is the 32 strike, 16x32 at 26, got %dx%d at %d",
		      kcell_w(), kcell_h(), kcell_ascent());
		CHECK(kcell_font_load("Terminus:pixelsize=11") == 0, "11 loads");
		CHECK(kcell_w() == 6 && kcell_h() == 12,
		      "11 is the 12 strike, 6x12, got %dx%d", kcell_w(),
		      kcell_h());
		/* 13 is equidistant from the 12 and 14 strikes; either is
		 * right, a 13-row cut of the 14 is not. */
		CHECK(kcell_font_load("Terminus:pixelsize=13") == 0, "13 loads");
		CHECK(kcell_h() == 12 || kcell_h() == 14,
		      "13 is a whole strike, got %dx%d", kcell_w(), kcell_h());
		/* The device sizes of a fractional output scale over the 32
		 * cell: each is the twin at exactly the scale times 16x32,
		 * which is what lets libkwl size a surface in named cells. */
		static const int S[3][3] = { { 40, 20, 40 }, { 48, 24, 48 },
					     { 56, 28, 56 } };
		for (int i = 0; i < 3; i++) {
			char name[64];

			CHECK(kcell_name_at_px("Terminus:pixelsize=32", S[i][0],
					       name, sizeof(name)) &&
				      kcell_font_load(name) == 0,
			      "%d loads", S[i][0]);
			CHECK(kcell_w() == S[i][1] && kcell_h() == S[i][2],
			      "%d is %dx%d, got %dx%d", S[i][0], S[i][1],
			      S[i][2], kcell_w(), kcell_h());
		}
	}
	kcell_font_free();
	printf("  fontcheck (size policy; loads %s)\n",
	       have_ttf && have_pcf ? "checked"
	       : have_ttf	    ? "of the twin only — no bitmap Terminus"
				    : "skipped — no Terminus (TTF) installed");
	return bad;
}
