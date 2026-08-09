/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   kdosbuild — timing history, the ETA model and the telemetry sampler
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

#define EWMA_ALPHA 0.4
#define FAST_INTERVAL 1.0
#define WALK_INTERVAL 20.0
#define WALK_BUDGET 15.0	/* the fs walk is a HUD counter, not a job */

/* ──────────────────────────────────────────────────────────────────────── */
/* Timing history                                                           */

static TimeRec *rec_find(TimeRec *v, int n, const char *key)
{
	for (int i = 0; i < n; i++)
		if (!strcmp(v[i].key, key))
			return &v[i];
	return NULL;
}

static void rec_set(TimeRec **v, int *n, int *cap, const char *key, double secs)
{
	TimeRec *at = rec_find(*v, *n, key);
	if (!at) {
		if (*n == *cap) {
			*cap = *cap ? *cap * 2 : 64;
			TimeRec *nv = kb_calloc((size_t)*cap, sizeof(*nv));
			memcpy(nv, *v, (size_t)*n * sizeof(*nv));
			free(*v);
			*v = nv;
		}
		at = &(*v)[(*n)++];
		kb_strlcpy(at->key, key, sizeof(at->key));
		at->secs = 0;
	}
	at->secs = secs;
}

static void load_section(const KjNode *root, const char *name, TimeRec **v,
			 int *n, int *cap)
{
	const KjNode *sec = kj_get(root, name);
	if (!sec || sec->type != KJ_OBJ)
		return;
	for (const KjNode *c = sec->child; c; c = c->next)
		if (c->key && c->type == KJ_NUM)
			rec_set(v, n, cap, c->key, c->num);
}

void tm_load(Timings *t, const char *path)
{
	memset(t, 0, sizeof(*t));
	kb_strlcpy(t->path, path, sizeof(t->path));

	size_t len = 0;
	char *text = kb_read_all(path, &len);
	if (!text)
		return;
	KjNode *root = kj_parse(text);
	free(text);
	if (!root) {
		/* A corrupt timings file is history, not state: start over
		 * rather than refuse to build. */
		kj_free(root);
		return;
	}
	load_section(root, "steps", &t->step, &t->nstep, &t->stepcap);
	load_section(root, "phases", &t->phase, &t->nphase, &t->phasecap);
	kj_free(root);
}

static void dump_section(KbBuf *b, const char *name, const TimeRec *v, int n,
			 int last)
{
	kb_buf_printf(b, " \"%s\": {", name);
	for (int i = 0; i < n; i++)
		kb_buf_printf(b, "%s\n  \"%s\": %g", i ? "," : "", v[i].key,
			      v[i].secs);
	kb_buf_printf(b, "%s}%s\n", n ? "\n " : "", last ? "" : ",");
}

void tm_save(Timings *t)
{
	if (!t->dirty)
		return;

	char dir[512];
	kb_strlcpy(dir, t->path, sizeof(dir));
	char *slash = strrchr(dir, '/');
	if (slash) {
		*slash = 0;
		kb_mkdir_p(dir);
	}

	KbBuf b = {0};
	kb_buf_str(&b, "{\n");
	dump_section(&b, "steps", t->step, t->nstep, 0);
	dump_section(&b, "phases", t->phase, t->nphase, 1);
	kb_buf_str(&b, "}");

	char tmp[600];
	snprintf(tmp, sizeof(tmp), "%s.tmp", t->path);
	if (kb_write_all(tmp, b.p, b.n) == 0 && rename(tmp, t->path) == 0)
		t->dirty = 0;
	kb_buf_free(&b);
}

/* Round to two decimals the way python's round() does — the file is shared
 * with build.py for as long as both exist. */
static double blend(double old, double now)
{
	double v = old <= 0 ? now : old * (1 - EWMA_ALPHA) + now * EWMA_ALPHA;
	return (double)((long long)(v * 100 + (v < 0 ? -0.5 : 0.5))) / 100.0;
}

void tm_record_step(Timings *t, const char *key, double secs)
{
	if (secs <= 0)
		return;
	TimeRec *at = rec_find(t->step, t->nstep, key);
	rec_set(&t->step, &t->nstep, &t->stepcap, key,
		blend(at ? at->secs : 0, secs));
	t->dirty = 1;
}

void tm_record_phase(Timings *t, const char *key, double secs)
{
	if (secs <= 0)
		return;
	TimeRec *at = rec_find(t->phase, t->nphase, key);
	rec_set(&t->phase, &t->nphase, &t->phasecap, key,
		blend(at ? at->secs : 0, secs));
	t->dirty = 1;
}

double tm_step_est(const Timings *t, const char *key, double fallback)
{
	TimeRec *at = rec_find(t->step, t->nstep, key);
	return at ? at->secs : fallback;
}

double tm_phase_est(const Timings *t, const char *dir_name)
{
	TimeRec *at = rec_find(t->phase, t->nphase, dir_name);
	return at ? at->secs : -1;
}

void tm_free(Timings *t)
{
	free(t->step);
	free(t->phase);
	t->step = t->phase = NULL;
	t->nstep = t->nphase = 0;
}

/* ──────────────────────────────────────────────────────────────────────── */
/* ETA                                                                      */

double eta_seconds(const Manager *m, const Timings *t)
{
	double observed = 0;
	int nobserved = 0;
	for (int i = 0; i < m->norder; i++) {
		const BStep *s = m->order[i];
		if (s->is_group || s->status != ST_DONE)
			continue;
		double d = step_duration(s);
		if (d > 0) {
			observed += d;
			nobserved++;
		}
	}
	double known = 0;
	for (int i = 0; i < t->nstep; i++)
		known += t->step[i].secs;

	double fallback = nobserved ? observed / nobserved
				    : (t->nstep ? known / t->nstep : -1);

	double total = 0;
	int have = 0;
	for (int i = 0; i < m->nroot; i++) {
		const BStep *g = m->root[i];
		if (g->status == ST_SKIPPED || g->status == ST_DONE)
			continue;

		if (g->nchild) {
			for (int k = 0; k < g->nchild; k++) {
				const BStep *c = g->child[k];
				if (c->status == ST_DONE || c->status == ST_SKIPPED)
					continue;
				char key[192];
				step_timing_key(c, key, sizeof(key));
				double est = tm_step_est(t, key, fallback);
				if (est < 0)
					continue;
				have = 1;
				if (c->status == ST_RUNNING) {
					double left = est - step_duration(c);
					total += left > 0 ? left : 0;
				} else {
					total += est;
				}
			}
		} else {
			double est = tm_phase_est(t, g->meta->dir_name);
			if (est < 0)
				continue;
			have = 1;
			total += est;
		}
	}
	return have ? total : -1;
}

/* ──────────────────────────────────────────────────────────────────────── */
/* Sampler
 *
 * build.py ran this on a thread. Here the cheap counters are read inline on a
 * one-second timer and the only expensive part — walking build/fs — is a
 * FORKED child writing one line back through a pipe. Same effect, no shared
 * state to get wrong, and a walk that wedges cannot take the UI with it.
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
	long long total = 0, avail = 0;
	for (char *line = data, *next; line && *line; line = next) {
		char *nl = strchr(line, '\n');
		next = nl ? nl + 1 : NULL;
		if (nl)
			*nl = 0;
		if (!strncmp(line, "MemTotal:", 9))
			total = strtoll(line + 9, NULL, 10) * 1024;
		else if (!strncmp(line, "MemAvailable:", 13))
			avail = strtoll(line + 13, NULL, 10) * 1024;
		if (total && avail)
			break;
	}
	free(data);
	s->mem_total = total;
	s->mem_used = total - avail > 0 ? total - avail : 0;
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

	int cap = (int)(sizeof(s->history) / sizeof(s->history[0]));
	if (s->nhistory == cap) {
		memmove(s->history, s->history + 1,
			sizeof(s->history[0]) * (size_t)(cap - 1));
		s->nhistory--;
	}
	s->history[s->nhistory++] = s->lines_per_sec;

	read_loadavg(s);
	read_meminfo(s);
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

/* python's "{:,}" — the file counts in `--list` are grouped, and that listing
 * is the one output a person reads next to the python version's. */
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
