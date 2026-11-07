/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   kdos-tools — the target's own commands
 *
 * One binary, dispatched on its own basename the way busybox does, installed
 * under every name it answers to. These were nine bash scripts.
 * ---------------------------------
 */

#ifndef KDOS_TOOLS_H
#define KDOS_TOOLS_H

#include <signal.h>
#include <stdarg.h>
#include <stdio.h>

#include "kbase.h"
#include "kcolor.h"

/* Overridable at compile time so the service logic can be exercised outside a
 * booted system. Not an environment hook: these are paths a root tool builds
 * from, and nothing that runs as root should take them from the caller. */
#ifndef INIT_DIR
#define INIT_DIR     "/etc/init.d"
#endif
#ifndef DISABLED_DIR
#define DISABLED_DIR "/etc/service.disabled"
#endif
#ifndef RUN_DIR
#define RUN_DIR      "/run"
#endif

int kdos_main(int argc, char **argv);		/* kdos                     */
int ksvc_main(int argc, char **argv);		/* service, ksvc            */
int getty_main(int argc, char **argv);		/* kdos-getty               */
/* A/B root slots (bootctl.c). The initramfs runs `select`, the end of rcS runs
 * `mark-good`, and everything else is administration. */
int bootctl_main(int argc, char **argv);	/* kdos-bootctl             */

/* `kdos rebuild` (rebuild.c): the stick rebuilding the stick. Finds the sources
 * an ISO built with KDOS_ISO_SOURCES=1 carries, checks the machine can do the
 * work, and hands over to kdosbuild. */
int rebuild_main(int argc, char **argv);

/*
 * `kdos march` (march.c): build a port both ways, benchmark it, and keep the
 * flags only where the win beat the noise. A port with no `bench =` line in its
 * recipe is unmeasurable and is never counted as a winner.
 */
int march_main(int argc, char **argv);

/*
 * `kdos clone` (clone.c): the stick writes the stick. A raw copy of the boot
 * medium onto another device, verified by reading it back — so the boot
 * arrangement is whatever the medium already carries and there is no second
 * opinion about how a KDOS stick is laid out.
 */
int clone_main(int argc, char **argv);
/*
 * `kdos persist` (persist.c): the live session remembers. Makes and reports
 * the ext4 filesystem labelled KDOS_PERSIST that the initramfs uses as the
 * overlay's upper layer, in place of the tmpfs a live session gets by default.
 */
int persist_main(int argc, char **argv);
int shot_main(int argc, char **argv);		/* kdos-shot                */
int banner_main(int argc, char **argv);		/* kdos-banner              */
int fetch_app_main(int argc, char **argv);	/* kdos-fetch-app           */
int fetch_static_main(int argc, char **argv);	/* kdos-fetch-static        */
int sandbox_main(int argc, char **argv);	/* kdos sandbox             */
/*
 * `kdos toggle` (toggle.c): stay-awake, night-light and Do Not Disturb, each
 * switched in the Plasma surface that owns it, so the battery applet, the
 * Night Light applet and the notification bell show what this set.
 */
int cmd_toggle(int argc, char **argv);

int why_main(int argc, char **argv);		/* kdos why                 */
int explain_main(int argc, char **argv);		/* kdos explain             */
int restarts_main(int argc, char **argv);	/* kdos restarts            */
int oracle_main(int argc, char **argv);		/* kdos oracle              */
/* `kdos hey` and `kdos appid` (hey.c, appid.c): the windows KWin has open,
 * read over its window-list protocol (windows.c). */
int hey_main(int argc, char **argv);
int appid_main(int argc, char **argv);

/* One window as KWin reports it. Strings are owned by the list. */
typedef struct {
	char *uuid, *app_id, *resource, *title;
	unsigned long state;
	long pid;
} KdtWindow;

/* Every window, or -1 when there is no session to ask: no Wayland socket, or
 * a compositor that does not offer the window list. -1 is NOT the same
 * answer as 0 windows. */
int kdt_windows(KdtWindow **out);
void kdt_windows_free(KdtWindow *w, int n);
/* One window, by uuid: close, activate, minimize, unminimize, maximize,
 * unmaximize, fullscreen, unfullscreen, keep-above or no-keep-above. 0 done,
 * 1 no such window, 2 no such action, -1 no session. */
int kdt_window_act(const char *uuid, const char *action);
/* The box a process belongs to, by its cgroup (app-kdos-<box>-<n>.scope):
 * 0 and the name in `out`, or -1 for a process in no box's scope. */
int kdt_box_of_pid(long pid, char *out, size_t n);

/* kdos-sfx (sfx.c): the machine's noises, and `render`, which writes the
 * sound theme's files at build time. */
int sfx_main(int argc, char **argv);
/* A basename of its own rather than a `kdos` subcommand, for the reason
 * kdos-sfx has one: a media key runs a program, and names it. */
int mpctl_main(int argc, char **argv);		/* kdos-mpctl               */

/*
 * The reasons corpus, shared by `kdos why`, `kdos explain` and `kdos oracle`.
 * $KDOS_REASONS beats the installed copy, which is how the tree can be queried
 * from a checkout and how preflight reads it.
 */
const char *kdt_reason_dir(void);
char *kdt_reason_header(const char *text, const char *key, int nth);

/* "the aphorism · the scar", picked by (day XOR pid). NULL when nothing is
 * installed. The caller frees. */
char *kdt_oracle_line(void);

/*
 * `kdos update` (update.c). Fetches a signed recipe tree into /ports, verified
 * against /etc/kdos/keys/tree, with the signed binhost index and slot image
 * manifest beside it, and fills the inactive slot from the image, the
 * binhost or a build here, then arms it through kdos-bootctl. `theme` is
 * handed `kdos theme`'s own entry point rather than reimplementing it.
 */
int kdt_update(int argc, char **argv, int (*theme)(int, char **));

/*
 * `kdos image` (image.c): a whole root slot as one signed manifest and the
 * bundles of objects it names, made at release and written into the
 * inactive slot by `kdos update apply`. Every object is checked against the
 * manifest's hash before it is kept, and the manifest against the keyring
 * before it is read.
 */
typedef struct KdtImage KdtImage;
typedef void (*KdtImageTick)(int done, int total, void *data);
int kdt_image_main(int argc, char **argv);
/* The manifest at `path`, verified against the keyring directory `keys`;
 * NULL with the reason in `why`. */
KdtImage *kdt_image_load(const char *path, const char *sigpath,
			 const char *keys, char *why, size_t wcap);
/*
 * The release index (relidx.c): `kdos-update.idx` and its signature, which
 * say where every file of a published binhost and slot image is and what it
 * hashes to. A lane asks for a file by its path (`binhost/PACKAGES`,
 * `image/kdos-image.manifest`) and is given a URL only through it.
 */
#define KDT_RELIDX_FILE "kdos-update.idx"
typedef struct KdtRelIdx KdtRelIdx;
/* How a download ended. */
enum { KDT_GOT_FAILED, KDT_GOT_OFFLINE, KDT_GOT_ABSENT, KDT_GOT_UNVERIFIED,
       KDT_GOT_OK };
KdtRelIdx *kdt_relidx_parse(const char *text, char *why, size_t wcap);
/* The signed index at `place` into `dir` (INDEX, INDEX.sig, ORIGIN), kept
 * only when it verifies against `keys`; `how` is a KDT_GOT_* on failure. */
KdtRelIdx *kdt_relidx_fetch(const char *place, const char *dir,
			    const char *keys, int *how, char *why, size_t wcap);
/* The index cached in `dir`, verified against `keys` again. */
KdtRelIdx *kdt_relidx_load(const char *dir, const char *keys, char *why,
			   size_t wcap);
int kdt_relidx_url(const KdtRelIdx *ix, const char *path, char *url,
		   size_t cap, char sha[65], long long *size);
/* `path` into `out`, kept only with the index's size and hash: KDT_GOT_*. */
int kdt_relidx_take(const KdtRelIdx *ix, const char *path, const char *out,
		    char *why, size_t wcap);
int kdt_relidx_holds(const KdtRelIdx *ix, const char *path, const char *file);
int kdt_relidx_curl(const char *url, const char *out);
/* A producer's index over `files` in `dir`, each at `<lane>/<name>`. */
int kdt_relidx_write(const char *dir, const char *lane, const char *version,
		     char *const *files, int n, const char *key, char *why,
		     size_t wcap);
const char *kdt_relidx_version(const KdtRelIdx *ix);
const char *kdt_relidx_origin(const KdtRelIdx *ix);
const char *kdt_relidx_signer(const KdtRelIdx *ix);
int kdt_relidx_count(const KdtRelIdx *ix);
void kdt_relidx_free(KdtRelIdx *ix);

/* Downloaded through the release index at `origin` into `dir` (INDEX,
 * MANIFEST, their signatures, ORIGIN) and verified; on failure `how` says
 * why, as one of these. */
enum { KDT_IMAGE_FAILED = KDT_GOT_FAILED, KDT_IMAGE_OFFLINE = KDT_GOT_OFFLINE,
       KDT_IMAGE_ABSENT = KDT_GOT_ABSENT,
       KDT_IMAGE_UNVERIFIED = KDT_GOT_UNVERIFIED };
KdtImage *kdt_image_fetch(const char *origin, const char *dir,
			  const char *keys, int *how, char *why,
			  size_t wcap);
void kdt_image_free(KdtImage *m);
const char *kdt_image_version(const KdtImage *m);
const char *kdt_image_arch(const KdtImage *m);
const char *kdt_image_tree(const KdtImage *m);
long long kdt_image_time(const KdtImage *m);
const char *kdt_image_signer(const KdtImage *m);
const char *kdt_image_signer_name(const KdtImage *m);
/* Bytes a root of version `from` downloads: its delta, else the full set. */
long long kdt_image_download_size(const KdtImage *m, const char *from);
int kdt_image_files(const KdtImage *m);
/* The version, tree and time a recorded manifest names, unverified. */
int kdt_image_peek(const char *path, char *ver, size_t vcap, char *tree,
		   size_t tcap, long long *t);
/* The image into `target` (never the running root), copying what `seed`
 * already holds and reading every bundle through the release index `ix`;
 * `tick` reports files in place, `stop` ends it early. */
int kdt_image_fill(const KdtImage *m, const KdtRelIdx *ix, const char *target,
		   const char *seed, KdtImageTick tick, void *data,
		   volatile sig_atomic_t *stop, char *why, size_t wcap);
/* The running root's configuration and machine state into a slot; `db` is
 * the running root's package database directory, `root_dev` the slot's own
 * fstab root device or NULL to keep the slot fstab's. */
int kdt_image_carry(const char *run, const char *slot, const char *db,
		    const char *root_dev, char *why, size_t wcap);

/* $XDG_CONFIG_HOME/<rest>, falling back to ~/.config. Shared rather than
 * copied: two XDG helpers in one binary is how the two drift apart. */
char *kdt_cfg_home(const char *rest);
char *kdt_data_home(const char *rest);
char *kdt_cache_home(const char *rest);
void kdt_mkparent(const char *path);

/* ── the session bus (bus.c) ──────────────────────────────────────────────
 *
 * The sd-bus API, opened at run time from libbasu (libsystemd on a
 * development host); see bus.c for why it is not linked. NULL from
 * kdt_bus_open means no library or no bus, and every other call takes a
 * NULL bus as a failed call. */
typedef struct KdtBus KdtBus;
typedef void KdtMsg;

KdtBus *kdt_bus_open(int system);
void kdt_bus_close(KdtBus *b);
/* 0 and the reply in *reply (NULL to discard it), or -1 with the peer's
 * error message in `err`. */
int kdt_bus_callv(KdtBus *b, const char *dest, const char *path,
		  const char *iface, const char *member, KdtMsg **reply,
		  char *err, size_t errn, const char *types, va_list ap);
int kdt_bus_call(KdtBus *b, const char *dest, const char *path,
		 const char *iface, const char *member, KdtMsg **reply,
		 char *err, size_t errn, const char *types, ...);
/* A method answering one boolean: 1, 0, or -1 for no answer. */
int kdt_bus_call_b(KdtBus *b, const char *dest, const char *path,
		   const char *iface, const char *member, char *err,
		   size_t errn, const char *types, ...);
/* A method answering one string, malloc'd; NULL for no answer. */
char *kdt_bus_call_s(KdtBus *b, const char *dest, const char *path,
		     const char *iface, const char *member, char *err,
		     size_t errn, const char *types, ...);
/* A method taking an array of `n` strings and one boolean. */
int kdt_bus_call_strv(KdtBus *b, const char *dest, const char *path,
		      const char *iface, const char *member, char *err,
		      size_t errn, char **v, int n, int flag);
int kdt_bus_read(KdtBus *b, KdtMsg *m, const char *types, ...);
int kdt_bus_enter(KdtBus *b, KdtMsg *m, char type, const char *contents);
int kdt_bus_leave(KdtBus *b, KdtMsg *m);
int kdt_bus_peek(KdtBus *b, KdtMsg *m, char *type, const char **contents);
int kdt_bus_skip(KdtBus *b, KdtMsg *m, const char *types);
void kdt_bus_free(KdtBus *b, KdtMsg *m);
/* The serving half, for a program that is a bus service itself: an object
 * at `path` answered by one callback (1 handled, 0 unknown method), a name,
 * and a loop over kdt_bus_fd()'s descriptor that calls kdt_bus_dispatch(). */
typedef int (*KdtHandler)(KdtMsg *m, void *ud, void *err);
int kdt_bus_serve(KdtBus *b, const char *path, KdtHandler cb, void *ud);
int kdt_bus_request_name(KdtBus *b, const char *name);
void kdt_bus_release_name(KdtBus *b, const char *name);
int kdt_bus_fd(KdtBus *b, short *events);
int kdt_bus_dispatch(KdtBus *b);
const char *kdt_bus_member(KdtBus *b, KdtMsg *m);
const char *kdt_bus_interface(KdtBus *b, KdtMsg *m);
KdtMsg *kdt_bus_new_reply(KdtBus *b, KdtMsg *call);
KdtMsg *kdt_bus_new_signal(KdtBus *b, const char *path, const char *iface,
			   const char *member);
int kdt_bus_append(KdtBus *b, KdtMsg *m, const char *types, ...);
int kdt_bus_open_container(KdtBus *b, KdtMsg *m, char type,
			   const char *contents);
int kdt_bus_close_container(KdtBus *b, KdtMsg *m);
int kdt_bus_send(KdtBus *b, KdtMsg *m);
/* An error reply to `call`; returns 1, the handler's "handled". */
int kdt_bus_reply_error(KdtBus *b, KdtMsg *call, const char *name,
			const char *message);
/* 1 owned, 0 not, -1 the bus did not answer. */
int kdt_bus_has_owner(KdtBus *b, const char *name);
/* A property through org.freedesktop.DBus.Properties.Get, the reply entered
 * past its variant so the value reads directly; `sig` is its type. */
int kdt_bus_get_prop(KdtBus *b, const char *dest, const char *path,
		     const char *iface, const char *prop, const char *sig,
		     KdtMsg **reply, char *err, size_t errn);
char *kdt_bus_get_prop_s(KdtBus *b, const char *dest, const char *path,
			 const char *iface, const char *prop, char *err,
			 size_t errn);

/* ── KDE's configuration files (kcfg.c) ──────────────────────────────────
 *
 * Read here, written by kwriteconfig6; a group is spelled as the file's
 * header spells it (`A][B` for a nested one). Every getter returns a
 * malloc'd value or NULL when the key is absent. */
char *kdt_kcfg_get_raw(const char *path, const char *group, const char *key);
char *kdt_kcfg_get(const char *path, const char *group, const char *key);
char *kdt_kcfg_unescape(const char *raw);
/* The person's own file, $XDG_CONFIG_HOME/<file>, and nothing else. */
char *kdt_kcfg_user(const char *file, const char *group, const char *key);
/* The whole cascade: the person's file, then each $XDG_CONFIG_DIRS entry. */
char *kdt_kcfg_cascade(const char *file, const char *group, const char *key);
int kdt_kcfg_bool(const char *value, int dflt);
/* kwriteconfig6; `value` NULL takes the key out of the person's own file, so
 * the cascade's value applies again. 0, or -1 when it failed. */
int kdt_kcfg_set(const char *file, const char *group, const char *key,
		 const char *value, int notify);
/* One key in one section of a plain INI file, everything else kept: for the
 * files a program of ours owns or merges into (qt5ct.conf, mc's ini, a
 * generated Konsole profile), never for a file KConfig writes. */
void kdt_ini_set(const char *path, const char *section, const char *key,
		 const char *value);

/* ── the accent (theme.c) ─────────────────────────────────────────────── */

int kdt_theme_main(int argc, char **argv);	/* kdos theme               */
/* The accent in $XDG_CONFIG_HOME/kdos/accent, else the KDOS colour scheme
 * Plasma wears, else the default. Points at a static buffer. */
const char *kdt_current_accent(void);
const char *kdt_accent_title(const KcolScheme *sc);	/* "Bone"   */
int kdt_accent_light(const KcolScheme *sc);
/* The switch itself, for a caller already holding the scheme (`kdos
 * phosphor` pairs the accent through it). KDT_THEME_FROM_PLASMA is kded-kdos's
 * run: the global theme is already applied, so it is not applied again. */
#define KDT_THEME_FROM_PLASMA 1
int kdt_theme_set(const KcolScheme *sc, int flags);
/* The open Konsole tabs whose profile name starts with `prefix` are moved to
 * profile `name`; returns how many moved. */
int kdt_konsole_retarget(KdtBus *b, const char *prefix, const char *name);
/* `kdos theme`'s Konsole step for the accent in force: the person's konsolerc
 * keys and the open tabs that name a KDOS profile move to the accent's (or to
 * KDOS-Terminus while Phosphor mode's Terminus option is on). */
int kdt_theme_konsole_follow(KdtBus *b);

/*
 * `kdos theme --audit` (themeaudit.c): the installed schemes against their
 * contrast floors, the accent's installed files, and the home's generated
 * files regenerated into a scratch $HOME and compared byte for byte. 0 when
 * everything matches, 1 on drift, 2 when the audit itself could not run.
 */
int kdt_theme_audit(const KcolScheme *sc, void (*apply)(const KcolScheme *),
		    const char *tty_accent, const char *tty_reset);

/* ── Phosphor mode, desks, tiling (phosphor.c, desk.c) ───────────────── */

int kdt_phosphor_main(int argc, char **argv);	/* kdos phosphor            */
/* Phosphor mode is on with its Terminus option: the KDOS Konsole profile in
 * force is then KDOS-Terminus, which `kdos theme` regenerates. */
int kdt_phosphor_terminus_active(void);
int kdt_phosphor_terminus_profile(const KcolScheme *sc);
int kdt_desk_main(int argc, char **argv);	/* kdos desk                */
int kdt_tiling_main(int argc, char **argv);	/* kdos tiling              */

/*
 * `kdos cve` (cve.c). Compares what is pinned or installed against the vendored
 * Alpine security database — offline, by version comparison, never by scanning
 * a binary. 0 clean, 1 something is behind a recorded fix, 2 no database.
 */
int kdt_cve(int argc, char **argv, const char *tty_accent,
	    const char *tty_warn, const char *tty_reset);

/*
 * `kdos app` (app.c). The applications this machine has and the ones on the
 * medium beside it — a handful of verbs over kdos-packd, not a store. Every
 * verb hands the daemon an id out of the list the daemon published.
 */
int kdt_app(int argc, char **argv);
int kdt_trash(int argc, char **argv);
int kdt_places(int argc, char **argv);
/* `kdos thumb` (thumb.c): the shared thumbnail cache, read and named. */
int kdt_thumb(int argc, char **argv);
/* `kdos verify` (verify.c): a file against its checksum files and its KDOS
 * signature. 0 verified, 1 a check failed, 2 unreadable or usage, 3 nothing
 * to verify it against. */
int kdt_verify_main(int argc, char **argv);
/*
 * `kdos speech` (speech.c): transcription and reading aloud, and the models
 * and voices both need. Everything runs locally; nothing is sent anywhere.
 */
int kdt_speech(int argc, char **argv);

/*
 * THE CLIPBOARD (clip.c). Neither direction puts the text in an argument
 * vector. `kdt_clip_take` answers 0 for an empty clipboard as well as for no
 * clipboard at all: a caller with nothing to send cannot use the difference.
 */
int kdt_clip_put(const char *text);
int kdt_clip_take(char *buf, size_t n);

/* `kdos share` / `kdos-share` (share.c). */
int share_main(int argc, char **argv);

/* `kdos remind` (remind.c) — a notification, later, over the per-user timer
 * table. */
int remind_main(int argc, char **argv);

/* `kdos timezone|hostname|locale|keymap` (machine.c), through the
 * timedate1, hostname1 and locale1 services; `kdos in-vm`. */
int kdt_machine_main(const char *verb, int argc, char **argv);

/* `kdos help --keys` (keys.c): the global shortcuts in force, grouped as the
 * key card groups them. Returns how many were printed. */
int kdt_keys_print(FILE *o, const char *head, const char *key,
		   const char *reset);

/* `kdos backup` (backup.c): the restic CLI over the Backups settings. */
int kdt_backup_main(int argc, char **argv);
/* `kdos privacy` (privacy.c): who is using the microphone, camera and
 * screen. */
int kdt_privacy_main(int argc, char **argv);
/* `kdos checklist` (checklist.c): the first-run checklist's items. */
int kdt_checklist_main(int argc, char **argv);

#endif /* KDOS_TOOLS_H */
