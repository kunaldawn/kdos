/*
 * ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   KD's Homebrew Linux Distro
 * ---------------------------------
 *
 * kdos-appbox — alien app runtime and box manager.
 */

#ifndef KDOS_APPBOX_H
#define KDOS_APPBOX_H

#include <stdarg.h>
#include <stddef.h>

#include "kbase.h"

/* g_box's "nothing chosen yet" value: a launch resolves the real box from the
 * app's pack, and one that cannot is refused by name rather than composed. */
#define DEFAULT_BOX   "kdos-apps"
#define APP_TABLE     "/usr/share/kdos/alien-apps"
/* The security-context-v1 engine. Absent on a tree built before M2.5, which is
 * why every use of it is guarded rather than assumed. */
#define KDOS_BOXSOCK  "/usr/bin/kdos-boxsock"

#define MAX_LINE   4096

/* ---------------------------------------------------------------- util.c */

void tracef(const char *fmt, ...);
void notify(const char *summary, const char *body);

/* ----------------------------------------------------------------- box.c */

/*
 * A box's sandbox profile. Every field maps 1:1 onto a distrobox flag, which
 * is the whole point: KDOS does not invent confinement it cannot enforce.
 * The defaults are what an unprofiled `distrobox create` already does, so an
 * existing box behaves identically whether or not it has a profile file.
 */
/* persistence — what happens to what a box writes. An app box is `frozen`
 * and a dev box is `persistent`; those three keys are the whole difference
 * between the two lanes. */
typedef enum {
	PERSIST_PERSISTENT = 0,	/* the upper is on disk and is yours        */
	PERSIST_EPHEMERAL,	/* the upper is tmpfs — gone next login     */
	PERSIST_FROZEN,		/* writes are discarded; an app box         */
} Persistence;

typedef struct {
	char name[64];
	char image[256];
	/* pack:<id> | image:<ref> | box:<name>; empty until the first launch
	 * composes the box and records what it is made of */
	char base[256];
	char accent[16];	/* phosphor | amber | ice | bone | norton | borland | perfect            */
	char grant[128];	/* globals a box may bind past the sandbox allowlist */
	Persistence persist;
	int  netns;      /* 1 = private network namespace (--unshare-netns)  */
	int  netnone;    /* 1 = no network at all         (--network none)   */
	int  ipc;        /* 1 = private IPC namespace     (--unshare-ipc)    */
	int  devsys;     /* 1 = private /dev and /sys     (--unshare-devsys) */
	int  process;    /* 1 = private PID namespace     (--unshare-process)*/
	int  privhome;   /* 1 = private $HOME, not the user's                */
	int  init;       /* 1 = run an init inside the container (--init)    */
	int  wayland;    /* 1 = tagged through kdos-boxsock; 0 = no display  */
	int  audio;
	int  gpu;
	int  autoexport; /* 1 = its apps become host launchers on install    */
	int  pids;       /* --pids-limit, 0 for unlimited                    */
	int  autostop_s; /* idle seconds before `kdos-box gc` stops it, 0 off*/
	/*
	 * `display` is carried and not interpreted: it means nothing to a
	 * container flag. It is held here so that a rewrite of this file keeps
	 * it — a profile writer that knows only its own keys silently deletes
	 * everybody else's.
	 *
	 * `render` IS WHO DRAWS. `auto` (the default) means the machine
	 * decides, `gpu` asks for the card and `software` refuses it; see
	 * profile_render_gpu(), which is the resolved form and the only one
	 * worth acting on here. The software answer sets
	 * LIBGL_ALWAYS_SOFTWARE=1 in the launch environment; the hardware one
	 * sets nothing at all, because Mesa asks the machine the same question
	 * by itself.
	 */
	char display[16];
	char render[16];
	char memory[32]; /* --memory                                          */
	char cpus[16];   /* --cpus                                            */
	/* What `profile_print` reports it could NOT enforce. A setting that
	 * silently does nothing is worse than an absent one, so an unknown key
	 * is kept here and named rather than dropped. */
	char unknown[8][64];
	int  nunknown;
} Profile;

void  profile_defaults(Profile *p, const char *box);
int   profile_load(Profile *p, const char *box);
int   profile_save(const Profile *p);
int   profile_set(Profile *p, const char *kv);
void  profile_print(const Profile *p);
const char *persist_name(Persistence p);
char *profile_path(const char *box);
char *profile_home(const char *box);

/* A DRM render node this user can open, and its path when `out` is not NULL.
 * The question both a `render` decision and a `gpu` grant come down to, and
 * the reason it is an open() and not a stat() is in box.c. */
int  box_render_node(char *out, size_t n);
/* The `render` key resolved against this machine: 1 when the box's graphics
 * are the card's, 0 when they are the CPU's. */
int  profile_render_gpu(const Profile *p);

int  box_exists(const char *box);
int  box_state(const char *box, char *buf, size_t n);
/* Wait a `stopping` container out, then force it back to a startable state.
 * Answers 0 (it was not stuck), 1 (it settled on its own), 2 (killed) or
 * 3 (removed, so the caller must create it again). */
int  box_unstick(const char *box, char *state, size_t n);
int  box_create(const Profile *p);
int  box_remove(const char *box, int force);
int  box_list(void);
int  box_setup_done(const char *box);
int  box_wait_ready(const char *box, int seconds);
int  image_exists(const char *image);
int  image_has_label(const char *image, const char *label);

/* ---------------------------------------------------------------- pack.c */

/* Is the pack lane in use? The store has a `base` and kdos-packd answers.
 * The migration seam, and W7-5 deletes it once packs are what ships. */
const char *pack_store(void);

/* 0 the daemon said ok, 1 it said err, -1 there is no daemon. A caller must
 * tell the last two apart: they send a person to different places. */
int  packd_ask(const char *req, char *out, size_t n);
char *pack_list(void);
int  pack_of_command(const char *cmd, char *id, size_t n);

/* Every `env =` the pack's own stack declares, nearest pack first. The pack
 * lane's answer to the image label: which QT_QPA_PLATFORMTHEME works is a fact
 * about the runtime that installed the platform theme, and the runtime is what
 * declares it. Returns how many were written. */
#define PACK_ENV_MAX 24
int  pack_env(const char *id, char out[][256], int max);
int  pack_compose(const char *box, const char *id, char *merged, size_t n);
int  pack_decompose(const char *box);
int  pack_box_create(const Profile *p, const char *merged);
int  pack_box_ensure(const char *box, const char *id);
int  pack_box_start(const char *box);

/* ----------------------------------------------------------------- app.c */

typedef struct {
	char name[64];
	char cmd[512];
} App;

int  app_table_load(App **out);
int  app_lookup(const char *name, char *cmd, size_t n);
/* The same, also answering which pack provides it — the third field of the
 * alien-apps table, empty on a table written before the pack lane. */
int  app_pack_by_exec(const char *exec, char *pack, size_t pn);
/* The basename of the program a command line runs, `env`/`sh -c` skipped. */
void app_exec_key(const char *cmdline, char *out, size_t n);
int  app_lookup_pack(const char *name, char *cmd, size_t n, char *pack,
		     size_t pn);
int  app_list(void);

/* ----------------------------------------------------------- launchers.c */

/* Regenerate the launchers, the mime cache, the alien-apps table and the
 * /usr/local/bin shims from an image's /usr/share/applications. */
int cmd_genlaunchers(const char *srcdir, const char *fsroot, int user, int packsdir);

/* ---------------------------------------------------------------- open.c */

/* A path, opened by whatever the freedesktop association says opens it —
 * host app or boxed app, since both are ordinary desktop entries here. */
int cmd_open(int argc, char **argv);

/* ------------------------------------------------------------- box_cmd.c */

/* `kdos-box` — a second name on this binary, basename-dispatched. */
int box_main(int argc, char **argv);

/* ----------------------------------------------------------- catalogue.c */

/*
 * The catalogue: every application this system knows how to build, as a parent
 * chain of apt packages. Shipped at CAT_PATH and read by the store surface,
 * kinstall and `kdos app` alike — there is no second copy.
 */
#define CAT_PATH       "/usr/share/kdos/appstore/catalogue"
#define CAT_ID_MAX     64
#define CAT_MAX_PACKS  512
#define CAT_MAX_GROUPS 32
/* A chain is base -> runtime -> runtime -> app; sixteen is four times the
 * deepest the catalogue has and bounds a file that names a parent loop. */
#define CAT_CHAIN_MAX  16

typedef struct {
	char id[CAT_ID_MAX];
	char kind[8];			/* base | runtime | app | data       */
	char parent[CAT_ID_MAX];	/* "" for a base                     */
	char image[256];		/* a base's own ref, else ""         */
	char name[128];			/* meta, else the id                 */
	char category[32];		/* meta, else "Other"                */
	char tagline[192];
	unsigned long long bytes;	/* meta: an installed-size ESTIMATE  */
	char *packages;			/* space-separated, "" when none     */
} CatPack;

typedef struct {
	char id[CAT_ID_MAX];
	char desc[128];
	char members[CAT_MAX_PACKS][CAT_ID_MAX];
	int  nmember;
} CatGroup;

/* `path` NULL means $KDOS_CATALOGUE, else CAT_PATH. The override is what the
 * fixture test sets and is the only reason it exists. */
int  cat_load(const char *path, char *err, size_t errn);
void cat_free(void);

int  cat_count(void);
const CatPack  *cat_at(int i);
const CatPack  *cat_find(const char *id);
int  cat_ngroups(void);
const CatGroup *cat_group_at(int i);
const CatGroup *cat_group_find(const char *id);

/* Walk `id` up to its base, BASE-FIRST because that is the build order — a
 * runtime is built FROM its base, so the other order names an image that does
 * not exist yet. -1 when a parent is missing or the chain is too long. */
int cat_chain(const char *id, const CatPack *out[CAT_CHAIN_MAX]);

/* The whole chain's env rows, base-first: a runtime's variable has to be set
 * for an application that never declares one. */
int cat_env(const char *id, char out[][256], int max);
int cat_cmds(const char *id, char out[][64], int max);
int cat_needs(const char *id, char out[][CAT_ID_MAX], int max);
int cat_deb(const char *id, char *url, size_t un, char *pat, size_t pn);
/* `box` selects boxgraft over graft. */
int cat_grafts(const char *id, int box, char from[][256], char to[][256],
	       int max);
const char *cat_snapshot(void);
/* The catalogue's own identity, for an exported set's provenance. */
const char *cat_version(void);

/* Expand ids, any of which may be a group, into app/data ids with duplicates
 * removed — installing one twice is a second build of an image that exists.
 * -1 with `err` filled when a name is neither. */
int cat_expand(const char *const *ids, int n, char out[][CAT_ID_MAX], int max,
	       char *err, size_t errn);

/* One line per app for the surfaces, or `--groups`. */
int cmd_catalogue(int argc, char **argv);

/* The assertion helpers the two --selftest seams share. One copy, so a failure
 * counts the same however it was reached. */
extern int cat_fail;
void cat_chk(int cond, const char *what);
int  cat_selftest(void);

/* --------------------------------------------------------------- store.c */

/*
 * Building an application out of the catalogue, on this machine.
 *
 * An image per catalogue row, each FROM the one below, so a runtime's layers
 * are stored once however many applications sit on it.
 */
#define STORE_BASE_IMAGE "debian:trixie-slim"
#define STORE_IMG_PREFIX "kdos/"
/* An EMPTY build context. Nothing is COPY'd in, and handing podman a real
 * directory makes it read and hash whatever is there for nothing. */
#define STORE_EMPTY_CTX  "/var/empty"

/* The Containerfile for one row. Pure — no network, no podman, no filesystem —
 * which is why it is the seam --selftest drives. -1 when it would not fit:
 * half a Containerfile builds an image missing whatever the tail installs. */
int store_containerfile(const CatPack *p, const CatPack *parent,
			const char *snapshot, char *out, size_t n);

/* `snapshot = auto` resolved against the base image, once per process. */
const char *store_snapshot(void);

int store_install(const char *id, int dry);
int store_uninstall(const char *id);
/* Over a list that may name groups. Returns how many failed; every outcome is
 * printed as it happens and nothing rolls back. */
int store_install_many(const char *const *ids, int n, int dry);
int store_uninstall_many(const char *const *ids, int n);
/* Regenerate the user's launchers, shims and mime cache from every store box.
 * The pack walk cannot see one: a store box is an IMAGE and kdos-packd has
 * never heard of it, so an application installed here would have no Start menu
 * entry, no $PATH shim and no "Open with" row. Returns how many were read. */
int store_launchers(void);

/* A set, as signed packs in one file. Packs and not `podman save`: kdos-packd
 * hashes and signature-checks a pack where it MOUNTS it, which is what makes
 * an imported application verified where a store-installed one is not. */
int store_export(const char *out, const char *const *ids, int n);
/* The SELECTION manifest — flat and commentable, so a set can be diffed and
 * hand-edited. A `group` line records what was picked; the id lines are what
 * is installed. */
int store_selection(const char *const *ids, int n, const char *catver,
		    char *out, size_t outn);
int store_selection_parse(const char *text, char out[][CAT_ID_MAX], int max);
/* Stage each pack through kdos-packd — which verifies it where it MOUNTS it —
 * and create a box per id. `n == 0` imports the whole SELECTION. */
int store_import(const char *archive, const char *const *ids, int n);
int store_selftest(void);

/* ---------------------------------------------------------------- main.c */

extern const char *g_box;

int cmd_run(int argc, char **argv);
int cmd_warmup(void);
int cmd_status(void);

#endif /* KDOS_APPBOX_H */
