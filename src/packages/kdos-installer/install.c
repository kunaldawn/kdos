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
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/stat.h>
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
	S_PACKS,
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
	{ "Format",      "encrypt if asked, then create the filesystems" },
	{ "Mount",       "attach the target at /mnt" },
	{ "Copy system", "the live tree, verbatim" },
	{ "Packs",       "the applications chosen from the medium" },
	{ "Configure",   "fstab, hostname, keymap, services" },
	{ "Accounts",    "users, passwords, autologin" },
	{ "Theme",       "regenerate the accent for the new home" },
	{ "Bootloader",  "rEFInd on the ESP" },
	{ "Finish",      "flush and unmount" },
};

/* ════════════════════════════════════════════════════════════════════════
 * Parent side
 * ════════════════════════════════════════════════════════════════════════ */

static int step_skipped(int i)
{
	if (i == S_PARTITION && cfg.plan != PLAN_WIPE)
		return 1;
	if (i == S_THEME && !strcmp(cfg.theme, "phosphor"))
		return 1;
	/* Nothing chosen, or a medium with no index on it — the step says
	 * SKIPPED rather than running and copying nothing, because a step that
	 * always succeeds having done nothing is a step nobody reads. */
	if (i == S_PACKS && (!ki_packs_present || ki_packs_bytes() == 0))
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
static char part_esp[96], part_root[96], part_swap[96];

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

/* Rewrite one colon-separated database in place, field by field. Renaming
 * the live user touches passwd, shadow, group (as a member AND as the
 * primary group name) and con.conf's `autologin` — miss any one of them and
 * the installed system logs nobody in. */
static void rewrite_accounts(const char *oldu, const char *newu,
			     const char *fullname, const char *userhash,
			     const char *roothash)
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

	/*
	 * con.conf's `autologin`: tty1 logs in the account this key names, so
	 * a rename has to reach it. It is the ONLY place the desktop's account
	 * is named — `/etc/inittab` runs `kdos-getty tty1 kdos-con-login tty1`
	 * and carries no account at all — so missing this key leaves the key
	 * naming a user the installed system does not have and the machine
	 * reachable only from tty2.
	 *
	 * Edited in place and after the `greet` rewrite, for the same reason
	 * that one is: the shipped file is mostly the explanation of what each
	 * key does, and replacing it wholesale leaves a configuration file
	 * nobody can read.
	 */
	snprintf(path, sizeof(path), "%s/etc/kdos/con.conf", TARGET);
	if (strcmp(oldu, newu) && slurp(path, buf, sizeof(buf)) > 0) {
		size_t o = 0;
		int done = 0;

		out[0] = 0;
		for (char *line = strtok(buf, "\n"); line;
		     line = strtok(NULL, "\n")) {
			const char *p = line;

			while (*p == ' ' || *p == '\t')
				p++;
			if (!strncmp(p, "autologin", 9) &&
			    (p[9] == ' ' || p[9] == '\t' || p[9] == '=')) {
				o += (size_t)snprintf(out + o, sizeof(out) - o,
						      "autologin = %s\n", newu);
				done = 1;
				continue;
			}
			o += (size_t)snprintf(out + o, sizeof(out) - o, "%s\n",
					      line);
			if (o >= sizeof(out) - 64)
				break;
		}
		if (!done)
			snprintf(out + o, sizeof(out) - o, "autologin = %s\n",
				 newu);
		if (!cfg.dry_run && kb_write_file(path, out) < 0)
			fail("cannot write %s", path);
		logf_("updated %s (autologin -> %s)", path, newu);
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

	if (cfg.swap == SWAP_PART && cfg.swap_mb > 0)
		snprintf(layout, sizeof(layout),
			 "label: gpt\n,%ldM,U\n,%ldM,S\n,,L\n", esp_mb, cfg.swap_mb);
	else
		snprintf(layout, sizeof(layout),
			 "label: gpt\n,%ldM,U\n,,L\n", esp_mb);

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
static char luks_part[64];

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

	logf_("ESP  = %s", part_esp);
	logf_("root = %s", part_root);
	if (luks_part[0])
		logf_("LUKS = %s", luks_part);
	if (part_swap[0])
		logf_("swap = %s", part_swap);
}

static void do_format(void)
{
	resolve_parts();

	if (!part_root[0])
		fail("no root partition resolved");
	if (!cfg.dry_run && !kb_path_exists(part_root))
		fail("%s does not exist", part_root);

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
		if (!cfg.dry_run && !kb_path_exists(part_root))
			fail("%s did not appear after unlocking", part_root);
		emit('P', "0.5");
	}

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
 * WHERE THE PACKS ARE READ FROM, and it cannot be `/mnt/iso`.
 *
 * TARGET is `/mnt` and the live medium is mounted at `/mnt/iso` — so the
 * moment the target root is mounted, the medium is UNDERNEATH it and every
 * path into it resolves inside the filesystem that was just created empty.
 * `do_packs` then reports `cannot copy alpine.kpack` for a file that is
 * sitting on the medium the installer booted from.
 *
 * The medium is bind-mounted somewhere the target cannot cover BEFORE that
 * happens, and `do_packs` reads from there. A bind rather than a second
 * mount of the device because the device is not the installer's to name: it
 * was mounted by the initramfs and MS_MOVEd across switch_root, and the only
 * handle anything has on it is the path.
 */
#define MEDIUM_BIND "/run/kdos-medium"

static char medium_dir[256];

static void bind_medium(void)
{
	const char *src = getenv("KDOS_PACK_MEDIUM");

	if (src && *src) {			/* a fixture names its own */
		kb_strlcpy(medium_dir, src, sizeof(medium_dir));
		return;
	}
	kb_strlcpy(medium_dir, "/mnt/iso/packs", sizeof(medium_dir));
	if (!kb_path_exists("/mnt/iso/packs"))
		return;
	mkpath(MEDIUM_BIND);
	char *b[] = { "mount", "--bind", "/mnt/iso", (char *)MEDIUM_BIND, NULL };
	if (run(b) == 0)
		kb_strlcpy(medium_dir, MEDIUM_BIND "/packs",
			   sizeof(medium_dir));
	else
		logf_("could not bind the medium; packs will be unreadable "
		      "once %s is mounted", TARGET);
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
 * The chosen packs, from the medium into the target's store.
 *
 * A COPY AND NOTHING ELSE. kdos-packd verifies a pack where it MOUNTS it, so
 * an install that hashed and checked signatures here would be doing the work
 * twice and would additionally have to carry libkpack into a program that
 * links three libraries. What this must get right is that the store ends up
 * owned by root and mode 0755, which is the whole of why the daemon does not
 * re-hash what is in it.
 *
 * The base and the runtimes come across whatever was ticked — an application
 * pack is a diff over a runtime and installing one without the other installs
 * something that cannot start.
 */
static void do_packs(void)
{
	const char *dir = medium_dir[0] ? medium_dir : NULL;
	unsigned long long total = ki_packs_bytes(), done = 0;
	int n = 0;

	/* `do_mount` put the medium somewhere the target does not cover; a
	 * plan that never mounts (a dump, a dry run) still has the live one. */
	if (!dir)
		dir = getenv("KDOS_PACK_MEDIUM");
	if (!dir || !*dir)
		dir = "/mnt/iso/packs";
	emit('N', "copying %s of packs", kb_human_size(total));
	mkpath("%s/var/lib/kdos/packs", TARGET);
	mkpath("%s/var/lib/kdos/packs/staging", TARGET);
	mkpath("%s/var/lib/kdos/packs/mnt", TARGET);

	for (int i = 0; i < ki_npack; i++) {
		char src[512], dst[640];

		if (!ki_pack[i].chosen)
			continue;
		snprintf(src, sizeof(src), "%s/%s", dir, ki_pack[i].file);
		snprintf(dst, sizeof(dst), "%s/var/lib/kdos/packs/%s", TARGET,
			 ki_pack[i].file);
		logf_("pack %s -> %s", ki_pack[i].id, dst);
		if (!cfg.dry_run && kb_copy_file(src, dst) != 0)
			fail("cannot copy %s", ki_pack[i].file);
		done += ki_pack[i].size;
		n++;
		emit('P', "%.4f", total ? (double)done / (double)total : 1.0);
	}
	/*
	 * The staging directory is the ONE place an unprivileged download may
	 * land and the daemon sets its mode at startup — but a first boot that
	 * inherited 0755 would refuse a `kdos app install` until the daemon had
	 * run once, which reads as the feature not working.
	 */
	if (!cfg.dry_run)
		chmod(TARGET "/var/lib/kdos/packs/staging", 01777);
	emit('L', "%d pack(s) installed", n);
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

	/* KDOS has no tzdata: musl falls back to UTC unless TZ is set, so the
	 * timezone is a POSIX TZ string in the environment. If a zoneinfo tree
	 * ever ships, the symlink below starts working and this stays valid. */
	wr("/etc/profile.d/20-timezone.sh",
	   "# Written by the KDOS installer.\n"
	   "# KDOS ships no tzdata, so the zone is a POSIX TZ string that musl\n"
	   "# parses directly. Change it here, or run `kinstall` again.\n"
	   "export TZ='%s'\n", cfg.tz);
	if (kb_path_exists("/usr/share/zoneinfo") && cfg.tz_label[0]) {
		char zi[256];
		snprintf(zi, sizeof(zi), "/usr/share/zoneinfo/%s", cfg.tz_label);
		if (kb_path_exists(zi) && !cfg.dry_run) {
			unlink(TARGET "/etc/localtime");
			if (symlink(zi, TARGET "/etc/localtime") == 0)
				logf_("linked /etc/localtime -> %s", zi);
		}
	}

	wr("/etc/keymap", "%s\n", cfg.keymap);

	/*
	 * con.conf's `greet`, edited in place rather than rewritten: the
	 * shipped file is mostly the explanation of what each key does, and a
	 * one-line replacement would leave the installed system with a
	 * configuration file nobody can read. Only the line is replaced; a
	 * file that has none gains one, and a missing file is left missing
	 * because the default already matches what would be written.
	 */
	{
		char cc[8192];
		int n = slurp(TARGET "/etc/kdos/con.conf", cc, sizeof(cc));

		if (n > 0) {
			char out[8192];
			size_t o = 0;
			int done = 0;

			for (char *line = strtok(cc, "\n"); line;
			     line = strtok(NULL, "\n")) {
				const char *p = line;

				while (*p == ' ' || *p == '\t')
					p++;
				if (!strncmp(p, "greet", 5) &&
				    (p[5] == ' ' || p[5] == '\t' ||
				     p[5] == '=')) {
					o += (size_t)snprintf(out + o,
							      sizeof(out) - o,
							      "greet = %s\n",
							      cfg.greet ? "yes"
									: "no");
					done = 1;
					continue;
				}
				o += (size_t)snprintf(out + o, sizeof(out) - o,
						      "%s\n", line);
				if (o >= sizeof(out) - 64)
					break;
			}
			if (!done)
				snprintf(out + o, sizeof(out) - o,
					 "greet = %s\n",
					 cfg.greet ? "yes" : "no");
			wr("/etc/kdos/con.conf", "%s", out);
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

	rewrite_accounts("kdos", cfg.username, cfg.fullname, uhash, rhash);

	if (cfg.user_wheel) {
		mkpath(TARGET "/etc/sudoers.d");
		wr("/etc/sudoers.d/10-wheel",
		   "# Written by the KDOS installer.\n"
		   "%%wheel ALL=(ALL:ALL) ALL\n");
		if (!cfg.dry_run)
			chmod(TARGET "/etc/sudoers.d/10-wheel", 0440);
	}
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

static void do_boot(void)
{
	char root_uuid[64] = "", esp_uuid[64] = "";
	char crypt_opt[128] = "", slot_opt[128] = "";
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
		if (lp.uuid[0])
			snprintf(crypt_opt, sizeof(crypt_opt),
				 "cryptdevice=UUID=%s:%s ", lp.uuid, LUKS_NAME);
		else if (!cfg.dry_run)
			fail("cannot read the LUKS UUID back from %s",
			     luks_part);
		logf_("LUKS UUID %s", lp.uuid);
	}

	const char *refind = "/usr/share/refind";
	if (!kb_path_exists(refind))
		refind = TARGET "/usr/share/refind";
	if (!kb_path_exists(refind))
		fail("rEFInd is not installed — no bootloader to place");

	emit('N', "placing rEFInd on the ESP");
	mkpath(TARGET "/boot/efi/EFI/refind");
	mkpath(TARGET "/boot/efi/EFI/BOOT");
	mkpath(TARGET "/boot/efi/EFI/kdos");

	/*
	 * `-r`, NOT `-a`, AND THE DESTINATION IS WHY. The ESP is vfat, which
	 * has no ownership to preserve: `cp -a` calls chown on every file it
	 * writes there, the kernel answers EPERM for each one, and cp exits 1
	 * with a page of `Operation not permitted` naming the SOURCE paths —
	 * which reads as a permissions problem with rEFInd rather than as a
	 * filesystem that cannot hold what was asked of it. Nothing on an ESP
	 * has a meaningful mode or owner anyway; the mount options decide.
	 */
	char src[320];
	snprintf(src, sizeof(src), "%s/.", refind);
	char *cp[] = { "cp", "-r", src, TARGET "/boot/efi/EFI/refind/", NULL };
	must(cp);

	snprintf(src, sizeof(src), "%s/refind_x64.efi", refind);
	copy_file(src, TARGET "/boot/efi/EFI/BOOT/bootx64.efi");
	emit('P', "0.4");

	/* Kernel and initramfs go ON the ESP. rEFInd can read ext4 only via
	 * its filesystem driver, and a boot that depends on a driver load is a
	 * boot that fails silently after a kernel update. FAT it can always
	 * read, so the menu entry points at paths it is guaranteed to see. */
	emit('N', "kernel and initramfs onto the ESP");
	copy_file(TARGET "/boot/vmlinuz-kdos", TARGET "/boot/efi/EFI/kdos/vmlinuz");
	copy_file(TARGET "/boot/initramfs.cpio.gz",
		  TARGET "/boot/efi/EFI/kdos/initramfs.cpio.gz");

	if (kb_path_exists("/usr/share/kdos/boot/kdos-banner.png"))
		copy_file("/usr/share/kdos/boot/kdos-banner.png",
			  TARGET "/boot/efi/EFI/refind/kdos-banner.png");
	const char *icon = "";
	if (kb_path_exists("/usr/share/kdos/boot/os_kdos.png")) {
		copy_file("/usr/share/kdos/boot/os_kdos.png",
			  TARGET "/boot/efi/EFI/refind/icons/os_kdos.png");
		icon = "    icon /EFI/refind/icons/os_kdos.png\n";
	}

	/* memtest86+ is a payload rather than a program: bad RAM is the one
	 * fault no tool running under an OS can honestly diagnose, because the
	 * OS is in the memory being tested. It has to be BOOTABLE from the
	 * installed machine, not just from the medium — the fault it finds is
	 * usually reported as "this install is unstable" months later. Absent
	 * is a skipped menu entry and not an error. */
	const char *memtest = "";
	if (kb_path_exists("/usr/share/kdos/memtest86plus/memtest.efi")) {
		copy_file("/usr/share/kdos/memtest86plus/memtest.efi",
			  TARGET "/boot/efi/EFI/kdos/memtest.efi");
		memtest = "\nmenuentry \"Memory Test (memtest86+)\" {\n"
			  "    loader /EFI/kdos/memtest.efi\n"
			  "}\n";
	}
	emit('P', "0.7");

	wr("/boot/efi/EFI/refind/refind.conf",
	   "# Written by the KDOS installer.\n"
	   "timeout 5\n"
	   "banner /EFI/refind/kdos-banner.png\n"
	   "banner_scale noscale\n"
	   "hideui hints,badges\n"
	   "showtools reboot, shutdown, firmware\n"
	   "use_graphics_for linux\n"
	   "scanfor manual,internal,external,optical\n"
	   "\n"
	   "menuentry \"KDOS\" {\n"
	   "    loader /EFI/kdos/vmlinuz\n"
	   "    initrd /EFI/kdos/initramfs.cpio.gz\n"
	   "    options \"%s%sroot=UUID=%s rw console=tty0 quiet loglevel=3\"\n"
	   "%s"
	   "    submenuentry \"Verbose boot\" {\n"
	   "        options \"%s%sroot=UUID=%s rw console=tty0 loglevel=7\"\n"
	   "    }\n"
	   "    submenuentry \"Single user\" {\n"
	   "        options \"%s%sroot=UUID=%s rw console=tty0 loglevel=7 single\"\n"
	   "    }\n"
	   "}\n"
	   "%s",
	   slot_opt, crypt_opt, root_uuid, icon, slot_opt, crypt_opt, root_uuid,
	   slot_opt, crypt_opt, root_uuid, memtest);

	/*
	 * The initial boot state, so the machine starts life as slot A with
	 * nothing to roll back to. An updater that installs into the other
	 * partition later fills in slot_b and calls `kdos-bootctl try b`; the
	 * initramfs already knows how to count and roll back either way.
	 *
	 * Written straight rather than through kdos-bootctl: this runs from the
	 * live image against a target at /mnt, and the tool's default path is
	 * the RUNNING system's ESP. One file, four lines, and the format is in
	 * bootctl.c.
	 */
	if (esp_uuid[0]) {
		mkpath(TARGET "/boot/efi/EFI/kdos");
		wr("/boot/efi/EFI/kdos/bootstate",
		   "# KDOS boot state. Written by the installer; maintained by\n"
		   "# kdos-bootctl.\n"
		   "slot_a   = %s\n"
		   "slot_b   = \n"
		   "active   = a\n"
		   "try      = \n"
		   "attempts = 0\n",
		   root_uuid);
	}

	/* Fallback path for firmware that ignores everything but BOOTX64. */
	wr("/boot/efi/EFI/BOOT/refind.conf",
	   "include /EFI/refind/refind.conf\n");

	/* And the auto-detection file, for anyone who later drops a kernel in
	 * /boot and expects rEFInd to find it the usual way. */
	wr("/boot/refind_linux.conf",
	   "\"KDOS\"          \"%sroot=UUID=%s rw quiet loglevel=3 initrd=boot/initramfs.cpio.gz\"\n"
	   "\"KDOS verbose\"  \"%sroot=UUID=%s rw loglevel=7 initrd=boot/initramfs.cpio.gz\"\n",
	   crypt_opt, root_uuid, crypt_opt, root_uuid);

	if (kb_have_prog("efibootmgr") && ki_sys.uefi) {
		char disk[64];
		kb_strlcpy(disk, cfg.disk, sizeof(disk));
		char *eb[] = { "efibootmgr", "--create", "--disk", disk,
			       "--part", "1", "--loader",
			       "\\EFI\\refind\\refind_x64.efi", "--label",
			       "KDOS", NULL };
		try_(eb);
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
		do_packs, do_config, do_accounts, do_theme, do_boot, do_finish,
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
