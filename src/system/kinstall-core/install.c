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
#include <grp.h>
#include <limits.h>
#include <poll.h>
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
	S_FIRMWARE,
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
	{ "Packs",       "native groups installed, boxed applications recorded for the first login" },
	{ "Firmware",    "what this machine's devices need beyond the medium's firmware" },
	{ "Configure",   "fstab, hostname, keyboard, login screen, services" },
	{ "Accounts",    "users, passwords, sudo, the home" },
	{ "Theme",       "the accent and the desk the first login takes" },
	{ "Slot B",      "the second root, filled for A/B updates" },
	{ "Bootloader",  NULL },
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
	if (i == S_APPS && (!ki_apps_present || ki_apps_route() == APPS_NONE) &&
	    !ki_groups_chosen())
		return 1;
	return 0;
}

/*
 * THE STEPS' WEIGHTS, for a front end's bar over the whole install: each
 * step's expected time in tenths of a second, so the bar moves with the
 * clock. Measured from kinstall.log's timestamps on the live medium's GUI
 * installs onto a virtual disk: Prepare, Partition, Format and Mount take
 * about three seconds between them, the copy of the live tree's 4.8 GiB 50
 * to 56 seconds, slot B's 21 to 28, and the steps after the copy about
 * four. The two copies weigh their bytes at the rates measured,
 * KI_COPY_MIB_S for ki_need_bytes() and KI_SLOTB_MIB_S for the payload less
 * the box store, which slot B's -x copy never enters. Packs weighs its two
 * seconds of recording and, only when a binhost is named and there is a
 * network (ki_binhost_named, ki_sys.network), KI_PACKS_PORT_DS for each
 * port of the chosen native groups, an allowance no install has measured;
 * with no binhost or no network the groups go to groups-pending and
 * nothing downloads. A short step given a share of
 * the copy's time would hold back the bar through the copy and fill a third
 * of it in the last seconds.
 */
#define KI_COPY_MIB_S 90.0
#define KI_SLOTB_MIB_S 190.0
#define KI_PACKS_PORT_DS 10

static int step_weight(int i)
{
	static const int short_ds[S_COUNT] = {
		[S_PREPARE] = 3,  [S_PARTITION] = 10, [S_FORMAT] = 15,
		[S_MOUNT] = 2,	  [S_APPS] = 20,      [S_FIRMWARE] = 3,
		[S_CONFIG] = 3,	  [S_ACCOUNTS] = 3,   [S_THEME] = 2,
		[S_BOOT] = 8,	  [S_FINISH] = 3,
	};
	double mib = 0, rate = 1;
	int w;

	if (i == S_COPY) {
		mib = (double)ki_need_bytes() / (1024.0 * 1024.0);
		rate = KI_COPY_MIB_S;
	} else if (i == S_SLOTB) {
		mib = (double)(ki_sys.payload_kb > ki_sys.appbox_kb
				       ? ki_sys.payload_kb - ki_sys.appbox_kb
				       : 0) /
		      1024.0;
		rate = KI_SLOTB_MIB_S;
	} else if (i == S_APPS) {
		w = short_ds[i];
		for (int k = 0; ki_binhost_named && ki_sys.network &&
				k < ki_nnative; k++)
			if (ki_native[k].chosen)
				w += ki_native[k].nports * KI_PACKS_PORT_DS;
		return w;
	} else {
		return short_ds[i];
	}
	w = (int)(mib / rate * 10 + 0.5);
	return w < 1 ? 1 : w;
}

/* The Partition step's line names what its last partition is: the LVM
 * volume, encrypted when asked, and what it holds, or the root itself; it
 * replaces the table's line, which names the root. At most 80 characters,
 * which the graphical Summary's value column wraps inside itself; a longer
 * line only costs rows, since the column's width is capped. */
static const char *partition_detail(void)
{
	static char d[160];
	const char *swap = ki_wipe_part_swap() ? ", swap" : "";

	if (cfg.plan == PLAN_WIPE && cfg.lvm)
		snprintf(d, sizeof(d),
			 "write the GPT layout: ESP, BIOS boot%s, %sLVM volume "
			 "for %s",
			 swap, cfg.luks ? "encrypted " : "",
			 ki_ab() ? "both system slots" : "the root");
	else
		snprintf(d, sizeof(d),
			 "write the GPT layout: ESP, BIOS boot%s, %sroot", swap,
			 cfg.luks ? "encrypted " : "");
	return d;
}

/* The Packs step's line says what becomes of the native groups chosen: on
 * a medium that names a binary host, with a network, they are downloaded
 * and installed, and otherwise they are recorded, as the Summary's rows
 * above the steps say (ki_binhost_named, ki_sys.network). The boxed
 * applications are recorded either way, and the first login's notice
 * (kdos-notice) names what each record holds and how it is installed, so
 * the line leaves the command to it. At most 80 characters, as
 * partition_detail's. */
static const char *packs_detail(void)
{
	return ki_binhost_named && ki_sys.network
		? "native groups downloaded and installed, boxed apps recorded "
		  "for the first login"
		: "native groups and boxed apps recorded; the first login says "
		  "how to install each";
}

/* The Bootloader step names only what this firmware gets: a BIOS machine has
 * no NVRAM, so no boot entry is written and the step must not promise one. */
static const char *boot_detail(void)
{
	return ki_sys.uefi
		? "Limine on the ESP, the BIOS boot code, the UEFI boot entry"
		: "Limine on the ESP and the BIOS boot code";
}

void install_plan(void)
{
	memset(inst.step, 0, sizeof(inst.step));
	inst.nsteps = S_COUNT;
	for (int i = 0; i < S_COUNT; i++) {
		inst.step[i].title = steps[i].title;
		inst.step[i].detail = i == S_PARTITION ? partition_detail()
				      : i == S_APPS ? packs_detail()
				      : i == S_BOOT ? boot_detail()
						    : steps[i].detail;
		inst.step[i].state = step_skipped(i) ? ST_SKIP : ST_PENDING;
		inst.step[i].frac = -1;
		inst.step[i].weight = step_weight(i);
	}
	inst.cur = -1;
	inst.failed = inst.done = inst.running = 0;
	inst.failmsg[0] = 0;
	inst.nwarn = 0;
}

/*
 * A LINE KEPT FOR A SCREEN HOLDS NO CONTROL BYTE: a tab becomes the spaces to
 * the next multiple of 8, as a terminal would set it, and every other C0 byte
 * is dropped. A screen counts a tab as one cell and a VT moves it to the next
 * tab stop, so mke2fs's and efibootmgr's tabbed columns would run past the
 * pane and into the sidebar. The log file keeps the line as it came.
 */
static void screen_line(char *out, size_t n, const char *line)
{
	size_t o = 0, col = 0;

	for (const unsigned char *c = (const unsigned char *)line;
	     *c && o + 1 < n; c++) {
		if (*c == '\t') {
			do {
				out[o++] = ' ';
				col++;
			} while (col % 8 && o + 1 < n);
			continue;
		}
		if (*c < 0x20 || *c == 0x7f)
			continue;
		out[o++] = (char)*c;
		/* A UTF-8 continuation byte is part of the cell before it. */
		if ((*c & 0xc0) != 0x80)
			col++;
	}
	out[o] = 0;
}

void install_log(const char *line)
{
	if (!inst.log)
		inst.log = kb_calloc(LOG_LINES, LOG_COLS);
	screen_line(inst.log[inst.nlog % LOG_LINES], LOG_COLS, line);
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

/*
 * THE LOG IS READABLE BY `wheel`, mode 0640, so the live session's account,
 * an administrator, can open the file the window's Done page names. It
 * holds commands and their output and never a secret: passwords and the
 * passphrase reach their programs on a pipe, never in argv, and nothing
 * logs them. Set on every open, because O_CREAT's mode reaches only a new
 * file.
 */
static int open_log(void)
{
	int fd = open("/var/log/kinstall.log",
		      O_WRONLY | O_CREAT | O_APPEND | O_CLOEXEC, 0640);
	struct group *g;

	if (fd < 0 || geteuid() != 0)
		return fd;
	g = getgrnam("wheel");
	if (g && fchown(fd, 0, g->gr_gid) == 0)
		fchmod(fd, 0640);
	else
		fchmod(fd, 0600);
	return fd;
}

void install_start(int from_step)
{
	int fds[2];
	/* Close-on-exec: the child's end is the event pipe, and every program
	 * the install runs would otherwise inherit it, which lvm names in a
	 * warning on every call. */
	if (pipe2(fds, O_CLOEXEC) < 0) {
		inst.failed = 1;
		inst.t0 = inst.t1 = kb_now_s();
		kb_strlcpy(inst.failmsg, "pipe() failed", sizeof(inst.failmsg));
		return;
	}

	if (!inst.log)
		inst.log = kb_calloc(LOG_LINES, LOG_COLS);
	if (!inst.logfd)
		inst.logfd = open_log();
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
	/* A retry keeps the first start: the elapsed time and Done's figure
	 * cover the steps that ran before the failure as well. */
	if (from_step == 0 || inst.t0 == 0)
		inst.t0 = kb_now_s();
	if (!cfg.dry_run)
		shield_from_oomd();

	pid_t pid = fork();
	if (pid < 0) {
		close(fds[0]);
		close(fds[1]);
		inst.failed = 1;
		inst.t1 = kb_now_s();
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
		inst.t1 = kb_now_s();
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
		inst.t1 = kb_now_s();
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
		inst.t1 = kb_now_s();
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
	inst.t1 = kb_now_s();
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

/* One static buffer: one call per message. */
const char *ki_root_cmd(const char *cmd)
{
	static char buf[256];

	if (cfg.user_wheel)
		snprintf(buf, sizeof(buf), "`sudo %s`", cmd);
	else
		snprintf(buf, sizeof(buf), "`su -c '%s'` (root's password)",
			 cmd);
	return buf;
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

/* `parse` reads each line the program prints (NULL: each goes to the log),
 * and `tick`, when given, runs each time KI_COPY_TICK_S passes with no
 * output. */
static int run_full(char *const argv[], const char *stdin_data,
		    LineParse parse, void (*tick)(void), const int *ok_codes,
		    int nok)
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

	/* Close-on-exec, so only the ends dup2 places on 0, 1 and 2 reach the
	 * program. */
	int out[2], in[2];
	if (pipe2(out, O_CLOEXEC) < 0)
		fail("pipe: %s", strerror(errno));
	if (stdin_data && pipe2(in, O_CLOEXEC) < 0)
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
		/* Onto the pipe, so the log says which program is missing
		 * rather than an exit 127 its caller reads as the program's
		 * own failure. */
		dprintf(2, "cannot run %s: %s\n", argv[0], strerror(errno));
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
	struct pollfd pfd = { .fd = out[0], .events = POLLIN };
	for (;;) {
		int r = poll(&pfd, 1, tick ? (int)(KI_COPY_TICK_S * 1000) : -1);

		if (r < 0 && errno == EINTR)
			continue;
		if (r == 0) {
			tick();
			continue;
		}
		if ((n = read(out[0], buf, sizeof(buf))) <= 0)
			break;
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
	return run_full(argv, NULL, NULL, NULL, ok, 1);
}

/* Same, with a secret on the child's stdin. cryptsetup's `--key-file=-` reads
 * from there, which keeps the passphrase out of argv and therefore out of
 * /proc/<pid>/cmdline. */
static int run_stdin(char *const argv[], const char *secret)
{
	static const int ok[] = { 0 };
	return run_full(argv, secret, NULL, NULL, ok, 1);
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
/* What the running copy will carry in all, every name of a hard-linked file
 * counted; 0 when it was not measured. */
static unsigned long long rsync_total_bytes;
/* Of rsync's total, the bytes of the names it makes links and never copies:
 * see THE BYTES LEFT ARE THE BYTES RSYNC COPIES. */
static unsigned long long rsync_link_bytes;
/* Where the running copy's cost lies: see THE FILES ARE COUNTED BY WHAT
 * THEY COST. */
static const KiCopyProfile *rsync_prof;
/* When the running copy started, on the monotonic clock. */
static double rsync_t0;
/* The running copy's last file counts, which every copy resets. */
static KiCopyFiles rsync_files;

/* rsync's progress2 byte count, which it prints with thousands separators
 * when run without -h: 10,544,196,181. */
static unsigned long long rsync_bytes(const char *s)
{
	unsigned long long v = 0;

	for (; *s; s++) {
		if (*s == ',')
			continue;
		if (!isdigit((unsigned char)*s))
			break;
		v = v * 10 + (unsigned long long)(*s - '0');
	}
	return v;
}

/* A size in the binary units every page of the installer writes: KiB
 * under a mebibyte, whole MiB under a gibibyte, GiB to one decimal. */
static void iec_size(unsigned long long b, char *out, size_t n)
{
	if (b >= 1ULL << 30)
		snprintf(out, n, "%.1f GiB", (double)b / (1ULL << 30));
	else if (b >= 1ULL << 20)
		snprintf(out, n, "%llu MiB", b >> 20);
	else
		snprintf(out, n, "%llu KiB", b >> 10);
}

void ki_copy_profile(KiCopyProfile *p, const unsigned char *kind, size_t n,
		     int store)
{
	double units = 0, u = 0;
	size_t m = 0, k = 0;
	int b = 1;

	memset(p, 0, sizeof(*p));
	for (size_t i = 0; i < n; i++) {
		if (!store && (kind[i] & KI_ENT_STORE))
			continue;
		m++;
		units += kind[i] & KI_ENT_FILE ? 1 : KI_COPY_OTHER_W;
	}
	if (!m)
		return;
	for (size_t i = 0; i < n && b <= KI_COPY_PROF; i++) {
		if (!store && (kind[i] & KI_ENT_STORE))
			continue;
		u += kind[i] & KI_ENT_FILE ? 1 : KI_COPY_OTHER_W;
		k++;
		while (b <= KI_COPY_PROF && k * KI_COPY_PROF >= (size_t)b * m)
			p->at[b++] = (float)(u / units);
	}
	p->per_entry = units / (double)m;
	p->ok = 1;
}

/* The fraction of a profile's cost done after fraction `x` of its
 * entries. */
static double copy_prof_at(const KiCopyProfile *p, double x)
{
	double k;
	int i;

	if (x <= 0)
		return 0;
	if (x >= 1)
		return 1;
	k = x * KI_COPY_PROF;
	i = (int)k;
	return p->at[i] + (p->at[i + 1] - p->at[i]) * (k - i);
}

/* Keep one sample of the copy's progress, at most one per `step` seconds;
 * a full set drops every other sample and doubles the step, so the samples
 * stay spread over the whole copy in fixed room. */
static void copy_sample(KiCopyFiles *c, double el, double bytes, double files)
{
	if (c->step <= 0)
		c->step = 0.25;
	if (c->n && el - c->el[c->n - 1] < c->step)
		return;
	if (c->n == KI_COPY_SAMPLES) {
		int k = 0;

		for (int i = 0; i < c->n; i += 2, k++) {
			c->el[k] = c->el[i];
			c->bytes[k] = c->bytes[i];
			c->files[k] = c->files[i];
		}
		c->n = k;
		c->step *= 2;
		if (el - c->el[c->n - 1] < c->step)
			return;
	}
	c->el[c->n] = el;
	c->bytes[c->n] = bytes;
	c->files[c->n] = files;
	c->n++;
}

/* A sample's weight in a fit, `age` seconds older than the newest: half
 * for every `half` seconds, in straight lines between, and none once it
 * is below a millionth. Past that the remainder of the age would be more
 * than `half` and the straight line negative, and a negative weight turns
 * the sums of squares negative and the fit's slopes to nothing. */
static double copy_weight(double age, double half)
{
	double w = 1;

	while (age >= half) {
		w /= 2;
		age -= half;
		if (w < 1e-6)
			return 0;
	}
	return w * (1 - 0.5 * age / half);
}

/* The slope of the seconds on one quantity, with an intercept; 0 when the
 * quantity did not move or the slope comes out negative. */
static double copy_slope(double sxx, double sxt)
{
	return sxx > 0 && sxt > 0 ? sxt / sxx : 0;
}

/* The weighted sums of squares and products about the weighted means of
 * the bytes (b), files (f) and seconds (t) of the samples, each weighed by
 * its age with half-life `half`. */
typedef struct {
	double bb, ff, bf, bt, ft;
} CopySums;

static void copy_sums(const KiCopyFiles *c, double half, CopySums *s)
{
	double mb = 0, mf = 0, mt = 0, sw = 0, w[KI_COPY_SAMPLES];

	memset(s, 0, sizeof(*s));
	for (int i = 0; i < c->n; i++) {
		w[i] = copy_weight(c->el[c->n - 1] - c->el[i], half);
		sw += w[i];
		mb += w[i] * c->bytes[i];
		mf += w[i] * c->files[i];
		mt += w[i] * c->el[i];
	}
	mb /= sw;
	mf /= sw;
	mt /= sw;
	for (int i = 0; i < c->n; i++) {
		double db = c->bytes[i] - mb, df = c->files[i] - mf;
		double dt = c->el[i] - mt;

		s->bb += w[i] * db * db;
		s->ff += w[i] * df * df;
		s->bf += w[i] * db * df;
		s->bt += w[i] * db * dt;
		s->ft += w[i] * df * dt;
	}
}

/* The time left with one cost fitted and the other held at `fc` seconds a
 * file: the cost per byte, or with no file counts (`fleft` < 0) the bytes'
 * alone. -1 when the bytes did not move in the samples' weight, so there
 * is no cost per byte to fit ... */
static double copy_by_bytes(const CopySums *s, double bleft, double fleft,
			    double fc)
{
	double a;

	if (fleft < 0)
		fc = fleft = 0;
	a = copy_slope(s->bb, s->bt - fc * s->bf);
	return a > 0 ? a * bleft + fc * fleft : -1;
}

/* ... and the cost per file with each byte held at `bc` seconds. */
static double copy_by_files(const CopySums *s, double bleft, double fleft,
			    double bc)
{
	double b = copy_slope(s->ff, s->ft - bc * s->bf);

	return b > 0 ? b * fleft + bc * bleft : -1;
}

/* The seconds the rest of the copy takes, from the samples: el = c +
 * a·bytes + b·files fitted by least squares, each sample weighed by its
 * age (copy_weight, half-life KI_COPY_HALF_S), and a·bytes_left +
 * b·files_left, of `btot` bytes and `ftot` files in all. -1 when the
 * samples span less than KI_COPY_SPAN_S seconds, or neither quantity has
 * moved enough, to fit.
 *
 * Both costs are fitted only when the samples can tell them apart: each
 * quantity has moved by a two-hundredth of its total, and the two have not
 * moved in step (r² of one on the other at most 0.99). Otherwise one cost
 * is fitted with the other held at its floor, KI_COPY_FILE_S a file or
 * KI_COPY_BYTE_S a byte, and of the two ways round the lower estimate is
 * taken. A cost fitted on a quantity that has barely moved, or on one that
 * moves in step with the other, takes the whole time as its own: through a
 * run of large files the time per file is the time per large file, which
 * times the small files still to go reads as many minutes when the tail is
 * one. The floor keeps the files left from counting for nothing; the
 * estimate is low by the rest of their cost until the samples show it.
 * The time shown starts from this estimate and counts down toward it
 * (copy_hold). */
static double copy_fit(const KiCopyFiles *c, double bleft, double btot,
		       double fleft, double ftot)
{
	CopySums s;
	double det, a, b, tb, tf, t;
	int bmoved, fmoved;

	if (c->n < 2 || c->el[c->n - 1] - c->el[0] < KI_COPY_SPAN_S)
		return -1;
	bmoved = c->bytes[c->n - 1] - c->bytes[0] >= btot / 200;
	fmoved = fleft >= 0 && c->files[c->n - 1] - c->files[0] >= ftot / 200;
	if (!bmoved && !fmoved)
		return -1;
	copy_sums(c, KI_COPY_HALF_S, &s);
	if (fleft < 0) {
		t = copy_by_bytes(&s, bleft, fleft, 0);
		return t < 0 ? 0 : t;
	}
	tb = copy_by_bytes(&s, bleft, fleft, KI_COPY_FILE_S);
	tf = copy_by_files(&s, bleft, fleft, KI_COPY_BYTE_S);
	if (tb < 0)
		tb = KI_COPY_FILE_S * fleft;
	if (tf < 0)
		tf = KI_COPY_BYTE_S * bleft;
	if (!fmoved || s.ff <= 0) {
		t = tb;
	} else if (!bmoved || s.bb <= 0) {
		t = tf;
	} else if ((det = s.bb * s.ff - s.bf * s.bf) <= 0.01 * s.bb * s.ff) {
		t = tb < tf ? tb : tf;
	} else {
		a = (s.ff * s.bt - s.bf * s.ft) / det;
		b = (s.bb * s.ft - s.bf * s.bt) / det;
		if (a < 0)
			t = copy_slope(s.ff, s.ft) * fleft;
		else if (b < 0)
			t = copy_slope(s.bb, s.bt) * bleft;
		else
			t = a * bleft + b * fleft;
	}
	return t;
}

/* Estimate `t`, made `el` seconds in, into the copy's history: one slot
 * each KI_COPY_HIST_S, holding the lowest and highest made in it. */
static void copy_hist_add(KiCopyFiles *c, double t, double el)
{
	int k = (c->hn + KI_COPY_HIST - 1) % KI_COPY_HIST;

	if (c->hn && el - c->h_el[k] < KI_COPY_HIST_S) {
		if (t < c->h_lo[k])
			c->h_lo[k] = t;
		if (t > c->h_hi[k])
			c->h_hi[k] = t;
		return;
	}
	k = c->hn++ % KI_COPY_HIST;
	c->h_el[k] = el;
	c->h_lo[k] = c->h_hi[k] = t;
}

/* The lowest and highest of the estimates that stood over the `span`
 * seconds before `el`: those made in them and the one standing as they
 * began. 0 when the history does not reach back that far. */
static int copy_hist(const KiCopyFiles *c, double el, double span,
		     double *lo, double *hi)
{
	int n = c->hn < KI_COPY_HIST ? c->hn : KI_COPY_HIST;

	*lo = *hi = c->est;
	for (int i = 0; i < n; i++) {
		int k = (c->hn - 1 - i) % KI_COPY_HIST;

		if (c->h_lo[k] < *lo)
			*lo = c->h_lo[k];
		if (c->h_hi[k] > *hi)
			*hi = c->h_hi[k];
		if (el - c->h_el[k] >= span)
			return 1;
	}
	return 0;
}

/* The time left to show for estimate `t`, `el` seconds in with `done`
 * bytes and `fdone` files copied, `last` on the copy's last line, or -1
 * while the estimate has not settled: see THE TIME SHOWN below. A line
 * (`line` 1) adds its estimate to the history; a tick (ki_copy_tick)
 * passes the last line's estimate and progress again. */
static double copy_hold(KiCopyFiles *c, double t, double el, double done,
			double fdone, int last, int line)
{
	double lo, hi, frac, below, rate, dt = el - c->shown_el;
	int covered;

	if (!c->shown_ok || done > c->shown_bytes || fdone > c->shown_files)
		c->moved_el = el;
	if (line)
		copy_hist_add(c, t, el);
	c->shown_ok = 1;
	c->shown_bytes = done;
	c->shown_files = fdone;
	if (last) {
		c->counting = 1;
		t = 0;
	} else if (!c->counting) {
		covered = copy_hist(c, el, KI_COPY_SETTLE_S, &lo, &hi);
		frac = el / (el + t);
		if (frac < KI_COPY_SETTLED &&
		    (frac < KI_COPY_START || !covered ||
		     hi > lo * (1 + KI_COPY_SPREAD))) {
			c->shown_el = el;
			return -1;
		}
		/* The first figure is the settled estimate. */
		c->counting = 1;
	} else if (el - c->moved_el >= KI_COPY_QUIET_S) {
		t = c->shown;
	} else {
		copy_hist(c, el, KI_COPY_EST_S, &lo, &hi);
		if (c->shown > hi) {
			t = c->shown - dt < hi ? c->shown - dt : hi;
			if (t < c->shown - KI_COPY_FAST * dt)
				t = c->shown - KI_COPY_FAST * dt;
		} else {
			below = hi > 0 ? 1 - c->shown / hi : 0;
			rate = 1 - (1 - KI_COPY_SLOW) * (below - KI_COPY_NEAR) /
					   (KI_COPY_FAR - KI_COPY_NEAR);
			if (rate > 1)
				rate = 1;
			if (rate < KI_COPY_SLOW)
				rate = KI_COPY_SLOW;
			t = c->shown - rate * dt;
		}
	}
	if (!last && t < 1)
		t = c->shown > 0 && c->shown < 1 ? c->shown : 1;
	c->shown = t;
	c->shown_el = el;
	return t;
}

/* The copy line's text for `done` bytes `el` seconds in, `t` seconds
 * left (below 0: "estimating time left") and a payload of `total` less
 * `links` bytes (none when 0). */
static void copy_text(double done, double el, unsigned long long total,
		      unsigned long long links, double t, char *note, size_t n)
{
	char had[24], all[24], rate[40] = "--", left[40] = "estimating time left";

	if (el >= 1) {
		char r[24];

		iec_size((unsigned long long)(done / el), r, sizeof(r));
		snprintf(rate, sizeof(rate), "%s/s", r);
	}
	if (t >= 0) {
		unsigned long long r2 = (unsigned long long)(t + 0.5);
		snprintf(left, sizeof(left), "about %llu:%02llu:%02llu left",
			 r2 / 3600, r2 / 60 % 60, r2 % 60);
	}
	iec_size((unsigned long long)done, had, sizeof(had));
	if (total) {
		iec_size(total > links ? total - links : total, all,
			 sizeof(all));
		snprintf(note, n, "%s of %s, %s, %s", had, all, rate, left);
	} else {
		snprintf(note, n, "%s copied, %s, %s", had, rate, left);
	}
}

/*
 * THE RATE AND THE TIME LEFT ARE KINSTALL'S OWN. rsync's progress2 time
 * field is the time left on its running lines and the time taken on the
 * line it prints as each file ends, which is most of them, and its rate is
 * in its own decimal units. Both are worked out here: the rate is the
 * bytes copied over the seconds since rsync started.
 *
 * THE TIME LEFT IS TWO COSTS: the bytes still to go at a cost per byte, and
 * the files still to go at a cost per file, the two fitted by least squares,
 * with an intercept, on samples of the seconds, bytes and files done
 * (copy_fit). A sample weighs half as much for every KI_COPY_HALF_S seconds
 * it is older than the newest, so the costs are those of the part of the
 * tree being copied now and not an average over the whole copy so far. A
 * live tree copies its large files at the disk's speed and
 * ends in tens of thousands of small ones, where the per-file cost is what
 * takes the time: scaled from bytes alone that tail reads as seconds when
 * it is a minute, and scaled from files alone, at a rate that counts the
 * large files' time as the files', the copy reads two or three times as
 * long as it is. The intercept takes up the seconds rsync spends listing
 * the tree before its first progress line, which an average from the
 * start would spread over every byte and file.
 *
 * A COST THE COPY HAS NOT SHOWN YET IS NOT KNOWN. While the samples cannot
 * tell the two costs apart — a run of large files, or of small ones, so
 * far — the unknown one is held at its floor (copy_fit), and the time left
 * is low by the rest of it: through a tree whose large files all come
 * before its small ones, the small ones' cost shows only when they start,
 * and the estimate takes it in a few seconds later, but a figure once
 * shown does not rise to it (THE TIME SHOWN): it slows, and reads low
 * through the tail. The live tree mixes the two from its first seconds,
 * and its time left is close from early on.
 *
 * Until the samples span KI_COPY_SPAN_S seconds there is nothing to fit,
 * and the note reads "estimating time left". The first time shown is a
 * fitted one: an estimate from the first lines alone, of bytes or of files
 * with the other cost left out, is low, and it would take a rise to
 * correct.
 * With no `files` to fit on, the time left is that estimate from the first
 * progress line on: the bytes', or the files' when that is lower, since
 * the one of the two that has barely moved gives hours.
 *
 * The files left are read from `to-chk=<left>/<total>` only. `ir-chk=` is
 * a list still growing, whose "left" is the part found so far; the copy
 * runs --no-inc-recursive, so rsync prints `to-chk=` from its first line.
 *
 * THE FILES ARE COUNTED BY WHAT THEY COST. `to-chk` counts every entry of
 * rsync's list, and a directory, a symbolic link or a later name of a file
 * costs a fraction of a regular file (KiCopyProfile). The walk's profile
 * turns the entries done into the cost done, read at the same fraction of
 * the list, so the files done and left that the fit sees are in the
 * profile's units: through the live tree's icon themes, where the links
 * outnumber the files, the files left shrink by what they cost and not by
 * their count, and the time left does not charge the links a file's cost.
 *
 * THE BYTES LEFT ARE THE BYTES RSYNC COPIES, which are not the bytes its
 * percentage is of. With -H its total counts every name of a hard-linked
 * file, and it copies the first and makes the others links, at no cost:
 * on the live tree those names are about 15% of its total, so the
 * percentage stops near 85% as the copy ends, and bytes left that cost
 * nothing read as a third more time than there is. The total is rsync's,
 * worked out from its percentage once that is ten or more, less `links`,
 * the bytes of every name after a file's first (probe_payload); before
 * that, `total` less `links`. It is never less than the bytes done, so the
 * bytes shown reach the total as the copy ends. The
 * note's "of" is `total` less `links`, the walk's own figure: one worked
 * out from a whole-number percentage moves by a tenth of a GiB from line to
 * line.
 *
 * rsync prints the file counts only on the line that ends a file; the
 * lines it prints once a second while one large file copies carry none.
 * The files done stand at the last reading through them, and a sample is
 * taken only on a line with counts, where bytes and files are of one
 * moment.
 *
 * THE FRACTION DONE IS OF THE TIME: the seconds so far over those and the
 * time left shown, or the estimate while no figure is shown, once there is
 * an estimate, and of the bytes before that, never less than the last one. A front end's bars follow it, and the live
 * tree's bytes are not spread like its time: two thirds of them go in the
 * first third of the copy, and a bar of bytes would stand near the end for
 * the half minute of small files and links. The time shown reaches 0 as
 * the copy ends, so the fraction reaches 1.
 *
 * THE TIME SHOWN COUNTS DOWN WHILE THE COPY MOVES, AND ONCE SHOWN IT NEVER
 * RISES (copy_hold). Each refit moves the estimate, and a clock that
 * climbs while bytes and files go by reads as a copy going backwards, one
 * that stands still as a copy that has stopped.
 *
 * NO FIGURE IS SHOWN UNTIL THE ESTIMATE SETTLES, and the note reads
 * "estimating time left". The first fits are low on a slower disk and on
 * a tree whose small files have not started, and high behind a slow
 * start, and a figure shown from them would have to rise, or stand, to
 * meet the copy. The estimate has settled once the note's own fraction,
 * the seconds so far over those and the estimate, is KI_COPY_START, and
 * the estimates that stood over the last KI_COPY_SETTLE_S seconds lie
 * within KI_COPY_SPREAD above the lowest of them; or once that fraction
 * is KI_COPY_SETTLED whatever they do. That fraction is of the estimate:
 * the forced settle comes before a fifth of the copy while the estimate
 * is at most 1.31 times the real time left (0.16r/(0.84 + 0.16r) of the
 * copy for an estimate r times it), and an estimate higher than that
 * which still spreads by more than KI_COPY_SPREAD leaves no figure shown
 * past a fifth. The first figure is the estimate then, and from it
 * on only the countdown moves it. While no figure is shown the fraction
 * done is of the estimate, so the bar moves with the copy.
 *
 * THE COUNTDOWN IS SET AGAINST THE ESTIMATE, the highest of those that
 * stood over the last KI_COPY_EST_S seconds. Above it, the time shown
 * falls to it, at least as fast as the clock and at most KI_COPY_FAST
 * times as fast, so a high first figure, or one an estimate has fallen
 * below, is corrected within a second or two wherever it came from.
 * Below it, the time shown counts at the clock while it is within
 * KI_COPY_NEAR of it, and slower the further below, down to KI_COPY_SLOW
 * of the clock from KI_COPY_FAR below it on, which lets the copy catch up
 * with a figure that is low. An estimate a little above the time shown is
 * as often a slow stretch of the copy as a copy slower than shown, and
 * the clock keeps the figure from following it up; one well above is the
 * copy's own pace. The time shown never rises.
 *
 * NO WHOLE SECOND STANDS FOR TWO SECONDS WHILE THE COPY MOVES, which a
 * front end framing once a second would show as three equal frames. rsync
 * prints a line as each file ends and once a second through a large file,
 * so the reader (run_full) ticks each KI_COPY_TICK_S without a line, and
 * a tick (ki_copy_tick) counts the time shown down from the last line's
 * estimate as a line would. A whole second then stands for at most
 * 1/KI_COPY_SLOW + KI_COPY_TICK_S seconds, under two. A copy whose bytes
 * and files have not advanced for KI_COPY_QUIET_S, longer than rsync's
 * second between running lines, is not moving, and keeps its time shown.
 *
 * The time shown is never below one second until the copy's last line
 * (`to-chk=0/N`, or no bytes and no files left), which shows 0:00:00: an
 * estimate that reaches 0 before rsync has finished would show 0:00:00 on
 * a copy still running, and the end of the copy would not be told apart
 * from a stall.
 */
int ki_copy_note(const char *line, double el, unsigned long long total,
		 unsigned long long links, const KiCopyProfile *prof,
		 KiCopyFiles *files, double *frac, char *note, size_t n)
{
	/* 10,544,196,181  45%  45.67MB/s    0:00:12 (xfr#123, to-chk=456/789) */
	char size[32], pct[16];
	const char *chk;
	long fleft = -1, ftotal = 0;
	unsigned long long done;
	double t = -1, btot = 0, bleft = -1, fdone = -1, fl = -1, ft = 0;
	int p;

	if (sscanf(line, "%31s %15s", size, pct) != 2 || !strchr(pct, '%') ||
	    !isdigit((unsigned char)size[0]))
		return 0;
	done = rsync_bytes(size);
	p = atoi(pct);
	chk = strstr(line, "to-chk=");
	if (chk && sscanf(chk + 7, "%ld/%ld", &fleft, &ftotal) != 2)
		fleft = -1;
	if (fleft >= 0 && ftotal >= fleft) {
		if (files) {
			files->left = fleft;
			files->total = ftotal;
		}
	} else if (files && files->total > 0) {
		fleft = files->left;
		ftotal = files->total;
	} else {
		fleft = -1;
	}
	if (fleft >= 0 && prof && prof->ok && ftotal > 0) {
		ft = (double)ftotal * prof->per_entry;
		fdone = ft * copy_prof_at(prof, (double)(ftotal - fleft) /
							(double)ftotal);
		fl = ft - fdone;
	} else if (fleft >= 0) {
		ft = (double)ftotal;
		fdone = (double)(ftotal - fleft);
		fl = (double)fleft;
	}

	if (p >= 10 && p < 100)
		btot = done * 100.0 / (p + 0.5) - (double)links;
	else if (total)
		btot = total > links ? (double)(total - links) : (double)total;
	else if (p > 0)
		btot = done * 100.0 / (p + 0.5) - (double)links;
	if (btot > 0 && btot < done)
		btot = (double)done;
	if (btot > 0)
		bleft = btot - done;
	*frac = btot > 0 ? done / btot : p / 100.0;
	if (*frac > 1)
		*frac = 1;
	if (files && el >= 0 && (chk || fleft < 0))
		copy_sample(files, el, (double)done, fdone >= 0 ? fdone : 0);

	if (el >= 1) {
		/* The first sample is the first progress line. */
		int first = files && files->n;
		double run = first ? el - files->el[0] : el;
		double b0 = first ? files->bytes[0] : 0;
		double f0 = first ? files->files[0] : 0;

		if (files && bleft >= 0)
			t = copy_fit(files, bleft, btot, fl, ft);
		if (!files && run >= 1 && bleft >= 0 && done > b0) {
			t = run * bleft / (done - b0);
			if (fl >= 0 && fdone > f0 && run * fl / (fdone - f0) < t)
				t = run * fl / (fdone - f0);
		}
		if (files && t >= 0) {
			double e = t;

			files->est = e;
			t = copy_hold(files, e, el, (double)done, fdone,
				      fleft == 0 || (bleft <= 0 && fl <= 0), 1);
			*frac = el / (el + (t >= 0 ? t : e));
		}
	}
	if (files) {
		if (*frac < files->frac)
			*frac = files->frac;
		files->frac = *frac;
	}
	copy_text((double)done, el, total, links, t, note, n);
	return 1;
}

int ki_copy_tick(double el, unsigned long long total,
		 unsigned long long links, KiCopyFiles *files, double *frac,
		 char *note, size_t n)
{
	double t;

	if (!files->shown_ok || !files->counting || files->shown <= 0 ||
	    el - files->moved_el >= KI_COPY_QUIET_S)
		return 0;
	t = copy_hold(files, files->est, el, files->shown_bytes,
		      files->shown_files, 0, 0);
	*frac = el / (el + t);
	if (*frac < files->frac)
		*frac = files->frac;
	files->frac = *frac;
	copy_text(files->shown_bytes, el, total, links, t, note, n);
	return 1;
}

/* KINSTALL_COPY_TRACE=<file>: every line a copy's rsync prints is appended
 * to the file as `<seconds since the copy started> <line>`, after a
 * `# <copy> total=<bytes> links=<bytes>` line naming the copy and the
 * figures its time left is given, and a `# prof <per_entry> <at[0]>…`
 * line holding the walk's profile. That is the stream `harness copy`
 * reads, so a copy on real hardware replays in the selftest as it ran. */
static FILE *rsync_trace;

static void copy_trace_start(const char *what)
{
	const char *path = getenv("KINSTALL_COPY_TRACE");

	if (rsync_trace)
		fclose(rsync_trace);
	rsync_trace = path && *path ? fopen(path, "ae") : NULL;
	if (!rsync_trace)
		return;
	fprintf(rsync_trace, "# %s total=%llu links=%llu\n", what,
		rsync_total_bytes, rsync_link_bytes);
	if (rsync_prof && rsync_prof->ok) {
		fprintf(rsync_trace, "# prof %.6f", rsync_prof->per_entry);
		for (int i = 0; i <= KI_COPY_PROF; i++)
			fprintf(rsync_trace, " %.6f", (double)rsync_prof->at[i]);
		fputc('\n', rsync_trace);
	}
}

static void copy_trace_end(void)
{
	if (rsync_trace)
		fclose(rsync_trace);
	rsync_trace = NULL;
}

static void rsync_parse(const char *line)
{
	char note[128];

	if (rsync_trace)
		fprintf(rsync_trace, "%.3f %s\n", kb_now_s() - rsync_t0, line);
	if (ki_copy_note(line, kb_now_s() - rsync_t0, rsync_total_bytes,
			 rsync_link_bytes, rsync_prof, &rsync_files,
			 &rsync_total_pct, note, sizeof(note))) {
		emit('P', "%.4f", rsync_total_pct);
		emit('N', "%s", note);
		return;
	}
	if (line[0] && line[0] != ' ')
		logf_("%s", line);
}

static void rsync_tick(void)
{
	char note[128];

	if (ki_copy_tick(kb_now_s() - rsync_t0, rsync_total_bytes,
			 rsync_link_bytes, &rsync_files, &rsync_total_pct, note,
			 sizeof(note))) {
		emit('P', "%.4f", rsync_total_pct);
		emit('N', "%s", note);
	}
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
	if (ki_target_check(why, sizeof(why)))
		fail("%s", why);

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
	/* The copy's source is this root, so the Bootloader step finds the
	 * payload here or nowhere; asked after the erase, a missing one leaves
	 * a wiped disk that does not start. A rehearsal places nothing. */
	if (!cfg.dry_run && !kb_path_exists("/usr/share/limine"))
		fail("no Limine payload at /usr/share/limine — the disk would "
		     "not start");
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
	int rc = run_full(sf, layout, NULL, NULL, ok0, 1);
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
	memset(&rsync_files, 0, sizeof(rsync_files));
	rsync_total_bytes = ki_need_bytes();
	rsync_link_bytes = cfg.with_appbox ? ki_sys.link_extra_bytes
					   : ki_sys.link_extra_bytes -
						     ki_sys.link_extra_appbox_bytes;
	rsync_prof = cfg.with_appbox ? &ki_sys.copy_prof : &ki_sys.copy_prof_sys;
	rsync_t0 = kb_now_s();
	copy_trace_start("root");

	/* 24 is "some files vanished before transfer" — on a live system with
	 * a running session that is the normal case, not a failure. */
	static const int ok[] = { 0, 24 };
	int rc = run_full(argv, NULL, rsync_parse, rsync_tick, ok, 2);
	copy_trace_end();
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

/* Whether the release's binhost index is in the target's cache: fetched once,
 * by the first step that needs it, and read by the next. -1 is not yet asked. */
static int index_state = -1;
/* Set when the fetch found no binhost named at all — an image built from a
 * tree that names no release, with no `binhost =` in update.conf — which is
 * no network failure, and the warnings must not read as one. */
static int index_unnamed;
/* Set when `kdos update fetch` found no network at all (its "the network
 * could not be reached" line): the warning then says so, rather than sending
 * the account to the log for the one fact it needs. */
static int index_offline;

static void index_parse(const char *line)
{
	logf_("%s", line);
	if (strstr(line, "no binhost is named"))
		index_unnamed = 1;
	if (strstr(line, "the network could not be reached"))
		index_offline = 1;
}

/* Why there is no index, as the head of a warning. */
static const char *index_why(void)
{
	if (index_unnamed)
		return "this image names no binary host";
	if (index_offline)
		return "no network was found, so the release's binary host "
		       "index could not be fetched";
	return "the release's binary host index could not be fetched "
	       "(/var/log/kinstall.log says why)";
}

/* THE CACHE IS THE TARGET'S OWN: the live system's is in RAM, and a group of a
 * few gigabytes downloaded there would fill it. The installed machine then
 * starts with its release's index in place. KDOS_UPDATE_CACHE stays set until
 * the Firmware step ends, so every `kdos` between reads and writes there. */
static int fetch_index(void)
{
	char cache[512];

	snprintf(cache, sizeof(cache), "%s/var/lib/kdos/binhost", TARGET);
	setenv("KDOS_UPDATE_CACHE", cache, 1);
	if (index_state < 0) {
		char *fetch[] = { "kdos", "update", "fetch", "--binhost-only",
				  NULL };

		static const int ok[] = { 0 };

		mkpath("%s", cache);
		index_state = run_full(fetch, NULL, index_parse, NULL, ok, 1) == 0;
	}
	return index_state;
}

/*
 * The chosen applications.
 *
 * NOTHING IS BAKED ONTO THE MEDIUM, and nothing is built here: `kdos-appbox`
 * has no target root, so an import or a build run now would land in the live
 * session's pack store and container storage and end with the medium. The
 * selection is recorded in the target's /var/lib/kdos/apps-pending, and the
 * first login offers to build it; `kdos app install --pending` does the same
 * from a terminal.
 *
 * EITHER WAY THE STORE'S DIRECTORIES ARE MADE. kdos-packd sets the staging
 * mode at startup, but a first boot that inherited 0755 would refuse an import
 * until the daemon had run once — which reads as the feature not working.
 */
/*
 * THE NATIVE GROUPS GO IN NOW, when the network answers. The live system
 * takes its own release's binhost index (`kdos update fetch --binhost-only`:
 * a newer release's packages are built against a newer base than the one
 * being installed), and `kdos update install --root <target> <id>` puts each
 * chosen group's packages into the new system, prebuilt, with its world set
 * and origin file under the target, so the installed machine updates them
 * like anything else installed on purpose. A group that does not go in, and
 * every group when the index cannot be had, is written to the target's
 * /var/lib/kdos/groups-pending for `kdos update install --pending` (as
 * ki_root_cmd names it to the account): a warning, never a failure, since
 * the system is installed and boots without it.
 */
static void do_native(void)
{
	KbBuf b = {0};
	char path[512];
	int n = ki_groups_chosen(), pend = 0, online;

	if (!n) {
		emit('L', "no native groups chosen");
		return;
	}
	online = fetch_index();
	if (!online)
		emit('W', "%s; the chosen groups are recorded and install "
			  "later with %s", index_why(),
		     ki_root_cmd("kdos update install --pending"));
	kb_buf_printf(&b, "# Native groups chosen during the install and not "
			  "yet installed.\n# %s installs them.\n",
		      ki_root_cmd("kdos update install --pending"));
	for (int i = 0; i < ki_nnative; i++) {
		if (!ki_native[i].chosen)
			continue;
		if (online) {
			char *inst[] = { "kdos", "update", "install", "--prebuilt",
					 "--root", TARGET, ki_native[i].id,
					 NULL };
			emit('L', "%s group %s (%s)",
			     cfg.dry_run ? "would install" : "installing",
			     ki_native[i].id, ki_native[i].title);
			if (run(inst) == 0)
				continue;
			emit('W', "group %s did not install; it is recorded for "
				  "%s", ki_native[i].id,
			     ki_root_cmd("kdos update install --pending"));
		}
		kb_buf_printf(&b, "%s\n", ki_native[i].id);
		pend++;
	}
	if (pend) {
		snprintf(path, sizeof(path), "%s/var/lib/kdos/groups-pending",
			 TARGET);
		mkpath("%s/var/lib/kdos", TARGET);
		if (!cfg.dry_run && kb_write_all(path, b.p, b.n) < 0)
			emit('W', "the pending groups could not be recorded in "
				  "%s — install them from Discover after the "
				  "first login", path);
	}
	kb_buf_free(&b);
}

static void do_apps(void)
{
	KbBuf b = {0};
	char path[512];
	int n = 0;

	mkpath("%s/var/lib/kdos/packs", TARGET);
	mkpath("%s/var/lib/kdos/packs/staging", TARGET);
	mkpath("%s/var/lib/kdos/packs/mnt", TARGET);
	if (!cfg.dry_run)
		chmod(TARGET "/var/lib/kdos/packs/staging", 01777);

	for (int i = 0; i < ki_ngroup; i++)
		if (ki_group[i].chosen)
			n++;
	if (ki_apps_route() == APPS_NONE || !n) {
		emit('L', "no applications chosen");
		do_native();
		emit('P', "1");
		return;
	}

	kb_buf_printf(&b, "# Chosen during the install and not yet "
			  "built.\n# `kdos app install --pending` "
			  "builds them.\n");
	for (int i = 0; i < ki_ngroup; i++)
		if (ki_group[i].chosen)
			kb_buf_printf(&b, "%s\n", ki_group[i].id);
	snprintf(path, sizeof(path), "%s/var/lib/kdos/apps-pending", TARGET);
	mkpath("%s/var/lib/kdos", TARGET);
	if (!cfg.dry_run && kb_write_all(path, b.p, b.n) < 0)
		emit('W', "the chosen applications could not be recorded in %s "
			  "— install them from Discover after the first login",
		     path);
	else
		emit('L', "%s %d group(s) for the first login: %s",
		     cfg.dry_run ? "would record" : "recorded", n, cfg.apps);
	kb_buf_free(&b);
	do_native();
	emit('P', "1");
}

/*
 * THE FIRMWARE THIS MACHINE NEEDS AND THE MEDIUM DOES NOT CARRY. The base
 * image holds the common vendors' firmware; `kdos firmware sync --root
 * <target>` reads this machine's devices (the live system runs on the same
 * hardware) and installs the firmware packages they name into the target,
 * prebuilt, from the index the Packs step fetched or this step fetches now.
 * Each line it prints is logged, so the log says what went in. Offline, a
 * failure, or a `kdos` without the command is a warning and never a failure:
 * the system boots on the medium's firmware, and `kdos firmware sync` as
 * root after the first boot does the rest (ki_root_cmd names it).
 */
static void do_firmware(void)
{
	char *sync[] = { "kdos", "firmware", "sync", "--root", TARGET, NULL };

	if (!fetch_index()) {
		emit('W', "%s, so no firmware beyond the medium's was "
			  "installed; run %s after the first boot",
		     index_why(), ki_root_cmd("kdos firmware sync"));
	} else if (run(sync) != 0) {
		emit('W', "`kdos firmware sync` did not finish; run %s after "
			  "the first boot", ki_root_cmd("kdos firmware sync"));
	} else {
		emit('L', "%s the firmware this machine's devices name",
		     cfg.dry_run ? "would install" : "installed");
	}
	unsetenv("KDOS_UPDATE_CACHE");
	emit('P', "1");
}

/*
 * THE SWAP FILE, made before fstab names it, and named only once it is whole.
 * How a swapfile is made depends on the filesystem under it, and getting it
 * wrong does not fail here — it fails at the next boot's `swapon -a`, with
 * the machine simply having no swap and nothing saying why.
 *
 * fallocate leaves unwritten extents, so xfs and f2fs get a file written out
 * in full, which no kernel's swapon can refuse for them. btrfs needs the file
 * NOCOW and uncompressed, which is the whole reason `btrfs filesystem
 * mkswapfile` exists.
 *
 * A FILE THAT COULD NOT BE MADE WHOLE IS DELETED, never given to mkswap: a
 * root that ran out of room part way leaves a short file, and a short swap
 * file is a machine with less swap than it was promised and a full root.
 * The machine is installed without swap and a warning says so. 1 when the
 * file is made.
 */
static int make_swapfile(void)
{
	char sz[32];
	int rc;
	int mode = ki_fs(cfg.fstype)->swapfile;

	emit('N', "swapfile (%ld MiB)", cfg.swap_mb);
	snprintf(sz, sizeof(sz), "%ldM", cfg.swap_mb);
	if (mode == KI_SWAPFILE_FALLOCATE && !kb_have_prog("fallocate"))
		mode = KI_SWAPFILE_DD;
	if (mode == KI_SWAPFILE_BTRFS && !kb_have_prog("btrfs"))
		mode = KI_SWAPFILE_DD;

	if (mode == KI_SWAPFILE_BTRFS) {
		char *bs[] = { "btrfs", "filesystem", "mkswapfile", "-s", sz,
			       TARGET "/swapfile", NULL };
		rc = run(bs);
	} else {
		if (mode == KI_SWAPFILE_FALLOCATE) {
			char *fa[] = { "fallocate", "-l", sz,
				       TARGET "/swapfile", NULL };
			rc = run(fa);
		} else {
			char cnt[32];
			snprintf(cnt, sizeof(cnt), "count=%ld", cfg.swap_mb);
			char *dd[] = { "dd", "if=/dev/zero",
				       "of=" TARGET "/swapfile", "bs=1M", cnt,
				       NULL };
			rc = run(dd);
		}
		if (!rc) {
			char *mks[] = { "mkswap", TARGET "/swapfile", NULL };
			if (!cfg.dry_run)
				chmod(TARGET "/swapfile", 0600);
			rc = run(mks);
		}
	}
	if (!rc)
		return 1;
	if (!cfg.dry_run)
		unlink(TARGET "/swapfile");
	emit('W', "the %ld MiB swap file could not be made (exit %d), so this "
		  "machine has no swap; the log says why", cfg.swap_mb, rc);
	return 0;
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
	/* The root is always formatted, so what a dry run reads from it is the
	 * filesystem mkfs would replace: only a real run has the UUID to log. */
	if (root_uuid[0] && !cfg.dry_run)
		logf_("root UUID %s", root_uuid);
	else
		logf_("root UUID: read back from %s after mkfs, which a dry "
		      "run does not run", part_root);

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
	if (cfg.swap == SWAP_FILE && cfg.swap_mb > 0 && !ki_ab() &&
	    make_swapfile())
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
	uid_t uid;
	gid_t gid;

	snprintf(home, sizeof(home), "%s/home/%s", TARGET, user);
	ki_account_ids(TARGET, user, &uid, &gid);
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
		/* No account: the first person makes one in plasma-setup, and
		 * the account it makes is an administrator. Root keeps the
		 * image's lock unless the answers gave it a password. */
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

static int run_seed(const KbArgv *a)
{
	return run((char *const *)a->v);
}

/*
 * THE ACCENT AND THE DESK. The machine's half is files the first login reads:
 * the global theme and the desk in /etc/xdg, the splash's and the console's
 * accent, and the login screen's; startplasma applies the global theme in
 * Defaults mode at the first login, and the theme's layout script lays out the
 * desk. The account's half is ki_seed_accent's two runs of `kdos theme
 * --account`: into /etc/skel, so every account made later starts in the
 * accent, and into the new account's home, which fill_home filled before the
 * accent was written and which a text console or an SSH login reads long
 * before any Plasma login would rewrite it. An OEM install has no account
 * yet: skel alone. A run that fails is a warning, not a failed install — the
 * first Plasma login brings the account into step on its own.
 */
static void do_theme(void)
{
	char why[256];

	emit('N', "%s, %s desk", cfg.theme, cfg.desk);
	if (ki_write_look(TARGET, why, sizeof(why)))
		fail("%s", why);
	emit('P', "0.4");
	if (ki_seed_accent(TARGET, "kdos", run_seed))
		emit('W', "a home did not get all of the %s accent; the first "
			  "Plasma login brings it into step", cfg.theme);
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
	rsync_total_pct = 0;
	memset(&rsync_files, 0, sizeof(rsync_files));
	rsync_t0 = kb_now_s();
	rsync_total_bytes = (ki_sys.payload_kb > ki_sys.appbox_kb
				     ? ki_sys.payload_kb - ki_sys.appbox_kb
				     : 0) * 1024;
	rsync_link_bytes = ki_sys.link_extra_bytes -
			   ki_sys.link_extra_appbox_bytes;
	rsync_prof = &ki_sys.copy_prof_sys;
	copy_trace_start("slot-b");
	if (run_full(argv, NULL, rsync_parse, rsync_tick, ok, 1))
		fail("rsync into slot B failed");
	copy_trace_end();

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
		} else {
			logf_("would write %s/etc/fstab with slot B's root UUID, "
			      "read back after mkfs", TARGET_B);
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

/* One KEY="value" of an os-release-shaped file, the quotes taken off, and
 * only printable ASCII other than a quote or a backslash kept: the value is
 * written back between quotes, and kdos-bootctl reads it into JSON. */
static void release_value(const char *data, const char *key, char *out,
			  size_t n)
{
	size_t kl = strlen(key), o = 0;

	out[0] = 0;
	for (const char *l = data; l && *l;) {
		const char *nl = strchr(l, '\n');
		const char *end = nl ? nl : l + strlen(l);

		if ((size_t)(end - l) > kl && !strncmp(l, key, kl) &&
		    l[kl] == '=') {
			for (const char *v = l + kl + 1; v < end && o + 1 < n; v++) {
				unsigned char ch = (unsigned char)*v;
				if (ch < 0x20 || ch > 0x7e || ch == '"' ||
				    ch == '\'' || ch == '\\')
					continue;
				out[o++] = *v;
			}
			out[o] = 0;
			return;
		}
		l = nl ? nl + 1 : NULL;
	}
}

/*
 * WHAT A SLOT HOLDS, beside its kernel: EFI/kdos/<slot>/release, in the shape
 * `kdos-bootctl deploy` writes for every later kernel, from the root's
 * /etc/kdos-release. A slot that is not running is named only by this file
 * (`kdos-bootctl status`, System Versions, the rollback dialog), because its
 * root is not mounted; slot B holds a copy of slot A's root, so both get
 * slot A's record. An image built without a commit carries no
 * /etc/kdos-release and gets no record, and the running slot is then named
 * from its own root.
 */
static void write_slot_release(const char *slot)
{
	static const char *const keys[] = { "VERSION", "BUILD_ID", "BUILD_DATE",
					    "TREE", "TREE_TIME" };
	char path[256], v[128];
	char *rel;
	KbBuf b = {0};

	snprintf(path, sizeof(path), TARGET "/boot/efi/EFI/kdos/%s/release",
		 slot);
	if (cfg.dry_run) {
		logf_("write %s from %s/etc/kdos-release", path, TARGET);
		return;
	}
	if (!(rel = kb_read_all(TARGET "/etc/kdos-release", NULL))) {
		logf_("no %s/etc/kdos-release: slot %s's version is not "
		      "recorded on the ESP", TARGET, slot);
		return;
	}
	kb_buf_printf(&b, "# What slot %s holds. Written by the installer, and "
			  "by kdos-bootctl deploy\n# from that root's "
			  "/etc/kdos-release.\n", slot);
	for (size_t i = 0; i < sizeof(keys) / sizeof(keys[0]); i++) {
		release_value(rel, keys[i], v, sizeof(v));
		if (!strcmp(keys[i], "TREE_TIME") && !v[0])
			kb_strlcpy(v, "0", sizeof(v));
		kb_buf_printf(&b, "%s=\"%s\"\n", keys[i], v);
	}
	free(rel);
	if (kb_write_file(path, b.p) < 0)
		emit('W', "cannot write %s: %s — slot %s shows no version until "
			  "an update deploys it", path, strerror(errno), slot);
	else
		logf_("write %s", path);
	kb_buf_free(&b);
}

/* The release the target's /etc/kdos-release names, without its `+<build>`:
 * "0.2.1", or "" when the root has no record. The first menu names it in the
 * shape kdos-bootctl's menu carries every later boot. */
static void target_release(char *out, size_t n)
{
	char *rel = kb_read_all(TARGET "/etc/kdos-release", NULL);

	out[0] = 0;
	if (!rel)
		return;
	release_value(rel, "VERSION", out, n);
	out[strcspn(out, "+")] = 0;
	free(rel);
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
	/* The release in the first entry's comment and in slot B's title,
	 * the words kdos-bootctl's menu_sync writes from the slot's release
	 * record: "KDOS 0.2.1 (slot B)", or "KDOS (slot B)" with none. */
	char ver[64], a_note[96] = "", b_note[96] = "KDOS ";
	target_release(ver, sizeof(ver));
	if (ver[0]) {
		snprintf(a_note, sizeof(a_note), ", KDOS %s", ver);
		snprintf(b_note, sizeof(b_note), "KDOS %s ", ver);
	}

	emit('N', "kernel and initramfs onto the ESP");
	mkpath(TARGET "/boot/efi/EFI/kdos/a");
	copy_file(kb_path_exists(TARGET "/boot/initramfs-kdos.cpio.gz")
			  ? TARGET "/boot/initramfs-kdos.cpio.gz"
			  : TARGET "/boot/initramfs.cpio.gz",
		  TARGET "/boot/efi/EFI/kdos/a/initramfs.cpio.gz");
	copy_file(TARGET "/boot/vmlinuz-kdos",
		  TARGET "/boot/efi/EFI/kdos/a/vmlinuz");
	write_slot_release("a");
	/* Slot B boots the kernel of the root it was filled from, which is the
	 * same kernel; it gets its own copy because every later update replaces
	 * one slot's pair and never the other's. */
	if (part_b[0]) {
		mkpath(TARGET "/boot/efi/EFI/kdos/b");
		copy_file(TARGET "/boot/efi/EFI/kdos/a/initramfs.cpio.gz",
			  TARGET "/boot/efi/EFI/kdos/b/initramfs.cpio.gz");
		copy_file(TARGET "/boot/efi/EFI/kdos/a/vmlinuz",
			  TARGET "/boot/efi/EFI/kdos/b/vmlinuz");
		write_slot_release("b");
		snprintf(b_entry, sizeof(b_entry),
			 "\n"
			 "/%s(slot B)\n"
			 "    comment: The other version, for one boot; nothing "
			 "changes\n"
			 "    protocol: linux\n"
			 "    path: boot():/EFI/kdos/b/vmlinuz\n"
			 "    module_path: boot():/EFI/kdos/b/initramfs.cpio.gz\n"
			 "    cmdline: kdos_slot=b %s%sroot=UUID=%s rw "
			 "console=tty0 " KI_QUIET "\n",
			 b_note, slot_opt, crypt_opt, b_uuid);
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
	   "    comment: Start this computer (slot A%s)\n"
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
	   theme, paper, fontline, a_note,
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
	 * THE CONSOLE MAP FOR THE DISK PASSPHRASE, beside the boot state: the
	 * initramfs reads EFI/kdos/keymap while it has the ESP mounted for
	 * `select` and loads that map on tty1 before it asks. It is one image
	 * for every machine and runs before the root holding /etc/keymap is
	 * open, so without this file the prompt types with the kernel's US map
	 * and a passphrase set on any other layout cannot be entered. The
	 * name is the one the keyboard step resolved into /etc/keymap, which
	 * runs before this one; kdos-settingsd keeps the two in step after.
	 */
	if (ki_keymap_name_ok(cfg.keymap))
		wr("/boot/efi/EFI/kdos/keymap", "%s\n", cfg.keymap);
	else
		emit('W', "the console keymap '%s' is not a kbd map name — the "
			  "disk passphrase prompt types with the US map",
		     cfg.keymap);

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
	/* THE ERASED DISK IS LET GO: its volume group deactivated and its
	 * container closed, as Prepare does before it writes. Left active, the
	 * group's name stands on this disk, and an install onto another disk
	 * from the same live session refuses it as a clash. Only the erase
	 * plan's disk is wholly the install's; a reused disk may hold volumes
	 * this session still uses. */
	if (cfg.plan == PLAN_WIPE)
		release_disk(cfg.disk);
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
		do_apps, do_firmware, do_config, do_accounts, do_theme, do_slotb, do_boot,
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
