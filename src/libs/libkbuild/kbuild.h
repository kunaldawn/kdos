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
/* Snapshots — the inventory, the chains, and what a restore would extract
 *
 * Creating and extracting archives runs tar as root and belongs to the driver,
 * in src/devtools/kdosbuild/snapshot.c. What lives here is what DECIDES: which
 * snapshots exist, which chain of archives supplies each path, what a layer
 * holds, and which held snapshots nothing needs any more.
 *
 * A snapshot path is archived FULL (the whole tree) or as a LAYER: only the
 * entries that changed since the snapshot named as its base, plus a `.gone`
 * list of the paths to delete before extracting it. A layer's base is another
 * snapshot's id, so restoring a path extracts its chain base first.
 */

#define KBUILD_MANIFEST        "manifest.json"
#define KBUILD_RESTORE_MARKER  ".restore-in-progress"
#define KBUILD_SNAP_SCHEMA     4
#define KBUILD_MAX_CHAIN       64	/* archives one path's restore may read */
#define KBUILD_MAX_SNAPS       128	/* phase directories plus held ones   */
#define KBUILD_MAX_RESTORE     (KBUILD_MAX_PATHS * KBUILD_MAX_CHAIN)
#define KBUILD_LINEAGE_DIR     ".snap-lineage"	/* under build/             */
#define KBUILD_HELD_DIR        ".held"		/* under build/snapshots/   */
#define KBUILD_SNAP_ID         96

typedef struct {
	char path[128];		/* relative to build/, e.g. "fs"            */
	char archive[192];	/* "fs.tar.zst", beside the manifest        */
	/* bytes_raw and files describe THIS archive; tree_bytes and tree_files
	 * the whole path as it stood when the snapshot was taken. */
	long long bytes_raw, bytes_compressed, files;
	int layer;		/* 0: the whole tree; 1: changes on base_id */
	char base_id[KBUILD_SNAP_ID];
	char removed[192];	/* "fs.gone" beside the manifest, "" if none */
	long long removed_count;
	long long tree_bytes, tree_files;
} KbuildSnapEntry;

typedef struct {
	/* Relative to build/snapshots/: "41_system", or for a held snapshot
	 * ".held/41_system@<id>". */
	char dir_name[192];
	char id[KBUILD_SNAP_ID];
	int held;
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
	char removed[512];	/* absolute .gone list, "" when there is none */
	char source[64];	/* the phase directory the snapshot is from  */
	char id[KBUILD_SNAP_ID];
	char codec[16];
	int layer;
	int seq, nseq;		/* this archive's place in the path's chain  */
	int complete;		/* of the snapshot this archive belongs to   */
	long long bytes_raw, bytes_compressed, files;
} KbuildRestoreItem;

const char *kbuild_snap_suffix(const char *codec);
void kbuild_snap_decompressor(const char *codec, KbArgv *a);
void kbuild_snap_archive_name(const char *path, const char *codec, char *out,
			      size_t cap);
/* "fs.gone" for "fs", with every '/' of the path turned into '_'. */
void kbuild_snap_gone_name(const char *path, char *out, size_t cap);
void kbuild_snap_dir(const char *root, const char *dir_name, char *out,
		     size_t cap);

/* One manifest, schema 4 (`paths`) or 3 and older (`entries`, always full,
 * with the id "legacy-<phase_dir>-<created x 10>"). 0 on success. A manifest
 * that does not parse, carries neither array, or names an archive or a .gone
 * list that is not on disk reads as ABSENT — never as partial. Chains are not
 * checked here. */
int kbuild_snap_load_dir(const char *root, const char *dir_name,
			 KbuildSnapshot *sn);
/* The phase directory `dir_name`, by the same rules. */
int kbuild_snap_load(const char *root, const char *dir_name, KbuildSnapshot *sn);

/* Every snapshot: the phase directories, then build/snapshots/.held/. A name
 * beginning with '.' in the root is never a phase snapshot. */
int kbuild_snap_list_all(const char *root, KbuildSnapshot *out, int max);
/* The phase directories whose every path's chain resolves. A snapshot whose
 * base is gone is treated as absent: restoring its layer alone would produce
 * a tree that never existed. */
int kbuild_snap_list(const char *root, KbuildSnapshot *out, int max);
const KbuildSnapshot *kbuild_snap_find(const KbuildSnapshot *snaps, int n,
				       const char *dir_name);
const KbuildSnapshot *kbuild_snap_by_id(const KbuildSnapshot *all, int n,
					const char *id);
const KbuildSnapEntry *kbuild_snap_entry(const KbuildSnapshot *sn,
					 const char *path);

/* The archives that rebuild `path` as `top` has it, base (a full archive)
 * first. Returns the length, or -1 when a base is missing, lacks the path,
 * the chain loops, or it is longer than `max` or KBUILD_MAX_CHAIN. */
int kbuild_snap_chain(const KbuildSnapshot *all, int n,
		      const KbuildSnapshot *top, const char *path,
		      const KbuildSnapshot **out, int max);
/* 1 when every path of `sn` has a chain. */
int kbuild_snap_usable(const KbuildSnapshot *all, int n,
		       const KbuildSnapshot *sn);
/* Indices into `all` of the snapshots whose chain for any path passes
 * through `id` (the snapshot itself excluded). */
int kbuild_snap_dependants(const KbuildSnapshot *all, int n, const char *id,
			   int *out, int max);
/* Indices into `all` of the held snapshots no phase directory's chain
 * reaches: what can be deleted without making anything unrestorable. */
int kbuild_snap_gc_set(const KbuildSnapshot *all, int n, int *out, int max);

/* KDOS_SNAPSHOT_EXCLUDE: fnmatch(pattern, relpath, 0), the path relative to
 * build/ — `*` crosses '/'. An excluded directory is left out whole. */
int kbuild_snap_exclude_match(const KbuildPhase *p, const char *relpath);
/* Walk order: byte order with '/' below every other byte, which is the order
 * of a pre-order walk that visits each directory's names sorted. */
int kbuild_snap_path_cmp(const char *a, const char *b);
/* A .gone entry is `path` itself or under it, relative, with no "..", "."
 * or empty component. */
int kbuild_snap_removal_ok(const char *path, const char *entry);

/* ── The index of the live tree ───────────────────────────────────────────
 *
 * build/.snap-lineage/<path, '/' as '_'>.idx describes the tree as the
 * snapshot `head` archived it (or as a restore of `head` left it): one record
 * per entry in walk order, "<type-octal> <ino> <ctime_ns> <alloc>\t<path>\0",
 * after a header of "kdos-snap-index 1", "head <id>", "phase <index>
 * <phase_dir>", "partial 0|1" and "root <st_dev> <st_ino>" lines. Any change
 * to an entry — content, mode, owner, xattr, link count, replacement — moves
 * its ctime or its inode, so comparing two indexes finds every change.
 */
typedef struct {
	unsigned mode;		/* st_mode & S_IFMT                          */
	unsigned long long ino;
	long long ctime_ns;
	long long alloc;	/* allocated bytes of a regular file, else 0 */
	size_t off;		/* the path, in `pool`                       */
} KbuildIdxRec;

typedef struct {
	char head[KBUILD_SNAP_ID];
	int phase_index;
	char phase_dir[64];
	int partial;
	unsigned long long root_dev, root_ino;
	KbuildIdxRec *rec;
	size_t n, cap;
	KbBuf pool;
} KbuildSnapIndex;

#define kbuild_idx_path(ix, i) ((ix)->pool.p + (ix)->rec[(i)].off)

void kbuild_snap_idx_file(const char *build_dir, const char *path, char *out,
			  size_t cap);
void kbuild_snap_idx_add(KbuildSnapIndex *ix, unsigned mode,
			 unsigned long long ino, long long ctime_ns,
			 long long alloc, const char *path);
void kbuild_snap_idx_free(KbuildSnapIndex *ix);
/* 0 on success. A file that is short, malformed, or out of walk order reads
 * as absent, and the next snapshot of that path is full. */
int kbuild_snap_idx_read(const char *file, KbuildSnapIndex *ix);
/* Written to "<file>.tmp" and renamed. 0 on success. */
int kbuild_snap_idx_write(const char *file, const KbuildSnapIndex *ix);

typedef struct {
	long long listed;	/* entries in the layer                      */
	long long removed;	/* entries in the .gone list                 */
	long long listed_alloc;	/* allocated bytes of its regular files      */
} KbuildSnapDiff;

/* What a layer of `cur` over `base` holds, as two NUL-separated lists in walk
 * order: `add` gets every entry that is new, changed type, or changed inode
 * or ctime, the directory holding each changed or removed entry (extracting
 * or deleting inside a directory resets its times, and the layer puts them
 * back), and always the root of the path. `gone` gets the top-most removed
 * paths and every path whose type changed, to delete before extracting. Both
 * indexes must be in walk order. */
void kbuild_snap_diff(const KbuildSnapIndex *base, const KbuildSnapIndex *cur,
		      KbBuf *add, KbBuf *gone, KbuildSnapDiff *st);

/* Newest-wins per path: each path comes from the newest phase snapshot at or
 * below target_index, and is expanded into its chain. Returned sorted by path
 * and then chain position, which is the extraction order. */
int kbuild_snap_plan_restore(const char *root, const KbuildPhase *ph, int nph,
			     int target_index, KbuildRestoreItem *out, int max);

/* A restore that never finished. Both snapshotting and the next build refuse
 * to run until it is resolved. */
int kbuild_snap_interrupted(const char *build_dir, char *target, size_t cap);

/* Mount points at or under `path`, longest first. */
int kbuild_snap_mounts_under(const char *path, char out[][256], int max);

#endif /* KBUILD_H */
