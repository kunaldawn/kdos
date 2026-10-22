/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   kdosbuild — the build orchestrator
 *
 * Runs the phases, snapshots them, restores them, and draws it all on libktui
 * — the one TUI toolkit in the tree, shared with kinstall and kdos-appbox.
 *
 * THE BUILD IS THE MAIN LOOP. libktui's input has a timeout, so the running
 * children and the terminal are polled together and whichever is ready is
 * read. Several steps run at once only as processes (--port-jobs); there is no
 * worker thread, which is why no progress callback has to guard against
 * drawing concurrently with the caller — none can.
 *
 * The pieces that only INSPECT the tree — phase discovery, the metadata
 * block, build plans, the snapshot inventory — are libkbuild's, and
 * testing/selftest.sh checks them against the repository itself.
 * ---------------------------------
 */

#ifndef KDOSBUILD_H
#define KDOSBUILD_H

#include <signal.h>
#include <sys/types.h>

#include "kbase.h"
#include "kbuild.h"
#include "kpkg.h"
#include "ktui.h"

/* Phases, every package they expand to, and under --port-jobs one commit step
 * per level: a level holds at least one port, so a phase of n ports never
 * grows past 2n rows. */
#define KB_MAX_STEPS   8192
/* The ceiling on ONE package phase's resolved order, and it is libkpkg's
 * KP_MAX_ORDER rather than a fraction of KB_MAX_STEPS: kpkgdepends cannot
 * return more than that, so a phase that reaches this has already been
 * truncated upstream and the build must say so rather than carry on. */
#define KB_MAX_PKGS    4096
#define KB_MAX_LOG     2000	/* lines kept per step                      */
#define KB_MAX_NOTICE  50
#define KB_MAX_WORKERS 8	/* --port-jobs ceiling: steps run at once   */
/* The jobserver FIFO, as the repository root names it and as a chroot entry
 * sees it: script/chroot/exec.sh binds <repo>/build at /kdos/build. */
#define KB_JOBSERVER_REL	"build/jobserver"
#define KB_JOBSERVER_CHROOT	"/kdos/build/jobserver"

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
	SX_CHROOT,		/* chroot/exec.sh bash <repo-relative path> */
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
	/* One slot past the largest phase: a retry puts kpkg's own step ahead
	 * of the rows the phase expands to (mgr_retry). */
	struct BStep *child[KB_MAX_PKGS + 1];
	int nchild;

	/* Groups only. */
	const KbuildPhase *meta;
	char packages_dir[512];	/* a package phase: the phase directory     */
	char script_dir[512];	/* a script phase: the same, for its *.sh   */
	/* The phase's rows have been made from its list. Not `nchild`: a
	 * retried phase holds kpkg's step before it is expanded again. */
	int resolved;

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
	/* ST_DONE without ever running: the host found the port installed
	 * and current before the phase entered the chroot for it. Until the
	 * run loop reaches it and asks again (`confirmed`), the phase is not
	 * finished: an edit made while earlier ports built can still bring
	 * its step back. */
	int installed;
	int confirmed;

	/* A package phase run by level (--port-jobs above 1). A port's
	 * `level` is one more than the highest of the ports it depends on;
	 * 0 in a phase that runs serially. A commit step (`is_commit`) carries
	 * the level it installs, and at its start the ports it names, in
	 * serial order (`commits`), `ncommitted` of them confirmed so far. */
	int level;
	int is_commit;
	int serial_pos;		/* position in kpkgdepends's order          */
	int no_commit;		/* --build-only skipped it as current       */
	struct BStep **commits;
	int ncommits, ncommitted;

	/* 10_bootstrap's 120_kpkg.sh, run by a retry ahead of the phase it
	 * retries (mgr_retry). It sits in that phase's rows but its log is
	 * 10_bootstrap's: renumber() leaves its step_number alone. */
	int kpkg_retry;

	/* Whether this run's seconds for the step are a measurement of its
	 * work (watch_line): 1 when a script step wrote any output or a port's
	 * step built that port and nothing else, 0 when it did no work — a
	 * guard that exited at once, a port skipped as installed, reused from
	 * the package store or already built — and -1 when a port's step built
	 * other ports too. Only 1 is recorded in timings.json, so a step that
	 * did nothing never replaces the figure of one that did. */
	int measured;

	int opened;		/* headless mode has printed its start    */
} BStep;

typedef struct {
	double when;
	char text[256];
} BNotice;

/* ──────────────────────────────────────────────────────────────────────── */
/* Live snapshot / restore progress, read by the TUI                        */

typedef struct {
	int active;
	char action[16];	/* preparing indexing snapshot restore      */
	char phase[64];
	char path[128];
	char layer[48];		/* "full", "layer on 40_lang", "layer 3/10" */
	char current[256];
	long long bytes, est_bytes;
	long long files, est_files;
	double rate;
	double started;
} SnapActivity;

/* ──────────────────────────────────────────────────────────────────────── */
/* The manager                                                              */

/* One running step: its process group, its output pipe and its log. */
typedef struct {
	BStep *step;		/* NULL while the slot is free              */
	pid_t pid;
	int fd;
	int log_fd;
	int killed;
	double killed_at;
	char buf[8192];
	size_t len;
} BWorker;

typedef struct {
	char script_dir[512];
	char phases_dir[512];	/* <script_dir>/phases: logs are named under it */
	char build_dir[512];
	char repo_root[512];
	char snap_root[512];
	char timings_path[600];	/* <snap_root>/timings.json (timings.c)    */
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
	/* Off unless --snapshot or the picker's S turns it on. */
	int snapshot_enabled;
	int full_snapshots;	/* --full-snapshots: never write a layer    */
	/* The run's wall clock: end_time is stamped when the run stops and
	 * cleared by a retry, so mgr_elapsed() stops counting while nothing
	 * runs. */
	double start_time, end_time;
	long long total_lines;

	/* The last phase a restore skipped and the last one --continue-from
	 * skipped: the banner names one or the other. */
	const KbuildPhase *restored_from;
	const KbuildPhase *continued_from;

	BNotice notice[KB_MAX_NOTICE];
	int nnotice;

	SnapActivity snap;
	/* A phase whose last step just finished. Snapshotting it is deferred
	 * by one pump turn so a front end can print that step's RESULT before
	 * the snapshot's progress starts arriving. */
	BStep *pending_finish;
	BStep *snapshot_request;	/* [S] — force a snapshot right here */

	/* The package phase being run: the configuration its `installed`
	 * rows were decided with, asked again when the loop reaches each. */
	KpConf *host_conf;

	char forced_seen[KBUILD_MAX_REBUILD][64];
	int nforced_seen;

	/* The steps running, one per busy slot. A serial phase uses slot 0
	 * alone; a level of a phase run with --port-jobs N fills up to N. */
	BWorker worker[KB_MAX_WORKERS];
	int nrunning;

	/* --port-jobs: 1 runs every phase one port at a time. `port_k` is the
	 * KDOS_JOBS each port of a level is given, and `cpu` the CPUs this
	 * process may use, from which each slot's affinity window is cut when
	 * there is no jobserver. `js_fd` is the jobserver FIFO
	 * (<repo>/build/jobserver) held open read-write, -1 without one, and
	 * `js_tokens` the tokens it holds when no port is running. */
	int port_jobs;
	int port_k;
	int cpu[1024];
	int ncpu;
	int js_fd;
	int js_tokens;

	/* The level being built: order[lvl_from .. lvl_commit - 1] are its
	 * ports and order[lvl_commit] its commit step; lvl_commit is -1 when
	 * no level is open. `draining` is a level whose port failed: nothing
	 * new starts, and the build stops once the running ones finish. */
	int lvl_from, lvl_commit;
	int draining;
} Manager;

void mgr_init(Manager *m, const char *script_dir, const char *build_dir);
void mgr_apply_plan(Manager *m);	/* after m->plan is filled in       */
void mgr_free(Manager *m);
double step_duration(const BStep *s);
double mgr_elapsed(const Manager *m);
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
/* Retry the phase of the failed step: kpkg's own step first, then the phase
 * expanded again from its list as it stands now (see manager.c). 0 when the
 * run is going again; -1 with `why` filled when nothing failed or a step is
 * still running. */
int mgr_retry(Manager *m, char *why, size_t cap);
int mgr_selftest(void);		/* --selftest: the retry, the jobserver    */

/* ──────────────────────────────────────────────────────────────────────── */
/* Snapshots — creating and extracting, on top of libkbuild's inventory     */

/* 1 wrote a snapshot, 0 skipped it, -1 failed with err filled in. */
int snap_create(Manager *m, BStep *group, char *err, size_t errcap);
int snap_restore(Manager *m, const KbuildRestoreItem *plan, int n,
		 const char *target, char *err, size_t errcap);
/* 0 when there is no such snapshot, 1 deleted, 2 held: another snapshot's
 * chain runs through it, so it moved to build/snapshots/.held/ and `deps`
 * names what needs it. Either way, held snapshots nothing needs any more are
 * deleted. */
int snap_delete(Manager *m, const char *dir_name, char *deps, size_t cap);
/* Deletes every held snapshot no phase directory's chain reaches; returns how
 * many. */
int snap_gc(Manager *m);
/* "full", "layer on 40_lang", or one clause per path when they differ. */
void snap_kind(const KbuildSnapshot *all, int n, const KbuildSnapshot *sn,
	       char *out, size_t cap);
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
/* Timings — build/snapshots/timings.json, merged on every record           */

/* Sets one key of `section` ("steps" or "phases") to `secs`, rounded to two
 * decimals, and keeps every other key the file holds. 0 written, -1 refused
 * or not written. */
int tm_record(const char *path, const char *section, const char *key,
	      double secs);
/* The seconds stored for `key`, -1 when there are none. */
double tm_lookup(const char *path, const char *section, const char *key);
int tm_selftest(void);		/* --selftest: the merge                   */

/* ──────────────────────────────────────────────────────────────────────── */
/* Telemetry sampler — cheap counters plus an occasional forked tree walk   */

/* Samples kept per graph, one a second. A braille cell holds two, so this is
 * also the widest graph the HUD draws, in cells, times two (hud_compute caps
 * a graph there). */
#define KB_HIST 1024

typedef struct {
	double load1;
	long long mem_used, mem_total;
	long long swap_used;
	double swap_rose_at;	/* when swap_used last grew; 0 never        */
	long long disk_free;
	double lines_per_sec;
	double cpu_busy;	/* 0..1 of every CPU, from /proc/stat       */
	int ncpu;		/* the cpuN lines /proc/stat lists          */
	unsigned long long cpu_busy_ticks, cpu_total_ticks;

	/* One sample per tick, oldest first, the newest at [nhist - 1]: CPU
	 * busy and memory in use as fractions, output in lines a second. */
	double cpu_hist[KB_HIST];
	double mem_hist[KB_HIST];
	double rate_hist[KB_HIST];
	int nhist;

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
/* Appends one sample to each history, dropping the oldest when full. */
void sam_push(Sampler *s, double cpu, double mem, double rate);

/* ──────────────────────────────────────────────────────────────────────── */
/* Formatting shared by the screens and the plain-text commands             */

const char *human_bytes(long long n);
const char *human_time(double seconds);
const char *format_when(double ts);
const char *human_count(long long n);

/* ──────────────────────────────────────────────────────────────────────── */
/* View geometry
 *
 * Every region's origin comes from here. Split into independent calculations
 * — hud_h in screen_build, a 45% cap in tree_width, the header height inline
 * — they disagree at some widths and the divider is drawn on top of the log
 * text. One struct means a size that breaks the layout breaks it visibly in
 * one place, and can be asserted over every size.
 */
typedef struct {
	KRect header, tree, divider, detail, hud, footer;
	int has_header, has_detail, hud_rows, too_small;
} Layout;

Layout layout_compute(int w, int h);

/* The HUD's panels inside its rect: a graph and its caption for CPU, memory
 * and output, then an info panel (rootfs, the heat strip, the last notice).
 * A HUD of HUD_BOXED_ROWS or more draws each panel in its own titled box,
 * `box[i]`, with the graph, caption or info rect inside the border; a
 * one-row HUD has no boxes. A panel that does not fit at its minimum is
 * absent, w == 0, and the order they are given up in is info, output,
 * memory; CPU goes last. Every rect lies inside the HUD and no two boxes,
 * nor any two contents, overlap — view_selftest asserts it over every width. */
enum { HUD_CPU = 0, HUD_MEM, HUD_OUT, HUD_NGRAPH };
#define HUD_INFO HUD_NGRAPH	/* box[] index of the info panel          */
#define HUD_BOXED_ROWS 6	/* four graph rows inside a border          */
typedef struct {
	KRect graph[HUD_NGRAPH], caption[HUD_NGRAPH];
	KRect info;
	KRect box[HUD_NGRAPH + 1];
	int boxed;
} HudLayout;

#define HUD_GRAPH_MIN 8		/* cells: 16 seconds                        */
#define HUD_INFO_MIN 24
HudLayout hud_compute(KRect r);
/* The caption column each graph is given, wide enough for its widest line. */
int hud_caption_w(int which);

/* Log line classification for the detail pane. */
enum { LOG_PLAIN = 0, LOG_WARN, LOG_ERR };
int log_severity(const char *line);

/* Byte offset of `needle` in `hay`, case-insensitive, -1 when absent. */
int log_find(const char *hay, const char *needle);

int view_selftest(void);	/* --selftest: the assertions over the above */

/* ──────────────────────────────────────────────────────────────────────── */
/* Headless reporting — text and JSON, one traversal (report.c)              */

typedef struct {
	void (*begin)(const Manager *m);	/* NULL for text          */
	void (*group)(const Manager *m, const BStep *s);
	void (*step_open)(const Manager *m, const BStep *s);
	void (*step_close)(const Manager *m, const BStep *s);
	void (*notice)(const Manager *m, const char *text);
	void (*restore)(const Manager *m, const char *phase);
	void (*snap_tick)(const Manager *m);
	void (*finish)(const Manager *m);
} Reporter;

const Reporter *reporter_for(int json);
void report_snapshots_json(const KbuildPhase *ph, int nph,
			   const KbuildSnapshot *all, int n,
			   const char *commit);

/* ──────────────────────────────────────────────────────────────────────── */
/* Screens                                                                  */

enum { PICK_QUIT = 0, PICK_FRESH, PICK_RESTORE, PICK_PLAN };

/* The startup picker. `index` receives the phase to restore when the result is
 * PICK_RESTORE. `snapshot_enabled` carries the snapshot-writing choice BOTH
 * ways: in as the state the screen opens on (the command line's), out as what
 * the operator left it at. It is answered here rather than on the command line
 * because the cost being chosen — gigabytes and a part of the run's wall
 * clock — is only knowable once the phase list and the codec are on
 * screen beside it. */
int screen_startup(Manager *m, int *index, const char *commit,
		   int *snapshot_enabled);
int screen_plan(Manager *m, KbuildPlan *out);
void screen_progress(Manager *m, const char *title);	/* restore HUD      */
void screen_build(Manager *m, Sampler *s);
/* The terminal's title while kdosbuild has it: pushed on the xterm title
 * stack at the start, set to "kdosbuild" and popped at the end. Nothing on the
 * Linux VT, which has no title. */
void term_title_begin(void);
void term_title_end(void);

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
 * the column arithmetic — a multi-terabyte total, a nine-digit file count,
 * hour-scale elapsed times and graphs that cross their warning levels. */
void preview_fixture(Manager *m, Sampler *s);
int preview_snapshots(Manager *m, KbuildSnapshot *out, int max);

#endif /* KDOSBUILD_H */
