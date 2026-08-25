/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   the mount routes, and a box's root filesystem
 * ---------------------------------
 *
 * TWO ROUTES TO ONE MOUNT, AND WHICH ONE YOU GET DEPENDS ON THE KERNEL.
 * With CONFIG_EROFS_FS_BACKED_BY_FILE the kernel takes a regular file as the
 * source directly and there is no loop device at all; without it, mount(2)
 * answers ENOTBLK and the pack goes through a loop device. The format does not
 * care which — the trailing footer is invisible to both — so the daemon tries
 * the cheap one, remembers the answer, and `status` reports it. A person
 * debugging a failed mount needs to know which one they are on.
 *
 * THE LOOP DEVICE IS SET UP WITH IOCTLS, NOT BY EXEC'ING losetup. This is a
 * root daemon; every process it starts is a process running as root, and the
 * output of losetup would then have to be parsed to learn the device name.
 * Three ioctls and no child is smaller than that in every direction.
 *
 * WHY MOUNTING AS ROOT IS THE WHOLE POINT: a ROOTLESS unpack records overlay
 * whiteouts and opaque markers in `trusted.overlay.*`, which the rootless
 * runtime cannot then read — that is the trap that forced the appbox image to
 * be flattened to one layer. This daemon is root, so the markers are real,
 * layering works, and an app pack may legitimately delete a file its base
 * provides. Measured: testing/notes/packs-w0.txt.
 */

#include <errno.h>
#include <fcntl.h>
#include <pwd.h>
#include <stdarg.h>
#include <linux/loop.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <sys/statfs.h>
#include <sys/vfs.h>
#include <unistd.h>

#include "kbase.h"
#include "ksig.h"
#include "packd.h"

#ifndef OVERLAYFS_SUPER_MAGIC
#define OVERLAYFS_SUPER_MAGIC 0x794c7630
#endif
#ifndef TMPFS_MAGIC
#define TMPFS_MAGIC 0x01021994
#endif
#ifndef EXT4_SUPER_MAGIC
#define EXT4_SUPER_MAGIC 0xEF53
#endif
#ifndef BTRFS_SUPER_MAGIC
#define BTRFS_SUPER_MAGIC 0x9123683E
#endif
#ifndef XFS_SUPER_MAGIC
#define XFS_SUPER_MAGIC 0x58465342
#endif
#ifndef F2FS_SUPER_MAGIC
#define F2FS_SUPER_MAGIC 0xF2F52010
#endif

KdRoute kd_route = KD_ROUTE_UNKNOWN;
KdBox kd_box[KD_BOXES];
int kd_nbox;

const char *kd_route_name(void)
{
	switch (kd_route) {
	case KD_ROUTE_FILE: return "file-backed";
	case KD_ROUTE_LOOP: return "loop device";
	default:            return "not yet used";
	}
}

/*
 * An overlay upperdir may NOT sit on overlayfs — measured, and the kernel says
 * so only at mount time, which is too late to choose. So the question is asked
 * of the filesystem instead, the same way kdos-appbox already asks it to pick
 * a container storage driver.
 */
int kd_fs_takes_upper(const char *path)
{
	struct statfs s;

	if (statfs(path, &s) != 0)
		return 0;
	switch ((unsigned long)s.f_type) {
	case EXT4_SUPER_MAGIC:
	case BTRFS_SUPER_MAGIC:
	case XFS_SUPER_MAGIC:
	case F2FS_SUPER_MAGIC:
	case TMPFS_MAGIC:
		return 1;
	default:
		/* overlayfs, squashfs, iso9660, and anything else this build
		 * has not been shown to work on: refused rather than guessed
		 * at, because the failure is silent data loss. */
		return 0;
	}
}

/* ── the loop route ────────────────────────────────────────────────────── */

static int loop_attach(const char *file, int *devnum)
{
	int ctl, fd, dev = -1, num;
	char node[64];

	fd = open(file, O_RDONLY | O_CLOEXEC);
	if (fd < 0)
		return -1;

	ctl = open("/dev/loop-control", O_RDWR | O_CLOEXEC);
	if (ctl < 0) {
		close(fd);
		return -1;
	}
	num = ioctl(ctl, LOOP_CTL_GET_FREE);
	close(ctl);
	if (num < 0) {
		close(fd);
		return -1;
	}
	snprintf(node, sizeof(node), "/dev/loop%d", num);
	dev = open(node, O_RDONLY | O_CLOEXEC);
	if (dev < 0) {
		close(fd);
		return -1;
	}
	if (ioctl(dev, LOOP_SET_FD, fd) < 0) {
		close(dev);
		close(fd);
		return -1;
	}
	close(fd);

	/*
	 * READ-ONLY on the loop device as well as on the mount. A pack is never
	 * modified in place — an update replaces the FILE — and a writable loop
	 * behind a read-only mount is a way for that to stop being true.
	 */
	struct loop_info64 info;
	memset(&info, 0, sizeof(info));
	info.lo_flags = LO_FLAGS_READ_ONLY | LO_FLAGS_AUTOCLEAR;
	kb_strlcpy((char *)info.lo_file_name, file, LO_NAME_SIZE);
	if (ioctl(dev, LOOP_SET_STATUS64, &info) < 0) {
		ioctl(dev, LOOP_CLR_FD, 0);
		close(dev);
		return -1;
	}
	close(dev);
	*devnum = num;
	return 0;
}

static void loop_detach(int num)
{
	char node[64];
	int fd;

	if (num < 0)
		return;
	snprintf(node, sizeof(node), "/dev/loop%d", num);
	fd = open(node, O_RDONLY | O_CLOEXEC);
	if (fd < 0)
		return;
	ioctl(fd, LOOP_CLR_FD, 0);
	close(fd);
}

/* ── mounting a pack ───────────────────────────────────────────────────── */

int kd_mount(int idx, char *msg, size_t n)
{
	KdPack *p = &kd_pack[idx];
	char dir[KD_PATH * 2];
	unsigned long flags = MS_RDONLY | MS_NOSUID | MS_NODEV;
	int num = -1;

	if (p->mnt[0]) {
		snprintf(msg, n, "%s", p->mnt);
		return 0;
	}

	/*
	 * A PACK ON THE MEDIUM HAS NEVER BEEN VERIFIED; ONE IN THE STORE WAS
	 * VERIFIED WHEN IT WAS WRITTEN. `install` hashes what it moves in and
	 * only root can write the store afterwards, so re-hashing a 620 MB base
	 * on every mount would buy nothing and cost a full read of a file the
	 * kernel was about to read lazily. The medium is the other case
	 * entirely — `kdos app install` mounts straight off ISO9660 without a
	 * copy, so this is the only place that file is ever checked.
	 *
	 * Stated cost: the first mount of a medium pack reads it once, which on
	 * a slow stick is seconds. The alternative is mounting bytes nobody
	 * looked at.
	 */
	if (p->origin == KD_ORIGIN_MEDIUM && !kd_fixture_trust) {
		KpkPack chk;
		KsigRing ring = {0};
		char who[KSIG_ID_HEX];
		KpkSigState sig;

		if (kpk_open(p->file, &chk) != 0) {
			snprintf(msg, n, "%s: unreadable on the medium", p->meta.id);
			return -1;
		}
		ksig_ring_load(&ring, KD_KEYS);
		sig = kpk_verify(&chk, &ring, who);
		if (sig == KPK_SIG_HASH || sig == KPK_SIG_BAD) {
			snprintf(msg, n, "%s: %s", p->meta.id,
				 kpk_sig_state_name(sig));
			return -1;
		}
	}

	snprintf(dir, sizeof(dir), "%s/%s", kd_mnt_dir(), p->meta.id);

	/*
	 * A DATA PACK IS MOUNTED noexec AND IS NEVER COMPOSED INTO A BOX ROOT.
	 * A data pack that ships a binary cannot run it — that is the rule that
	 * makes "there is no script near an untrusted artefact" true rather
	 * than merely intended.
	 */
	if (p->meta.kind == KPK_KIND_DATA)
		flags |= MS_NOEXEC;

	if (kd_fixture) {
		snprintf(msg, n, "%s -> %s (%s)", p->meta.id, dir,
			 p->meta.kind == KPK_KIND_DATA ? "ro,nosuid,nodev,noexec"
						       : "ro,nosuid,nodev");
		kb_strlcpy(p->mnt, dir, sizeof(p->mnt));
		return 0;
	}

	if (kb_mkdir_p(dir) != 0) {
		snprintf(msg, n, "cannot make %s", dir);
		return -1;
	}

	if (kd_route != KD_ROUTE_LOOP &&
	    mount(p->file, dir, "erofs", flags, NULL) == 0) {
		kd_route = KD_ROUTE_FILE;
	} else {
		if (kd_route == KD_ROUTE_UNKNOWN && errno != ENOTBLK &&
		    errno != EINVAL) {
			snprintf(msg, n, "%s: %s", p->meta.id, strerror(errno));
			rmdir(dir);
			return -1;
		}
		if (loop_attach(p->file, &num) != 0) {
			snprintf(msg, n, "%s: no loop device (%s)", p->meta.id,
				 strerror(errno));
			rmdir(dir);
			return -1;
		}
		char node[64];
		snprintf(node, sizeof(node), "/dev/loop%d", num);
		if (mount(node, dir, "erofs", flags, NULL) != 0) {
			snprintf(msg, n, "%s: %s", p->meta.id, strerror(errno));
			loop_detach(num);
			rmdir(dir);
			return -1;
		}
		kd_route = KD_ROUTE_LOOP;
		p->loop = num;
	}

	kb_strlcpy(p->mnt, dir, sizeof(p->mnt));
	kd_log("mounted %s at %s (%s)", p->meta.id, dir, kd_route_name());
	snprintf(msg, n, "%s", dir);
	return 0;
}

int kd_unmount(int idx, char *msg, size_t n)
{
	KdPack *p = &kd_pack[idx];

	if (!p->mnt[0]) {
		snprintf(msg, n, "%s is not mounted", p->meta.id);
		return -1;
	}
	/*
	 * REFERENCE COUNTED, because two boxes legitimately share a runtime.
	 * Unmounting rt-gtk because one of them stopped would pull the
	 * lowerdir out from under the other, and the symptom is every path in
	 * a running application going ENOENT.
	 */
	if (p->refs) {
		snprintf(msg, n, "%s is used by %d box(es)", p->meta.id, p->refs);
		return -1;
	}
	if (!kd_fixture && umount2(p->mnt, 0) != 0) {
		snprintf(msg, n, "%s: %s", p->meta.id, strerror(errno));
		return -1;
	}
	if (!kd_fixture) {
		loop_detach(p->loop);
		rmdir(p->mnt);
	}
	p->loop = -1;
	snprintf(msg, n, "%s", p->mnt);
	p->mnt[0] = 0;
	return 0;
}

/*
 * The daemon can be restarted while boxes are running, so the table is rebuilt
 * from what is actually mounted. A daemon that forgot would unmount a live
 * box's own root the first time somebody asked it to tidy up.
 */
void kd_adopt(void)
{
	size_t len = 0;
	char *mounts = kb_read_all("/proc/mounts", &len);
	char *line, *save;
	const char *base = kd_mnt_dir();
	size_t bl = strlen(base);

	if (!mounts)
		return;
	for (line = strtok_r(mounts, "\n", &save); line;
	     line = strtok_r(NULL, "\n", &save)) {
		char *sp = strchr(line, ' ');
		char *dir, *end;
		int i;

		if (!sp)
			continue;
		dir = sp + 1;
		end = strchr(dir, ' ');
		if (!end)
			continue;
		*end = 0;
		if (strncmp(dir, base, bl) || dir[bl] != '/')
			continue;
		i = kd_find(dir + bl + 1);
		if (i >= 0) {
			kb_strlcpy(kd_pack[i].mnt, dir, sizeof(kd_pack[i].mnt));
			kd_log("adopted %s, already mounted", kd_pack[i].meta.id);
		}
	}
	free(mounts);
}

/* ── composing a box's root ────────────────────────────────────────────── */

int kd_box_find(const char *name)
{
	for (int i = 0; i < kd_nbox; i++)
		if (!strcmp(kd_box[i].name, name))
			return i;
	return -1;
}

static const char *run_dir(uid_t uid)
{
	static char b[KD_PATH];
	snprintf(b, sizeof(b), "/run/user/%u", (unsigned)uid);
	return b;
}

/*
 * The caller's own home, from the password database rather than from
 * /home/<name>: a box's upper follows the user, and this is the one process
 * that must not guess where that is. KDOS_PACK_HOME moves it for the fixture,
 * which is how the live-stick fallback gets tested on a machine that is not a
 * live stick.
 */
static const char *home_of(uid_t uid)
{
	static char b[KD_PATH];
	const char *h = getenv("KDOS_PACK_HOME");
	struct passwd *pw;

	if (h && *h) {
		kb_strlcpy(b, h, sizeof(b));
		return b;
	}
	pw = getpwuid(uid);
	if (pw && pw->pw_dir && pw->pw_dir[0]) {
		kb_strlcpy(b, pw->pw_dir, sizeof(b));
		return b;
	}
	snprintf(b, sizeof(b), "/run/user/%u", (unsigned)uid);
	return b;
}

/*
 * A TRUNCATED PATH IS REFUSED, NOT SHORTENED. These build a box's upper and
 * merged directories; a silently cut path is a box whose writes land somewhere
 * else, which is the one failure in this file that loses somebody's work
 * rather than merely failing.
 */
__attribute__((format(printf, 3, 4)))
static int path_fmt(char *out, size_t cap, const char *fmt, ...)
{
	va_list ap;
	int n;

	va_start(ap, fmt);
	n = vsnprintf(out, cap, fmt, ap);
	va_end(ap);
	return (n < 0 || (size_t)n >= cap) ? -1 : 0;
}

int kd_compose(const char *box, char *const *ids, int nids, uid_t uid,
	       char *msg, size_t n)
{
	const KpkMeta *avail[KD_PACKS];
	int order[KD_STACK];
	char err[256];
	KbBuf lower = {0};
	KdBox *b;
	int at, nord;
	const char *home;
	int persistent;

	if (!kd_id_ok(box)) {
		snprintf(msg, n, "a box name is [A-Za-z0-9._-]");
		return -1;
	}
	if (nids < 1 || nids > KD_STACK) {
		snprintf(msg, n, "a stack is 1 to %d packs", KD_STACK);
		return -1;
	}
	at = kd_box_find(box);
	if (at >= 0) {
		snprintf(msg, n, "%s", kd_box[at].merged);
		return 0;
	}
	if (kd_nbox >= KD_BOXES) {
		snprintf(msg, n, "no room for another box");
		return -1;
	}

	for (int i = 0; i < kd_npack; i++)
		avail[i] = &kd_pack[i].meta;

	/*
	 * THE SOLVE IS THE DAEMON'S, NEVER THE CLIENT'S. A client that worked
	 * out its own stack could ask for one whose requirements are not met,
	 * and the result would be an application launching into a root
	 * filesystem missing the library it needs — which presents as the
	 * application being broken.
	 */
	nord = kpk_solve(avail, kd_npack, (const char *const *)ids, nids, order,
			 KD_STACK, err, sizeof(err));
	if (nord < 0) {
		snprintf(msg, n, "%s", err);
		return -1;
	}

	for (int i = 0; i < nord; i++) {
		if (kd_pack[order[i]].meta.kind == KPK_KIND_DATA) {
			/* A data pack is grafted, never composed: it is mounted
			 * noexec, and putting it in a root filesystem would be
			 * offering a /usr nothing in it can execute. */
			snprintf(msg, n, "%s is a data pack — graft it",
				 kd_pack[order[i]].meta.id);
			return -1;
		}
		if (kd_mount(order[i], msg, n) != 0)
			return -1;
	}

	b = &kd_box[kd_nbox];
	memset(b, 0, sizeof(*b));
	kb_strlcpy(b->name, box, sizeof(b->name));
	b->uid = uid;
	b->nstack = nord;
	memcpy(b->stack, order, sizeof(order[0]) * (size_t)nord);

	/* lowerdir is highest-priority FIRST, and the solve produced
	 * dependencies first — so it is walked backwards. Getting this the
	 * other way round makes the base shadow the application. */
	for (int i = nord - 1; i >= 0; i--)
		kb_buf_printf(&lower, "%s%s", i == nord - 1 ? "" : ":",
			      kd_pack[order[i]].mnt);

	home = home_of(uid);
	persistent = kd_fs_takes_upper(home);
	if (persistent) {
		if (path_fmt(b->upper, sizeof(b->upper),
			     "%s/.local/share/kdos/boxes/%s/upper", home, box) != 0 ||
		    path_fmt(b->work, sizeof(b->work),
			     "%s/.local/share/kdos/boxes/%s/work", home, box) != 0) {
			snprintf(msg, n, "the path to %s's upper is too long", box);
			return -1;
		}
	} else {
		/*
		 * A live session's $HOME is on the boot overlay and the kernel
		 * refuses to stack an upper on overlayfs. The fallback is
		 * tmpfs, and the CALLER IS TOLD: losing somebody's work quietly
		 * is the failure this branch exists to avoid.
		 */
		b->ephemeral = 1;
		if (path_fmt(b->upper, sizeof(b->upper), "%s/kdos/boxes/%s/upper",
			     run_dir(uid), box) != 0 ||
		    path_fmt(b->work, sizeof(b->work), "%s/kdos/boxes/%s/work",
			     run_dir(uid), box) != 0) {
			snprintf(msg, n, "the path to %s's upper is too long", box);
			return -1;
		}
	}
	if (path_fmt(b->merged, sizeof(b->merged), "%s/kdos/boxes/%s/root",
		     run_dir(uid), box) != 0) {
		snprintf(msg, n, "the path to %s's root is too long", box);
		return -1;
	}

	if (kd_fixture) {
		snprintf(msg, n, "%s %s upper=%s%s", b->merged, lower.p,
			 b->upper, b->ephemeral ? " (ephemeral)" : "");
		kb_buf_free(&lower);
		for (int i = 0; i < nord; i++)
			kd_pack[order[i]].refs++;
		kd_nbox++;
		return 0;
	}

	kb_mkdir_p(b->upper);
	kb_mkdir_p(b->work);
	kb_mkdir_p(b->merged);
	/* The merged tree is entered by a rootless container running as the
	 * caller; the upper is where its writes land and must be theirs. */
	(void)!chown(b->upper, uid, (gid_t)-1);
	(void)!chown(b->work, uid, (gid_t)-1);
	(void)!chown(b->merged, uid, (gid_t)-1);

	{
		KbBuf opt = {0};
		kb_buf_printf(&opt, "lowerdir=%s,upperdir=%s,workdir=%s",
			      lower.p, b->upper, b->work);
		if (mount("overlay", b->merged, "overlay", 0, opt.p) != 0) {
			snprintf(msg, n, "overlay for %s: %s", box, strerror(errno));
			kb_buf_free(&opt);
			kb_buf_free(&lower);
			return -1;
		}
		kb_buf_free(&opt);
	}
	kb_buf_free(&lower);

	for (int i = 0; i < nord; i++)
		kd_pack[order[i]].refs++;
	kd_nbox++;
	kd_log("composed %s from %d packs at %s%s", box, nord, b->merged,
	       b->ephemeral ? " (ephemeral upper)" : "");
	snprintf(msg, n, "%s", b->merged);
	return 0;
}

int kd_decompose(const char *box, char *msg, size_t n)
{
	int at = kd_box_find(box);
	KdBox *b;

	if (at < 0) {
		snprintf(msg, n, "%s is not composed", box);
		return -1;
	}
	b = &kd_box[at];
	if (!kd_fixture && umount2(b->merged, 0) != 0 && errno != EINVAL &&
	    errno != ENOENT) {
		snprintf(msg, n, "%s: %s", box, strerror(errno));
		return -1;
	}
	for (int i = 0; i < b->nstack; i++)
		if (kd_pack[b->stack[i]].refs > 0)
			kd_pack[b->stack[i]].refs--;

	/*
	 * The packs stay mounted. A runtime that another box is using must not
	 * go, and one nobody is using costs a mount entry and no memory — the
	 * pages are the page cache's and the kernel reclaims them. Unmounting
	 * on the last reference would make the next launch pay for the mount
	 * again for no gain.
	 */
	snprintf(msg, n, "%s", b->merged);
	kd_box[at] = kd_box[--kd_nbox];
	return 0;
}
