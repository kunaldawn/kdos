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
 * This is the part of script/buildlib that is pure inspection of the repo: it
 * reads, it decides nothing, and it runs nothing.
 *
 * THE ENV FILES ARE PARSED, NEVER SOURCED. Several of them end with
 * `rm -rf /var/cache/kpkg/work`, which at source time hits the BUILD
 * CONTAINER's own filesystem rather than the target's. That is why only
 * `export NAME=VALUE` lines are read, why only five keys are honoured, and
 * why a value has to be a literal — no expansion is performed and anything
 * that is not a plain literal reads as empty.
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

typedef struct {
	int index;
	char dir_name[64];	/* 03_phase3                               */
	char dir_path[512];
	char name[64];		/* phase3                                  */
	char env_file[512];	/* script/phase3.env.sh, "" when absent    */

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
} KbuildPhase;

/* Ordered by directory name, which is what the numeric prefix is for. Only
 * directories matching ^[0-9]+_ are phases; buildlib/, util/ and __pycache__/
 * are not. */
int kbuild_discover(const char *script_dir, KbuildPhase *out, int max);

/* A phase with no KDOS_SNAPSHOT_PATHS is never snapshotted. */
int kbuild_snapshottable(const KbuildPhase *p);

/* "phase3" or "phase3 (Chroot)" — the label the picker shows. */
void kbuild_label(const KbuildPhase *p, char *out, size_t cap);

/* A snapshot path must stay inside $BUILD_DIR. Snapshot and restore delete
 * and re-extract these as root, so anything absolute, empty, or climbing out
 * with ".." is refused rather than sanitised. */
int kbuild_safe_relpath(const char *path);

/* The literal on the right of an `export NAME=`. No shell expansion: an
 * unquoted value ends at whitespace or a comment, and an unterminated quote
 * reads as empty. */
void kbuild_unquote(const char *raw, char *out, size_t cap);

/* A phase by its directory name OR its short name, so both `04_phase4` and
 * `phase4` resolve. NULL when neither matches. */
const KbuildPhase *kbuild_find(const KbuildPhase *ph, int n, const char *token);

/* ──────────────────────────────────────────────────────────────────────── */
/* JSON — read only, and only for files python wrote
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
int kj_len(const KjNode *n);

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

/* The *.sh a script-phase would run, in execution order. A packages.txt phase
 * has no steps — the package list IS the work. */
char **kbuild_steps(const KbuildPhase *p, int *count);

/* The names in a phase's packages.txt, comments and blanks stripped. */
char **kbuild_packages(const KbuildPhase *p, int *count);

/* Every port name in the repo, so a dependency-only port is selectable too. */
char **kbuild_ports(const char *repo_root, int *count);

typedef struct {
	char name[64];
	char phase[64];		/* "" when no packages.txt claims it */
} KbuildPkgRef;

int kbuild_package_index(const KbuildPhase *ph, int nph, const char *repo_root,
			 KbuildPkgRef *out, int max);

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

/* $BUILD_DIR/.devplan.json, byte-identical to python's json.dump(indent=2) so
 * the file round-trips while build.py still drives the build. */
int kbuild_plan_save(const KbuildPlan *pl, const char *build_dir);
int kbuild_plan_load(KbuildPlan *pl, const char *build_dir);

/* ──────────────────────────────────────────────────────────────────────── */
/* Snapshots — the inventory and what a restore would extract
 *
 * Creating and extracting archives runs tar as root and is still build.py's.
 * What lives here is what DECIDES: which snapshots exist, and which archive
 * supplies each path.
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
