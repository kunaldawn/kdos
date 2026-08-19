/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   attrib.c — turning one energy number into a per-app answer
 *
 * RAPL gives ONE microjoule counter for the whole CPU package. Everything that
 * makes this a per-app number happens here, and every step of it is a choice
 * that can be got wrong quietly:
 *
 * THE IDLE FLOOR IS SUBTRACTED FIRST. A package burns watts with nothing
 * running, and a cycle-share model that skips this hands that idle energy to
 * whatever process happened to tick — so a machine sitting at a login prompt
 * reports its screensaver as 90% of its energy. The floor is the lowest average
 * power seen since the daemon started: a measurement, monotonically improving,
 * reported with the answer rather than assumed. Only the energy above it is
 * attributed to anyone.
 *
 * THE DENOMINATOR IS /proc/stat, NOT THE SUM OF THE PIDS. A process that starts
 * and exits inside one window is gone by the second sample, and dividing by the
 * survivors' ticks would silently hand its energy to them — a compile that ran
 * for eight seconds would show up as the web browser. /proc/stat counts the
 * ticks of the dead too, so the difference between it and the surviving pids is
 * a real quantity, and it gets its own line rather than being spread.
 *
 * IDENTITY IS THE PART EVERY OTHER DESKTOP CANNOT DO, AND HERE IT IS FREE.
 * "Firefox" is forty processes in scattered cgroups on a normal Linux desktop.
 * On KDOS every alien app runs inside a podman container whose supervisor knows
 * its name, and /usr/share/kdos/alien-apps maps the in-box command to the
 * canonical app. So a process is walked up its parent chain: to the `conmon`
 * that names the box, and past any ancestor that names an app. Firefox's
 * content processes land on Firefox because Firefox is their ancestor, not
 * because anything guessed.
 *
 * THE GPU HALF IS `drm-engine-*` IN fdinfo, and it is TIME, never energy. The
 * kernel's per-client DRM accounting says how long each client's work occupied
 * each engine; nothing on the machine says what that cost in joules. On a part
 * with integrated graphics the GPU's energy is already inside the RAPL package
 * number and needs no separate accounting; on a discrete card it is outside
 * RAPL entirely and no amount of arithmetic here can recover it. Both are
 * stated in the report. Drivers that implement no fdinfo stats at all (the
 * proprietary nvidia one, today) produce no GPU column rather than a column of
 * zeroes.
 * ---------------------------------
 */

#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "energyd.h"

/* comm in /proc/<pid>/stat is TASK_COMM_LEN-1 = 15 characters, truncated by the
 * kernel with no marker. Every comparison against a command name from the
 * alien-apps table has to be made against the same 15, or `libreoffice-calc`
 * never matches the `libreoffic` the kernel actually reports. */
#define KE_COMM 15

static long g_hz;

long ke_hz(void)
{
	if (g_hz <= 0) {
		g_hz = sysconf(_SC_CLK_TCK);
		if (g_hz <= 0)
			g_hz = 100;
	}
	return g_hz;
}

const char *ke_proc(void)
{
	const char *p = getenv("KDOS_ENERGY_PROC");
	return p && *p ? p : "/proc";
}

const char *ke_powercap(void)
{
	const char *p = getenv("KDOS_ENERGY_POWERCAP");
	return p && *p ? p : "/sys/class/powercap";
}

static char *slurp(const char *fmt, ...) __attribute__((format(printf, 1, 2)));

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

/* ── the alien-app table ───────────────────────────────────────────────── */

typedef struct {
	char name[KE_NAME];		/* the canonical app id                 */
	char key[KE_COMM + 1];		/* its command, as comm will report it  */
} KeAlien;

static KeAlien *g_alien;
static int g_nalien;
static bool g_alien_loaded;

void ke_apps_load(void)
{
	if (g_alien_loaded)
		return;
	g_alien_loaded = true;

	const char *path = getenv("KDOS_ALIEN_APPS");
	if (!path || !*path)
		path = "/usr/share/kdos/alien-apps";
	size_t len = 0;
	char *data = kb_read_all(path, &len);
	if (!data)
		return;

	int cap = 128;
	g_alien = kb_calloc((size_t)cap, sizeof(*g_alien));

	for (char *line = data, *next; line && *line; line = next) {
		char *nl = strchr(line, '\n');
		next = nl ? nl + 1 : NULL;
		if (nl)
			*nl = '\0';
		if (*line == '#' || !*line)
			continue;
		char *tab = strchr(line, '\t');
		if (!tab)
			continue;
		*tab = '\0';

		/* The command's first word, minus its directory: `/usr/bin/gimp
		 * -f %U` is the process that will appear as `gimp`. */
		char *cmd = tab + 1;
		while (*cmd == ' ')
			cmd++;
		char *sp = strpbrk(cmd, " \t");
		if (sp)
			*sp = '\0';
		const char *base = kb_basename(cmd);
		if (!*base)
			continue;

		if (g_nalien == cap) {
			cap *= 2;
			KeAlien *bigger = kb_calloc((size_t)cap, sizeof(*bigger));
			memcpy(bigger, g_alien,
			       (size_t)g_nalien * sizeof(*bigger));
			free(g_alien);
			g_alien = bigger;
		}
		kb_strlcpy(g_alien[g_nalien].name, line,
			   sizeof(g_alien[0].name));
		kb_strlcpy(g_alien[g_nalien].key, base, sizeof(g_alien[0].key));
		g_nalien++;
	}
	free(data);
}

static const char *alien_name(const char *comm)
{
	for (int i = 0; i < g_nalien; i++)
		if (!strcmp(g_alien[i].key, comm))
			return g_alien[i].name;
	return NULL;
}

/* ── one process ───────────────────────────────────────────────────────── */

static int is_number(const char *s)
{
	if (!*s)
		return 0;
	for (const char *p = s; *p; p++)
		if (!isdigit((unsigned char)*p))
			return 0;
	return 1;
}

/*
 * Σ of every `drm-engine-<name>: <n> ns` across a process's DRM fds.
 *
 * The fds are found by READLINK rather than by reading every fdinfo file: a
 * process has tens of fds and almost none of them are a GPU, and one readlink
 * is far cheaper than an open-read-close of a file that turns out to be a
 * socket. This runs every sampling interval against every process on the
 * machine, so the constant factor is the design.
 */
static unsigned long long gpu_of(int pid, bool *seen)
{
	char dir[256];
	snprintf(dir, sizeof(dir), "%s/%d/fd", ke_proc(), pid);
	int n = 0;
	char **fds = kb_listdir(dir, &n);
	if (!fds)
		return 0;

	unsigned long long total = 0;
	for (int i = 0; i < n; i++) {
		char link[512], target[256];
		snprintf(link, sizeof(link), "%s/%s", dir, fds[i]);
		ssize_t got = readlink(link, target, sizeof(target) - 1);
		if (got <= 0)
			continue;
		target[got] = '\0';
		if (strncmp(target, "/dev/dri/", 9))
			continue;

		char *info = slurp("%s/%d/fdinfo/%s", ke_proc(), pid, fds[i]);
		if (!info)
			continue;
		for (char *line = info, *next; line && *line; line = next) {
			char *nl = strchr(line, '\n');
			next = nl ? nl + 1 : NULL;
			if (nl)
				*nl = '\0';
			if (strncmp(line, "drm-engine-", 11))
				continue;
			char *colon = strchr(line, ':');
			if (!colon)
				continue;
			*seen = true;
			total += strtoull(colon + 1, NULL, 10);
		}
		free(info);
	}
	kb_strv_free(fds);
	return total;
}

static int read_stat(int pid, KeProc *out)
{
	char *data = slurp("%s/%d/stat", ke_proc(), pid);
	if (!data)
		return 0;

	/* The comm field is parenthesised and may itself contain spaces and
	 * parentheses, so it is delimited by the LAST ')' rather than scanned. */
	char *open = strchr(data, '(');
	char *close = open ? strrchr(open, ')') : NULL;
	if (!open || !close || close < open + 1) {
		free(data);
		return 0;
	}
	size_t len = (size_t)(close - open - 1);
	if (len >= sizeof(out->comm))
		len = sizeof(out->comm) - 1;
	memcpy(out->comm, open + 1, len);
	out->comm[len] = '\0';

	int ppid = 0;
	unsigned long long utime = 0, stime = 0;
	int got = sscanf(close + 2,
			 "%*c %d %*d %*d %*d %*d %*u %*u %*u %*u %*u %llu %llu",
			 &ppid, &utime, &stime);
	free(data);
	if (got < 3)
		return 0;
	out->pid = pid;
	out->ppid = ppid;
	out->cpu = utime + stime;
	return 1;
}

/* ── the sample ────────────────────────────────────────────────────────── */

static unsigned long long read_busy(void)
{
	char *data = slurp("%s/stat", ke_proc());
	if (!data)
		return 0;
	unsigned long long v[10] = {0};
	unsigned long long busy = 0;
	if (!strncmp(data, "cpu ", 4) || !strncmp(data, "cpu\t", 4)) {
		sscanf(data + 3, "%llu %llu %llu %llu %llu %llu %llu %llu",
		       &v[0], &v[1], &v[2], &v[3], &v[4], &v[5], &v[6], &v[7]);
		/* user nice system [idle iowait] irq softirq steal — idle and
		 * iowait are the machine NOT working, and including them would
		 * make every machine look equally busy. guest time is already
		 * counted inside user. */
		busy = v[0] + v[1] + v[2] + v[5] + v[6] + v[7];
	}
	free(data);
	return busy;
}

void ke_sample_free(KeSample *s)
{
	free(s->p);
	s->p = NULL;
	s->n = 0;
}

static int by_pid(const void *a, const void *b)
{
	return ((const KeProc *)a)->pid - ((const KeProc *)b)->pid;
}

void ke_sample_take(KeSample *s)
{
	ke_sample_free(s);
	memset(s, 0, sizeof(*s));
	s->when = kb_now_s();
	s->busy = read_busy();
	ke_hz();

	int count = 0;
	char **names = kb_listdir(ke_proc(), &count);
	if (!names)
		return;
	s->p = kb_calloc(KE_MAX_PROC, sizeof(*s->p));
	for (int i = 0; i < count && s->n < KE_MAX_PROC; i++) {
		if (!is_number(names[i]))
			continue;
		int pid = atoi(names[i]);
		if (!read_stat(pid, &s->p[s->n]))
			continue;
		s->p[s->n].gpu_ns = gpu_of(pid, &s->gpu_seen);
		s->n++;
	}
	kb_strv_free(names);
	qsort(s->p, (size_t)s->n, sizeof(*s->p), by_pid);
}

/* ── identity ──────────────────────────────────────────────────────────── */

static const KeProc *find_pid(const KeSample *s, int pid)
{
	int lo = 0, hi = s->n - 1;
	while (lo <= hi) {
		int mid = (lo + hi) / 2;
		if (s->p[mid].pid == pid)
			return &s->p[mid];
		if (s->p[mid].pid < pid)
			lo = mid + 1;
		else
			hi = mid - 1;
	}
	return NULL;
}

/*
 * Which app does this process belong to?
 *
 * ONE WALK, TWO ANSWERS, and that is why this is not libkproc's.
 *
 * kpr_box_of() answers "which container is this pid in", which is a reading
 * about the machine. This answers "which APPLICATION does this process roll
 * up to", which is an attribution POLICY: the highest ancestor in the
 * alien-app table wins, so a helper an app spawned is reported as the app and
 * not as an app of its own. The two happen to climb the same chain and are
 * different questions, and moving this into the library would put one
 * program's policy in every consumer's path.
 *
 * One walk up the parent chain answers both halves. `conmon` is podman's
 * supervisor and its argv carries `-n <name>`, which is the box; any ancestor
 * whose comm is in the alien-app table is the app, and the HIGHEST such
 * ancestor wins so that a helper process spawned by an app is not reported as
 * an app of its own.
 *
 * cgroups are the textbook answer and are unusable here: no systemd means
 * rootless podman gets no cgroup delegation, and the whole box frequently sits
 * in `0::/`. The ppid chain costs nothing extra — this walk is already reading
 * the stats it needs.
 */
void ke_name_of(const KeSample *s, int idx, char *out, size_t cap)
{
	ke_apps_load();

	const KeProc *p = &s->p[idx];
	const char *app = alien_name(p->comm);
	char box[64] = "";

	int pid = p->ppid;
	for (int hop = 0; hop < 24 && pid > 1; hop++) {
		const KeProc *up = find_pid(s, pid);
		if (!up)
			break;
		if (!strcmp(up->comm, "conmon")) {
			char *cmd = slurp("%s/%d/cmdline", ke_proc(), up->pid);
			if (cmd) {
				size_t len = 0;
				for (char *a = cmd; *a; a += len + 1) {
					len = strlen(a);
					if (!strcmp(a, "-n") && a[len + 1]) {
						kb_strlcpy(box, a + len + 1,
							   sizeof(box));
						break;
					}
				}
				free(cmd);
			}
			break;		/* conmon is the boundary of the box */
		}
		const char *up_app = alien_name(up->comm);
		if (up_app)
			app = up_app;
		pid = up->ppid;
	}

	if (app && *box)
		snprintf(out, cap, "%s (appbox %s)", app, box);
	else if (app)
		snprintf(out, cap, "%s", app);
	else if (*box)
		snprintf(out, cap, "%s (appbox %s)", p->comm, box);
	else
		snprintf(out, cap, "%s", p->comm);
}

/* ── the ledger ────────────────────────────────────────────────────────── */

static KeApp *app_slot(KeLedger *l, const char *name)
{
	for (int i = 0; i < l->n; i++)
		if (!strcmp(l->app[i].name, name))
			return &l->app[i];
	if (l->n == KE_MAX_APP)
		return NULL;
	KeApp *a = &l->app[l->n++];
	kb_strlcpy(a->name, name, sizeof(a->name));
	return a;
}

void ke_ledger_add(KeLedger *l, const KeSample *prev, const KeSample *cur,
		   long long energy_uj)
{
	double dt = cur->when - prev->when;
	if (dt <= 0 || energy_uj < 0)
		return;

	l->samples++;
	l->window += dt;
	l->total_uj += (double)energy_uj;
	if (cur->gpu_seen)
		l->gpu_seen = true;

	/* The floor is a minimum over the whole session and is applied at report
	 * time; see the comment on KeApp for why it cannot be applied here. */
	double power_uw = (double)energy_uj / dt;
	if (!l->have_floor || power_uw < l->floor_uw) {
		l->floor_uw = power_uw;
		l->have_floor = true;
	}

	unsigned long long busy = cur->busy > prev->busy
					? cur->busy - prev->busy : 0;
	if (busy == 0) {
		/* Energy with nothing on any run queue. Nobody can be named for
		 * it, and naming nobody is the answer. */
		l->short_we += (double)energy_uj;
		l->short_wt += dt;
		return;
	}

	unsigned long long matched = 0;
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
		unsigned long long g = cur->p[j].gpu_ns > prev->p[i].gpu_ns
					? cur->p[j].gpu_ns - prev->p[i].gpu_ns : 0;
		if (d || g) {
			char name[KE_NAME];
			ke_name_of(cur, j, name, sizeof(name));
			KeApp *a = app_slot(l, name);
			if (a) {
				double w = (double)d / (double)busy;
				a->we += w * (double)energy_uj;
				a->wt += w * dt;
				a->cpu_ticks += (double)d;
				a->gpu_ns += (double)g;
			}
		}
		matched += d;
		i++;
		j++;
	}

	/* What /proc/stat counted and no surviving process claims: the ticks of
	 * everything that started and exited inside this window. */
	if (matched < busy) {
		double w = (double)(busy - matched) / (double)busy;
		l->short_we += w * (double)energy_uj;
		l->short_wt += w * dt;
	}
}
