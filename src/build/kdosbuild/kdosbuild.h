/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   kdosbuild — the build orchestrator
 *
 * Replaces script/build.py and the python modules under script/buildlib. Runs
 * the phases, snapshots them, restores them, and draws it all on libktui — the
 * change that kills the third TUI toolkit (kinstall's, kdos-appbox's ncurses
 * one and buildlib's python curses one were three implementations of one).
 *
 * ONE STRUCTURAL CHANGE FROM THE PYTHON, AND IT IS A SIMPLIFICATION.
 * build.py ran the build on a worker THREAD because curses' getch() blocks.
 * libktui's input has a timeout, so here the build IS the main loop: poll the
 * running child and the terminal together, read whichever is ready, redraw.
 * No threads, no locks, no "never let the worker die silently" wrapper, and
 * the progress callbacks that had to be careful never to draw concurrently
 * with the caller now cannot be.
 *
 * The pieces that only INSPECT the tree — phase discovery, the metadata
 * block, build plans, the snapshot inventory — are libkbuild's, and are
 * verified against buildlib itself by testing/selftest.sh.
 * ---------------------------------
 */

#ifndef KDOSBUILD_H
#define KDOSBUILD_H

#include <signal.h>
#include <sys/types.h>

#include "kbase.h"
#include "kbuild.h"
#include "ktui.h"

#define KB_MAX_STEPS   4096	/* phases + every package they expand to    */
#define KB_MAX_LOG     2000	/* lines kept per step, as in build.py      */
#define KB_MAX_NOTICE  50

/* ──────────────────────────────────────────────────────────────────────── */
/* The step tree                                                            */

enum {
	ST_PENDING = 0,
	ST_RUNNING,
	ST_DONE,
	ST_FAIL,
	ST_SKIPPED
};

enum {
	SX_HOST = 0,		/* bash <script>                            */
	SX_CHROOT,		/* chroot_exec.sh bash <repo-relative path> */
	SX_CUSTOM		/* a command line we built (kpkg install)   */
};

typedef struct BStep {
	char path[512];
	char title[128];	/* "# Title:" from the script, or the name  */
	int is_group;
	int status;
	int step_type;
	int step_number;
	int return_code;

	struct BStep *parent;
	struct BStep *child[KB_MAX_STEPS / 8];
	int nchild;

	/* Groups only. */
	const KbuildPhase *meta;
	char packages_file[512];
	char script_dir[512];

	/* SX_CUSTOM only: the argv to run, already built. `cmd_line` owns the
	 * string cmd.v points at — KbArgv stores POINTERS, it does not copy,
	 * so the storage has to outlive the argv. */
	KbArgv cmd;
	char *cmd_line;
	int have_cmd;

	char note[64];		/* short UI annotation (snapshot state)     */
	char snap_state[16];	/* ok | partial | failed | aborted | skipped */

	char **log;
	int nlog, logcap;

	double start_time, end_time;
	int expanded;
	int reported;		/* headless mode has printed its result   */
} BStep;

typedef struct {
	double when;
	char text[256];
} BNotice;

/* ──────────────────────────────────────────────────────────────────────── */
/* Timing history — EWMA across builds, used for the ETA                    */

typedef struct {
	char key[192];
	double secs;
} TimeRec;

typedef struct {
	char path[512];
	TimeRec *step;
	int nstep, stepcap;
	TimeRec *phase;
	int nphase, phasecap;
	int dirty;
} Timings;

void tm_load(Timings *t, const char *path);
void tm_save(Timings *t);
void tm_record_step(Timings *t, const char *key, double secs);
void tm_record_phase(Timings *t, const char *key, double secs);
double tm_step_est(const Timings *t, const char *key, double fallback);
double tm_phase_est(const Timings *t, const char *dir_name);
void tm_free(Timings *t);

/* ──────────────────────────────────────────────────────────────────────── */
/* Live snapshot / restore progress, read by the TUI                        */

typedef struct {
	int active;
	char action[16];	/* preparing measuring snapshot restore     */
	char phase[64];
	char path[128];
	char current[256];
	long long bytes, est_bytes;
	long long files, est_files;
	double rate;
	double started;
} SnapActivity;

/* ──────────────────────────────────────────────────────────────────────── */
/* The manager                                                              */

typedef struct {
	char script_dir[512];
	char build_dir[512];
	char repo_root[512];
	char snap_root[512];
	/* Stable storage: cmd_prefix() puts this in a KbArgv, which keeps the
	 * pointer rather than a copy, so a local would dangle at return. */
	char chroot_exec[600];

	KbuildPhase phase[KBUILD_MAX_PHASES];
	int nphase;

	KbuildPlan plan;
	int have_plan;

	BStep *root[KBUILD_MAX_PHASES];
	int nroot;
	BStep *order[KB_MAX_STEPS];
	int norder;
	int cursor;		/* index into order[] the run loop is at    */

	BStep *current_step;
	BStep *current_phase;
	BStep *error_step;

	int is_running;
	int stop_requested;
	int force_quit;		/* a second Q: kill the group and leave    */
	int snapshot_enabled;
	double start_time;
	long long total_lines;

	const KbuildPhase *restored_from;
	const KbuildPhase *resumed_inside;
	const KbuildPhase *continued_from;

	BNotice notice[KB_MAX_NOTICE];
	int nnotice;

	SnapActivity snap;
	/* A phase whose last step just finished. Snapshotting it is deferred
	 * by one pump turn so a front end can print that step's RESULT before
	 * the snapshot's progress starts arriving. */
	BStep *pending_finish;
	BStep *snapshot_request;	/* [S] — force a snapshot right here */

	char forced_seen[KBUILD_MAX_REBUILD][64];
	int nforced_seen;

	Timings *timings;

	/* The child of the step that is running, and its pipe. */
	pid_t child;
	int child_killed;
	double child_killed_at;
	int child_fd;
	char child_buf[8192];
	size_t child_len;
	int log_fd;
} Manager;

void mgr_init(Manager *m, const char *script_dir, const char *build_dir);
void mgr_apply_plan(Manager *m);	/* after m->plan is filled in       */
void mgr_free(Manager *m);
double step_duration(const BStep *s);
void step_timing_key(const BStep *s, char *out, size_t cap);
/* build/logs/<phase>/<NNNN>_<name>.log — the file the step's output was TEE'd
 * to. The in-memory log is capped at KB_MAX_LOG lines, so on a long step the
 * file is the only place the head of it still exists, which is exactly when
 * somebody wants to read it. */
void log_path_for(const Manager *m, const BStep *s, char *out, size_t cap);
void mgr_notice(Manager *m, const char *fmt, ...)
	__attribute__((format(printf, 2, 3)));
void mgr_mark_continued(Manager *m, int phase_index);
void mgr_mark_restored(Manager *m, int phase_index, int resume_inside);
void mgr_start(Manager *m);
/* One turn of the build. Returns the ms the caller may wait for input. */
int mgr_pump(Manager *m);
BStep *mgr_phase_of(BStep *s);

/* ──────────────────────────────────────────────────────────────────────── */
/* Snapshots — creating and extracting, on top of libkbuild's inventory     */

/* 1 wrote a snapshot, 0 skipped it, -1 failed with err filled in. */
int snap_create(Manager *m, BStep *group, char *err, size_t errcap);
int snap_restore(Manager *m, const KbuildRestoreItem *plan, int n,
		 const char *target, char *err, size_t errcap);
int snap_delete(Manager *m, const char *dir_name);
/* A redraw hook, so a 40-minute tar keeps the screen alive. */
void snap_set_tick(Manager *m, void (*fn)(Manager *));
void snap_git_info(const char *repo_root, char *commit, size_t ccap, int *dirty);
const char *snap_codec(void);

typedef struct {
	long long bytes, files;
	int complete;
} Usage;

Usage dir_usage(const char *path, double deadline,
		void (*on_tick)(long long files, long long bytes, void *user),
		void *user);

/* ──────────────────────────────────────────────────────────────────────── */
/* Telemetry sampler — cheap counters plus an occasional forked tree walk   */

typedef struct {
	double load1;
	long long mem_used, mem_total;
	long long disk_free;
	double lines_per_sec;
	double history[240];
	int nhistory;

	double load_hist[240];
	int nload;

	long long fs_files, fs_bytes;
	int fs_partial;
	double fs_sampled_at;

	pid_t walker;		/* the forked walk, -1 when idle            */
	int walker_fd;
	double last_walk;
	double last_tick;
	long long last_lines;
} Sampler;

void sam_init(Sampler *s);
void sam_pump(Sampler *s, Manager *m);
void sam_stop(Sampler *s);
double eta_seconds(const Manager *m, const Timings *t);

/* ──────────────────────────────────────────────────────────────────────── */
/* Formatting shared by the screens and the plain-text commands             */

const char *human_bytes(long long n);
const char *human_time(double seconds);
const char *format_when(double ts);
const char *human_count(long long n);

/* ──────────────────────────────────────────────────────────────────────── */
/* View geometry
 *
 * Every region's origin comes from here. It used to be three independent
 * calculations — hud_h in screen_build, the 45% cap in tree_width, the header
 * height inline — and at some widths the divider was drawn on top of the log
 * text. One struct means a size that breaks the layout breaks it visibly in
 * one place, and can be asserted over every size.
 */
typedef struct {
	KRect header, tree, divider, detail, hud, footer;
	int has_header, has_detail, hud_rows, too_small;
} Layout;

Layout layout_compute(int w, int h);

/* Log line classification for the detail pane. */
enum { LOG_PLAIN = 0, LOG_WARN, LOG_ERR };
int log_severity(const char *line);

/* Byte offset of `needle` in `hay`, case-insensitive, -1 when absent. */
int log_find(const char *hay, const char *needle);

int view_selftest(void);	/* --selftest: the assertions over the above */

/* ──────────────────────────────────────────────────────────────────────── */
/* Screens                                                                  */

enum { PICK_QUIT = 0, PICK_FRESH, PICK_RESTORE, PICK_PLAN };

int screen_startup(Manager *m, int *index, const char *commit);
int screen_plan(Manager *m, KbuildPlan *out);
void screen_progress(Manager *m, const char *title);	/* restore HUD      */
void screen_build(Manager *m, Sampler *s, Timings *t);

/* ──────────────────────────────────────────────────────────────────────── */
/* --preview — render one screen offscreen and dump it as text
 *
 * Each screen interleaved its drawing with its own event loop, so each has a
 * draw_*_frame() split out of it that the loop and this both call. What that
 * buys is the class of defect neither the compiler nor testing/selftest.sh can
 * see, because the test suite has no terminal and cannot draw: a column that
 * has drifted out from under its own header, text over a box border, a chart
 * running past its rect, a gauge invisible on a selected row. Six of those
 * shipped in this TUI and every one was found by hand arithmetic.
 */
int preview_main(const char *screen, const char *size, const char *tier);
/* Draws one named screen once against the fixture. Non-zero for a bad name. */
int preview_screen(const char *screen);
/* Synthetic state chosen to exercise what breaks, not to look plausible: a
 * group mid-run, a skipped step, a failed one, a log carrying both a warning
 * and an error, a name too long for any pane, and values wide enough to stress
 * the column arithmetic — a multi-terabyte total, a nine-digit file count and
 * an hour-scale ETA. */
void preview_fixture(Manager *m, Sampler *s, Timings *t);
int preview_snapshots(Manager *m, KbuildSnapshot *out, int max);

#endif /* KDOSBUILD_H */
