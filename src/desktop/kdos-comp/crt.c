/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   kdos-comp — the CRT pass
 *
 * The whole reason for owning a compositor. cosmic-comp had no shader hook, so
 * the CRT identity lived in the boot splash, the wallpaper, the TTY palette and
 * nowhere else — the desktop was the one screen of the machine that did not look
 * like the machine.
 *
 * wlroots has NO shader API: `wlr/render/pass.h` offers add_texture and add_rect
 * and that is all, and wlr_scene has three node types and no callback node. What
 * it does have is a documented seam:
 *
 *   wlr_scene_output_build_state(so, &state, &options)
 *
 * whose `options.swapchain` field is documented as "Allows use of a custom
 * swapchain". So the scene composites the desktop into a buffer of OURS, and we
 * then render that buffer as a texture into the output's real buffer with our
 * own GLES2 program in the middle. wlr_scene keeps doing damage tracking,
 * buffer management and direct-scanout bookkeeping for the composite; we own
 * only the final blit.
 *
 * Costs, stated rather than discovered later:
 *
 *   - We are out of wlr_scene_output_commit(), so direct scanout and overlay
 *     planes are no longer chosen for us. A fullscreen video can no longer be
 *     handed straight to the display controller while the pass is on. That is
 *     inherent: a shader over the whole screen means the whole screen goes
 *     through the GPU.
 *   - The pass writes every pixel, so output damage is the full output every
 *     frame. Damage tracking still saves the scene composite, which is the
 *     expensive half; it no longer saves the blit.
 *   - One extra fullscreen buffer per output, and one extra fullscreen
 *     texturing pass.
 *
 * TWO HONEST FALLBACKS, and the order matters:
 *
 *   1. A renderer that is not GLES2 gets NO CRT pass at all — pixman is
 *      software (`make run` with plain virtio-vga, and every headless test),
 *      and a fullscreen post-process on a software renderer is not a look, it
 *      is a slideshow. Logged once at startup, `crt` reported as off.
 *   2. Anything that fails at runtime — a swapchain that will not allocate, a
 *      buffer that will not import as a texture — marks that OUTPUT broken and
 *      permanently returns to wlr_scene_output_commit(). A compositor with no
 *      scanlines is a working desktop; a compositor that renders nothing is a
 *      black screen, and this file must never be the reason for one.
 *
 * SYNCHRONISATION, because it is the one thing that is not obvious. The scene's
 * composite and our pass run in the SAME GLES2 context — wlroots has exactly one
 * — and GL executes a context's commands in order, so sampling the composited
 * buffer immediately after the scene rendered it needs no fence of ours. The
 * commit is the other half: wlr_scene_output_commit() would have attached the
 * scene's wait timeline to the state, and a hand-built state cannot, so the
 * destination buffer relies on implicit dmabuf fencing after glFlush(). That is
 * the standard path for a GL-rendered dmabuf and is what a compositor without
 * explicit sync has always done — but it is the part of this file that only real
 * hardware can confirm, and the symptom of getting it wrong is a torn or
 * half-drawn frame rather than an error.
 *
 * What the shader does NOT do in v1, so nobody goes looking: no phosphor
 * PERSISTENCE (it needs a history buffer and a second blend pass) and no real
 * BLOOM (a separable blur is two more passes at two more resolutions). What is
 * here is a three-tap horizontal bleed, which is the part of a phosphor dot's
 * glow that is visible at desktop scale for a fraction of the fill rate.
 *
 * The colours come from libkcolor, the same `KCOL_SCHEMES` table the boot
 * splash, the TTY palette, the icons and the GTK stylesheet expand — which is
 * the point of the milestone. There is no second copy of the phosphor green.
 * ---------------------------------
 */

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <GLES2/gl2.h>
/* GL_TEXTURE_EXTERNAL_OES and nothing else — a dmabuf import may land on that
 * target rather than GL_TEXTURE_2D, and the base GLES2 header does not name it. */
#include <GLES2/gl2ext.h>
#include <pixman.h>

#include <wlr/render/egl.h>
#include <wlr/render/gles2.h>
#include <wlr/render/swapchain.h>
#include <wlr/render/wlr_texture.h>

#include "kcolor.h"
#include "kdos-comp.h"

/* ── the program ───────────────────────────────────────────────────────────
 *
 * Two programs, not one: a buffer imported from a dmabuf may land on
 * GL_TEXTURE_EXTERNAL_OES rather than GL_TEXTURE_2D, and a samplerExternalOES
 * needs its own declaration and an #extension line. wlroots itself carries the
 * same pair for the same reason. Which one a frame uses is read off the texture,
 * never assumed.
 */

enum { KC_PROG_2D = 0, KC_PROG_EXT = 1, KC_NPROG = 2 };

struct kc_prog {
	GLuint id;
	GLint a_pos;
	GLint u_tex, u_res, u_int, u_scan, u_curve, u_tint;
};

struct kc_crt_gl {
	bool ok;
	struct kc_prog prog[KC_NPROG];
	float tint[3];			/* the accent, 0..1 */
	/*
	 * KDOS_CRT_DUMP=<prefix> writes <prefix>-in.ppm (the composited desktop)
	 * and <prefix>-out.ppm (after the pass) once, and never again. This is
	 * how the pass gets LOOKED AT without a screen — the same reason
	 * kdosbuild has --preview and kdos-splash has `preview`.
	 *
	 * KDOS_CRT_DUMP_FRAME=<n> waits n CRT frames first, because frame 1 of a
	 * fresh session is an empty desktop and an empty desktop proves nothing
	 * about a shader.
	 */
	const char *dump;
	long dump_at, frames;
	bool dumped;
};

struct kc_crt {
	/* The desktop is composited here, and scanned out from the other one.
	 * Both come from wlr_output_configure_primary_swapchain(), so both carry
	 * buffers the output would have accepted directly — no format or
	 * modifier guesswork of ours. */
	struct wlr_swapchain *scene_sc;
	struct wlr_swapchain *out_sc;
	bool broken;
	bool said, said_scanout;	/* the one-time path logs below */
};

static const char *VERT_SRC =
	"attribute vec2 a_pos;\n"
	"varying vec2 v_uv;\n"
	"void main() {\n"
	"	v_uv = a_pos * 0.5 + 0.5;\n"
	"	gl_Position = vec4(a_pos, 0.0, 1.0);\n"
	"}\n";

/*
 * The fragment shader. `%s` is the sampler declaration and the extension line
 * that a samplerExternalOES needs; everything below it is identical between the
 * two programs, deliberately — two copies of a shader is two things to keep in
 * agreement.
 *
 * Every magic number here is a proportion of the intensity knob, so `crt = 0`
 * is genuinely nothing rather than "a bit less".
 */
static const char *FRAG_FMT =
	"%s"
	"precision highp float;\n"
	"varying vec2 v_uv;\n"
	"uniform vec2 u_res;\n"
	"uniform float u_int;\n"
	"uniform float u_scan;\n"
	"uniform float u_curve;\n"
	"uniform vec3 u_tint;\n"
	"void main() {\n"
	/*
	 * Barrel distortion: the tube is not flat. Small on purpose — past a
	 * couple of percent it smears text at the edges, and this desktop is a
	 * character grid.
	 *
	 * Normalised by the displacement AT THE CORNER (r² = 2), which is what
	 * stops the curve cropping the desktop. Without that divisor the edges of
	 * the frame are pushed outside the source and sampled as black: measured
	 * at curve = 100, the top-left 20x20 came back entirely black and the
	 * panel's top row was simply gone. Now the corner maps exactly to the
	 * corner, nothing is lost, and the bulge is in the middle where a tube's
	 * is.
	 */
	"	vec2 c = v_uv * 2.0 - 1.0;\n"
	"	float k = u_curve * 0.06;\n"
	"	c *= (1.0 + k * dot(c, c)) / (1.0 + 2.0 * k);\n"
	"	vec2 uv = c * 0.5 + 0.5;\n"
	"	if (uv.x < 0.0 || uv.x > 1.0 || uv.y < 0.0 || uv.y > 1.0) {\n"
	"		gl_FragColor = vec4(0.0, 0.0, 0.0, 1.0);\n"
	"		return;\n"
	"	}\n"
	"	vec3 col = texture2D(u_tex, uv).rgb;\n"
	/* Three-tap horizontal bleed — a phosphor dot glows into its
	 * neighbours. Weights 1/4, 1/2, 1/4. */
	"	vec2 dx = vec2(1.0 / u_res.x, 0.0);\n"
	"	vec3 bleed = texture2D(u_tex, uv + dx).rgb +\n"
	"		     texture2D(u_tex, uv - dx).rgb;\n"
	"	col = mix(col, 0.5 * col + 0.25 * bleed, 0.5 * u_int);\n"
	/* Scanlines every third PHYSICAL row: the period the boot splash and the
	 * wallpaper already use, so the three agree. In buffer rows rather than
	 * a screen fraction, or the pattern would change with resolution. */
	"	float row = floor(uv.y * u_res.y);\n"
	"	float line = mod(row, 3.0) < 1.0 ? 1.0 - 0.45 * u_scan : 1.0;\n"
	"	col *= line;\n"
	/* Vignette. */
	"	vec2 v = uv * 2.0 - 1.0;\n"
	"	col *= 1.0 - 0.15 * u_int * dot(v, v);\n"
	/* A phosphor screen is never truly black: it carries a faint glow of
	 * its own colour even where nothing is drawn. This is the one place the
	 * accent reaches the whole frame, and it is small. */
	"	col += u_tint * 0.02 * u_int;\n"
	"	gl_FragColor = vec4(col, 1.0);\n"
	"}\n";

static const char *FRAG_HEAD[KC_NPROG] = {
	"uniform sampler2D u_tex;\n",
	"#extension GL_OES_EGL_image_external : require\n"
	"uniform samplerExternalOES u_tex;\n",
};

static GLuint compile_one(GLenum type, const char *src)
{
	GLuint sh = glCreateShader(type);
	if (!sh)
		return 0;
	glShaderSource(sh, 1, &src, NULL);
	glCompileShader(sh);
	GLint ok = GL_FALSE;
	glGetShaderiv(sh, GL_COMPILE_STATUS, &ok);
	if (!ok) {
		char log[1024] = {0};
		glGetShaderInfoLog(sh, sizeof(log) - 1, NULL, log);
		wlr_log(WLR_ERROR, "crt: shader failed to compile: %s", log);
		glDeleteShader(sh);
		return 0;
	}
	return sh;
}

static bool build_prog(struct kc_prog *p, const char *head)
{
	char frag[4096];
	if (snprintf(frag, sizeof(frag), FRAG_FMT, head) >= (int)sizeof(frag))
		return false;

	GLuint vs = compile_one(GL_VERTEX_SHADER, VERT_SRC);
	GLuint fs = compile_one(GL_FRAGMENT_SHADER, frag);
	if (!vs || !fs) {
		if (vs)
			glDeleteShader(vs);
		if (fs)
			glDeleteShader(fs);
		return false;
	}

	p->id = glCreateProgram();
	glAttachShader(p->id, vs);
	glAttachShader(p->id, fs);
	glLinkProgram(p->id);
	/* Attached shaders are deleted here and freed with the program; keeping
	 * them alive buys nothing once the link is done. */
	glDetachShader(p->id, vs);
	glDetachShader(p->id, fs);
	glDeleteShader(vs);
	glDeleteShader(fs);

	GLint ok = GL_FALSE;
	glGetProgramiv(p->id, GL_LINK_STATUS, &ok);
	if (!ok) {
		char log[1024] = {0};
		glGetProgramInfoLog(p->id, sizeof(log) - 1, NULL, log);
		wlr_log(WLR_ERROR, "crt: program failed to link: %s", log);
		glDeleteProgram(p->id);
		p->id = 0;
		return false;
	}

	p->a_pos = glGetAttribLocation(p->id, "a_pos");
	p->u_tex = glGetUniformLocation(p->id, "u_tex");
	p->u_res = glGetUniformLocation(p->id, "u_res");
	p->u_int = glGetUniformLocation(p->id, "u_int");
	p->u_scan = glGetUniformLocation(p->id, "u_scan");
	p->u_curve = glGetUniformLocation(p->id, "u_curve");
	p->u_tint = glGetUniformLocation(p->id, "u_tint");
	return p->a_pos >= 0;
}

/* ── the EGL context ───────────────────────────────────────────────────────
 *
 * The current EGL context is global state, and wlroots' own renderer saves and
 * restores it around every operation it performs. Doing raw GL means doing the
 * same thing by hand: make the renderer's context current, work, put back
 * whatever was there. Not doing this is how a compositor ends up drawing into
 * another library's context and blaming the driver.
 */
struct kc_egl_ctx {
	EGLDisplay ours;
	EGLDisplay dpy;
	EGLContext ctx;
	EGLSurface draw, read;
};

static bool gl_enter(struct kc_server *s, struct kc_egl_ctx *sv)
{
	struct wlr_egl *egl = wlr_gles2_renderer_get_egl(s->renderer);
	if (!egl)
		return false;
	sv->ours = wlr_egl_get_display(egl);
	sv->dpy = eglGetCurrentDisplay();
	sv->ctx = eglGetCurrentContext();
	sv->draw = eglGetCurrentSurface(EGL_DRAW);
	sv->read = eglGetCurrentSurface(EGL_READ);
	return eglMakeCurrent(sv->ours, EGL_NO_SURFACE, EGL_NO_SURFACE,
			      wlr_egl_get_context(egl));
}

static void gl_leave(struct kc_egl_ctx *sv)
{
	/* Nothing was current before: release ours rather than leaving it
	 * current, so no other consumer inherits a context it did not ask for. */
	if (sv->dpy == EGL_NO_DISPLAY || sv->ctx == EGL_NO_CONTEXT) {
		eglMakeCurrent(sv->ours, EGL_NO_SURFACE, EGL_NO_SURFACE,
			       EGL_NO_CONTEXT);
		return;
	}
	eglMakeCurrent(sv->dpy, sv->draw, sv->read, sv->ctx);
}

/* ── init ──────────────────────────────────────────────────────────────── */

/* The accent, from $XDG_CACHE_HOME/kdos/theme — the same one-word file
 * kdos-shell and kdos-appbox's TUI read. Absent is the normal case on a fresh
 * system and means phosphor. */
static const KcolScheme *accent_scheme(void)
{
	char path[512];
	const char *cache = getenv("XDG_CACHE_HOME");
	const char *home = getenv("HOME");
	if (cache && *cache)
		snprintf(path, sizeof(path), "%s/kdos/theme", cache);
	else if (home && *home)
		snprintf(path, sizeof(path), "%s/.cache/kdos/theme", home);
	else
		return kcol_find("phosphor");

	const KcolScheme *sc = NULL;
	FILE *f = fopen(path, "r");
	if (f) {
		char name[64] = {0};
		if (fgets(name, sizeof(name), f)) {
			char *nl = strpbrk(name, "\r\n");
			if (nl)
				*nl = '\0';
			sc = kcol_find(name);
		}
		fclose(f);
	}
	return sc ? sc : kcol_find("phosphor");
}

void kc_crt_init(struct kc_server *s)
{
	if (s->crt_intensity <= 0) {
		wlr_log(WLR_INFO, "crt: off (crt = 0)");
		return;
	}

	/*
	 * Fallback 1, and it is a hard one: no GLES2 renderer, no pass. pixman
	 * is a software renderer and a fullscreen post-process on it costs more
	 * than the entire rest of the frame. Reported rather than silently
	 * ignored — "why are there no scanlines in the VM" deserves an answer in
	 * the log the user already has.
	 */
	if (!wlr_renderer_is_gles2(s->renderer)) {
		wlr_log(WLR_INFO, "crt: off — this is not the GLES2 renderer, "
				  "and a fullscreen shader on a software "
				  "renderer is a slideshow");
		s->crt_intensity = 0;
		return;
	}

	struct kc_crt_gl *gl = calloc(1, sizeof(*gl));
	if (!gl) {
		s->crt_intensity = 0;
		return;
	}

	const KcolScheme *sc = accent_scheme();
	KcolRgb rgb = kcol_rgb(sc ? sc->primary : 0x39ff14);
	gl->tint[0] = rgb.r / 255.0f;
	gl->tint[1] = rgb.g / 255.0f;
	gl->tint[2] = rgb.b / 255.0f;
	gl->dump = getenv("KDOS_CRT_DUMP");
	if (gl->dump && !*gl->dump)
		gl->dump = NULL;
	gl->dump_at = 1;
	const char *at = getenv("KDOS_CRT_DUMP_FRAME");
	if (at && *at) {
		char *end = NULL;
		long n = strtol(at, &end, 10);
		if (end && !*end && n > 0)
			gl->dump_at = n;
	}

	struct kc_egl_ctx sv;
	if (!gl_enter(s, &sv)) {
		wlr_log(WLR_ERROR, "crt: off — no EGL context");
		free(gl);
		s->crt_intensity = 0;
		return;
	}

	bool ok = build_prog(&gl->prog[KC_PROG_2D], FRAG_HEAD[KC_PROG_2D]);
	/* The external-image program is optional: without the extension a
	 * dmabuf-backed intermediate simply cannot be sampled, and that output
	 * falls back rather than the whole session. */
	if (ok && wlr_gles2_renderer_check_ext(s->renderer,
					       "GL_OES_EGL_image_external"))
		build_prog(&gl->prog[KC_PROG_EXT], FRAG_HEAD[KC_PROG_EXT]);
	gl_leave(&sv);

	if (!ok) {
		wlr_log(WLR_ERROR, "crt: off — the shader did not build");
		free(gl);
		s->crt_intensity = 0;
		return;
	}

	gl->ok = true;
	s->crt_gl = gl;

	/*
	 * Direct scanout and this pass are mutually exclusive, and the reason is
	 * not performance. When wlr_scene takes the scanout path it hands the
	 * commit a CLIENT's buffer and a destination box on the output — not a
	 * picture of the desktop — so a post-process that treated it as one would
	 * blow a panel up to fill the screen. wlroots exposes exactly one switch
	 * for it, an environment variable read by wlr_scene_create(), so this is
	 * set here: kc_crt_init() runs before the scene is created for this
	 * reason. Overwritten rather than defaulted, because a session with the
	 * pass on cannot be correct with scanout enabled.
	 */
	setenv("WLR_SCENE_DISABLE_DIRECT_SCANOUT", "1", 1);
	char hex[7];
	kcol_format(sc ? sc->primary : 0x39ff14, hex);
	wlr_log(WLR_INFO, "crt: on — intensity %d, scanlines %d, curve %d, "
			  "phosphor #%s (%s)", s->crt_intensity, s->crt_scan,
		s->crt_curve, hex, sc ? sc->name : "phosphor");
}

/*
 * The accent changed under us — `kdos theme amber` writes the state file and
 * SIGHUPs the session.
 *
 * Only the tint is re-read, because only the tint came from outside: the
 * intensity, the scanlines and the curvature are comp.conf's, and a config
 * reload is a different thing to a retint. Nothing has to be rebuilt — the
 * accent is a uniform, not a shader constant — so this is three floats and a log
 * line, and the next frame carries it.
 */
void kc_crt_reload(struct kc_server *s)
{
	struct kc_crt_gl *gl = s->crt_gl;
	if (!gl || !gl->ok)
		return;
	const KcolScheme *sc = accent_scheme();
	KcolRgb rgb = kcol_rgb(sc ? sc->primary : 0x39ff14);
	gl->tint[0] = rgb.r / 255.0f;
	gl->tint[1] = rgb.g / 255.0f;
	gl->tint[2] = rgb.b / 255.0f;
	char hex[7];
	kcol_format(sc ? sc->primary : 0x39ff14, hex);
	wlr_log(WLR_INFO, "crt: phosphor is now #%s (%s)", hex,
		sc ? sc->name : "phosphor");

	/* The pass renders every frame anyway, but only when something asked for
	 * one. A retint on an idle desktop would otherwise not be seen until
	 * something else moved. */
	struct kc_output *o;
	wl_list_for_each(o, &s->outputs, link)
		wlr_output_schedule_frame(o->wlr_output);
}

void kc_crt_free(struct kc_server *s)
{
	struct kc_crt_gl *gl = s->crt_gl;
	if (!gl)
		return;
	struct kc_egl_ctx sv;
	if (gl_enter(s, &sv)) {
		for (int i = 0; i < KC_NPROG; i++)
			if (gl->prog[i].id)
				glDeleteProgram(gl->prog[i].id);
		gl_leave(&sv);
	}
	free(gl);
	s->crt_gl = NULL;
}

/* ── per-output state ──────────────────────────────────────────────────── */

/*
 * THE TEXTURE IS NOT CACHED, and that is the interesting part of this file.
 *
 * The obvious optimisation is to import each swapchain slot once and keep the
 * texture: a swapchain hands back the same four buffers forever, and importing a
 * dmabuf costs real time on some drivers. It does not work.
 * `wlr_texture_from_buffer()` LOCKS the buffer, and a swapchain slot is only
 * released for reuse when its buffer's last lock goes away. Cache four textures
 * and all four slots are permanently held; the fifth frame gets
 *
 *   [ERROR] [render/swapchain.c:95] No free output buffer slot
 *
 * and the scene stops rendering entirely. That was measured here, not reasoned
 * about, and it is why the import is per frame and the texture is destroyed at
 * the end of the pass.
 */
void kc_crt_output_free(struct kc_output *o)
{
	struct kc_crt *crt = o->crt;
	if (!crt)
		return;
	if (crt->scene_sc)
		wlr_swapchain_destroy(crt->scene_sc);
	if (crt->out_sc)
		wlr_swapchain_destroy(crt->out_sc);
	free(crt);
	o->crt = NULL;
}

/* ── the dump ──────────────────────────────────────────────────────────── */

/*
 * The read-back half, shared by both dumps.
 *
 * The rows go out in the order glReadPixels returns them, which is NOT the
 * textbook answer. GL's framebuffer origin is bottom-left, so the reflex is to
 * reverse them — and doing that produced a dump with the lock screen's text
 * upside down. wlroots renders into these buffers with a flipped projection, so
 * buffer row 0 already IS the top of the screen: the blit is an identity copy in
 * buffer coordinates and the file wants the same order.
 */
static void dump_bound(int w, int h, const char *prefix, const char *suffix)
{
	unsigned char *px = malloc((size_t)w * h * 4);
	if (!px)
		return;
	glReadPixels(0, 0, w, h, GL_RGBA, GL_UNSIGNED_BYTE, px);

	char path[512];
	snprintf(path, sizeof(path), "%s-%s.ppm", prefix, suffix);
	FILE *f = fopen(path, "wb");
	if (f) {
		fprintf(f, "P6\n%d %d\n255\n", w, h);
		for (int y = 0; y < h; y++)
			for (int x = 0; x < w; x++)
				fwrite(&px[((size_t)y * w + x) * 4], 1, 3, f);
		fclose(f);
		wlr_log(WLR_INFO, "crt: wrote %s (%dx%d)", path, w, h);
	} else {
		wlr_log(WLR_ERROR, "crt: cannot write %s", path);
	}
	free(px);
}

/*
 * The INPUT, read back through the very texture the shader samples rather than
 * through the buffer it came from. That is deliberate: the source may be a
 * client's buffer handed over by direct scanout, or one of ours that the driver
 * will not give a framebuffer for, and in both cases the texture exists — it is
 * what the pass just used. An external image cannot be attached to a
 * framebuffer at all, so that one case dumps nothing and says so.
 */
static void dump_texture(const struct wlr_gles2_texture_attribs *ta, int w, int h,
			 const char *prefix)
{
	if (ta->target != GL_TEXTURE_2D) {
		wlr_log(WLR_INFO, "crt: the input is an external image and "
				  "cannot be read back; dumping the output only");
		return;
	}
	GLuint fbo = 0;
	glGenFramebuffers(1, &fbo);
	glBindFramebuffer(GL_FRAMEBUFFER, fbo);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
			       GL_TEXTURE_2D, ta->tex, 0);
	if (glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE)
		dump_bound(w, h, prefix, "in");
	else
		wlr_log(WLR_ERROR, "crt: the input texture will not attach to a "
				   "framebuffer; dumping the output only");
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	glDeleteFramebuffers(1, &fbo);
}

/*
 * One buffer of the pass, as a PPM. glReadPixels is bottom-up and a PPM is
 * top-down, so the rows are written in reverse and the file comes out the way
 * the screen looks.
 */
static void dump_fbo(GLuint fbo, int w, int h, const char *prefix,
		     const char *suffix)
{
	glBindFramebuffer(GL_FRAMEBUFFER, fbo);
	dump_bound(w, h, prefix, suffix);
}

/* ── the frame ─────────────────────────────────────────────────────────── */

/* Everything failed at is one-way: this output goes back to the plain path and
 * stays there. A pass that fails once will fail every frame, and 60 identical
 * error lines a second is worse than the missing scanlines. */
static bool give_up(struct kc_output *o, const char *why)
{
	wlr_log(WLR_ERROR, "crt: %s on %s — falling back to the plain path",
		why, o->wlr_output->name);
	if (o->crt)
		o->crt->broken = true;
	return false;
}

bool kc_crt_frame(struct kc_output *o, struct wlr_scene_output *so)
{
	struct kc_server *s = o->server;
	struct kc_crt_gl *gl = s->crt_gl;

	if (s->crt_intensity <= 0 || !gl || !gl->ok)
		return false;
	if (o->crt && o->crt->broken)
		return false;
	if (!o->crt) {
		o->crt = calloc(1, sizeof(*o->crt));
		if (!o->crt)
			return false;
	}
	struct kc_crt *crt = o->crt;

	/* Nothing changed: the scene would commit nothing, and a CRT pass over
	 * an unchanged frame produces the same pixels at full cost. */
	if (!wlr_scene_output_needs_frame(so))
		return false;

	/*
	 * Both swapchains come from the output's own suitability check, so both
	 * hold buffers the output would have accepted for scanout, at the right
	 * size, with the right format and modifiers. A resolution change is
	 * handled inside: the call reallocates when what it has is unsuitable.
	 */
	struct wlr_output_state probe;
	wlr_output_state_init(&probe);
	bool have = wlr_output_configure_primary_swapchain(o->wlr_output, &probe,
							  &crt->scene_sc) &&
		    wlr_output_configure_primary_swapchain(o->wlr_output, &probe,
							  &crt->out_sc);
	wlr_output_state_finish(&probe);
	if (!have)
		return give_up(o, "no swapchain");

	/* ── 1. the desktop, composited into a buffer of ours ───────────── */
	struct wlr_output_state scene_state;
	wlr_output_state_init(&scene_state);
	struct wlr_scene_output_state_options opts = { .swapchain = crt->scene_sc };
	if (!wlr_scene_output_build_state(so, &scene_state, &opts)) {
		wlr_output_state_finish(&scene_state);
		return give_up(o, "the scene would not render");
	}
	/* build_state is allowed to decide there is nothing to draw. Its state
	 * may still carry other changes, so it is committed as it stands. */
	if (!(scene_state.committed & WLR_OUTPUT_STATE_BUFFER)) {
		bool ok = wlr_output_commit_state(o->wlr_output, &scene_state);
		wlr_output_state_finish(&scene_state);
		return ok;
	}
	/*
	 * DIRECT SCANOUT, and this one is a trap worth spelling out. wlr_scene
	 * still tries it inside build_state, and when it succeeds the state
	 * carries a CLIENT's buffer plus a `buffer_dst_box` saying where on the
	 * output it goes — it is not a picture of the desktop at all. A pass that
	 * assumed otherwise would stretch a 13-pixel-tall panel over the whole
	 * screen, which is exactly what the first version of this file did.
	 *
	 * `kc_crt_init` therefore turns scanout off for the session, and this is
	 * the belt to that braces: a foreign buffer here means the frame cannot be
	 * post-processed, so it is committed exactly as wlr_scene_output_commit()
	 * would have committed it. A frame without scanlines beats a frame with
	 * the wrong thing on it.
	 */
	if (!wlr_swapchain_has_buffer(crt->scene_sc, scene_state.buffer)) {
		if (!crt->said_scanout) {
			crt->said_scanout = true;
			wlr_log(WLR_INFO, "crt: %s took direct scanout — that "
				"frame goes out unprocessed", o->wlr_output->name);
		}
		bool ok = wlr_output_commit_state(o->wlr_output, &scene_state);
		wlr_output_state_finish(&scene_state);
		return ok;
	}

	struct wlr_buffer *src = wlr_buffer_lock(scene_state.buffer);
	wlr_output_state_finish(&scene_state);

	struct wlr_texture *tex = wlr_texture_from_buffer(s->renderer, src);
	if (!tex) {
		wlr_buffer_unlock(src);
		return give_up(o, "the composited buffer will not import as a "
				  "texture");
	}

	/* One cleanup path from here on: two buffer locks have to be let go on
	 * every exit, and four hand-written copies of that is how one of them ends
	 * up missing a line. */
	const char *err = NULL;
	struct wlr_buffer *dst = NULL;

	struct wlr_gles2_texture_attribs ta = {0};
	wlr_gles2_texture_get_attribs(tex, &ta);
	int which = ta.target == GL_TEXTURE_EXTERNAL_OES ? KC_PROG_EXT
							 : KC_PROG_2D;
	/* Once per session, because which of these paths a machine takes is the
	 * first question any CRT bug report has to answer, and the answer is
	 * driver-specific: whether the scene composited into a buffer of ours or
	 * handed us a client's, and whether that buffer sampled as an ordinary
	 * texture or only as an external image. */
	if (!crt->said) {
		crt->said = true;
		wlr_log(WLR_INFO, "crt: on %s, sampling the composite as %s",
			o->wlr_output->name,
			which == KC_PROG_EXT ? "GL_TEXTURE_EXTERNAL_OES"
					     : "GL_TEXTURE_2D");
	}

	const struct kc_prog *p = &gl->prog[which];
	if (!p->id) {
		err = "the composited buffer needs an external sampler and this "
		      "driver has none";
		goto fail;
	}

	/* ── 2. our pass, onto the buffer that will be scanned out ──────── */
	dst = wlr_swapchain_acquire(crt->out_sc);
	if (!dst) {
		err = "no free output buffer";
		goto fail;
	}

	GLuint fbo = wlr_gles2_renderer_get_buffer_fbo(s->renderer, dst);
	if (!fbo) {
		err = "no framebuffer for the output buffer";
		goto fail;
	}

	struct kc_egl_ctx sv;
	if (!gl_enter(s, &sv)) {
		err = "no EGL context";
		goto fail;
	}

	int w = dst->width, h = dst->height;
	static const GLfloat quad[] = {
		-1.0f, -1.0f,  1.0f, -1.0f,  -1.0f, 1.0f,
		 1.0f, -1.0f,  1.0f,  1.0f,  -1.0f, 1.0f,
	};

	glBindFramebuffer(GL_FRAMEBUFFER, fbo);
	glViewport(0, 0, w, h);
	glDisable(GL_BLEND);
	glDisable(GL_SCISSOR_TEST);

	glUseProgram(p->id);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(ta.target, ta.tex);
	/* CLAMP_TO_EDGE and LINEAR: the bleed samples a texel either side, and
	 * at the border that must not wrap round to the far edge. */
	glTexParameteri(ta.target, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(ta.target, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(ta.target, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(ta.target, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

	glUniform1i(p->u_tex, 0);
	glUniform2f(p->u_res, (GLfloat)w, (GLfloat)h);
	glUniform1f(p->u_int, s->crt_intensity / 100.0f);
	glUniform1f(p->u_scan, s->crt_scan / 100.0f);
	glUniform1f(p->u_curve, s->crt_curve / 100.0f);
	glUniform3f(p->u_tint, gl->tint[0], gl->tint[1], gl->tint[2]);

	glVertexAttribPointer((GLuint)p->a_pos, 2, GL_FLOAT, GL_FALSE, 0, quad);
	glEnableVertexAttribArray((GLuint)p->a_pos);
	glDrawArrays(GL_TRIANGLES, 0, 6);
	glDisableVertexAttribArray((GLuint)p->a_pos);
	glBindTexture(ta.target, 0);
	glUseProgram(0);

	if (gl->dump && !gl->dumped && ++gl->frames >= gl->dump_at) {
		gl->dumped = true;
		dump_texture(&ta, src->width, src->height, gl->dump);
		dump_fbo(fbo, w, h, gl->dump, "out");
	}

	/* The commit hands the buffer to the display; the GPU work has to be
	 * ordered before it. Implicit fencing on the dmabuf takes it from here,
	 * but only once the commands have actually been submitted. */
	glFlush();
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	gl_leave(&sv);

	/* ── 3. scan it out ─────────────────────────────────────────────── */
	struct wlr_output_state st;
	wlr_output_state_init(&st);
	wlr_output_state_set_buffer(&st, dst);
	/*
	 * Full-output damage, every frame, and that is not laziness: the pass
	 * reads a texel either side of each pixel and warps the whole frame, so
	 * a one-pixel scene change does not produce a one-pixel output change.
	 * Damage tracking still saved the composite in step 1, which is the
	 * expensive half.
	 */
	pixman_region32_t full;
	pixman_region32_init_rect(&full, 0, 0, (unsigned)w, (unsigned)h);
	wlr_output_state_set_damage(&st, &full);
	pixman_region32_fini(&full);

	bool ok = wlr_output_commit_state(o->wlr_output, &st);
	wlr_output_state_finish(&st);
	if (!ok)
		err = "the output refused the frame";

fail:
	wlr_texture_destroy(tex);
	if (dst)
		wlr_buffer_unlock(dst);
	wlr_buffer_unlock(src);
	if (err)
		return give_up(o, err);
	return true;
}
