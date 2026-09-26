/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   KDOS Installer — the part that writes
 *
 * The work runs in a forked child that speaks one line per event back over a
 * pipe. That keeps the install a plain top-to-bottom program — which is what
 * it is — while the parent stays a single-threaded poll loop that never
 * blocks on a 4 GB rsync. The protocol is deliberately tiny:
 *
 *   S<n>  begin step n        K<n>  skip step n
 *   P<f>  progress 0..1, or -1 for indeterminate
 *   N<s>  one-line note for the current step
 *   L<s>  log line
 *   F<s>  fail, with reason      D  all done
 *
 * There is no system() and no shell in this file. Device paths, user names
 * and passwords all reach it from a menu; a shell in the middle would turn
 * every one of them into an injection point.
 * ---------------------------------
 */

#include <crypt.h>
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <sys/sysmacros.h>
#include <sys/wait.h>

#include "kinstall.h"

Install inst;

#define TARGET "/mnt"

enum {
	S_PREPARE = 0,
	S_PARTITION,
	S_FORMAT,
	S_MOUNT,
	S_COPY,
	S_APPS,
	S_CONFIG,
	S_ACCOUNTS,
	S_THEME,
	S_BOOT,
	S_FINISH,
	S_COUNT
};

static const struct {
	const char *title;
	const char *detail;
} steps[S_COUNT] = {
	{ "Prepare",     "unmount the target, stop swap" },
	{ "Partition",   "write the GPT layout" },
	{ "Format",      "encrypt and make the volume group if asked, then the filesystems" },
	{ "Mount",       "attach the target at /mnt" },
	{ "Copy system", "the live tree, verbatim" },
	{ "Packs",       "the applications chosen from the medium" },
	{ "Configure",   "fstab, hostname, keymap, autologin, services" },
	{ "Accounts",    "users, passwords, sudo" },
	{ "Theme",       "regenerate the accent for the new home" },
	{ "Bootloader",  "Limine on the ESP, BIOS and UEFI" },
	{ "Finish",      "flush and unmount" },
};

/* ════════════════════════════════════════════════════════════════════════
 * Parent side
 * ════════════════════════════════════════════════════════════════════════ */

static int step_skipped(int i)
{
	if (i == S_PARTITION && cfg.plan != PLAN_WIPE)
		return 1;
	if (i == S_THEME && !strcmp(cfg.theme, KCOL_DEFAULT_NAME))
		return 1;
	/* Nothing chosen, or a medium with no catalogue on it — the step says
	 * SKIPPED rather than running and doing nothing, because a step that
	 * always succeeds having done nothing is a step nobody reads. */
	if (i == S_APPS && (!ki_apps_present || ki_apps_route() == APPS_NONE))
		return 1;
	return 0;
}

void install_plan(void)
{
	memset(inst.step, 0, sizeof(inst.step));
	inst.nsteps = S_COUNT;
	for (int i = 0; i < S_COUNT; i++) {
		inst.step[i].title = steps[i].title;
		inst.step[i].detail = steps[i].detail;
		inst.step[i].state = step_skipped(i) ? ST_SKIP : ST_PENDING;
		inst.step[i].frac = -1;
	}
	inst.cur = -1;
	inst.failed = inst.done = inst.running = 0;
	inst.failmsg[0] = 0;
}

void install_log(const char *line)
{
	if (!inst.log)
		inst.log = kb_calloc(LOG_LINES, LOG_COLS);
	kb_strlcpy(inst.log[inst.nlog % LOG_LINES], line, LOG_COLS);
	inst.nlog++;
	if (inst.logfd >= 0) {
		char stamp[32];
		time_t t = time(NULL);
		struct tm tm;
		localtime_r(&t, &tm);
		strftime(stamp, sizeof(stamp), "%H:%M:%S", &tm);
		dprintf(inst.logfd, "[%s] %s\n", stamp, line);
	}
}

void install_start(int from_step)
{
	int fds[2];
	if (pipe(fds) < 0) {
		inst.failed = 1;
		kb_strlcpy(inst.failmsg, "pipe() failed", sizeof(inst.failmsg));
		return;
	}

	if (!inst.log)
		inst.log = kb_calloc(LOG_LINES, LOG_COLS);
	if (!inst.logfd)
		inst.logfd = open("/var/log/kinstall.log",
				  O_WRONLY | O_CREAT | O_APPEND | O_CLOEXEC, 0600);
	if (inst.logfd <= 0)
		inst.logfd = -1;

	for (int i = from_step; i < S_COUNT; i++) {
		inst.step[i].state = step_skipped(i) ? ST_SKIP : ST_PENDING;
		inst.step[i].frac = -1;
		inst.step[i].note[0] = 0;
	}
	inst.failed = 0;
	inst.done = 0;
	inst.failmsg[0] = 0;
	inst.t0 = kb_now_s();

	pid_t pid = fork();
	if (pid < 0) {
		close(fds[0]);
		close(fds[1]);
		inst.failed = 1;
		kb_strlcpy(inst.failmsg, "fork() failed", sizeof(inst.failmsg));
		return;
	}
	if (pid == 0) {
		close(fds[0]);
		_exit(install_child_main(fds[1], from_step));
	}

	close(fds[1]);
	inst.pid = pid;
	inst.fd = fds[0];
	fcntl(inst.fd, F_SETFL, O_NONBLOCK);
	inst.running = 1;
	inst.cur = from_step;
}

static void handle_line(char *s)
{
	char c = s[0];
	char *arg = s + 1;

	switch (c) {
	case 'S': {
		int n = atoi(arg);
		if (n >= 0 && n < S_COUNT) {
			inst.cur = n;
			inst.step[n].state = ST_RUNNING;
			inst.step[n].t0 = kb_now_s();
			inst.step[n].frac = -1;
			/* Close out every earlier step, not just n-1: a skipped
			 * step sits between them often enough (no repartition,
			 * no theme regen) that "the previous one" leaves the
			 * real predecessor spinning for the rest of the run. */
			for (int k = 0; k < n; k++)
				if (inst.step[k].state == ST_RUNNING) {
					inst.step[k].state = ST_DONE;
					inst.step[k].t1 = kb_now_s();
				}
		}
		break;
	}
	case 'K': {
		int n = atoi(arg);
		if (n >= 0 && n < S_COUNT)
			inst.step[n].state = ST_SKIP;
		break;
	}
	case 'P':
		if (inst.cur >= 0)
			inst.step[inst.cur].frac = atof(arg);
		break;
	case 'N':
		if (inst.cur >= 0)
			kb_strlcpy(inst.step[inst.cur].note, arg,
				 sizeof(inst.step[inst.cur].note));
		break;
	case 'L':
		install_log(arg);
		break;
	case 'F':
		inst.failed = 1;
		kb_strlcpy(inst.failmsg, arg, sizeof(inst.failmsg));
		if (inst.cur >= 0) {
			inst.step[inst.cur].state = ST_FAIL;
			inst.step[inst.cur].t1 = kb_now_s();
		}
		install_log(arg);
		break;
	case 'D':
		for (int i = 0; i < S_COUNT; i++)
			if (inst.step[i].state == ST_RUNNING) {
				inst.step[i].state = ST_DONE;
				inst.step[i].t1 = kb_now_s();
			}
		inst.done = 1;
		break;
	default:
		break;
	}
}

void install_pump(void)
{
	static char buf[8192];
	static size_t len;

	if (!inst.running)
		return;

	for (;;) {
		ssize_t n = read(inst.fd, buf + len, sizeof(buf) - len - 1);
		if (n > 0) {
			len += (size_t)n;
			buf[len] = 0;
			char *start = buf, *nl;
			while ((nl = strchr(start, '\n'))) {
				*nl = 0;
				if (*start)
					handle_line(start);
				start = nl + 1;
			}
			len = strlen(start);
			memmove(buf, start, len + 1);
			if (len + 1 >= sizeof(buf))
				len = 0;
			continue;
		}
		if (n == 0)
			break;		/* EOF                             */
		if (errno == EAGAIN || errno == EWOULDBLOCK)
			return;
		if (errno == EINTR)
			continue;
		break;
	}

	int status = 0;
	waitpid(inst.pid, &status, 0);
	close(inst.fd);
	inst.running = 0;
	inst.fd = -1;

	if (!inst.done && !inst.failed) {
		inst.failed = 1;
		snprintf(inst.failmsg, sizeof(inst.failmsg),
			 "install process exited unexpectedly (status %d)",
			 WIFEXITED(status) ? WEXITSTATUS(status) : -1);
		if (inst.cur >= 0)
			inst.step[inst.cur].state = ST_FAIL;
	}
}

void install_abort(void)
{
	if (!inst.running)
		return;
	kill(inst.pid, SIGTERM);
	waitpid(inst.pid, NULL, 0);
	close(inst.fd);
	inst.running = 0;
	inst.failed = 1;
	kb_strlcpy(inst.failmsg, "aborted by user", sizeof(inst.failmsg));
}

/* ════════════════════════════════════════════════════════════════════════
 * Child side
 * ════════════════════════════════════════════════════════════════════════ */

static int wfd = -1;
static char part_esp[96], part_root[192], part_swap[96];

/* Append into a fixed buffer without the strncat sizing dance. Truncates
 * rather than overflowing; every caller here is building a file we then
 * write, and a silently short file is still better than a smashed stack. */
static void cat(char *dst, size_t cap, const char *src)
{
	size_t n = strlen(dst);
	if (n + 1 >= cap)
		return;
	kb_strlcpy(dst + n, src, cap - n);
}

static void emit(char kind, const char *fmt, ...)
{
	char buf[1200];
	buf[0] = kind;
	va_list ap;
	va_start(ap, fmt);
	vsnprintf(buf + 1, sizeof(buf) - 2, fmt, ap);
	va_end(ap);
	for (char *p = buf + 1; *p; p++)
		if (*p == '\n' || *p == '\r')
			*p = ' ';
	size_t n = strlen(buf);
	buf[n++] = '\n';
	ssize_t r = write(wfd, buf, n);
	(void)r;
}

static void logf_(const char *fmt, ...)
{
	char buf[1024];
	va_list ap;
	va_start(ap, fmt);
	vsnprintf(buf, sizeof(buf), fmt, ap);
	va_end(ap);
	emit('L', "%s", buf);
}

__attribute__((noreturn))
static void fail(const char *fmt, ...)
{
	char buf[512];
	va_list ap;
	va_start(ap, fmt);
	vsnprintf(buf, sizeof(buf), fmt, ap);
	va_end(ap);
	emit('F', "%s", buf);
	_exit(1);
}

/* ──────────────────────────────────────────────────────────────────────── */

typedef void (*LineParse)(const char *line);

static int run_full(char *const argv[], const char *stdin_data,
		    LineParse parse, const int *ok_codes, int nok)
{
	char cmd[512] = "";
	for (int i = 0; argv[i]; i++) {
		cat(cmd, sizeof(cmd), argv[i]);
		cat(cmd, sizeof(cmd), " ");
	}
	logf_("$ %s", cmd);

	if (cfg.dry_run) {
		logf_("  (dry run: not executed)");
		return 0;
	}

	int out[2], in[2];
	if (pipe(out) < 0)
		fail("pipe: %s", strerror(errno));
	if (stdin_data && pipe(in) < 0)
		fail("pipe: %s", strerror(errno));

	pid_t pid = fork();
	if (pid < 0)
		fail("fork: %s", strerror(errno));

	if (pid == 0) {
		dup2(out[1], 1);
		dup2(out[1], 2);
		close(out[0]);
		close(out[1]);
		if (stdin_data) {
			dup2(in[0], 0);
			close(in[0]);
			close(in[1]);
		}
		execvp(argv[0], argv);
		_exit(127);
	}

	close(out[1]);
	if (stdin_data) {
		close(in[0]);
		size_t n = strlen(stdin_data), off = 0;
		while (off < n) {
			ssize_t w = write(in[1], stdin_data + off, n - off);
			if (w <= 0)
				break;
			off += (size_t)w;
		}
		close(in[1]);
	}

	/* rsync reports progress with \r, everything else with \n; treat both
	 * as terminators or a 4 GB copy arrives as one enormous line. */
	char buf[4096], line[1024];
	size_t ll = 0;
	ssize_t n;
	while ((n = read(out[0], buf, sizeof(buf))) > 0) {
		for (ssize_t i = 0; i < n; i++) {
			char c = buf[i];
			if (c == '\n' || c == '\r') {
				line[ll] = 0;
				if (ll) {
					if (parse)
						parse(line);
					else
						logf_("%s", line);
				}
				ll = 0;
			} else if (ll + 1 < sizeof(line)) {
				line[ll++] = c;
			}
		}
	}
	if (ll) {
		line[ll] = 0;
		if (parse)
			parse(line);
		else
			logf_("%s", line);
	}
	close(out[0]);

	int status = 0;
	waitpid(pid, &status, 0);
	int rc = WIFEXITED(status) ? WEXITSTATUS(status) : 128;

	for (int i = 0; i < nok; i++)
		if (rc == ok_codes[i])
			return 0;
	return rc;
}

static int run(char *const argv[])
{
	static const int ok[] = { 0 };
	return run_full(argv, NULL, NULL, ok, 1);
}

/* Same, with a secret on the child's stdin. cryptsetup's `--key-file=-` reads
 * from there, which keeps the passphrase out of argv and therefore out of
 * /proc/<pid>/cmdline. */
static int run_stdin(char *const argv[], const char *secret)
{
	static const int ok[] = { 0 };
	return run_full(argv, secret, NULL, ok, 1);
}

static void must(char *const argv[])
{
	int rc = run(argv);
	if (rc)
		fail("%s failed (exit %d)", argv[0], rc);
}

static void try_(char *const argv[])
{
	int rc = run(argv);
	if (rc)
		logf_("  (ignored: exit %d)", rc);
}

/* ──────────────────────────────────────────────────────────────────────── */

static void partname(const char *disk, int n, char *out, size_t cap)
{
	size_t l = strlen(disk);
	int digit = l && isdigit((unsigned char)disk[l - 1]);
	snprintf(out, cap, "%s%s%d", disk, digit ? "p" : "", n);
}

/* The index of a partition within its disk, read from the kernel, or -1 when
 * the node is not a partition. The kernel's own attribute is what makes sdaN,
 * nvme0n1pN and mmcblk0pN one case rather than three naming rules to parse —
 * and the reason the number is measured and never derived from the name. */
static int part_index(const char *part)
{
	/* sysfs is keyed by the kernel's node name, so a link is followed
	 * first: an answer file may name the ESP as /dev/disk/by-id/… or
	 * /dev/disk/by-partuuid/…, which mount and mkfs take and which has no
	 * directory of its own under /sys/class/block. */
	char real[PATH_MAX];
	if (realpath(part, real))
		part = real;

	const char *base = strrchr(part, '/');
	base = base ? base + 1 : part;

	char path[256], buf[32];
	int n = snprintf(path, sizeof(path), "/sys/class/block/%.64s/partition",
			 base);
	if (n < 0 || (size_t)n >= sizeof(path))
		return -1;
	if (kb_read_line_file(path, buf, sizeof(buf)) < 0)
		return -1;

	long idx = strtol(buf, NULL, 10);
	/* DISK_MAX_PARTS is the kernel's own ceiling; anything outside it is a
	 * value this attribute cannot hold and is read as unusable. */
	if (idx < 1 || idx > 256)
		return -1;
	return (int)idx;
}

static void mkpath(const char *fmt, ...)
{
	char path[512];
	va_list ap;
	va_start(ap, fmt);
	vsnprintf(path, sizeof(path), fmt, ap);
	va_end(ap);
	if (cfg.dry_run)
		return;
	for (char *p = path + 1; *p; p++) {
		if (*p != '/')
			continue;
		*p = 0;
		mkdir(path, 0755);
		*p = '/';
	}
	mkdir(path, 0755);
}

static void wr(const char *path, const char *fmt, ...)
{
	char full[512];
	snprintf(full, sizeof(full), "%s%s", TARGET, path);
	char body[8192];
	va_list ap;
	va_start(ap, fmt);
	vsnprintf(body, sizeof(body), fmt, ap);
	va_end(ap);
	logf_("write %s", full);
	if (cfg.dry_run)
		return;
	if (kb_write_file(full, body) < 0)
		fail("cannot write %s: %s", full, strerror(errno));
}

static int slurp(const char *path, char *buf, size_t cap)
{
	return kb_read_file(path, buf, cap);
}

/* ──────────────────────────────────────────────────────────────────────── */

static void unmount_below(const char *prefix)
{
	for (int pass = 0; pass < 4; pass++) {
		char buf[16384];
		if (slurp("/proc/mounts", buf, sizeof(buf)) < 0)
			return;
		char *save = NULL;
		char victims[16][192];
		int nv = 0;
		for (char *l = strtok_r(buf, "\n", &save); l && nv < 16;
		     l = strtok_r(NULL, "\n", &save)) {
			char dev[128], mnt[192];
			if (sscanf(l, "%127s %191s", dev, mnt) != 2)
				continue;
			if (!strncmp(mnt, prefix, strlen(prefix)))
				kb_strlcpy(victims[nv++], mnt, sizeof(victims[0]));
		}
		if (!nv)
			return;
		/* Deepest first, or the parent umount fails with EBUSY. */
		for (int i = nv - 1; i >= 0; i--) {
			char *a[] = { "umount", "-R", victims[i], NULL };
			try_(a);
		}
	}
}

static void unmount_disk(const char *disk)
{
	char buf[16384];
	if (slurp("/proc/mounts", buf, sizeof(buf)) < 0)
		return;
	char *save = NULL;
	for (char *l = strtok_r(buf, "\n", &save); l;
	     l = strtok_r(NULL, "\n", &save)) {
		char dev[128], mnt[192];
		if (sscanf(l, "%127s %191s", dev, mnt) != 2)
			continue;
		if (strncmp(dev, disk, strlen(disk)))
			continue;
		char *a[] = { "umount", "-l", mnt, NULL };
		logf_("unmounting %s (%s)", mnt, dev);
		try_(a);
	}
}

/*
 * EVERY MOUNT OF ONE DEVICE, by its device number. A logical volume is
 * mounted under /dev/mapper/<vg>-<lv> and chosen as /dev/<vg>/<lv>, so no
 * comparison of names finds it; the number is the one thing both share.
 */
static void unmount_rdev(dev_t want)
{
	char buf[16384];
	if (slurp("/proc/mounts", buf, sizeof(buf)) < 0)
		return;
	char *save = NULL;
	for (char *l = strtok_r(buf, "\n", &save); l;
	     l = strtok_r(NULL, "\n", &save)) {
		char dev[192], mnt[192];
		struct stat st;
		if (sscanf(l, "%191s %191s", dev, mnt) != 2 ||
		    stat(dev, &st) != 0 || !S_ISBLK(st.st_mode) ||
		    st.st_rdev != want)
			continue;
		char *a[] = { "umount", "-l", mnt, NULL };
		logf_("unmounting %s (%s)", mnt, dev);
		try_(a);
	}
}

static void unmount_dev(const char *path)
{
	struct stat st;
	if (stat(path, &st) == 0 && S_ISBLK(st.st_mode))
		unmount_rdev(st.st_rdev);
}

/*
 * TAKE DOWN EVERYTHING STACKED ON ONE BLOCK DEVICE, top first. The prober
 * activates every volume group, so a disk that carried LVM — a previous
 * install on it is the usual case — arrives at the erase plan with its
 * volumes live, and the kernel will not re-read a partition table under a
 * partition something holds: sfdisk's new table would be written and not
 * seen. A volume's whole group is deactivated, because it is losing a
 * physical volume; a container is closed; any other dm device is removed.
 * An array is named and left, and the partitioning that follows says why.
 */
static void release_holders(const char *name, int depth)
{
	char path[256];
	char **ents;

	if (depth > 8)
		return;
	snprintf(path, sizeof(path), "/sys/class/block/%.64s/holders", name);
	ents = kb_listdir(path, NULL);
	for (char **e = ents; e && *e; e++) {
		char uuid[160] = "", dmname[160] = "", vg[128], lv[128];
		char dev[96], sys[256];
		unsigned ma, mi;

		release_holders(*e, depth + 1);
		if (strncmp(*e, "dm-", 3)) {
			logf_("%s is held by %s — stop it before erasing this "
			      "disk", name, *e);
			continue;
		}
		/* By the number sysfs gives, not by a stat of /dev/dm-N: the
		 * node is devtmpfs's, and a /dev without it still has the
		 * mount to find. */
		snprintf(sys, sizeof(sys), "/sys/block/%.64s/dev", *e);
		if (kb_read_line_file(sys, dev, sizeof(dev)) >= 0 &&
		    sscanf(dev, "%u:%u", &ma, &mi) == 2)
			unmount_rdev(makedev(ma, mi));
		snprintf(sys, sizeof(sys), "/sys/block/%.64s/dm/uuid", *e);
		kb_read_line_file(sys, uuid, sizeof(uuid));
		snprintf(sys, sizeof(sys), "/sys/block/%.64s/dm/name", *e);
		kb_read_line_file(sys, dmname, sizeof(dmname));
		if (!dmname[0])
			continue;
		if (!strncmp(uuid, "LVM-", 4) &&
		    ki_dm_split(dmname, vg, sizeof(vg), lv, sizeof(lv)) == 0) {
			char *a[] = { "lvm", "vgchange", "-an", vg, NULL };
			try_(a);
		} else if (!strncmp(uuid, "CRYPT-", 6)) {
			char *a[] = { "cryptsetup", "close", dmname, NULL };
			try_(a);
		} else {
			char *a[] = { "dmsetup", "remove", dmname, NULL };
			try_(a);
		}
	}
	kb_strv_free(ents);
}

/* The disk itself, then each of its partitions. */
static void release_disk(const char *disk)
{
	char real[PATH_MAX], dir[300];
	const char *base;
	char **ents;

	base = kb_basename(realpath(disk, real) ? real : disk);
	release_holders(base, 0);
	snprintf(dir, sizeof(dir), "/sys/block/%.64s", base);
	ents = kb_listdir(dir, NULL);
	for (char **e = ents; e && *e; e++) {
		char chk[400];
		if (strncmp(*e, base, strlen(base)))
			continue;
		snprintf(chk, sizeof(chk), "%s/%.64s/partition", dir, *e);
		if (kb_path_exists(chk))
			release_holders(*e, 0);
	}
	kb_strv_free(ents);
}

/* ──────────────────────────────────────────────────────────────────────── */

static double rsync_total_pct;

static void rsync_parse(const char *line)
{
	/*   1.23G  45%  45.67MB/s    0:00:12 (xfr#123, to-chk=456/789) */
	char size[32], pct[16], speed[32], eta[32];
	int n = sscanf(line, "%31s %15s %31s %31s", size, pct, speed, eta);
	if (n >= 3 && strchr(pct, '%')) {
		int p = atoi(pct);
		rsync_total_pct = p / 100.0;
		emit('P', "%.4f", rsync_total_pct);
		emit('N', "%s copied  %s  ETA %s", size, speed,
		     n >= 4 ? eta : "--:--:--");
		return;
	}
	if (line[0] && line[0] != ' ')
		logf_("%s", line);
}

/* ──────────────────────────────────────────────────────────────────────── */

static const char *hash_password(const char *plain)
{
	static char salt[24];
	unsigned char rnd[12];
	static const char b64[] =
		"./0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";

	int fd = open("/dev/urandom", O_RDONLY | O_CLOEXEC);
	if (fd < 0 || read(fd, rnd, sizeof(rnd)) != (ssize_t)sizeof(rnd)) {
		struct timespec ts;
		clock_gettime(CLOCK_REALTIME, &ts);
		for (size_t i = 0; i < sizeof(rnd); i++)
			rnd[i] = (unsigned char)(ts.tv_nsec >> (i % 4 * 8)) ^
				 (unsigned char)(i * 37 + getpid());
	}
	if (fd >= 0)
		close(fd);

	strcpy(salt, "$6$");
	for (size_t i = 0; i < sizeof(rnd); i++)
		salt[3 + i] = b64[rnd[i] & 63];
	salt[3 + sizeof(rnd)] = '$';
	salt[4 + sizeof(rnd)] = 0;

	char *h = crypt(plain, salt);
	if (!h || h[0] != '$')
		fail("crypt() refused to hash the password");
	return h;
}

/* Rename the owner field of a subordinate-ID file (`/etc/subuid`,
 * `/etc/subgid`). newuidmap and containers/storage look the range up by the
 * user's NAME, so a range left keyed on the live account's name gives the
 * renamed one no mapping and every rootless container — every box — exits
 * before it starts. */
static void rewrite_subid(const char *file, const char *oldu, const char *newu)
{
	char buf[16384], out[16384], path[256];
	size_t ol = strlen(oldu);

	snprintf(path, sizeof(path), "%s%s", TARGET, file);
	if (slurp(path, buf, sizeof(buf)) <= 0)
		return;
	out[0] = 0;
	char *save = NULL;
	for (char *l = strtok_r(buf, "\n", &save); l;
	     l = strtok_r(NULL, "\n", &save)) {
		if (!strncmp(l, oldu, ol) && l[ol] == ':') {
			cat(out, sizeof(out), newu);
			cat(out, sizeof(out), l + ol);
		} else {
			cat(out, sizeof(out), l);
		}
		cat(out, sizeof(out), "\n");
	}
	if (!cfg.dry_run && kb_write_file(path, out) < 0)
		fail("cannot write %s", path);
	logf_("updated %s", path);
}

/* Rewrite one colon-separated database in place, field by field. Renaming
 * the live user touches passwd, shadow, group (as a member AND as the
 * primary group name) and the subordinate-ID ranges — miss any one of them
 * and the installed system logs nobody in or starts no box. login.conf's
 * `autologin` names the same account and is NOT touched here: do_config is
 * its single writer, runs before this step and already writes cfg.username,
 * so a second editor could only disagree with it about whether the key is
 * commented out.
 *
 * `admin` is the whole of the Administrator choice. The sudo port's
 * `%wheel ALL=(ALL) ALL`, the polkit admin rules and every root daemon's
 * configuration verbs grant on membership of `wheel`, and the live image ships
 * the account in it — so a non-administrator is taken OUT of `wheel` here, or
 * unticking the box would leave full root through sudo.
 *
 * ONLY `wheel` IS FILTERED. Every other membership is kept and renamed, and
 * `seat` is the one that matters: seatd hands the display to it, and
 * kdos-powerd's suspend, power-off and reboot, kdos-mountd and kdos-oomd admit
 * it as well as `wheel`. Dropping the account from `seat` would leave a
 * non-administrator with no desktop at all. */
static void rewrite_accounts(const char *oldu, const char *newu,
			     const char *fullname, const char *userhash,
			     const char *roothash, int admin)
{
	char buf[65536], out[65536];
	char path[256];

	/* passwd */
	snprintf(path, sizeof(path), "%s/etc/passwd", TARGET);
	if (slurp(path, buf, sizeof(buf)) > 0) {
		out[0] = 0;
		char *save = NULL;
		for (char *l = strtok_r(buf, "\n", &save); l;
		     l = strtok_r(NULL, "\n", &save)) {
			char f[7][256], line[1200];
			if (sscanf(l, "%255[^:]:%255[^:]:%255[^:]:%255[^:]:%255[^:]:%255[^:]:%255s",
				   f[0], f[1], f[2], f[3], f[4], f[5], f[6]) == 7 &&
			    !strcmp(f[0], oldu)) {
				snprintf(line, sizeof(line),
					 "%s:%s:%s:%s:%s:/home/%s:%s",
					 newu, f[1], f[2], f[3], fullname, newu,
					 f[6]);
				l = line;
			}
			cat(out, sizeof(out), l);
			cat(out, sizeof(out), "\n");
		}
		if (!cfg.dry_run && kb_write_file(path, out) < 0)
			fail("cannot write %s", path);
		logf_("updated %s", path);
	}

	/* group: both the membership lists and the primary group's own name */
	snprintf(path, sizeof(path), "%s/etc/group", TARGET);
	if (slurp(path, buf, sizeof(buf)) > 0) {
		out[0] = 0;
		char *save = NULL;
		for (char *l = strtok_r(buf, "\n", &save); l;
		     l = strtok_r(NULL, "\n", &save)) {
			char name[128], pw[64], gid[32], mem[512];
			mem[0] = 0;
			int n = sscanf(l, "%127[^:]:%63[^:]:%31[^:]:%511s",
				       name, pw, gid, mem);
			if (n < 3) {
				cat(out, sizeof(out), l);
				cat(out, sizeof(out), "\n");
				continue;
			}
			if (!strcmp(name, oldu))
				kb_strlcpy(name, newu, sizeof(name));

			char newmem[512] = "";
			char *ms = NULL;
			for (char *m = strtok_r(mem, ",", &ms); m;
			     m = strtok_r(NULL, ",", &ms)) {
				if (!admin && !strcmp(name, "wheel") &&
				    (!strcmp(m, oldu) || !strcmp(m, newu)))
					continue;
				if (newmem[0])
					cat(newmem, sizeof(newmem), ",");
				cat(newmem, sizeof(newmem),
				    !strcmp(m, oldu) ? newu : m);
			}
			char line[900];
			snprintf(line, sizeof(line), "%s:%s:%s:%s", name, pw, gid,
				 newmem);
			cat(out, sizeof(out), line);
			cat(out, sizeof(out), "\n");
		}
		if (!cfg.dry_run && kb_write_file(path, out) < 0)
			fail("cannot write %s", path);
		logf_("updated %s", path);
	}

	/* shadow */
	snprintf(path, sizeof(path), "%s/etc/shadow", TARGET);
	if (slurp(path, buf, sizeof(buf)) > 0) {
		out[0] = 0;
		char *save = NULL;
		long days = (long)(time(NULL) / 86400);
		for (char *l = strtok_r(buf, "\n", &save); l;
		     l = strtok_r(NULL, "\n", &save)) {
			char name[128];
			char line[1024];
			if (sscanf(l, "%127[^:]", name) == 1) {
				if (!strcmp(name, oldu) && userhash) {
					snprintf(line, sizeof(line),
						 "%s:%s:%ld:0:99999:7:::",
						 newu, userhash, days);
					l = line;
				} else if (!strcmp(name, "root")) {
					snprintf(line, sizeof(line),
						 "root:%s:%ld:0:99999:7:::",
						 roothash ? roothash : "!", days);
					l = line;
				}
			}
			cat(out, sizeof(out), l);
			cat(out, sizeof(out), "\n");
		}
		if (!cfg.dry_run && kb_write_file(path, out) < 0)
			fail("cannot write %s", path);
		if (!cfg.dry_run)
			chmod(path, 0600);
		logf_("updated %s", path);
	}

	if (strcmp(oldu, newu)) {
		rewrite_subid("/etc/subuid", oldu, newu);
		rewrite_subid("/etc/subgid", oldu, newu);
	}
}

/* ══════════════════════════════════════════════════════════════════════ */

static void do_prepare(void)
{
	emit('N', "checking the target");

	if (!cfg.disk[0])
		fail("no target disk selected");
	if (!kb_path_exists(cfg.disk))
		fail("%s has gone away", cfg.disk);
	if (geteuid() != 0)
		fail("kinstall must run as root");

	if (cfg.luks && !kb_have_prog("cryptsetup"))
		fail("cryptsetup is not installed — cannot create an encrypted "
		     "root");
	if (cfg.plan == PLAN_WIPE && cfg.lvm && !kb_have_prog("lvm"))
		fail("lvm is not installed — cannot put the root on LVM");
	for (const char **p = (const char *[]){ "mount", "umount",
						"mkfs.vfat", "rsync", NULL };
	     *p; p++)
		if (!kb_have_prog(*p))
			fail("required tool missing: %s", *p);
	if (!kb_have_prog(ki_fs(cfg.fstype)->mkfs))
		fail("required tool missing: %s", ki_fs(cfg.fstype)->mkfs);

	char *sw[] = { "swapoff", "-a", NULL };
	try_(sw);
	unmount_below(TARGET "/");
	char *um[] = { "umount", "-R", TARGET, NULL };
	try_(um);
	unmount_disk(cfg.disk);
	if (cfg.plan == PLAN_WIPE)
		release_disk(cfg.disk);
	else if (cfg.part_root[0])
		/* A logical volume is not under the disk's name. */
		unmount_dev(cfg.part_root);
	mkpath(TARGET);
	emit('P', "1");
}

static void do_partition(void)
{
	char layout[512];
	long esp_mb = 512;

	emit('N', "wiping %s", cfg.disk);
	char *wipe[] = { "wipefs", "-a", cfg.disk, NULL };
	must(wipe);

	/* `V` is sfdisk's name for the Linux LVM type, which is what a
	 * physical volume's partition is marked as. */
	char rtype = cfg.lvm ? 'V' : 'L';
	if (cfg.swap == SWAP_PART && cfg.swap_mb > 0)
		snprintf(layout, sizeof(layout),
			 "label: gpt\n,%ldM,U\n,%ldM,S\n,,%c\n", esp_mb,
			 cfg.swap_mb, rtype);
	else
		snprintf(layout, sizeof(layout),
			 "label: gpt\n,%ldM,U\n,,%c\n", esp_mb, rtype);

	logf_("sfdisk layout:");
	for (char *p = layout, *nl; (nl = strchr(p, '\n')); p = nl + 1) {
		*nl = 0;
		logf_("  %s", p);
		*nl = '\n';
	}

	emit('N', "writing the partition table");
	char *sf[] = { "sfdisk", "--wipe", "always", cfg.disk, NULL };
	static const int ok0[] = { 0 };
	int rc = run_full(sf, layout, NULL, ok0, 1);
	if (rc)
		fail("sfdisk failed (exit %d)", rc);

	char *pp[] = { "partprobe", cfg.disk, NULL };
	try_(pp);
	char *us[] = { "udevadm", "settle", NULL };
	try_(us);

	/* udev can be a beat behind sfdisk; the node has to exist before mkfs */
	for (int i = 0; i < 50; i++) {
		char p2[96];	/* same width as part_esp/part_root */
		partname(cfg.disk, 2, p2, sizeof(p2));
		if (kb_path_exists(p2) || cfg.dry_run)
			break;
		struct timespec ts = { 0, 100000000 };
		nanosleep(&ts, NULL);
	}
	emit('P', "1");
}

/* The name the container is opened under, on the kernel command line and in
 * /dev/mapper. Fixed rather than configurable: the initramfs has to be told it
 * anyway, and a second knob buys nothing. */
#define LUKS_NAME "kdosroot"

/* The raw partition holding the LUKS header, once part_root has been redirected
 * at the mapper device. Empty when the install is not encrypted. */
static char luks_part[192];

/* The physical volume of an erase plan on LVM: the root partition, or the
 * container opened on it. Empty when the root is not on LVM. */
static char pv_dev[192];

static void resolve_parts(void)
{
	if (cfg.plan == PLAN_WIPE) {
		partname(cfg.disk, 1, part_esp, sizeof(part_esp));
		if (cfg.swap == SWAP_PART && cfg.swap_mb > 0) {
			partname(cfg.disk, 2, part_swap, sizeof(part_swap));
			partname(cfg.disk, 3, part_root, sizeof(part_root));
		} else {
			partname(cfg.disk, 2, part_root, sizeof(part_root));
		}
	} else {
		kb_strlcpy(part_esp, cfg.part_esp, sizeof(part_esp));
		kb_strlcpy(part_root, cfg.part_root, sizeof(part_root));
	}
	/*
	 * With LUKS, `part_root` becomes the MAPPER device from here on: the
	 * filesystem, the mount, the fstab UUID and the rsync all belong to what
	 * is inside the container, and only do_format and the boot options ever
	 * need the container itself. Keeping one name for "where the root
	 * filesystem is" is what stops half the installer writing to the wrong
	 * device.
	 */
	if (cfg.luks) {
		kb_strlcpy(luks_part, part_root, sizeof(luks_part));
		snprintf(part_root, sizeof(part_root), "/dev/mapper/%s",
			 LUKS_NAME);
	}
	/*
	 * AND WITH LVM, what was the root becomes the physical volume and the
	 * root moves once more, to the volume. Under LUKS the group is inside
	 * the container, which is the order that asks for one passphrase for
	 * both slots; the initramfs activates on each side of the unlock, so
	 * it finds the group either way.
	 */
	if (cfg.plan == PLAN_WIPE && cfg.lvm) {
		kb_strlcpy(pv_dev, part_root, sizeof(pv_dev));
		snprintf(part_root, sizeof(part_root), "/dev/%s/%s", KI_VG,
			 KI_LV_A);
	}

	logf_("ESP  = %s", part_esp);
	logf_("root = %s", part_root);
	if (luks_part[0])
		logf_("LUKS = %s", luks_part);
	if (pv_dev[0])
		logf_("PV   = %s, group %s", pv_dev, KI_VG);
	if (part_swap[0])
		logf_("swap = %s", part_swap);
}

/*
 * THE GROUP AND SLOT A'S VOLUME, on a physical volume that was a partition a
 * moment ago. Every call goes through `lvm <command>` rather than the
 * pvcreate/vgcreate names, which are symlinks an image may not carry.
 *
 * THE OLD SIGNATURES GO FIRST. The erase plan writes the same layout at the
 * same offsets, so a disk that held an earlier install still has that
 * install's PV label where the new partition starts: pvcreate refuses it, and
 * vgcreate refuses the group name it still carries. A container has just been
 * formatted over its partition and has none.
 */
static void make_volume_group(void)
{
	char vgdev[192];
	char *pv[] = { "lvm", "pvcreate", "-y", pv_dev, NULL };
	char *vg[] = { "lvm", "vgcreate", "-y", (char *)KI_VG, pv_dev, NULL };
	char *lv[] = { "lvm", "lvcreate", "-y", "-n", (char *)KI_LV_A, "-l",
		       ki_lvm_half() ? "50%VG" : "100%FREE", (char *)KI_VG,
		       NULL };
	char *us[] = { "udevadm", "settle", NULL };

	if (!cfg.luks) {
		char *wf[] = { "wipefs", "-a", pv_dev, NULL };
		must(wf);
	}
	emit('N', "volume group %s on %s", KI_VG, pv_dev);
	must(pv);
	must(vg);
	emit('N', "%s/%s, %s of the group", KI_VG, KI_LV_A,
	     ki_lvm_half() ? "half" : "all");
	must(lv);
	try_(us);

	snprintf(vgdev, sizeof(vgdev), "/dev/%s/%s", KI_VG, KI_LV_A);
	for (int i = 0; i < 50 && !cfg.dry_run && !kb_path_exists(vgdev); i++) {
		struct timespec ts = { 0, 100000000 };
		nanosleep(&ts, NULL);
	}
	if (!cfg.dry_run && !kb_path_exists(vgdev))
		fail("%s did not appear after lvcreate", vgdev);
	emit('P', "0.5");
}

static void do_format(void)
{
	resolve_parts();

	if (!part_root[0])
		fail("no root partition resolved");
	/* A retried Format meets whatever the failed one opened on this
	 * disk, and luksFormat and pvcreate both refuse a held partition. */
	if (cfg.plan == PLAN_WIPE)
		release_disk(cfg.disk);
	/* The device that has to exist now is the one the first command
	 * writes: the container's partition, the physical volume's, or the
	 * root itself. The mapper device and the volume appear later. */
	const char *raw = luks_part[0] ? luks_part
			  : pv_dev[0]  ? pv_dev
				       : part_root;
	if (!cfg.dry_run && !kb_path_exists(raw))
		fail("%s does not exist", raw);

	/*
	 * LUKS2 first, because everything after this point talks to the mapper
	 * device. The passphrase goes in on STDIN both times: an argument would
	 * be readable through /proc/<pid>/cmdline by every process on the
	 * machine for as long as cryptsetup runs.
	 */
	if (cfg.luks) {
		if (!luks_part[0])
			fail("no partition to encrypt");
		emit('N', "luksFormat %s", luks_part);
		char *lf[] = { "cryptsetup", "luksFormat", "--type", "luks2",
			       "--batch-mode", "--key-file=-", luks_part, NULL };
		if (run_stdin(lf, cfg.luks_pass))
			fail("cryptsetup luksFormat failed");

		emit('N', "opening %s as %s", luks_part, LUKS_NAME);
		char *lo[] = { "cryptsetup", "open", "--key-file=-", luks_part,
			       (char *)LUKS_NAME, NULL };
		if (run_stdin(lo, cfg.luks_pass))
			fail("cryptsetup open failed");
		if (!cfg.dry_run && !kb_path_exists(pv_dev[0] ? pv_dev
							       : part_root))
			fail("%s did not appear after unlocking",
			     pv_dev[0] ? pv_dev : part_root);
		emit('P', "0.4");
	}

	if (pv_dev[0])
		make_volume_group();

	emit('N', "mkfs %s on %s", cfg.fstype, part_root);
	const Filesystem *fs = ki_fs(cfg.fstype);
	char *mk[] = { (char *)fs->mkfs, (char *)fs->force, "-L", "KDOS",
		       part_root, NULL };
	must(mk);
	emit('P', "0.6");

	if (cfg.format_esp && part_esp[0]) {
		emit('N', "mkfs vfat on %s", part_esp);
		char *mv[] = { "mkfs.vfat", "-F", "32", "-n", "KDOS_EFI",
			       part_esp, NULL };
		must(mv);
	} else {
		logf_("keeping the existing ESP filesystem on %s", part_esp);
	}
	emit('P', "0.85");

	if (part_swap[0]) {
		emit('N', "mkswap %s", part_swap);
		char *ms[] = { "mkswap", part_swap, NULL };
		try_(ms);
	}
	emit('P', "1");
}

/*
 * WHERE AN EXPORTED SET IS READ FROM, and it cannot be a path under TARGET.
 *
 * TARGET is `/mnt`, and a stick with an archive on it is mounted at
 * `/mnt/<something>` — so the moment the target root is mounted, that stick is
 * UNDERNEATH it and every path into it resolves inside the filesystem that was
 * just created empty. `do_apps` would then report that an archive it listed a
 * moment ago is not there.
 *
 * The tree is bind-mounted somewhere the target cannot cover BEFORE that
 * happens, and the archive path is rewritten to the new location. A bind
 * rather than a second mount of the device because the device is not the
 * installer's to name: it was mounted by the initramfs or by the mount daemon,
 * and the only handle anything has on it is the path.
 *
 * AN ARCHIVE OUTSIDE TARGET IS LEFT ALONE. `/media` and `/run/media` are not
 * covered by mounting `/mnt`, so binding them would be work that buys nothing.
 */
#define MEDIUM_BIND "/run/kdos-medium"

static void bind_medium(void)
{
	char *b[] = { "mount", "--bind", TARGET, (char *)MEDIUM_BIND, NULL };
	/* MEDIUM_BIND replaces TARGET at the front, and it is the longer of the
	 * two — so the rewritten path can exceed the field it came out of. */
	char moved[sizeof(ki_apps_archive) + sizeof(MEDIUM_BIND)];

	if (!ki_apps_archive[0])
		return;
	if (strncmp(ki_apps_archive, TARGET "/", sizeof(TARGET)))
		return;			/* not under the target's mountpoint */

	mkpath(MEDIUM_BIND);
	if (run(b) != 0) {
		logf_("could not bind %s; %s becomes unreadable once the "
		      "target is mounted", TARGET, ki_apps_archive);
		ki_apps_archive[0] = '\0';
		return;
	}
	snprintf(moved, sizeof(moved), "%s%s", MEDIUM_BIND,
		 ki_apps_archive + sizeof(TARGET) - 1);
	logf_("archive %s -> %s", ki_apps_archive, moved);
	kb_strlcpy(ki_apps_archive, moved, sizeof(ki_apps_archive));
}

static void do_mount(void)
{
	resolve_parts();
	bind_medium();
	mkpath(TARGET);
	emit('N', "mounting %s at %s", part_root, TARGET);
	char *m[] = { "mount", part_root, TARGET, NULL };
	must(m);

	mkpath(TARGET "/boot/efi");
	if (part_esp[0]) {
		emit('N', "mounting %s at %s/boot/efi", part_esp, TARGET);
		char *me[] = { "mount", part_esp, TARGET "/boot/efi", NULL };
		must(me);
	}
	emit('P', "1");
}

static void do_copy(void)
{
	char *argv[32];
	int n = 0;

	argv[n++] = "rsync";
	argv[n++] = "-aHAX";
	argv[n++] = "-x";		/* never cross into /proc or /sys   */
	argv[n++] = "--numeric-ids";
	argv[n++] = "--info=progress2";
	argv[n++] = "--no-inc-recursive";
	argv[n++] = "--exclude=/dev/*";
	argv[n++] = "--exclude=/proc/*";
	argv[n++] = "--exclude=/sys/*";
	argv[n++] = "--exclude=/tmp/*";
	argv[n++] = "--exclude=/run/*";
	argv[n++] = "--exclude=/mnt/*";
	argv[n++] = "--exclude=/media/*";
	argv[n++] = "--exclude=/lost+found";
	argv[n++] = "--exclude=/var/log/kinstall.log";
	/*
	 * PER-MACHINE IDENTITY IS NOT COPIED. Both of these are generated on
	 * first boot only when absent (fs/etc/init.d/40_dbus.sh,
	 * fs/etc/init.d/70_sshd.sh), so a copy from the live session would be
	 * adopted by the installed system and never regenerated: every machine
	 * installed from one medium would answer GetMachineId with the same
	 * UUID and present the same SSH host keys.
	 */
	argv[n++] = "--exclude=/var/lib/dbus/machine-id";
	argv[n++] = "--exclude=/etc/ssh/ssh_host_*";
	if (!cfg.with_appbox)
		argv[n++] = "--exclude=/home/kdos/.local/share/containers/***";
	argv[n++] = "/";
	argv[n++] = TARGET "/";
	argv[n] = NULL;

	emit('N', "copying the live tree");
	emit('P', "0");
	rsync_total_pct = 0;

	/* 24 is "some files vanished before transfer" — on a live system with
	 * a running session that is the normal case, not a failure. */
	static const int ok[] = { 0, 24 };
	int rc = run_full(argv, NULL, rsync_parse, ok, 2);
	if (rc)
		fail("rsync failed (exit %d)", rc);

	/* The excluded pseudo-directories still have to exist in the target. */
	static const char *dirs[] = { "dev", "proc", "sys", "run", "tmp", "mnt",
				      "media", NULL };
	for (int i = 0; dirs[i]; i++)
		mkpath("%s/%s", TARGET, dirs[i]);
	if (!cfg.dry_run)
		chmod(TARGET "/tmp", 01777);
	emit('P', "1");
}

/*
 * The chosen applications.
 *
 * NOTHING IS BAKED ONTO THE MEDIUM, so there is no copy to make. What this
 * does depends on what `ki_apps_route()` decided, and the Applications page
 * has already SAID which — a person must not discover at first boot that
 * nothing was installed.
 *
 *   import   an exported set on a mounted device: staged through kdos-packd in
 *            the target, which verifies each pack where it mounts it. Offline,
 *            and the only route on a machine with no network.
 *   network  built during the install, over the network.
 *   pending  recorded in /var/lib/kdos/apps-pending; the first session offers
 *            them. Building is podman and apt and can be most of an hour, and
 *            an installer that did that silently is an installer that appears
 *            to have hung.
 *
 * EITHER WAY THE STORE'S DIRECTORIES ARE MADE. kdos-packd sets the staging
 * mode at startup, but a first boot that inherited 0755 would refuse an import
 * until the daemon had run once — which reads as the feature not working.
 */
static void do_apps(void)
{
	int route = ki_apps_route();
	int n = 0;

	mkpath("%s/var/lib/kdos/packs", TARGET);
	mkpath("%s/var/lib/kdos/packs/staging", TARGET);
	mkpath("%s/var/lib/kdos/packs/mnt", TARGET);
	if (!cfg.dry_run)
		chmod(TARGET "/var/lib/kdos/packs/staging", 01777);

	for (int i = 0; i < ki_ngroup; i++)
		if (ki_group[i].chosen)
			n++;
	if (route == APPS_NONE || !n) {
		emit('L', "no applications chosen");
		emit('P', "1");
		return;
	}

	if (route == APPS_IMPORT) {
		KbArgv a = {0};

		emit('N', "importing %s", ki_apps_archive);
		kb_argv_add(&a, "kdos-appbox");
		kb_argv_add(&a, "import");
		kb_argv_add(&a, ki_apps_archive);
		kb_argv_end(&a);
		if (!cfg.dry_run && kb_run(&a) != 0) {
			/* NOT `fail`. The system is installed and bootable;
			 * an archive that would not import is a reason to say
			 * so and carry on, not to abandon a disk mid-install
			 * and leave a machine with no operating system. */
			emit('W', "the set would not import — the selection is "
				  "recorded instead");
			route = APPS_PENDING;
		} else {
			emit('L', "imported from %s", ki_apps_archive);
		}
	} else if (route == APPS_NETWORK) {
		KbArgv a = {0};

		emit('N', "building %d group(s) — this takes minutes", n);
		kb_argv_add(&a, "kdos-appbox");
		kb_argv_add(&a, "install");
		for (int i = 0; i < ki_ngroup; i++)
			if (ki_group[i].chosen)
				kb_argv_add(&a, ki_group[i].id);
		kb_argv_end(&a);
		if (!cfg.dry_run && kb_run(&a) != 0) {
			emit('W', "some did not build — the selection is "
				  "recorded for the first login");
			route = APPS_PENDING;
		} else {
			emit('L', "%d group(s) built", n);
		}
	}

	if (route == APPS_PENDING) {
		KbBuf b = {0};
		char path[512];

		kb_buf_printf(&b, "# Chosen during the install and not yet "
				  "built.\n# `kdos app install --pending` "
				  "builds them.\n");
		for (int i = 0; i < ki_ngroup; i++)
			if (ki_group[i].chosen)
				kb_buf_printf(&b, "%s\n", ki_group[i].id);
		snprintf(path, sizeof(path), "%s/var/lib/kdos/apps-pending",
			 TARGET);
		mkpath("%s/var/lib/kdos", TARGET);
		if (!cfg.dry_run)
			kb_write_all(path, b.p, b.n);
		kb_buf_free(&b);
		emit('L', "%d group(s) recorded for the first login", n);
	}
	emit('P', "1");
}

static void do_config(void)
{
	char buf[65536];
	Part p;

	resolve_parts();

	char root_uuid[64] = "", esp_uuid[64] = "", swap_uuid[64] = "";
	probe_part(part_root, &p);
	kb_strlcpy(root_uuid, p.uuid, sizeof(root_uuid));
	if (part_esp[0]) {
		probe_part(part_esp, &p);
		kb_strlcpy(esp_uuid, p.uuid, sizeof(esp_uuid));
	}
	if (part_swap[0]) {
		probe_part(part_swap, &p);
		kb_strlcpy(swap_uuid, p.uuid, sizeof(swap_uuid));
	}
	if (!root_uuid[0] && !cfg.dry_run)
		fail("cannot read a UUID back from %s", part_root);
	logf_("root UUID %s", root_uuid);

	/* fstab is APPENDED to, never replaced. The shipped file carries the
	 * proc/sys/dev lines and the tmpfs /tmp with mode=1777 — dropping
	 * those locks every non-root user out of /tmp, which surfaces later as
	 * "GIMP does not start" and costs an afternoon to trace back here. */
	emit('N', "fstab");
	const Filesystem *rfs = ki_fs(cfg.fstype);
	char fst[8192] = "";
	snprintf(fst, sizeof(fst),
		 "# Written by the KDOS installer.\n"
		 "UUID=%s\t/\t%s\t%s\t0 %d\n",
		 root_uuid, rfs->name, rfs->opts, rfs->passno);
	if (esp_uuid[0]) {
		char l[256];
		snprintf(l, sizeof(l),
			 "UUID=%s\t/boot/efi\tvfat\tdefaults,umask=0077\t0 2\n",
			 esp_uuid);
		cat(fst, sizeof(fst), l);
	}
	if (swap_uuid[0]) {
		char l[256];
		snprintf(l, sizeof(l), "UUID=%s\tnone\tswap\tdefaults\t0 0\n",
			 swap_uuid);
		cat(fst, sizeof(fst), l);
	}
	if (cfg.swap == SWAP_FILE && cfg.swap_mb > 0)
		cat(fst, sizeof(fst), "/swapfile\tnone\tswap\tdefaults\t0 0\n");
	cat(fst, sizeof(fst), "\n");

	if (slurp(TARGET "/etc/fstab", buf, sizeof(buf)) > 0)
		cat(fst, sizeof(fst), buf);
	wr("/etc/fstab", "%s", fst);
	emit('P', "0.3");

	wr("/etc/hostname", "%s\n", cfg.hostname);

	/*
	 * The machine's own name resolves through the `127.0.1.1` line of
	 * `/etc/hosts` — musl reads that file and nothing else before the
	 * DNS — and the image ships it naming `kdos`. Left alone, any other
	 * hostname falls through to the DNS: NXDOMAIN online, a timeout
	 * offline, for everything that resolves its own name.
	 */
	{
		char hosts[8192] = "", *save = NULL;
		int seen = 0;
		if (slurp(TARGET "/etc/hosts", buf, sizeof(buf)) > 0) {
			for (char *l = strtok_r(buf, "\n", &save); l;
			     l = strtok_r(NULL, "\n", &save)) {
				if (!strncmp(l, "127.0.1.1", 9) &&
				    (l[9] == ' ' || l[9] == '\t')) {
					if (seen++)
						continue;
					cat(hosts, sizeof(hosts), "127.0.1.1   ");
					cat(hosts, sizeof(hosts), cfg.hostname);
				} else {
					cat(hosts, sizeof(hosts), l);
				}
				cat(hosts, sizeof(hosts), "\n");
			}
		} else {
			cat(hosts, sizeof(hosts),
			    "127.0.0.1   localhost\n"
			    "::1         localhost ip6-localhost ip6-loopback\n");
		}
		if (!seen) {
			cat(hosts, sizeof(hosts), "127.0.1.1   ");
			cat(hosts, sizeof(hosts), cfg.hostname);
			cat(hosts, sizeof(hosts), "\n");
		}
		wr("/etc/hosts", "%s", hosts);
	}

	/*
	 * BOTH HALVES OR NEITHER. `/etc/localtime` is what a program reading
	 * the zoneinfo tree follows; `TZ` is what musl reads, and it WINS
	 * where it is set — so a `TZ` naming different rules from the symlink
	 * makes `date` and the desktop disagree about the time. The colon form
	 * points musl at the same file, which is the only value that cannot
	 * drift from it. `kdos-powerd`'s `timezone` verb writes exactly these
	 * two afterwards.
	 */
	wr("/etc/profile.d/20-timezone.sh",
	   "# Written by the KDOS installer.\n"
	   "# `/etc/localtime` is what a program reading the zoneinfo tree\n"
	   "# follows; this is what musl reads, and it wins where it is set.\n"
	   "# Both say the same zone or `date` and the desktop disagree.\n"
	   "export TZ='%s'\n", cfg.tz);
	if (cfg.tz_label[0]) {
		char zi[256];
		snprintf(zi, sizeof(zi), "/usr/share/zoneinfo/%s", cfg.tz_label);
		if (kb_path_exists(zi) && !cfg.dry_run) {
			unlink(TARGET "/etc/localtime");
			if (symlink(zi, TARGET "/etc/localtime") == 0)
				logf_("linked /etc/localtime -> %s", zi);
		} else if (!kb_path_exists(zi)) {
			/* A zone the picker offered and the target does not
			 * carry leaves `TZ` pointing at a symlink that is not
			 * there, and musl answers UTC with no error. Said
			 * here, where the log is read. */
			logf_("no zone file for %s; the machine will keep UTC",
			      cfg.tz_label);
		}
	}

	/*
	 * THE WI-FI COUNTRY, FROM THE ZONE. cfg80211 starts in the world
	 * regulatory domain, which keeps every 5 GHz DFS channel closed and caps
	 * transmit power, until something names a country. `zone.tab` gives
	 * each zone exactly one, so the zone just chosen is the answer; a zone
	 * with no row (UTC) writes nothing. `kdos-powerd`'s `timezone` verb
	 * rewrites the same file afterwards.
	 */
	if (cfg.tz_label[0]) {
		char cc[3] = "";
		char line[512];
		FILE *zt = fopen("/usr/share/zoneinfo/zone.tab", "r");

		while (zt && !cc[0] && fgets(line, sizeof(line), zt)) {
			char c[3], z[128];

			if (line[0] != '#' &&
			    sscanf(line, "%2[A-Z]\t%*[^\t]\t%127[^\t\n]", c,
				   z) == 2 &&
			    strlen(c) == 2 && !strcmp(z, cfg.tz_label))
				memcpy(cc, c, 3);
		}
		if (zt)
			fclose(zt);
		if (cc[0]) {
			mkpath(TARGET "/etc/modprobe.d");
			wr("/etc/modprobe.d/kdos-regdom.conf",
			   "# Written by the KDOS installer from the timezone (%s):\n"
			   "# the country the Wi-Fi radio's channels and transmit\n"
			   "# power are set for.\n"
			   "options cfg80211 ieee80211_regdom=%s\n",
			   cfg.tz_label, cc);
		}
	}

	wr("/etc/keymap", "%s\n", cfg.keymap);

	/*
	 * login.conf's `autologin`, edited in place rather than rewritten: the
	 * shipped file is mostly the explanation of what the key does, and a
	 * one-line replacement would leave the installed system with a
	 * configuration file nobody can read. The lines are walked by hand
	 * rather than with strtok for the same reason — strtok collapses runs
	 * of newlines, and the installed file would arrive with every
	 * paragraph break in that explanation gone.
	 *
	 * THIS IS THE INSTALL'S SINGLE WRITER OF THE KEY, and the key is the
	 * ONLY place the desktop's account is named: `/etc/inittab` runs
	 * `kdos-getty tty1 kdos-login tty1` and carries no account at all. So
	 * writing cfg.username here is also what carries a renamed user to
	 * tty1; a second editor in a later step would only disagree with this
	 * one about whether the key is commented, and leave the file with two
	 * `autologin` lines — of which kdos-login honours the last.
	 *
	 * OFF IS A COMMENTED LINE AND NOT AN EMPTY VALUE. kdos-login reads the
	 * key and asks when it finds none, and `autologin =` with nothing after
	 * it would be a key naming an account called "", which agetty would be
	 * handed. A file that has no line at all gains one only when autologin
	 * was asked for.
	 */
	{
		char cc[8192];
		int n = slurp(TARGET "/etc/kdos/login.conf", cc, sizeof(cc));

		if (n > 0) {
			char out[8192];
			size_t o = 0;
			int done = 0;
			char *line = cc;

			while (*line) {
				char *nl = strchr(line, '\n');
				char *next = nl ? nl + 1 : line + strlen(line);
				const char *p = line;

				if (nl)
					*nl = '\0';
				while (*p == ' ' || *p == '\t')
					p++;
				if (*p == '#')
					p++;
				while (*p == ' ' || *p == '\t')
					p++;
				if (!strncmp(p, "autologin", 9) &&
				    (p[9] == ' ' || p[9] == '\t' ||
				     p[9] == '=')) {
					o += (size_t)snprintf(out + o,
							      sizeof(out) - o,
							      cfg.autologin
							      ? "autologin = %s\n"
							      : "#autologin = %s\n",
							      cfg.username);
					done = 1;
				} else {
					o += (size_t)snprintf(out + o,
							      sizeof(out) - o,
							      "%s\n", line);
				}
				if (o >= sizeof(out) - 64)
					break;
				line = next;
			}
			if (!done && cfg.autologin)
				snprintf(out + o, sizeof(out) - o,
					 "autologin = %s\n", cfg.username);
			wr("/etc/kdos/login.conf", "%s", out);
		}
	}

	emit('P', "0.6");

	if (kb_path_exists("/etc/resolv.conf") && !cfg.dry_run) {
		char rc[4096];
		if (slurp("/etc/resolv.conf", rc, sizeof(rc)) > 0)
			wr("/etc/resolv.conf", "%s", rc);
	}

	emit('N', "services");
	mkpath(TARGET "/etc/service.disabled");
	for (int i = 0; i < ki_nservices; i++) {
		char path[256];
		snprintf(path, sizeof(path), "%s/etc/service.disabled/%s", TARGET,
			 ki_services[i].name);
		if (cfg.svc_off & (1u << i)) {
			logf_("disable service %s", ki_services[i].name);
			if (!cfg.dry_run)
				kb_write_file(path, "");
		} else {
			if (!cfg.dry_run)
				unlink(path);
		}
	}

	if (cfg.swap == SWAP_FILE && cfg.swap_mb > 0) {
		emit('N', "swapfile (%ld MiB)", cfg.swap_mb);
		char sz[32];
		snprintf(sz, sizeof(sz), "%ldM", cfg.swap_mb);

		/*
		 * How a swapfile is made depends on the filesystem under it,
		 * and getting it wrong does not fail here — it fails at the
		 * next boot's `swapon -a`, with the machine simply having no
		 * swap and nothing saying why.
		 *
		 * fallocate leaves unwritten extents; swapon rejects those on
		 * xfs ("swapfile has holes"). btrfs additionally needs the file
		 * to be NOCOW and uncompressed, which is the whole reason
		 * `btrfs filesystem mkswapfile` exists.
		 */
		int mode = ki_fs(cfg.fstype)->swapfile;
		if (mode == KI_SWAPFILE_FALLOCATE && !kb_have_prog("fallocate"))
			mode = KI_SWAPFILE_DD;
		if (mode == KI_SWAPFILE_BTRFS && !kb_have_prog("btrfs"))
			mode = KI_SWAPFILE_DD;

		if (mode == KI_SWAPFILE_BTRFS) {
			char *bs[] = { "btrfs", "filesystem", "mkswapfile",
				       "-s", sz, TARGET "/swapfile", NULL };
			try_(bs);
		} else {
			if (mode == KI_SWAPFILE_FALLOCATE) {
				char *fa[] = { "fallocate", "-l", sz,
					       TARGET "/swapfile", NULL };
				try_(fa);
			} else {
				char cnt[32];
				snprintf(cnt, sizeof(cnt), "count=%ld",
					 cfg.swap_mb);
				char *dd[] = { "dd", "if=/dev/zero",
					       "of=" TARGET "/swapfile",
					       "bs=1M", cnt, NULL };
				try_(dd);
			}
			if (!cfg.dry_run)
				chmod(TARGET "/swapfile", 0600);
			char *mks[] = { "mkswap", TARGET "/swapfile", NULL };
			try_(mks);
		}
	}
	emit('P', "1");
}

static void do_accounts(void)
{
	const char *uhash = NULL, *rhash = NULL;

	emit('N', "hashing credentials");
	if (cfg.userpass[0])
		uhash = kb_strdup(hash_password(cfg.userpass));
	if (!cfg.root_locked && cfg.rootpass[0])
		rhash = kb_strdup(hash_password(cfg.rootpass));
	else
		rhash = "!";

	if (strcmp(cfg.username, "kdos")) {
		emit('N', "renaming kdos -> %s", cfg.username);
		char from[256], to[256];
		snprintf(from, sizeof(from), "%s/home/kdos", TARGET);
		snprintf(to, sizeof(to), "%s/home/%s", TARGET, cfg.username);
		if (!cfg.dry_run && kb_path_exists(from) && rename(from, to) < 0)
			logf_("  home rename failed: %s", strerror(errno));
	}

	rewrite_accounts("kdos", cfg.username, cfg.fullname, uhash, rhash,
			 cfg.user_wheel);
	emit('P', "1");
}

static void do_theme(void)
{
	if (!kb_have_prog("kdos")) {
		logf_("kdos not on PATH — leaving the seeded accent alone");
		return;
	}

	/* Run the LIVE `kdos theme` with HOME pointed into the target rather
	 * than chrooting: the two trees are the same binaries, and this way
	 * the generators need no /proc, no /dev and no bind mounts. */
	char home[256];
	const char *targets[2];
	char u[256];
	snprintf(home, sizeof(home), "%s/etc/skel", TARGET);
	snprintf(u, sizeof(u), "%s/home/%s", TARGET, cfg.username);
	targets[0] = home;
	targets[1] = u;

	for (int i = 0; i < 2; i++) {
		if (!kb_path_exists(targets[i]))
			continue;
		emit('N', "%s -> %s", cfg.theme, targets[i]);
		logf_("$ HOME=%s kdos theme %s", targets[i], cfg.theme);
		if (cfg.dry_run)
			continue;

		pid_t pid = fork();
		if (pid == 0) {
			setenv("HOME", targets[i], 1);
			unsetenv("XDG_CONFIG_HOME");
			unsetenv("XDG_CACHE_HOME");
			int null = open("/dev/null", O_RDWR);
			dup2(null, 0);
			dup2(null, 1);
			dup2(null, 2);
			execlp("kdos", "kdos", "theme", cfg.theme, NULL);
			_exit(127);
		}
		int st = 0;
		waitpid(pid, &st, 0);
		if (WEXITSTATUS(st))
			logf_("  kdos theme exited %d", WEXITSTATUS(st));
		emit('P', "%.2f", (i + 1) / 2.0);
	}
}

static void copy_file(const char *src, const char *dst)
{
	if (cfg.dry_run) {
		logf_("copy %s -> %s", src, dst);
		return;
	}
	int in = open(src, O_RDONLY | O_CLOEXEC);
	if (in < 0) {
		logf_("  missing %s", src);
		return;
	}
	int out = open(dst, O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, 0644);
	if (out < 0) {
		close(in);
		logf_("  cannot create %s: %s", dst, strerror(errno));
		return;
	}
	char b[65536];
	ssize_t n;
	while ((n = read(in, b, sizeof(b))) > 0) {
		ssize_t off = 0;
		while (off < n) {
			ssize_t w = write(out, b + off, (size_t)(n - off));
			if (w <= 0)
				break;
			off += w;
		}
	}
	close(in);
	close(out);
	logf_("copy %s -> %s", src, dst);
}

/*
 * Room on the ESP for a second slot's kernel. An A/B update writes slot B's
 * kernel and initramfs beside slot A's, so an ESP that holds one pair and not
 * two installs and boots fine and then refuses every A/B update. The ESP this
 * installer makes is 512 MiB, which holds several pairs; a reused one — a
 * 100 MiB ESP another system made is the usual case — may not, and saying so
 * now is the only time it costs nothing to hear.
 */
static void esp_room_for_b(void)
{
	struct stat k, i;
	struct statvfs vf;

	if (cfg.dry_run ||
	    stat(TARGET "/boot/efi/EFI/kdos/a/vmlinuz", &k) != 0 ||
	    stat(TARGET "/boot/efi/EFI/kdos/a/initramfs.cpio.gz", &i) != 0 ||
	    statvfs(TARGET "/boot/efi", &vf) != 0)
		return;
	unsigned long long need = (unsigned long long)k.st_size +
				  (unsigned long long)i.st_size + (1ULL << 20);
	unsigned long long avail = (unsigned long long)vf.f_bavail * vf.f_frsize;
	logf_("ESP: %llu MiB free, a second kernel needs %llu MiB",
	      avail >> 20, need >> 20);
	if (avail < need)
		emit('W', "the ESP has %llu MiB free and a second root slot's "
			  "kernel needs %llu MiB — A/B updates will be refused",
		     avail >> 20, need >> 20);
}

static void do_boot(void)
{
	char root_uuid[64] = "", esp_uuid[64] = "";
	char crypt_opt[128] = "", slot_opt[128] = "";
	/* The CONTAINER this slot's filesystem lives inside, recorded in the
	 * boot state beside the filesystem. Empty on an unencrypted install.
	 * See the bootstate write below. */
	char luks_uuid[64] = "";
	Part p;

	resolve_parts();
	probe_part(part_root, &p);
	kb_strlcpy(root_uuid, p.uuid, sizeof(root_uuid));
	if (part_esp[0]) {
		Part ep;
		probe_part(part_esp, &ep);
		kb_strlcpy(esp_uuid, ep.uuid, sizeof(esp_uuid));
		if (esp_uuid[0])
			snprintf(slot_opt, sizeof(slot_opt),
				 "bootstate=UUID=%s ", esp_uuid);
	}

	/*
	 * Two UUIDs, and confusing them is the whole trap: `root=` names the
	 * FILESYSTEM inside the container, `cryptdevice=` names the container
	 * itself. The filesystem's UUID does not exist until the container is
	 * open, which is why the initramfs unlocks first and looks second.
	 */
	if (cfg.luks && luks_part[0]) {
		Part lp;
		probe_part(luks_part, &lp);
		if (lp.uuid[0]) {
			snprintf(crypt_opt, sizeof(crypt_opt),
				 "cryptdevice=UUID=%s:%s ", lp.uuid, LUKS_NAME);
			kb_strlcpy(luks_uuid, lp.uuid, sizeof(luks_uuid));
		}
		else if (!cfg.dry_run)
			fail("cannot read the LUKS UUID back from %s",
			     luks_part);
		logf_("LUKS UUID %s", lp.uuid);
	}

	const char *lim = "/usr/share/limine";
	if (!kb_path_exists(lim))
		lim = TARGET "/usr/share/limine";
	if (!kb_path_exists(lim))
		fail("Limine is not installed — no bootloader to place");

	emit('N', "placing Limine on the ESP");
	mkpath(TARGET "/boot/efi/EFI/BOOT");
	mkpath(TARGET "/boot/efi/EFI/kdos");

	char src[320];

	/*
	 * EVERYTHING THE BOOTLOADER READS LIVES ON THE ESP, AND THAT IS WHAT
	 * MAKES ONE DISK BOOT ON BOTH FIRMWARES. The ESP is vfat, which Limine
	 * reads from BIOS and from UEFI alike and which a UEFI firmware can
	 * also read by itself. A kernel on the root filesystem would need a
	 * filesystem driver, and the installer offers roots — xfs, f2fs, LUKS —
	 * that no bootloader reads before the kernel exists.
	 */
	snprintf(src, sizeof(src), "%s/BOOTX64.EFI", lim);
	copy_file(src, TARGET "/boot/efi/EFI/BOOT/BOOTX64.EFI");

	/*
	 * AND THE 32-BIT FIRMWARE'S FIRST STAGE BESIDE IT, so a disk written on
	 * one machine starts on the other. A 64-bit CPU does not imply a 64-bit
	 * firmware — the early Atom tablets are the case — and firmware reads
	 * only the `EFI/BOOT/BOOT<arch>.EFI` it can execute, so the two never
	 * compete. It costs about a hundred kilobytes of an ESP.
	 *
	 * COPIED WHERE IT EXISTS AND SKIPPED WHERE IT DOES NOT: a Limine built
	 * without `--enable-uefi-ia32` installs no such file, and failing the
	 * install over a fallback for firmware this machine does not have would
	 * throw away a working system.
	 */
	snprintf(src, sizeof(src), "%s/BOOTIA32.EFI", lim);
	if (kb_path_exists(src))
		copy_file(src, TARGET "/boot/efi/EFI/BOOT/BOOTIA32.EFI");
	else
		emit('W', "Limine has no BOOTIA32.EFI — this disk will not "
			  "start on 32-bit UEFI firmware");

	/*
	 * limine-bios.sys IS THE BIOS SECOND STAGE and it is found by NAME, not
	 * by configuration: the MBR code written below searches the root,
	 * /boot, /limine and /boot/limine of each volume for exactly this file.
	 * Placed anywhere else the machine gets as far as the boot code and
	 * stops with "limine-bios.sys not found", which is the only message a
	 * BIOS boot has room to give.
	 */
	snprintf(src, sizeof(src), "%s/limine-bios.sys", lim);
	copy_file(src, TARGET "/boot/efi/limine-bios.sys");
	emit('P', "0.4");

	/*
	 * SLOT A'S OWN DIRECTORY, because each root slot boots its own kernel:
	 * the modules for a kernel exist only in the root it was installed
	 * into, so a slot booted on the other slot's kernel has none. An A/B
	 * update fills EFI/kdos/b/ the same way through `kdos-bootctl deploy`,
	 * which is also where the choice of initramfs below is made for every
	 * later kernel — the `linux` postinstall's, where it wrote one, else
	 * the image's own.
	 */
	emit('N', "kernel and initramfs onto the ESP");
	mkpath(TARGET "/boot/efi/EFI/kdos/a");
	copy_file(kb_path_exists(TARGET "/boot/initramfs-kdos.cpio.gz")
			  ? TARGET "/boot/initramfs-kdos.cpio.gz"
			  : TARGET "/boot/initramfs.cpio.gz",
		  TARGET "/boot/efi/EFI/kdos/a/initramfs.cpio.gz");
	copy_file(TARGET "/boot/vmlinuz-kdos",
		  TARGET "/boot/efi/EFI/kdos/a/vmlinuz");
	esp_room_for_b();

	/* The menu's face and wallpaper, so an installed machine looks like the
	 * medium it came from. Both are optional: absent, Limine draws its own
	 * font on a plain backdrop and the entries are unchanged. */
	/*
	 * THE PATH ONLY. The scale, the style and every colour come out of
	 * kcol_limine_conf below, so a machine themed later moves its layout
	 * with its palette rather than keeping whichever arrangement it was
	 * installed under.
	 */
	/*
	 * THE FACE LIVES ON THE MEDIUM AND NOWHERE ELSE. `psf2limine.py` writes
	 * font.bin straight into the ISO tree during packaging, so it is never
	 * inside the root filesystem: /boot/limine/ exists on the ISO9660 volume
	 * the initramfs mounts at /mnt/iso, and a running KDOS has no such
	 * directory. Look in both, in that order, or an installed machine falls
	 * back to Limine's built-in typeface while the stick it came from does
	 * not.
	 */
	const char *fontline = "";
	static const char *const fonts[] = {
		"/boot/limine/font.bin",
		"/mnt/iso/boot/limine/font.bin",
	};
	for (size_t i = 0; i < sizeof(fonts) / sizeof(fonts[0]); i++)
		if (kb_path_exists(fonts[i])) {
			copy_file(fonts[i], TARGET "/boot/efi/EFI/kdos/font.bin");
			fontline = "term_font: boot():/EFI/kdos/font.bin\n"
				   "term_font_size: 8x16\n";
			break;
		}
	/*
	 * The artwork, unlike the face, IS in the root filesystem: the same two
	 * files packaging reads are shipped by the fs/ overlay, so this resolves
	 * without a mounted medium and an install run from a running system gets
	 * the same backdrop as one run from the stick. The order matches
	 * `02_iso.sh` — the generated backdrop, then the banner.
	 *
	 * The file is dimmed IN ITSELF, because Limine has no wallpaper opacity.
	 * How dark it is belongs to the generator that made it, not to this step.
	 */
	const char *paper = "";
	static const char *const papers[] = {
		"/usr/share/kdos/boot/kdos-backdrop.png",
		"/usr/share/kdos/boot/kdos-banner.png",
	};
	for (size_t i = 0; i < sizeof(papers) / sizeof(papers[0]); i++)
		if (kb_path_exists(papers[i])) {
			copy_file(papers[i],
				  TARGET "/boot/efi/EFI/kdos/wallpaper.png");
			paper = "wallpaper: boot():/EFI/kdos/wallpaper.png\n";
			break;
		}

	/*
	 * THE COLOURS ARE THE INSTALLED SYSTEM'S ACCENT, out of libkcolor, and
	 * they are emitted by the same function the medium's own menu was
	 * written with. Two hand-copied sets of nine literals is how a stick
	 * and the machine installed from it end up different colours.
	 */
	char theme[1024];
	const KcolScheme *boot_sc = kcol_find(cfg.theme);
	if (kcol_limine_conf(boot_sc, theme, sizeof(theme)) >= (int)sizeof(theme))
		kb_die("limine theme block does not fit");

	/* memtest86+ is a payload rather than a program: bad RAM is the one
	 * fault no tool running under an OS can honestly diagnose, because the
	 * OS is in the memory being tested. It has to be BOOTABLE from the
	 * installed machine, not just from the medium — the fault it finds is
	 * usually reported as "this install is unstable" months later. Absent
	 * is a skipped menu entry and not an error; the payload is an EFI
	 * binary, so `if_fw_type` keeps it off a BIOS menu that could not start
	 * it. */
	const char *memtest = "";
	if (kb_path_exists("/usr/share/kdos/memtest86plus/memtest.efi")) {
		copy_file("/usr/share/kdos/memtest86plus/memtest.efi",
			  TARGET "/boot/efi/EFI/kdos/memtest.efi");
		memtest = "\n/Memory Test (memtest86+)\n"
			  "    comment: Test this machine's RAM — UEFI only\n"
			  "    protocol: efi\n"
			  "    if_fw_type: UEFI\n"
			  "    path: boot():/EFI/kdos/memtest.efi\n";
	}
	emit('P', "0.7");

	/*
	 * ONE CONFIG AT THE ROOT OF THE ESP, FOUND BY BOTH FIRMWARES. Limine
	 * looks beside its own EFI binary first and then at /boot/limine/,
	 * /boot/, /limine/ and / on each volume; only that last set is searched
	 * on BIOS. The root of the ESP is therefore the one path both find, and
	 * a second copy beside BOOTX64.EFI would be the copy that goes stale.
	 *
	 * TEN SECONDS IS A COUNTDOWN SOMEBODY CAN ACT ON, and it matches the
	 * medium's. Every second of it is boot time spent before the kernel
	 * exists, with nothing else running — but the menu is the only way to
	 * reach the verbose and single-user entries and memtest86+, and a
	 * machine that will not boot needs one of those. A countdown short
	 * enough to miss makes them unreachable on exactly the machine that
	 * needs them. Any keypress cancels it and leaves the menu up.
	 *
	 * `timeout: 0` does not mean "boot at once with a menu"; it boots the
	 * default entry without drawing one, and those entries go with it.
	 */
	wr("/boot/efi/limine.conf",
	   "# Written by the KDOS installer.\n"
	   "timeout: 10\n"
	   "default_entry: 1\n"
	   "\n"
	   "%s"
	   "%s%s"
	   "\n"
	   "/KDOS\n"
	   "    comment: Start this machine (slot a)\n"
	   "    protocol: linux\n"
	   "    path: boot():/EFI/kdos/a/vmlinuz\n"
	   "    module_path: boot():/EFI/kdos/a/initramfs.cpio.gz\n"
	   "    cmdline: kdos_slot=a %s%sroot=UUID=%s rw console=tty0 quiet loglevel=3\n"
	   "\n"
	   "/KDOS (verbose)\n"
	   "    comment: Every kernel message on the console\n"
	   "    protocol: linux\n"
	   "    path: boot():/EFI/kdos/a/vmlinuz\n"
	   "    module_path: boot():/EFI/kdos/a/initramfs.cpio.gz\n"
	   "    cmdline: kdos_slot=a %s%sroot=UUID=%s rw console=tty0 loglevel=7\n"
	   "\n"
	   "/KDOS (single user)\n"
	   "    comment: A root shell, no session\n"
	   "    protocol: linux\n"
	   "    path: boot():/EFI/kdos/a/vmlinuz\n"
	   "    module_path: boot():/EFI/kdos/a/initramfs.cpio.gz\n"
	   "    cmdline: kdos_slot=a %s%sroot=UUID=%s rw console=tty0 loglevel=7 single\n"
	   "%s",
	   theme, paper, fontline,
	   slot_opt, crypt_opt, root_uuid,
	   slot_opt, crypt_opt, root_uuid,
	   slot_opt, crypt_opt, root_uuid, memtest);

	/*
	 * The initial boot state, so the machine starts life as slot A with
	 * nothing to roll back to. Slot B is described later with
	 * `kdos-bootctl set-slot b`; `kdos update` installs into it, puts its
	 * kernel in EFI/kdos/b/ with `kdos-bootctl deploy` and calls
	 * `kdos-bootctl try b`, and kdos-bootctl rewrites the /KDOS entries
	 * above for every change of state. The entries written here are the
	 * shape it regenerates, and the command line it carries over.
	 *
	 * Written straight rather than through kdos-bootctl: this runs from the
	 * live image against a target at /mnt, and the tool's default path is
	 * the RUNNING system's ESP. One file, and the format is in bootctl.c.
	 *
	 * `crypt_a` IS WHAT JOINS THE TWO MECHANISMS. `slot_a` is the
	 * FILESYSTEM and `crypt_a` is the container it is inside; the command
	 * line can name only one `cryptdevice=`, so a second slot inside a
	 * second container is only reachable because each slot records its
	 * own and the initramfs asks after it has chosen. Empty here on an
	 * unencrypted install, which is the fallback to the command line.
	 */
	if (esp_uuid[0]) {
		mkpath(TARGET "/boot/efi/EFI/kdos");
		wr("/boot/efi/EFI/kdos/bootstate",
		   "# KDOS boot state. Written by the installer; maintained by\n"
		   "# kdos-bootctl.\n"
		   "slot_a   = %s\n"
		   "slot_b   = \n"
		   "crypt_a  = %s\n"
		   "crypt_b  = \n"
		   "active   = a\n"
		   "try      = \n"
		   "attempts = 0\n",
		   root_uuid, luks_uuid);
	}

	/*
	 * THE BIOS BOOT CODE IS WRITTEN WHATEVER THIS MACHINE BOOTED AS, and
	 * that is deliberate: it costs one sector and it makes the installed
	 * disk start on firmware that is not the firmware it was installed
	 * from. A disk imaged on a UEFI machine and moved to a legacy one is
	 * the case that would otherwise install perfectly and never boot.
	 *
	 * Failure is reported and not fatal. On a UEFI machine the EFI path
	 * above is already complete and refusing the install here would throw
	 * away a working system over a legacy fallback.
	 */
	if (kb_have_prog("limine")) {
		emit('N', "BIOS boot code onto %s", cfg.disk);
		char disk[64];
		kb_strlcpy(disk, cfg.disk, sizeof(disk));
		char *bi[] = { "limine", "bios-install", disk, NULL };
		if (run(bi) != 0)
			emit('W', "limine bios-install failed — this disk will "
				  "boot on UEFI but not on legacy BIOS");
	} else {
		emit('W', "limine not on PATH — no BIOS boot code written");
	}

	if (kb_have_prog("efibootmgr") && ki_sys.uefi) {
		char disk[64];
		/*
		 * THE ENTRY NAMES ONE PATH, so it has to be the one THIS
		 * firmware can execute — an NVRAM entry pointing at a binary
		 * the firmware cannot load is a boot option that fails rather
		 * than falls through. Both files are on the ESP above; only
		 * the removable-media fallback picks between them by itself.
		 *
		 * 0 bits is a kernel that did not publish the width, and is
		 * read as 64: that is nearly every machine, and being wrong
		 * leaves a 32-bit firmware booting through the fallback path
		 * instead of through its own entry.
		 */
		const char *loader = ki_sys.fw_bits == 32
					     ? "\\EFI\\BOOT\\BOOTIA32.EFI"
					     : "\\EFI\\BOOT\\BOOTX64.EFI";

		/*
		 * THE ENTRY NAMES THE PARTITION THE ESP IS ACTUALLY ON, and
		 * that is measured rather than assumed: only the wipe plan
		 * lays the ESP out at index 1, and a reuse install takes
		 * whichever partition the user picked. An entry naming any
		 * other partition is a boot option the firmware cannot load.
		 * Without a measured index no entry is written at all — the
		 * removable-media fallback still starts this disk, and a
		 * guessed index would take that chance away.
		 */
		int esp_part = part_index(part_esp);
		/* A dry run has no partition table to measure; the command is
		 * logged and never executed. */
		if (esp_part < 0 && cfg.dry_run)
			esp_part = 1;

		char pn[8];
		int w = esp_part < 0 ? -1 : snprintf(pn, sizeof(pn), "%d",
						     esp_part);
		if (w < 0 || (size_t)w >= sizeof(pn)) {
			emit('W', "no partition index for %s — no NVRAM boot "
				  "entry; this disk boots through the "
				  "removable-media fallback",
			     part_esp);
		} else {
			kb_strlcpy(disk, cfg.disk, sizeof(disk));
			char *eb[] = { "efibootmgr", "--create", "--disk", disk,
				       "--part", pn, "--loader", (char *)loader,
				       "--label", "KDOS", NULL };
			try_(eb);
		}
	}
	emit('P', "1");
}

static void do_finish(void)
{
	emit('N', "flushing");
	if (!cfg.dry_run)
		sync();
	/* The medium's bind is OUTSIDE the target on purpose, so
	 * `unmount_below(TARGET)` cannot reach it and it would keep the medium
	 * busy for whatever unmounts it next. */
	char *um[] = { "umount", (char *)MEDIUM_BIND, NULL };
	if (kb_path_exists(MEDIUM_BIND))
		try_(um);
	unmount_below(TARGET "/");
	char *u[] = { "umount", TARGET, NULL };
	try_(u);
	emit('P', "1");
}

/* ══════════════════════════════════════════════════════════════════════ */

int install_child_main(int fd, int from_step)
{
	wfd = fd;

	/* Nothing but the protocol may reach the pipe, and nothing at all may
	 * reach the terminal — the parent still owns it in raw mode. */
	int null = open("/dev/null", O_RDWR);
	if (null >= 0) {
		dup2(null, 0);
		dup2(null, 1);
		dup2(null, 2);
		if (null > 2)
			close(null);
	}

	static void (*const fns[S_COUNT])(void) = {
		do_prepare, do_partition, do_format, do_mount, do_copy,
		do_apps, do_config, do_accounts, do_theme, do_boot, do_finish,
	};

	if (cfg.dry_run)
		logf_("DRY RUN — nothing is written to any disk");

	for (int i = from_step; i < S_COUNT; i++) {
		if (step_skipped(i)) {
			emit('K', "%d", i);
			continue;
		}
		emit('S', "%d", i);
		emit('P', "-1");
		fns[i]();
	}

	emit('D', "%s", "");
	return 0;
}
