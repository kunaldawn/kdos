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
typedef struct {
	char name[64];
	char image[256];
	int  netns;      /* 1 = private network namespace (--unshare-netns)  */
	int  ipc;        /* 1 = private IPC namespace     (--unshare-ipc)    */
	int  devsys;     /* 1 = private /dev and /sys     (--unshare-devsys) */
	int  process;    /* 1 = private PID namespace     (--unshare-process)*/
	int  privhome;   /* 1 = private $HOME, not the user's                */
	int  init;       /* 1 = run an init inside the container (--init)    */
} Profile;

void  profile_defaults(Profile *p, const char *box);
int   profile_load(Profile *p, const char *box);
int   profile_save(const Profile *p);
int   profile_set(Profile *p, const char *kv);
void  profile_print(const Profile *p);
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
int  image_has_qt_gtk(const char *image);

/* ----------------------------------------------------------------- app.c */

typedef struct {
	char name[64];
	char cmd[512];
} App;

int  app_table_load(App **out);
int  app_lookup(const char *name, char *cmd, size_t n);
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

/* --------------------------------------------------------------- image.c */

/* pack / assemble / remap-uids — the appbox image in and out of the repo. */
int cmd_image(int argc, char **argv);

/* ---------------------------------------------------------------- main.c */

extern const char *g_box;

int cmd_run(int argc, char **argv);
int cmd_ensure(void);
int cmd_warmup(void);
int cmd_status(void);

#endif /* KDOS_APPBOX_H */
