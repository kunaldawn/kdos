/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   libkbuild — what the orchestrator knows about the build tree
 *
 * Phase discovery, the phase-env metadata block, and the snapshot path rules.
 * This is the part of the build system that is pure inspection of the repo: it
 * reads, it decides nothing, and it runs nothing.
 *
 * A phase is a directory `<script>/phases/<NN>_<name>/`, and its environment
 * is the `phase.env` inside it.
 *
 * THE ENV FILES ARE PARSED, NEVER SOURCED. A chroot phase's environment ends
 * with `rm -rf /var/cache/kpkg/work`, which at source time hits the BUILD
 * CONTAINER's own filesystem rather than the target's. That is why only
 * `export NAME=VALUE` lines are read, why only five keys are honoured, and
 * why a value has to be a literal — no expansion is performed and anything
 * that is not a plain literal reads as empty. Only the file's own text is
 * read: a `source` line in it is not followed, so CHROOT and the KDOS_* keys
 * have to be written in `phase.env` itself, or the phase runs on the host
 * with no title and no snapshot.
 * ---------------------------------
 */

#ifndef KBUILD_H
#define KBUILD_H

#include "kbase.h"

#define KBUILD_MAX_PHASES  32
#define KBUILD_MAX_PATHS   32
#define KBUILD_MAX_STEPS   64
#define KBUILD_MAX_REBUILD 256	/* also sizes the sort scratch in kb_plan.c */
#define KBUILD_PLAN_FILE   ".devplan.json"
#define KBUILD_PHASES_DIR  "phases"		/* under the script directory */
#define KBUILD_PHASE_ENV   "phase.env"
#define KBUILD_PKG_FILE    "packages.txt"
#define KBUILD_PKG_DIR     "packages.d"
#define KBUILD_ORDER_FILE  "00-order.txt"	/* packages.d's order run */

typedef struct {
	int index;
	char dir_name[64];	/* 30_foundation                           */
	char dir_path[512];
	char name[64];		/* foundation                              */
	char env_file[512];	/* <dir_path>/phase.env, "" when absent    */

	int chroot;		/* CHROOT=1 — decides the execution wrapper */
	char title[128];
	char desc[256];

	char snap_path[KBUILD_MAX_PATHS][128];
	int nsnap;
	char snap_exclude[KBUILD_MAX_PATHS][256];
	int nexclude;
	/* Paths the metadata asked for that were REFUSED — absolute, empty, or
	 * climbing out with "..". Reported rather than silently dropped: a
	 * snapshot restore extracts these as root. */
	char rejected[KBUILD_MAX_PATHS][256];
	int nrejected;
	/* Why this phase cannot run, "" when it can: both packages.txt and
	 * packages.d/, a packages.d/ with no list in it, or no list and no
	 * step at all. The driver refuses to start while any phase has one. */
	char error[256];
} KbuildPhase;

/* The phases under <script_dir>/phases/, ordered by directory name, which is
 * what the numeric prefix is for. Only directories matching ^[0-9]+_ are
 * phases. `script_dir` is the script directory itself, not its phases/: the
 * driver takes the repository root as the script directory's parent. */
int kbuild_discover(const char *script_dir, KbuildPhase *out, int max);

/* A phase with no KDOS_SNAPSHOT_PATHS is never snapshotted. */
int kbuild_snapshottable(const KbuildPhase *p);

/* "foundation" or "foundation (Chroot)" — the label the picker shows. */
void kbuild_label(const KbuildPhase *p, char *out, size_t cap);

/* A snapshot path must stay inside $BUILD_DIR. Snapshot and restore delete
 * and re-extract these as root, so anything absolute, empty, or climbing out
 * with ".." is refused rather than sanitised. */
int kbuild_safe_relpath(const char *path);

/* The literal on the right of an `export NAME=`. No shell expansion: an
 * unquoted value ends at whitespace or a comment, and an unterminated quote
 * reads as empty. */
void kbuild_unquote(const char *raw, char *out, size_t cap);

/* A phase by its directory name OR its short name, so both `41_system` and
 * `system` resolve. NULL when neither matches. */
const KbuildPhase *kbuild_find(const KbuildPhase *ph, int n, const char *token);

/* ──────────────────────────────────────────────────────────────────────── */
/* JSON — the kj_* reader; every writer emits its own bytes
 *
 * kj_parse returns NULL for anything that does not parse whole. Every caller
 * treats that as "absent" rather than "partial": a half-read manifest that
 * looks complete is the failure that loses a tree.
 */

typedef enum { KJ_NULL, KJ_BOOL, KJ_NUM, KJ_STR, KJ_ARR, KJ_OBJ } KjType;

typedef struct KjNode {
	KjType type;
	char *key;		/* set when the node is a member of an object */
	char *str;
	double num;
	int bval;
	struct KjNode *child, *next;
} KjNode;

KjNode *kj_parse(const char *text);
void kj_free(KjNode *n);
const KjNode *kj_get(const KjNode *obj, const char *key);
const char *kj_str(const KjNode *obj, const char *key, const char *def);
double kj_num(const KjNode *obj, const char *key, double def);
int kj_bool(const KjNode *obj, const char *key, int def);

/* ──────────────────────────────────────────────────────────────────────── */
/* The build plan
 *
 * A plan is the "work on the tree you have" counterpart to a snapshot: it
 * restores nothing and only narrows what the next run executes. A plan that
 * narrows anything SUPPRESSES snapshot writes — a snapshot taken from a
 * partially re-run tree would be filed under a phase name it no longer
 * represents.
 *
 * All three return a NULL-terminated strv the caller frees with kb_strv_free.
 */

/* A package phase's list is packages.txt OR the files packages.d/<name>.txt.
 * 1 when the phase has one of them, 0 when it has neither (a script phase),
 * -1 when it has both. */
int kbuild_is_package_phase(const KbuildPhase *p);

/* The files that make up a package phase's list, in reading order:
 * packages.txt alone, or every packages.d/<name>.txt in byte order. Empty for
 * any other phase. */
char **kbuild_list_files(const KbuildPhase *p, int *count);

/* The *.sh a script-phase would run, in execution order. A package phase
 * has no steps — the package list IS the work. */
char **kbuild_steps(const KbuildPhase *p, int *count);

/* The names in a phase's list, comments and blanks stripped. packages.d/ is
 * read as its files concatenated in kbuild_list_files order, so splitting a
 * list into files changes nothing a reader sees. */
char **kbuild_packages(const KbuildPhase *p, int *count);

/* The phase's order run, in list order: every name of packages.d/00-order.txt,
 * or the names a packages.txt gives ahead of its first shelf banner
 * (`# <shelf> — <description>`) — all of them when it has none. The rule is
 * testing/phaseclosure.py's. kdosbuild builds the run strictly in order, one
 * port at a time, before anything of the phase builds beside another. Returns
 * the count; `*names` is a NULL-terminated strv the caller frees. */
int kbuild_packages_order_run(const KbuildPhase *ph, char ***names);

/* Every port name under ports/core, src/system and src/art, walked by
 * libkpkg's own walker at both depths, so a dependency-only port is
 * selectable too; src/desktop and src/daemons reach the index through the
 * desktop phase's list. NULL with `err` set when the tree is malformed (a name
 * filed twice, a port nested below its shelf) or ports/core yields no port. */
char **kbuild_ports(const char *repo_root, int *count, char *err,
		    size_t errcap);

typedef struct {
	char name[64];
	char phase[64];		/* "" when no phase's list claims it */
} KbuildPkgRef;

/* -1 with `err` set when kbuild_ports fails. */
int kbuild_package_index(const KbuildPhase *ph, int nph, const char *repo_root,
			 KbuildPkgRef *out, int max, char *err, size_t errcap);

/* The repositories a phase's `kpkg` searches, as paths on the host: the last
 * `[export ]PORT_REPO=` line of its phase.env, read by the rule
 * testing/phaseclosure.py uses, else kpkg.conf's /ports/core. The chroot binds
 * the repository's ports/ at /ports and the repository at /kdos, so /ports/...
 * maps to <repo_root>/ports/... and /kdos/... to <repo_root>/...; any other
 * absolute path is taken under <repo_root>. `out` gets them space-separated,
 * ready for kp_conf_set_repos. Returns the count, or -1 (with `out` empty) when
 * they do not fit in `cap`. */
int kbuild_phase_repos(const KbuildPhase *ph, const char *repo_root,
		       char *out, size_t cap);

typedef struct {
	char dir[64];
	char step[KBUILD_MAX_STEPS][64];
	int n;
} KbuildPlanSteps;

typedef struct {
	/* has_phases == 0 means "every phase", which is not the same as a
	 * selection that happens to be empty — a loaded plan carrying
	 * `"phases": []` runs nothing at all, and that is honoured. */
	int has_phases;
	char phase[KBUILD_MAX_PHASES][64];
	int nphase;

	KbuildPlanSteps steps[KBUILD_MAX_PHASES];
	int nsteps;

	char rebuild[KBUILD_MAX_REBUILD][64];
	int nrebuild;
} KbuildPlan;

/* --phases / --steps / --rebuild. 0 on success; -1 with a message in err. */
int kbuild_plan_from_cli(KbuildPlan *pl, const char *phases_arg,
			 const char *steps_arg, const char *rebuild_arg,
			 const KbuildPhase *ph, int nph,
			 char *err, size_t errcap);

int kbuild_plan_custom(const KbuildPlan *pl);
int kbuild_plan_narrows(const KbuildPlan *pl);
int kbuild_plan_phase_selected(const KbuildPlan *pl, const char *dir_name);
int kbuild_plan_step_selected(const KbuildPlan *pl, const char *dir_name,
			      const char *basename);
int kbuild_plan_forced(const KbuildPlan *pl, const char *package);
void kbuild_plan_summary(const KbuildPlan *pl, char *out, size_t cap);

/* $BUILD_DIR/.devplan.json, written as two-space-indented JSON so the file is
 * legible on disk and round-trips through the loader below. */
int kbuild_plan_save(const KbuildPlan *pl, const char *build_dir);
int kbuild_plan_load(KbuildPlan *pl, const char *build_dir);

/* ──────────────────────────────────────────────────────────────────────── */
/* Snapshots — the inventory and what a restore would extract
 *
 * Creating and extracting archives runs tar as root and belongs to the driver,
 * in src/devtools/kdosbuild/snapshot.c. What lives here is what DECIDES: which
 * snapshots exist, and which archive supplies each path.
 */

#define KBUILD_MANIFEST        "manifest.json"
#define KBUILD_RESTORE_MARKER  ".restore-in-progress"

typedef struct {
	char path[128];		/* relative to build/, e.g. "fs"            */
	char archive[192];	/* "fs.tar.zst", beside the manifest        */
	long long bytes_raw, bytes_compressed, files;
} KbuildSnapEntry;

typedef struct {
	char dir_name[64];	/* the directory under build/snapshots/     */
	char phase_dir[64];
	char phase[64];
	char title[128];
	char codec[16];
	char created_iso[32];
	char git_commit[64];
	int git_dirty;
	double created, duration_s, snapshot_s;
	int schema, steps, total_steps;
	/* 0 for a PARTIAL snapshot taken with [S] mid-phase: restoring one
	 * re-runs that phase rather than continuing past it. */
	int complete;

	KbuildSnapEntry entry[KBUILD_MAX_PATHS];
	int nentries;
} KbuildSnapshot;

typedef struct {
	char path[128];
	char archive[512];	/* absolute                                  */
	char source[64];	/* the snapshot it came from                 */
	char codec[16];
	long long bytes_raw, bytes_compressed, files;
} KbuildRestoreItem;

const char *kbuild_snap_suffix(const char *codec);
void kbuild_snap_decompressor(const char *codec, KbArgv *a);
void kbuild_snap_archive_name(const char *path, const char *codec, char *out,
			      size_t cap);
void kbuild_snap_dir(const char *root, const char *dir_name, char *out,
		     size_t cap);

/* 0 on success. A manifest that does not parse, carries no "entries", or names
 * an archive that is not on disk reads as ABSENT — never as partial. */
int kbuild_snap_load(const char *root, const char *dir_name, KbuildSnapshot *sn);
int kbuild_snap_list(const char *root, KbuildSnapshot *out, int max);
const KbuildSnapshot *kbuild_snap_find(const KbuildSnapshot *snaps, int n,
				       const char *dir_name);

/* Layered, newest-wins: each path comes from the newest snapshot at or below
 * target_index. Returned in path order, which is the extraction order. */
int kbuild_snap_plan_restore(const char *root, const KbuildPhase *ph, int nph,
			     int target_index, KbuildRestoreItem *out, int max);

/* A restore that never finished. Both snapshotting and the next build refuse
 * to run until it is resolved. */
int kbuild_snap_interrupted(const char *build_dir, char *target, size_t cap);

/* Mount points at or under `path`, longest first. */
int kbuild_snap_mounts_under(const char *path, char out[][256], int max);

#endif /* KBUILD_H */
