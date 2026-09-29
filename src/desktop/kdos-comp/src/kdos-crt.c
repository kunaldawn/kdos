// SPDX-License-Identifier: GPL-2.0-only
/*
 * The CRT pass, ported from the pre-fork kdos-comp — the whole reason
 * KDOS owns a compositor. See that file's header for the full story;
 * the load-bearing facts survive verbatim:
 *
 * - wlroots has NO shader API. The documented seam is
 *   wlr_scene_output_build_state(so, &state, &options) with
 *   options.swapchain: the scene composites the desktop into a buffer
 *   of OURS, and our GLES2 program blits it into the output's real
 *   buffer with the effect applied. Both swapchains come from
 *   wlr_output_configure_primary_swapchain(), so no format guesswork.
 *
 * - THE TEXTURE IS NOT CACHED. wlr_texture_from_buffer() LOCKS the
 *   buffer and a swapchain slot is only reused once its last lock
 *   goes; caching four textures holds all four slots and the fifth
 *   frame gets "No free output buffer slot" and a scene that stops
 *   rendering. Measured, not reasoned about. Import per frame,
 *   destroy after the pass.
 *
 * - TWO HONEST FALLBACKS: a renderer that is not GLES2 gets no pass
 *   at all (pixman is a slideshow); anything failing at runtime puts
 *   that OUTPUT on the plain path for a doubling cooldown (see
 *   give_up()) — this file must never be the reason for a black screen.
 *
 * - Direct scanout is decided per output and per frame, by this file
 *   alone (kdos_crt_scanout()): off while the pass builds its frame,
 *   allowed whenever the frame goes the plain way. A scanout state
 *   carries a CLIENT's buffer plus a dst box, not a picture of the
 *   desktop, so running the pass over one stretches a 13-pixel panel
 *   across the whole screen; a foreign buffer that reaches the pass
 *   anyway is committed unprocessed.
 *
 * - THE PASS REDRAWS ONLY WHAT CHANGED when it may: with the curve at 0
 *   and no degauss or power-down running, an output pixel depends on
 *   its own row and the columns beside it (kdos-crt-pass.h), so the
 *   scene's frame damage, grown by that reach, is what changed on the
 *   output. Each output keeps a buffer-age ring over its out_sc
 *   buffers, and a frame draws the union of the damage since the
 *   buffer it was handed last held a picture. Any commit of a buffer
 *   that is not ours makes every one of them stale, because it
 *   consumed the scene damage this frame would have been told about.
 *
 * - The accent comes from $XDG_CACHE_HOME/kdos/theme (libkcolor's
 *   KCOL_SCHEMES — there is no second copy of the phosphor green) and
 *   kdos_crt_reload() re-reads it on SIGHUP/Reconfigure.
 *
 * Fork adaptations: per-output state lives in this file's own records
 * (destroy listener on the wlr_output), config comes from kdos_conf,
 * and the pre-fork ascii effect (Super+A) is not carried over.
 */
#define _POSIX_C_SOURCE 200809L
#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>
#include <pixman.h>

#include <wlr/render/color.h>
#include <wlr/render/egl.h>
#include <wlr/render/gles2.h>
#include <wlr/render/swapchain.h>
#include <wlr/render/wlr_texture.h>
#include <wlr/types/wlr_damage_ring.h>
#include <wlr/types/wlr_output.h>
#include <wlr/types/wlr_scene.h>
#include <wlr/util/log.h>

#include "kcolor.h"
#include "kdos.h"
#include "kdos-crt-pass.h"
#include "labwc.h"
#include "magnifier.h"
#include "output.h"
#include "view.h"

struct kdos_prog {
	GLuint id;
	GLint a_pos;
	GLint u_tex, u_res, u_int, u_scan, u_curve, u_tint;
	GLint u_fx, u_collapse;
};

struct kdos_crt_gl {
	bool ok;
	struct kdos_prog prog[KDOS_NPROG];
	float tint[3];			/* the accent, 0..1 */
	/* D7.3: an accent change plays 400 ms of decaying wobble — the
	 * degauss thoomp. Amplitude decays CPU-side; 0 means over. */
	int64_t degauss_start_ns;
	/* KDOS_CRT_DUMP=<prefix> writes <prefix>-in.ppm / -out.ppm once;
	 * KDOS_CRT_DUMP_FRAME=<n> waits past the empty first frames. */
	const char *dump;
	long dump_at, frames;
	bool dumped;
};

struct kdos_crt_output {
	struct wl_list link;
	struct output *output;
	struct wl_listener destroy;
	struct wlr_swapchain *scene_sc;
	struct wlr_swapchain *out_sc;
	/*
	 * C7: a failure is a COOLDOWN, not a sentence — a transient commit
	 * error (hotplug link renegotiation, VT switch) would otherwise
	 * untheme the screen for the session. 5 s, doubling per consecutive
	 * trip, capped at 60 s; a completed pass resets the backoff.
	 */
	int64_t broken_until_ns;
	int64_t cooldown_ns;
	bool said, said_scanout;
	/*
	 * What changed on the output since each out_sc buffer last held a
	 * picture, keyed by buffer; a buffer it has never seen is drawn
	 * whole. `committing` marks the pass's own commit so the commit
	 * listener can tell it from every other one.
	 */
	struct wlr_damage_ring ring;
	struct wl_listener commit;
	bool committing;
	/*
	 * The picture on screen is not the pass's last committed frame:
	 * none went out yet, one failed, or someone else committed a
	 * buffer. The next pass frame then commits whole damage as well as
	 * drawing whole, or a display that refreshes only the damage clips
	 * (a PSR2 panel, a nested session) keeps the other picture outside
	 * them. Cleared only by a pass commit that went through.
	 */
	bool stale;
	/* the uniforms the last committed pass frame was drawn with, the
	 * degauss included: any difference changes every pixel */
	float drawn[8];
};

static struct kdos_crt_gl *crt_gl;
/* THIS session refused the pass, and the reason will not change in it.
 * kdos_conf.crt cannot carry that: kdos_conf_reload() re-reads the file and
 * puts the configured value back, so a SIGHUP on pixman would otherwise
 * promise a pass this renderer has already declined. */
static bool crt_declined;
static struct wl_list crt_outputs = { .prev = &crt_outputs,
	.next = &crt_outputs };

static GLuint
compile_one(GLenum type, const char *src)
{
	GLuint sh = glCreateShader(type);
	if (!sh) {
		return 0;
	}
	glShaderSource(sh, 1, &src, NULL);
	glCompileShader(sh);
	GLint ok = GL_FALSE;
	glGetShaderiv(sh, GL_COMPILE_STATUS, &ok);
	if (!ok) {
		char log[1024] = { 0 };
		glGetShaderInfoLog(sh, sizeof(log) - 1, NULL, log);
		wlr_log(WLR_ERROR, "crt: shader failed to compile: %s", log);
		glDeleteShader(sh);
		return 0;
	}
	return sh;
}

static bool
build_prog(struct kdos_prog *p, const char *head)
{
	char frag[4096];
	if (snprintf(frag, sizeof(frag), kdos_crt_frag_fmt, head)
			>= (int)sizeof(frag)) {
		return false;
	}

	GLuint vs = compile_one(GL_VERTEX_SHADER, kdos_crt_vert_src);
	GLuint fs = compile_one(GL_FRAGMENT_SHADER, frag);
	if (!vs || !fs) {
		if (vs) {
			glDeleteShader(vs);
		}
		if (fs) {
			glDeleteShader(fs);
		}
		return false;
	}

	p->id = glCreateProgram();
	glAttachShader(p->id, vs);
	glAttachShader(p->id, fs);
	glLinkProgram(p->id);
	glDetachShader(p->id, vs);
	glDetachShader(p->id, fs);
	glDeleteShader(vs);
	glDeleteShader(fs);

	GLint ok = GL_FALSE;
	glGetProgramiv(p->id, GL_LINK_STATUS, &ok);
	if (!ok) {
		char log[1024] = { 0 };
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
	p->u_fx = glGetUniformLocation(p->id, "u_fx");
	p->u_collapse = glGetUniformLocation(p->id, "u_collapse");
	return p->a_pos >= 0;
}

/*
 * The current EGL context is global state and wlroots restores it
 * around every operation IT performs; raw GL means doing the same by
 * hand or drawing into another library's context.
 */
struct kdos_egl_ctx {
	EGLDisplay ours;
	EGLDisplay dpy;
	EGLContext ctx;
	EGLSurface draw, read;
};

static bool
gl_enter(struct kdos_egl_ctx *sv)
{
	struct wlr_egl *egl = wlr_gles2_renderer_get_egl(server.renderer);
	if (!egl) {
		return false;
	}
	sv->ours = wlr_egl_get_display(egl);
	sv->dpy = eglGetCurrentDisplay();
	sv->ctx = eglGetCurrentContext();
	sv->draw = eglGetCurrentSurface(EGL_DRAW);
	sv->read = eglGetCurrentSurface(EGL_READ);
	return eglMakeCurrent(sv->ours, EGL_NO_SURFACE, EGL_NO_SURFACE,
		wlr_egl_get_context(egl));
}

static void
gl_leave(struct kdos_egl_ctx *sv)
{
	if (sv->dpy == EGL_NO_DISPLAY || sv->ctx == EGL_NO_CONTEXT) {
		eglMakeCurrent(sv->ours, EGL_NO_SURFACE, EGL_NO_SURFACE,
			EGL_NO_CONTEXT);
		return;
	}
	eglMakeCurrent(sv->dpy, sv->draw, sv->read, sv->ctx);
}

/*
 * The scene's direct-scanout switch, and its only writer. The field is
 * scene-GLOBAL while the pass is decided per output, so it is written
 * immediately before every build that reads it: off before the pass's
 * build (kdos_crt_frame(), the power-down), allowed before a plain
 * frame's (handle_output_frame(), once kdos_crt_frame() has declined
 * the frame). The last writer holds until the next build. It is never
 * on while the magnifier is — the magnified inset is drawn into the
 * composited buffer, which a scanned-out client buffer is not — nor
 * when the session started with WLR_SCENE_DISABLE_DIRECT_SCANOUT set
 * (server.direct_scanout_enabled). wlroots still has the last word per
 * frame: a software cursor, a second visible node, a transform or a
 * failed test commit each keep the frame composited.
 */
void
kdos_crt_scanout(bool want)
{
	server.scene->WLR_PRIVATE.direct_scanout = want
		&& server.direct_scanout_enabled && !magnifier_is_enabled();
}

/*
 * Before the backend starts: wlroots reads WLR_NO_HARDWARE_CURSORS once
 * per output, when the output is created, so the cursor plane has to
 * be decided before the first one is.
 */
void
kdos_crt_early_init(void)
{
	/*
	 * C6: the hardware cursor plane is composited by the display
	 * engine AFTER our shader, so under barrel distortion it floats
	 * unwarped over a warped desktop and drifts off the hotspot
	 * toward the edges. Software cursor is drawn into the scene and
	 * warps with everything else. wlroots refuses direct scanout on
	 * an output with a visible software cursor, so with the curve on
	 * a fullscreen client scans out only while the pointer is hidden
	 * or on another output.
	 */
	if (kdos_conf.crt > 0 && kdos_conf.crt_curve > 0) {
		setenv("WLR_NO_HARDWARE_CURSORS", "1", 1);
		wlr_log(WLR_INFO, "crt: software cursor — the hardware plane "
			"cannot follow crt_curve's warp");
	}
}

void
kdos_crt_init(void)
{
	if (kdos_conf.crt <= 0) {
		wlr_log(WLR_INFO, "crt: off (crt = 0)");
		return;
	}

	if (!wlr_renderer_is_gles2(server.renderer)) {
		wlr_log(WLR_INFO, "crt: off — this is not the GLES2 "
			"renderer, and a fullscreen shader on a software "
			"renderer is a slideshow");
		kdos_conf.crt = 0;
		crt_declined = true;
		return;
	}

	struct kdos_crt_gl *gl = calloc(1, sizeof(*gl));
	if (!gl) {
		kdos_conf.crt = 0;
		crt_declined = true;
		return;
	}

	const KcolScheme *sc = kdos_accent_scheme();
	KcolRgb rgb = kcol_rgb(sc ? sc->primary : 0x39ff14);
	gl->tint[0] = rgb.r / 255.0f;
	gl->tint[1] = rgb.g / 255.0f;
	gl->tint[2] = rgb.b / 255.0f;
	gl->dump = getenv("KDOS_CRT_DUMP");
	if (gl->dump && !*gl->dump) {
		gl->dump = NULL;
	}
	gl->dump_at = 1;
	const char *at = getenv("KDOS_CRT_DUMP_FRAME");
	if (at && *at) {
		char *end = NULL;
		long n = strtol(at, &end, 10);
		if (end && !*end && n > 0) {
			gl->dump_at = n;
		}
	}

	struct kdos_egl_ctx sv;
	if (!gl_enter(&sv)) {
		wlr_log(WLR_ERROR, "crt: off — no EGL context");
		free(gl);
		kdos_conf.crt = 0;
		crt_declined = true;
		return;
	}

	bool ok = build_prog(&gl->prog[KDOS_PROG_2D],
		kdos_crt_frag_head[KDOS_PROG_2D]);
	/* the external-image program is optional: without the extension
	 * a dmabuf-backed intermediate falls back per output */
	if (ok && wlr_gles2_renderer_check_ext(server.renderer,
			"GL_OES_EGL_image_external")) {
		build_prog(&gl->prog[KDOS_PROG_EXT],
			kdos_crt_frag_head[KDOS_PROG_EXT]);
	}
	gl_leave(&sv);

	if (!ok) {
		wlr_log(WLR_ERROR, "crt: off — the shader did not build");
		free(gl);
		kdos_conf.crt = 0;
		crt_declined = true;
		return;
	}

	gl->ok = true;
	crt_gl = gl;

	char hex[7];
	kcol_format(sc ? sc->primary : 0x39ff14, hex);
	wlr_log(WLR_INFO, "crt: on — intensity %d, scanlines %d, curve %d, "
		"phosphor #%s (%s)", kdos_conf.crt, kdos_conf.crt_scanlines,
		kdos_conf.crt_curve, hex, sc ? sc->name : kcol_default()->name);
}

/*
 * SIGHUP/Reconfigure: only the tint is re-read, because only the tint
 * came from outside — the accent is a uniform, not a shader constant.
 */
void
kdos_crt_reload(void)
{
	struct kdos_crt_gl *gl = crt_gl;
	if (!gl || !gl->ok) {
		/* a conf reload can only tune a pass that exists: the
		 * shader is built once, at startup, or not at all */
		if (kdos_conf.crt > 0) {
			wlr_log(WLR_INFO, "%s", crt_declined
				? "crt: on in comp.conf, but this session "
					"could not have it"
				: "crt: on in comp.conf but off at startup "
					"— a new session turns it on");
		}
		return;
	}
	const KcolScheme *sc = kdos_accent_scheme();
	KcolRgb rgb = kcol_rgb(sc ? sc->primary : 0x39ff14);
	gl->tint[0] = rgb.r / 255.0f;
	gl->tint[1] = rgb.g / 255.0f;
	gl->tint[2] = rgb.b / 255.0f;
	char hex[7];
	kcol_format(sc ? sc->primary : 0x39ff14, hex);
	wlr_log(WLR_INFO, "crt: phosphor is now #%s (%s)", hex,
		sc ? sc->name : kcol_default()->name);

	/* D7.3: the retint IS the degauss moment — unless motion is off:
	 * the wobble is exactly what a reduce-motion switch is for, and the
	 * retint itself still lands on the next frame */
	if (kdos_conf.motion) {
		gl->degauss_start_ns = kdos_frames_now();
	}

	/* a retint on an idle desktop must not wait for motion */
	struct output *o;
	wl_list_for_each(o, &server.outputs, link) {
		wlr_output_schedule_frame(o->wlr_output);
	}
}

/* the ring first: its entries listen on the buffers the swapchains own */
static void
crt_output_free(struct kdos_crt_output *co)
{
	wl_list_remove(&co->destroy.link);
	wl_list_remove(&co->commit.link);
	wl_list_remove(&co->link);
	wlr_damage_ring_finish(&co->ring);
	if (co->scene_sc) {
		wlr_swapchain_destroy(co->scene_sc);
	}
	if (co->out_sc) {
		wlr_swapchain_destroy(co->out_sc);
	}
	free(co);
}

void
kdos_crt_finish(void)
{
	struct kdos_crt_output *co, *tmp;
	wl_list_for_each_safe(co, tmp, &crt_outputs, link) {
		crt_output_free(co);
	}
	wl_list_init(&crt_outputs);

	struct kdos_crt_gl *gl = crt_gl;
	if (!gl) {
		return;
	}
	struct kdos_egl_ctx sv;
	if (gl_enter(&sv)) {
		for (int i = 0; i < KDOS_NPROG; i++) {
			if (gl->prog[i].id) {
				glDeleteProgram(gl->prog[i].id);
			}
		}
		gl_leave(&sv);
	}
	free(gl);
	crt_gl = NULL;
}

/* ── per-output state ───────────────────────────────────────────── */

static void
handle_crt_output_destroy(struct wl_listener *listener, void *data)
{
	struct kdos_crt_output *co = wl_container_of(listener, co, destroy);
	(void)data;
	crt_output_free(co);
}

/*
 * A buffer committed by anyone but the pass — the plain path under the
 * magnifier, a cooldown or the fullscreen bypass, a scanout buffer — was
 * built from the scene's pending damage and cleared it. The next pass
 * frame is told only what changed after that commit, not after the
 * picture its own buffers hold, so all of them are redrawn whole; and
 * the screen shows that other picture at every pixel, so the next pass
 * commit reports the whole output as damage.
 */
static void
handle_crt_output_commit(struct wl_listener *listener, void *data)
{
	struct kdos_crt_output *co = wl_container_of(listener, co, commit);
	const struct wlr_output_event_commit *ev = data;
	if (!co->committing
			&& (ev->state->committed & WLR_OUTPUT_STATE_BUFFER)) {
		wlr_damage_ring_add_whole(&co->ring);
		co->stale = true;
	}
}

static struct kdos_crt_output *
crt_output_get(struct output *output)
{
	struct kdos_crt_output *co;
	wl_list_for_each(co, &crt_outputs, link) {
		if (co->output == output) {
			return co;
		}
	}
	co = calloc(1, sizeof(*co));
	if (!co) {
		return NULL;
	}
	co->output = output;
	co->stale = true;
	wlr_damage_ring_init(&co->ring);
	co->destroy.notify = handle_crt_output_destroy;
	wl_signal_add(&output->wlr_output->events.destroy, &co->destroy);
	co->commit.notify = handle_crt_output_commit;
	wl_signal_add(&output->wlr_output->events.commit, &co->commit);
	wl_list_insert(&crt_outputs, &co->link);
	return co;
}

/* ── the dump ───────────────────────────────────────────────────── */

/*
 * Rows go out in glReadPixels order, NOT reversed: wlroots renders
 * with a flipped projection, so buffer row 0 already IS the top of
 * the screen. Reversing produced an upside-down lock screen once.
 */
static void
dump_bound(int w, int h, const char *prefix, const char *suffix)
{
	unsigned char *px = malloc((size_t)w * h * 4);
	if (!px) {
		return;
	}
	glReadPixels(0, 0, w, h, GL_RGBA, GL_UNSIGNED_BYTE, px);

	char path[512];
	snprintf(path, sizeof(path), "%s-%s.ppm", prefix, suffix);
	FILE *f = fopen(path, "wb");
	if (f) {
		fprintf(f, "P6\n%d %d\n255\n", w, h);
		for (int y = 0; y < h; y++) {
			for (int x = 0; x < w; x++) {
				fwrite(&px[((size_t)y * w + x) * 4], 1, 3, f);
			}
		}
		fclose(f);
		wlr_log(WLR_INFO, "crt: wrote %s (%dx%d)", path, w, h);
	} else {
		wlr_log(WLR_ERROR, "crt: cannot write %s", path);
	}
	free(px);
}

/* the INPUT is read back through the texture the shader sampled — the
 * buffer it came from is not always renderable */
static void
dump_texture(const struct wlr_gles2_texture_attribs *ta, int w, int h,
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
	if (glCheckFramebufferStatus(GL_FRAMEBUFFER)
			== GL_FRAMEBUFFER_COMPLETE) {
		dump_bound(w, h, prefix, "in");
	} else {
		wlr_log(WLR_ERROR, "crt: the input texture will not attach "
			"to a framebuffer; dumping the output only");
	}
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	glDeleteFramebuffers(1, &fbo);
}

static void
dump_fbo(GLuint fbo, int w, int h, const char *prefix, const char *suffix)
{
	glBindFramebuffer(GL_FRAMEBUFFER, fbo);
	dump_bound(w, h, prefix, suffix);
}

/* ── the frame ──────────────────────────────────────────────────── */

#define KDOS_CRT_COOLDOWN_MIN_NS 5000000000LL	/* 5 s */
#define KDOS_CRT_COOLDOWN_MAX_NS 60000000000LL	/* 60 s */

/*
 * A cooldown, then one retry: 60 identical error lines a second is worse
 * than missing scanlines, but a permanent give-up unthemed a screen for
 * the session over one transient failure. Logged once per trip — this
 * runs at most once per cooldown by construction.
 */
static bool
give_up(struct kdos_crt_output *co, const char *why)
{
	co->cooldown_ns = co->cooldown_ns <= 0 ? KDOS_CRT_COOLDOWN_MIN_NS
		: co->cooldown_ns * 2;
	if (co->cooldown_ns > KDOS_CRT_COOLDOWN_MAX_NS) {
		co->cooldown_ns = KDOS_CRT_COOLDOWN_MAX_NS;
	}
	co->broken_until_ns = kdos_frames_now() + co->cooldown_ns;
	wlr_log(WLR_ERROR, "crt: %s on %s — plain path for %llds",
		why, co->output->wlr_output->name,
		(long long)(co->cooldown_ns / 1000000000LL));
	return false;
}

/*
 * C11: crt_fullscreen = off means a fullscreen topmost view gets its
 * frames without the pass, on the plain path with direct scanout
 * allowed: a client buffer the display can take goes to the screen
 * with no composite at all, and one it cannot is composited once
 * instead of twice. That is the battery and latency lever for video
 * and games, and what makes adaptiveSync and allowTearing reach them.
 */
static bool
output_topmost_is_fullscreen(struct output *output)
{
	struct view *view;
	for_each_view(view, &server.views, LAB_VIEW_CRITERIA_CURRENT_WORKSPACE) {
		if (view->minimized || view->output != output) {
			continue;
		}
		return view->fullscreen;
	}
	return false;
}

/* Whole-output damage, the shape scene-helpers.c already uses — needed
 * when an animation must repaint a scene that has no damage of its own. */
static void
crt_damage_whole(struct wlr_scene_output *so)
{
	pixman_region32_t full;
	pixman_region32_init_rect(&full, 0, 0, so->output->width,
		so->output->height);
	wlr_damage_ring_add(&so->damage_ring, &full);
	pixman_region32_union(&so->WLR_PRIVATE.pending_commit_damage,
		&so->WLR_PRIVATE.pending_commit_damage, &full);
	pixman_region32_fini(&full);
}

/* the power-down owns every output's frames while it runs */
static bool crt_in_powerdown;

bool
kdos_crt_frame(struct output *output, struct wlr_scene_output *so)
{
	struct kdos_crt_gl *gl = crt_gl;

	/* the shutdown animation is drawing; a live frame under it would
	 * paint the desktop back over the collapse */
	if (crt_in_powerdown) {
		return true;
	}
	if (kdos_conf.crt <= 0 || !gl || !gl->ok) {
		return false;
	}
	struct kdos_crt_output *co = crt_output_get(output);
	if (!co || co->broken_until_ns > kdos_frames_now()) {
		return false;
	}
	if (!kdos_conf.crt_fullscreen && output_topmost_is_fullscreen(output)) {
		return false;
	}

	/* D7.3: while the degauss runs, every frame is forced and the
	 * next one scheduled — a static desktop still animates */
	float dg_amp = 0.0f, dg_phase = 0.0f;
	if (gl->degauss_start_ns > 0) {
		double t = (kdos_frames_now() - gl->degauss_start_ns) / 1e9;
		if (t >= 0.0 && t < 0.4) {
			dg_amp = 0.012f * (float)exp(-t * 8.0);
			dg_phase = (float)(t * 40.0);
		} else {
			gl->degauss_start_ns = 0;
		}
	}

	/*
	 * The magnifier and the pass are mutually exclusive, and the whole
	 * frame goes one way or the other. labwc draws the magnified inset
	 * inside lab_wlr_scene_output_commit(), which this path replaces —
	 * so with the pass on, a magnified frame lost its inset whenever
	 * the scene needed redrawing and kept it whenever the scene was
	 * static (a hardware cursor damages nothing), which is a flicker
	 * between two different pictures. Handing the frame back whole is
	 * also the right answer on its own: an accessibility zoom read
	 * through scanlines and a three-tap bleed is harder to read, not
	 * easier. The desktop is unprocessed while the magnifier is on.
	 */
	if (magnifier_is_enabled()) {
		return false;
	}

	/*
	 * Every uniform, so a frame can tell whether the picture on the
	 * output was drawn with the same ones. The frame after the degauss
	 * ends is such a change: it is drawn whole even on a static scene,
	 * or the last wobbled frame stays on screen.
	 */
	const float u[8] = {
		kdos_conf.crt / 100.0f,
		kdos_conf.crt_scanlines / 100.0f,
		kdos_conf.crt_curve / 100.0f,
		gl->tint[0], gl->tint[1], gl->tint[2],
		dg_amp, dg_phase,
	};
	bool retuned = memcmp(u, co->drawn, sizeof(u)) != 0;

	/* nothing changed: nothing to draw — let the plain path take its
	 * early-out. Not while the screen holds a picture that is not the
	 * pass's (`stale`): after a scanned-out or plain frame, the first
	 * frame back on the pass redraws whole even on a static scene, or
	 * the unprocessed picture stays up until something moves. */
	if (!wlr_scene_output_needs_frame(so)) {
		if (dg_amp <= 0.0f && !retuned && !co->stale) {
			return false;
		}
		crt_damage_whole(so);
	}

	struct wlr_output_state probe;
	wlr_output_state_init(&probe);
	bool have = wlr_output_configure_primary_swapchain(output->wlr_output,
			&probe, &co->scene_sc)
		&& wlr_output_configure_primary_swapchain(output->wlr_output,
			&probe, &co->out_sc);
	wlr_output_state_finish(&probe);
	if (!have) {
		return give_up(co, "no swapchain");
	}

	/* ── 1. the desktop, composited into a buffer of ours ─────── */
	kdos_crt_scanout(false);
	struct wlr_output_state scene_state;
	wlr_output_state_init(&scene_state);
	struct wlr_scene_output_state_options opts = {
		.swapchain = co->scene_sc };
	if (!wlr_scene_output_build_state(so, &scene_state, &opts)) {
		wlr_output_state_finish(&scene_state);
		return give_up(co, "the scene would not render");
	}
	if (!(scene_state.committed & WLR_OUTPUT_STATE_BUFFER)) {
		bool ok = wlr_output_commit_state(output->wlr_output,
			&scene_state);
		wlr_output_state_finish(&scene_state);
		return ok;
	}
	/* scanout is off for this build, so this is a net: a foreign
	 * (scanout) buffer is not a picture of the desktop — commit it
	 * unprocessed */
	if (!wlr_swapchain_has_buffer(co->scene_sc, scene_state.buffer)) {
		if (!co->said_scanout) {
			co->said_scanout = true;
			wlr_log(WLR_INFO, "crt: %s took direct scanout — "
				"that frame goes out unprocessed",
				output->wlr_output->name);
		}
		bool ok = wlr_output_commit_state(output->wlr_output,
			&scene_state);
		wlr_output_state_finish(&scene_state);
		return ok;
	}

	struct wlr_buffer *src = wlr_buffer_lock(scene_state.buffer);
	/*
	 * C2: the scene applies wlr-gamma-control as a color transform in
	 * the state it built. Our replacement commit must carry it too, or
	 * night light silently no-ops while the pass is on. The transform
	 * is refcounted; the flag with a NULL transform is a reset and is
	 * propagated as one.
	 */
	bool have_ct = scene_state.committed & WLR_OUTPUT_STATE_COLOR_TRANSFORM;
	struct wlr_color_transform *ct = have_ct && scene_state.color_transform
		? wlr_color_transform_ref(scene_state.color_transform) : NULL;
	/*
	 * What the scene redrew, in buffer pixels: its damage since the
	 * last commit of a buffer. The whole output instead whenever a
	 * pixel can depend on more than its neighbours (the curve, the
	 * degauss's wobble), on something that changed for every pixel at
	 * once (a uniform, the degauss ending), or when the screen does not
	 * hold the pass's last committed frame (`stale`).
	 */
	bool whole = kdos_conf.crt_curve > 0 || dg_amp > 0.0f || retuned
		|| co->stale
		|| !(scene_state.committed & WLR_OUTPUT_STATE_DAMAGE);
	pixman_region32_t scene_dmg;
	pixman_region32_init(&scene_dmg);
	if (!whole) {
		pixman_region32_copy(&scene_dmg, &scene_state.damage);
	}
	wlr_output_state_finish(&scene_state);

	struct wlr_texture *tex = wlr_texture_from_buffer(server.renderer, src);
	if (!tex) {
		wlr_buffer_unlock(src);
		if (ct) {
			wlr_color_transform_unref(ct);
		}
		pixman_region32_fini(&scene_dmg);
		return give_up(co, "the composited buffer will not import "
			"as a texture");
	}

	/* one cleanup path from here on: two buffer locks and three
	 * regions must be let go on every exit */
	const char *err = NULL;
	struct wlr_buffer *dst = NULL;
	pixman_region32_t frame, draw;
	pixman_region32_init(&frame);
	pixman_region32_init(&draw);

	struct wlr_gles2_texture_attribs ta = { 0 };
	wlr_gles2_texture_get_attribs(tex, &ta);
	int which = ta.target == GL_TEXTURE_EXTERNAL_OES ? KDOS_PROG_EXT
		: KDOS_PROG_2D;
	if (!co->said) {
		co->said = true;
		wlr_log(WLR_INFO, "crt: on %s, sampling the composite as %s",
			output->wlr_output->name,
			which == KDOS_PROG_EXT ? "GL_TEXTURE_EXTERNAL_OES"
				: "GL_TEXTURE_2D");
	}

	const struct kdos_prog *p = &gl->prog[which];
	if (!p->id) {
		err = "the composited buffer needs an external sampler and "
			"this driver has none";
		goto fail;
	}

	/* ── 2. our pass, onto the buffer that will be scanned out ── */
	dst = wlr_swapchain_acquire(co->out_sc);
	if (!dst) {
		err = "no free output buffer";
		goto fail;
	}

	GLuint fbo = wlr_gles2_renderer_get_buffer_fbo(server.renderer, dst);
	if (!fbo) {
		err = "no framebuffer for the output buffer";
		goto fail;
	}

	struct kdos_egl_ctx sv;
	if (!gl_enter(&sv)) {
		err = "no EGL context";
		goto fail;
	}

	int w = dst->width, h = dst->height;

	/*
	 * `frame` is what changed on the output since the last frame, and
	 * what the commit reports as damage. `draw` is what changed since
	 * dst last held a picture — frame plus every frame drawn into the
	 * other buffers since, or all of it for a buffer the ring has not
	 * seen — and is what gets redrawn.
	 */
	if (whole) {
		pixman_region32_union_rect(&frame, &frame, 0, 0,
			(unsigned)w, (unsigned)h);
	} else {
		kdos_crt_pass_scope(&frame, &scene_dmg, w, h);
	}
	wlr_damage_ring_add(&co->ring, &frame);
	wlr_damage_ring_rotate_buffer(&co->ring, dst, &draw);

	glBindFramebuffer(GL_FRAMEBUFFER, fbo);
	glViewport(0, 0, w, h);
	glDisable(GL_BLEND);
	glDisable(GL_SCISSOR_TEST);

	glUseProgram(p->id);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(ta.target, ta.tex);
	/* CLAMP_TO_EDGE: the bleed samples a texel either side and the
	 * border must not wrap to the far edge */
	glTexParameteri(ta.target, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(ta.target, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(ta.target, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(ta.target, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

	glUniform1i(p->u_tex, 0);
	glUniform2f(p->u_res, (GLfloat)w, (GLfloat)h);
	glUniform1f(p->u_int, u[0]);
	glUniform1f(p->u_scan, u[1]);
	glUniform1f(p->u_curve, u[2]);
	glUniform3f(p->u_tint, u[3], u[4], u[5]);
	glUniform2f(p->u_fx, u[6], u[7]);
	glUniform2f(p->u_collapse, 1.0f, 1.0f);

	kdos_crt_pass_draw((GLuint)p->a_pos, &draw, w, h);
	glBindTexture(ta.target, 0);
	glUseProgram(0);

	if (gl->dump && !gl->dumped && ++gl->frames >= gl->dump_at) {
		gl->dumped = true;
		dump_texture(&ta, src->width, src->height, gl->dump);
		dump_fbo(fbo, w, h, gl->dump, "out");
	}

	/* implicit dmabuf fencing takes it from here — once the
	 * commands are actually submitted */
	glFlush();
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	gl_leave(&sv);

	/* ── 3. scan it out ─────────────────────────────────────────
	 * The damage is `frame`: it contains the scene's own, which the
	 * commit clears from the scene's pending damage, and it is what a
	 * display with FB_DAMAGE_CLIPS refreshes. */
	struct wlr_output_state st;
	wlr_output_state_init(&st);
	wlr_output_state_set_buffer(&st, dst);
	if (have_ct) {
		wlr_output_state_set_color_transform(&st, ct);
	}
	wlr_output_state_set_damage(&st, &frame);

	co->committing = true;
	bool ok = wlr_output_commit_state(output->wlr_output, &st);
	co->committing = false;
	wlr_output_state_finish(&st);
	if (ok) {
		co->stale = false;
		memcpy(co->drawn, u, sizeof(u));
	} else {
		err = "the output refused the frame";
	}

fail:
	/* dst may have been rotated into the ring and not drawn: no
	 * buffer's history can be trusted after a failed frame, and the
	 * screen still holds whatever was there before it */
	if (err) {
		wlr_damage_ring_add_whole(&co->ring);
		co->stale = true;
	}
	pixman_region32_fini(&scene_dmg);
	pixman_region32_fini(&frame);
	pixman_region32_fini(&draw);
	wlr_texture_destroy(tex);
	if (dst) {
		wlr_buffer_unlock(dst);
	}
	wlr_buffer_unlock(src);
	if (ct) {
		wlr_color_transform_unref(ct);
	}
	if (err) {
		return give_up(co, err);
	}
	/* a whole pass went through: the cooldown backoff starts over */
	co->cooldown_ns = 0;
	if (gl->degauss_start_ns > 0) {
		wlr_output_schedule_frame(output->wlr_output);
	}
	return true;
}

/* ── the power-down ─────────────────────────────────────────────── */

/*
 * D7.4: the signature every retro machine had, closing the loop with
 * the boot splash's power-ON. The last composite collapses vertically
 * to a bright centre line (~350 ms), then horizontally to a dot
 * (~100 ms). Runs AFTER wl_display_run() returns, so the event loop is
 * pumped by hand each frame — DRM page-flip completions arrive through
 * it, and without the pump every commit after the first would find the
 * previous flip still pending. Hard 600 ms deadline: this must never
 * be the reason a shutdown hangs, so anything that fails is skipped,
 * never retried past the clock.
 */
#define KDOS_PD_V_MS		350
#define KDOS_PD_H_MS		100
#define KDOS_PD_DEADLINE_MS	600
#define KDOS_PD_MAX_OUTPUTS	8
#define KDOS_PD_MIN_SCALE	0.006f

struct kdos_pd_output {
	struct kdos_crt_output *co;
	struct wlr_buffer *src;
	struct wlr_texture *tex;
	struct wlr_gles2_texture_attribs ta;
	bool dead;
};

static bool
pd_blit(struct kdos_pd_output *pd, float vs, float hs)
{
	struct kdos_crt_gl *gl = crt_gl;
	struct kdos_crt_output *co = pd->co;
	int which = pd->ta.target == GL_TEXTURE_EXTERNAL_OES ? KDOS_PROG_EXT
		: KDOS_PROG_2D;
	const struct kdos_prog *p = &gl->prog[which];
	if (!p->id) {
		return false;
	}

	struct wlr_buffer *dst = wlr_swapchain_acquire(co->out_sc);
	if (!dst) {
		/* every slot still locked by an unflipped commit — not
		 * fatal, the next iteration tries again */
		return true;
	}
	GLuint fbo = wlr_gles2_renderer_get_buffer_fbo(server.renderer, dst);
	if (!fbo) {
		wlr_buffer_unlock(dst);
		return false;
	}

	struct kdos_egl_ctx sv;
	if (!gl_enter(&sv)) {
		wlr_buffer_unlock(dst);
		return false;
	}

	int w = dst->width, h = dst->height;

	glBindFramebuffer(GL_FRAMEBUFFER, fbo);
	glViewport(0, 0, w, h);
	glDisable(GL_BLEND);
	glDisable(GL_SCISSOR_TEST);

	glUseProgram(p->id);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(pd->ta.target, pd->ta.tex);
	glTexParameteri(pd->ta.target, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(pd->ta.target, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(pd->ta.target, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(pd->ta.target, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

	glUniform1i(p->u_tex, 0);
	glUniform2f(p->u_res, (GLfloat)w, (GLfloat)h);
	glUniform1f(p->u_int, kdos_conf.crt / 100.0f);
	glUniform1f(p->u_scan, kdos_conf.crt_scanlines / 100.0f);
	glUniform1f(p->u_curve, kdos_conf.crt_curve / 100.0f);
	glUniform3f(p->u_tint, gl->tint[0], gl->tint[1], gl->tint[2]);
	glUniform2f(p->u_fx, 0.0f, 0.0f);
	glUniform2f(p->u_collapse, vs, hs);

	/* the collapse moves every pixel: always the whole output */
	kdos_crt_pass_draw((GLuint)p->a_pos, NULL, w, h);
	glBindTexture(pd->ta.target, 0);
	glUseProgram(0);
	glFlush();
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	gl_leave(&sv);

	struct wlr_output_state st;
	wlr_output_state_init(&st);
	wlr_output_state_set_buffer(&st, dst);
	pixman_region32_t full;
	pixman_region32_init_rect(&full, 0, 0, (unsigned)w, (unsigned)h);
	wlr_output_state_set_damage(&st, &full);
	pixman_region32_fini(&full);
	/* a refused commit is a pending flip more often than a dead
	 * output; keep going, the deadline bounds the worst case */
	wlr_output_commit_state(co->output->wlr_output, &st);
	wlr_output_state_finish(&st);
	wlr_buffer_unlock(dst);
	return true;
}

void
kdos_crt_powerdown(void)
{
	struct kdos_crt_gl *gl = crt_gl;
	if (!gl || !gl->ok) {
		return;		/* GLES2-only; off is off */
	}
	if (!kdos_conf.motion) {
		return;		/* the collapse is motion; reduce-motion skips it */
	}

	/* freeze the last composite of every healthy output — a composite:
	 * a scanned-out client buffer is not the desktop, and an output
	 * whose build took one would be left out of the collapse */
	kdos_crt_scanout(false);
	struct kdos_pd_output pd[KDOS_PD_MAX_OUTPUTS];
	int npd = 0;
	struct kdos_crt_output *co;
	wl_list_for_each(co, &crt_outputs, link) {
		if (npd >= KDOS_PD_MAX_OUTPUTS || !co->scene_sc || !co->out_sc
				|| co->broken_until_ns > kdos_frames_now()) {
			continue;
		}
		struct wlr_scene_output *so = co->output->scene_output;
		if (!so) {
			continue;
		}
		crt_damage_whole(so);	/* force a full final render */
		struct wlr_output_state ss;
		wlr_output_state_init(&ss);
		struct wlr_scene_output_state_options opts = {
			.swapchain = co->scene_sc };
		if (!wlr_scene_output_build_state(so, &ss, &opts)
				|| !(ss.committed & WLR_OUTPUT_STATE_BUFFER)
				|| !wlr_swapchain_has_buffer(co->scene_sc,
					ss.buffer)) {
			wlr_output_state_finish(&ss);
			continue;
		}
		struct wlr_buffer *src = wlr_buffer_lock(ss.buffer);
		wlr_output_state_finish(&ss);
		struct wlr_texture *tex = wlr_texture_from_buffer(
			server.renderer, src);
		if (!tex) {
			wlr_buffer_unlock(src);
			continue;
		}
		pd[npd].co = co;
		pd[npd].src = src;
		pd[npd].tex = tex;
		pd[npd].ta = (struct wlr_gles2_texture_attribs) { 0 };
		wlr_gles2_texture_get_attribs(tex, &pd[npd].ta);
		pd[npd].dead = false;
		npd++;
	}
	if (!npd) {
		return;
	}

	crt_in_powerdown = true;
	int64_t start = kdos_frames_now();
	for (;;) {
		int64_t el = (kdos_frames_now() - start) / 1000000;
		if (el > KDOS_PD_V_MS + KDOS_PD_H_MS
				|| el > KDOS_PD_DEADLINE_MS) {
			break;
		}
		float vs, hs;
		if (el <= KDOS_PD_V_MS) {
			vs = 1.0f - (float)el / KDOS_PD_V_MS
				* (1.0f - KDOS_PD_MIN_SCALE);
			hs = 1.0f;
		} else {
			vs = KDOS_PD_MIN_SCALE;
			hs = 1.0f - (float)(el - KDOS_PD_V_MS) / KDOS_PD_H_MS
				* (1.0f - KDOS_PD_MIN_SCALE);
		}
		int alive = 0;
		for (int i = 0; i < npd; i++) {
			if (pd[i].dead) {
				continue;
			}
			if (!pd_blit(&pd[i], vs, hs)) {
				pd[i].dead = true;
			} else {
				alive++;
			}
		}
		/* nothing left to draw on: the rest of the deadline would be
		 * a session that has already ended still holding the loop
		 * open, dispatching sources main() is about to tear down */
		if (!alive) {
			break;
		}
		/* flip completions arrive through the (stopped) loop */
		wl_event_loop_dispatch(
			wl_display_get_event_loop(server.wl_display), 0);
		struct timespec ts = { 0, 16000000L };
		nanosleep(&ts, NULL);
	}

	for (int i = 0; i < npd; i++) {
		wlr_texture_destroy(pd[i].tex);
		wlr_buffer_unlock(pd[i].src);
	}
	crt_in_powerdown = false;
}
