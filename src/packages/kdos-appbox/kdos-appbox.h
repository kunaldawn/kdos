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

#define DEFAULT_BOX   "kdos-apps"
#define DEFAULT_IMAGE "localhost/kdos-appbox:latest"
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
	/* pack:<id> | image:<ref> | box:<name>, or empty for the image lane */
	char base[256];
	char accent[16];	/* phosphor | amber | ice | bone            */
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

int  box_exists(const char *box);
int  box_state(const char *box, char *buf, size_t n);
int  box_create(const Profile *p);
int  box_remove(const char *box, int force);
int  box_list(void);
int  box_ensure(const char *box);
int  box_setup_done(const char *box);
int  box_wait_ready(const char *box, int seconds);
int  image_exists(const char *image);
int  image_has_label(const char *image, const char *label);

/* ---------------------------------------------------------------- pack.c */

/* Is the pack lane in use? The store has a `base` and kdos-packd answers.
 * The migration seam, and W7-5 deletes it once packs are what ships. */
int  pack_mode(void);
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
int  app_lookup_pack(const char *name, char *cmd, size_t n, char *pack,
		     size_t pn);
int  app_install(const char *box, const char *pkg);
int  app_uninstall(const char *box, const char *pkg);
int  app_refresh(const char *box);
int  app_list(void);

/* ----------------------------------------------------------------- tui.c */

int tui_main(void);

/* ----------------------------------------------------------- launchers.c */

/* Regenerate the launchers, the mime cache, the alien-apps table and the
 * /usr/local/bin shims from an image's /usr/share/applications. */
int cmd_genlaunchers(const char *srcdir, const char *fsroot);

/* ---------------------------------------------------------------- open.c */

/* A path, opened by whatever the freedesktop association says opens it —
 * host app or boxed app, since both are ordinary desktop entries here. */
int cmd_open(int argc, char **argv);

/* --------------------------------------------------------------- image.c */

/* pack / assemble / remap-uids — the appbox image in and out of the repo. */
int cmd_image(int argc, char **argv);

/* ------------------------------------------------------------- box_cmd.c */

/* `kdos-box` — a second name on this binary, basename-dispatched. */
int box_main(int argc, char **argv);

/* ---------------------------------------------------------------- main.c */

extern const char *g_box;

int cmd_run(int argc, char **argv);
int cmd_ensure(void);
int cmd_warmup(void);
int cmd_status(void);

#endif /* KDOS_APPBOX_H */
