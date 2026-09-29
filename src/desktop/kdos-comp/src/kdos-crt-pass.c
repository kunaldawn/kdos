// SPDX-License-Identifier: GPL-2.0-only
/*
 * The phosphor pass's shader and the scoped draw. See kdos-crt-pass.h for
 * why this is a file of its own; kdos-crt.c owns everything that touches
 * wlroots — the swapchains, the buffer-age ring, the commit.
 */
#include <stddef.h>

#include "kdos-crt-pass.h"

const char *const kdos_crt_vert_src =
	"attribute vec2 a_pos;\n"
	"varying vec2 v_uv;\n"
	"void main() {\n"
	"	v_uv = a_pos * 0.5 + 0.5;\n"
	"	gl_Position = vec4(a_pos, 0.0, 1.0);\n"
	"}\n";

/*
 * `%s` is the sampler declaration; everything below is shared between
 * the two programs. Every magic number is a proportion of the
 * intensity knob so `crt = 0` is genuinely nothing. The curvature is
 * normalised by the corner displacement (r^2 = 2) so no value crops
 * the desktop — at curve = 100 without the divisor the corners go
 * black and the panel's top row is lost.
 *
 * With u_curve 0, u_fx (0,0) and u_collapse (1,1), an output pixel reads
 * only its own texel and the ones beside it on its row, and everything
 * else it computes comes from its position. That locality is what lets
 * kdos-crt.c redraw only the damaged part of the output; any new effect
 * that reads further, or warps, must widen KDOS_CRT_SCOPE_REACH_* or be
 * added to the whole-output conditions in kdos_crt_frame().
 */
const char *const kdos_crt_frag_fmt =
	"%s"
	"precision highp float;\n"
	"varying vec2 v_uv;\n"
	"uniform vec2 u_res;\n"
	"uniform float u_int;\n"
	"uniform float u_scan;\n"
	"uniform float u_curve;\n"
	"uniform vec3 u_tint;\n"
	/* degauss: x amplitude (UV units), y phase — both 0 when idle */
	"uniform vec2 u_fx;\n"
	/* power-down: vertical/horizontal sample-space scale, (1,1) live */
	"uniform vec2 u_collapse;\n"
	"void main() {\n"
	"	vec2 c = v_uv * 2.0 - 1.0;\n"
	"	float k = u_curve * 0.06;\n"
	"	c *= (1.0 + k * dot(c, c)) / (1.0 + 2.0 * k);\n"
	/* the collapse expands sample space, so everything off the
	 * shrinking band lands in the black branch below */
	"	c = vec2(c.x / u_collapse.y, c.y / u_collapse.x);\n"
	"	vec2 uv = c * 0.5 + 0.5;\n"
	"	uv.x += u_fx.x * sin(uv.y * 90.0 + u_fx.y);\n"
	"	if (uv.x < 0.0 || uv.x > 1.0 || uv.y < 0.0 || uv.y > 1.0) {\n"
	"		gl_FragColor = vec4(0.0, 0.0, 0.0, 1.0);\n"
	"		return;\n"
	"	}\n"
	"	vec3 col = texture2D(u_tex, uv).rgb;\n"
	/* three-tap horizontal bleed, weights 1/4 1/2 1/4 */
	"	vec2 dx = vec2(1.0 / u_res.x, 0.0);\n"
	"	vec3 bleed = texture2D(u_tex, uv + dx).rgb +\n"
	"		     texture2D(u_tex, uv - dx).rgb;\n"
	"	col = mix(col, 0.5 * col + 0.25 * bleed, 0.5 * u_int);\n"
	/* scanlines every third PHYSICAL row — the splash's period */
	"	float row = floor(uv.y * u_res.y);\n"
	"	float line = mod(row, 3.0) < 1.0 ? 1.0 - 0.45 * u_scan : 1.0;\n"
	"	col *= line;\n"
	/* vignette */
	"	vec2 v = uv * 2.0 - 1.0;\n"
	"	col *= 1.0 - 0.15 * u_int * dot(v, v);\n"
	/* degauss lifts the picture a little; the collapse concentrates
	 * a screen's worth of light into the surviving band */
	"	col *= 1.0 + 6.0 * u_fx.x\n"
	"		+ 1.5 * (2.0 - u_collapse.x - u_collapse.y);\n"
	/* the phosphor floor: black is never quite black */
	"	col += u_tint * 0.02 * u_int;\n"
	"	gl_FragColor = vec4(col, 1.0);\n"
	"}\n";

const char *const kdos_crt_frag_head[KDOS_NPROG] = {
	"uniform sampler2D u_tex;\n",
	"#extension GL_OES_EGL_image_external : require\n"
	"uniform samplerExternalOES u_tex;\n",
};

void
kdos_crt_pass_scope(pixman_region32_t *out, const pixman_region32_t *damage,
		int w, int h)
{
	pixman_region32_clear(out);
	int n = 0;
	const pixman_box32_t *b = pixman_region32_rectangles(
		(pixman_region32_t *)damage, &n);
	pixman_box32_t ext;
	if (n > KDOS_CRT_SCOPE_MAX_RECTS) {
		ext = *pixman_region32_extents((pixman_region32_t *)damage);
		b = &ext;
		n = 1;
	}
	for (int i = 0; i < n; i++) {
		pixman_region32_union_rect(out, out,
			b[i].x1 - KDOS_CRT_SCOPE_REACH_X,
			b[i].y1 - KDOS_CRT_SCOPE_REACH_Y,
			(unsigned)(b[i].x2 - b[i].x1 + 2 * KDOS_CRT_SCOPE_REACH_X),
			(unsigned)(b[i].y2 - b[i].y1 + 2 * KDOS_CRT_SCOPE_REACH_Y));
	}
	pixman_region32_intersect_rect(out, out, 0, 0, (unsigned)w,
		(unsigned)h);
	/* growing overlapping rectangles re-bands the region, which can
	 * come out in more pieces than went in: the cap applies again */
	if (pixman_region32_n_rects(out) > KDOS_CRT_SCOPE_MAX_RECTS) {
		ext = *pixman_region32_extents(out);
		pixman_region32_fini(out);
		pixman_region32_init_rect(out, ext.x1, ext.y1,
			(unsigned)(ext.x2 - ext.x1), (unsigned)(ext.y2 - ext.y1));
	}
}

void
kdos_crt_pass_draw(GLuint a_pos, const pixman_region32_t *draw, int w, int h)
{
	static const GLfloat quad[] = {
		-1.0f, -1.0f,  1.0f, -1.0f,  -1.0f, 1.0f,
		 1.0f, -1.0f,  1.0f,  1.0f,  -1.0f, 1.0f,
	};
	glVertexAttribPointer(a_pos, 2, GL_FLOAT, GL_FALSE, 0, quad);
	glEnableVertexAttribArray(a_pos);

	int n = 0;
	const pixman_box32_t *b = draw ? pixman_region32_rectangles(
		(pixman_region32_t *)draw, &n) : NULL;
	pixman_box32_t ext;
	if (draw && n > KDOS_CRT_SCOPE_MAX_RECTS) {
		ext = *pixman_region32_extents((pixman_region32_t *)draw);
		b = &ext;
		n = 1;
	}

	glDisable(GL_SCISSOR_TEST);
	if (!draw || (n == 1 && b[0].x1 <= 0 && b[0].y1 <= 0
			&& b[0].x2 >= w && b[0].y2 >= h)) {
		glDrawArrays(GL_TRIANGLES, 0, 6);
	} else if (n > 0) {
		glEnable(GL_SCISSOR_TEST);
		for (int i = 0; i < n; i++) {
			glScissor(b[i].x1, b[i].y1, b[i].x2 - b[i].x1,
				b[i].y2 - b[i].y1);
			glDrawArrays(GL_TRIANGLES, 0, 6);
		}
		glDisable(GL_SCISSOR_TEST);
	}
	glDisableVertexAttribArray(a_pos);
}
