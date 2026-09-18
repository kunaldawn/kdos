/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   KD's Homebrew Linux Distro
 * ---------------------------------
 */

/*
 * The /proc walk: one readdir, and per pid only what the caller asked for.
 *
 * A monitor that is the load is a bug, so `stat` is the only file read
 * unconditionally. Each KPR_WANT_* adds one open per process — KPR_WANT_CMDLINE
 * a readlink of exe/ besides, and KPR_WANT_GPU a readdir of fd/ — and a device
 * page passes 0 and walks no per-process files at all.
 */

#include <ctype.h>
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "kproc.h"

long kpr_hz(void)
{
	static long hz;
	if (!hz) {
		hz = sysconf(_SC_CLK_TCK);
		if (hz <= 0)
			hz = 100;
	}
	return hz;
}

unsigned long long kpr_mono_ms(void)
{
	struct timespec ts;
	clock_gettime(CLOCK_MONOTONIC, &ts);
	return (unsigned long long)ts.tv_sec * 1000ULL +
	       (unsigned long long)(ts.tv_nsec / 1000000L);
}

/*
 * A per-process file into a caller stack buffer, with the heap kept for the
 * files a buffer cannot bound.
 *
 * A walk of several hundred processes reads four files each, and a
 * whole-file slurp pays an fstat, an allocation and an EOF-confirming second
 * read for every one of them — three syscalls and a heap block to carry a
 * few hundred bytes, inside a draw loop. A full buffer means the content may
 * be cut, and a cut value parsed as if it were whole is a wrong reading, so
 * that case is re-read on the heap. *heap is what the caller must free, NULL
 * when the answer is in the stack buffer.
 */
static char *proc_file(int pid, const char *leaf, char *buf, size_t cap,
		       char **heap)
{
	int n = kpr_read_into_proc(buf, cap, "%d/%s", pid, leaf);

	*heap = NULL;
	if (n < 0)
		return NULL;
	if ((size_t)n + 1 < cap)
		return buf;
	*heap = kpr_slurp_proc("%d/%s", pid, leaf);
	return *heap;
}

/*
 * /proc/<pid>/stat, and the one parse in this library that an unprivileged
 * user controls.
 *
 * comm is whatever the process called itself and may contain spaces and
 * parentheses — anything but NUL and '/'. The fields after it are positional,
 * so the split has to be on the LAST ')' in the line, not the first: a process
 * named ") R 1 0 0" otherwise shifts every field that follows and the parse
 * reports its ppid, its state and its times as whatever the name spelled.
 */
static int parse_stat(char *d, KprProc *p)
{
	char *open = strchr(d, '(');
	char *close = strrchr(d, ')');
	if (!open || !close || close < open)
		return -1;

	*close = 0;
	kb_strlcpy(p->comm, open + 1, sizeof(p->comm));
	/* A newline inside comm would break every consumer that prints a row. */
	for (char *q = p->comm; *q; q++)
		if (*q == '\n' || *q == '\t')
			*q = ' ';

	p->pid = atoi(d);

	/* field 3 is state, then ppid; utime/stime are 14/15, starttime 22,
	 * counted from the character after ") ". */
	char *r = close + 1;
	while (*r == ' ')
		r++;
	p->state = *r ? *r : '?';

	/*
	 * The fields are positional and the skips have to be exact. Counting
	 * from the character after the state:
	 *   4 ppid   9 flags   14 utime  15 stime   19 nice
	 *   20 num_threads     22 starttime
	 * Everything between is consumed with %*, which takes no length
	 * modifier — there is no destination for one, and gcc rejects the
	 * combination. A suppressed conversion still consumes the whole
	 * numeric token, so the wide fields are skipped correctly.
	 *
	 * A miscount here silently reports one field's value as another's.
	 */
	long long nice = 0, threads = 0;
	sscanf(r + 1,
	       " %d %*d %*d %*d %*d %*u %*u %*u %*u %*u %llu %llu"
	       " %*d %*d %*d %lld %lld %*d %llu",
	       &p->ppid, &p->utime, &p->stime, &nice, &threads,
	       &p->starttime);
	p->nice = (int)nice;
	if (p->threads <= 0)
		p->threads = (int)threads;
	return 0;
}

static void parse_status(int pid, KprProc *p)
{
	/* Uid, VmRSS, VmSwap and Threads are all in the first kilobyte; the
	 * affinity masks that can run long come after them. */
	char buf[4096], *heap;
	char *d = proc_file(pid, "status", buf, sizeof(buf), &heap);
	if (!d)
		return;
	for (char *line = d, *next; line && *line; line = next) {
		char *nl = strchr(line, '\n');
		next = nl ? nl + 1 : NULL;
		if (nl)
			*nl = 0;
		if (!strncmp(line, "Uid:", 4)) {
			int real = 0;
			if (sscanf(line + 4, "%d", &real) == 1)
				p->uid = real;
		} else if (!strncmp(line, "VmRSS:", 6)) {
			unsigned long long kb = 0;
			if (sscanf(line + 6, "%llu", &kb) == 1)
				p->rss = kb * 1024ULL;
		} else if (!strncmp(line, "VmSwap:", 7)) {
			unsigned long long kb = 0;
			if (sscanf(line + 7, "%llu", &kb) == 1)
				p->swap = kb * 1024ULL;
		} else if (!strncmp(line, "Threads:", 8)) {
			p->threads = atoi(line + 8);
		}
		if (nl)
			*nl = '\n';
	}
	free(heap);
}

/*
 * io. An EACCES here is the ordinary case on a multi-user machine — other
 * users' and root's processes — and it must reach the screen as an em dash.
 * Zero would say "this process has done no disk io", which is a different and
 * false claim.
 */
static void parse_io(int pid, KprProc *p)
{
	char buf[512], *heap;
	p->rd_bytes = p->wr_bytes = KPR_UNREADABLE;
	char *d = proc_file(pid, "io", buf, sizeof(buf), &heap);
	if (!d)
		return;
	const char *r = strstr(d, "read_bytes:");
	const char *w = strstr(d, "write_bytes:");
	if (r)
		sscanf(r + 11, "%llu", &p->rd_bytes);
	if (w)
		sscanf(w + 12, "%llu", &p->wr_bytes);
	free(heap);
}

/*
 * The command line, joined for display and STORED AT ITS OWN LENGTH.
 *
 * /proc/<pid>/cmdline reports st_size 0 and is not bounded by any buffer —
 * the kernel publishes the whole argument area — so the read is a stack
 * buffer with a heap re-read behind it, and what is kept is a copy the size
 * of the string. Keeping the read buffer instead costs four kilobytes per
 * process for a median well under a hundred bytes, twice over in a monitor
 * holding two samples, and hands the allocator a few thousand four-kilobyte
 * blocks a second to churn.
 */
static void parse_cmdline(int pid, KprProc *p)
{
	char buf[4096], *heap = NULL, *d = buf;
	size_t len;
	int n = kpr_read_into_proc(buf, sizeof(buf), "%d/cmdline", pid);

	if (n < 0)
		return;
	len = (size_t)n;
	if (len + 1 == sizeof(buf)) {
		char path[512];

		snprintf(path, sizeof(path), "%s/%d/cmdline", kpr_proc(), pid);
		d = heap = kb_read_all(path, &len);
		if (!d)
			return;
	}
	if (!len) {
		free(heap);
		return;
	}
	/* NUL-separated on disk; joined with spaces for display. The trailing
	 * NUL is left as the terminator. */
	for (size_t i = 0; i + 1 < len; i++)
		if (!d[i])
			d[i] = ' ';
	p->cmdline = kb_strdup(d);
	free(heap);
}

/*
 * The executable behind the process.
 *
 * The one field that tells two processes with the same `comm` apart. NULL
 * where the link cannot be followed — EACCES for another user's process,
 * ENOENT for a kernel thread, and always under a fixture, which records
 * files and not symlinks. It stays NULL rather than becoming an empty
 * string, because a consumer tests the pointer and would otherwise draw an
 * empty row. It is read through kpr_proc() like everything else here, or a
 * replayed sample answers from the live machine.
 */
static void parse_exe(int pid, KprProc *p)
{
	char link[512], target[4096];
	ssize_t r;

	snprintf(link, sizeof(link), "%s/%d/exe", kpr_proc(), pid);
	r = readlink(link, target, sizeof(target) - 1);
	if (r <= 0)
		return;
	target[r] = 0;
	p->exe = kb_strdup(target);
}

/*
 * GPU engine time, summed over the process's DRM fds.
 *
 * fd/ is readlink'd rather than every fdinfo being read: a process with a
 * hundred open files has at most a couple of DRM ones, and opening a hundred
 * fdinfo files per process per tick is how this becomes the load.
 */
static void parse_gpu(int pid, KprProc *p)
{
	char dir[512];
	snprintf(dir, sizeof(dir), "%s/%d/fd", kpr_proc(), pid);
	DIR *d = opendir(dir);
	if (!d)
		return;

	struct dirent *e;
	while ((e = readdir(d))) {
		if (e->d_name[0] == '.')
			continue;
		/* dir is up to 512 and a dirent name up to 255. */
		char link[1024], target[1024];
		snprintf(link, sizeof(link), "%s/%s", dir, e->d_name);
		ssize_t n = readlink(link, target, sizeof(target) - 1);
		if (n <= 0)
			continue;
		target[n] = 0;
		if (!strstr(target, "/dri/card") && !strstr(target, "/dri/renderD"))
			continue;

		char *fi = kpr_slurp_proc("%d/fdinfo/%s", pid, e->d_name);
		if (!fi)
			continue;
		for (char *line = fi, *next; line && *line; line = next) {
			char *nl = strchr(line, '\n');
			next = nl ? nl + 1 : NULL;
			if (nl)
				*nl = 0;
			if (!strncmp(line, "drm-engine-", 11)) {
				char *v = strchr(line, ':');
				if (v)
					p->gpu_ns += strtoull(v + 1, NULL, 10);
			} else if (!strncmp(line, "drm-memory-", 11) ||
				   !strncmp(line, "drm-resident-", 13)) {
				char *v = strchr(line, ':');
				if (v)
					p->gpu_mem += strtoull(v + 1, NULL, 10) * 1024ULL;
			}
			if (nl)
				*nl = '\n';
		}
		free(fi);
	}
	closedir(d);
}

static void index_pids(KprSample *s);

int kpr_sample_take(KprSample *s, unsigned flags)
{
	memset(s, 0, sizeof(*s));
	s->wall_ms = kpr_mono_ms();

	DIR *d = opendir(kpr_proc());
	if (!d)
		return -1;

	int cap = 256;
	s->p = kb_calloc((size_t)cap, sizeof(*s->p));

	struct dirent *e;
	while ((e = readdir(d))) {
		if (!isdigit((unsigned char)e->d_name[0]))
			continue;
		int pid = atoi(e->d_name);
		if (pid <= 0)
			continue;

		char stbuf[1024], *stheap;
		char *st = proc_file(pid, "stat", stbuf, sizeof(stbuf),
				     &stheap);
		if (!st)
			continue;			/* it exited mid-walk */

		if (s->n == cap) {
			int ncap = cap * 2;
			KprProc *np = kb_calloc((size_t)ncap, sizeof(*np));
			memcpy(np, s->p, (size_t)cap * sizeof(*np));
			free(s->p);
			s->p = np;
			cap = ncap;
		}
		KprProc *p = &s->p[s->n];
		memset(p, 0, sizeof(*p));
		p->uid = -1;
		p->rd_bytes = p->wr_bytes = KPR_UNREADABLE;

		if (parse_stat(st, p) != 0) {
			free(stheap);
			continue;
		}
		free(stheap);

		if (flags & (KPR_WANT_STATUS | KPR_WANT_BOX))
			parse_status(pid, p);
		if (flags & KPR_WANT_IO)
			parse_io(pid, p);
		if (flags & KPR_WANT_CMDLINE) {
			parse_cmdline(pid, p);
			parse_exe(pid, p);
		}
		if (flags & KPR_WANT_GPU)
			parse_gpu(pid, p);

		/* A kernel thread has no cmdline. Counted rather than dropped:
		 * a table that silently omits half of /proc is one that gets
		 * argued with, so the caller filters and reports the number. */
		if (p->ppid == 2 || pid == 2)
			s->nkthread++;

		s->n++;
	}
	closedir(d);

	/*
	 * The box, in a SECOND pass. The conmon walk climbs the parent chain
	 * and so needs every pid already in the sample: resolving it inside
	 * the readdir would answer only for the processes whose ancestors
	 * happened to be read first.
	 */
	/*
	 * THE PID INDEX, BEFORE ANYTHING WALKS PARENTS. kpr_box_of climbs the
	 * parent chain through kpr_find_pid once per hop for every process,
	 * and over a linear scan that is the process count squared.
	 */
	index_pids(s);

	/* One supervisor answers for every process in its box; the memo is
	 * this pass's, because a pid can be somebody else's by the next. */
	kpr_conmon_forget();
	if (flags & KPR_WANT_BOX)
		for (int i = 0; i < s->n; i++)
			kpr_box_of(s, s->p[i].pid, s->p[i].box,
				   sizeof(s->p[i].box));
	return 0;
}

/*
 * `p` in pid order, so kpr_find_pid is a binary search.
 *
 * The array itself stays in readdir order — a consumer sorts it by its own
 * column and starting from a pid sort would be a different first screen — so
 * the order lives beside it as indices rather than as a permutation of the
 * rows, which would invalidate every KprProc pointer a caller is holding.
 */
static int by_pid_cmp(const void *a, const void *b, void *ctx)
{
	const KprProc *p = ctx;
	int x = p[*(const int *)a].pid, y = p[*(const int *)b].pid;

	return x < y ? -1 : x > y ? 1 : 0;
}

static void index_pids(KprSample *s)
{
	free(s->by_pid);
	s->by_pid = NULL;
	if (s->n <= 0)
		return;
	s->by_pid = kb_calloc((size_t)s->n, sizeof(*s->by_pid));
	if (!s->by_pid)
		return;		/* kpr_find_pid falls back to the scan */
	for (int i = 0; i < s->n; i++)
		s->by_pid[i] = i;
	qsort_r(s->by_pid, (size_t)s->n, sizeof(*s->by_pid), by_pid_cmp,
		s->p);
}

void kpr_sample_free(KprSample *s)
{
	if (!s || !s->p) {
		if (s) {
			free(s->by_pid);
			s->by_pid = NULL;
		}
		return;
	}
	free(s->by_pid);
	s->by_pid = NULL;
	for (int i = 0; i < s->n; i++) {
		free(s->p[i].cmdline);
		free(s->p[i].exe);
	}
	free(s->p);
	s->p = NULL;
	s->n = 0;
}

const KprProc *kpr_find_pid(const KprSample *s, int pid)
{
	if (!s || !s->p)
		return NULL;
	if (!s->by_pid) {
		/* No index: a sample a caller built by hand, or one whose
		 * index could not be allocated. */
		for (int i = 0; i < s->n; i++)
			if (s->p[i].pid == pid)
				return &s->p[i];
		return NULL;
	}
	for (int lo = 0, hi = s->n - 1; lo <= hi;) {
		int mid = lo + (hi - lo) / 2;
		const KprProc *c = &s->p[s->by_pid[mid]];

		if (c->pid == pid)
			return c;
		if (c->pid < pid)
			lo = mid + 1;
		else
			hi = mid - 1;
	}
	return NULL;
}

/*
 * Percent of ONE core over the interval between two samples.
 *
 * A process that appears between them has no previous CPU time to subtract
 * from, and reporting the whole of its lifetime usage as if it happened in
 * this interval is how a freshly started process shows 4000%. Such a process
 * reads 0 for one tick and correctly from the next.
 */
double kpr_proc_cpu(const KprProc *prev, const KprProc *cur,
		    unsigned long long wall_ms)
{
	if (!cur || !wall_ms)
		return 0.0;
	if (!prev || prev->starttime != cur->starttime)
		return 0.0;		/* new, or a recycled pid */

	unsigned long long pj = prev->utime + prev->stime;
	unsigned long long cj = cur->utime + cur->stime;
	if (cj <= pj)
		return 0.0;
	double secs = (double)wall_ms / 1000.0;
	double used = (double)(cj - pj) / (double)kpr_hz();
	return used / secs * 100.0;
}
