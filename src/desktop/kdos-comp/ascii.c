/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   ascii.c — Super+A: the whole screen as characters
 *
 * Off by default. On, the composited desktop — application pixels included —
 * is quantised to Terminus glyphs, and the CRT shader then runs on the result,
 * so the effect is phosphor text rather than text with phosphor next to it.
 *
 * THE ALGORITHM IS libkcell's, NOT A SECOND COPY. kcell_ascii.c measures the
 * candidate glyphs and defines the six sample discs, the contrast pass and the
 * distance metric; this file uploads that same table to the GPU and does the
 * same arithmetic in GLSL. When they disagree it is a bug in one of them rather
 * than a difference of opinion, which is the point of `kdos-shot --text` using
 * the CPU half: it is the same picture, made twice.
 *
 * TWO PASSES, NOT THE REFERENCE'S SIX.
 *
 *   1. AT CELL RESOLUTION: six disc samples from the composite, the contrast
 *      pass, then a loop over every candidate glyph, writing the winner's index
 *      into R. A 1080p desktop at a 16x32 cell is 120x33 = 4 000 fragments, so
 *      an 82-iteration loop there is nothing.
 *   2. AT FULL RESOLUTION: read the index (NEAREST — an interpolated glyph
 *      index is a different glyph), look the glyph up in the atlas, and tint it
 *      with the source sampled at the cell centre through LINEAR filtering,
 *      which is a mean colour for free.
 *
 * The reference's other four passes are its directional-contrast refinement.
 * That is polish; this is the part that makes the picture.
 *
 * FALLBACKS, the same two crt.c already enforces and for the same reasons: a
 * renderer that is not GLES2 never gets here at all, and anything that fails at
 * runtime turns the effect off PERMANENTLY rather than logging once a frame.
 * The desktop keeps working; it just stops being made of letters.
 * ---------------------------------
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>

#include "kcell.h"
#include "kdos-comp.h"

struct kc_ascii {
	bool ok, broken;
	GLuint prog_idx, prog_draw;
	GLuint atlas, vectors;
	GLuint fbo, idx_tex;
	/* The result, full resolution. The ASCII pass CANNOT write straight into
	 * the output framebuffer: the CRT shader has to read what it produced,
	 * and a framebuffer is not a texture. Rendering into our own texture is
	 * what lets the phosphor run on the letters rather than beside them. */
	GLuint out_fbo, out_tex;
	int out_w, out_h;
	int idx_w, idx_h;		/* the index texture, in cells */
	int nglyphs;
	int cell_w, cell_h;

	GLint i_pos, i_tex, i_res, i_cell, i_vec, i_n, i_disc;
	GLint d_pos, d_tex, d_idx, d_atlas, d_res, d_cell, d_n, d_mono, d_tint;
};

/* ── shaders ───────────────────────────────────────────────────────────── */

static const char VERT[] =
	"attribute vec2 a_pos;\n"
	"varying vec2 v_uv;\n"
	"void main() {\n"
	"	v_uv = a_pos * 0.5 + 0.5;\n"
	"	gl_Position = vec4(a_pos, 0.0, 1.0);\n"
	"}\n";

/*
 * Pass 1. The disc centres and the radius are the same numbers kcell_ascii.c
 * uses; changing one without the other is what makes the two halves disagree.
 *
 * Nine taps per disc rather than the CPU's full area sum. The CPU walks every
 * pixel inside the ellipse because it can afford to; nine points on a 3x3 grid
 * inside the same ellipse is the same mean to within a rounding error and is
 * 54 taps a cell instead of several hundred.
 */
static const char FRAG_IDX_HEAD[] =
	"precision highp float;\n"
	"varying vec2 v_uv;\n"
	"uniform sampler2D u_tex;\n"
	"uniform sampler2D u_vec;\n"
	"uniform vec2 u_res;\n"
	"uniform vec2 u_cell;\n"
	"uniform float u_n;\n"
	/*
	 * A UNIFORM array, not a const initialiser.
	 *
	 * `const vec2 DISC[6] = vec2[6](...)` is GLSL ES 3.00; this is an ES
	 * 1.00 shader and array constructors do not exist there. It compiles on
	 * a permissive desktop driver and fails on the embedded one, which is
	 * the worst way round — so the values are uploaded from C, from the same
	 * DISC_X/DISC_Y kcell_ascii.c uses.
	 */
	"uniform vec2 u_disc[6];\n"
	"const float R = 0.26;\n"
	"float luma(vec3 c) { return (c.r*30.0 + c.g*59.0 + c.b*11.0) / 100.0; }\n"
	"void main() {\n"
	"	vec2 cell = floor(v_uv * u_res / u_cell);\n"
	"	vec2 base = cell * u_cell;\n"
	"	float s[6];\n"
	"	float m = 0.0;\n"
	"	for (int d = 0; d < 6; d++) {\n"
	"		float acc = 0.0;\n"
	"		for (int j = -1; j <= 1; j++)\n"
	"		for (int i = -1; i <= 1; i++) {\n"
	"			vec2 o = u_disc[d] + vec2(float(i), float(j)) * R * 0.6;\n"
	"			vec2 px = (base + o * u_cell) / u_res;\n"
	"			acc += luma(texture2D(u_tex, px).rgb);\n"
	"		}\n"
	"		s[d] = acc / 9.0;\n"
	"		m = max(m, s[d]);\n"
	"	}\n"
	/* The contrast pass: normalise, square, scale back. Exponent 2, exactly
	 * as kcell_ascii_contrast() does it. */
	"	if (m > 0.0)\n"
	"		for (int d = 0; d < 6; d++) { float v = s[d] / m; s[d] = v*v*m; }\n"
	"	float best = 0.0;\n"
	"	float bestd = -1.0;\n"
	"	for (int i = 0; i < NGLYPH; i++) {\n"
	"		float u = (float(i) + 0.5) / u_n;\n"
	"		vec4 a = texture2D(u_vec, vec2(u, 0.25));\n"
	"		vec4 b = texture2D(u_vec, vec2(u, 0.75));\n"
	"		float g[6];\n"
	"		g[0]=a.r; g[1]=a.g; g[2]=a.b; g[3]=a.a; g[4]=b.r; g[5]=b.g;\n"
	"		float d2 = 0.0;\n"
	"		for (int k = 0; k < 6; k++) { float df = s[k]-g[k]; d2 += df*df; }\n"
	/* Squared distance, never sqrt: it orders identically and costs less. */
	"		if (bestd < 0.0 || d2 < bestd) { bestd = d2; best = float(i); }\n"
	"	}\n"
	"	gl_FragColor = vec4(best / 255.0, 0.0, 0.0, 1.0);\n"
	"}\n";

/*
 * Pass 2. NEAREST on the index texture is not a preference: an interpolated
 * glyph index is a DIFFERENT GLYPH, so a linear filter here would smear the
 * boundary between two cells into whatever candidate happens to sit between
 * them in the table.
 */
static const char FRAG_DRAW[] =
	"precision highp float;\n"
	"varying vec2 v_uv;\n"
	"uniform sampler2D u_tex;\n"
	"uniform sampler2D u_idx;\n"
	"uniform sampler2D u_atlas;\n"
	"uniform vec2 u_res;\n"
	"uniform vec2 u_cell;\n"
	"uniform float u_n;\n"
	"uniform float u_mono;\n"
	"uniform vec3 u_tint;\n"
	"void main() {\n"
	"	vec2 px = v_uv * u_res;\n"
	"	vec2 cell = floor(px / u_cell);\n"
	"	vec2 inside = (px - cell * u_cell) / u_cell;\n"
	"	vec2 icoord = (cell + 0.5) / (u_res / u_cell);\n"
	"	float idx = floor(texture2D(u_idx, icoord).r * 255.0 + 0.5);\n"
	"	vec2 auv = vec2((idx + inside.x) / u_n, inside.y);\n"
	"	float cov = texture2D(u_atlas, auv).a;\n"
	/* The tint is the source at the cell CENTRE, read through a linear
	 * filter — a mean colour for one tap. `ascii_color = accent` replaces it
	 * with the phosphor, which is the monochrome terminal. */
	"	vec3 col = texture2D(u_tex, (cell + 0.5) * u_cell / u_res).rgb;\n"
	"	col = mix(col, u_tint, u_mono);\n"
	"	gl_FragColor = vec4(col * cov, 1.0);\n"
	"}\n";

/* ── GL helpers ────────────────────────────────────────────────────────── */

static GLuint compile(GLenum type, const char *const *src, int n)
{
	GLuint sh = glCreateShader(type);
	glShaderSource(sh, n, src, NULL);
	glCompileShader(sh);
	GLint ok = 0;
	glGetShaderiv(sh, GL_COMPILE_STATUS, &ok);
	if (!ok) {
		char log[1024] = { 0 };
		glGetShaderInfoLog(sh, sizeof(log) - 1, NULL, log);
		wlr_log(WLR_ERROR, "ascii: shader: %s", log);
		glDeleteShader(sh);
		return 0;
	}
	return sh;
}

static GLuint link_prog(const char *const *frag, int nfrag)
{
	const char *v[1] = { VERT };
	GLuint vs = compile(GL_VERTEX_SHADER, v, 1);
	if (!vs)
		return 0;
	GLuint fs = compile(GL_FRAGMENT_SHADER, frag, nfrag);
	if (!fs) {
		glDeleteShader(vs);
		return 0;
	}
	GLuint p = glCreateProgram();
	glAttachShader(p, vs);
	glAttachShader(p, fs);
	glLinkProgram(p);
	glDeleteShader(vs);
	glDeleteShader(fs);

	GLint ok = 0;
	glGetProgramiv(p, GL_LINK_STATUS, &ok);
	if (!ok) {
		char log[1024] = { 0 };
		glGetProgramInfoLog(p, sizeof(log) - 1, NULL, log);
		wlr_log(WLR_ERROR, "ascii: link: %s", log);
		glDeleteProgram(p);
		return 0;
	}
	return p;
}

/*
 * The atlas: every candidate glyph side by side, one cell wide each, as an
 * alpha texture. GL_ALPHA rather than GL_LUMINANCE because coverage IS alpha —
 * the draw pass multiplies the tint by it.
 */
static GLuint build_atlas(int n, int cw, int ch)
{
	uint8_t *pix = calloc(1, (size_t)n * cw * ch);
	if (!pix)
		return 0;

	for (int i = 0; i < n; i++) {
		KCellGlyph g;
		uint32_t cp = kcell_ascii_glyph(i);
		if (cp == ' ' || !kcell_glyph_scaled(cp, 1, &g) || !g.pix)
			continue;
		pixman_format_code_t fmt = pixman_image_get_format(g.pix);
		const uint8_t *src = (const uint8_t *)pixman_image_get_data(g.pix);
		int stride = pixman_image_get_stride(g.pix);
		int bpp = fmt == PIXMAN_a8 ? 1 : 4;

		for (int y = 0; y < g.height; y++) {
			int dy = y + kcell_ascent() - g.y;
			if (dy < 0 || dy >= ch)
				continue;
			for (int x = 0; x < g.width; x++) {
				int dx = x + g.x;
				if (dx < 0 || dx >= cw)
					continue;
				const uint8_t *p = src + (size_t)y * stride +
						   (size_t)x * bpp;
				pix[(size_t)dy * (n * cw) + i * cw + dx] =
					bpp == 1 ? p[0] : p[3];
			}
		}
	}

	GLuint t = 0;
	glGenTextures(1, &t);
	glBindTexture(GL_TEXTURE_2D, t);
	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_ALPHA, n * cw, ch, 0, GL_ALPHA,
		     GL_UNSIGNED_BYTE, pix);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glBindTexture(GL_TEXTURE_2D, 0);
	free(pix);
	return t;
}

/* The shape vectors, N wide and two rows tall: row 0 carries components 0..3
 * and row 1 carries 4 and 5. Six floats do not fit in four channels, and two
 * textures would be two samplers for no gain. */
static GLuint build_vectors(int n)
{
	uint8_t *pix = calloc(1, (size_t)n * 2 * 4);
	if (!pix)
		return 0;

	for (int i = 0; i < n; i++) {
		const float *v = kcell_ascii_vector(i);
		if (!v)
			continue;
		for (int d = 0; d < 4; d++)
			pix[(size_t)i * 4 + d] = (uint8_t)(v[d] * 255.0f + 0.5f);
		pix[(size_t)(n + i) * 4 + 0] = (uint8_t)(v[4] * 255.0f + 0.5f);
		pix[(size_t)(n + i) * 4 + 1] = (uint8_t)(v[5] * 255.0f + 0.5f);
	}

	GLuint t = 0;
	glGenTextures(1, &t);
	glBindTexture(GL_TEXTURE_2D, t);
	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, n, 2, 0, GL_RGBA,
		     GL_UNSIGNED_BYTE, pix);
	/* NEAREST: these are table entries, not a picture. A linear filter here
	 * would silently invent a glyph between two real ones. */
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glBindTexture(GL_TEXTURE_2D, 0);
	free(pix);
	return t;
}

/* ── the public half ───────────────────────────────────────────────────── */

void kc_ascii_init(struct kc_server *s)
{
	if (s->ascii)
		return;
	/* Nothing is built until the effect is asked for. Most sessions never
	 * press Super+A, and an atlas plus two programs is not worth paying for
	 * on the chance. */
	s->ascii = calloc(1, sizeof(*s->ascii));
}

static bool ascii_build(struct kc_server *s)
{
	struct kc_ascii *a = s->ascii;
	if (a->ok)
		return true;
	if (a->broken)
		return false;

	int n = kcell_ascii_init();
	if (n <= 0) {
		wlr_log(WLR_ERROR, "ascii: no font — the effect is unavailable");
		a->broken = true;
		return false;
	}

	a->nglyphs = n;
	a->cell_w = kcell_w();
	a->cell_h = kcell_h();

	/*
	 * The candidate count is baked into the shader as a compile-time
	 * constant. GLES2 requires a loop bound the compiler can see, so a
	 * uniform will not do — and the count depends on which candidates the
	 * loaded font actually has, which is not known until now.
	 */
	char def[64];
	snprintf(def, sizeof(def), "#define NGLYPH %d\n", n);
	const char *idx_src[2] = { def, FRAG_IDX_HEAD };
	const char *draw_src[1] = { FRAG_DRAW };

	a->prog_idx = link_prog(idx_src, 2);
	a->prog_draw = link_prog(draw_src, 1);
	a->atlas = build_atlas(n, a->cell_w, a->cell_h);
	a->vectors = build_vectors(n);
	if (!a->prog_idx || !a->prog_draw || !a->atlas || !a->vectors) {
		a->broken = true;
		return false;
	}

	a->i_pos = glGetAttribLocation(a->prog_idx, "a_pos");
	a->i_tex = glGetUniformLocation(a->prog_idx, "u_tex");
	a->i_vec = glGetUniformLocation(a->prog_idx, "u_vec");
	a->i_res = glGetUniformLocation(a->prog_idx, "u_res");
	a->i_cell = glGetUniformLocation(a->prog_idx, "u_cell");
	a->i_n = glGetUniformLocation(a->prog_idx, "u_n");
	a->i_disc = glGetUniformLocation(a->prog_idx, "u_disc");

	a->d_pos = glGetAttribLocation(a->prog_draw, "a_pos");
	a->d_tex = glGetUniformLocation(a->prog_draw, "u_tex");
	a->d_idx = glGetUniformLocation(a->prog_draw, "u_idx");
	a->d_atlas = glGetUniformLocation(a->prog_draw, "u_atlas");
	a->d_res = glGetUniformLocation(a->prog_draw, "u_res");
	a->d_cell = glGetUniformLocation(a->prog_draw, "u_cell");
	a->d_n = glGetUniformLocation(a->prog_draw, "u_n");
	a->d_mono = glGetUniformLocation(a->prog_draw, "u_mono");
	a->d_tint = glGetUniformLocation(a->prog_draw, "u_tint");

	glGenFramebuffers(1, &a->fbo);
	glGenFramebuffers(1, &a->out_fbo);
	glGenTextures(1, &a->idx_tex);
	glGenTextures(1, &a->out_tex);

	a->ok = true;
	wlr_log(WLR_INFO, "ascii: %d glyphs, %dx%d cell", n, a->cell_w, a->cell_h);
	return true;
}

bool kc_ascii_on(const struct kc_server *s)
{
	return s->ascii && s->ascii_on && !s->ascii->broken;
}

void kc_ascii_toggle(struct kc_server *s)
{
	if (!s->ascii)
		return;
	s->ascii_on = !s->ascii_on;
	wlr_log(WLR_INFO, "ascii: %s", s->ascii_on ? "on" : "off");
	/* Every output has to redraw: nothing else changed, so no damage would
	 * be reported and the screen would keep the picture it had. */
	struct kc_output *o;
	wl_list_for_each(o, &s->outputs, link)
		wlr_output_schedule_frame(o->wlr_output);
}

bool kc_ascii_render(struct kc_server *s, unsigned tex, unsigned target,
		     int w, int h, const float tint[3], unsigned *out_tex)
{
	struct kc_ascii *a = s->ascii;

	if (!a || !s->ascii_on || a->broken)
		return false;
	if (!ascii_build(s))
		return false;
	/*
	 * An external texture cannot be sampled by a plain sampler2D, and the
	 * index pass needs two texture units of its own. Rather than carry a
	 * second pair of programs for the OES case, the effect declines the
	 * frame — the CRT pass then runs on the unmodified composite, which is
	 * the desktop as it was. Declining is not failing, so `broken` is not
	 * set.
	 */
	if (target != GL_TEXTURE_2D)
		return false;

	int cols = w / a->cell_w, rows = h / a->cell_h;
	if (cols < 1 || rows < 1)
		return false;

	static const GLfloat quad[] = {
		-1.0f, -1.0f,  1.0f, -1.0f,  -1.0f, 1.0f,
		 1.0f, -1.0f,  1.0f,  1.0f,  -1.0f, 1.0f,
	};

	/* Reallocated only when the geometry changes — an output does not
	 * resize every frame, and a texture per frame would be the swapchain
	 * mistake crt.c already records. */
	if (a->idx_w != cols || a->idx_h != rows) {
		glBindTexture(GL_TEXTURE_2D, a->idx_tex);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, cols, rows, 0, GL_RGBA,
			     GL_UNSIGNED_BYTE, NULL);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		glBindTexture(GL_TEXTURE_2D, 0);
		a->idx_w = cols;
		a->idx_h = rows;
	}
	if (a->out_w != w || a->out_h != h) {
		glBindTexture(GL_TEXTURE_2D, a->out_tex);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA,
			     GL_UNSIGNED_BYTE, NULL);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		glBindTexture(GL_TEXTURE_2D, 0);
		a->out_w = w;
		a->out_h = h;
	}

	GLint prev_fbo = 0;
	glGetIntegerv(GL_FRAMEBUFFER_BINDING, &prev_fbo);

	/* ── pass 1: the glyph index, at cell resolution ── */
	glBindFramebuffer(GL_FRAMEBUFFER, a->fbo);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
			       GL_TEXTURE_2D, a->idx_tex, 0);
	if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
		wlr_log(WLR_ERROR, "ascii: incomplete FBO — effect off for good");
		a->broken = true;
		glBindFramebuffer(GL_FRAMEBUFFER, (GLuint)prev_fbo);
		return false;
	}

	glViewport(0, 0, cols, rows);
	glDisable(GL_BLEND);
	glUseProgram(a->prog_idx);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, tex);
	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_2D, a->vectors);
	glUniform1i(a->i_tex, 0);
	glUniform1i(a->i_vec, 1);
	glUniform2f(a->i_res, (GLfloat)w, (GLfloat)h);
	glUniform2f(a->i_cell, (GLfloat)a->cell_w, (GLfloat)a->cell_h);
	glUniform1f(a->i_n, (GLfloat)a->nglyphs);
	/* The same six positions libkcell measured the glyphs at. Passed rather
	 * than repeated in the shader source, so there is one place they can be
	 * changed and no way for the two halves to drift. */
	{
		GLfloat disc[KCELL_ASCII_DIM * 2];
		kcell_ascii_discs(disc);
		glUniform2fv(a->i_disc, KCELL_ASCII_DIM, disc);
	}
	glVertexAttribPointer((GLuint)a->i_pos, 2, GL_FLOAT, GL_FALSE, 0, quad);
	glEnableVertexAttribArray((GLuint)a->i_pos);
	glDrawArrays(GL_TRIANGLES, 0, 6);
	glDisableVertexAttribArray((GLuint)a->i_pos);

	/* ── pass 2: the glyphs, at full resolution, into OUR texture ── */
	glBindFramebuffer(GL_FRAMEBUFFER, a->out_fbo);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
			       GL_TEXTURE_2D, a->out_tex, 0);
	if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
		wlr_log(WLR_ERROR, "ascii: incomplete output FBO — effect off");
		a->broken = true;
		glBindFramebuffer(GL_FRAMEBUFFER, (GLuint)prev_fbo);
		return false;
	}
	glViewport(0, 0, w, h);
	glUseProgram(a->prog_draw);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, tex);
	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_2D, a->idx_tex);
	glActiveTexture(GL_TEXTURE2);
	glBindTexture(GL_TEXTURE_2D, a->atlas);
	glUniform1i(a->d_tex, 0);
	glUniform1i(a->d_idx, 1);
	glUniform1i(a->d_atlas, 2);
	glUniform2f(a->d_res, (GLfloat)w, (GLfloat)h);
	glUniform2f(a->d_cell, (GLfloat)a->cell_w, (GLfloat)a->cell_h);
	glUniform1f(a->d_n, (GLfloat)a->nglyphs);
	glUniform1f(a->d_mono, s->ascii_mono ? 1.0f : 0.0f);
	glUniform3f(a->d_tint, tint[0], tint[1], tint[2]);
	glVertexAttribPointer((GLuint)a->d_pos, 2, GL_FLOAT, GL_FALSE, 0, quad);
	glEnableVertexAttribArray((GLuint)a->d_pos);
	glDrawArrays(GL_TRIANGLES, 0, 6);
	glDisableVertexAttribArray((GLuint)a->d_pos);

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, 0);
	glUseProgram(0);
	/* Back to whatever the caller had bound, so the CRT pass draws where it
	 * meant to rather than into our texture. */
	glBindFramebuffer(GL_FRAMEBUFFER, (GLuint)prev_fbo);
	glViewport(0, 0, w, h);
	*out_tex = a->out_tex;
	return true;
}

void kc_ascii_free(struct kc_server *s)
{
	struct kc_ascii *a = s->ascii;
	if (!a)
		return;
	if (a->ok) {
		glDeleteProgram(a->prog_idx);
		glDeleteProgram(a->prog_draw);
		glDeleteTextures(1, &a->atlas);
		glDeleteTextures(1, &a->vectors);
		glDeleteTextures(1, &a->idx_tex);
		glDeleteTextures(1, &a->out_tex);
		glDeleteFramebuffers(1, &a->fbo);
		glDeleteFramebuffers(1, &a->out_fbo);
	}
	free(a);
	s->ascii = NULL;
}
