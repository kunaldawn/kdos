/* SPDX-License-Identifier: MIT */
/*
 * libkcell's own plumbing. NOT INSTALLED AND NOT FOR A CONSUMER: everything a
 * program outside this directory may call is in kcell.h.
 */
#ifndef KCELL_PRIV_H
#define KCELL_PRIV_H

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

#endif /* KCELL_PRIV_H */
