/* SPDX-License-Identifier: MIT */
/*
 * libkcell's own plumbing. NOT INSTALLED AND NOT FOR A CONSUMER: everything a
 * program outside this directory may call is in kcell.h.
 */
#ifndef KCELL_PRIV_H
#define KCELL_PRIV_H

#include <stddef.h>

/*
 * FCFT'S LIFETIME, COUNTED. fcft_init() is not idempotent and fcft_fini() is
 * not refcounted, and two files here bring the library up independently — the
 * cell faces in kcell_font.c and the canvas's arbitrary-size text in
 * kcell_canvas.c. Each holder takes ONE reference and releases it once: the
 * library comes up on the first and goes down on the last, either may come
 * first, and a reference asked for while it is already up costs nothing.
 * kcell_fcft_ref() answers 0 for a machine with no usable FreeType, and the
 * REFUSAL IS LATCHED BY THE CALLER and not here — a bar drawing sixty times a
 * second must not retry the whole library init on every frame.
 *
 * THE DECLARATION LIVES HERE AND NOWHERE ELSE. A second copy beside a caller
 * drifts from this one, and a mismatched prototype for a function whose
 * definition no translation unit can see is a link that succeeds and a call
 * that does not.
 *
 * A CONSUMER NEVER CALLS THE PAIR: a reference taken outside this library
 * matches no face inside it, so nothing could ever release it and FreeType
 * would stand for the life of the process. That is why it is in this header
 * and not in kcell.h.
 */
int kcell_fcft_ref(void);
void kcell_fcft_unref(void);

/*
 * HOW FAR ONE ROW OF A SYNTHESISED ITALIC LEANS, in whole pixels, positive to
 * the right. `row` is counted from the top of the glyph's own mask and `h` is
 * its height, because the shear is about the mask's middle and not about the
 * baseline — see kcell_font.c for why.
 *
 * DECLARED HERE SO IT CAN BE MEASURED. The shear's geometry is arithmetic and
 * is the half of the slant that can be checked without a font, a screen or a
 * frame; a test that re-derived the formula beside it would be a test that
 * agrees with itself while the library drifts.
 */
int kcell_oblique_shift(int row, int h);

/*
 * THE NAME THE CELL FONT WAS ASKED FOR — `Terminus:pixelsize=32` — or "" while
 * none is loaded. It is the family canvas and display text are drawn in when
 * nobody named one with kcell_canvas_font(): text at any size in the chrome's
 * own face, so a heading drawn at twice the cell is the same letters doubled
 * and not fontconfig's idea of `monospace`. Here and not in kcell.h because
 * the one reader is kcell_canvas.c.
 */
const char *kcell_font_name(void);

/*
 * Drop the scratch a picture is scaled through before it is cut into tiles.
 * It is kept between tilings because an animation re-tiles at one size for
 * every frame it plays; nothing but a shutdown or a grid that changed shape
 * needs to ask.
 *
 * DECLARED HERE AND NOT IN kcell.h: both callers are inside this library, and
 * a consumer that dropped the scratch between two frames of one animation
 * would pay the scale again for every frame.
 */
void kcell_tile_forget(void);

/*
 * THE FONT POLICY'S ARITHMETIC, shared by the cell faces and the canvas text
 * and declared here so a fixture can measure it without a font installed.
 *
 * kcell_name_px() is the `:pixelsize=N` a fontconfig name carries, 0 when it
 * carries none: a point size is not comparable with a face's pixel height
 * without the DPI fcft chose, so a name sized in points is taken as it
 * resolves.
 *
 * kcell_face_short() is true when a face resolved for `px` pixels is shorter
 * than that by more than a tenth. A scalable face is never short: its line is
 * at least its em.
 *
 * kcell_strike_off() is true when a bitmap face whose nearest strike is
 * `strike` pixels, which fontconfig will have fcft scale by `fixup`, is not
 * drawn pixel-exact at `px`: a fractional scale (fontconfig's rule for any
 * size more than a fifth from the strike) doubles some rows and columns and
 * not others, and an unscaled strike more than a tenth short is the wrong
 * size. A whole multiple of a strike is exact and is not off. The strike and
 * the factor come from fontconfig's own answer — kcell_bitmap_off_strike() —
 * because fcft reports neither.
 *
 * kcell_name_outline() is false when fontconfig's match for `name` is a
 * bitmap, and true for an outline or a match that does not say. It decides
 * whether a cell may be pinned to the asked size: a bitmap's line is its
 * strike, ink to the last row, and is never cut.
 *
 * kcell_twin_name() writes `<family> (TTF)<rest>` for a name whose first
 * family is not already a `(TTF)` one, and answers 0 otherwise or when it does
 * not fit. It is the name the scalable build of Terminus is published under,
 * and a family with no such twin resolves to fontconfig's substitute, which
 * kcell_face_named() refuses.
 *
 * kcell_face_named() is true when fcft's full name for a face (`Terminus
 * (TTF) Bold`) belongs to the first family `spec` asked for, case-folded as
 * fontconfig compares. fontconfig never fails a match, so this is the only
 * way to tell the twin from whatever stood in for it.
 *
 * kcell_pin_fits() is true when a face whose ascent and descent add up to
 * asc + desc may have its cell pinned to `px` rows: the two exceed `px` by no
 * more than a sixteenth of it, rounded up, so by one row from 1 to 16 pixels.
 * Rounding down would allow nothing below 16, and Terminus TTF, whose line is
 * one row taller than its size (9 at 8), would keep its own taller cell there.
 *
 * kcell_pin_ascent() is the baseline of a face whose line is squeezed to `px`
 * rows: the face's ascent scaled by px / (ascent + descent), rounded, inside
 * the cell. Terminus TTF measured at 24, 32, 48 and 64 pixels gives 19, 26, 39
 * and 53, which at 24 and 32 is the bitmap strike's own ascent.
 */
int kcell_name_px(const char *name);
int kcell_face_short(int height, int px);
int kcell_strike_off(double strike, double fixup, int px);
int kcell_bitmap_off_strike(const char *name, int px);
int kcell_name_outline(const char *name);
int kcell_twin_name(const char *name, char *out, size_t n);
int kcell_face_named(const char *fullname, const char *spec);
int kcell_pin_fits(int ascent, int descent, int px);
int kcell_pin_ascent(int ascent, int descent, int px);

#endif /* KCELL_PRIV_H */
