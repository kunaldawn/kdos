/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   kinstall-core — the part that writes
 *
 * The work runs in a forked child that speaks one line per event back over a
 * pipe. That keeps the install a plain top-to-bottom program — which is what
 * it is — while the parent stays a single-threaded loop that never blocks on
 * a 4 GB rsync: the terminal wizard's draw loop, or the events stream that the
 * graphical installer reads (events.c). The protocol is deliberately tiny:
 *
 *   S<n>  begin step n        K<n>  skip step n
 *   P<f>  progress 0..1, or -1 for indeterminate
 *   N<s>  one-line note for the current step
 *   L<s>  log line               W<s>  warning: the install carries on
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

#include "kinstall-core.h"

Install inst;

/*
 * THE TARGET IS A DIRECTORY OF ITS OWN, never `/mnt` itself. The live
 * session's boot medium is mounted at /mnt/iso and a stick carrying an
 * exported set at /mnt/<name>; a root mounted over /mnt would hide both —
 * the medium's font.bin and the archive the Packs step imports — and the
 * Prepare step's sweep of mounts under the target would unmount them.
 */
#define TARGET "/mnt/kinstall"
/* Slot B's root while it is filled: beside the target and never under it, so
 * the copy of slot A into it cannot meet itself. */
#define TARGET_B "/mnt/kinstall-b"

/* THE WORDS A QUIET BOOT ADDS, on each entry that boots to the desktop: no
 * kernel messages and no console cursor between the boot menu and the login
 * screen, so the splash hands a black screen to the greeter. The verbose and
 * single-user entries leave both out; kdos-bootctl regenerates every entry
 * with the same set. */
#define KI_QUIET "quiet loglevel=3 vt.global_cursor_default=0"

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
	S_SLOTB,
	S_BOOT,
	S_FINISH,
	S_COUNT
};

static const struct {
	const char *title;
	const char *detail;
} steps[S_COUNT] = {
	{ "Prepare",     "unmount the target, stop swap" },
	{ "Partition",   "write the GPT layout: ESP, BIOS boot, root" },
	{ "Format",      "encrypt and make the volume group if asked, then the filesystems" },
	{ "Mount",       "attach the target at " TARGET },
	{ "Copy system", "the live system, without the live session" },
	{ "Packs",       "the applications chosen from the medium" },
	{ "Configure",   "fstab, hostname, keyboard, login screen, services" },
	{ "Accounts",    "users, passwords, sudo, the home" },
	{ "Theme",       "the accent and the desk the first login takes" },
	{ "Slot B",      "the second root, filled for A/B updates" },
	{ "Bootloader",  "Limine on the ESP, the BIOS boot code, the UEFI boot entry" },
	{ "Finish",      "flush and unmount" },
};

/* ════════════════════════════════════════════════════════════════════════
 * Parent side
 * ════════════════════════════════════════════════════════════════════════ */

static int step_skipped(int i)
{
	if (i == S_PARTITION && cfg.plan != PLAN_WIPE)
		return 1;
	if (i == S_SLOTB && !ki_ab())
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
	inst.nwarn = 0;
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

/*
 * THE INSTALL IS SHIELDED FROM kdos-oomd, the wizard and every process it
 * starts alike: the value is inherited across fork and exec, so the copy's
 * rsync, cryptsetup's key derivation and mkfs carry it too. The copy streams
 * the whole live tree through the page cache, and on a 4 GB machine that
 * stalls memory long enough for the daemon's pressure trigger; it picks the
 * largest process it does not protect, which is rsync, and a killed copy is
 * an install that stops half written. kdos-oomd passes over any process at
 * or below -500. -900 stays above OOM_SCORE_ADJ_MIN, so the kernel's own OOM
 * killer can still end the install when memory is truly gone.
 *
 * Lowering the value needs CAP_SYS_RESOURCE, which root has; without it the
 * write fails, and the Prepare step then refuses the install for not being
 * root.
 */
#define KI_OOM_SCORE_ADJ "-900"

static void shield_from_oomd(void)
{
	int fd = open("/proc/self/oom_score_adj", O_WRONLY | O_CLOEXEC);

	if (fd < 0)
		return;
	if (write(fd, KI_OOM_SCORE_ADJ, sizeof(KI_OOM_SCORE_ADJ) - 1) < 0)
		install_log("could not lower oom_score_adj; kdos-oomd may "
			    "end the install under memory pressure");
	close(fd);
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
	if (!cfg.dry_run)
		shield_from_oomd();

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

void (*ki_line_hook)(const char *line);

const char *ki_state_name(int st)
{
	switch (st) {
	case ST_PENDING: return "pending";
	case ST_RUNNING: return "running";
	case ST_DONE:    return "done";
	case ST_FAIL:    return "failed";
	case ST_SKIP:    return "skipped";
	}
	return "?";
}

static void handle_line(char *s)
{
	char c = s[0];

	if (ki_line_hook)
		ki_line_hook(s);
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
	case 'W': {
		/* A retried step sends its warning again; it is one warning. */
		int seen = 0;
		char line[LOG_COLS];

		for (int i = 0; i < inst.nwarn; i++)
			if (!strcmp(inst.warn[i], arg))
				seen = 1;
		if (!seen && inst.nwarn < (int)(sizeof(inst.warn) /
						sizeof(inst.warn[0])))
			kb_strlcpy(inst.warn[inst.nwarn++], arg,
				   sizeof(inst.warn[0]));
		snprintf(line, sizeof(line), "warning: %s", arg);
		install_log(line);
		break;
	}
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
/* The A/B layout's other volumes, empty on any other: slot B's root, the
 * shared home, and the swap volume. */
static char part_b[192], part_home[192], part_swaplv[192];

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

/* A BIOS boot path that could not be written: a failure on a machine that
 * booted from legacy BIOS, which has no other way to start the disk, and a
 * warning on UEFI, where the EFI path is complete. */
static void bios_problem(const char *fmt, ...)
{
	char buf[512];
	va_list ap;
	va_start(ap, fmt);
	vsnprintf(buf, sizeof(buf), fmt, ap);
	va_end(ap);
	if (!ki_sys.uefi)
		fail("%s", buf);
	emit('W', "%s", buf);
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

/*
 * EVERY SWAP AREA BUT ZRAM. A swap partition or file on the disk holds it
 * busy through partitioning and formatting. The compressed swap in RAM
 * holds no disk; turned off, its pages have to come back into a live
 * system's memory and the copy then runs with no swap at all.
 */
static void swapoff_disks(void)
{
	char buf[8192];
	if (slurp("/proc/swaps", buf, sizeof(buf)) < 0)
		return;
	char *save = NULL;
	/* The first line is the column header. */
	char *l = strtok_r(buf, "\n", &save);
	while (l && (l = strtok_r(NULL, "\n", &save))) {
		char dev[192];
		if (sscanf(l, "%191s", dev) != 1 ||
		    !strncmp(dev, "/dev/zram", 9))
			continue;
		char *a[] = { "swapoff", dev, NULL };
		logf_("turning off swap on %s", dev);
		try_(a);
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
 * and the installed system logs nobody in or starts no box. SDDM's
 * `[Autologin] User=` names the same account and is NOT touched here:
 * ki_write_login is its single writer, runs before this step and already
 * writes cfg.username.
 *
 * `admin` is the whole of the Administrator choice. The sudo port's
 * `%wheel ALL=(ALL) ALL`, the polkit admin rules and every root daemon's
 * configuration verbs grant on membership of `wheel`, and the live image ships
 * the account in it — so a non-administrator is taken OUT of `wheel` here, or
 * unticking the box would leave full root through sudo.
 *
 * ONLY `wheel` IS FILTERED. Every other membership is kept and renamed:
 * the devices and the power actions a desktop needs are granted to the active
 * login session by elogind and polkit, not by a group, so what the image put
 * the live account in is what the person's account keeps. */
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

/*
 * ONE ACCOUNT TAKEN OUT OF EVERY DATABASE, for the OEM install: its passwd and
 * shadow lines, its own group, its memberships of every other group and its
 * subordinate-ID ranges. plasma-setup makes the first account on the first
 * boot; a live account left behind would be a second login with the image's
 * published password, and plasma-setup skips its account page on a machine
 * that already has a user.
 */
static void drop_from(const char *file, const char *user, int members)
{
	char buf[65536], out[65536], path[256];
	size_t ul = strlen(user);

	snprintf(path, sizeof(path), "%s%s", TARGET, file);
	if (slurp(path, buf, sizeof(buf)) <= 0)
		return;
	out[0] = 0;
	char *save = NULL;
	for (char *l = strtok_r(buf, "\n", &save); l;
	     l = strtok_r(NULL, "\n", &save)) {
		if (!strncmp(l, user, ul) && l[ul] == ':')
			continue;
		if (members) {
			/* name:pw:gid:a,b,c — the list rebuilt without it. */
			char *c3 = l;
			for (int i = 0; i < 3 && c3; i++) {
				c3 = strchr(c3, ':');
				if (c3)
					c3++;
			}
			if (c3) {
				char mem[1024], kept[1024] = "";
				char *ms = NULL;

				kb_strlcpy(mem, c3, sizeof(mem));
				for (char *m = strtok_r(mem, ",", &ms); m;
				     m = strtok_r(NULL, ",", &ms)) {
					if (!strcmp(m, user))
						continue;
					if (kept[0])
						cat(kept, sizeof(kept), ",");
					cat(kept, sizeof(kept), m);
				}
				*c3 = 0;
				cat(out, sizeof(out), l);
				cat(out, sizeof(out), kept);
				cat(out, sizeof(out), "\n");
				continue;
			}
		}
		cat(out, sizeof(out), l);
		cat(out, sizeof(out), "\n");
	}
	if (!cfg.dry_run && kb_write_file(path, out) < 0)
		fail("cannot write %s", path);
	logf_("removed %s from %s", user, path);
}

static void drop_account(const char *user)
{
	char home[256];

	drop_from("/etc/passwd", user, 0);
	drop_from("/etc/shadow", user, 0);
	if (!cfg.dry_run)
		chmod(TARGET "/etc/shadow", 0600);
	drop_from("/etc/group", user, 1);
	drop_from("/etc/subuid", user, 0);
	drop_from("/etc/subgid", user, 0);
	snprintf(home, sizeof(home), "%s/home/%s", TARGET, user);
	if (kb_path_exists(home)) {
		char *rm[] = { "rm", "-rf", home, NULL };
		try_(rm);
	}
}

/* ══════════════════════════════════════════════════════════════════════ */

static void do_prepare(void)
{
	char why[256];

	emit('N', "checking the target");

	if (!cfg.disk[0])
		fail("no target disk selected");
	/* The front end's own checks ran or did not; these are the rules every
	 * front end shares, asked again before anything is written. */
	if (ki_answers_check(why, sizeof(why)))
		fail("%s", why);
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
	/* The Layout page refuses this too; an unattended install never
	 * shows it, and this is before anything is written. */
	if (!ki_sys.uefi && ki_bios_boot_part() < 0)
		fail("this machine starts in legacy BIOS mode and %s has no "
		     "BIOS boot partition — the installed system could not "
		     "start here", cfg.disk);
	if (!ki_sys.uefi && !kb_have_prog("limine"))
		fail("required tool missing: limine — this machine starts in "
		     "legacy BIOS mode and needs its BIOS boot code");

	swapoff_disks();
	unmount_below(TARGET "/");
	unmount_below(TARGET_B "/");
	char *um[] = { "umount", "-R", TARGET, NULL };
	try_(um);
	char *umb[] = { "umount", "-R", TARGET_B, NULL };
	try_(umb);
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

	emit('N', "wiping %s", cfg.disk);
	char *wipe[] = { "wipefs", "-a", cfg.disk, NULL };
	must(wipe);

	/*
	 * ONE LINE PER PARTITION, numbered in order, so the order here IS
	 * KI_PART_ESP, KI_PART_BIOS_BOOT, ki_wipe_part_swap() and
	 * ki_wipe_part_root(). sfdisk has no shortcut for the BIOS boot type
	 * and takes its GUID. `V` is sfdisk's name for the Linux LVM type,
	 * which is what a physical volume's partition is marked as.
	 */
	char rtype = cfg.lvm ? 'V' : 'L';
	if (ki_wipe_part_swap())
		snprintf(layout, sizeof(layout),
			 "label: gpt\n,%dM,U\n,%dM,%s\n,%ldM,S\n,,%c\n",
			 KI_ESP_MB, KI_BIOS_BOOT_MB, KI_GUID_BIOS_BOOT,
			 cfg.swap_mb, rtype);
	else
		snprintf(layout, sizeof(layout),
			 "label: gpt\n,%dM,U\n,%dM,%s\n,,%c\n", KI_ESP_MB,
			 KI_BIOS_BOOT_MB, KI_GUID_BIOS_BOOT, rtype);

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

	/* udev can be a beat behind sfdisk; the node has to exist before mkfs.
	 * The root is the last partition, so its node is the last to appear. */
	for (int i = 0; i < 50; i++) {
		char p2[96];	/* same width as part_esp/part_root */
		ki_part_name(cfg.disk, ki_wipe_part_root(), p2, sizeof(p2));
		if (kb_path_exists(p2) || cfg.dry_run)
			break;
		struct timespec ts = { 0, 100000000 };
		nanosleep(&ts, NULL);
	}

	/*
	 * THE BIOS BOOT PARTITION STARTS ZEROED. It is raw space, at an offset
	 * an earlier layout of this disk may have given a filesystem, and
	 * `limine bios-install` refuses a partition whose first sector still
	 * reads as FAT or NTFS.
	 */
	char bb[96];
	ki_part_name(cfg.disk, KI_PART_BIOS_BOOT, bb, sizeof(bb));
	logf_("zeroing %s", bb);
	if (!cfg.dry_run) {
		static const char zero[65536];
		int fd = open(bb, O_WRONLY | O_CLOEXEC);
		size_t left = (size_t)KI_BIOS_BOOT_MB << 20;

		if (fd < 0)
			fail("cannot open %s: %s", bb, strerror(errno));
		while (left) {
			ssize_t w = write(fd, zero, left < sizeof(zero)
							     ? left
							     : sizeof(zero));
			if (w <= 0) {
				close(fd);
				fail("cannot zero %s: %s", bb, strerror(errno));
			}
			left -= (size_t)w;
		}
		if (fsync(fd) < 0 || close(fd) < 0)
			fail("cannot zero %s: %s", bb, strerror(errno));
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
		ki_part_name(cfg.disk, KI_PART_ESP, part_esp, sizeof(part_esp));
		if (ki_wipe_part_swap())
			ki_part_name(cfg.disk, ki_wipe_part_swap(), part_swap,
				     sizeof(part_swap));
		ki_part_name(cfg.disk, ki_wipe_part_root(), part_root,
			     sizeof(part_root));
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
	/* The A/B layout's three other volumes, in the same group. */
	if (ki_ab()) {
		snprintf(part_b, sizeof(part_b), "/dev/%s/%s", KI_VG, KI_LV_B);
		snprintf(part_home, sizeof(part_home), "/dev/%s/%s", KI_VG,
			 KI_LV_HOME);
		if (ki_swap_lv_mb())
			snprintf(part_swaplv, sizeof(part_swaplv), "/dev/%s/%s",
				 KI_VG, KI_LV_SWAP);
	}

	logf_("ESP  = %s", part_esp);
	logf_("root = %s", part_root);
	if (luks_part[0])
		logf_("LUKS = %s", luks_part);
	if (pv_dev[0])
		logf_("PV   = %s, group %s", pv_dev, KI_VG);
	if (part_swap[0])
		logf_("swap = %s", part_swap);
	if (part_b[0])
		logf_("B    = %s", part_b);
	if (part_home[0])
		logf_("home = %s", part_home);
	if (part_swaplv[0])
		logf_("swap = %s", part_swaplv);
}

/*
 * THE GROUP AND ITS VOLUMES, on a physical volume that was a partition a
 * moment ago. Every call goes through `lvm <command>` rather than the
 * pvcreate/vgcreate names, which are symlinks an image may not carry.
 *
 * On the A/B layout the slots are made first and at a fixed size, then the
 * swap, and the home takes whatever is left: a home sized by subtraction
 * could come out smaller than the floor ki_ab() promised if LVM rounded the
 * others up. Anywhere else slot A takes the whole group.
 *
 * THE OLD SIGNATURES GO FIRST. The erase plan writes the same layout at the
 * same offsets, so a disk that held an earlier install still has that
 * install's PV label where the new partition starts: pvcreate refuses it, and
 * vgcreate refuses the group name it still carries. A container has just been
 * formatted over its partition and has none.
 */
static void lv_create(const char *name, const char *how, const char *size)
{
	char *lv[] = { "lvm", "lvcreate", "-y", "-W", "y", "-n", (char *)name,
		       (char *)how, (char *)size, (char *)KI_VG, NULL };
	char dev[192];

	emit('N', "%s/%s (%s %s)", KI_VG, name, how, size);
	must(lv);
	snprintf(dev, sizeof(dev), "/dev/%s/%s", KI_VG, name);
	for (int i = 0; i < 50 && !cfg.dry_run && !kb_path_exists(dev); i++) {
		struct timespec ts = { 0, 100000000 };
		nanosleep(&ts, NULL);
	}
	if (!cfg.dry_run && !kb_path_exists(dev))
		fail("%s did not appear after lvcreate", dev);
}

static void make_volume_group(void)
{
	char *pv[] = { "lvm", "pvcreate", "-y", pv_dev, NULL };
	char *vg[] = { "lvm", "vgcreate", "-y", (char *)KI_VG, pv_dev, NULL };
	char *us[] = { "udevadm", "settle", NULL };

	if (!cfg.luks) {
		char *wf[] = { "wipefs", "-a", pv_dev, NULL };
		must(wf);
	}
	emit('N', "volume group %s on %s", KI_VG, pv_dev);
	must(pv);
	must(vg);
	if (ki_ab()) {
		char slot[32], swp[32];

		snprintf(slot, sizeof(slot), "%llum", ki_slot_mb());
		lv_create(KI_LV_A, "-L", slot);
		lv_create(KI_LV_B, "-L", slot);
		if (ki_swap_lv_mb()) {
			snprintf(swp, sizeof(swp), "%llum", ki_swap_lv_mb());
			lv_create(KI_LV_SWAP, "-L", swp);
		}
		lv_create(KI_LV_HOME, "-l", "100%FREE");
	} else {
		lv_create(KI_LV_A, "-l", "100%FREE");
	}
	try_(us);
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

	/* The A/B layout's other filesystems, made here with the first so every
	 * UUID the boot state and fstab name exists before either is written. */
	if (part_b[0]) {
		emit('N', "mkfs %s on %s", cfg.fstype, part_b);
		char *mb[] = { (char *)fs->mkfs, (char *)fs->force, "-L",
			       "KDOS_B", part_b, NULL };
		must(mb);
	}
	if (part_home[0]) {
		emit('N', "mkfs %s on %s", cfg.fstype, part_home);
		char *mh[] = { (char *)fs->mkfs, (char *)fs->force, "-L",
			       "KDOS_HOME", part_home, NULL };
		must(mh);
	}
	if (part_swaplv[0]) {
		emit('N', "mkswap %s", part_swaplv);
		char *ms[] = { "mkswap", "-L", "KDOS_SWAP", part_swaplv, NULL };
		must(ms);
	}
	emit('P', "1");
}

static void do_mount(void)
{
	resolve_parts();
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
	/* The shared home under the target before the copy, so the box store
	 * the copy carries lands on the volume both slots mount. */
	if (part_home[0]) {
		mkpath(TARGET "/home");
		emit('N', "mounting %s at %s/home", part_home, TARGET);
		char *mh[] = { "mount", part_home, TARGET "/home", NULL };
		must(mh);
		/* And the pack store, bound from it the way fstab binds it
		 * at every boot, so the packs the copy carries land there. */
		mkpath(TARGET KI_SHARED_PACKS);
		mkpath(TARGET "/var/lib/kdos/packs");
		emit('N', "binding %s%s at %s/var/lib/kdos/packs", TARGET,
		     KI_SHARED_PACKS, TARGET);
		char *mb[] = { "mount", "--bind", TARGET KI_SHARED_PACKS,
			       TARGET "/var/lib/kdos/packs", NULL };
		must(mb);
	}
	emit('P', "1");
}

static void do_copy(void)
{
	char *argv[48];
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
	/*
	 * THE LIVE SESSION'S RECORD STAYS ON THE MEDIUM. Every file under
	 * /var/log and /var/cache was written by the live boot — the image
	 * ships those trees as directories alone — so copied, the installed
	 * system's syslog and `last` open with a session it never ran, and
	 * this run's own kinstall.log among them. The directories are kept:
	 * packages own them, and `kpkg check` reports a missing one. Rules
	 * match in order, so the directory include comes before the exclude.
	 */
	argv[n++] = "--include=/var/log/**/";
	argv[n++] = "--exclude=/var/log/**";
	argv[n++] = "--include=/var/cache/**/";
	argv[n++] = "--exclude=/var/cache/**";
	/*
	 * NOR ROOT'S HISTORY. Shell and atuin history, and root's caches, are
	 * what was typed and run before the install — a password passed on a
	 * command line among it.
	 */
	argv[n++] = "--exclude=/root/.bash_history";
	argv[n++] = "--exclude=/root/.cache/***";
	argv[n++] = "--exclude=/root/.local/share/atuin/***";
	/*
	 * NOR THE LIVE SESSION. The live account's home is the desktop that ran
	 * this install: its panel layout, its wallet, its file index, the
	 * welcome tour marked as seen, its recent files and history. Carried
	 * over, the installed account's first login would find a layout already
	 * laid out — the desk chosen here never applied — and the tour already
	 * shown. Two things the image built into it are kept: the box store,
	 * when asked for, and bat's syntax cache. The Accounts step fills the
	 * rest of the home from /etc/skel, as a new account's is.
	 *
	 * Rules match in order and a directory must be included for anything
	 * under it to be reached, so each kept tree's parents come first.
	 */
	argv[n++] = "--include=/home/kdos/";
	argv[n++] = "--include=/home/kdos/.cache/";
	argv[n++] = "--include=/home/kdos/.cache/bat/***";
	if (cfg.with_appbox && !cfg.oem) {
		argv[n++] = "--include=/home/kdos/.local/";
		argv[n++] = "--include=/home/kdos/.local/share/";
		argv[n++] = "--include=/home/kdos/.local/share/containers/***";
	}
	argv[n++] = "--exclude=/home/kdos/**";
	/*
	 * NOR WHAT THE LIVE BOOT STEP ADDED. The "Install KDOS" entry and the
	 * polkit rule that lets the live session start an install without a
	 * password are written onto the medium's overlay at every live boot
	 * (63_live-installer.sh); on an installed system the entry would offer
	 * to rerun the installer over the disk the machine boots from, and the
	 * rule would let any session do it unasked.
	 */
	argv[n++] = "--exclude=/usr/share/applications/kdos-installer.desktop";
	argv[n++] = "--exclude=/etc/polkit-1/rules.d/49-kdos-installer-live.rules";
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
	/*
	 * NOR ARE THE LIVE ACCOUNTS' BACKUP FILES. shadow's tools write
	 * passwd-, group-, shadow- and gshadow- beside each file they change,
	 * and a password set in the live session lands in them; the installer
	 * writes the accounts themselves, never these copies.
	 */
	argv[n++] = "--exclude=/etc/passwd-";
	argv[n++] = "--exclude=/etc/group-";
	argv[n++] = "--exclude=/etc/shadow-";
	argv[n++] = "--exclude=/etc/gshadow-";
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
 *   import   an exported set on a mounted device, run through the live
 *            session's `kdos-appbox import` with no target root: the packs
 *            land in the live store and end with it, and the target gets
 *            only the empty store below. Offline.
 *   network  `kdos-appbox install` in the live session, with no target root:
 *            what it builds lands in the live store, as with import.
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
	char home_uuid[64] = "";
	probe_part(part_root, &p);
	kb_strlcpy(root_uuid, p.uuid, sizeof(root_uuid));
	if (part_esp[0]) {
		probe_part(part_esp, &p);
		kb_strlcpy(esp_uuid, p.uuid, sizeof(esp_uuid));
	}
	if (part_swap[0] || part_swaplv[0]) {
		probe_part(part_swap[0] ? part_swap : part_swaplv, &p);
		kb_strlcpy(swap_uuid, p.uuid, sizeof(swap_uuid));
	}
	if (part_home[0]) {
		probe_part(part_home, &p);
		kb_strlcpy(home_uuid, p.uuid, sizeof(home_uuid));
		if (!home_uuid[0] && !cfg.dry_run)
			fail("cannot read a UUID back from %s", part_home);
	}
	if (!root_uuid[0] && !cfg.dry_run)
		fail("cannot read a UUID back from %s", part_root);
	logf_("root UUID %s", root_uuid);

	/* fstab is never replaced: the installer's root, ESP, home and swap
	 * lines go first and the shipped file follows them. The shipped file
	 * carries the proc/sys/dev lines and the tmpfs /tmp with mode=1777 —
	 * dropping those locks every non-root user out of /tmp, and a program
	 * that needs a temporary file there fails. Slot B gets this same file
	 * with its own root's UUID on the first line; every other line names
	 * a volume both slots share. */
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
	if (home_uuid[0]) {
		char l[256];
		snprintf(l, sizeof(l), "UUID=%s\t/home\t%s\t%s\t0 %d\n",
			 home_uuid, rfs->name, rfs->opts, rfs->passno ? 2 : 0);
		cat(fst, sizeof(fst), l);
		cat(fst, sizeof(fst), KI_SHARED_PACKS "\t/var/lib/kdos/packs\t"
				      "none\tbind\t0 0\n");
	}
	if (swap_uuid[0]) {
		char l[256];
		snprintf(l, sizeof(l), "UUID=%s\tnone\tswap\tdefaults\t0 0\n",
			 swap_uuid);
		cat(fst, sizeof(fst), l);
	}
	if (cfg.swap == SWAP_FILE && cfg.swap_mb > 0 && !ki_ab())
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
	 * drift from it. kdos-settingsd, serving timedate1's SetTimezone, writes
	 * exactly these two afterwards.
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
			/* The check reads the live root's zoneinfo tree, the
			 * one the target is installed from. A zone the picker
			 * offered and that tree does not carry leaves `TZ`
			 * pointing at a symlink that is not there, and musl
			 * answers UTC with no error. Said here, where the log
			 * is read. */
			logf_("no zone file for %s; the machine will keep UTC",
			      cfg.tz_label);
		}
	}

	/*
	 * THE WI-FI COUNTRY, FROM THE ZONE. cfg80211 starts in the world
	 * regulatory domain, which keeps every 5 GHz DFS channel closed and caps
	 * transmit power, until something names a country. `zone.tab` gives
	 * each zone exactly one, so the zone just chosen is the answer; a zone
	 * with no row (UTC) writes nothing. kdos-settingsd rewrites the same
	 * file when the zone is changed through timedate1.
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

	/* The keyboard and the login screen: the console keymap, the session's
	 * layout, SDDM's [Autologin] and the first-boot wizard's marker. */
	{
		char why[256];

		emit('N', "keyboard and login screen");
		if (ki_write_keyboard(TARGET, why, sizeof(why)) ||
		    ki_write_login(TARGET, why, sizeof(why)))
			fail("%s", why);
	}

	/*
	 * THE SERIAL CONSOLE ASKS FOR A LOGIN. The image's `S0` line in
	 * /etc/inittab logs root in on ttyS0 with no password, which is what a
	 * live medium is for and what no installed machine may keep: anything
	 * plugged into its serial port would be root. `--autologin root` is cut
	 * from that one line and the rest of the file is kept as it ships, its
	 * comments included; the agetty left behind prompts for a login. Only
	 * the `S0` line is touched, so tty1's display manager and tty2's
	 * recovery prompt stay as they are. A file too large for the buffer fails the
	 * install rather than leave the serial root login in place.
	 */
	{
		static const char cut[] = " --autologin root";
		char it[8192];
		int n = slurp(TARGET "/etc/inittab", it, sizeof(it));

		if (n > 0 && (size_t)n < sizeof(it) - 1) {
			char out[8192];
			size_t o = 0;
			int changed = 0;
			char *line = it;

			while (*line) {
				char *nl = strchr(line, '\n');
				char *next = nl ? nl + 1 : line + strlen(line);
				char *at;

				if (nl)
					*nl = '\0';
				if (!strncmp(line, "S0:", 3) &&
				    (at = strstr(line, cut)) != NULL) {
					o += (size_t)snprintf(out + o,
							      sizeof(out) - o,
							      "%.*s%s\n",
							      (int)(at - line),
							      line,
							      at + sizeof(cut) - 1);
					changed = 1;
				} else {
					o += (size_t)snprintf(out + o,
							      sizeof(out) - o,
							      "%s\n", line);
				}
				if (o >= sizeof(out))
					break;
				line = next;
			}
			if (o >= sizeof(out))
				fail("%s/etc/inittab is too large to edit",
				     TARGET);
			if (changed)
				wr("/etc/inittab", "%s", out);
		} else if (n > 0) {
			fail("%s/etc/inittab is too large to edit", TARGET);
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

	if (cfg.swap == SWAP_FILE && cfg.swap_mb > 0 && !ki_ab()) {
		emit('N', "swapfile (%ld MiB)", cfg.swap_mb);
		char sz[32];
		snprintf(sz, sizeof(sz), "%ldM", cfg.swap_mb);

		/*
		 * How a swapfile is made depends on the filesystem under it,
		 * and getting it wrong does not fail here — it fails at the
		 * next boot's `swapon -a`, with the machine simply having no
		 * swap and nothing saying why.
		 *
		 * fallocate leaves unwritten extents, so xfs and f2fs get a
		 * file written out in full, which no kernel's swapon can refuse
		 * for them. btrfs needs the file NOCOW and uncompressed, which
		 * is the whole reason `btrfs filesystem mkswapfile` exists.
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

/*
 * THE NEW ACCOUNT'S HOME IS A NEW ACCOUNT'S: /etc/skel, the XDG directories
 * /etc/xdg/user-dirs.defaults lists (KEY=Folder lines, relative to the home),
 * and what the copy kept of the live home (the box store and bat's cache).
 * skel carries no user-dirs.dirs, so the folders are made here and the first
 * login's xdg-user-dirs-update writes the file over them. skel is laid UNDER
 * what is there, never over it, and everything is handed to the account by
 * number, because the target's passwd is not the live system's and a name
 * could resolve to someone else.
 */
static void fill_home(const char *user)
{
	char home[256], chown_arg[64], dirs[4096];
	uid_t uid = 1000;
	gid_t gid = 1000;

	snprintf(home, sizeof(home), "%s/home/%s", TARGET, user);
	{
		char pw[65536];
		char *save = NULL;
		size_t ul = strlen(user);

		if (slurp(TARGET "/etc/passwd", pw, sizeof(pw)) > 0)
			for (char *l = strtok_r(pw, "\n", &save); l;
			     l = strtok_r(NULL, "\n", &save)) {
				unsigned u, g;

				if (strncmp(l, user, ul) || l[ul] != ':')
					continue;
				if (sscanf(l + ul, ":%*[^:]:%u:%u", &u, &g) == 2) {
					uid = (uid_t)u;
					gid = (gid_t)g;
				}
				break;
			}
	}
	mkpath("%s", home);
	snprintf(chown_arg, sizeof(chown_arg), "--chown=%u:%u", (unsigned)uid,
		 (unsigned)gid);
	emit('N', "%s's home from /etc/skel", user);
	if (kb_path_exists(TARGET "/etc/skel")) {
		char src[300];
		char *rs[] = { "rsync", "-a", "--ignore-existing", "--numeric-ids",
			       chown_arg, src, home, NULL };

		snprintf(src, sizeof(src), "%s/etc/skel/", TARGET);
		if (run(rs))
			emit('W', "the new home did not get all of /etc/skel");
	}
	if (slurp(TARGET "/etc/xdg/user-dirs.defaults", dirs,
		  sizeof(dirs)) > 0) {
		char *save = NULL;

		for (char *l = strtok_r(dirs, "\n", &save); l;
		     l = strtok_r(NULL, "\n", &save)) {
			char path[512];
			const char *rel = strchr(l, '=');

			/* One folder directly under the home: a value with a
			 * slash, or one that is . or .., names no folder of
			 * its own and is skipped. */
			if (l[0] == '#' || !rel || !*++rel || strchr(rel, '/') ||
			    !strcmp(rel, ".") || !strcmp(rel, ".."))
				continue;
			snprintf(path, sizeof(path), "%s/%s", home, rel);
			mkpath("%s", path);
			if (!cfg.dry_run && chown(path, uid, gid) < 0)
				logf_("  cannot hand %s to %s: %s", path, user,
				      strerror(errno));
		}
	}
	if (!cfg.dry_run) {
		if (chown(home, uid, gid) < 0)
			logf_("  cannot hand %s to %s: %s", home, user,
			      strerror(errno));
		chmod(home, 0700);
	}
}

static void do_accounts(void)
{
	const char *uhash = NULL, *rhash = NULL;

	if (cfg.oem) {
		/* No account: the first person makes one in plasma-setup. Root
		 * stays locked, as the image ships it; the account plasma-setup
		 * makes is an administrator. */
		emit('N', "removing the live account for the first boot's setup");
		drop_account("kdos");
		rewrite_accounts("kdos", "kdos", "KDOS User", NULL,
				 cfg.root_locked ? NULL : kb_strdup(hash_password(
								 cfg.rootpass)),
				 0);
		emit('P', "1");
		return;
	}

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
	emit('P', "0.5");
	fill_home(cfg.username);
	emit('P', "1");
}

/*
 * THE ACCENT AND THE DESK, as files the first login reads: the global theme
 * and the desk in /etc/xdg, the splash's and the console's accent, and the
 * login screen's. Nothing is run: startplasma applies the global theme in
 * Defaults mode at the first login, and the theme's layout script lays out the
 * desk, so the account starts exactly as a new account on this machine would.
 */
static void do_theme(void)
{
	char why[256];

	emit('N', "%s, %s desk", cfg.theme, cfg.desk);
	if (ki_write_look(TARGET, why, sizeof(why)))
		fail("%s", why);
	emit('P', "1");
}

/*
 * SLOT B, FILLED WITH SLOT A AS IT NOW STANDS: configured, with its accounts
 * and its accent. Both slots then hold the same system, and the first update
 * has an inactive slot to be written into; kdos-bootctl takes it from there.
 *
 * `-x` keeps the copy on slot A's own filesystem, so the shared home and the
 * ESP mounted under the target are not copied into a slot — each slot mounts
 * them. The one line that differs is fstab's root, rewritten to B's own UUID.
 */
static void do_slotb(void)
{
	char a_uuid[64] = "", b_uuid[64] = "", from[96], to[96];
	Part p;

	resolve_parts();
	if (!part_b[0])
		fail("no second slot was made");
	probe_part(part_root, &p);
	kb_strlcpy(a_uuid, p.uuid, sizeof(a_uuid));
	probe_part(part_b, &p);
	kb_strlcpy(b_uuid, p.uuid, sizeof(b_uuid));
	if (!cfg.dry_run && (!a_uuid[0] || !b_uuid[0]))
		fail("cannot read the slots' UUIDs back");

	mkpath(TARGET_B);
	emit('N', "mounting %s at %s", part_b, TARGET_B);
	char *m[] = { "mount", part_b, TARGET_B, NULL };
	must(m);

	char *argv[] = { "rsync", "-aHAX", "-x", "--numeric-ids",
			 "--info=progress2", "--no-inc-recursive",
			 TARGET "/", TARGET_B "/", NULL };
	static const int ok[] = { 0 };
	emit('N', "copying slot A into slot B");
	emit('P', "0");
	if (run_full(argv, NULL, rsync_parse, ok, 1))
		fail("rsync into slot B failed");

	snprintf(from, sizeof(from), "UUID=%s\t/\t", a_uuid);
	snprintf(to, sizeof(to), "UUID=%s\t/\t", b_uuid);
	{
		char fst[16384], out[16384] = "";
		char *at;

		if (slurp(TARGET_B "/etc/fstab", fst, sizeof(fst)) > 0 &&
		    (at = strstr(fst, from)) != NULL) {
			*at = 0;
			cat(out, sizeof(out), fst);
			cat(out, sizeof(out), to);
			cat(out, sizeof(out), at + strlen(from));
			logf_("write %s/etc/fstab (root UUID=%s)", TARGET_B,
			      b_uuid);
			if (!cfg.dry_run &&
			    kb_write_file(TARGET_B "/etc/fstab", out) < 0)
				fail("cannot write %s/etc/fstab", TARGET_B);
		} else if (!cfg.dry_run) {
			fail("slot B's fstab names no root UUID=%s", a_uuid);
		}
	}

	if (!cfg.dry_run)
		sync();
	char *u[] = { "umount", TARGET_B, NULL };
	must(u);
	emit('P', "1");
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
	char root_uuid[64] = "", esp_uuid[64] = "", b_uuid[64] = "";
	char crypt_opt[128] = "", slot_opt[128] = "";
	/* Slot B's own menu entry, in the shape kdos-bootctl gives the other
	 * slot, or "" on a machine with one root. */
	char b_entry[768] = "";
	/* The CONTAINER this slot's filesystem lives inside, recorded in the
	 * boot state beside the filesystem. Empty on an unencrypted install.
	 * See the bootstate write below. */
	char luks_uuid[64] = "";
	Part p;

	resolve_parts();
	probe_part(part_root, &p);
	kb_strlcpy(root_uuid, p.uuid, sizeof(root_uuid));
	if (part_b[0]) {
		Part bp;
		probe_part(part_b, &bp);
		kb_strlcpy(b_uuid, bp.uuid, sizeof(b_uuid));
		if (!b_uuid[0] && !cfg.dry_run)
			fail("cannot read a UUID back from %s", part_b);
	}
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
	/* Slot B boots the kernel of the root it was filled from, which is the
	 * same kernel; it gets its own copy because every later update replaces
	 * one slot's pair and never the other's. */
	if (part_b[0]) {
		mkpath(TARGET "/boot/efi/EFI/kdos/b");
		copy_file(TARGET "/boot/efi/EFI/kdos/a/initramfs.cpio.gz",
			  TARGET "/boot/efi/EFI/kdos/b/initramfs.cpio.gz");
		copy_file(TARGET "/boot/efi/EFI/kdos/a/vmlinuz",
			  TARGET "/boot/efi/EFI/kdos/b/vmlinuz");
		snprintf(b_entry, sizeof(b_entry),
			 "\n"
			 "/KDOS (slot b)\n"
			 "    comment: The other root, for one boot: nothing is "
			 "confirmed\n"
			 "    protocol: linux\n"
			 "    path: boot():/EFI/kdos/b/vmlinuz\n"
			 "    module_path: boot():/EFI/kdos/b/initramfs.cpio.gz\n"
			 "    cmdline: kdos_slot=b %s%sroot=UUID=%s rw "
			 "console=tty0 " KI_QUIET "\n",
			 slot_opt, crypt_opt, b_uuid);
	} else {
		esp_room_for_b();
	}

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
	 * THE FACE LIVES ON A BOOT VOLUME AND NEVER IN THE ROOT FILESYSTEM.
	 * `psf2limine.py` writes font.bin straight into the ISO tree during
	 * 70_image, so on a live session it is on the ISO9660 volume the
	 * initramfs mounts at /mnt/iso — readable here because TARGET is a
	 * directory below /mnt and not /mnt itself. An install run from an
	 * installed KDOS finds the copy this step put on that machine's own
	 * ESP. Neither found, and the installed menu falls back to Limine's
	 * built-in typeface while the medium it came from does not.
	 */
	const char *fontline = "";
	static const char *const fonts[] = {
		"/mnt/iso/boot/limine/font.bin",
		"/boot/efi/EFI/kdos/font.bin",
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
	 * files 70_image reads are shipped by the fs/ overlay, so this resolves
	 * without a mounted medium and an install run from a running system gets
	 * the same backdrop as one run from the stick. The order matches
	 * `110_iso.sh` — the generated backdrop, then the banner.
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
	   "    cmdline: kdos_slot=a %s%sroot=UUID=%s rw console=tty0 " KI_QUIET "\n"
	   "\n"
	   "/KDOS (verbose)\n"
	   "    comment: Every kernel message on the console\n"
	   "    protocol: linux\n"
	   "    path: boot():/EFI/kdos/a/vmlinuz\n"
	   "    module_path: boot():/EFI/kdos/a/initramfs.cpio.gz\n"
	   "    cmdline: kdos_slot=a %s%sroot=UUID=%s rw console=tty0 loglevel=7\n"
	   "\n"
	   "/KDOS (single user)\n"
	   "    comment: A root shell for root's password, no desktop\n"
	   "    protocol: linux\n"
	   "    path: boot():/EFI/kdos/a/vmlinuz\n"
	   "    module_path: boot():/EFI/kdos/a/initramfs.cpio.gz\n"
	   "    cmdline: kdos_slot=a %s%sroot=UUID=%s rw console=tty0 loglevel=7 single\n"
	   "%s%s",
	   theme, paper, fontline,
	   slot_opt, crypt_opt, root_uuid,
	   slot_opt, crypt_opt, root_uuid,
	   slot_opt, crypt_opt, root_uuid, b_entry, memtest);

	/*
	 * The initial boot state, so the machine starts life as slot A with
	 * nothing to roll back to, and with slot B — where the layout made one —
	 * recorded and holding the same system. `kdos update` installs into the
	 * slot not running, puts its kernel in that slot's EFI/kdos/<slot>/ with
	 * `kdos-bootctl deploy` and calls `kdos-bootctl try`, and kdos-bootctl
	 * rewrites the /KDOS entries above for every change of state. The
	 * entries written here are the shape it regenerates, and the command
	 * line it carries over.
	 *
	 * Written straight rather than through kdos-bootctl: this runs from the
	 * live image against a target at /mnt/kinstall, and the tool's default path is
	 * the RUNNING system's ESP. One file, and the format is in bootctl.c.
	 *
	 * `crypt_a` IS WHAT JOINS THE TWO MECHANISMS. `slot_a` is the
	 * FILESYSTEM and `crypt_a` is the container it is inside; the command
	 * line can name only one `cryptdevice=`, so a second slot inside a
	 * second container is only reachable because each slot records its
	 * own and the initramfs asks after it has chosen. Empty here on an
	 * unencrypted install, which is the fallback to the command line. On
	 * the A/B layout both slots are volumes of one group inside one
	 * container, so both name it.
	 */
	if (esp_uuid[0]) {
		mkpath(TARGET "/boot/efi/EFI/kdos");
		wr("/boot/efi/EFI/kdos/bootstate",
		   "# KDOS boot state. Written by the installer; maintained by\n"
		   "# kdos-bootctl.\n"
		   "slot_a   = %s\n"
		   "slot_b   = %s\n"
		   "crypt_a  = %s\n"
		   "crypt_b  = %s\n"
		   "active   = a\n"
		   "try      = \n"
		   "attempts = 0\n",
		   root_uuid, b_uuid, luks_uuid, b_uuid[0] ? luks_uuid : "");
	}

	/*
	 * THE BIOS BOOT CODE IS WRITTEN WHATEVER THIS MACHINE BOOTED AS, so a
	 * disk installed on a UEFI machine also starts on a legacy one. On a
	 * GPT disk Limine's BIOS stage goes into the BIOS boot partition whose
	 * number is passed — the erase plan's own, or one a reused disk
	 * already has — because the gap after the boot sector holds the GPT;
	 * on a dos table it goes into that gap and takes no number. A reused
	 * GPT disk with no BIOS boot partition gets none and starts on UEFI
	 * only.
	 *
	 * A FAILURE STOPS THE INSTALL ONLY WHERE IT LEAVES THIS MACHINE
	 * UNABLE TO START IT. Booted from legacy BIOS, the BIOS stage is the
	 * only way in, so the step fails; on UEFI the EFI path above is
	 * complete and the failure is a warning the Install and Done pages
	 * list. Limine's own `error:` lines are in kinstall.log either way.
	 */
	int bb = ki_bios_boot_part();
	if (bb < 0) {
		bios_problem("%s has no BIOS boot partition — this disk starts "
			     "on UEFI only", cfg.disk);
	} else if (!kb_have_prog("limine")) {
		bios_problem("limine not on PATH — no BIOS boot code written; "
			     "this disk starts on UEFI only");
	} else {
		char disk[64], num[12];
		kb_strlcpy(disk, cfg.disk, sizeof(disk));
		snprintf(num, sizeof(num), "%d", bb);
		emit('N', "BIOS boot code onto %s", cfg.disk);
		char *bi[] = { "limine", "bios-install", disk, bb ? num : NULL,
			       NULL };
		if (run(bi) != 0)
			bios_problem("limine bios-install failed (see "
				     "/var/log/kinstall.log) — this disk "
				     "starts on UEFI only");
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
	unmount_below(TARGET_B "/");
	unmount_below(TARGET "/");
	char *u[] = { "umount", TARGET, NULL };
	try_(u);
	emit('P', "1");
}

/* ══════════════════════════════════════════════════════════════════════ */

/* The writers' lines, into the log the parent keeps. */
static void child_log(const char *line)
{
	emit('L', "%s", line);
}

int install_child_main(int fd, int from_step)
{
	wfd = fd;
	ki_log_hook = child_log;

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
		do_apps, do_config, do_accounts, do_theme, do_slotb, do_boot,
		do_finish,
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
