/*
 * KK — the KDOS demo.  Forked from BB (C) 1997 AA-group; see LICENSE.notice.
 *
 * KDOS-side additions: option parsing, the frame counter and the benchmark
 * that answers "how fast is this actually on a TTY" with a number instead of
 * an opinion. Nothing here is upstream.
 */

#ifndef KK_H
#define KK_H

/* The three vendored modules, named by their ROLE rather than by whatever
 * ModArchive called them — build.sh installs them under these names and
 * music/MUSIC.credits records which track each one actually is. Swapping a
 * track is then a rename, not a source edit. */
#define SONG_INTRO   "kk-intro.xm"
#define SONG_MIDDLE  "kk-middle.xm"
#define SONG_CREDITS "kk-credits.xm"

typedef struct {
	int loop;		/* -loop: run the script forever            */
	int scene;		/* --scene N: start at stage N              */
	int sound;		/* --no-sound clears this                   */
	int fps;		/* --fps: draw the counter in the corner    */
	int benchmark;		/* --benchmark[=SEC]: seconds, 0 = off      */
	const char *driver;	/* --driver NAME, or NULL for the default   */
} KkOpts;

extern KkOpts kk;

/* Strip KK's own options out of argv, leaving aalib's for aa_parseoptions.
 * Returns 0 to continue, 1 when it printed help and the caller should exit. */
int kk_parse_args(int *argc, char **argv);

/* Remember which display driver aalib actually initialized. Recorded while
 * the context is alive because kk_report() runs after aa_close(). */
void kk_note_driver(const char *shortname);

/* Called once per rendered frame, immediately before aa_flush(). Counts the
 * frame, paints the overlay when --fps is on, and ends a --benchmark run. */
void kk_frame(void);

/* Frames per second over the whole run, and the display driver that was
 * actually initialized — printed on exit under --fps or --benchmark. */
void kk_report(void);

#endif /* KK_H */
