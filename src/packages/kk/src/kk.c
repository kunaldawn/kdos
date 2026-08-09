/*
 * KK — the KDOS demo.  Forked from BB (C) 1997 AA-group; see LICENSE.notice.
 *
 * Options, the frame counter and the benchmark. All KDOS, none of it upstream.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <aalib.h>
#include "kk.h"

KkOpts kk = { 0, 1, 1, 0, 0, NULL };

extern aa_context *context;

static const char *display_driver = "?";
static double t0;
static long frames;
static double fps_shown;
static double fps_last_t;
static long fps_last_frames;

static double now(void)
{
	struct timespec ts;
	clock_gettime(CLOCK_MONOTONIC, &ts);
	return (double)ts.tv_sec + (double)ts.tv_nsec / 1e9;
}

static void usage(void)
{
	printf("kk — the KDOS demo\n"
	       "\n"
	       "Usage: kk [options] [aalib options]\n"
	       "\n"
	       "  -loop, --loop      play the demo in an infinite loop\n"
	       "  --scene N          start at stage N (1-3)\n"
	       "  --no-sound         run silent, do not open the audio device\n"
	       "  --fps              show a live frame counter\n"
	       "  --benchmark[=SEC]  run SEC seconds (default 20), print fps, exit\n"
	       "  --driver NAME      force an aalib display driver (linux, curses)\n"
	       "  -h, --help         this text\n"
	       "\n"
	       "In the demo: s or Backspace skips a scene, q or Escape quits.\n"
	       "\n"
	       "The two drivers are not equivalent. 'linux' writes cells straight\n"
	       "into /dev/vcsa<n> and needs read-write access to it; 'curses' goes\n"
	       "through the terminal and works anywhere. --fps prints which one\n"
	       "was actually initialized.\n"
	       "\n"
	       "aalib options:\n%s\n", aa_help);
}

int kk_parse_args(int *argc, char **argv)
{
	int in, out = 1;

	for (in = 1; in < *argc; in++) {
		char *a = argv[in];

		if (!strcmp(a, "-h") || !strcmp(a, "--help")) {
			usage();
			return 1;
		} else if (!strcmp(a, "-loop") || !strcmp(a, "--loop")) {
			kk.loop = 1;
		} else if (!strcmp(a, "--no-sound")) {
			kk.sound = 0;
		} else if (!strcmp(a, "--fps")) {
			kk.fps = 1;
		} else if (!strcmp(a, "--benchmark")) {
			kk.benchmark = 20;
		} else if (!strncmp(a, "--benchmark=", 12)) {
			kk.benchmark = atoi(a + 12);
			if (kk.benchmark <= 0)
				kk.benchmark = 20;
		} else if (!strcmp(a, "--scene") && in + 1 < *argc) {
			kk.scene = atoi(argv[++in]);
			if (kk.scene < 1 || kk.scene > 3)
				kk.scene = 1;
		} else if (!strcmp(a, "--driver") && in + 1 < *argc) {
			kk.driver = argv[++in];
		} else if (a[0] >= '1' && a[0] <= '8' && !a[1]) {
			/* Upstream's bare stage number. */
			kk.scene = a[0] - '0';
		} else {
			/* Not ours — hand it to aa_parseoptions. */
			argv[out++] = a;
		}
	}
	argv[out] = NULL;
	*argc = out;

	/* A benchmark that stops for a keypress is not a benchmark. */
	if (kk.benchmark)
		kk.fps = 1;
	return 0;
}

void kk_note_driver(const char *shortname)
{
	if (shortname)
		display_driver = shortname;
}

void kk_frame(void)
{
	double t;

	if (!t0)
		t0 = fps_last_t = now();
	frames++;

	if (!kk.fps)
		return;

	t = now();
	if (t - fps_last_t >= 0.5) {
		fps_shown = (double)(frames - fps_last_frames) / (t - fps_last_t);
		fps_last_t = t;
		fps_last_frames = frames;
	}

	/* AA_SPECIAL is the attribute the demo already uses for text over the
	 * rendered image, so the counter cannot be mistaken for a pixel. */
	aa_printf(context, 0, 0, AA_SPECIAL, "%5.1f fps %s", fps_shown,
		  display_driver);

	if (kk.benchmark && t - t0 >= kk.benchmark) {
		aa_close(context);
		kk_report();
		exit(0);
	}
}

void kk_report(void)
{
	double t = now();

	if (!frames || t <= t0)
		return;
	fprintf(stderr, "kk: %ld frames in %.1fs = %.1f fps (display driver: %s)\n",
		frames, t - t0, (double)frames / (t - t0), display_driver);
}
