/*
 * The phosphor pass redraws only what changed, and the picture must be the
 * one a whole-output pass would have drawn — pixel for pixel, every frame.
 *
 * kdos-comp keeps a buffer-age ring over its output buffers and redraws, per
 * frame, the scene's damage grown by the pass's reach plus whatever changed
 * since the buffer it was handed last held a picture. A reach one column too
 * small leaves a stale bleed column beside every change; an age off by one
 * frame leaves a whole stale edit on every third frame. Neither is visible to
 * a compiler, and the rig cannot show either: its virtual display puts the
 * compositor on software rendering, where the pass is off.
 *
 * So this runs the real shader (kdos-crt-pass.c, the same object the
 * compositor links) on a surfaceless EGL context — llvmpipe is enough — and
 * the real damage ring (wlroots' own wlr_damage_ring.c, from the port's
 * tarball) over three output buffers handed out in an irregular order. Each
 * frame edits the scene picture in scripted places — single pixels, 16x32
 * cell rows, the four edges, a burst of rectangles past the rectangle cap, an
 * empty frame, a change of intensity, and a commit by someone else that
 * swallows the scene's damage — draws it once whole into a reference buffer
 * and once scoped into the output buffer the ring chose, and compares the
 * two with memcmp.
 *
 *   scopecheck            exit 0 when every frame matches and some frames
 *                         were actually scoped
 *   scopecheck no-reach   the damage is NOT grown by the reach
 *   scopecheck no-age     the buffer's age is ignored (draw = this frame)
 *
 * Both broken modes must exit 1: selftest.sh runs them to prove the
 * comparison can fail, because a check that cannot is not one.
 *
 * Not shipped. testing/selftest.sh compiles it and nothing else does.
 */
#define _POSIX_C_SOURCE 200809L
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES2/gl2.h>
#include <pixman.h>
#include <wayland-server-core.h>

#include <wlr/types/wlr_buffer.h>
#include <wlr/types/wlr_damage_ring.h>

#include "kdos-crt-pass.h"

/* odd on purpose: no edge case hides behind a power of two */
#define W	643
#define H	397
#define NBUF	3
#define FRAMES	96

enum mode { MODE_OK, MODE_NO_REACH, MODE_NO_AGE };

static uint32_t rng = 0x4b444f53u;

static uint32_t
rnd(void)
{
	rng ^= rng << 13;
	rng ^= rng >> 17;
	rng ^= rng << 5;
	return rng;
}

static GLuint
compile(GLenum type, const char *src)
{
	GLuint sh = glCreateShader(type);
	glShaderSource(sh, 1, &src, NULL);
	glCompileShader(sh);
	GLint ok = GL_FALSE;
	glGetShaderiv(sh, GL_COMPILE_STATUS, &ok);
	if (!ok) {
		char log[1024] = { 0 };
		glGetShaderInfoLog(sh, sizeof(log) - 1, NULL, log);
		fprintf(stderr, "scopecheck: shader: %s\n", log);
		exit(2);
	}
	return sh;
}

static GLuint
fbo_tex(GLuint *tex)
{
	glGenTextures(1, tex);
	glBindTexture(GL_TEXTURE_2D, *tex);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, W, H, 0, GL_RGBA,
		GL_UNSIGNED_BYTE, NULL);
	GLuint fbo;
	glGenFramebuffers(1, &fbo);
	glBindFramebuffer(GL_FRAMEBUFFER, fbo);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
		GL_TEXTURE_2D, *tex, 0);
	if (glCheckFramebufferStatus(GL_FRAMEBUFFER)
			!= GL_FRAMEBUFFER_COMPLETE) {
		fprintf(stderr, "scopecheck: framebuffer incomplete\n");
		exit(2);
	}
	return fbo;
}

/* noise, so the bleed has something different on each side of an edge */
static void
paint(uint8_t *px, pixman_region32_t *dmg, int x, int y, int w, int h)
{
	if (x < 0) {
		w += x;
		x = 0;
	}
	if (y < 0) {
		h += y;
		y = 0;
	}
	if (x + w > W) {
		w = W - x;
	}
	if (y + h > H) {
		h = H - y;
	}
	if (w <= 0 || h <= 0) {
		return;
	}
	for (int r = y; r < y + h; r++) {
		for (int c = x; c < x + w; c++) {
			uint32_t v = rnd();
			memcpy(&px[((size_t)r * W + c) * 4], &v, 4);
			px[((size_t)r * W + c) * 4 + 3] = 255;
		}
	}
	if (dmg) {
		pixman_region32_union_rect(dmg, dmg, x, y, (unsigned)w,
			(unsigned)h);
	}
}

int
main(int argc, char **argv)
{
	enum mode mode = MODE_OK;
	if (argc > 1 && !strcmp(argv[1], "no-reach")) {
		mode = MODE_NO_REACH;
	} else if (argc > 1 && !strcmp(argv[1], "no-age")) {
		mode = MODE_NO_AGE;
	} else if (argc > 1) {
		fprintf(stderr, "usage: scopecheck [no-reach|no-age]\n");
		return 2;
	}

	EGLDisplay dpy = eglGetPlatformDisplay(EGL_PLATFORM_SURFACELESS_MESA,
		EGL_DEFAULT_DISPLAY, NULL);
	if (dpy == EGL_NO_DISPLAY || !eglInitialize(dpy, NULL, NULL)) {
		printf("scopecheck: SKIP — no surfaceless EGL display\n");
		return 77;
	}
	eglBindAPI(EGL_OPENGL_ES_API);
	static const EGLint cattr[] = { EGL_CONTEXT_CLIENT_VERSION, 2,
		EGL_NONE };
	EGLContext ctx = eglCreateContext(dpy, EGL_NO_CONFIG_KHR,
		EGL_NO_CONTEXT, cattr);
	if (ctx == EGL_NO_CONTEXT || !eglMakeCurrent(dpy, EGL_NO_SURFACE,
			EGL_NO_SURFACE, ctx)) {
		printf("scopecheck: SKIP — no GLES2 context\n");
		return 77;
	}

	char frag[4096];
	snprintf(frag, sizeof(frag), kdos_crt_frag_fmt,
		kdos_crt_frag_head[KDOS_PROG_2D]);
	GLuint prog = glCreateProgram();
	glAttachShader(prog, compile(GL_VERTEX_SHADER, kdos_crt_vert_src));
	glAttachShader(prog, compile(GL_FRAGMENT_SHADER, frag));
	glLinkProgram(prog);
	GLint linked = GL_FALSE;
	glGetProgramiv(prog, GL_LINK_STATUS, &linked);
	if (!linked) {
		fprintf(stderr, "scopecheck: program did not link\n");
		return 2;
	}
	GLint a_pos = glGetAttribLocation(prog, "a_pos");

	/* the scene's composite: always the whole, correct picture */
	GLuint scene;
	glGenTextures(1, &scene);
	glBindTexture(GL_TEXTURE_2D, scene);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

	GLuint ref_tex, out_tex[NBUF];
	GLuint ref = fbo_tex(&ref_tex);
	GLuint out[NBUF];
	struct wlr_buffer buf[NBUF];
	for (int i = 0; i < NBUF; i++) {
		out[i] = fbo_tex(&out_tex[i]);
		memset(&buf[i], 0, sizeof(buf[i]));
		buf[i].width = W;
		buf[i].height = H;
		wl_signal_init(&buf[i].events.destroy);
		wl_signal_init(&buf[i].events.release);
	}
	struct wlr_damage_ring ring;
	wlr_damage_ring_init(&ring);

	uint8_t *px = malloc((size_t)W * H * 4);
	uint8_t *want = malloc((size_t)W * H * 4);
	uint8_t *got = malloc((size_t)W * H * 4);
	if (!px || !want || !got) {
		return 2;
	}
	paint(px, NULL, 0, 0, W, H);

	float intensity = 0.55f;
	int last = -1, bad_frames = 0, scoped = 0;
	long long drawn_px = 0;
	for (int f = 0; f < FRAMES; f++) {
		pixman_region32_t dmg, frame, draw;
		pixman_region32_init(&dmg);
		pixman_region32_init(&frame);
		pixman_region32_init(&draw);
		bool whole = f == 0;

		switch (f % 12) {
		case 0:	/* one pixel */
			paint(px, &dmg, (int)(rnd() % W), (int)(rnd() % H), 1, 1);
			break;
		case 1:	/* a cell row, the panel's common case */
			paint(px, &dmg, 0, 32 * (int)(rnd() % (H / 32)), W, 32);
			break;
		case 2:	/* the four edges */
			paint(px, &dmg, 0, 40, 1, 7);
			paint(px, &dmg, W - 1, 90, 1, 9);
			paint(px, &dmg, 100, 0, 13, 1);
			paint(px, &dmg, 200, H - 1, 17, 1);
			break;
		case 3:	/* nothing: the frame is drawn for age alone */
			break;
		case 4:	/* past the rectangle cap */
			for (int i = 0; i < 40; i++) {
				paint(px, &dmg, (int)(rnd() % W),
					(int)(rnd() % H), 1 + (int)(rnd() % 5),
					1 + (int)(rnd() % 3));
			}
			break;
		case 5:	/* a glyph cell */
			paint(px, &dmg, 16 * (int)(rnd() % (W / 16)),
				32 * (int)(rnd() % (H / 32)), 16, 32);
			break;
		case 6:	/* someone else committed a buffer: the scene's
			 * damage for it is gone, the ring hears of it */
			paint(px, NULL, (int)(rnd() % W), (int)(rnd() % H), 30,
				20);
			wlr_damage_ring_add_whole(&ring);
			break;
		case 7:	/* two separate cells on one row: two rects */
			paint(px, &dmg, 48, 64, 16, 32);
			paint(px, &dmg, 400, 64, 16, 32);
			break;
		case 8:	/* a uniform changed: every pixel does */
			intensity = intensity > 0.5f ? 0.3f : 0.55f;
			whole = true;
			paint(px, &dmg, 5, 5, 3, 3);
			break;
		default:	/* a window-sized block somewhere */
			paint(px, &dmg, (int)(rnd() % W), (int)(rnd() % H),
				20 + (int)(rnd() % 120), 10 + (int)(rnd() % 80));
			break;
		}

		glBindTexture(GL_TEXTURE_2D, scene);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, W, H, 0, GL_RGBA,
			GL_UNSIGNED_BYTE, px);

		glUseProgram(prog);
		glActiveTexture(GL_TEXTURE0);
		glUniform1i(glGetUniformLocation(prog, "u_tex"), 0);
		glUniform2f(glGetUniformLocation(prog, "u_res"), W, H);
		glUniform1f(glGetUniformLocation(prog, "u_int"), intensity);
		glUniform1f(glGetUniformLocation(prog, "u_scan"), 0.6f);
		glUniform1f(glGetUniformLocation(prog, "u_curve"), 0.0f);
		glUniform3f(glGetUniformLocation(prog, "u_tint"), 0.22f, 1.0f,
			0.08f);
		glUniform2f(glGetUniformLocation(prog, "u_fx"), 0.0f, 0.0f);
		glUniform2f(glGetUniformLocation(prog, "u_collapse"), 1.0f,
			1.0f);
		glViewport(0, 0, W, H);
		glDisable(GL_BLEND);

		/* the reference: the whole output, every frame */
		glBindFramebuffer(GL_FRAMEBUFFER, ref);
		kdos_crt_pass_draw((GLuint)a_pos, NULL, W, H);
		glReadPixels(0, 0, W, H, GL_RGBA, GL_UNSIGNED_BYTE, want);

		/* the swapchain hands out any buffer but the one on screen */
		int b;
		do {
			b = (int)(rnd() % NBUF);
		} while (b == last);
		last = b;

		if (whole) {
			pixman_region32_union_rect(&frame, &frame, 0, 0, W, H);
		} else if (mode == MODE_NO_REACH) {
			pixman_region32_copy(&frame, &dmg);
		} else {
			kdos_crt_pass_scope(&frame, &dmg, W, H);
		}
		wlr_damage_ring_add(&ring, &frame);
		wlr_damage_ring_rotate_buffer(&ring, &buf[b], &draw);
		if (mode == MODE_NO_AGE) {
			pixman_region32_copy(&draw, &frame);
		}

		glBindFramebuffer(GL_FRAMEBUFFER, out[b]);
		kdos_crt_pass_draw((GLuint)a_pos, &draw, W, H);
		glReadPixels(0, 0, W, H, GL_RGBA, GL_UNSIGNED_BYTE, got);

		pixman_box32_t *e = pixman_region32_extents(&draw);
		int n = 0;
		pixman_box32_t *rb = pixman_region32_rectangles(&draw, &n);
		long long area = 0;
		for (int i = 0; i < n; i++) {
			area += (long long)(rb[i].x2 - rb[i].x1)
				* (rb[i].y2 - rb[i].y1);
		}
		if (n > KDOS_CRT_SCOPE_MAX_RECTS) {
			area = (long long)(e->x2 - e->x1) * (e->y2 - e->y1);
		}
		drawn_px += area;
		if (area < (long long)W * H) {
			scoped++;
		}

		if (memcmp(want, got, (size_t)W * H * 4)) {
			long diff = 0;
			int fx = -1, fy = -1;
			for (long i = 0; i < (long)W * H; i++) {
				if (memcmp(&want[i * 4], &got[i * 4], 4)) {
					if (!diff) {
						fx = (int)(i % W);
						fy = (int)(i / W);
					}
					diff++;
				}
			}
			if (bad_frames < 5) {
				printf("scopecheck: frame %d (case %d, buffer "
					"%d): %ld pixels differ, first at "
					"%d,%d\n", f, f % 12, b, diff, fx, fy);
			}
			bad_frames++;
		}
		pixman_region32_fini(&dmg);
		pixman_region32_fini(&frame);
		pixman_region32_fini(&draw);
	}

	printf("scopecheck: %d frames, %d scoped, %.1f%% of the pixels a "
		"whole-output pass draws, %d differ\n", FRAMES, scoped,
		100.0 * (double)drawn_px / ((double)W * H * FRAMES), bad_frames);
	wlr_damage_ring_finish(&ring);
	free(px);
	free(want);
	free(got);
	eglMakeCurrent(dpy, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
	eglDestroyContext(dpy, ctx);
	eglTerminate(dpy);
	if (bad_frames) {
		return 1;
	}
	if (scoped < FRAMES / 2) {
		printf("scopecheck: too few frames were scoped to prove "
			"anything\n");
		return 1;
	}
	return 0;
}
