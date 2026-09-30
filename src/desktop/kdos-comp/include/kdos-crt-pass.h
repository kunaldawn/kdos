/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * The phosphor pass's pure half: the shader text, the damage scoping and the
 * draw. GLES2 and pixman only, no wlroots and no compositor state, so
 * testing/fixtures/crt/scopecheck.c links this same code and compares a
 * damage-scoped run against a whole-output one pixel for pixel. Anything
 * that decides WHICH pixels the pass redraws belongs here, or that check
 * is testing a copy.
 */
#ifndef KDOS_CRT_PASS_H
#define KDOS_CRT_PASS_H

#include <GLES2/gl2.h>
#include <pixman.h>

enum { KDOS_PROG_2D = 0, KDOS_PROG_EXT = 1, KDOS_NPROG = 2 };

extern const char *const kdos_crt_vert_src;
/* printf format; `%s` is kdos_crt_frag_head[program] */
extern const char *const kdos_crt_frag_fmt;
extern const char *const kdos_crt_frag_head[KDOS_NPROG];

/*
 * How far one changed input pixel reaches in the output with the curve at
 * 0 and no degauss or power-down running. The bleed taps the texel either
 * side, so one column each way is the exact reach where samples land on
 * texel centres, and scopecheck measures that on llvmpipe. The second
 * column and the row are margin for a GPU whose interpolated coordinate
 * lands a hair off a centre and takes linear weight from the texel beyond;
 * they cost a few pixels per rectangle. At 0 columns, a change leaves a
 * stale bleed column beside it.
 */
#define KDOS_CRT_SCOPE_REACH_X 2
#define KDOS_CRT_SCOPE_REACH_Y 1

/*
 * Rectangles one frame is drawn in. Past this the region's bounding box is
 * drawn instead: each rectangle is a draw call, and a region shredded into
 * hundreds of slivers costs more in calls than its extents cost in fill.
 */
#define KDOS_CRT_SCOPE_MAX_RECTS 16

/*
 * `out` = the output pixels a scene damage of `damage` changes: every
 * rectangle grown by the reach, clipped to w x h, and collapsed to its
 * bounding box past KDOS_CRT_SCOPE_MAX_RECTS. Coordinates are buffer
 * pixels, row 0 first in memory, which is also GL window row 0 on the
 * pass's framebuffer. `out` must not be `damage`.
 */
void kdos_crt_pass_scope(pixman_region32_t *out,
	const pixman_region32_t *damage, int w, int h);

/*
 * Draw the full-screen quad with the bound program, once per rectangle of
 * `draw` under a scissor; NULL, or one rectangle covering w x h, is one
 * unscissored draw. Pixels outside `draw` keep what the framebuffer held.
 * Leaves GL_SCISSOR_TEST disabled and `a_pos` disabled.
 */
void kdos_crt_pass_draw(GLuint a_pos, const pixman_region32_t *draw,
	int w, int h);

#endif /* KDOS_CRT_PASS_H */
