/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   kdosbuild — writing and extracting snapshots
 *
 * libkbuild decides WHICH archives supply what and what a layer holds; this
 * walks the tree, runs the tar and commits the result. Two processes, `tar |
 * zstd` or `zstd -dc | tar`, with progress sampled off the output file's size
 * (creating) or the decompressor's /proc rchar (extracting).
 *
 * With a tar that takes NUL-separated lists, a snapshot path is archived from
 * the walk's own list and tar never recurses: the whole list for a full
 * archive, the diff against the path's index for a layer. A snapshot is
 * staged in build/snapshots/.new-<phase>/ and renamed into place; the one it
 * replaces moves to .held/ while any other snapshot's chain still runs
 * through it, and is deleted once none does.
 * ---------------------------------
 */

#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#include "kdosbuild.h"

#define MAX_WALK_DEPTH 64

/* Flags we would LIKE tar to have. Probed against `tar --help` because
 * Alpine's GNU tar may be built without acl/xattr support and busybox tar has
 * almost none of them. */
static const char *WANTED_TAR_FLAGS[] = {
	"--numeric-owner", "--one-file-system", "--sparse", "--xattrs",
	"--acls", NULL
};
static const char *EXTRACT_TAR_FLAGS[] = {
	"--numeric-owner", "--xattrs", "--acls", NULL
};
/* Root-only: tar refuses to chown as a normal user and drops setuid/setgid
 * bits unless told to keep them. The build runs as root, so this is the
 * normal path; it is skipped when someone inspects a snapshot unprivileged. */
static const char *EXTRACT_TAR_FLAGS_ROOT[] = {
	"--same-owner", "--same-permissions", NULL
};

/* ──────────────────────────────────────────────────────────────────────── */
/* Probing                                                                  */

static char *tar_help_text(void)
{
	static char *cached;
	static int probed;
	if (probed)
		return cached;
	probed = 1;

	KbArgv a = {0};
	kb_argv_add(&a, "tar");
	kb_argv_add(&a, "--help");
	kb_argv_end(&a);

	char buf[65536];
	if (kb_run_capture(&a, buf, sizeof(buf)) >= 0)
		cached = kb_strdup(buf);
	return cached;
}

static int tar_has(const char *flag)
{
	const char *help = tar_help_text();
	return help && strstr(help, flag) != NULL;
}

const char *snap_codec(void)
{
	static const char *cached;
	if (cached)
		return cached;
	cached = kb_have_prog("zstd") ? "zstd" :
		 kb_have_prog("gzip") ? "gzip" : "none";
	return cached;
}

/* zstd -1: a snapshot is written every phase and read back rarely, so the
 * level that keeps pace with tar wins over a smaller archive. The decompressor
 * reads any level, so an archive written at another one still restores. */
static void compress_cmd(KbArgv *a)
{
	const char *codec = snap_codec();
	if (!strcmp(codec, "zstd")) {
		kb_argv_add(a, "zstd");
		kb_argv_add(a, "-1");
		kb_argv_add(a, "-T0");
		kb_argv_add(a, "-q");
		kb_argv_add(a, "-c");
		kb_argv_add(a, "-");
	} else if (!strcmp(codec, "gzip")) {
		kb_argv_add(a, "gzip");
		kb_argv_add(a, "-1");
		kb_argv_add(a, "-c");
	} else {
		kb_argv_add(a, "cat");
	}
	kb_argv_end(a);
}

/* ──────────────────────────────────────────────────────────────────────── */
/* git                                                                      */

void snap_git_info(const char *repo_root, char *commit, size_t ccap, int *dirty)
{
	commit[0] = 0;
	*dirty = 0;

	/* Inside the build container `.git` is not mounted, so the Makefile
	 * passes these through the environment; the git commands are the
	 * fallback for running on the host. */
	const char *env = getenv("KDOS_GIT_COMMIT");
	const char *env_dirty = getenv("KDOS_GIT_DIRTY");
	if (env && *env)
		kb_strlcpy(commit, env, ccap);
	if (env_dirty)
		*dirty = *env_dirty && strcmp(env_dirty, "0") &&
			 strcmp(env_dirty, "false");

	if (!commit[0]) {
		KbArgv a = {0};
		kb_argv_add(&a, "git");
		kb_argv_add(&a, "-C");
		kb_argv_add(&a, repo_root);
		kb_argv_add(&a, "rev-parse");
		kb_argv_add(&a, "--short");
		kb_argv_add(&a, "HEAD");
		kb_argv_end(&a);
		char buf[128];
		if (kb_run_capture(&a, buf, sizeof(buf)) == 0) {
			char *nl = strchr(buf, '\n');
			if (nl)
				*nl = 0;
			kb_strlcpy(commit, buf, ccap);
		}
	}
	if (!env_dirty && commit[0]) {
		KbArgv a = {0};
		kb_argv_add(&a, "git");
		kb_argv_add(&a, "-C");
		kb_argv_add(&a, repo_root);
		kb_argv_add(&a, "status");
		kb_argv_add(&a, "--porcelain");
		kb_argv_end(&a);
		char buf[4096];
		if (kb_run_capture(&a, buf, sizeof(buf)) == 0)
			*dirty = buf[0] != 0;
	}
}

/* ──────────────────────────────────────────────────────────────────────── */
/* Tree walk
 *
 * Cycle-safe. `build/fs/kdos` is a bind mount of the repo root, which
 * contains `build/fs` again — and because a bind mount of the same filesystem
 * keeps the same st_dev, a device check ALONE walks that loop forever.
 * Directories are tracked by (st_dev, st_ino), live mountpoints are skipped
 * and depth is capped. A lazily-detached mount is gone from /proc/mounts but
 * still traversable from inside, so the inode check — not the mount list — is
 * what actually closes the loop. The visited set is an open-addressing hash,
 * so the check costs the same on the ten-thousandth directory as on the
 * first; a list scanned per directory makes a large tree's walk quadratic.
 */

typedef struct {
	dev_t dev;
	ino_t ino;
	int used;
} NodeKey;

typedef struct {
	NodeKey *slot;
	size_t cap;	/* a power of two */
	size_t n;
} NodeSet;

static size_t node_hash(dev_t dev, ino_t ino)
{
	return (size_t)(((unsigned long long)ino * 0x9E3779B97F4A7C15ULL) ^
			(unsigned long long)dev);
}

static void node_put(NodeSet *s, dev_t dev, ino_t ino)
{
	size_t i = node_hash(dev, ino) & (s->cap - 1);
	while (s->slot[i].used)
		i = (i + 1) & (s->cap - 1);
	s->slot[i].dev = dev;
	s->slot[i].ino = ino;
	s->slot[i].used = 1;
	s->n++;
}

/* 1 when (dev, ino) was already in the set; otherwise adds it and returns 0.
 * Grows at half full, which keeps every probe run short. */
static int node_seen_or_add(NodeSet *s, dev_t dev, ino_t ino)
{
	size_t i = node_hash(dev, ino) & (s->cap - 1);
	while (s->slot[i].used) {
		if (s->slot[i].dev == dev && s->slot[i].ino == ino)
			return 1;
		i = (i + 1) & (s->cap - 1);
	}
	if ((s->n + 1) * 2 > s->cap) {
		NodeSet g = { kb_calloc(s->cap * 2, sizeof(NodeKey)),
			      s->cap * 2, 0 };
		for (size_t k = 0; k < s->cap; k++)
			if (s->slot[k].used)
				node_put(&g, s->slot[k].dev, s->slot[k].ino);
		free(s->slot);
		*s = g;
	}
	node_put(s, dev, ino);
	return 0;
}

typedef struct {
	char path[1024];
	int depth;
} WalkItem;

Usage dir_usage(const char *path, double deadline,
		void (*on_tick)(long long files, long long bytes, void *user),
		void *user)
{
	Usage u = { 0, 0, 1 };
	struct stat st;

	if (lstat(path, &st) < 0)
		return u;
	if (!S_ISDIR(st.st_mode)) {
		u.bytes = st.st_size;
		u.files = 1;
		return u;
	}

	char (*mounts)[256] = kb_calloc(512, sizeof(*mounts));
	int nmount = kbuild_snap_mounts_under("/", mounts, 512);

	dev_t root_dev = st.st_dev;
	NodeSet keys = { kb_calloc(1024, sizeof(NodeKey)), 1024, 0 };
	node_put(&keys, st.st_dev, st.st_ino);

	int nstack = 0, stackcap = 256;
	WalkItem *stack = kb_calloc((size_t)stackcap, sizeof(*stack));
	kb_strlcpy(stack[nstack].path, path, sizeof(stack[0].path));
	stack[nstack].depth = 0;
	nstack++;

	double next_tick = kb_now_s() + 0.25;

	while (nstack) {
		WalkItem item = stack[--nstack];
		double now = kb_now_s();
		if (deadline > 0 && now > deadline) {
			u.complete = 0;
			break;
		}
		if (on_tick && now >= next_tick) {
			next_tick = now + 0.25;
			on_tick(u.files, u.bytes, user);
		}

		DIR *d = opendir(item.path);
		if (!d)
			continue;
		struct dirent *e;
		while ((e = readdir(d))) {
			if (!strcmp(e->d_name, ".") || !strcmp(e->d_name, ".."))
				continue;
			char child[1024];
			/* A path that would not fit is not silently truncated
			 * into a DIFFERENT path — it is skipped, and the walk
			 * is a size estimate, so a miss costs nothing. */
			size_t dl = strlen(item.path), nl = strlen(e->d_name);
			if (dl + nl + 2 > sizeof(child))
				continue;
			memcpy(child, item.path, dl);
			child[dl] = '/';
			memcpy(child + dl + 1, e->d_name, nl + 1);
			struct stat cs;
			if (lstat(child, &cs) < 0)
				continue;
			if (cs.st_dev != root_dev)
				continue;	/* not our filesystem to count */
			if (!S_ISDIR(cs.st_mode)) {
				u.files++;
				u.bytes += cs.st_size;
				continue;
			}
			if (item.depth >= MAX_WALK_DEPTH)
				continue;

			int is_mount = 0;
			for (int i = 0; i < nmount && !is_mount; i++)
				is_mount = !strcmp(mounts[i], child);
			if (is_mount)
				continue;

			if (node_seen_or_add(&keys, cs.st_dev, cs.st_ino))
				continue;	/* bind-mount loop back in */

			if (nstack == stackcap) {
				stackcap *= 2;
				WalkItem *ns = kb_calloc((size_t)stackcap,
							 sizeof(*ns));
				memcpy(ns, stack, (size_t)nstack * sizeof(*ns));
				free(stack);
				stack = ns;
			}
			kb_strlcpy(stack[nstack].path, child,
				   sizeof(stack[0].path));
			stack[nstack].depth = item.depth + 1;
			nstack++;
		}
		closedir(d);
	}

	free(mounts);
	free(keys.slot);
	free(stack);
	return u;
}

/* ──────────────────────────────────────────────────────────────────────── */
/* The two-process pipeline                                                 */

static long long file_size(const char *path)
{
	struct stat st;
	return stat(path, &st) == 0 ? (long long)st.st_size : 0;
}

/* Bytes the decompressor has read from its input, via /proc. `rchar` rather
 * than `read_bytes`: the latter counts physical reads only and stays at zero
 * for a page-cached archive. */
static long long consumed(pid_t pid, long long total)
{
	char path[64];
	snprintf(path, sizeof(path), "/proc/%d/io", (int)pid);
	size_t len = 0;
	char *data = kb_read_all(path, &len);
	if (!data)
		return 0;
	long long out = 0;
	for (char *line = data, *next; line && *line; line = next) {
		char *nl = strchr(line, '\n');
		next = nl ? nl + 1 : NULL;
		if (nl)
			*nl = 0;
		if (!strncmp(line, "rchar:", 6)) {
			out = strtoll(line + 6, NULL, 10);
			break;
		}
	}
	free(data);
	return out > total ? total : out;
}

typedef struct {
	long long files;
	char current[256];
	char err[2048];
	char names[1024];
} PipeCount;

static void collect(PipeCount *c, const char *line, int is_name)
{
	if (!*line)
		return;
	if (is_name) {
		c->files++;
		kb_strlcpy(c->current, line, sizeof(c->current));
		/* tar interleaves its own errors with -v names on the same
		 * stream, so keep a tail of them for diagnostics. */
		size_t n = strlen(c->names);
		if (n > sizeof(c->names) / 2)
			n = 0;
		snprintf(c->names + n, sizeof(c->names) - n, "%s%s",
			 n ? "; " : "", line);
	} else {
		size_t n = strlen(c->err);
		if (n < sizeof(c->err) - 128)
			snprintf(c->err + n, sizeof(c->err) - n, "%s%s",
				 n ? "; " : "", line);
	}
}

enum { NAMES_FIRST_STDERR, NAMES_SECOND_STDOUT };

/* Run `first | second`, optionally to a file, reporting progress through
 * m->snap and redrawing via the caller's tick callback.
 *
 * Which stream carries tar's member names depends on direction: creating an
 * archive puts the archive on stdout so `-v` names go to stderr, while
 * extracting puts the names on stdout. Getting that backwards costs both the
 * file counter and, worse, tar's diagnostics — so the name stream is named
 * explicitly and every other stream is collected as error output.
 */
static int run_pipe(Manager *m, const KbArgv *first, const KbArgv *second,
		    const char *out_path, int names_from, long long input_size,
		    void (*tick)(Manager *m), char *err, size_t errcap)
{
	PipeCount c = {0};
	int names_stdout = names_from == NAMES_SECOND_STDOUT;

	int out_fd = -1;
	if (out_path) {
		out_fd = open(out_path, O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC,
			      0644);
		if (out_fd < 0) {
			snprintf(err, errcap, "cannot write %s: %s", out_path,
				 strerror(errno));
			return -1;
		}
	}

	int link[2], e1[2], e2[2], so[2];
	if (pipe(link) < 0 || pipe(e1) < 0 || pipe(e2) < 0 || pipe(so) < 0) {
		snprintf(err, errcap, "pipe: %s", strerror(errno));
		if (out_fd >= 0)
			close(out_fd);
		return -1;
	}

	pid_t p1 = fork();
	if (p1 == 0) {
		dup2(link[1], STDOUT_FILENO);
		dup2(e1[1], STDERR_FILENO);
		close(link[0]); close(link[1]);
		close(e1[0]); close(e1[1]);
		close(e2[0]); close(e2[1]);
		close(so[0]); close(so[1]);
		if (out_fd >= 0)
			close(out_fd);
		execvp(first->v[0], (char *const *)first->v);
		_exit(127);
	}

	pid_t p2 = fork();
	if (p2 == 0) {
		dup2(link[0], STDIN_FILENO);
		if (out_fd >= 0)
			dup2(out_fd, STDOUT_FILENO);
		else if (names_stdout)
			dup2(so[1], STDOUT_FILENO);
		else {
			int devnull = open("/dev/null", O_WRONLY);
			if (devnull >= 0) {
				dup2(devnull, STDOUT_FILENO);
				close(devnull);
			}
		}
		dup2(e2[1], STDERR_FILENO);
		close(link[0]); close(link[1]);
		close(e1[0]); close(e1[1]);
		close(e2[0]); close(e2[1]);
		close(so[0]); close(so[1]);
		if (out_fd >= 0)
			close(out_fd);
		execvp(second->v[0], (char *const *)second->v);
		_exit(127);
	}

	close(link[0]);
	close(link[1]);
	close(e1[1]);
	close(e2[1]);
	close(so[1]);
	if (out_fd >= 0)
		close(out_fd);

	struct pollfd pfd[3] = {
		{ e1[0], POLLIN, 0 },
		{ e2[0], POLLIN, 0 },
		{ names_stdout ? so[0] : -1, POLLIN, 0 }
	};
	if (!names_stdout)
		close(so[0]);

	static char part[3][8192];
	static size_t plen[3];
	memset(plen, 0, sizeof(plen));

	int alive = names_stdout ? 3 : 2;
	long long last_bytes = 0;
	double last_t = kb_now_s();
	int aborted = 0;

	while (alive > 0) {
		if (poll(pfd, 3, 250) < 0 && errno != EINTR)
			break;

		for (int i = 0; i < 3; i++) {
			if (pfd[i].fd < 0 || !(pfd[i].revents & (POLLIN | POLLHUP)))
				continue;
			ssize_t r = read(pfd[i].fd, part[i] + plen[i],
					 sizeof(part[0]) - plen[i] - 1);
			if (r <= 0) {
				close(pfd[i].fd);
				pfd[i].fd = -1;
				alive--;
				continue;
			}
			plen[i] += (size_t)r;
			part[i][plen[i]] = 0;

			int is_name = (i == 0 && names_from == NAMES_FIRST_STDERR)
				   || (i == 2);
			char *start = part[i], *nl;
			while ((nl = strchr(start, '\n'))) {
				*nl = 0;
				char *e = nl;
				while (e > start && (e[-1] == '\r' || e[-1] == ' '))
					*--e = 0;
				collect(&c, start, is_name);
				start = nl + 1;
			}
			size_t left = plen[i] - (size_t)(start - part[i]);
			memmove(part[i], start, left);
			plen[i] = left;
			if (plen[i] >= sizeof(part[0]) - 1)
				plen[i] = 0;	/* absurd line: drop it   */
		}

		long long done = out_path ? file_size(out_path)
					  : (input_size ? consumed(p1, input_size) : 0);
		double now = kb_now_s();
		if (now > last_t)
			m->snap.rate = (double)(done - last_bytes) / (now - last_t);
		last_bytes = done;
		last_t = now;
		m->snap.bytes = done;
		m->snap.files = c.files;
		kb_strlcpy(m->snap.current, c.current, sizeof(m->snap.current));
		if (tick)
			tick(m);

		if (m->stop_requested && !aborted) {
			aborted = 1;
			kill(p2, SIGTERM);
			kill(p1, SIGTERM);
		}
	}

	int s1 = 0, s2 = 0;
	while (waitpid(p1, &s1, 0) < 0 && errno == EINTR)
		;
	while (waitpid(p2, &s2, 0) < 0 && errno == EINTR)
		;

	if (aborted) {
		snprintf(err, errcap, "aborted");
		return -1;
	}

	/* GNU tar exits 1 for warnings ("file changed as we read it"); the
	 * build is paused during a snapshot, so treat that as non-fatal. */
	struct { int status; const char *name; } procs[2] = {
		{ s1, first->v[0] }, { s2, second->v[0] }
	};
	for (int i = 0; i < 2; i++) {
		int rc = WIFEXITED(procs[i].status) ? WEXITSTATUS(procs[i].status)
						    : 128;
		if (!rc)
			continue;
		if (!strcmp(procs[i].name, "tar") && rc == 1)
			continue;
		const char *tail = c.err[0] ? c.err : c.names;
		snprintf(err, errcap, "%s exited %d%s%.200s", procs[i].name, rc,
			 *tail ? ": " : "", tail);
		return -1;
	}

	m->snap.files = c.files;
	kb_strlcpy(m->snap.current, c.current, sizeof(m->snap.current));
	return 0;
}

static Manager *tick_manager;
static void (*tick_fn)(Manager *m);

/* ──────────────────────────────────────────────────────────────────────── */
/* The snapshot walk
 *
 * One record per entry of a snapshot path, in walk order: each directory's
 * names sorted, parents before children. The walk is what a snapshot
 * archives — tar is handed its list with --no-recursion — so the archive and
 * the index written beside it describe the same tree by construction.
 *
 * The loop guards are dir_usage's: another filesystem is not entered, a live
 * mount point is skipped, and a directory reached twice (a bind mount of an
 * ancestor keeps its st_dev) is not walked again. A tree deeper than
 * SNAP_WALK_DEPTH fails the snapshot: cut short, it would archive a tree with
 * a hole in it and record that as the whole.
 */

#define SNAP_WALK_DEPTH 1024

typedef struct {
	Manager *m;
	const KbuildPhase *excl;
	KbuildSnapIndex *ix;
	dev_t root_dev;
	NodeSet keys;
	char (*mounts)[256];
	int nmount;
	KbBuf abs;		/* the entry being visited, absolute       */
	size_t prefix;		/* where its path relative to build/ starts */
	double next_tick;
	char *err;
	size_t errcap;
	int failed;
} Walk;

static long long ctime_ns(const struct stat *st)
{
	return (long long)st->st_ctim.tv_sec * 1000000000LL +
	       (long long)st->st_ctim.tv_nsec;
}

static void walk_record(Walk *w, const struct stat *st)
{
	kbuild_snap_idx_add(w->ix, st->st_mode & S_IFMT,
			    (unsigned long long)st->st_ino, ctime_ns(st),
			    S_ISREG(st->st_mode) ? (long long)st->st_blocks * 512
						 : 0,
			    w->abs.p + w->prefix);
	double now = kb_now_s();
	if (tick_fn && now >= w->next_tick) {
		w->next_tick = now + 0.25;
		w->m->snap.files = (long long)w->ix->n;
		kb_strlcpy(w->m->snap.current, w->abs.p + w->prefix,
			   sizeof(w->m->snap.current));
		tick_fn(w->m);
	}
}

static void walk_children(Walk *w, int depth)
{
	if (w->failed)
		return;
	if (depth >= SNAP_WALK_DEPTH) {
		snprintf(w->err, w->errcap, "%.200s is deeper than %d levels",
			 w->abs.p + w->prefix, SNAP_WALK_DEPTH);
		w->failed = 1;
		return;
	}
	size_t len = w->abs.n;
	char **names = kb_listdir(w->abs.p, NULL);
	if (!names) {
		snprintf(w->err, w->errcap, "cannot read %.200s: %s",
			 w->abs.p + w->prefix, strerror(errno));
		w->failed = 1;
		return;
	}
	for (char **e = names; *e && !w->failed; e++) {
		w->abs.n = len;
		w->abs.p[len] = 0;
		kb_buf_add(&w->abs, "/", 1);
		kb_buf_str(&w->abs, *e);
		if (kbuild_snap_exclude_match(w->excl, w->abs.p + w->prefix))
			continue;
		struct stat st;
		if (lstat(w->abs.p, &st) < 0 || st.st_dev != w->root_dev)
			continue;
		if (S_ISDIR(st.st_mode)) {
			int is_mount = 0;
			for (int i = 0; i < w->nmount && !is_mount; i++)
				is_mount = !strcmp(w->mounts[i], w->abs.p);
			if (is_mount ||
			    node_seen_or_add(&w->keys, st.st_dev, st.st_ino))
				continue;
		}
		walk_record(w, &st);
		if (S_ISDIR(st.st_mode))
			walk_children(w, depth + 1);
	}
	w->abs.n = len;
	w->abs.p[len] = 0;
	kb_strv_free(names);
}

/* 0 with `ix` filled and its root recorded; -1 with `err` set. */
static int snap_walk(Manager *m, const char *path, const KbuildPhase *excl,
		     KbuildSnapIndex *ix, char *err, size_t errcap)
{
	memset(ix, 0, sizeof(*ix));
	Walk w = {0};
	w.m = m;
	w.excl = excl;
	w.ix = ix;
	w.err = err;
	w.errcap = errcap;
	kb_buf_printf(&w.abs, "%s/", m->build_dir);
	w.prefix = w.abs.n;
	kb_buf_str(&w.abs, path);

	struct stat st;
	if (lstat(w.abs.p, &st) < 0) {
		snprintf(err, errcap, "%s: %s", path, strerror(errno));
		kb_buf_free(&w.abs);
		return -1;
	}
	w.root_dev = st.st_dev;
	ix->root_dev = (unsigned long long)st.st_dev;
	ix->root_ino = (unsigned long long)st.st_ino;
	w.keys.cap = 1024;
	w.keys.slot = kb_calloc(w.keys.cap, sizeof(NodeKey));
	node_put(&w.keys, st.st_dev, st.st_ino);
	w.mounts = kb_calloc(256, sizeof(*w.mounts));
	w.nmount = kbuild_snap_mounts_under(w.abs.p, w.mounts, 256);
	w.next_tick = kb_now_s() + 0.25;

	walk_record(&w, &st);
	if (S_ISDIR(st.st_mode))
		walk_children(&w, 0);

	free(w.mounts);
	free(w.keys.slot);
	kb_buf_free(&w.abs);
	if (w.failed) {
		kbuild_snap_idx_free(ix);
		return -1;
	}
	return 0;
}

/* ──────────────────────────────────────────────────────────────────────── */
/* Create                                                                   */

static void release_mounts(Manager *m, const char *path, KbBuf *released,
			   KbBuf *blockers)
{
	/* script/chroot/exec.sh leaves no mounts here: each entry mounts in a
	 * private namespace that goes away with it. A mount found under
	 * build/fs belongs to something else — a hand-made bind, a wrapper
	 * from another tree — and the phase's steps have all finished, so it
	 * is released rather than archived; the host's /dev or the repository
	 * inside a snapshot is gigabytes of the wrong thing. */
	(void)m;
	char (*mnt)[256] = kb_calloc(256, sizeof(*mnt));
	for (int lazy = 0; lazy < 2; lazy++) {
		int n = kbuild_snap_mounts_under(path, mnt, 256);
		for (int i = 0; i < n; i++) {
			KbArgv a = {0};
			kb_argv_add(&a, "umount");
			if (lazy)
				kb_argv_add(&a, "-l");
			kb_argv_add(&a, mnt[i]);
			kb_argv_end(&a);
			if (kb_run(&a) == 0)
				kb_buf_printf(released, "%s%s%s",
					      released->n ? ", " : "", mnt[i],
					      lazy ? " (lazy)" : "");
		}
		if (!kbuild_snap_mounts_under(path, mnt, 256))
			break;
	}
	int n = kbuild_snap_mounts_under(path, mnt, 256);
	for (int i = 0; i < n && i < 3; i++)
		kb_buf_printf(blockers, "%s%s", blockers->n ? ", " : "", mnt[i]);
	free(mnt);
}

static long long free_bytes(const char *path)
{
	struct statvfs st;
	if (statvfs(path, &st) < 0)
		return -1;
	return (long long)st.f_bavail * (long long)st.f_frsize;
}

/* Set by the TUI so a long tar keeps the screen alive. */
void snap_set_tick(Manager *m, void (*fn)(Manager *))
{
	tick_manager = m;
	tick_fn = fn;
}

/* tar takes the walk's list only when it can read NUL-separated names
 * verbatim and be told not to recurse into them. Without that, a snapshot is
 * always full and archived by tar's own recursion. */
static int tar_takes_lists(void)
{
	return tar_has("--no-recursion") && tar_has("--null") &&
	       tar_has("--files-from") && tar_has("--verbatim-files-from");
}

/* "<phase_dir>-<unix time>-<8 hex>": unique across retakes of one phase in
 * the same second, which is what makes a held snapshot distinguishable from
 * the one that replaced it. */
static void new_id(const char *phase_dir, char *out, size_t cap)
{
	unsigned r = 0;
	int fd = open("/dev/urandom", O_RDONLY | O_CLOEXEC);
	if (fd < 0 || read(fd, &r, sizeof(r)) != (ssize_t)sizeof(r))
		r = (unsigned)time(NULL) ^ ((unsigned)getpid() << 16) ^
		    (unsigned)(kb_now_s() * 1e6);
	if (fd >= 0)
		close(fd);
	snprintf(out, cap, "%.60s-%lld-%08x", phase_dir,
		 (long long)time(NULL), r);
}

/* .new-* and .trash-* under build/snapshots/ are a snapshot that never
 * committed and one whose removal never finished. Neither is ever read. */
static void sweep_staging(Manager *m)
{
	char **names = kb_listdir(m->snap_root, NULL);
	if (!names)
		return;
	for (char **e = names; *e; e++) {
		if (strncmp(*e, ".new-", 5) && strncmp(*e, ".trash-", 7))
			continue;
		char p[900];
		snprintf(p, sizeof(p), "%s/%s", m->snap_root, *e);
		kb_rmtree(p);
	}
	kb_strv_free(names);
}

/* "full", "layer on 40_lang", or one clause per path when they differ. */
void snap_kind(const KbuildSnapshot *all, int n, const KbuildSnapshot *sn,
	       char *out, size_t cap)
{
	KbBuf b = {0};
	char first[128] = "";
	int differ = 0;
	for (int i = 0; i < sn->nentries; i++) {
		const KbuildSnapEntry *e = &sn->entry[i];
		char one[128];
		if (!e->layer) {
			kb_strlcpy(one, "full", sizeof(one));
		} else {
			const KbuildSnapshot *b0 = kbuild_snap_by_id(all, n,
								     e->base_id);
			snprintf(one, sizeof(one), "layer on %s%s",
				 b0 ? b0->phase_dir : "(missing)",
				 b0 && b0->held ? " (held)" : "");
		}
		if (!i)
			kb_strlcpy(first, one, sizeof(first));
		else if (strcmp(first, one))
			differ = 1;
		kb_buf_printf(&b, "%s%s %s", i ? ", " : "", e->path, one);
	}
	kb_strlcpy(out, differ ? (b.p ? b.p : "") : first, cap);
	kb_buf_free(&b);
}

/* The base a new snapshot of `path` may be a layer on, or NULL for a full
 * one. The index and its head are what make a layer exact: the index lists
 * the tree as restoring `head` produces it, so the diff against it is what
 * the layer has to add. */
static const KbuildSnapshot *pick_base(Manager *m, const KbuildPhase *meta,
				       const char *path,
				       const KbuildSnapIndex *cur,
				       const KbuildSnapIndex *prev,
				       const KbuildSnapshot *all, int nall)
{
	if (m->full_snapshots || !prev)
		return NULL;
	if (prev->root_dev != cur->root_dev || prev->root_ino != cur->root_ino)
		return NULL;	/* the path was replaced wholesale */
	const KbuildSnapshot *head = kbuild_snap_by_id(all, nall, prev->head);
	if (!head)
		return NULL;
	/* The tree may be AHEAD of head's phase (a later phase's snapshot was
	 * written after it) only by being this phase's own partial snapshot. */
	int order = strcmp(head->phase_dir, meta->dir_name);
	if (order > 0 || (order == 0 && head->complete))
		return NULL;
	const KbuildSnapshot *chain[KBUILD_MAX_CHAIN];
	int len = kbuild_snap_chain(all, nall, head, path, chain,
				    KBUILD_MAX_CHAIN);
	if (len < 0 || len + 1 > KBUILD_MAX_CHAIN)
		return NULL;
	return head;
}

typedef struct {
	KbuildSnapIndex ix;
	KbuildSnapEntry e;
	const KbuildSnapshot *base;
	char list[1100];
	long long est_raw, listed;
	double ratio;
} PathWork;

static int write_nul_list(const char *file, const KbBuf *b, char *err,
			  size_t errcap)
{
	if (kb_write_all(file, b->p ? b->p : "", b->n) < 0) {
		snprintf(err, errcap, "cannot write %s: %s", file,
			 strerror(errno));
		return -1;
	}
	return 0;
}

/* The directory a snapshot is moved to when something still layers on it. */
static void held_path(Manager *m, const KbuildSnapshot *sn, char *out,
		      size_t cap)
{
	snprintf(out, cap, "%s/%s/%s@%s", m->snap_root, KBUILD_HELD_DIR,
		 sn->phase_dir, sn->id);
}

int snap_gc(Manager *m)
{
	KbuildSnapshot *all = kb_calloc(KBUILD_MAX_SNAPS, sizeof(*all));
	int nall = kbuild_snap_list_all(m->snap_root, all, KBUILD_MAX_SNAPS);
	int idx[KBUILD_MAX_SNAPS];
	int n = kbuild_snap_gc_set(all, nall, idx, KBUILD_MAX_SNAPS);
	for (int i = 0; i < n; i++) {
		char dir[900];
		kbuild_snap_dir(m->snap_root, all[idx[i]].dir_name, dir,
				sizeof(dir));
		kb_rmtree(dir);
	}
	free(all);
	return n;
}

int snap_create(Manager *m, BStep *group, char *err, size_t errcap)
{
	const KbuildPhase *meta = group->meta;

	char target[256];
	if (kbuild_snap_interrupted(m->build_dir, target, sizeof(target))) {
		snprintf(err, errcap,
			 "build/ is mid-restore of %s; refusing to snapshot it",
			 target[0] ? target : "?");
		return -1;
	}

	/* Present paths only; a phase whose declared paths do not exist yet is
	 * skipped rather than recorded as an empty snapshot. */
	char paths[KBUILD_MAX_PATHS][128];
	int npath = 0;
	for (int i = 0; i < meta->nsnap; i++) {
		char full[900];
		snprintf(full, sizeof(full), "%s/%s", m->build_dir,
			 meta->snap_path[i]);
		if (kb_path_exists(full))
			kb_strlcpy(paths[npath++], meta->snap_path[i], 128);
	}
	if (!npath)
		return 0;

	m->snap.active = 1;
	kb_strlcpy(m->snap.action, "preparing", sizeof(m->snap.action));
	kb_strlcpy(m->snap.phase, meta->dir_name, sizeof(m->snap.phase));
	kb_strlcpy(m->snap.current, "checking mounts...",
		   sizeof(m->snap.current));
	m->snap.path[0] = m->snap.layer[0] = 0;
	m->snap.bytes = m->snap.est_bytes = m->snap.files = 0;
	m->snap.est_files = 0;
	m->snap.started = kb_now_s();
	if (tick_fn)
		tick_fn(m);

	char fs_dir[900];
	snprintf(fs_dir, sizeof(fs_dir), "%s/fs", m->build_dir);
	char (*probe)[256] = kb_calloc(8, sizeof(*probe));
	int mounted = kbuild_snap_mounts_under(fs_dir, probe, 8);
	free(probe);
	if (mounted) {
		KbBuf released = {0}, blockers = {0};
		release_mounts(m, fs_dir, &released, &blockers);
		if (released.n)
			mgr_notice(m, "released leftover mount(s): %s",
				   released.p);
		if (blockers.n) {
			snprintf(err, errcap,
				 "mounts still active under build/fs: %s",
				 blockers.p);
			kb_buf_free(&released);
			kb_buf_free(&blockers);
			return -1;
		}
		kb_buf_free(&released);
		kb_buf_free(&blockers);
	}

	kb_mkdir_p(m->snap_root);
	sweep_staging(m);

	KbuildSnapshot *all = kb_calloc(KBUILD_MAX_SNAPS, sizeof(*all));
	int nall = kbuild_snap_list_all(m->snap_root, all, KBUILD_MAX_SNAPS);
	PathWork *pw = kb_calloc(KBUILD_MAX_PATHS, sizeof(*pw));
	int lists = tar_takes_lists();
	const char *codec = snap_codec();
	double started = kb_now_s();

	int done = 0;
	for (int i = 0; i < group->nchild; i++)
		if (group->child[i]->status == ST_DONE)
			done++;
	/* A phase that resolved to no work is complete, not partial. */
	int complete = !group->nchild || done == group->nchild;

	char id[KBUILD_SNAP_ID];
	new_id(meta->dir_name, id, sizeof(id));

	char stage[900];
	snprintf(stage, sizeof(stage), "%s/.new-%s", m->snap_root,
		 meta->dir_name);
	kb_rmtree(stage);
	if (kb_mkdir_p(stage) < 0) {
		snprintf(err, errcap, "cannot create %s: %s", stage,
			 strerror(errno));
		goto abort;
	}

	/* Walk and decide every path before the first archive, so the free
	 * space guard sees the whole snapshot's size and not its first path. */
	long long needed = 0;
	for (int i = 0; i < npath; i++) {
		PathWork *w = &pw[i];
		KbuildSnapEntry *e = &w->e;
		kb_strlcpy(e->path, paths[i], sizeof(e->path));
		kbuild_snap_archive_name(paths[i], codec, e->archive,
					 sizeof(e->archive));

		kb_strlcpy(m->snap.action, "indexing", sizeof(m->snap.action));
		kb_strlcpy(m->snap.path, paths[i], sizeof(m->snap.path));
		m->snap.files = m->snap.bytes = 0;
		if (tick_fn)
			tick_fn(m);
		if (snap_walk(m, paths[i], meta, &w->ix, err, errcap) < 0)
			goto abort;
		e->tree_files = (long long)w->ix.n;
		for (size_t k = 0; k < w->ix.n; k++)
			e->tree_bytes += w->ix.rec[k].alloc;

		KbuildSnapIndex prev;
		char idxf[900];
		kbuild_snap_idx_file(m->build_dir, paths[i], idxf, sizeof(idxf));
		int have_prev = lists && kbuild_snap_idx_read(idxf, &prev) == 0;
		w->base = lists ? pick_base(m, meta, paths[i], &w->ix,
					    have_prev ? &prev : NULL, all, nall)
				: NULL;

		KbBuf add = {0}, gone = {0};
		KbuildSnapDiff st = {0};
		if (w->base) {
			kbuild_snap_diff(&prev, &w->ix, &add, &gone, &st);
			e->layer = 1;
			kb_strlcpy(e->base_id, w->base->id, sizeof(e->base_id));
		} else {
			for (size_t k = 0; k < w->ix.n; k++) {
				const char *p = kbuild_idx_path(&w->ix, k);
				kb_buf_add(&add, p, strlen(p) + 1);
				st.listed_alloc += w->ix.rec[k].alloc;
			}
			st.listed = (long long)w->ix.n;
		}
		if (have_prev)
			kbuild_snap_idx_free(&prev);

		int rc = 0;
		if (lists) {
			snprintf(w->list, sizeof(w->list), "%s/%s.list", stage,
				 e->archive);
			rc = write_nul_list(w->list, &add, err, errcap);
		}
		if (!rc && st.removed) {
			kbuild_snap_gone_name(paths[i], e->removed,
					      sizeof(e->removed));
			char gp[1100];
			snprintf(gp, sizeof(gp), "%s/%s", stage, e->removed);
			rc = write_nul_list(gp, &gone, err, errcap);
			e->removed_count = st.removed;
		}
		kb_buf_free(&add);
		kb_buf_free(&gone);
		if (rc < 0)
			goto abort;

		w->est_raw = st.listed_alloc;
		w->listed = st.listed;
		e->bytes_raw = st.listed_alloc;
		m->snap.est_files = st.listed;
		/* No history: zstd -1 on a rootfs lands near 2x. */
		w->ratio = 0.5;
		const KbuildSnapEntry *be = w->base
			? kbuild_snap_entry(w->base, paths[i]) : NULL;
		if (be && be->bytes_raw > 0 && be->bytes_compressed > 0)
			w->ratio = (double)be->bytes_compressed /
				   (double)be->bytes_raw;
		needed += (long long)((double)w->est_raw * w->ratio);
	}

	long long freeb = free_bytes(m->snap_root);
	if (needed && freeb >= 0 && freeb < needed + needed / 5) {
		char want[32];
		kb_strlcpy(want, human_bytes(needed + needed / 5), sizeof(want));
		snprintf(err, errcap, "only %s free, need ~%s",
			 human_bytes(freeb), want);
		goto abort;
	}

	for (int i = 0; i < npath; i++) {
		if (m->stop_requested) {
			snprintf(err, errcap, "aborted");
			goto abort;
		}
		PathWork *w = &pw[i];
		KbuildSnapEntry *e = &w->e;

		kb_strlcpy(m->snap.action, "snapshot", sizeof(m->snap.action));
		kb_strlcpy(m->snap.path, paths[i], sizeof(m->snap.path));
		if (e->layer)
			snprintf(m->snap.layer, sizeof(m->snap.layer),
				 "layer on %.38s", w->base->phase_dir);
		else
			kb_strlcpy(m->snap.layer, "full",
				   sizeof(m->snap.layer));
		m->snap.bytes = m->snap.files = 0;
		m->snap.est_bytes = (long long)((double)w->est_raw * w->ratio);
		m->snap.est_files = lists ? w->listed : 0;
		m->snap.started = kb_now_s();
		m->snap.current[0] = 0;

		KbArgv tar = {0};
		kb_argv_add(&tar, "tar");
		for (int f = 0; WANTED_TAR_FLAGS[f]; f++)
			if (tar_has(WANTED_TAR_FLAGS[f]))
				kb_argv_add(&tar, WANTED_TAR_FLAGS[f]);
		kb_argv_add(&tar, "-C");
		kb_argv_add(&tar, m->build_dir);
		if (lists) {
			kb_argv_add(&tar, "--no-recursion");
			kb_argv_add(&tar, "--verbatim-files-from");
			kb_argv_add(&tar, "--null");
			kb_argv_add(&tar, "--files-from");
			kb_argv_add(&tar, w->list);
			kb_argv_add(&tar, "-cvf");
			kb_argv_add(&tar, "-");
		} else {
			/* tar's own --exclude matches unanchored, which the
			 * walk does not; this form is only the fallback for a
			 * tar that cannot take a list. */
			for (int x = 0; x < meta->nexclude; x++) {
				const char *pat = meta->snap_exclude[x];
				size_t pl = strlen(paths[i]);
				if (strcmp(pat, paths[i]) &&
				    !(!strncmp(pat, paths[i], pl) &&
				      pat[pl] == '/'))
					continue;
				kb_argv_add(&tar, "--exclude");
				kb_argv_add(&tar, pat);
			}
			kb_argv_add(&tar, "-cvf");
			kb_argv_add(&tar, "-");
			kb_argv_add(&tar, paths[i]);
		}
		kb_argv_end(&tar);

		KbArgv comp = {0};
		compress_cmd(&comp);

		char out[1100];
		snprintf(out, sizeof(out), "%s/%s", stage, e->archive);
		if (run_pipe(m, &tar, &comp, out, NAMES_FIRST_STDERR, 0,
			     tick_fn, err, errcap) < 0)
			goto abort;

		e->files = m->snap.files;
		e->bytes_compressed = file_size(out);
		if (lists)
			unlink(w->list);
	}

	char commit[64];
	int dirty = 0;
	snap_git_info(m->repo_root, commit, sizeof(commit), &dirty);

	time_t now = time(NULL);
	struct tm tmv;
	char iso[32] = "";
	if (localtime_r(&now, &tmv))
		strftime(iso, sizeof(iso), "%Y-%m-%dT%H:%M:%S", &tmv);

	KbBuf b = {0};
	kb_buf_printf(&b, "{\n  \"schema\": %d,\n", KBUILD_SNAP_SCHEMA);
	kb_buf_printf(&b, "  \"id\": \"%s\",\n", id);
	kb_buf_printf(&b, "  \"phase_dir\": \"%s\",\n", meta->dir_name);
	kb_buf_printf(&b, "  \"phase\": \"%s\",\n", meta->name);
	kb_buf_printf(&b, "  \"title\": \"%s\",\n", meta->title);
	kb_buf_printf(&b, "  \"created\": %.1f,\n", (double)now);
	kb_buf_printf(&b, "  \"created_iso\": \"%s\",\n", iso);
	if (commit[0])
		kb_buf_printf(&b, "  \"git_commit\": \"%s\",\n", commit);
	else
		kb_buf_printf(&b, "  \"git_commit\": null,\n");
	kb_buf_printf(&b, "  \"git_dirty\": %s,\n", dirty ? "true" : "false");
	kb_buf_printf(&b, "  \"duration_s\": %.1f,\n", step_duration(group));
	kb_buf_printf(&b, "  \"snapshot_s\": %.1f,\n", kb_now_s() - started);
	kb_buf_printf(&b, "  \"steps\": %d,\n", done);
	kb_buf_printf(&b, "  \"complete\": %s,\n", complete ? "true" : "false");
	kb_buf_printf(&b, "  \"total_steps\": %d,\n", group->nchild);
	kb_buf_str(&b, "  \"done_steps\": [");
	int emitted = 0;
	for (int i = 0; i < group->nchild; i++) {
		if (group->child[i]->status != ST_DONE)
			continue;
		kb_buf_printf(&b, "%s\n    \"%s\"", emitted ? "," : "",
			      group->child[i]->title);
		emitted++;
	}
	kb_buf_printf(&b, "%s],\n", emitted ? "\n  " : "");
	kb_buf_printf(&b, "  \"codec\": \"%s\",\n", codec);
	kb_buf_str(&b, "  \"paths\": [");
	for (int i = 0; i < npath; i++) {
		const KbuildSnapEntry *e = &pw[i].e;
		kb_buf_printf(&b, "%s\n    {\n      \"path\": \"%s\",\n"
			      "      \"archive\": \"%s\",\n"
			      "      \"kind\": \"%s\",\n",
			      i ? "," : "", e->path, e->archive,
			      e->layer ? "layer" : "full");
		if (e->layer)
			kb_buf_printf(&b, "      \"base\": \"%s\",\n", e->base_id);
		else
			kb_buf_str(&b, "      \"base\": null,\n");
		if (e->removed[0])
			kb_buf_printf(&b, "      \"removed\": \"%s\",\n",
				      e->removed);
		else
			kb_buf_str(&b, "      \"removed\": null,\n");
		kb_buf_printf(&b, "      \"removed_count\": %lld,\n"
			      "      \"bytes_raw\": %lld,\n"
			      "      \"bytes_compressed\": %lld,\n"
			      "      \"files\": %lld,\n"
			      "      \"tree_bytes\": %lld,\n"
			      "      \"tree_files\": %lld\n    }",
			      e->removed_count, e->bytes_raw,
			      e->bytes_compressed, e->files, e->tree_bytes,
			      e->tree_files);
	}
	kb_buf_printf(&b, "%s]\n}\n", npath ? "\n  " : "");

	char mpath[1000];
	snprintf(mpath, sizeof(mpath), "%s/%s", stage, KBUILD_MANIFEST);
	int wrc = kb_write_all(mpath, b.p, b.n);
	kb_buf_free(&b);
	if (wrc < 0) {
		snprintf(err, errcap, "cannot write %s: %s", mpath,
			 strerror(errno));
		goto abort;
	}

	/* Commit. The phase directory holds either the old snapshot or the
	 * new one at every instant: the old one is moved aside — into .held
	 * when anything still layers on it, else to a trash name — and the
	 * staged one renamed into its place. */
	char dest[900], trash[900];
	kbuild_snap_dir(m->snap_root, meta->dir_name, dest, sizeof(dest));
	snprintf(trash, sizeof(trash), "%s/.trash-%s", m->snap_root,
		 meta->dir_name);
	if (kb_is_dir(dest)) {
		KbuildSnapshot *old = kb_calloc(1, sizeof(*old));
		int moved = 0;
		if (kbuild_snap_load_dir(m->snap_root, meta->dir_name, old) == 0) {
			int needed_by = 0;
			for (int i = 0; i < npath && !needed_by; i++)
				needed_by = pw[i].e.layer &&
					    !strcmp(pw[i].e.base_id, old->id);
			int dep[KBUILD_MAX_SNAPS];
			if (!needed_by)
				needed_by = kbuild_snap_dependants(
					all, nall, old->id, dep,
					KBUILD_MAX_SNAPS) > 0;
			if (needed_by) {
				char hd[900], hp[1100];
				snprintf(hd, sizeof(hd), "%s/%s", m->snap_root,
					 KBUILD_HELD_DIR);
				kb_mkdir_p(hd);
				held_path(m, old, hp, sizeof(hp));
				kb_rmtree(hp);
				moved = rename(dest, hp) == 0;
				if (!moved) {
					snprintf(err, errcap, "cannot hold %s: "
						 "%s", meta->dir_name,
						 strerror(errno));
					free(old);
					goto abort;
				}
			}
		}
		free(old);
		if (!moved && rename(dest, trash) < 0) {
			snprintf(err, errcap, "cannot replace %s: %s",
				 meta->dir_name, strerror(errno));
			goto abort;
		}
	}
	if (rename(stage, dest) < 0) {
		snprintf(err, errcap, "cannot commit %s: %s", meta->dir_name,
			 strerror(errno));
		goto abort;
	}
	kb_rmtree(trash);

	/* The index goes last: until it names the new id, the next snapshot
	 * layers on the previous head, which is still exact. */
	if (lists) {
		char ld[900];
		snprintf(ld, sizeof(ld), "%s/%s", m->build_dir,
			 KBUILD_LINEAGE_DIR);
		kb_mkdir_p(ld);
		for (int i = 0; i < npath; i++) {
			KbuildSnapIndex *ix = &pw[i].ix;
			kb_strlcpy(ix->head, id, sizeof(ix->head));
			ix->phase_index = meta->index;
			kb_strlcpy(ix->phase_dir, meta->dir_name,
				   sizeof(ix->phase_dir));
			ix->partial = !complete;
			char idxf[900];
			kbuild_snap_idx_file(m->build_dir, paths[i], idxf,
					     sizeof(idxf));
			if (kbuild_snap_idx_write(idxf, ix) < 0)
				unlink(idxf);
		}
	}
	for (int i = 0; i < npath; i++)
		kbuild_snap_idx_free(&pw[i].ix);
	free(pw);
	free(all);
	snap_gc(m);
	m->snap.layer[0] = 0;
	return 1;

abort:
	/* The previous snapshot and the index files stay as they were: only
	 * the staging directory, with whatever was mid-write in it, goes. */
	kb_rmtree(stage);
	for (int i = 0; i < npath; i++)
		kbuild_snap_idx_free(&pw[i].ix);
	free(pw);
	free(all);
	m->snap.layer[0] = 0;
	return -1;
}

/* ──────────────────────────────────────────────────────────────────────── */
/* Restore                                                                  */

static void write_marker(Manager *m, const char *target,
			 const KbuildRestoreItem *plan, int n)
{
	char path[900];
	snprintf(path, sizeof(path), "%s/%s", m->build_dir,
		 KBUILD_RESTORE_MARKER);
	KbBuf b = {0};
	kb_buf_printf(&b, "{\"target\": \"%s\", \"paths\": [", target);
	for (int i = 0, k = 0; i < n; i++)
		if (!plan[i].seq)
			kb_buf_printf(&b, "%s\"%s\"", k++ ? ", " : "",
				      plan[i].path);
	kb_buf_printf(&b, "], \"started\": %.1f}", (double)time(NULL));
	kb_write_all(path, b.p, b.n);
	kb_buf_free(&b);
}

static void clear_marker(Manager *m)
{
	char path[900];
	snprintf(path, sizeof(path), "%s/%s", m->build_dir,
		 KBUILD_RESTORE_MARKER);
	unlink(path);
}

/* A .gone list, NUL-separated; every name must pass kbuild_snap_removal_ok
 * for `path`. Returns the names as a strv, or NULL with `err` set. */
static char **read_gone(const KbuildRestoreItem *it, char *err, size_t errcap)
{
	size_t len = 0;
	char *text = kb_read_all(it->removed, &len);
	if (!text) {
		snprintf(err, errcap, "cannot read %s", it->removed);
		return NULL;
	}
	size_t cap = 16, n = 0;
	char **v = kb_calloc(cap, sizeof(*v));
	for (size_t off = 0; off < len;) {
		const char *name = text + off;
		size_t l = strnlen(name, len - off);
		off += l + 1;
		if (!l)
			continue;
		if (!kbuild_snap_removal_ok(it->path, name)) {
			snprintf(err, errcap, "refusing removal of '%.200s' "
				 "listed by %s", name, it->removed);
			for (size_t k = 0; k < n; k++)
				free(v[k]);
			free(v);
			free(text);
			return NULL;
		}
		if (n + 1 >= cap) {
			cap *= 2;
			v = kb_realloc(v, cap * sizeof(*v));
		}
		v[n++] = kb_strdup(name);
	}
	v[n] = NULL;
	free(text);
	return v;
}

/* 1 when a directory above `rel` (or the snapshot path itself) is a symlink.
 * Deleting through one would delete wherever it points, outside build/. */
static int symlink_above(const char *build_dir, const char *rel)
{
	char p[2048];
	int base = snprintf(p, sizeof(p), "%s/", build_dir);
	if (base < 0 || (size_t)base + strlen(rel) >= sizeof(p))
		return 1;
	strcpy(p + base, rel);
	for (char *s = p + base; (s = strchr(s, '/')); s++) {
		*s = 0;
		struct stat st;
		int link = lstat(p, &st) == 0 && S_ISLNK(st.st_mode);
		*s = '/';
		if (link)
			return 1;
	}
	return 0;
}

/* Walk every restored path again and write its index with the chain's top as
 * head: after a restore the next snapshot layers on what was restored. The
 * walk uses the excludes of the phase the top snapshot came from, which are
 * what shaped the archives. A path that cannot be indexed simply has no
 * index, and its next snapshot is full. */
static void reindex(Manager *m, const KbuildRestoreItem *plan, int n)
{
	if (!tar_takes_lists())
		return;
	char ld[900];
	snprintf(ld, sizeof(ld), "%s/%s", m->build_dir, KBUILD_LINEAGE_DIR);
	kb_mkdir_p(ld);
	for (int i = 0; i < n; i++) {
		if (plan[i].seq != plan[i].nseq - 1)
			continue;
		const KbuildPhase *ph = kbuild_find(m->phase, m->nphase,
						    plan[i].source);
		KbuildPhase none;
		memset(&none, 0, sizeof(none));
		kb_strlcpy(m->snap.action, "indexing", sizeof(m->snap.action));
		kb_strlcpy(m->snap.path, plan[i].path, sizeof(m->snap.path));
		m->snap.layer[0] = 0;
		KbuildSnapIndex ix;
		char err[256];
		if (snap_walk(m, plan[i].path, ph ? ph : &none, &ix, err,
			      sizeof(err)) < 0) {
			mgr_notice(m, "cannot index %s after the restore (%s); "
				   "its next snapshot is full", plan[i].path,
				   err);
			continue;
		}
		kb_strlcpy(ix.head, plan[i].id, sizeof(ix.head));
		ix.phase_index = ph ? ph->index : -1;
		kb_strlcpy(ix.phase_dir, plan[i].source, sizeof(ix.phase_dir));
		ix.partial = !plan[i].complete;
		char idxf[900];
		kbuild_snap_idx_file(m->build_dir, plan[i].path, idxf,
				     sizeof(idxf));
		if (kbuild_snap_idx_write(idxf, &ix) < 0)
			unlink(idxf);
		kbuild_snap_idx_free(&ix);
	}
}

int snap_restore(Manager *m, const KbuildRestoreItem *plan, int n,
		 const char *target, char *err, size_t errcap)
{
	/* Validate everything before touching the tree, so a rejected plan
	 * leaves build/ exactly as it was — and unmarked. */
	for (int i = 0; i < n; i++) {
		if (!kbuild_safe_relpath(plan[i].path)) {
			snprintf(err, errcap, "refusing unsafe restore path: %s",
				 plan[i].path);
			return -1;
		}
		if (!kb_path_exists(plan[i].archive) ||
		    kb_is_dir(plan[i].archive)) {
			snprintf(err, errcap, "missing archive: %s",
				 plan[i].archive);
			return -1;
		}
		if (!plan[i].seq && plan[i].layer) {
			snprintf(err, errcap, "the chain for %s does not start "
				 "with a full archive", plan[i].path);
			return -1;
		}
		if (plan[i].removed[0]) {
			char **gone = read_gone(&plan[i], err, errcap);
			if (!gone)
				return -1;
			kb_strv_free(gone);
		}
	}

	/* The index files describe the tree being replaced; from here on they
	 * would lie, so they go before anything else does. */
	for (int i = 0; i < n; i++) {
		char idxf[900];
		kbuild_snap_idx_file(m->build_dir, plan[i].path, idxf,
				     sizeof(idxf));
		unlink(idxf);
	}

	/* From the first rmtree until the last member is extracted the tree is
	 * inconsistent; the marker is what lets the next run know that. */
	write_marker(m, target && *target ? target :
		     (n ? plan[0].source : "?"), plan, n);

	m->snap.active = 1;
	for (int i = 0; i < n; i++) {
		if (m->stop_requested) {
			snprintf(err, errcap, "aborted");
			return -1;
		}

		char dest[900];
		snprintf(dest, sizeof(dest), "%s/%s", m->build_dir, plan[i].path);
		if (!plan[i].seq) {
			if (kb_is_dir(dest) && !kb_is_link(dest)) {
				char (*mnt)[256] = kb_calloc(8, sizeof(*mnt));
				int nm = kbuild_snap_mounts_under(dest, mnt, 8);
				if (nm) {
					KbBuf b = {0};
					for (int k = 0; k < nm && k < 3; k++)
						kb_buf_printf(&b, "%s%s",
							      k ? ", " : "",
							      mnt[k]);
					snprintf(err, errcap,
						 "mounts active under %s: %s",
						 plan[i].path, b.p ? b.p : "");
					kb_buf_free(&b);
					free(mnt);
					return -1;
				}
				free(mnt);
				kb_rmtree(dest);
			} else if (kb_path_exists(dest) || kb_is_link(dest)) {
				unlink(dest);
			}

			char parent[900];
			kb_strlcpy(parent, dest, sizeof(parent));
			char *slash = strrchr(parent, '/');
			if (slash) {
				*slash = 0;
				kb_mkdir_p(parent);
			}
		} else if (plan[i].removed[0]) {
			char **gone = read_gone(&plan[i], err, errcap);
			if (!gone)
				return -1;
			for (char **g = gone; *g; g++) {
				if (symlink_above(m->build_dir, *g)) {
					snprintf(err, errcap,
						 "refusing to remove %.200s: a "
						 "directory above it is a "
						 "symlink", *g);
					kb_strv_free(gone);
					return -1;
				}
				char victim[2048];
				snprintf(victim, sizeof(victim), "%s/%s",
					 m->build_dir, *g);
				kb_rmtree(victim);
			}
			kb_strv_free(gone);
		}

		kb_strlcpy(m->snap.action, "restore", sizeof(m->snap.action));
		kb_strlcpy(m->snap.phase, plan[i].source, sizeof(m->snap.phase));
		kb_strlcpy(m->snap.path, plan[i].path, sizeof(m->snap.path));
		if (plan[i].nseq > 1)
			snprintf(m->snap.layer, sizeof(m->snap.layer),
				 "layer %d/%d", plan[i].seq + 1, plan[i].nseq);
		else
			m->snap.layer[0] = 0;
		m->snap.bytes = m->snap.files = 0;
		m->snap.est_bytes = plan[i].bytes_compressed;
		m->snap.est_files = plan[i].files;
		m->snap.started = kb_now_s();
		m->snap.current[0] = 0;

		KbArgv decomp = {0};
		kbuild_snap_decompressor(plan[i].codec, &decomp);
		/* kbuild_snap_decompressor ends the argv, so the archive has to
		 * be appended by rebuilding it. */
		KbArgv first = {0};
		for (int k = 0; decomp.v[k]; k++)
			kb_argv_add(&first, decomp.v[k]);
		kb_argv_add(&first, plan[i].archive);
		kb_argv_end(&first);

		KbArgv tar = {0};
		kb_argv_add(&tar, "tar");
		for (int f = 0; EXTRACT_TAR_FLAGS[f]; f++)
			if (tar_has(EXTRACT_TAR_FLAGS[f]))
				kb_argv_add(&tar, EXTRACT_TAR_FLAGS[f]);
		/* --xattrs alone extracts only user.*: file capabilities
		 * (security.capability) are in the archive and would be
		 * dropped on the way out. */
		if (tar_has("--xattrs-include"))
			kb_argv_add(&tar, "--xattrs-include=*");
		if (geteuid() == 0)
			for (int f = 0; EXTRACT_TAR_FLAGS_ROOT[f]; f++)
				if (tar_has(EXTRACT_TAR_FLAGS_ROOT[f]))
					kb_argv_add(&tar, EXTRACT_TAR_FLAGS_ROOT[f]);
		kb_argv_add(&tar, "-C");
		kb_argv_add(&tar, m->build_dir);
		kb_argv_add(&tar, "-xvf");
		kb_argv_add(&tar, "-");
		kb_argv_end(&tar);

		if (run_pipe(m, &first, &tar, NULL, NAMES_SECOND_STDOUT,
			     file_size(plan[i].archive), tick_fn, err,
			     errcap) < 0)
			return -1;
	}

	reindex(m, plan, n);
	clear_marker(m);
	m->snap.layer[0] = 0;
	m->snap.active = 0;
	return 0;
}

/* The phase directories among `idx`, as "42_graphics…50_desktop" for a run of
 * more than two, else joined with ", ". */
static void name_dependants(const KbuildSnapshot *all, const int *idx, int n,
			    char *out, size_t cap)
{
	const char *names[KBUILD_MAX_SNAPS];
	int k = 0;
	for (int i = 0; i < n; i++)
		if (!all[idx[i]].held)
			names[k++] = all[idx[i]].dir_name;
	if (!k)
		snprintf(out, cap, "%d held snapshot%s", n, n == 1 ? "" : "s");
	else if (k > 2)
		snprintf(out, cap, "%s…%s", names[0], names[k - 1]);
	else if (k == 2)
		snprintf(out, cap, "%s, %s", names[0], names[1]);
	else
		snprintf(out, cap, "%s", names[0]);
}

int snap_delete(Manager *m, const char *dir_name, char *deps, size_t cap)
{
	if (deps && cap)
		deps[0] = 0;
	char dir[700];
	kbuild_snap_dir(m->snap_root, dir_name, dir, sizeof(dir));
	if (!kb_is_dir(dir))
		return 0;

	KbuildSnapshot *all = kb_calloc(KBUILD_MAX_SNAPS, sizeof(*all));
	int nall = kbuild_snap_list_all(m->snap_root, all, KBUILD_MAX_SNAPS);
	const KbuildSnapshot *sn = kbuild_snap_find(all, nall, dir_name);
	int rc = 1;
	if (sn) {
		int dep[KBUILD_MAX_SNAPS];
		int nd = kbuild_snap_dependants(all, nall, sn->id, dep,
						KBUILD_MAX_SNAPS);
		if (nd) {
			char hd[900], hp[1100];
			snprintf(hd, sizeof(hd), "%s/%s", m->snap_root,
				 KBUILD_HELD_DIR);
			kb_mkdir_p(hd);
			held_path(m, sn, hp, sizeof(hp));
			kb_rmtree(hp);
			if (rename(dir, hp) == 0) {
				rc = 2;
				if (deps && cap)
					name_dependants(all, dep, nd, deps, cap);
			}
		}
	}
	free(all);
	if (rc == 1)
		kb_rmtree(dir);
	snap_gc(m);
	return rc;
}
