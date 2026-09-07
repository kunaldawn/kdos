/*
 * kdos-netagent, with the display and the compositor replaced.
 *
 * The agent's wire behaviour is the part that cannot be photographed: the
 * error NAME on a cancel, the flag that must be set before anything is asked,
 * and the exact `a{sa{sv}}` the secret comes back in. This links the real
 * netagent.c against a scripted display so all three can be asserted from a
 * shell script.
 *
 * $KDOS_NETAGENT_KEYS is the keystrokes, one per poll: `\n` is Enter, `\e` is
 * Escape, anything else is itself. Exhausting it reports no further events, so
 * a script that never presses Enter leaves the prompt up and the deadline
 * decides — which is what the timeout case wants.
 *
 * $KDOS_NETAGENT_NOWIN makes kdisp_init fail, which is a session with no
 * compositor.
 *
 * $KDOS_NETAGENT_DUMP writes the first composed frame to stdout as text. The
 * box's layout is not otherwise looked at by anything: a label over the border
 * or a button bar off the right edge is invisible to the compiler and to a
 * test that only reads the bus.
 */

#define _POSIX_C_SOURCE 200809L
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "kdisp.h"
#include "ktui.h"

int netagent_main(int argc, char **argv);

/* ── the display ───────────────────────────────────────────────────────── */

const KDispImpl *const kdos_disp[] = { NULL };
const int kdos_disp_n = 0;

volatile sig_atomic_t sh_theme_dirty;
void sh_theme_from_cache(void) {}
void sh_theme_watch(void) {}
void kch_px_popup(int body_slot) { (void)body_slot; }

static const char *script;
static int wl_fd = -1;

static int dumped;

static void fake_flush(const KtuiCell *cur, KtuiCell *prev, int w, int h, int f)
{
	(void)cur; (void)prev; (void)w; (void)h; (void)f;
	/* ktui_draw_dump() reads the buffer being composed, which is what
	 * ktui_draw_flush() hands a backend as `cur` — so here is the one place
	 * the finished frame can be read without presenting it. */
	if (!dumped && getenv("KDOS_NETAGENT_DUMP")) {
		dumped = 1;
		ktui_draw_dump();
		/* Flushed here: stdout is a file, the dump is under one
		 * buffer, and the harness ends this process with a signal. */
		fflush(stdout);
	}
}

static void fake_size(int *w, int *h) { *w = ktui_w; *h = ktui_h; }
/*
 * NO UTF-8, so the box draws in the ascii glyph tier and the golden is a plain
 * file — the same tier every other committed dump in this tree is in, because
 * ktui_caps is 0 when there is no terminal.
 */
static int fake_caps(void) { return 0; }

static int fake_poll(KtuiEvent *ev, int timeout_ms)
{
	(void)timeout_ms;
	if (!script || !*script)
		return 0;
	memset(ev, 0, sizeof(*ev));
	ev->type = KT_EVT_KEY;
	switch (*script) {
	case '\n': ev->key = KT_K_ENTER; break;
	case '\033': ev->key = KT_K_ESC; break;
	default: ev->key = (unsigned char)*script; break;
	}
	script++;
	return 1;
}

static const KtuiBackend fake_backend = {
	.name = "netagent-fixture",
	.flush = fake_flush,
	.poll_event = fake_poll,
	.size = fake_size,
	.caps = fake_caps,
};

int kdisp_init(const KDispConfig *cfg, const KDispImpl *const *impls, int n)
{
	int fds[2];

	(void)impls; (void)n;
	if (getenv("KDOS_NETAGENT_NOWIN"))
		return -1;
	ktui_w = cfg->cols;
	ktui_h = cfg->rows;
	ktui_backend_set(&fake_backend);
	script = getenv("KDOS_NETAGENT_KEYS");
	/*
	 * A pipe with a byte in it and nothing reading: poll reports the
	 * display readable on every pass, which is what makes the scripted
	 * keys arrive without a real Wayland connection.
	 */
	if (wl_fd < 0 && pipe(fds) == 0) {
		if (write(fds[1], "x", 1) != 1)
			return -1;
		wl_fd = fds[0];
	}
	return 0;
}

void kdisp_shutdown(void) { ktui_backend_set(NULL); }
int kdisp_should_close(void) { return 0; }
int kdisp_fd(void) { return wl_fd; }
void kdisp_pump(void) {}
int kdisp_cell_w(void) { return 16; }
int kdisp_cell_h(void) { return 32; }
int kdisp_scale(void) { return 1; }
int kdisp_px_h(void) { return 0; }
int kdisp_decorated(void) { return 0; }
int kdisp_popup_offset(void) { return 0; }
void kdisp_cursor_set(enum kdisp_cursor c) { (void)c; }
void kdisp_input_cells(const KRect *r, int n) { (void)r; (void)n; }

int main(int argc, char **argv)
{
	return netagent_main(argc, argv);
}
