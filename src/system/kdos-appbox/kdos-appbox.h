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
/* The security-context-v1 engine, a dependency of this port. $KDOS_BOXSOCK
 * names another one, which is how the selftest drives the launch path without
 * a compositor; it can only ever run as the user who set it. */
#define KDOS_BOXSOCK  "/usr/bin/kdos-boxsock"

#define MAX_LINE   4096

/* ---------------------------------------------------------------- util.c */

void tracef(const char *fmt, ...);
void notify(const char *summary, const char *body);
unsigned long long proc_start_time(long pid);

/* Where writes to `path` land, asked of its nearest existing directory. A
 * live session's $HOME is an overlay whose upper is RAM, and a box made there
 * keeps its writes only as long as the session. */
#define FS_OVERLAY_MAGIC 0x794c7630UL
#define FS_TMPFS_MAGIC   0x01021994UL
int  fs_in_memory(const char *path);	/* overlayfs or tmpfs */
int  fs_is_overlay(const char *path);
unsigned long long fs_free_bytes(const char *path);
void podman_storage_dir(char *out, size_t n);
/* Run with stderr captured; on a failure `why` is its last line that says
 * something (podman's `Error: ` dropped), else the exit status. Returns the
 * child's status. */
int  run_last_line(const KbArgv *a, char *why, size_t n);

/* ----------------------------------------------------------------- box.c */

/*
 * A box's sandbox profile. Every field that changes a launch maps 1:1 onto a
 * container-engine flag, which is the whole point: KDOS does not invent
 * confinement it cannot enforce. The two that change nothing are marked below,
 * and profile_print() prints each of them as unenforced rather than letting it
 * read as a switch.
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
	char accent[16];	/* a libkcolor scheme name; "" = the neutral box scheme */
	/* What the box may reach past the sandbox: KWin globals and bus names,
	 * by the names in box.c's grant table. */
	char grant[128];
	Persistence persist;
	int  netns;      /* 1 = private network namespace (--unshare-netns)  */
	int  netnone;    /* 1 = no network at all         (--network none)   */
	int  ipc;        /* 1 = private IPC namespace     (--unshare-ipc)    */
	int  devsys;     /* 1 = private /dev and /sys     (--unshare-devsys) */
	int  process;    /* 1 = private PID namespace     (--unshare-process)*/
	int  privhome;   /* 1 = private $HOME, not the user's                */
	int  init;       /* 1 = run an init inside the container (--init)    */
	/* What the box's private runtime directory holds. A box whose
	 * /run/user/<uid> is that directory reaches no socket it was not given,
	 * so `wayland = no` (no tagged socket) and `audio = no` (no PipeWire or
	 * PulseAudio socket) are enforced there; a box created over the
	 * session's runtime directory shares every socket in it whatever these
	 * say, and profile_print() reports that. */
	int  wayland;
	int  audio;
	int  gpu;
	/* `yes` (the default) or `no`: whether the box is handed the
	 * session's X11 display and its cookie. profile_x11() reads it; see
	 * the comment there for what each costs. */
	char x11[8];
	/* Recorded and printed, never acted on: `kdos-appbox genlaunchers`
	 * writes launchers for every installed pack and every store box at
	 * once, and `kdos-box export` is the per-application route. */
	int  autoexport;
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
/* An accent as a person reads it: `none` for no accent, whose stored token is
 * `session`. */
const char *accent_shown(const char *accent);
/* 1 for no accent or a libkcolor accent name; 0 for a name the title-bar rule
 * cannot colour, which falls back to the neutral scheme. */
int   accent_known(const char *accent);
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

/*
 * WHAT MADE THE BOX, which decides what it can be given. A pack box is
 * `podman create --rootfs` over kdos-packd's overlay; a store box is the same
 * create over an image this machine built (`base=image:kdos/<id>`); both see
 * their own runtime directory as /run/user/<uid>. A box `distrobox create`
 * made from somebody else's image shares the session's.
 */
enum { BOX_LANE_PACK, BOX_LANE_STORE, BOX_LANE_DISTROBOX };
int  box_lane(const Profile *p);

/* Whether `grant` names `name`, by the separators KWin's grant check splits
 * on: spaces, commas and tabs. */
int  profile_granted(const Profile *p, const char *name);
/* The `x11` key resolved: 1 when the box is handed the session's display. */
int  profile_x11(const Profile *p);
/* 1 when the profile is valid for a container name, a profile file name and
 * a socket path component. */
int  box_name_ok(const char *s);

/* ─────────────────────────────────────────────────────────────── rundir.c */

/*
 * THE BOX'S PRIVATE RUNTIME DIRECTORY, $XDG_RUNTIME_DIR/kdos/box/<box>, is
 * what the box sees as its whole /run/user/<uid>: its tagged Wayland sockets,
 * its proxied bus, links to the audio sockets, the
 * accessibility bus when it is wanted and the X11 cookie when it is granted.
 * The session's Wayland socket (KWin's `wayland-1`) and `bus` are never in
 * it.
 */
#define BOX_PROXY_LEAF "dbus-proxy"

/* $XDG_RUNTIME_DIR, else /run/user/<uid> — never /tmp, which a box shares. */
const char *box_rundir(void);
/* Whether boxes get the accessibility bus: ~/.config/kdos/a11y, or
 * KDOS_A11Y=1 for one launch. */
int  box_a11y_wanted(void);
/* /run/user/<uid>: the path the box sees its private directory at. */
const char *box_inbox_rundir(void);
/* 0 and the path, or -1 when it would not fit a Unix socket address with a
 * socket leaf under it. */
int  box_private_dir(const char *box, char *out, size_t n);
/* $XDG_RUNTIME_DIR/kdos/boxes, created this user's: the state files and the
 * launch ledgers. Made before kdos-packd is asked anything, because the
 * daemon creates the same parent as root when it gets there first. */
int  box_state_dir(char *out, size_t n);
/* Make the private directory and refresh what it links. Returns 0, or -1
 * with the reason printed. */
int  rundir_prepare(const Profile *p);
/* The audio and accessibility sockets, linked into the private directory.
 * Idempotent; the keeper runs it again whenever one of them is replaced. */
void rundir_link_sockets(const char *box, int audio, int a11y);
/* The path a file in the private directory has INSIDE the box: under
 * /run/user/<uid> for a private-layout box, at its host path for one that
 * shares the session's runtime directory. */
int  rundir_inbox_path(const char *box, int private_layout, const char *leaf,
		       char *out, size_t n);

/* What the container was created over: 1 when its /run/user/<uid> is the
 * private directory, 0 when it is the session's, -1 when there is no such
 * container. */
int  box_layout_private(const char *box);
/* The cgroup a new pack box is created under: a scope in the current login
 * session's delegated subtree, or 0 and "" when there is none to use. */
int  box_cgroup_parent(const char *box, char *out, size_t n);
/* 1 when a container's recorded cgroup parent can no longer be created
 * again — the login session that held it has gone. */
int  box_cgroup_stale(const char *box);

/*
 * WHICH BOUNDARY HELD, recorded for every launch in
 * $XDG_RUNTIME_DIR/kdos/boxes/<box>.state and read by the Boxes page.
 */
typedef enum {
	CONF_UNCONFINED = 0,	/* no tag: the launch was refused             */
	CONF_TAGGED,		/* tagged, and the box can reach past the tag */
	CONF_CONFINED,		/* tagged, private runtime, filtered bus, no X11 */
} Confinement;

typedef struct {
	int  tagged;		/* kdos-boxsock served the launch's socket    */
	int  private_rundir;	/* the container's runtime dir is the box's   */
	int  bus;		/* 1 the keeper's filtered proxy, 0 none      */
	int  x11;		/* the X11 display is reachable from the box  */
	char launcher[128];
	char reason[256];	/* why it is not confined, for a person       */
} BoxBoundary;

Confinement boundary_state(const BoxBoundary *b);
const char *confinement_name(Confinement c);
/* 1 when the session's X11 cookie is somewhere the box can read without
 * being handed it: under /tmp, or under a $HOME the box shares. */
int  x11_cookie_exposed(const Profile *p);
int  box_state_write(const char *box, const BoxBoundary *b);
void box_state_clear(const char *box);

/* The launch ledger, $XDG_RUNTIME_DIR/kdos/boxes/<box>.apps: one line per
 * running launch, which `session save` reads and the keeper keeps current. */
void box_ledger_add(const char *box, const char *launcher, int argc,
		    char **argv);

/* ──────────────────────────────────────────────────────────────── keeper.c */

/* `kdos-appbox keeper <box>`: the box's bus proxy, its socket links and its
 * launch ledger, for as long as the box's container runs. Started detached
 * by a launch; a second one for the same box exits at once. */
int  cmd_keeper(const char *box);
/* Start the keeper if none runs, and wait for the proxied bus. 0 when the
 * box's bus answers at BOX_PROXY_LEAF, -1 with `why` filled otherwise. */
int  keeper_ensure(const char *box, char *why, size_t n);
/* The xdg-dbus-proxy arguments after the two addresses, for `p`. Pure: what
 * `--selftest` asserts the policy against. Returns how many were written. */
int  proxy_policy(const Profile *p, const char *const *owns, int nowns,
		  int a11y, char out[][192], int max);

/* ───────────────────────────────────────────────────────────────── rules.c */

/*
 * One KWin window rule per box in kwinrulesrc, in groups named
 * `kdos-box-<box>`: the title bar takes the box's accent scheme. Every other
 * group is left as it was. `path` NULL means ~/.config/kwinrulesrc.
 * Returns how many rules were written, or -1.
 */
int  rules_write(const char *path, const char *const *boxes,
		 const char *const *accents, int n);
/* The colour scheme a box's title bar is forced to: KDOS<Title> of its
 * accent, or KDOSBox for a box with none. */
void rules_scheme(const char *accent, char *out, size_t n);
/* Every box the user has — launchers and profiles — written as rules into
 * ~/.config/kwinrulesrc, and KWin told to read them. */
int  box_rules_refresh(void);
void kwin_reconfigure(void);

/* ─────────────────────────────────────────────────────────────── session.c */

/* `kdos-appbox session save|restore`: the boxes a logout leaves running, and
 * the box applications KDE's own session restore does not bring back. */
int  cmd_session(int argc, char **argv);

/* ───────────────────────────────────────────────────────────── throwaway.c */

/* `kdos-appbox throwaway [--pack <id>] <file>`: the file, read-only, in a
 * fresh box with no network that is removed when the application exits. */
int  cmd_throwaway(int argc, char **argv);

/* ──────────────────────────────────────────────────────────────── boxtest.c */

/* `kdos-appbox boxes --selftest`: names, launcher ids, the proxy policy, the
 * confinement states and the window rules, with no engine and no session. */
int  boxes_selftest(void);

int  box_exists(const char *box);
int  box_state(const char *box, char *buf, size_t n);
/* Force a `stopping` container back to a startable state with the stop
 * sequence. Answers 0 (it was not stuck), 2 (killed), 3 (a pack box, removed,
 * so the caller must create it again) or 4 (any other box, still stuck and
 * kept, because its writes are in the container). */
int  box_unstick(const char *box, char *state, size_t n);
/* `sig` to every process of the box but its init — the applications its
 * launches started — by the init's mount namespace, or by KDOS_BOX once the
 * init is gone. Signal 0 counts them. Returns how many were signalled. */
int  box_signal_all(const char *box, int sig);
/* Stop boxes without `podman stop`: SIGTERM to the applications and the init,
 * `grace` seconds, then SIGKILL and three more. Returns how many are still
 * up. */
int  box_stop_many(const char *const *boxes, int n, int grace);
int  box_create(const Profile *p);
int  box_list(void);
int  box_setup_done(const char *box);
int  box_wait_ready(const char *box, int seconds);
int  image_exists(const char *image);
int  image_has_label(const char *image, const char *label);

/* ---------------------------------------------------------------- pack.c */

/* Where the packs and their staging directory live: $KDOS_PACK_STORE when it
 * is set, else /var/lib/kdos/packs. A caller that spells the default itself
 * instead of asking loses the override, and a bake writing staging and a mount
 * reading it then land in two different stores. */
const char *pack_store(void);

/* 0 the daemon said ok, 1 it said err, -1 there is no daemon. A caller must
 * tell the last two apart: they send a person to different places. */
int  packd_ask(const char *req, char *out, size_t n);
char *pack_list(void);

/* Every `env =` the pack's own stack declares, nearest pack first. The pack
 * lane's answer to the image label: which QT_QPA_PLATFORMTHEME works is a fact
 * about the runtime that installed the platform theme, and the runtime is what
 * declares it. Returns how many were written. */
#define PACK_ENV_MAX 24
int  pack_env(const char *id, char out[][256], int max);
int  pack_compose(const char *box, const char *id, char *merged, size_t n);
int  pack_decompose(const char *box);
/* `extra` is appended to podman's create flags, before the rootfs: what a
 * throwaway box adds for the one file it is given. `sealed` gives the box a
 * /tmp of its own instead of the host's, which a throwaway box needs and every
 * other box cannot do without (see pack.c). */
int  pack_box_create(const Profile *p, const char *merged,
		     const char *const *extra, int nextra, int sealed);
int  pack_box_ensure(const char *box, const char *id);
int  pack_box_start(const char *box);
/* Why the last create or start failed: podman's last line, or this program's
 * own reason; never empty. */
const char *pack_box_error(void);
void pack_box_set_error(const char *fmt, ...)
	__attribute__((format(printf, 1, 2)));
/* Run podman with its stderr captured. On a failure its last line is printed
 * against `box` and kept for pack_box_error(); returns podman's status. */
int  podman_run_reason(const KbArgv *a, const char *box);

/* ----------------------------------------------------------------- app.c */

/* Which pack provides the app a command line runs — the third field of the
 * alien-apps table, empty where the row predates the pack lane. */
int  app_pack_by_exec(const char *exec, char *pack, size_t pn);
/* The basename of the program a command line runs, `env`/`sh -c` skipped. */
void app_exec_key(const char *cmdline, char *out, size_t n);
int  app_lookup_pack(const char *name, char *cmd, size_t n, char *pack,
		     size_t pn);
/* The same lookup, with the fourth field: the launcher id the row's
 * application runs under, "" for a command-only row. */
int  app_lookup(const char *name, char *cmd, size_t n, char *pack, size_t pn,
		char *launcher, size_t ln);
int  app_list(void);

/* ----------------------------------------------------------- launchers.c */

/* Regenerate the launchers, their icons, the mime cache, the alien-apps
 * table, the /usr/local/bin shims and the boxes' window rules. */
int cmd_genlaunchers(const char *srcdir, const char *fsroot, int user, int packsdir);

/* A generated launcher of a box, read back: its launcher id, the in-box
 * command after `run`, and its MimeType. */
typedef struct {
	char id[128];
	char upstream[128];	/* the application's own desktop id */
	char exec[1024];
	char mime[2048];
} BoxLauncher;

/* The launchers genlaunchers wrote for `box`, the user's tree first, then the
 * skeleton's. Returns how many. */
int box_launchers(const char *box, BoxLauncher *out, int max);
/* 1, and the launcher it runs, when `box` is a throwaway box's name,
 * `<launcher-id>.tw<n>`, and that launcher exists (throwaway.c). */
int throwaway_launcher(const char *box, BoxLauncher *out);
/* The launcher id a box gives one of its launchers: the box alone when the
 * pack exports one launcher, `<box>.<stem>` for each of several. The stem is
 * the shim name with every character a bus or app id component cannot carry
 * turned into `_`. */
void launcher_id(const char *box, const char *stem, int several, char *out,
		 size_t n);

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
/* KDOS's distribution XDG defaults, bound read-only into every box at the
 * same path and named last in each launch's XDG_CONFIG_DIRS. */
#define BOX_KDOS_XDG "/usr/share/kdos/xdg"
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
/* `deb:<package>`, Discover's Debian lane: the box `deb.<package>` (each `+`
 * written `_`) over the base row with that one package installed. 0 done,
 * 1 refused or failed. */
#define STORE_DEB_PREFIX "deb."
int store_deb_install(const char *spec, int dry);
int store_deb_uninstall(const char *spec);
int store_uninstall_many(const char *const *ids, int n);
/* Regenerate the user's launchers, shims and mime cache from every store box
 * and every installed app pack. The pack walk cannot see a store box: it is an
 * IMAGE and kdos-packd has never heard of it, so an application installed here
 * would have no Start menu entry, no $PATH shim and no "Open with" row.
 * Returns how many store boxes were read. */
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
/* The launcher a launch is for, from `-l`, the table, or the box itself. */
extern const char *g_launcher;

int cmd_run(int argc, char **argv);
/* cmd_run that waits for the application instead of becoming it, and
 * returns its exit status: what a throwaway box needs to remove itself. */
int cmd_run_wait(int argc, char **argv);
int cmd_warmup(void);
int cmd_status(void);

#endif /* KDOS_APPBOX_H */
