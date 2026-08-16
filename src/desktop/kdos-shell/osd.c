/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   kdos-osd — the volume and brightness overlay
 *
 *   ╔════════════════════════════════╗
 *   ║ volume                     62% ║
 *   ║ ██████████████░░░░░░░░░░░░░░░░ ║
 *   ╚════════════════════════════════╝
 *
 * Bound to the media keys, which is why it both CHANGES the value and shows
 * it: a separate "change" tool and "show" tool would have to agree about
 * rounding, about what happens at the ends of the range, and about mute — and
 * they would drift.
 *
 * VOLUME GOES THROUGH ALSA, not pipewire. Hardware volume lives in the ALSA
 * mixer whether or not a sound server is running, so exactly this command works
 * on the desktop and on a bare TTY — and KDOS deliberately has no pipewire on a
 * TTY (see `bb`, and the `audio` group). Going through pipewire would make the
 * media keys a desktop-only feature for no benefit.
 *
 * Brightness is /sys/class/backlight. Writing it needs permission the user may
 * not have, and that is reported rather than silently swallowed — "my
 * brightness keys do nothing" is otherwise unattributable.
 */

#include <alsa/asoundlib.h>
#include <dirent.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/file.h>
#include <time.h>
#include <unistd.h>

#include "kwl.h"
#include "shell.h"

#define OSD_COLS 34
#define OSD_ROWS 4
#define OSD_MS   1200

/* ── ALSA ──────────────────────────────────────────────────────────────── */

/*
 * `Master` on most cards, `PCM` on the ones that have no master — USB audio
 * devices in particular expose only PCM, and a tool that knows about Master
 * alone silently does nothing on them.
 */
static snd_mixer_elem_t *find_elem(snd_mixer_t *h)
{
	static const char *const NAMES[] = { "Master", "PCM", "Speaker",
					     "Headphone" };
	snd_mixer_selem_id_t *sid;
	snd_mixer_selem_id_alloca(&sid);

	for (size_t i = 0; i < sizeof(NAMES) / sizeof(NAMES[0]); i++) {
		snd_mixer_selem_id_set_index(sid, 0);
		snd_mixer_selem_id_set_name(sid, NAMES[i]);
		snd_mixer_elem_t *e = snd_mixer_find_selem(h, sid);
		if (e && snd_mixer_selem_has_playback_volume(e))
			return e;
	}
	return NULL;
}

/*
 * The capture side, for the mic mode. `Capture` is the selem ALSA gives the
 * recording path on essentially every HDA codec; `Mic` covers the USB headsets
 * that name it after the jack instead. A switch with no volume still counts —
 * mute is the half the panel's ●MIC indicator needs a companion for.
 */
static snd_mixer_elem_t *find_cap_elem(snd_mixer_t *h)
{
	static const char *const NAMES[] = { "Capture", "Mic" };
	snd_mixer_selem_id_t *sid;
	snd_mixer_selem_id_alloca(&sid);

	for (size_t i = 0; i < sizeof(NAMES) / sizeof(NAMES[0]); i++) {
		snd_mixer_selem_id_set_index(sid, 0);
		snd_mixer_selem_id_set_name(sid, NAMES[i]);
		snd_mixer_elem_t *e = snd_mixer_find_selem(h, sid);
		if (e && (snd_mixer_selem_has_capture_volume(e) ||
			  snd_mixer_selem_has_capture_switch(e)))
			return e;
	}
	return NULL;
}

/*
 * ALSA prints its own failures, and on a machine with no card it prints EIGHT
 * lines per attempt — `cannot find card '0'`, `Invalid CTL default`, four
 * `No such file or directory`. The panel retries every 30s forever, so a
 * booted VM's session log grew by ~0.8 lines a second with nothing else in
 * it. That is not a cosmetic complaint: the log is where a crash gets
 * diagnosed, and the wallpaper assert that started this investigation was
 * already competing with pages of noise for the reader's attention.
 *
 * The first occurrence is kept, because a real card that fails is worth one
 * line, and everything after it is dropped. Silencing the lot outright would
 * hide the one message that means something.
 */
static void alsa_quiet_handler(const char *file, int line, const char *fn,
			       int err, const char *fmt, ...)
{
	static int spoken;
	(void)file; (void)line; (void)fn; (void)err; (void)fmt;
	if (spoken)
		return;
	spoken = 1;
	fprintf(stderr, "kdos-shell: ALSA reports no usable mixer; "
		"the volume applet stays off (further ALSA messages "
		"suppressed)\n");
}

void sh_alsa_quiet(void)
{
	static int done;
	if (done)
		return;
	done = 1;
	snd_lib_error_set_handler(alsa_quiet_handler);
}

static snd_mixer_t *mixer_open_raw(void)
{
	snd_mixer_t *h = NULL;
	sh_alsa_quiet();
	if (snd_mixer_open(&h, 0) < 0)
		return NULL;
	if (snd_mixer_attach(h, "default") < 0 ||
	    snd_mixer_selem_register(h, NULL, NULL) < 0 ||
	    snd_mixer_load(h) < 0) {
		snd_mixer_close(h);
		return NULL;
	}
	return h;
}

static snd_mixer_t *mixer_open(snd_mixer_elem_t **elem)
{
	snd_mixer_t *h = mixer_open_raw();
	if (!h)
		return NULL;
	*elem = find_elem(h);
	if (!*elem) {
		snd_mixer_close(h);
		return NULL;
	}
	return h;
}

/* Returns the volume percent, or -1 if there is no mixer at all. `muted` is
 * left alone when the element has no mute switch — plenty do not. */
static int volume_get(snd_mixer_elem_t *e, int *muted)
{
	long min = 0, max = 0, val = 0;

	*muted = 0;
	if (snd_mixer_selem_get_playback_volume_range(e, &min, &max) < 0)
		return -1;
	if (max <= min)
		return -1;
	if (snd_mixer_selem_get_playback_volume(e, SND_MIXER_SCHN_FRONT_LEFT,
						&val) < 0)
		return -1;
	if (snd_mixer_selem_has_playback_switch(e)) {
		int on = 1;
		snd_mixer_selem_get_playback_switch(e, SND_MIXER_SCHN_FRONT_LEFT,
						    &on);
		*muted = !on;
	}
	return (int)(((val - min) * 100 + (max - min) / 2) / (max - min));
}

static void volume_set(snd_mixer_elem_t *e, int pct)
{
	long min = 0, max = 0;
	if (pct < 0)
		pct = 0;
	if (pct > 100)
		pct = 100;
	if (snd_mixer_selem_get_playback_volume_range(e, &min, &max) < 0)
		return;
	long val = min + (max - min) * pct / 100;
	snd_mixer_selem_set_playback_volume_all(e, val);
	/*
	 * Raising the volume unmutes. A key that increases a number you cannot
	 * hear is the single most common complaint about media keys, and the
	 * fix is one line.
	 */
	if (pct > 0 && snd_mixer_selem_has_playback_switch(e))
		snd_mixer_selem_set_playback_switch_all(e, 1);
}

/* ── the mixer as the PANEL sees it ────────────────────────────────────────
 *
 * Opened once and kept, because the panel asks every second and
 * open+attach+register+load per tick is a mixer rebuilt sixty times a minute
 * to read one number. `snd_mixer_handle_events` is what makes the cached
 * handle honest: it drains the card's own change notifications, so a volume
 * changed by amixer or by an alien app shows up here.
 */
static snd_mixer_t *g_mixer;
static snd_mixer_elem_t *g_elem;

static int mixer_ensure(void)
{
	/*
	 * A machine with no sound card is the case this guard exists for: the
	 * panel asks once a second, and without it every one of those seconds
	 * pays for an ALSA open, an attach and a failure. Thirty seconds is
	 * long enough to cost nothing and short enough that a card whose module
	 * loads late still lights the applet up on its own.
	 */
	static time_t last_try;

	if (g_mixer) {
		snd_mixer_handle_events(g_mixer);
		return 0;
	}
	time_t now = time(NULL);
	if (last_try && now - last_try < 30)
		return -1;
	last_try = now;
	g_mixer = mixer_open(&g_elem);
	return g_mixer ? 0 : -1;
}

int sh_volume_get(int *muted)
{
	int m = 0;
	if (mixer_ensure() != 0)
		return -1;
	int pct = volume_get(g_elem, &m);
	if (muted)
		*muted = m;
	return pct;
}

void sh_volume_set(int pct)
{
	if (mixer_ensure() == 0)
		volume_set(g_elem, pct);
}

void sh_volume_toggle(void)
{
	int m = 0;
	if (mixer_ensure() != 0)
		return;
	if (volume_get(g_elem, &m) < 0 ||
	    !snd_mixer_selem_has_playback_switch(g_elem))
		return;
	snd_mixer_selem_set_playback_switch_all(g_elem, m);
}

/* ── the microphone ────────────────────────────────────────────────────────
 *
 * Opened per invocation rather than cached: the mic mode is a keypress, not
 * the panel's once-a-second read, so there is nothing here worth keeping a
 * handle alive for.
 */

/* Returns the capture percent, or -1 when there is no capture mixer at all.
 * An element with a switch but no volume answers 100 — mute is then the only
 * state it has, and a bar at zero while live would read as muted. */
static int mic_get(int *muted)
{
	snd_mixer_t *h = mixer_open_raw();
	int pct = -1;

	*muted = 0;
	if (!h)
		return -1;
	snd_mixer_elem_t *e = find_cap_elem(h);
	if (e) {
		long min = 0, max = 0, val = 0;
		pct = 100;
		if (snd_mixer_selem_has_capture_volume(e) &&
		    snd_mixer_selem_get_capture_volume_range(e, &min, &max) == 0 &&
		    max > min &&
		    snd_mixer_selem_get_capture_volume(e,
						       SND_MIXER_SCHN_FRONT_LEFT,
						       &val) == 0)
			pct = (int)(((val - min) * 100 + (max - min) / 2) /
				    (max - min));
		if (snd_mixer_selem_has_capture_switch(e)) {
			int on = 1;
			snd_mixer_selem_get_capture_switch(e,
							   SND_MIXER_SCHN_FRONT_LEFT,
							   &on);
			*muted = !on;
		}
	}
	snd_mixer_close(h);
	return pct;
}

/* delta 0 toggles the switch; a delta moves the volume and, like the playback
 * side, unmutes on the way up — a key that raises a number nobody can hear is
 * the media-key complaint, capture edition. */
static void mic_adjust(int delta)
{
	snd_mixer_t *h = mixer_open_raw();
	if (!h)
		return;
	snd_mixer_elem_t *e = find_cap_elem(h);
	if (!e) {
		snd_mixer_close(h);
		return;
	}
	if (delta == 0) {
		if (snd_mixer_selem_has_capture_switch(e)) {
			int on = 1;
			snd_mixer_selem_get_capture_switch(e,
							   SND_MIXER_SCHN_FRONT_LEFT,
							   &on);
			snd_mixer_selem_set_capture_switch_all(e, !on);
		}
	} else if (snd_mixer_selem_has_capture_volume(e)) {
		long min = 0, max = 0, val = 0;
		if (snd_mixer_selem_get_capture_volume_range(e, &min, &max) == 0 &&
		    max > min &&
		    snd_mixer_selem_get_capture_volume(e,
						       SND_MIXER_SCHN_FRONT_LEFT,
						       &val) == 0) {
			int pct = (int)(((val - min) * 100 + (max - min) / 2) /
					(max - min)) + delta;
			if (pct < 0)
				pct = 0;
			if (pct > 100)
				pct = 100;
			snd_mixer_selem_set_capture_volume_all(e,
					min + (max - min) * pct / 100);
			if (pct > 0 && snd_mixer_selem_has_capture_switch(e))
				snd_mixer_selem_set_capture_switch_all(e, 1);
		}
	}
	snd_mixer_close(h);
}

/* ── backlight ─────────────────────────────────────────────────────────── */

static int backlight_path(char *buf, size_t len, const char *leaf)
{
	DIR *d = opendir("/sys/class/backlight");
	struct dirent *e;
	int found = 0;

	if (!d)
		return -1;
	while ((e = readdir(d))) {
		if (e->d_name[0] == '.')
			continue;
		snprintf(buf, len, "/sys/class/backlight/%s/%s", e->d_name, leaf);
		found = 1;
		break;		/* the first one; a laptop has exactly one */
	}
	closedir(d);
	return found ? 0 : -1;
}

static long backlight_read(const char *leaf)
{
	char path[512], val[64];
	if (backlight_path(path, sizeof(path), leaf) != 0)
		return -1;
	if (sh_read_line(path, val, sizeof(val)) != 0)
		return -1;
	return atol(val);
}

static int backlight_get(void)
{
	long cur = backlight_read("brightness");
	long max = backlight_read("max_brightness");
	if (cur < 0 || max <= 0)
		return -1;
	return (int)((cur * 100 + max / 2) / max);
}

static int backlight_set(int pct)
{
	char path[512];
	long max = backlight_read("max_brightness");
	if (max <= 0 || backlight_path(path, sizeof(path), "brightness") != 0)
		return -1;
	if (pct < 1)
		pct = 1;	/* never all the way off: a black screen looks
				 * like a crash and the key to undo it is one
				 * you can no longer see */
	if (pct > 100)
		pct = 100;

	FILE *f = fopen(path, "w");
	if (!f)
		return -1;	/* almost always permission — reported by the
				 * caller rather than swallowed */
	fprintf(f, "%ld\n", max * pct / 100);
	return fclose(f) == 0 ? 0 : -1;
}

/* ── drawing ───────────────────────────────────────────────────────────── */

static void draw_osd(const char *label, int pct, int muted)
{
	int w = ktui_w, h = ktui_h;
	if (w < 8 || h < 3)
		return;

	ktui_draw_fill(krect(0, 0, w, h), KT_SURFACE);
	ktui_draw_box(krect(0, 0, w, h), NULL, KT_ACCENT, KT_SURFACE, 1);

	char right[16];
	if (muted)
		snprintf(right, sizeof(right), "muted");
	else
		snprintf(right, sizeof(right), "%d%%", pct);

	ktui_draw_text(2, 1, w - 4, label, KT_TEXT, KT_SURFACE, KT_A_NONE);
	ktui_draw_text_right(0, 1, w - 2, right, muted ? KT_DIM : KT_ACCENT,
			     KT_SURFACE, KT_A_NONE);

	/*
	 * The bar is drawn with ktui_progress rather than by hand so it uses
	 * the glyph tier the surface actually has — eighth blocks here, three
	 * levels on a tty. A hand-rolled bar would pick one and be wrong on the
	 * other.
	 */
	ktui_progress(krect(2, 2, w - 4, 1), muted ? 0.0 : pct / 100.0, NULL);
	ktui_draw_flush();
}

/* ── one OSD at a time ─────────────────────────────────────────────────────
 *
 * A media key is HELD. Every press used to be a whole process — fork, connect,
 * load a 32-pixel bitmap font, create a layer surface, sit there for 1.2
 * seconds — so holding volume-up produced a queue of overlapping overlays, each
 * showing a value that was already stale, and a dozen font caches at once.
 *
 * So the first one to take the lock is the one on screen, and every later press
 * applies its change (which it has already done by this point), tells the owner
 * to look again, and exits. The lock file carries the owner's pid; the word to
 * show goes in a file beside it, because a signal cannot carry an argument.
 */
static volatile sig_atomic_t poked;

static void on_poke(int sig)
{
	(void)sig;
	poked = 1;
}

static int64_t osd_now_ms(void)
{
	struct timespec ts;
	clock_gettime(CLOCK_MONOTONIC, &ts);
	return (int64_t)ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}

static int osd_path(char *buf, size_t n, const char *leaf)
{
	const char *rt = getenv("XDG_RUNTIME_DIR");
	if (!rt || !*rt)
		return -1;
	snprintf(buf, n, "%s/kdos-osd.%s", rt, leaf);
	return 0;
}

static void osd_write_what(const char *what)
{
	char p[512];
	if (osd_path(p, sizeof(p), "what") != 0)
		return;
	FILE *f = fopen(p, "w");
	if (!f)
		return;
	fprintf(f, "%s\n", what);
	fclose(f);
}

static void osd_read_what(char *out, size_t n)
{
	char p[512];
	if (osd_path(p, sizeof(p), "what") == 0)
		sh_read_line(p, out, n);
}

/*
 * Returns 1 when this process should DRAW, 0 when another one already is (and
 * has been told to refresh). The lock fd is deliberately leaked into the draw:
 * it must stay held for as long as the overlay is up.
 */
static int osd_claim(void)
{
	char p[512], buf[32];
	int fd;

	/* The caller has already written what is to be shown: the poke below
	 * carries no argument, so the file IS the argument. */
	if (osd_path(p, sizeof(p), "lock") != 0)
		return 1;	/* no runtime dir: one-shot, as it always was */
	fd = open(p, O_RDWR | O_CREAT | O_CLOEXEC, 0600);
	if (fd < 0)
		return 1;

	if (flock(fd, LOCK_EX | LOCK_NB) != 0) {
		ssize_t k = read(fd, buf, sizeof(buf) - 1);
		close(fd);
		if (k > 0) {
			buf[k] = '\0';
			pid_t other = (pid_t)atoi(buf);
			/* ESRCH means the owner died between the flock and
			 * here; the lock is about to be released and the next
			 * press will draw. Nothing to recover. */
			if (other > 0)
				kill(other, SIGUSR1);
		}
		return 0;
	}

	int n = snprintf(buf, sizeof(buf), "%d\n", (int)getpid());
	if (ftruncate(fd, 0) == 0 && lseek(fd, 0, SEEK_SET) == 0) {
		if (write(fd, buf, (size_t)n) < 0) {
			/* The pid is a convenience for the next press, not
			 * correctness: without it that press simply draws
			 * nothing rather than misbehaving. */
		}
	}
	struct sigaction sa = { 0 };
	sa.sa_handler = on_poke;
	sigemptyset(&sa.sa_mask);
	sigaction(SIGUSR1, &sa, NULL);
	return 1;
}

/* Re-read whatever the newest press changed. */
static void osd_value(const char *what, int *pct, int *muted)
{
	if (!strcmp(what, "brightness")) {
		*pct = backlight_get();
		*muted = 0;
	} else if (!strcmp(what, "mic")) {
		*pct = mic_get(muted);
	} else {
		*pct = sh_volume_get(muted);
	}
}

/* The mic wears the panel indicator's bullet, so the overlay and the ●MIC
 * warning read as two views of the same thing. */
static void osd_show(const char *what, int pct, int muted)
{
	char label[48];

	if (!strcmp(what, "mic"))
		snprintf(label, sizeof(label), "%smic",
			 ktui_glyph[KT_G_BULLET]);
	else
		snprintf(label, sizeof(label), "%s", what);
	draw_osd(label, pct, muted);
}

/* ── main ──────────────────────────────────────────────────────────────── */

static int usage(void)
{
	fprintf(stderr,
		"usage: kdos-osd volume [+N|-N|mute|toggle]\n"
		"       kdos-osd mic [toggle|up|down|+N|-N]\n"
		"       kdos-osd brightness [+N|-N]\n");
	return 2;
}

int osd_main(int argc, char **argv)
{
	if (argc < 2)
		return usage();

	const char *what = argv[1];
	const char *arg = argc > 2 ? argv[2] : NULL;
	int pct = -1, muted = 0;

	if (!strcmp(what, "volume")) {
		pct = sh_volume_get(&muted);
		if (pct < 0) {
			fprintf(stderr, "kdos-osd: no ALSA mixer with a playback "
					"volume — is the card driver loaded?\n");
			return 1;
		}
		if (arg) {
			if (!strcmp(arg, "mute") || !strcmp(arg, "toggle")) {
				sh_volume_toggle();
			} else if (arg[0] == '+' || arg[0] == '-') {
				sh_volume_set(pct + atoi(arg));
			} else {
				return usage();
			}
			pct = sh_volume_get(&muted);
		}
	} else if (!strcmp(what, "mic")) {
		pct = mic_get(&muted);
		if (pct < 0) {
			fprintf(stderr, "kdos-osd: no ALSA capture mixer — is "
					"the card driver loaded?\n");
			return 1;
		}
		if (arg) {
			if (!strcmp(arg, "mute") || !strcmp(arg, "toggle"))
				mic_adjust(0);
			else if (!strcmp(arg, "up"))
				mic_adjust(5);
			else if (!strcmp(arg, "down"))
				mic_adjust(-5);
			else if (arg[0] == '+' || arg[0] == '-')
				mic_adjust(atoi(arg));
			else
				return usage();
			pct = mic_get(&muted);
		}
	} else if (!strcmp(what, "brightness")) {
		pct = backlight_get();
		if (pct < 0) {
			fprintf(stderr, "kdos-osd: no backlight device\n");
			return 1;
		}
		if (arg && (arg[0] == '+' || arg[0] == '-')) {
			if (backlight_set(pct + atoi(arg)) != 0) {
				fprintf(stderr,
					"kdos-osd: cannot write brightness — "
					"the user needs write access to "
					"/sys/class/backlight/*/brightness\n");
				return 1;
			}
			pct = backlight_get();
		} else if (arg) {
			return usage();
		}
	} else {
		return usage();
	}

	/*
	 * The value has already changed by this point. Showing it is best
	 * effort: on a tty there is no compositor, and a volume key that
	 * refuses to work because it cannot draw would be worse than one that
	 * works silently.
	 */
	osd_write_what(what);
	if (!osd_claim())
		return 0;		/* another OSD is up; it will refresh */

	KwlConfig cfg = {
		.role = KWL_ROLE_OVERLAY,
		/*
		 * Bottom-centre, where a volume bezel goes — dead centre put
		 * it on top of whatever the media key was pressed OVER.
		 */
		.corner = KWL_CORNER_BOTTOM_CENTER,
		.cols = OSD_COLS,
		.rows = OSD_ROWS,
		.app_id = "kdos-osd",
	};
	sh_theme_from_cache();
	if (kwl_init(&cfg) != 0)
		return 0;
	/*
	 * NO pointer input, ever: this overlay sat mid-screen with the default
	 * input region and ate every click under it for 1.2 s per keypress.
	 * It changes nothing on a click and must be transparent to one.
	 */
	kwl_input_cells(NULL, 0);
	ktui_draw_init();

	char shown[32];
	snprintf(shown, sizeof(shown), "%s", what);
	osd_show(shown, pct, muted);

	/*
	 * Draw, wait out the dwell, exit — but a press that arrives while this
	 * one is still up EXTENDS it and changes what it says, instead of
	 * starting a second overlay on top of this one. That is the whole of
	 * the singleton: no resident daemon, just the first press of a run
	 * holding the screen for the rest of them.
	 *
	 * A deadline rather than a counter: poll_event returns early on any
	 * event at all, and adding the timeout each time round made the dwell
	 * as short as the pointer was busy.
	 */
	KtuiEvent ev;
	int64_t until = osd_now_ms() + OSD_MS;
	while (osd_now_ms() < until && !kwl_should_close()) {
		int rem = (int)(until - osd_now_ms());
		ktui_backend()->poll_event(&ev, rem > 100 ? 100 : rem);
		if (!poked)
			continue;
		poked = 0;
		osd_read_what(shown, sizeof(shown));
		if (!shown[0])
			snprintf(shown, sizeof(shown), "%s", what);
		osd_value(shown, &pct, &muted);
		osd_show(shown, pct, muted);
		until = osd_now_ms() + OSD_MS;
	}

	kwl_shutdown();
	return 0;
}
