/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   kdosbuild — the telemetry sampler and the shared formatting
 * ---------------------------------
 */

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/statvfs.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#include "kdosbuild.h"

#define FAST_INTERVAL 1.0
#define WALK_INTERVAL 20.0
#define WALK_BUDGET 15.0	/* the fs walk is a HUD counter, not a job */

/* ──────────────────────────────────────────────────────────────────────── */
/* Sampler
 *
 * The cheap counters are read inline on a one-second timer; the only
 * expensive part — walking build/fs — is a FORKED child writing one line back
 * through a pipe. No shared state to get wrong, and a walk that wedges cannot
 * take the UI with it.
 */

void sam_init(Sampler *s)
{
	memset(s, 0, sizeof(*s));
	s->walker = -1;
	s->walker_fd = -1;
}

static void read_meminfo(Sampler *s)
{
	size_t len = 0;
	char *data = kb_read_all("/proc/meminfo", &len);
	if (!data)
		return;
	long long total = 0, avail = 0, swap_total = 0, swap_free = 0;
	for (char *line = data, *next; line && *line; line = next) {
		char *nl = strchr(line, '\n');
		next = nl ? nl + 1 : NULL;
		if (nl)
			*nl = 0;
		if (!strncmp(line, "MemTotal:", 9))
			total = strtoll(line + 9, NULL, 10) * 1024;
		else if (!strncmp(line, "MemAvailable:", 13))
			avail = strtoll(line + 13, NULL, 10) * 1024;
		else if (!strncmp(line, "SwapTotal:", 10))
			swap_total = strtoll(line + 10, NULL, 10) * 1024;
		else if (!strncmp(line, "SwapFree:", 9))
			swap_free = strtoll(line + 9, NULL, 10) * 1024;
	}
	free(data);
	s->mem_total = total;
	s->mem_used = total - avail > 0 ? total - avail : 0;
	long long swap = swap_total - swap_free > 0 ? swap_total - swap_free : 0;
	if (swap > s->swap_used)
		s->swap_rose_at = kb_now_s();
	s->swap_used = swap;
}

/* Busy over the last tick: user, nice, system, irq, softirq and steal against
 * those plus idle and iowait, from the counters' deltas. The first read has
 * no previous one and gives the average since boot. Guest time is already
 * inside user and nice, so it is not added again. */
static void read_cpu(Sampler *s)
{
	size_t len = 0;
	char *data = kb_read_all("/proc/stat", &len);
	if (!data)
		return;
	unsigned long long v[8] = { 0 };
	int ncpu = 0;
	for (char *line = data, *next; line && *line; line = next) {
		char *nl = strchr(line, '\n');
		next = nl ? nl + 1 : NULL;
		if (nl)
			*nl = 0;
		if (!strncmp(line, "cpu ", 4))
			sscanf(line + 4, "%llu %llu %llu %llu %llu %llu %llu %llu",
			       &v[0], &v[1], &v[2], &v[3], &v[4], &v[5], &v[6],
			       &v[7]);
		else if (!strncmp(line, "cpu", 3) &&
			 line[3] >= '0' && line[3] <= '9')
			ncpu++;
	}
	free(data);
	unsigned long long busy = v[0] + v[1] + v[2] + v[5] + v[6] + v[7];
	unsigned long long total = busy + v[3] + v[4];
	unsigned long long db = busy - s->cpu_busy_ticks;
	unsigned long long dt = total - s->cpu_total_ticks;
	if (total > s->cpu_total_ticks && busy >= s->cpu_busy_ticks)
		s->cpu_busy = (double)db / (double)dt;
	s->cpu_busy_ticks = busy;
	s->cpu_total_ticks = total;
	if (s->cpu_busy > 1)
		s->cpu_busy = 1;
	s->ncpu = ncpu;
}

static void read_loadavg(Sampler *s)
{
	size_t len = 0;
	char *data = kb_read_all("/proc/loadavg", &len);
	if (!data)
		return;
	s->load1 = strtod(data, NULL);
	free(data);
}

static void start_walk(Sampler *s, const char *build_dir)
{
	int fd[2];
	if (pipe(fd) < 0)
		return;

	pid_t pid = fork();
	if (pid < 0) {
		close(fd[0]);
		close(fd[1]);
		return;
	}
	if (pid == 0) {
		close(fd[0]);
		char path[900];
		snprintf(path, sizeof(path), "%s/fs", build_dir);
		Usage u = dir_usage(path, kb_now_s() + WALK_BUDGET, NULL, NULL);
		dprintf(fd[1], "%lld %lld %d\n", u.files, u.bytes, !u.complete);
		close(fd[1]);
		_exit(0);
	}
	close(fd[1]);
	fcntl(fd[0], F_SETFL, O_NONBLOCK);
	s->walker = pid;
	s->walker_fd = fd[0];
}

static void reap_walk(Sampler *s)
{
	if (s->walker <= 0)
		return;
	char buf[128];
	ssize_t r = read(s->walker_fd, buf, sizeof(buf) - 1);
	if (r > 0) {
		buf[r] = 0;
		long long files = 0, bytes = 0;
		int partial = 0;
		if (sscanf(buf, "%lld %lld %d", &files, &bytes, &partial) == 3) {
			s->fs_files = files;
			s->fs_bytes = bytes;
			s->fs_partial = partial;
			s->fs_sampled_at = kb_now_s();
		}
	}
	if (r == 0 || (r < 0 && errno != EAGAIN && errno != EWOULDBLOCK)) {
		close(s->walker_fd);
		s->walker_fd = -1;
		int status;
		while (waitpid(s->walker, &status, 0) < 0 && errno == EINTR)
			;
		s->walker = -1;
	}
}

static void push_one(double *hist, int n, double v)
{
	if (n == KB_HIST) {
		memmove(hist, hist + 1, sizeof(hist[0]) * (KB_HIST - 1));
		n--;
	}
	hist[n] = v;
}

void sam_push(Sampler *s, double cpu, double mem, double rate)
{
	push_one(s->cpu_hist, s->nhist, cpu);
	push_one(s->mem_hist, s->nhist, mem);
	push_one(s->rate_hist, s->nhist, rate);
	if (s->nhist < KB_HIST)
		s->nhist++;
}

void sam_pump(Sampler *s, Manager *m)
{
	reap_walk(s);

	double now = kb_now_s();
	if (now - s->last_tick < FAST_INTERVAL)
		return;
	double dt = s->last_tick ? now - s->last_tick : FAST_INTERVAL;
	s->last_tick = now;

	long long lines = m->total_lines;
	long long delta = lines - s->last_lines;
	s->lines_per_sec = delta > 0 ? (double)delta / dt : 0;
	s->last_lines = lines;

	read_loadavg(s);
	read_cpu(s);
	read_meminfo(s);
	sam_push(s, s->cpu_busy,
		 s->mem_total ? (double)s->mem_used / (double)s->mem_total : 0,
		 s->lines_per_sec);
	struct statvfs st;
	if (statvfs(m->build_dir, &st) == 0)
		s->disk_free = (long long)st.f_bavail * (long long)st.f_frsize;

	if (now - s->last_walk >= WALK_INTERVAL && s->walker < 0) {
		s->last_walk = now;
		start_walk(s, m->build_dir);
	}
}

void sam_stop(Sampler *s)
{
	if (s->walker > 0) {
		kill(s->walker, SIGKILL);
		int status;
		while (waitpid(s->walker, &status, 0) < 0 && errno == EINTR)
			;
		s->walker = -1;
	}
	if (s->walker_fd >= 0) {
		close(s->walker_fd);
		s->walker_fd = -1;
	}
}

/* ──────────────────────────────────────────────────────────────────────── */
/* Formatting                                                               */

const char *human_bytes(long long n)
{
	static char out[8][32];
	static int slot;
	char *b = out[slot++ & 7];

	if (n < 0) {
		kb_strlcpy(b, "-", 32);
		return b;
	}
	double v = (double)n;
	const char *units[] = { "B", "K", "M", "G", "T" };
	int u = 0;
	while (v >= 1024 && u < 4) {
		v /= 1024.0;
		u++;
	}
	if (u == 0)
		snprintf(b, 32, "%dB", (int)v);
	else
		snprintf(b, 32, "%.1f%s", v, units[u]);
	return b;
}

const char *human_time(double seconds)
{
	static char out[8][32];
	static int slot;
	char *b = out[slot++ & 7];

	if (seconds < 0) {
		kb_strlcpy(b, "--", 32);
		return b;
	}
	long s = (long)seconds;
	if (s < 60)
		snprintf(b, 32, "%lds", s);
	else if (s < 3600)
		snprintf(b, 32, "%ldm%02lds", s / 60, s % 60);
	else
		snprintf(b, 32, "%ldh%02ldm", s / 3600, (s % 3600) / 60);
	return b;
}

/* Thousands-grouped: the file counts in `--list` are read by eye, and an
 * ungrouped seven-digit count cannot be sized at a glance. */
const char *human_count(long long n)
{
	static char out[4][32];
	static int slot;
	char *b = out[slot++ & 3];

	char raw[32];
	snprintf(raw, sizeof(raw), "%lld", n < 0 ? -n : n);
	int len = (int)strlen(raw), o = 0;
	if (n < 0)
		b[o++] = '-';
	for (int i = 0; i < len; i++) {
		if (i && (len - i) % 3 == 0)
			b[o++] = ',';
		b[o++] = raw[i];
	}
	b[o] = 0;
	return b;
}

const char *format_when(double ts)
{
	static char out[4][32];
	static int slot;
	char *b = out[slot++ & 3];

	if (ts <= 0) {
		kb_strlcpy(b, "-", 32);
		return b;
	}
	time_t t = (time_t)ts;
	struct tm tmv;
	if (localtime_r(&t, &tmv))
		strftime(b, 32, "%Y-%m-%d %H:%M", &tmv);
	else
		kb_strlcpy(b, "-", 32);
	return b;
}
