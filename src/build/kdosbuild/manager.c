/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   kdosbuild — the execution order and the step runner
 * ---------------------------------
 */

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#include "kdosbuild.h"

/* ──────────────────────────────────────────────────────────────────────── */
/* Steps                                                                    */

static BStep *step_new(const char *path, int is_group)
{
	BStep *s = kb_calloc(1, sizeof(*s));
	kb_strlcpy(s->path, path, sizeof(s->path));
	s->is_group = is_group;
	s->status = ST_PENDING;
	s->step_type = SX_HOST;
	s->expanded = 1;
	s->logcap = 64;
	s->log = kb_calloc((size_t)s->logcap, sizeof(*s->log));
	return s;
}

/* `# Title: ...` in the first five lines, else the filename tidied. The
 * python matched it case-insensitively and so does this. */
static void step_derive_title(BStep *s)
{
	if (!s->is_group) {
		FILE *f = fopen(s->path, "r");
		if (f) {
			char line[512];
			for (int i = 0; i < 5 && fgets(line, sizeof(line), f); i++) {
				char *p = line;
				while (*p == ' ' || *p == '\t')
					p++;
				if (*p != '#')
					continue;
				p++;
				while (*p == ' ' || *p == '\t')
					p++;
				if (strncasecmp(p, "Title:", 6))
					continue;
				p += 6;
				while (*p == ' ' || *p == '\t')
					p++;
				char *e = p + strlen(p);
				while (e > p && (e[-1] == '\n' || e[-1] == '\r' ||
						 e[-1] == ' ' || e[-1] == '\t'))
					e--;
				*e = 0;
				if (*p) {
					kb_strlcpy(s->title, p, sizeof(s->title));
					fclose(f);
					return;
				}
			}
			fclose(f);
		}
	}

	char name[256];
	kb_strlcpy(name, kb_basename(s->path), sizeof(name));
	char *dot = strrchr(name, '.');
	if (dot && dot != name)
		*dot = 0;
	char *p = name;
	while (isdigit((unsigned char)*p))
		p++;
	if (p != name && *p == '_')
		p++;
	else
		p = name;
	for (char *c = p; *c; c++)
		if (*c == '_' || *c == '-')
			*c = ' ';
	kb_strlcpy(s->title, p, sizeof(s->title));
}

/* Log lines are stripped of ANSI, tabs become four spaces and control
 * characters are dropped — the cell buffer draws exactly what it is given, so
 * a stray escape from a build script would otherwise repaint the screen. */
static void step_log(BStep *s, const char *line)
{
	char out[4096];
	size_t o = 0;
	for (const char *p = line; *p && o < sizeof(out) - 4; p++) {
		if (*p == 0x1b) {
			p++;
			if (*p == '[') {
				p++;
				while (*p && !(*p >= '@' && *p <= '~'))
					p++;
			} else if (*p) {
				/* two-character sequence */
			}
			if (!*p)
				break;
			continue;
		}
		if (*p == '\t') {
			for (int i = 0; i < 4 && o < sizeof(out) - 1; i++)
				out[o++] = ' ';
			continue;
		}
		if ((unsigned char)*p < 32)
			continue;
		out[o++] = *p;
	}
	out[o] = 0;

	if (s->nlog == s->logcap) {
		if (s->logcap >= KB_MAX_LOG) {
			free(s->log[0]);
			memmove(s->log, s->log + 1,
				(size_t)(s->nlog - 1) * sizeof(*s->log));
			s->nlog--;
		} else {
			s->logcap *= 2;
			char **nv = kb_calloc((size_t)s->logcap, sizeof(*nv));
			memcpy(nv, s->log, (size_t)s->nlog * sizeof(*nv));
			free(s->log);
			s->log = nv;
		}
	}
	s->log[s->nlog++] = kb_strdup(out);
}

double step_duration(const BStep *s)
{
	if (s->is_group) {
		double total = 0;
		for (int i = 0; i < s->nchild; i++)
			total += step_duration(s->child[i]);
		return total;
	}
	if (s->start_time <= 0)
		return 0;
	return (s->end_time > 0 ? s->end_time : kb_now_s()) - s->start_time;
}

void step_timing_key(const BStep *s, char *out, size_t cap)
{
	const char *parent = "root";
	if (s->parent && s->parent->meta)
		parent = s->parent->meta->dir_name;
	snprintf(out, cap, "%s/%s", parent, s->title);
}

static void step_free(BStep *s)
{
	if (!s)
		return;
	for (int i = 0; i < s->nchild; i++)
		step_free(s->child[i]);
	for (int i = 0; i < s->nlog; i++)
		free(s->log[i]);
	free(s->log);
	free(s->cmd_line);
	free(s);
}

/* ──────────────────────────────────────────────────────────────────────── */
/* Construction                                                             */

static void renumber(Manager *m)
{
	for (int i = 0; i < m->nroot; i++) {
		m->root[i]->step_number = i + 1;
		for (int j = 0; j < m->root[i]->nchild; j++)
			m->root[i]->child[j]->step_number = j;
	}
}

void mgr_init(Manager *m, const char *script_dir, const char *build_dir)
{
	memset(m, 0, sizeof(*m));
	m->child = -1;
	m->child_fd = -1;
	m->log_fd = -1;
	m->snapshot_enabled = 1;

	char *abs = realpath(script_dir, NULL);
	kb_strlcpy(m->script_dir, abs ? abs : script_dir, sizeof(m->script_dir));
	free(abs);
	kb_strlcpy(m->build_dir, build_dir, sizeof(m->build_dir));

	/* chroot_exec.sh bind-mounts the repo root at /kdos and cds there, so
	 * anything handed to a chroot command has to be repo-relative: a host
	 * path like /workspace/script/phase2.env.sh does not exist inside. */
	kb_strlcpy(m->repo_root, m->script_dir, sizeof(m->repo_root));
	char *slash = strrchr(m->repo_root, '/');
	if (slash && slash != m->repo_root)
		*slash = 0;

	snprintf(m->snap_root, sizeof(m->snap_root), "%.480s/snapshots",
		 m->build_dir);
	snprintf(m->chroot_exec, sizeof(m->chroot_exec), "%.560s/chroot_exec.sh",
		 m->script_dir);
	m->nphase = kbuild_discover(m->script_dir, m->phase, KBUILD_MAX_PHASES);

	for (int i = 0; i < m->nphase; i++) {
		const KbuildPhase *p = &m->phase[i];
		char label[128];
		kbuild_label(p, label, sizeof(label));

		BStep *g = step_new(label, 1);
		kb_strlcpy(g->title, label, sizeof(g->title));
		g->meta = p;
		g->step_type = p->chroot ? SX_CHROOT : SX_HOST;

		char pkgs[600];
		snprintf(pkgs, sizeof(pkgs), "%s/packages.txt", p->dir_path);
		if (kb_path_exists(pkgs) && !kb_is_dir(pkgs))
			kb_strlcpy(g->packages_file, pkgs, sizeof(g->packages_file));
		else
			kb_strlcpy(g->script_dir, p->dir_path, sizeof(g->script_dir));

		m->root[m->nroot++] = g;
		m->order[m->norder++] = g;
	}
	renumber(m);
}

/* A plan deselects whole phases up front; mark_continued and mark_restored
 * only ever add more skips on top of that. Called after the plan is known. */
void mgr_apply_plan(Manager *m)
{
	if (!m->have_plan)
		return;
	for (int i = 0; i < m->nroot; i++) {
		if (kbuild_plan_phase_selected(&m->plan, m->root[i]->meta->dir_name))
			continue;
		m->root[i]->status = ST_SKIPPED;
		kb_strlcpy(m->root[i]->note, "not in plan",
			   sizeof(m->root[i]->note));
	}
}

void mgr_free(Manager *m)
{
	for (int i = 0; i < m->nroot; i++)
		step_free(m->root[i]);
	m->nroot = m->norder = 0;
}

void mgr_notice(Manager *m, const char *fmt, ...)
{
	char text[256];
	va_list ap;
	va_start(ap, fmt);
	vsnprintf(text, sizeof(text), fmt, ap);
	va_end(ap);

	if (m->nnotice == KB_MAX_NOTICE) {
		memmove(m->notice, m->notice + 1,
			sizeof(m->notice[0]) * (KB_MAX_NOTICE - 1));
		m->nnotice--;
	}
	m->notice[m->nnotice].when = kb_now_s();
	kb_strlcpy(m->notice[m->nnotice].text, text,
		   sizeof(m->notice[0].text));
	m->nnotice++;

	/* The in-process list dies with the TUI, so keep a durable copy: a
	 * snapshot that failed on the last phase must stay discoverable. */
	char dir[600], path[700];
	snprintf(dir, sizeof(dir), "%s/logs", m->build_dir);
	kb_mkdir_p(dir);
	snprintf(path, sizeof(path), "%s/snapshots.log", dir);
	int fd = open(path, O_WRONLY | O_CREAT | O_APPEND | O_CLOEXEC, 0644);
	if (fd >= 0) {
		time_t now = time(NULL);
		struct tm tmv;
		char stamp[32] = "";
		if (localtime_r(&now, &tmv))
			strftime(stamp, sizeof(stamp), "%Y-%m-%d %H:%M:%S", &tmv);
		dprintf(fd, "%s %s\n", stamp, text);
		close(fd);
	}
}

/* Resume at `phase_index` on the existing tree without restoring. Earlier
 * phases are skipped, so they are not re-run and — importantly — not
 * re-snapshotted: snapshotting phase 2 again from a tree that already holds
 * phase 3's packages would file a mislabelled archive. */
void mgr_mark_continued(Manager *m, int phase_index)
{
	for (int i = 0; i < m->nroot; i++)
		if (m->root[i]->meta->index < phase_index) {
			m->root[i]->status = ST_SKIPPED;
			m->continued_from = m->root[i]->meta;
		}
}

/* `resume_inside` is for a PARTIAL snapshot: the phase it was taken from
 * still has steps left, so it is left pending and re-runs. kpkg skips
 * packages that are already installed, so re-running is cheap. */
void mgr_mark_restored(Manager *m, int phase_index, int resume_inside)
{
	int ceiling = resume_inside ? phase_index - 1 : phase_index;
	for (int i = 0; i < m->nroot; i++) {
		if (m->root[i]->meta->index <= ceiling) {
			m->root[i]->status = ST_SKIPPED;
			m->restored_from = m->root[i]->meta;
		} else if (resume_inside &&
			   m->root[i]->meta->index == phase_index) {
			m->resumed_inside = m->root[i]->meta;
		}
	}
}

BStep *mgr_phase_of(BStep *s)
{
	if (!s)
		return NULL;
	return s->is_group ? s : s->parent;
}

/* ──────────────────────────────────────────────────────────────────────── */
/* Command construction                                                     */

static void path_for(const Manager *m, const char *path, int step_type,
		     char *out, size_t cap)
{
	if (step_type != SX_CHROOT) {
		kb_strlcpy(out, path, cap);
		return;
	}
	char *abs = realpath(path, NULL);
	const char *full = abs ? abs : path;
	size_t rl = strlen(m->repo_root);
	if (!strncmp(full, m->repo_root, rl) && full[rl] == '/')
		kb_strlcpy(out, full + rl + 1, cap);
	else
		kb_strlcpy(out, full, cap);
	free(abs);
}

static void cmd_prefix(const Manager *m, int step_type, KbArgv *a)
{
	if (step_type == SX_CHROOT)
		kb_argv_add(a, m->chroot_exec);
	kb_argv_add(a, "bash");
	kb_argv_add(a, "-c");
}

static void env_source(const Manager *m, const BStep *g, char *out, size_t cap)
{
	out[0] = 0;
	if (!g->meta || !g->meta->env_file[0])
		return;
	char p[512];
	path_for(m, g->meta->env_file, g->step_type, p, sizeof(p));
	snprintf(out, cap, "source %s && ", p);
}

/* ──────────────────────────────────────────────────────────────────────── */
/* Running one child                                                        */

void log_path_for(const Manager *m, const BStep *s, char *out, size_t cap)
{
	/* build/logs/<phase dir>/<NNNN>_<name>.log — the path the user is told
	 * to tail when a port fails. */
	char rel[512] = "";
	size_t rl = strlen(m->script_dir);
	if (!strncmp(s->path, m->script_dir, rl) && s->path[rl] == '/')
		kb_strlcpy(rel, s->path + rl + 1, sizeof(rel));
	else
		kb_strlcpy(rel, s->path, sizeof(rel));
	char *slash = strrchr(rel, '/');
	if (slash)
		*slash = 0;
	else
		rel[0] = 0;

	char name[256];
	kb_strlcpy(name, kb_basename(s->path), sizeof(name));
	char *p = name;
	while (isdigit((unsigned char)*p))
		p++;
	if (p != name && *p == '_')
		p++;
	else
		p = name;

	if (rel[0])
		snprintf(out, cap, "%s/logs/%s/%04d_%s.log", m->build_dir, rel,
			 s->step_number, p);
	else
		snprintf(out, cap, "%s/logs/%04d_%s.log", m->build_dir,
			 s->step_number, p);
}

/* True when the plan named this exact script, not merely its phase. Many
 * steps guard themselves with a mark file and exit 0 on a second pass, so a
 * step the user picked has to be told to run anyway. */
static int explicitly_selected(const Manager *m, const BStep *s)
{
	if (!m->have_plan || s->is_group || !s->parent || !s->parent->meta)
		return 0;
	const char *dir = s->parent->meta->dir_name;
	const char *base = kb_basename(s->path);
	for (int i = 0; i < m->plan.nsteps; i++) {
		if (strcmp(m->plan.steps[i].dir, dir))
			continue;
		for (int k = 0; k < m->plan.steps[i].n; k++)
			if (!strcmp(m->plan.steps[i].step[k], base))
				return 1;
		return 0;
	}
	return 0;
}

static void set_family_status(BStep *s, int status)
{
	s->status = status;
	for (BStep *c = s->parent; c; c = c->parent) {
		int running = 0, failed = 0, done = 0, started = 0, active = 0;
		for (int i = 0; i < c->nchild; i++) {
			if (c->child[i]->status == ST_SKIPPED)
				continue;
			active++;
			if (c->child[i]->status == ST_RUNNING)
				running = 1;
			if (c->child[i]->status == ST_FAIL)
				failed = 1;
			if (c->child[i]->status == ST_DONE)
				done++;
			if (c->child[i]->status != ST_PENDING)
				started = 1;
		}
		if (failed)
			c->status = ST_FAIL;
		else if (running)
			c->status = ST_RUNNING;
		else if (active && done == active)
			c->status = ST_DONE;
		else if (started)
			c->status = ST_RUNNING;
		else
			c->status = ST_PENDING;
	}
}

static void start_step(Manager *m, BStep *s)
{
	set_family_status(s, ST_RUNNING);
	s->start_time = kb_now_s();
	s->end_time = 0;

	char logp[900];
	log_path_for(m, s, logp, sizeof(logp));
	char dir[900];
	kb_strlcpy(dir, logp, sizeof(dir));
	char *slash = strrchr(dir, '/');
	if (slash) {
		*slash = 0;
		kb_mkdir_p(dir);
	}
	m->log_fd = open(logp, O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, 0644);

	KbArgv a = {0};
	if (s->have_cmd) {
		a = s->cmd;
	} else if (s->step_type == SX_CHROOT) {
		char rel[512];
		path_for(m, s->path, s->step_type, rel, sizeof(rel));
		kb_argv_add(&a, m->chroot_exec);
		kb_argv_add(&a, "bash");
		kb_argv_add(&a, rel);
		kb_argv_end(&a);
	} else {
		kb_argv_add(&a, "bash");
		kb_argv_add(&a, s->path);
		kb_argv_end(&a);
	}

	int pipefd[2];
	if (pipe(pipefd) < 0) {
		step_log(s, "INTERNAL ERROR: pipe failed");
		s->return_code = 999;
		return;
	}

	pid_t pid = fork();
	if (pid < 0) {
		close(pipefd[0]);
		close(pipefd[1]);
		step_log(s, "INTERNAL ERROR: fork failed");
		s->return_code = 999;
		return;
	}
	if (pid == 0) {
		/* Its own process group, so a stop can reach the WHOLE tree.
		 * Without this, `kill(child)` signals only bash — and `make`
		 * and every compiler under it survive, keep the pipe's write
		 * end open, and the drain loop waits for an EOF that never
		 * comes. That is a build the user cannot quit. */
		setsid();
		close(pipefd[0]);
		dup2(pipefd[1], STDOUT_FILENO);
		dup2(pipefd[1], STDERR_FILENO);
		close(pipefd[1]);
		int devnull = open("/dev/null", O_RDONLY);
		if (devnull >= 0) {
			dup2(devnull, STDIN_FILENO);
			close(devnull);
		}
		setenv("KDOS_REPLAY", explicitly_selected(m, s) ? "1" : "0", 1);
		execvp(a.v[0], (char *const *)a.v);
		_exit(127);
	}

	close(pipefd[1]);
	/* Non-blocking: the build IS the main loop, so a step that goes quiet
	 * for ten minutes must not stop the screen from redrawing. */
	fcntl(pipefd[0], F_SETFL, O_NONBLOCK);
	m->child = pid;
	m->child_fd = pipefd[0];
	m->child_len = 0;
	m->current_step = s;
}

/* Drain whatever the child has written. Returns 1 while it is still running. */
static int pump_child(Manager *m, BStep *s)
{
	for (;;) {
		ssize_t r = read(m->child_fd, m->child_buf + m->child_len,
				 sizeof(m->child_buf) - m->child_len - 1);
		if (r < 0) {
			if (errno == EINTR)
				continue;
			break;		/* EAGAIN — nothing more right now  */
		}
		if (r == 0) {
			/* EOF: flush whatever has no newline on it. */
			if (m->child_len) {
				m->child_buf[m->child_len] = 0;
				step_log(s, m->child_buf);
				m->total_lines++;
				if (m->log_fd >= 0)
					dprintf(m->log_fd, "%s\n", m->child_buf);
				m->child_len = 0;
			}
			close(m->child_fd);
			m->child_fd = -1;

			int status = 0;
			while (waitpid(m->child, &status, 0) < 0 && errno == EINTR)
				;
			m->child = -1;
			s->return_code = WIFEXITED(status) ? WEXITSTATUS(status)
							  : 128 + WTERMSIG(status);
			s->end_time = kb_now_s();
			m->child_killed = 0;
			if (m->log_fd >= 0) {
				close(m->log_fd);
				m->log_fd = -1;
			}
			return 0;
		}

		m->child_len += (size_t)r;
		m->child_buf[m->child_len] = 0;

		char *start = m->child_buf, *nl;
		while ((nl = strchr(start, '\n'))) {
			*nl = 0;
			char *e = nl;
			while (e > start && (e[-1] == '\r' || e[-1] == ' ' ||
					     e[-1] == '\t'))
				*--e = 0;
			step_log(s, start);
			m->total_lines++;
			if (m->log_fd >= 0)
				dprintf(m->log_fd, "%s\n", start);
			start = nl + 1;
		}
		size_t left = m->child_len - (size_t)(start - m->child_buf);
		memmove(m->child_buf, start, left);
		m->child_len = left;

		/* A line longer than the buffer: flush it rather than spin. */
		if (m->child_len >= sizeof(m->child_buf) - 1) {
			m->child_buf[m->child_len] = 0;
			step_log(s, m->child_buf);
			m->total_lines++;
			if (m->log_fd >= 0)
				dprintf(m->log_fd, "%s\n", m->child_buf);
			m->child_len = 0;
		}
	}
	return 1;
}

/* ──────────────────────────────────────────────────────────────────────── */
/* Expansion                                                                */

static void fail_expansion(Manager *m, BStep *s, const char *what,
			   const char *detail)
{
	s->status = ST_FAIL;
	char head[256];
	snprintf(head, sizeof(head), "%s failed for %s:", what, s->title);
	step_log(s, head);
	char copy[4096];
	kb_strlcpy(copy, detail, sizeof(copy));
	for (char *line = copy, *next; line && *line; line = next) {
		char *nl = strchr(line, '\n');
		next = nl ? nl + 1 : NULL;
		if (nl)
			*nl = 0;
		step_log(s, line);
	}

	char dir[900], path[1000];
	snprintf(dir, sizeof(dir), "%s/logs/%s", m->build_dir,
		 s->meta ? kb_basename(s->meta->dir_path) : "");
	kb_mkdir_p(dir);
	snprintf(path, sizeof(path), "%s/expansion.log", dir);
	int fd = open(path, O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, 0644);
	if (fd >= 0) {
		dprintf(fd, "%s failed\n%s\n", what, detail);
		close(fd);
	}

	mgr_notice(m, "%s: %s failed - see build/logs/.../expansion.log",
		   s->meta ? s->meta->dir_name : s->title, what);
	m->error_step = s;
	m->stop_requested = 1;
	m->is_running = 0;
}

static void order_insert(Manager *m, int at, BStep **nodes, int n)
{
	if (m->norder + n > KB_MAX_STEPS)
		n = KB_MAX_STEPS - m->norder;
	memmove(m->order + at + n, m->order + at,
		(size_t)(m->norder - at) * sizeof(*m->order));
	memcpy(m->order + at, nodes, (size_t)n * sizeof(*nodes));
	m->norder += n;
}

/* kpkgdepends prints the resolved order on stdout and NOTHING else, so stderr
 * is kept separate: merging it means any diagnostic written by the chroot
 * wrapper (or by a sourced env file) gets split on whitespace and installed as
 * a package. Every token is validated for the same reason. */
static int run_capture2(const KbArgv *a, KbBuf *out, KbBuf *err)
{
	int op[2], ep[2];
	if (pipe(op) < 0)
		return -1;
	if (pipe(ep) < 0) {
		close(op[0]);
		close(op[1]);
		return -1;
	}

	pid_t pid = fork();
	if (pid < 0) {
		close(op[0]); close(op[1]); close(ep[0]); close(ep[1]);
		return -1;
	}
	if (pid == 0) {
		close(op[0]);
		close(ep[0]);
		dup2(op[1], STDOUT_FILENO);
		dup2(ep[1], STDERR_FILENO);
		close(op[1]);
		close(ep[1]);
		execvp(a->v[0], (char *const *)a->v);
		_exit(127);
	}
	close(op[1]);
	close(ep[1]);

	struct pollfd pfd[2] = { { op[0], POLLIN, 0 }, { ep[0], POLLIN, 0 } };
	int open_fds = 2;
	while (open_fds > 0) {
		if (poll(pfd, 2, 500) < 0 && errno != EINTR)
			break;
		for (int i = 0; i < 2; i++) {
			if (pfd[i].fd < 0 || !pfd[i].revents)
				continue;
			char buf[4096];
			ssize_t r = read(pfd[i].fd, buf, sizeof(buf));
			if (r > 0) {
				kb_buf_add(i ? err : out, buf, (size_t)r);
			} else if (r == 0 || (r < 0 && errno != EINTR)) {
				close(pfd[i].fd);
				pfd[i].fd = -1;
				open_fds--;
			}
		}
	}
	if (pfd[0].fd >= 0) close(pfd[0].fd);
	if (pfd[1].fd >= 0) close(pfd[1].fd);

	int status = 0;
	while (waitpid(pid, &status, 0) < 0 && errno == EINTR)
		;
	return WIFEXITED(status) ? WEXITSTATUS(status) : 128;
}

static int valid_port_token(const char *t)
{
	if (!isalnum((unsigned char)t[0]))
		return 0;
	for (const char *c = t; *c; c++)
		if (!isalnum((unsigned char)*c) && !strchr("._+-", *c))
			return 0;
	return 1;
}

static int expand_packages(Manager *m, BStep *g, int idx)
{
	g->status = ST_RUNNING;

	int npkg = 0;
	char **pkgs = kbuild_packages(g->meta, &npkg);
	if (!npkg) {
		kb_strv_free(pkgs);
		g->status = ST_DONE;
		return 1;
	}

	char env[600];
	env_source(m, g, env, sizeof(env));

	KbBuf line = {0};
	kb_buf_printf(&line, "%sexport PKGDB_DIR=/dev/null && kpkgdepends", env);
	for (int i = 0; i < npkg; i++)
		kb_buf_printf(&line, " %s", pkgs[i]);
	kb_strv_free(pkgs);

	KbArgv a = {0};
	cmd_prefix(m, g->step_type, &a);
	kb_argv_add(&a, line.p);
	kb_argv_end(&a);

	/* `line` stays alive across the run: the argv holds its pointer. */
	KbBuf out = {0}, err = {0};
	int rc = run_capture2(&a, &out, &err);
	kb_buf_free(&line);

	char detail[4096];
	if (rc != 0) {
		snprintf(detail, sizeof(detail), "kpkgdepends exited %d\n%s", rc,
			 err.p ? err.p : (out.p ? out.p : ""));
		fail_expansion(m, g, "package resolution", detail);
		goto fail;
	}

	BStep *nodes[KB_MAX_STEPS / 8];
	int n = 0;
	char *save = out.p ? out.p : (char *)"";
	for (char *tok = strtok(save, " \t\r\n"); tok;
	     tok = strtok(NULL, " \t\r\n")) {
		if (!valid_port_token(tok)) {
			snprintf(detail, sizeof(detail),
				 "kpkgdepends returned a token that is not a "
				 "package name: %s\nfull stdout:\n%.1500s%s%.500s",
				 tok, out.p ? out.p : "",
				 err.n ? "\nstderr:\n" : "", err.p ? err.p : "");
			fail_expansion(m, g, "package resolution", detail);
			goto fail;
		}
		if (n == (int)(sizeof(nodes) / sizeof(nodes[0])))
			break;

		char nodepath[600];
		snprintf(nodepath, sizeof(nodepath), "%s/%02d_%s.install",
			 g->meta->dir_path, n, tok);
		BStep *node = step_new(nodepath, 0);
		kb_strlcpy(node->title, tok, sizeof(node->title));
		node->parent = g;
		node->step_type = SX_CUSTOM;

		/* -f only for ports the plan asked to rebuild: kpkg's -f really
		 * does force, so passing it blanketly rebuilds the whole tree. */
		int forced = m->have_plan && kbuild_plan_forced(&m->plan, tok);
		if (forced) {
			if (m->nforced_seen < KBUILD_MAX_REBUILD)
				kb_strlcpy(m->forced_seen[m->nforced_seen++],
					   tok, 64);
			kb_strlcpy(node->note, "rebuild", sizeof(node->note));
		}

		/* Overwrite for every package a phase installs. The userland
		 * genuinely overlaps — toybox, util-linux, procps-ng and gawk
		 * all ship /usr/bin/awk-shaped paths — and the build's rule has
		 * always been that whoever comes last in the dependency order
		 * wins. It is NOT -f: nothing is rebuilt by it, and the path
		 * changes hands in the database instead of being claimed twice.
		 *
		 * It goes in the ENVIRONMENT rather than on the command line
		 * because the kpkg in the tree is not necessarily the kpkg this
		 * orchestrator was built beside: restoring a phase-1 or phase-2
		 * snapshot puts an OLDER kpkg back, and an older one parsed
		 * `--overwrite` as a package name and died with `Port not
		 * found: --overwrite` on the first package of the phase. An
		 * unknown env var is ignored by every version. */
		KbBuf cl = {0};
		kb_buf_printf(&cl, "%sexport KPKG_OVERWRITE=1 && kpkg install%s %s",
			      env, forced ? " -f" : "", tok);
		node->cmd_line = cl.p;		/* the node owns it now */
		cmd_prefix(m, g->step_type, &node->cmd);
		kb_argv_add(&node->cmd, node->cmd_line);
		kb_argv_end(&node->cmd);
		node->have_cmd = 1;

		nodes[n++] = node;
	}

	if (!n) {
		snprintf(detail, sizeof(detail), "kpkgdepends returned nothing%s%.500s",
			 err.n ? "\nstderr:\n" : "", err.p ? err.p : "");
		fail_expansion(m, g, "package resolution", detail);
		goto fail;
	}

	for (int i = 0; i < n && g->nchild < (int)(sizeof(g->child) /
						   sizeof(g->child[0])); i++)
		g->child[g->nchild++] = nodes[i];
	order_insert(m, idx + 1, nodes, n);
	renumber(m);

	kb_buf_free(&out);
	kb_buf_free(&err);
	g->status = ST_DONE;
	return 1;
fail:
	kb_buf_free(&out);
	kb_buf_free(&err);
	return 0;
}

static int expand_scripts(Manager *m, BStep *g, int idx)
{
	g->status = ST_RUNNING;

	int nsh = 0;
	char **sh = kbuild_steps(g->meta, &nsh);

	BStep *nodes[KB_MAX_STEPS / 8];
	int n = 0;
	for (int i = 0; i < nsh; i++) {
		if (m->have_plan &&
		    !kbuild_plan_step_selected(&m->plan, g->meta->dir_name, sh[i]))
			continue;
		char full[900];
		snprintf(full, sizeof(full), "%s/%s", g->script_dir, sh[i]);
		BStep *node = step_new(full, 0);
		step_derive_title(node);
		node->parent = g;
		node->step_type = g->step_type;
		if (n < (int)(sizeof(nodes) / sizeof(nodes[0])))
			nodes[n++] = node;
	}
	kb_strv_free(sh);

	for (int i = 0; i < n && g->nchild < (int)(sizeof(g->child) /
						   sizeof(g->child[0])); i++)
		g->child[g->nchild++] = nodes[i];
	if (n) {
		order_insert(m, idx + 1, nodes, n);
		renumber(m);
	}
	g->status = ST_DONE;
	return 1;
}

/* ──────────────────────────────────────────────────────────────────────── */
/* Snapshot bookkeeping around a finished phase                             */

static void set_snap_state(Manager *m, BStep *g, const char *state,
			   const char *detail)
{
	(void)m;
	kb_strlcpy(g->snap_state, state, sizeof(g->snap_state));
	kb_strlcpy(g->note, detail && *detail ? detail : state, sizeof(g->note));
}

static void do_snapshot(Manager *m, BStep *g, int forced)
{
	if (!(m->snapshot_enabled || forced))
		return;
	const KbuildPhase *meta = g->meta;
	if (!meta)
		return;

	if (meta->nrejected) {
		KbBuf b = {0};
		for (int i = 0; i < meta->nrejected; i++)
			kb_buf_printf(&b, "%s%s", i ? " " : "", meta->rejected[i]);
		mgr_notice(m, "%s: ignoring unsafe snapshot path(s): %s",
			   meta->dir_name, b.p ? b.p : "");
		kb_buf_free(&b);
	}
	if (!kbuild_snapshottable(meta)) {
		set_snap_state(m, g, "skipped", "no snapshot paths");
		return;
	}

	char err[512] = "";
	int rc = snap_create(m, g, err, sizeof(err));
	m->snap.active = 0;

	if (rc < 0) {
		int aborted = strstr(err, "abort") != NULL;
		mgr_notice(m, "snapshot %s %s: %s", meta->dir_name,
			   aborted ? "aborted" : "FAILED", err);
		set_snap_state(m, g, aborted ? "aborted" : "failed", err);
		return;
	}
	if (rc == 0) {
		mgr_notice(m, "snapshot %s skipped: declared paths do not exist",
			   meta->dir_name);
		set_snap_state(m, g, "skipped", "nothing to archive");
		return;
	}

	KbuildSnapshot sn;
	long long total = 0;
	if (kbuild_snap_load(m->snap_root, meta->dir_name, &sn) == 0)
		for (int i = 0; i < sn.nentries; i++)
			total += sn.entry[i].bytes_compressed;

	int done = 0;
	for (int i = 0; i < g->nchild; i++)
		if (g->child[i]->status == ST_DONE)
			done++;
	int complete = !g->nchild || done == g->nchild;

	if (complete) {
		set_snap_state(m, g, "ok", human_bytes(total));
		mgr_notice(m, "snapshot %s -> %s", meta->dir_name,
			   human_bytes(total));
	} else {
		char detail[64];
		snprintf(detail, sizeof(detail), "%s @ %d/%d",
			 human_bytes(total), done, g->nchild);
		set_snap_state(m, g, "partial", detail);
		mgr_notice(m, "PARTIAL snapshot %s at step %d/%d -> %s "
			   "(restoring it re-runs the phase)", meta->dir_name,
			   done, g->nchild, human_bytes(total));
	}
}

static void finish_phase(Manager *m, BStep *g)
{
	if (m->timings && g->meta) {
		tm_record_phase(m->timings, g->meta->dir_name, step_duration(g));
		tm_save(m->timings);
	}
	do_snapshot(m, g, 0);
}

/* Stop waiting on a child that will not die. Its group has had SIGKILL; if
 * the pipe is still open something outside the group inherited it, and the
 * build must not hang on that. */
static void abandon_child(Manager *m, BStep *s)
{
	if (m->child_fd >= 0) {
		close(m->child_fd);
		m->child_fd = -1;
	}
	int status = 0;
	waitpid(m->child, &status, WNOHANG);
	m->child = -1;
	m->child_killed = 0;
	m->child_len = 0;
	if (m->log_fd >= 0) {
		close(m->log_fd);
		m->log_fd = -1;
	}
	if (s) {
		s->end_time = kb_now_s();
		s->return_code = 143;		/* terminated */
		set_family_status(s, ST_FAIL);
		step_log(s, "step terminated on request");
		if (!m->error_step)
			m->error_step = s;
	}
	mgr_notice(m, "step killed; its process group did not exit");
}

/* ──────────────────────────────────────────────────────────────────────── */
/* The loop                                                                 */

void mgr_start(Manager *m)
{
	m->start_time = kb_now_s();
	m->is_running = 1;
	m->cursor = 0;

	char dir[600];
	snprintf(dir, sizeof(dir), "%s/logs", m->build_dir);
	kb_mkdir_p(dir);
}

int mgr_pump(Manager *m)
{
	if (!m->is_running)
		return 30;

	if (m->pending_finish) {
		BStep *g = m->pending_finish;
		m->pending_finish = NULL;
		finish_phase(m, g);
		return 0;
	}

	/* A child is running: drain it, and only advance when it exits. */
	if (m->child > 0) {
		BStep *s = m->current_step;
		/* The whole GROUP, because the step is `bash -c` and the work
		 * is its descendants. SIGTERM first, SIGKILL if the tree
		 * ignores it, then stop waiting entirely: a process that
		 * inherited the pipe and outlived its group would otherwise
		 * hold the build open forever. */
		if ((m->stop_requested || m->force_quit) && !m->child_killed) {
			kill(-m->child, m->force_quit ? SIGKILL : SIGTERM);
			m->child_killed = 1;
			m->child_killed_at = kb_now_s();
		} else if (m->child_killed) {
			double waited = kb_now_s() - m->child_killed_at;
			if (m->force_quit || waited > 5)
				kill(-m->child, SIGKILL);
			if (waited > 10) {
				abandon_child(m, s);
				m->is_running = 0;
				return 30;
			}
		}

		if (pump_child(m, s)) {
			/* Still going. pump_child drained everything readable,
			 * so the caller waits rather than spinning — returning
			 * 0 here pinned a core for the whole build. */
			return 20;
		}

		if (m->timings && s->return_code == 0) {
			char key[192];
			step_timing_key(s, key, sizeof(key));
			tm_record_step(m->timings, key, step_duration(s));
		}

		if (s->return_code == 0) {
			set_family_status(s, ST_DONE);
			BStep *p = s->parent;
			if (p && p->nchild) {
				int all = 1;
				for (int i = 0; i < p->nchild; i++)
					if (p->child[i]->status != ST_DONE)
						all = 0;
				if (all)
					m->pending_finish = p;
			}
		} else {
			set_family_status(s, ST_FAIL);
			m->error_step = s;
			m->stop_requested = 1;
			m->is_running = 0;
			return 30;
		}

		if (m->snapshot_request) {
			BStep *t = m->snapshot_request;
			m->snapshot_request = NULL;
			do_snapshot(m, t, 1);
		}
		m->cursor++;
		return 0;
	}

	if (m->stop_requested) {
		m->is_running = 0;
		return 30;
	}

	while (m->cursor < m->norder) {
		BStep *s = m->order[m->cursor];

		if (s->status == ST_SKIPPED) {
			m->cursor++;
			continue;
		}

		m->current_step = s;
		if (s->is_group)
			m->current_phase = s;

		if (s->is_group && s->packages_file[0] && !s->nchild) {
			if (!expand_packages(m, s, m->cursor))
				return 30;
			if (!s->nchild)
				finish_phase(m, s);
			m->cursor++;
			return 0;
		}
		if (s->is_group && s->script_dir[0] && !s->nchild) {
			if (!expand_scripts(m, s, m->cursor))
				return 30;
			if (!s->nchild)
				finish_phase(m, s);
			m->cursor++;
			return 0;
		}
		if (s->is_group || s->status == ST_DONE) {
			m->cursor++;
			continue;
		}

		start_step(m, s);
		return 0;
	}

	/* A rebuild the user asked for that never showed up is otherwise a
	 * silent no-op: the port is in no selected phase's closure. */
	if (m->have_plan && m->plan.nrebuild) {
		KbBuf missed = {0};
		for (int i = 0; i < m->plan.nrebuild; i++) {
			int seen = 0;
			for (int k = 0; k < m->nforced_seen; k++)
				seen |= !strcmp(m->forced_seen[k],
						m->plan.rebuild[i]);
			if (!seen)
				kb_buf_printf(&missed, "%s%s", missed.n ? " " : "",
					      m->plan.rebuild[i]);
		}
		if (missed.n)
			mgr_notice(m, "NOT rebuilt (not reached by the selected "
				   "phases): %s", missed.p);
		kb_buf_free(&missed);
	}

	m->is_running = 0;
	return 30;
}
