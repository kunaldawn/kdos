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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
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

static snd_mixer_t *mixer_open(snd_mixer_elem_t **elem)
{
	snd_mixer_t *h = NULL;
	if (snd_mixer_open(&h, 0) < 0)
		return NULL;
	if (snd_mixer_attach(h, "default") < 0 ||
	    snd_mixer_selem_register(h, NULL, NULL) < 0 ||
	    snd_mixer_load(h) < 0) {
		snd_mixer_close(h);
		return NULL;
	}
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

/* ── main ──────────────────────────────────────────────────────────────── */

static int usage(void)
{
	fprintf(stderr,
		"usage: kdos-osd volume [+N|-N|mute|toggle]\n"
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
		snd_mixer_elem_t *elem = NULL;
		snd_mixer_t *h = mixer_open(&elem);
		if (!h) {
			fprintf(stderr, "kdos-osd: no ALSA mixer — is the card "
					"driver loaded?\n");
			return 1;
		}
		pct = volume_get(elem, &muted);
		if (pct < 0) {
			snd_mixer_close(h);
			fprintf(stderr, "kdos-osd: the mixer has no playback "
					"volume\n");
			return 1;
		}
		if (arg) {
			if (!strcmp(arg, "mute") || !strcmp(arg, "toggle")) {
				if (snd_mixer_selem_has_playback_switch(elem)) {
					muted = !muted;
					snd_mixer_selem_set_playback_switch_all(
						elem, !muted);
				}
			} else if (arg[0] == '+' || arg[0] == '-') {
				pct += atoi(arg);
				volume_set(elem, pct);
				pct = volume_get(elem, &muted);
			} else {
				snd_mixer_close(h);
				return usage();
			}
		}
		snd_mixer_close(h);
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
	KwlConfig cfg = {
		.role = KWL_ROLE_OVERLAY,
		.cols = OSD_COLS,
		.rows = OSD_ROWS,
		.app_id = "kdos-osd",
	};
	sh_theme_from_cache();
	if (kwl_init(&cfg) != 0)
		return 0;
	ktui_draw_init();

	draw_osd(what, pct, muted);

	/* One shot: draw, wait out the dwell, exit. There is no state to keep,
	 * and a resident OSD daemon would be a process per session holding a
	 * surface it shows for one second an hour. */
	KtuiEvent ev;
	int64_t waited = 0;
	while (waited < OSD_MS && !kwl_should_close()) {
		ktui_backend()->poll_event(&ev, 100);
		waited += 100;
	}

	kwl_shutdown();
	return 0;
}
