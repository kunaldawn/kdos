/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   kdos-packd — the only thing on this machine that mounts a pack
 * ---------------------------------
 */

#ifndef KDOS_PACKD_H
#define KDOS_PACKD_H

#include <sys/types.h>

#include "kpack.h"

#define KD_SOCKET   "/run/kdos-packd.sock"
#define KD_GROUP    "wheel"
#define KD_STORE    "/var/lib/kdos/packs"
#define KD_MANIFEST "/var/lib/kdos/pack-manifest"
#define KD_ISO      "/mnt/iso/packs"
#define KD_SHARE    "/usr/share"
/*
 * A KEY IS SCOPED TO WHAT IT SIGNS. `/etc/kdos/keys` is kpkg's binhost ring —
 * whoever is trusted to publish PACKAGES for the HOST — and a pack-bake key
 * has no business there: it attests that some packs came off one medium, not
 * that a package repository is sound. ksig_ring_load reads *.pub flat and
 * never descends, so a subdirectory is a real boundary rather than a
 * convention.
 */
#define KD_KEYS     "/etc/kdos/keys/packs"

/*
 * As many packs as an index can name. The table is a few megabytes of BSS,
 * which is demand-zero: only the packs actually scanned ever touch a page, so
 * a machine with six packs pays for six.
 */
#define KD_PACKS    KPK_INDEX_MAX
#define KD_BOXES    32
#define KD_STACK    32

/* Derived paths are built by joining onto a directory this daemon owns, so
 * every buffer that holds one is wider than the widest thing joined into it.
 * The compiler checks this and will not be talked out of it. */
#define KD_PATH     (KPK_PATH * 4)

/* Where a pack came from. A pack on the medium costs nothing until it is
 * mounted, which is why `available` is a state and not an absence. */
typedef enum {
	KD_ORIGIN_STORE = 0,	/* /var/lib/kdos/packs — installed          */
	KD_ORIGIN_MEDIUM,	/* /mnt/iso/packs — available, not copied   */
} KdOrigin;

typedef struct {
	KpkMeta meta;
	char file[KD_PATH];		/* the pack file                    */
	char mnt[KD_PATH];		/* where it is mounted, or ""       */
	KdOrigin origin;
	unsigned long long size;
	int refs;			/* boxes composed over it           */
	int loop;			/* the loop device number, or -1    */
} KdPack;

typedef struct {
	char name[64];
	char merged[KD_PATH];
	char upper[KD_PATH];
	char work[KD_PATH];
	int stack[KD_STACK];		/* indices into the pack table      */
	int nstack;
	int ephemeral;			/* the upper is on tmpfs            */
	uid_t uid;
} KdBox;

/* ── the store (store.c) ───────────────────────────────────────────────── */

extern KdPack kd_pack[KD_PACKS];
extern int kd_npack;

const char *kd_store_dir(void);
const char *kd_medium_dir(void);
const char *kd_mnt_dir(void);
const char *kd_manifest(void);
/* The one place an unprivileged download may land: mode 01777 inside the
 * store, and the only path a client is expected to write to. */
const char *kd_staging_dir(void);
/* How many superseded versions of one pack the store keeps. `rollback` is a
 * rename of a file that is still there, so this is what makes it possible. */
int kd_retain(void);

/* Re-read both directories. Called on EVERY request rather than cached: a
 * medium pulled out between two requests must not still be offered, and the
 * scan is a footer read per file. */
void kd_scan(void);

int kd_find(const char *id);

/* `[A-Za-z0-9._-]+`, no slash, no leading dot. Every id that reaches this
 * daemon is a filename it will join onto a directory it owns. */
int kd_id_ok(const char *s);

int kd_install(const char *staged, char *msg, size_t n);
int kd_remove(const char *id, char *msg, size_t n);
int kd_rollback(const char *id, char *msg, size_t n);

/* ── mounting (mount.c) ────────────────────────────────────────────────── */

/* Which route the kernel gave us. Reported by `status`, because the answer
 * differs between kernels and a person debugging a failed mount needs it. */
typedef enum { KD_ROUTE_UNKNOWN = 0, KD_ROUTE_FILE, KD_ROUTE_LOOP } KdRoute;
extern KdRoute kd_route;
const char *kd_route_name(void);

int kd_mount(int idx, char *msg, size_t n);
int kd_unmount(int idx, char *msg, size_t n);

/* Adopt what is already mounted. The daemon can be restarted while boxes are
 * running, and a table rebuilt from /proc/mounts is the only honest answer —
 * the alternative is a daemon that unmounts a live box's root because it does
 * not remember mounting it. */
void kd_adopt(void);

extern KdBox kd_box[KD_BOXES];
extern int kd_nbox;

int kd_compose(const char *box, char *const *ids, int nids, uid_t uid,
	       char *msg, size_t n);
int kd_decompose(const char *box, char *msg, size_t n);
int kd_box_find(const char *name);

/* Is `$HOME`'s filesystem one an overlay upper may live on? */
int kd_fs_takes_upper(const char *path);

/* ── data packs (graft.c) ──────────────────────────────────────────────── */

int kd_graft(int idx, uid_t uid, char *msg, size_t n);
int kd_ungraft(const char *id, char *msg, size_t n);

/* ── shared ────────────────────────────────────────────────────────────── */

extern int kd_fixture;		/* print what WOULD happen; mount nothing */
/* The fixture still hashes a medium pack — that is the check being tested. */
extern int kd_fixture_trust;
void kd_log(const char *fmt, ...) __attribute__((format(printf, 1, 2)));

#endif /* KDOS_PACKD_H */
