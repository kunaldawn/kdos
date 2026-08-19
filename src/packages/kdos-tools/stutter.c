/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   kdos stutter — why the desktop hiccuped, with a name
 *
 * The join nobody else performs. Three sources, none of which is an answer on
 * its own:
 *
 *   - The COMPOSITOR knows a frame was late, by how much, and what its own
 *     render cost (kdos-comp's frames.c, over $XDG_RUNTIME_DIR/kdos-frames.sock).
 *   - PSI (the files under /proc/pressure) knows the machine was starved, and of
 *     what — cpu, io or memory — but not by whom.
 *   - `/proc/<pid>/stat` knows who burned the CPU and who was stuck in
 *     uninterruptible sleep, but not that anybody cared.
 *
 * The closest prior art, Latency Lens, reads PSI and explains stutter in plain
 * language, and says outright that it "cannot identify which specific process
 * caused a frame miss." That is exactly the sentence this file exists to finish.
 *
 * WHAT IT WILL NOT DO. It never says "X caused this" — it says what X was doing
 * when it happened. Attribution from sampled counters is circumstantial by
 * construction: a 500 ms window cannot prove causation, and a tool that claimed
 * it would be wrong the first time two things were busy at once. The wording is
 * deliberate throughout, and the numbers that back it are printed rather than
 * summarised away.
 *
 * The one causal claim it DOES make is about the compositor itself, because
 * there it has both halves: if `render_ms` used most of the frame budget, the
 * desktop was late on its own account, and no amount of CPU elsewhere is the
 * explanation. That branch is checked first.
 *
 * Everything is read from a directory that DEFAULTS to /proc but does not have
 * to be one — `--fixture` points the whole sampler at recorded snapshots, which
 * is what makes an attribution engine testable without a stutter to hand.
 * ---------------------------------
 */

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <time.h>
#include <unistd.h>

#include "kproc.h"
#include "kdos-tools.h"

#define ST_MAX_PROC 4096
#define ST_TOP 3

typedef struct {
	int pid;
	int ppid;
	char comm[40];
	unsigned long long cpu;		/* utime + stime, in clock ticks */
	int uninterruptible;		/* state D: waiting on the disk        */
} StProc;

typedef struct {
	StProc *p;
	int n;
	double when;			/* CLOCK_MONOTONIC ms                   */
	double psi_cpu, psi_io, psi_mem;	/* "some" avg10, percent        */
	double psi_io_full, psi_mem_full;
} StSample;

/* One offender, as the report names it. */
typedef struct {
	char name[128];
	int pid;
	double cpu_pct;
	int blocked;
} StBlame;

static const char *g_proc = "/proc";
static long g_hz = 100;

/* ── reading ───────────────────────────────────────────────────────────── */

static char *slurp(const char *fmt, ...)
	__attribute__((format(printf, 1, 2)));

static char *slurp(const char *fmt, ...)
{
	char path[512];
	va_list ap;
	va_start(ap, fmt);
	vsnprintf(path, sizeof(path), fmt, ap);
	va_end(ap);
	size_t n = 0;
	return kb_read_all(path, &n);
}

/* "some avg10=1.23 avg60=... total=..." — avg10 is the only one worth showing:
 * a stutter lasts a moment, and avg60 has already averaged it away. */
static void read_pressure(const char *what, double *some, double *full)
{
	*some = *full = -1.0;
	char *data = slurp("%s/pressure/%s", g_proc, what);
	if (!data)
		return;
	for (char *line = data, *next; line && *line; line = next) {
		char *nl = strchr(line, '\n');
		next = nl ? nl + 1 : NULL;
		double v = 0;
		const char *avg = strstr(line, "avg10=");
		if (!avg)
			continue;
		v = atof(avg + 6);
		if (!strncmp(line, "some", 4))
			*some = v;
		else if (!strncmp(line, "full", 4))
			*full = v;
	}
	free(data);
}

/*
 * /proc/<pid>/stat, parsed from the LAST ')' forward.
 *
 * comm is field 2, it is wrapped in parentheses, and it may contain both spaces
 * and parentheses — `(Web Content)` and `(sh (deleted))` are both real. Every
 * naive sscanf on this file is wrong for those, and they are not rare enough to
 * ignore: a browser is exactly the sort of thing this tool ends up naming.
 */
static int read_stat(int pid, StProc *out)
{
	char *data = slurp("%s/%d/stat", g_proc, pid);
	if (!data)
		return 0;
	char *close = strrchr(data, ')');
	char *open = strchr(data, '(');
	if (!close || !open || close < open) {
		free(data);
		return 0;
	}
	size_t len = (size_t)(close - open - 1);
	if (len >= sizeof(out->comm))
		len = sizeof(out->comm) - 1;
	memcpy(out->comm, open + 1, len);
	out->comm[len] = '\0';

	char state = 0;
	int ppid = 0;
	unsigned long long utime = 0, stime = 0;
	/* After the comm: state ppid pgrp session tty tpgid flags minflt cminflt
	 * majflt cmajflt utime stime — utime is the 11th field after the state. */
	int got = sscanf(close + 2,
			 "%c %d %*d %*d %*d %*d %*u %*u %*u %*u %*u %llu %llu",
			 &state, &ppid, &utime, &stime);
	free(data);
	if (got < 4)
		return 0;
	out->pid = pid;
	out->ppid = ppid;
	out->cpu = utime + stime;
	out->uninterruptible = state == 'D';
	return 1;
}

static int is_number(const char *s)
{
	if (!*s)
		return 0;
	for (const char *p = s; *p; p++)
		if (!isdigit((unsigned char)*p))
			return 0;
	return 1;
}

static void sample_free(StSample *s)
{
	free(s->p);
	s->p = NULL;
	s->n = 0;
}

static int by_pid(const void *a, const void *b)
{
	return ((const StProc *)a)->pid - ((const StProc *)b)->pid;
}

static void sample_take(StSample *s, double when_ms)
{
	sample_free(s);
	s->when = when_ms;
	read_pressure("cpu", &s->psi_cpu, &(double){0});
	read_pressure("io", &s->psi_io, &s->psi_io_full);
	read_pressure("memory", &s->psi_mem, &s->psi_mem_full);

	int count = 0;
	char **names = kb_listdir(g_proc, &count);
	if (!names)
		return;
	s->p = calloc(ST_MAX_PROC, sizeof(*s->p));
	if (!s->p) {
		kb_strv_free(names);
		return;
	}
	for (int i = 0; i < count && s->n < ST_MAX_PROC; i++) {
		if (!is_number(names[i]))
			continue;
		if (read_stat(atoi(names[i]), &s->p[s->n]))
			s->n++;
	}
	kb_strv_free(names);
	qsort(s->p, (size_t)s->n, sizeof(*s->p), by_pid);
}

/* ── naming ────────────────────────────────────────────────────────────── */

/*
 * The container's name, which is the useful one — not `conmon`'s and not a
 * pid. libkproc owns the walk: it is the same ppid climb to podman's
 * supervisor that kdos-oomd and kdos-teams need, and it lived in four places
 * with four different hop bounds before it lived in one.
 *
 * cgroup would be the textbook answer and is not usable here: KDOS has no
 * systemd, rootless podman gets no cgroup delegation, and the whole box
 * frequently sits in `0::/` — so the cgroup line says nothing at all.
 */
static int box_of(int pid, char *out, size_t cap)
{
	return kpr_box_of_pid(pid, out, cap);
}

static void name_of(const StProc *p, char *out, size_t cap)
{
	char box[64];
	if (box_of(p->pid, box, sizeof(box)))
		snprintf(out, cap, "%s (appbox %s)", p->comm, box);
	else
		snprintf(out, cap, "%s", p->comm);
}

/* ── the join ──────────────────────────────────────────────────────────── */

/*
 * Who moved between two samples.
 *
 * CPU is a delta of ticks over the wall time between the samples, expressed as
 * a percentage of ONE core — 100% means a core's worth, and on an 8-thread
 * machine three processes at 100% each is perfectly possible and is exactly what
 * a stutter looks like.
 */
static int blame(const StSample *prev, const StSample *cur, StBlame *out, int max)
{
	double secs = (cur->when - prev->when) / 1000.0;
	if (secs <= 0)
		return 0;

	StBlame *cand = calloc((size_t)cur->n, sizeof(*cand));
	if (!cand)
		return 0;
	int n = 0;

	int i = 0, j = 0;
	while (i < prev->n && j < cur->n) {
		if (prev->p[i].pid < cur->p[j].pid) {
			i++;
			continue;
		}
		if (prev->p[i].pid > cur->p[j].pid) {
			j++;	/* new since the last sample: no delta to take */
			continue;
		}
		unsigned long long d = cur->p[j].cpu > prev->p[i].cpu
					? cur->p[j].cpu - prev->p[i].cpu : 0;
		double pct = 100.0 * (double)d / (double)g_hz / secs;
		if (pct >= 1.0 || cur->p[j].uninterruptible) {
			cand[n].pid = cur->p[j].pid;
			cand[n].cpu_pct = pct;
			cand[n].blocked = cur->p[j].uninterruptible;
			name_of(&cur->p[j], cand[n].name, sizeof(cand[n].name));
			n++;
		}
		i++;
		j++;
	}

	/* Blocked first, then by CPU: a process stuck in D state is the one that
	 * explains an io-pressure spike, and it will show almost no CPU while
	 * doing it. Sorting on CPU alone hides exactly the case the io half of
	 * this tool exists for. */
	for (int a = 0; a < n; a++)
		for (int b = a + 1; b < n; b++) {
			int swap = 0;
			if (cand[b].blocked != cand[a].blocked)
				swap = cand[b].blocked;
			else if (cand[b].cpu_pct > cand[a].cpu_pct)
				swap = 1;
			if (swap) {
				StBlame t = cand[a];
				cand[a] = cand[b];
				cand[b] = t;
			}
		}

	int take = n < max ? n : max;
	memcpy(out, cand, (size_t)take * sizeof(*out));
	free(cand);
	return take;
}

/* ── reporting ─────────────────────────────────────────────────────────── */

typedef struct {
	double late_ms, render_ms, refresh_hz, wall_ms;
	int dropped;
	char output[32];
	char source[16];
} StMiss;

static void print_miss(const StMiss *m, const StSample *cur,
		       const StBlame *b, int nb, int json)
{
	double budget = m->refresh_hz > 0 ? 1000.0 / m->refresh_hz : 16.67;
	int self = m->render_ms > 0.6 * budget;

	if (json) {
		KbBuf out = {0};
		kb_buf_printf(&out,
			"{\"event\":\"stutter\",\"output\":\"%s\",\"source\":\"%s\","
			"\"late_ms\":%.3f,\"dropped\":%d,\"render_ms\":%.3f,"
			"\"budget_ms\":%.3f,\"wall_ms\":%.3f,"
			"\"compositor_was_late\":%s,"
			"\"psi\":{\"cpu\":%.2f,\"io\":%.2f,\"memory\":%.2f},"
			"\"busiest\":[",
			m->output, m->source, m->late_ms, m->dropped,
			m->render_ms, budget, m->wall_ms, self ? "true" : "false",
			cur->psi_cpu, cur->psi_io, cur->psi_mem);
		for (int i = 0; i < nb; i++) {
			kb_buf_printf(&out, "%s{\"pid\":%d,\"name\":", i ? "," : "",
				      b[i].pid);
			kb_json_str(&out, b[i].name);
			kb_buf_printf(&out, ",\"cpu_pct\":%.1f,\"blocked_on_io\":%s}",
				      b[i].cpu_pct, b[i].blocked ? "true" : "false");
		}
		kb_buf_printf(&out, "]}\n");
		fwrite(out.p, 1, out.n, stdout);
		kb_buf_free(&out);
		fflush(stdout);
		return;
	}

	/* The compositor's own wall clock, not ours: it stamped the miss, and a
	 * replayed fixture then prints the time the stutter HAPPENED rather than
	 * the time it was read back. */
	char when[16] = "";
	time_t now = m->wall_ms > 0 ? (time_t)(m->wall_ms / 1000.0) : time(NULL);
	struct tm tm;
	if (localtime_r(&now, &tm))
		strftime(when, sizeof(when), "%H:%M:%S", &tm);

	printf("%s  %d frame%s dropped on %s (%.0f ms)\n", when, m->dropped,
	       m->dropped == 1 ? "" : "s", m->output, m->late_ms);

	if (self) {
		/* The one causal statement this tool is entitled to make. */
		printf("          the desktop itself was late: the compositor "
		       "took %.1f ms of a %.1f ms frame\n", m->render_ms, budget);
	} else {
		/* What was measured, not a claim about what the compositor was
		 * doing: a render that finished fast says the delay was outside
		 * the render path, and that is all it says. */
		printf("          the compositor's own render took %.1f ms of a "
		       "%.1f ms frame", m->render_ms, budget);
		if (cur->psi_cpu >= 0)
			printf(", cpu pressure %.0f%%, io %.0f%%", cur->psi_cpu,
			       cur->psi_io);
		printf("\n");
	}

	if (nb == 0) {
		printf("          nothing was measurably busy — a driver stall, "
		       "or something too short to sample\n");
		return;
	}
	printf("          busiest just then:");
	for (int i = 0; i < nb; i++) {
		if (b[i].blocked)
			printf(" %s (waiting on the disk)", b[i].name);
		else
			printf(" %s (%.0f%% of a core)", b[i].name, b[i].cpu_pct);
		if (i < nb - 1)
			printf(",");
	}
	printf("\n");
}

/* ── the event stream ──────────────────────────────────────────────────── */

/* A field out of one NDJSON line. No parser: these lines are written by
 * kdos-comp three functions away, the shapes are fixed, and a JSON reader here
 * would be more code than the thing it reads. */
static double jnum(const char *line, const char *key, double dflt)
{
	char pat[64];
	snprintf(pat, sizeof(pat), "\"%s\":", key);
	const char *at = strstr(line, pat);
	return at ? atof(at + strlen(pat)) : dflt;
}

static void jstr(const char *line, const char *key, char *out, size_t cap)
{
	char pat[64];
	snprintf(pat, sizeof(pat), "\"%s\":\"", key);
	const char *at = strstr(line, pat);
	out[0] = '\0';
	if (!at)
		return;
	at += strlen(pat);
	const char *end = strchr(at, '"');
	if (!end)
		return;
	size_t n = (size_t)(end - at);
	if (n >= cap)
		n = cap - 1;
	memcpy(out, at, n);
	out[n] = '\0';
}

static int parse_miss(const char *line, StMiss *m)
{
	if (!strstr(line, "\"event\":\"miss\""))
		return 0;
	memset(m, 0, sizeof(*m));
	m->late_ms = jnum(line, "late_ms", 0);
	m->render_ms = jnum(line, "render_ms", 0);
	m->refresh_hz = jnum(line, "refresh_hz", 60);
	m->wall_ms = jnum(line, "wall_ms", 0);
	m->dropped = (int)jnum(line, "dropped", 1);
	jstr(line, "output", m->output, sizeof(m->output));
	jstr(line, "source", m->source, sizeof(m->source));
	return 1;
}

static double mono_ms(void)
{
	struct timespec ts;
	clock_gettime(CLOCK_MONOTONIC, &ts);
	return ts.tv_sec * 1000.0 + ts.tv_nsec / 1e6;
}

static int connect_frames(void)
{
	const char *rt = getenv("XDG_RUNTIME_DIR");
	if (!rt || !*rt) {
		fprintf(stderr, "kdos stutter: no XDG_RUNTIME_DIR — this is not "
				"a desktop session\n");
		return -1;
	}
	struct sockaddr_un addr = { .sun_family = AF_UNIX };
	snprintf(addr.sun_path, sizeof(addr.sun_path), "%s/kdos-frames.sock", rt);
	int fd = socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0);
	if (fd < 0)
		return -1;
	if (connect(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
		fprintf(stderr, "kdos stutter: nothing is reporting frame timing "
				"on %s\n", addr.sun_path);
		fprintf(stderr, "             kdos-comp opens it at session "
				"start; is this a KDOS desktop?\n");
		close(fd);
		return -1;
	}
	return fd;
}

/* ── the fixture ───────────────────────────────────────────────────────── */

/*
 * A recorded stutter, replayed.
 *
 * `<dir>/t0` and `<dir>/t1` are two /proc snapshots — the same layout, so the
 * sampler code below is the code that runs for real — and `<dir>/events.jsonl`
 * is what the compositor would have sent between them. This is the only way an
 * attribution engine gets a regression test: a real stutter cannot be summoned
 * on demand, and one that could would prove nothing about the join.
 */
static int run_fixture(const char *dir, int json)
{
	StSample prev = {0}, cur = {0};
	char *p0 = kb_path_join(dir, "t0");
	char *p1 = kb_path_join(dir, "t1");

	/*
	 * BOTH roots move together. The sampler reads through g_proc and the
	 * box identity reads through libkproc's own root, so setting one and
	 * not the other leaves the container walk looking at the REAL /proc
	 * while everything else replays the recording — and the fixture then
	 * reports a process with no box on a machine where it has one.
	 */
	g_proc = p0;
	kpr_root_set(p0, NULL);
	sample_take(&prev, 0.0);
	g_proc = p1;
	kpr_root_set(p1, NULL);
	sample_take(&cur, 500.0);

	char *evp = kb_path_join(dir, "events.jsonl");
	size_t len = 0;
	char *ev = kb_read_all(evp, &len);
	if (!ev) {
		fprintf(stderr, "kdos stutter: %s has no events.jsonl\n", dir);
		return 2;
	}

	int seen = 0;
	for (char *line = ev, *next; line && *line; line = next) {
		char *nl = strchr(line, '\n');
		next = nl ? nl + 1 : NULL;
		if (nl)
			*nl = '\0';
		StMiss m;
		if (!parse_miss(line, &m))
			continue;
		StBlame b[ST_TOP];
		int nb = blame(&prev, &cur, b, ST_TOP);
		print_miss(&m, &cur, b, nb, json);
		seen++;
	}

	free(ev);
	free(evp);
	free(p0);
	free(p1);
	sample_free(&prev);
	sample_free(&cur);
	return seen ? 0 : 1;
}

/* ── main ──────────────────────────────────────────────────────────────── */

static void usage(FILE *o)
{
	fprintf(o,
		"usage: kdos stutter [--json] [--fixture DIR]\n"
		"\n"
		"  Watches the compositor's frame timing and, when a frame is\n"
		"  late, says what the machine was doing at that moment.\n"
		"\n"
		"  --json          one NDJSON record per stutter\n"
		"  --fixture DIR   replay a recorded one (two /proc snapshots\n"
		"                  and an event stream) instead of watching\n");
}

int stutter_main(int argc, char **argv)
{
	int json = 0;
	const char *fixture = NULL;

	for (int i = 1; i < argc; i++) {
		if (!strcmp(argv[i], "--json")) {
			json = 1;
		} else if (!strcmp(argv[i], "--fixture") && i + 1 < argc) {
			fixture = argv[++i];
		} else if (!strcmp(argv[i], "-h") || !strcmp(argv[i], "--help")) {
			usage(stdout);
			return 0;
		} else {
			usage(stderr);
			return 2;
		}
	}

	/* Line-buffered: this is a live tool and its output is usually going into
	 * a pipe or a log. Block buffering would hold a stutter report until the
	 * next 4 KB, which for a tool that prints three lines an hour means the
	 * report arrives when the session ends. */
	setvbuf(stdout, NULL, _IOLBF, 0);

	long hz = sysconf(_SC_CLK_TCK);
	if (hz > 0)
		g_hz = hz;

	if (fixture)
		return run_fixture(fixture, json);

	int fd = connect_frames();
	if (fd < 0)
		return 2;

	if (!json)
		printf("Watching for late frames. Ctrl-C to stop.\n");

	StSample prev = {0}, cur = {0};
	sample_take(&prev, mono_ms());
	double last_sample = prev.when;

	char buf[4096];
	size_t have = 0;
	for (;;) {
		/*
		 * The sampler runs on its own cadence and the events arrive on
		 * theirs, so the socket read has a timeout: half a second, which
		 * is also the sampling window. Long enough that the sampling
		 * itself is not what the report is measuring — reading /proc for
		 * a few hundred processes is real work — and short enough that
		 * the window a miss lands in still means something.
		 */
		struct timeval tv = { .tv_sec = 0, .tv_usec = 500000 };
		fd_set rd;
		FD_ZERO(&rd);
		FD_SET(fd, &rd);
		int r = select(fd + 1, &rd, NULL, NULL, &tv);

		double now = mono_ms();
		if (now - last_sample >= 400.0) {
			sample_free(&prev);
			prev = cur;
			memset(&cur, 0, sizeof(cur));
			sample_take(&cur, now);
			last_sample = now;
		}

		if (r <= 0)
			continue;

		/*
		 * A LINE THAT DOES NOT FIT IS DROPPED, NOT MISTAKEN FOR EOF.
		 *
		 * `have` reaches sizeof(buf) - 1 with no newline in it only
		 * when the compositor has written a line longer than this
		 * buffer. Without the reset below, read() is then asked for
		 * zero bytes, returns zero, and the n <= 0 branch takes that
		 * for a closed socket — reporting a dead compositor that is
		 * running fine. Resync instead: discard the partial line and
		 * keep reading.
		 */
		size_t room = sizeof(buf) - have - 1;
		if (room == 0) {
			have = 0;
			buf[0] = '\0';
			room = sizeof(buf) - 1;
		}

		ssize_t n = read(fd, buf + have, room);
		if (n <= 0) {
			if (n < 0 && errno == EINTR)
				continue;
			fprintf(stderr, "kdos stutter: the compositor closed the "
					"connection\n");
			break;
		}
		have += (size_t)n;
		buf[have] = '\0';

		char *line = buf, *nl;
		while ((nl = strchr(line, '\n'))) {
			*nl = '\0';
			StMiss m;
			if (parse_miss(line, &m) && prev.n && cur.n) {
				StBlame b[ST_TOP];
				int nb = blame(&prev, &cur, b, ST_TOP);
				print_miss(&m, &cur, b, nb, json);
			}
			line = nl + 1;
		}
		have = strlen(line);
		memmove(buf, line, have + 1);
	}

	sample_free(&prev);
	sample_free(&cur);
	close(fd);
	return 0;
}
