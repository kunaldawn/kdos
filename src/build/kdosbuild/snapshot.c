/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   kdosbuild — writing and extracting snapshots
 *
 * libkbuild decides WHICH snapshot supplies what; this runs the tar. Two
 * processes, `tar | zstd` or `zstd -dc | tar`, with progress sampled off the
 * output file's size (creating) or the decompressor's /proc rchar (extracting).
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
#define SIZE_ESTIMATE_BUDGET 120.0

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

static void compress_cmd(KbArgv *a)
{
	const char *codec = snap_codec();
	if (!strcmp(codec, "zstd")) {
		kb_argv_add(a, "zstd");
		kb_argv_add(a, "-3");
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
 * what actually closes the loop.
 */

typedef struct {
	dev_t dev;
	ino_t ino;
} NodeKey;

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
	int nkey = 0, keycap = 1024;
	NodeKey *keys = kb_calloc((size_t)keycap, sizeof(*keys));
	keys[nkey].dev = st.st_dev;
	keys[nkey].ino = st.st_ino;
	nkey++;

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

			int seen = 0;
			for (int i = 0; i < nkey && !seen; i++)
				seen = keys[i].dev == cs.st_dev &&
				       keys[i].ino == cs.st_ino;
			if (seen)
				continue;	/* bind-mount loop back in */

			if (nkey == keycap) {
				keycap *= 2;
				NodeKey *nk = kb_calloc((size_t)keycap,
							sizeof(*nk));
				memcpy(nk, keys, (size_t)nkey * sizeof(*nk));
				free(keys);
				keys = nk;
			}
			keys[nkey].dev = cs.st_dev;
			keys[nkey].ino = cs.st_ino;
			nkey++;

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
	free(keys);
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

/* ──────────────────────────────────────────────────────────────────────── */
/* Create                                                                   */

static void release_mounts(Manager *m, const char *path, KbBuf *released,
			   KbBuf *blockers)
{
	/* These are chroot_exec.sh's own bind mounts and the phase's steps
	 * have all finished by now, so releasing them is what the wrapper
	 * intended anyway — one transient EBUSY at wrapper exit should not
	 * cost the phase its snapshot. */
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

static Manager *tick_manager;
static void (*tick_fn)(Manager *m);
static void measure_tick(long long files, long long bytes, void *user)
{
	Manager *m = user;
	m->snap.files = files;
	m->snap.bytes = bytes;
	if (tick_fn)
		tick_fn(m);
}

/* Set by the TUI so a long tar keeps the screen alive. */
void snap_set_tick(Manager *m, void (*fn)(Manager *))
{
	tick_manager = m;
	tick_fn = fn;
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
	m->snap.path[0] = 0;
	m->snap.bytes = m->snap.est_bytes = m->snap.files = 0;
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
			mgr_notice(m, "released stale chroot mount(s): %s",
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

	KbuildSnapshot prev;
	int have_prev = kbuild_snap_load(m->snap_root, meta->dir_name, &prev) == 0;
	long long prev_total = 0;
	if (have_prev)
		for (int i = 0; i < prev.nentries; i++)
			prev_total += prev.entry[i].bytes_compressed;

	/* Measuring an 8 GB tree costs tens of seconds on a cold cache and it
	 * only feeds the disk guard and an informational field. Walk it only
	 * when there is no previous figure to reuse, bound it, and stream
	 * progress so it cannot look like a hang. */
	long long raw[KBUILD_MAX_PATHS] = {0};
	double deadline = kb_now_s() + SIZE_ESTIMATE_BUDGET;
	for (int i = 0; i < npath; i++) {
		long long known = 0;
		if (have_prev)
			for (int k = 0; k < prev.nentries; k++)
				if (!strcmp(prev.entry[k].path, paths[i]))
					known = prev.entry[k].bytes_raw;
		if (known) {
			raw[i] = known;
			continue;
		}
		kb_strlcpy(m->snap.action, "measuring", sizeof(m->snap.action));
		kb_strlcpy(m->snap.path, paths[i], sizeof(m->snap.path));
		snprintf(m->snap.current, sizeof(m->snap.current),
			 "sizing %.120s...", paths[i]);
		m->snap.files = m->snap.bytes = 0;
		if (tick_fn)
			tick_fn(m);

		char full[900];
		snprintf(full, sizeof(full), "%.700s/%.150s", m->build_dir,
			 paths[i]);
		Usage u = dir_usage(full, deadline, measure_tick, m);
		raw[i] = u.bytes;
	}

	/* No history for the first snapshot of a phase: zstd -3 on a rootfs
	 * lands near 3x, so a third of the raw size is the estimate. */
	long long needed = prev_total;
	if (!needed) {
		long long sum = 0;
		for (int i = 0; i < npath; i++)
			sum += raw[i];
		needed = sum / 3;
	}
	long long freeb = free_bytes(m->build_dir);
	if (needed && freeb >= 0 && freeb < needed + needed / 5) {
		char want[32];
		kb_strlcpy(want, human_bytes(needed + needed / 5), sizeof(want));
		snprintf(err, errcap, "only %s free, need ~%s",
			 human_bytes(freeb), want);
		return -1;
	}

	char dest[700];
	kbuild_snap_dir(m->snap_root, meta->dir_name, dest, sizeof(dest));
	kb_mkdir_p(dest);

	const char *codec = snap_codec();
	KbuildSnapEntry entry[KBUILD_MAX_PATHS];
	char tmp_path[KBUILD_MAX_PATHS][900];
	int nentry = 0;
	double started = kb_now_s();

	for (int i = 0; i < npath; i++) {
		if (m->stop_requested) {
			snprintf(err, errcap, "aborted");
			goto abort;
		}

		KbuildSnapEntry *e = &entry[nentry];
		memset(e, 0, sizeof(*e));
		kb_strlcpy(e->path, paths[i], sizeof(e->path));
		kbuild_snap_archive_name(paths[i], codec, e->archive,
					 sizeof(e->archive));
		e->bytes_raw = raw[i];
		snprintf(tmp_path[nentry], sizeof(tmp_path[0]), "%s/%s.tmp",
			 dest, e->archive);

		long long est = 0;
		long long est_files = 0;
		if (have_prev)
			for (int k = 0; k < prev.nentries; k++)
				if (!strcmp(prev.entry[k].path, paths[i])) {
					est = prev.entry[k].bytes_compressed;
					est_files = prev.entry[k].files;
				}

		kb_strlcpy(m->snap.action, "snapshot", sizeof(m->snap.action));
		kb_strlcpy(m->snap.path, paths[i], sizeof(m->snap.path));
		m->snap.bytes = m->snap.files = 0;
		m->snap.est_bytes = est;
		m->snap.est_files = est_files;
		m->snap.started = kb_now_s();
		m->snap.current[0] = 0;

		KbArgv tar = {0};
		kb_argv_add(&tar, "tar");
		for (int f = 0; WANTED_TAR_FLAGS[f]; f++)
			if (tar_has(WANTED_TAR_FLAGS[f]))
				kb_argv_add(&tar, WANTED_TAR_FLAGS[f]);
		kb_argv_add(&tar, "-C");
		kb_argv_add(&tar, m->build_dir);
		for (int x = 0; x < meta->nexclude; x++) {
			const char *pat = meta->snap_exclude[x];
			size_t pl = strlen(paths[i]);
			if (strcmp(pat, paths[i]) &&
			    !(!strncmp(pat, paths[i], pl) && pat[pl] == '/'))
				continue;
			kb_argv_add(&tar, "--exclude");
			kb_argv_add(&tar, pat);
		}
		kb_argv_add(&tar, "-cvf");
		kb_argv_add(&tar, "-");
		kb_argv_add(&tar, paths[i]);
		kb_argv_end(&tar);

		KbArgv comp = {0};
		compress_cmd(&comp);

		if (run_pipe(m, &tar, &comp, tmp_path[nentry],
			     NAMES_FIRST_STDERR, 0, tick_fn, err, errcap) < 0)
			goto abort;

		e->files = m->snap.files;
		e->bytes_compressed = file_size(tmp_path[nentry]);
		nentry++;
	}

	/* Past this point the old archives are replaced; the manifest is
	 * rewritten immediately after, so the window of inconsistency is a
	 * handful of renames rather than the whole compression run. */
	char mpath[900];
	snprintf(mpath, sizeof(mpath), "%s/%s", dest, KBUILD_MANIFEST);
	unlink(mpath);
	for (int i = 0; i < nentry; i++) {
		char final[900];
		snprintf(final, sizeof(final), "%s/%s", dest, entry[i].archive);
		if (rename(tmp_path[i], final) < 0) {
			snprintf(err, errcap, "rename %s: %s", entry[i].archive,
				 strerror(errno));
			return -1;
		}
	}

	int done = 0;
	for (int i = 0; i < group->nchild; i++)
		if (group->child[i]->status == ST_DONE)
			done++;
	/* A phase that resolved to no work is complete, not partial. */
	int complete = !group->nchild || done == group->nchild;

	char commit[64];
	int dirty = 0;
	snap_git_info(m->repo_root, commit, sizeof(commit), &dirty);

	time_t now = time(NULL);
	struct tm tmv;
	char iso[32] = "";
	if (localtime_r(&now, &tmv))
		strftime(iso, sizeof(iso), "%Y-%m-%dT%H:%M:%S", &tmv);

	KbBuf b = {0};
	kb_buf_printf(&b, "{\n  \"schema\": 3,\n");
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
	kb_buf_str(&b, "  \"entries\": [");
	for (int i = 0; i < nentry; i++)
		kb_buf_printf(&b, "%s\n    {\n      \"path\": \"%s\",\n"
			      "      \"archive\": \"%s\",\n"
			      "      \"bytes_raw\": %lld,\n"
			      "      \"bytes_compressed\": %lld,\n"
			      "      \"files\": %lld\n    }",
			      i ? "," : "", entry[i].path, entry[i].archive,
			      entry[i].bytes_raw, entry[i].bytes_compressed,
			      entry[i].files);
	kb_buf_printf(&b, "%s]\n}", nentry ? "\n  " : "");

	char tmpman[950];
	snprintf(tmpman, sizeof(tmpman), "%s.tmp", mpath);
	kb_write_all(tmpman, b.p, b.n);
	kb_buf_free(&b);
	if (rename(tmpman, mpath) < 0) {
		snprintf(err, errcap, "rename manifest: %s", strerror(errno));
		return -1;
	}

	/* Drop archives left over from a previous set of declared paths. */
	char **left = kb_listdir(dest, NULL);
	if (left) {
		for (char **f = left; *f; f++) {
			int keep = !strcmp(*f, KBUILD_MANIFEST);
			for (int i = 0; i < nentry && !keep; i++)
				keep = !strcmp(*f, entry[i].archive);
			if (keep)
				continue;
			char victim[950];
			snprintf(victim, sizeof(victim), "%s/%s", dest, *f);
			if (kb_is_dir(victim))
				kb_rmtree(victim);
			else
				unlink(victim);
		}
		kb_strv_free(left);
	}
	return 1;

abort:
	/* Leave the previous snapshot intact: drop every partial archive,
	 * including the one that was mid-write when this blew up. */
	{
		char **junk = kb_listdir(dest, NULL);
		if (junk) {
			for (char **f = junk; *f; f++) {
				size_t l = strlen(*f);
				if (l < 4 || strcmp(*f + l - 4, ".tmp"))
					continue;
				char victim[950];
				snprintf(victim, sizeof(victim), "%s/%s", dest, *f);
				unlink(victim);
			}
			kb_strv_free(junk);
		}
	}
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
	for (int i = 0; i < n; i++)
		kb_buf_printf(&b, "%s\"%s\"", i ? ", " : "", plan[i].path);
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
		if (kb_is_dir(dest) && !kb_is_link(dest)) {
			char (*mnt)[256] = kb_calloc(8, sizeof(*mnt));
			int nm = kbuild_snap_mounts_under(dest, mnt, 8);
			if (nm) {
				KbBuf b = {0};
				for (int k = 0; k < nm && k < 3; k++)
					kb_buf_printf(&b, "%s%s", k ? ", " : "",
						      mnt[k]);
				snprintf(err, errcap, "mounts active under %s: %s",
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

		kb_strlcpy(m->snap.action, "restore", sizeof(m->snap.action));
		kb_strlcpy(m->snap.phase, plan[i].source, sizeof(m->snap.phase));
		kb_strlcpy(m->snap.path, plan[i].path, sizeof(m->snap.path));
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

	clear_marker(m);
	m->snap.active = 0;
	return 0;
}

int snap_delete(Manager *m, const char *dir_name)
{
	char dir[700];
	kbuild_snap_dir(m->snap_root, dir_name, dir, sizeof(dir));
	if (!kb_is_dir(dir))
		return 0;
	kb_rmtree(dir);
	return 1;
}
