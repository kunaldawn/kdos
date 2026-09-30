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
#include <sched.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#include "kdosbuild.h"
#include "kpkg.h"

/*
 * A PACKAGE PHASE RUNS ONE PORT AT A TIME unless --port-jobs asks for more.
 * Then it runs BY LEVEL: a port's level is one more than the highest level of
 * the ports of the same phase its recipe depends on, a level's ports build
 * side by side with `kpkg install --build-only`, and once every one of them has
 * built, one commit step installs them all with `kpkg install --commit`, in
 * the serial order. Nothing installs while a build runs, so each port builds
 * against the earlier phases and the lower levels and nothing else: what it
 * sees is a function of the tree, never of which sibling finished first.
 * expand_levels() has the graph and its rules.
 */

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

/* `# Title: ...` in the first five lines, else the filename tidied. The key
 * matches case-insensitively, so a step writing `# title:` is still titled. */
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

/* A group's duration is the wall-clock span of its steps, first start to last
 * end: under --port-jobs its steps overlap, and their sum would count the
 * overlap twice. */
double step_duration(const BStep *s)
{
	if (s->is_group) {
		double first = 0, last = 0;
		for (int i = 0; i < s->nchild; i++) {
			const BStep *c = s->child[i];
			if (c->start_time <= 0)
				continue;
			double end = c->end_time > 0 ? c->end_time : kb_now_s();
			if (!first || c->start_time < first)
				first = c->start_time;
			if (end > last)
				last = end;
		}
		return first > 0 ? last - first : 0;
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
	free(s->commits);
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
	for (int w = 0; w < KB_MAX_WORKERS; w++) {
		m->worker[w].pid = -1;
		m->worker[w].fd = -1;
		m->worker[w].log_fd = -1;
	}
	m->snapshot_enabled = 1;
	m->port_jobs = 1;
	m->lvl_from = m->lvl_commit = -1;

	/* The CPUs this process may run on, in order: a level's slots are
	 * windows cut from this list, never from the host's whole set. */
	cpu_set_t set;
	CPU_ZERO(&set);
	if (sched_getaffinity(0, sizeof(set), &set) == 0)
		for (int c = 0; c < CPU_SETSIZE && m->ncpu < (int)(sizeof(m->cpu) /
								sizeof(m->cpu[0])); c++)
			if (CPU_ISSET(c, &set))
				m->cpu[m->ncpu++] = c;

	char *abs = realpath(script_dir, NULL);
	kb_strlcpy(m->script_dir, abs ? abs : script_dir, sizeof(m->script_dir));
	free(abs);
	kb_strlcpy(m->build_dir, build_dir, sizeof(m->build_dir));

	/* script/chroot/exec.sh bind-mounts the repo root at /kdos and cds
	 * there, so anything handed to a chroot command has to be
	 * repo-relative: a host path like
	 * /workspace/script/phases/20_selfhost/phase.env does not exist inside.
	 * The repo root is the script directory's parent, which is why the
	 * script directory is `script` and never `script/phases`. */
	kb_strlcpy(m->repo_root, m->script_dir, sizeof(m->repo_root));
	char *slash = strrchr(m->repo_root, '/');
	if (slash && slash != m->repo_root)
		*slash = 0;

	snprintf(m->snap_root, sizeof(m->snap_root), "%.480s/snapshots",
		 m->build_dir);
	snprintf(m->chroot_exec, sizeof(m->chroot_exec), "%.560s/chroot/exec.sh",
		 m->script_dir);
	snprintf(m->phases_dir, sizeof(m->phases_dir), "%.500s/"
		 KBUILD_PHASES_DIR, m->script_dir);
	m->nphase = kbuild_discover(m->script_dir, m->phase, KBUILD_MAX_PHASES);

	for (int i = 0; i < m->nphase; i++) {
		const KbuildPhase *p = &m->phase[i];
		char label[128];
		kbuild_label(p, label, sizeof(label));

		BStep *g = step_new(label, 1);
		kb_strlcpy(g->title, label, sizeof(g->title));
		g->meta = p;
		g->step_type = p->chroot ? SX_CHROOT : SX_HOST;

		/* A phase with both lists is refused before anything runs (its
		 * `error`), and is filed as a package phase so that nothing
		 * can mistake it for a script phase in the meantime. */
		if (kbuild_is_package_phase(p))
			kb_strlcpy(g->packages_dir, p->dir_path,
				   sizeof(g->packages_dir));
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
	free(m->host_conf);
	m->host_conf = NULL;
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
 * re-snapshotted: snapshotting 20_selfhost again from a tree that already
 * holds 30_foundation's packages would file a mislabelled archive. */
void mgr_mark_continued(Manager *m, int phase_index)
{
	for (int i = 0; i < m->nroot; i++)
		if (m->root[i]->meta->index < phase_index) {
			m->root[i]->status = ST_SKIPPED;
			m->continued_from = m->root[i]->meta;
		}
}

/* `resume_inside` is for a PARTIAL snapshot: the phase it was taken from
 * still has steps left, so it is left pending and re-runs. A port already
 * installed and current runs no step (mark_installed), so re-running is
 * cheap. */
void mgr_mark_restored(Manager *m, int phase_index, int resume_inside)
{
	int ceiling = resume_inside ? phase_index - 1 : phase_index;
	for (int i = 0; i < m->nroot; i++)
		if (m->root[i]->meta->index <= ceiling) {
			m->root[i]->status = ST_SKIPPED;
			m->restored_from = m->root[i]->meta;
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
	size_t rl = strlen(m->phases_dir);
	if (!strncmp(s->path, m->phases_dir, rl) && s->path[rl] == '/')
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

/* A step the driver cannot even start is a FAILED step, and the stamp has to
 * be terminal: the cursor only advances past a step that reached ST_DONE or
 * stopped the run, so leaving one at ST_RUNNING re-enters start_step_on() on
 * it for ever and leaks the log descriptor on every pass. With other steps of
 * a level still running, the level drains rather than stopping under them. */
static void fail_start(Manager *m, BWorker *wk, BStep *s, const char *why)
{
	step_log(s, why);
	if (wk->log_fd >= 0) {
		dprintf(wk->log_fd, "%s\n", why);
		close(wk->log_fd);
		wk->log_fd = -1;
	}
	s->return_code = 999;
	s->end_time = kb_now_s();
	set_family_status(s, ST_FAIL);
	if (!m->error_step)
		m->error_step = s;
	if (m->nrunning) {
		m->draining = 1;
	} else {
		m->stop_requested = 1;
		m->is_running = 0;
	}
	mgr_notice(m, "%s: %s", s->title, why);
}

/* Give a child's output pipe 1 MiB. Failure is ignored: the kernel may cap a
 * pipe below that (fs.pipe-max-size), and the default 64 KiB is slower, not
 * wrong. */
static void grow_pipe(int fd)
{
#ifdef F_SETPIPE_SZ
	(void)fcntl(fd, F_SETPIPE_SZ, 1 << 20);
#else
	(void)fd;
#endif
}

/* In the forked child of a level's port, before exec: slot `w` of N runs on
 * min(ncpu, 2k) of this process's CPUs, starting w*ncpu/N along the list, so
 * the ninja, cargo, rustc and go that size themselves from the CPUs they may
 * use size themselves to the window rather than to the whole machine. The
 * windows overlap, so a port whose neighbours are linking or idle still gets
 * twice its share. A refusal leaves the child on every CPU, which is slower
 * under contention, not wrong. */
static void pin_window(const Manager *m, int w)
{
	if (m->ncpu < 1 || m->port_jobs < 2)
		return;
	int width = 2 * m->port_k;
	if (width > m->ncpu)
		width = m->ncpu;
	int first = w * m->ncpu / m->port_jobs;
	cpu_set_t set;
	CPU_ZERO(&set);
	for (int i = 0; i < width; i++)
		CPU_SET(m->cpu[(first + i) % m->ncpu], &set);
	(void)sched_setaffinity(0, sizeof(set), &set);
}

static void start_step_on(Manager *m, int w, BStep *s)
{
	BWorker *wk = &m->worker[w];
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
	wk->log_fd = open(logp, O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, 0644);

	/* `rel` outlives the branch that fills it: KbArgv stores the pointer
	 * and the exec happens after the fork, not inside the branch. */
	char rel[512];
	KbArgv a = {0};
	if (s->have_cmd) {
		a = s->cmd;
	} else if (s->step_type == SX_CHROOT) {
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
		fail_start(m, wk, s, "INTERNAL ERROR: pipe failed");
		return;
	}
	grow_pipe(pipefd[1]);

	pid_t pid = fork();
	if (pid < 0) {
		close(pipefd[0]);
		close(pipefd[1]);
		fail_start(m, wk, s, "INTERNAL ERROR: fork failed");
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
		if (s->level && !s->is_commit)
			pin_window(m, w);
		setenv("KDOS_REPLAY", explicitly_selected(m, s) ? "1" : "0", 1);
		kb_child_reset_signals();
		execvp(a.v[0], (char *const *)a.v);
		_exit(127);
	}

	close(pipefd[1]);
	/* Non-blocking: the build IS the main loop, so a step that goes quiet
	 * for ten minutes must not stop the screen from redrawing. The loop
	 * drains once per turn, so the pipe's capacity is the most a step can
	 * write per turn before it blocks: the 1 MiB set above lets a noisy
	 * step run at tens of MB/s where the default 64 KiB held it to a few. */
	fcntl(pipefd[0], F_SETFL, O_NONBLOCK);
	wk->pid = pid;
	wk->fd = pipefd[0];
	wk->len = 0;
	wk->killed = 0;
	wk->step = s;
	m->nrunning++;
	m->current_step = s;
}

/* The slot is free again. The step that ran in it becomes current only if
 * nothing else is running; otherwise the most recently started running step
 * does, which is what a front end follows. */
static void release_worker(Manager *m, BWorker *wk)
{
	BStep *s = wk->step;
	wk->step = NULL;
	wk->pid = -1;
	wk->killed = 0;
	wk->len = 0;
	if (wk->log_fd >= 0) {
		close(wk->log_fd);
		wk->log_fd = -1;
	}
	m->nrunning--;
	if (m->current_step != s || !m->nrunning)
		return;
	BStep *latest = NULL;
	for (int w = 0; w < KB_MAX_WORKERS; w++) {
		BStep *r = m->worker[w].step;
		if (r && (!latest || r->start_time > latest->start_time))
			latest = r;
	}
	m->current_step = latest;
}

/*
 * What a level's steps say that the orchestrator acts on. A port's
 * `--build-only` that finds it installed and current builds nothing and
 * records nothing, so the commit must not name it. A commit prints `kpkg:
 * committed <port>` after each install, and kpkgadd prints `Taking <n>
 * path(s) from <owner>` inside the install of the port that takes them: a
 * path that changes hands from a port the serial order installs AFTER the
 * taker ends up with the other owner here than it would one at a time, and
 * that pair has to be pinned for the result not to depend on --port-jobs.
 */
static void watch_line(Manager *m, BStep *s, const char *line)
{
	if (!s->level)
		return;
	if (!s->is_commit) {
		char skip[200];
		snprintf(skip, sizeof(skip), "==> Skipping %s (already installed)",
			 s->title);
		if (!strcmp(line, skip))
			s->no_commit = 1;
		return;
	}
	if (!strncmp(line, "kpkg: committed ", 16)) {
		s->ncommitted++;
		return;
	}
	const char *from = strncmp(line, "==> Taking ", 11) ? NULL
				: strstr(line, " from ");
	if (!from || s->ncommitted >= s->ncommits)
		return;
	from += 6;
	const BStep *taker = s->commits[s->ncommitted];
	const BStep *g = s->parent;
	for (int i = 0; g && i < g->nchild; i++) {
		const BStep *c = g->child[i];
		if (c->is_commit || strcmp(c->title, from))
			continue;
		if (c->serial_pos > taker->serial_pos)
			mgr_notice(m, "%s took paths from %s; the serial order had "
				   "%s last — pin the pair in 00-order.txt",
				   taker->title, c->title, c->title);
		break;
	}
}

static void take_line(Manager *m, BWorker *wk, const char *line)
{
	step_log(wk->step, line);
	m->total_lines++;
	if (wk->log_fd >= 0)
		dprintf(wk->log_fd, "%s\n", line);
	watch_line(m, wk->step, line);
}

/* Drain whatever the child has written. Returns 1 while it is still running;
 * 0 once it has exited, with its step stamped and its slot released. */
static int pump_child(Manager *m, BWorker *wk)
{
	BStep *s = wk->step;
	for (;;) {
		ssize_t r = read(wk->fd, wk->buf + wk->len,
				 sizeof(wk->buf) - wk->len - 1);
		if (r < 0) {
			if (errno == EINTR)
				continue;
			break;		/* EAGAIN — nothing more right now  */
		}
		if (r == 0) {
			/* EOF: flush whatever has no newline on it. */
			if (wk->len) {
				wk->buf[wk->len] = 0;
				take_line(m, wk, wk->buf);
				wk->len = 0;
			}
			close(wk->fd);
			wk->fd = -1;

			int status = 0;
			while (waitpid(wk->pid, &status, 0) < 0 && errno == EINTR)
				;
			s->return_code = WIFEXITED(status) ? WEXITSTATUS(status)
							  : 128 + WTERMSIG(status);
			s->end_time = kb_now_s();
			release_worker(m, wk);
			return 0;
		}

		wk->len += (size_t)r;
		wk->buf[wk->len] = 0;

		char *start = wk->buf, *nl;
		while ((nl = strchr(start, '\n'))) {
			*nl = 0;
			char *e = nl;
			while (e > start && (e[-1] == '\r' || e[-1] == ' ' ||
					     e[-1] == '\t'))
				*--e = 0;
			take_line(m, wk, start);
			start = nl + 1;
		}
		size_t left = wk->len - (size_t)(start - wk->buf);
		memmove(wk->buf, start, left);
		wk->len = left;

		/* A line longer than the buffer: flush it rather than spin. */
		if (wk->len >= sizeof(wk->buf) - 1) {
			wk->buf[wk->len] = 0;
			take_line(m, wk, wk->buf);
			wk->len = 0;
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

/* The clamp below must stay UNREACHABLE: KB_MAX_STEPS is sized to hold every
 * phase plus KB_MAX_PKGS from each package list, and a build that hit it would
 * drop steps off the end of its own order with nothing said. */
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
	grow_pipe(op[1]);
	grow_pipe(ep[1]);

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
		kb_child_reset_signals();
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

/* Whether a phase.env points its `kpkg` at a database other than the
 * chroot's own. Read from the file's text, as every phase.env key is: a
 * phase that names any of these keeps every step, because the host cannot
 * follow the redirection to the database that phase really reads. */
static int phase_env_redirects_db(const KbuildPhase *p)
{
	static const char *KEYS[] = { "PKGDB_DIR", "KPKG_ROOT", "KPKG_CONF",
				      "--root", NULL };
	if (!p->env_file[0])
		return 0;
	size_t len = 0;
	char *data = kb_read_all(p->env_file, &len);
	int hit = 0;
	for (int i = 0; data && KEYS[i] && !hit; i++)
		hit = strstr(data, KEYS[i]) != NULL;
	free(data);
	return hit;
}

/*
 * A PORT THE CHROOT WOULD ONLY SKIP IS NOT A STEP THAT RUNS.
 *
 * The order comes from an empty database, so it holds every port in the
 * phase's closure, and on any tree past its first build most of them are
 * installed and current: each would enter the chroot to print `Skipping`.
 * The host asks kpkg's own question first — kp_installed_current(), over the
 * database the chroot reads (script/chroot/exec.sh roots it at
 * <repo>/build/fs) and the phase's repositories as the host sees them, with
 * the strict recipe check common.env turns on — and marks the answer ST_DONE,
 * noted "installed", never started. The rule stays in that one function; a
 * step that does run is checked again by kpkg itself.
 *
 * The answer given here is provisional. A package phase runs for hours, and
 * an edit to a later port's recipe, or to src/libs, which every source-less
 * src/ port hashes, must still reach that port in the same run — as it did
 * when kpkg asked at the moment the step started. So the configuration is
 * kept on the Manager, and the run loop asks again when it reaches each
 * marked row (recheck_installed); the phase finishes only once every one of
 * them is confirmed.
 *
 * A port the plan forces always runs. So does one the host cannot resolve to
 * a single directory: kp_installed_current() reads a port it cannot find as
 * current, which is the chroot's answer to give, not the host's.
 *
 * Only for a chroot phase whose phase.env leaves the database alone; any
 * other phase keeps every step. Returns the number marked.
 */
static int mark_installed(Manager *m, BStep *g, BStep **nodes, int n)
{
	if (g->step_type != SX_CHROOT || !g->meta ||
	    phase_env_redirects_db(g->meta))
		return 0;
	char repos[4096];
	if (kbuild_phase_repos(g->meta, m->repo_root, repos, sizeof(repos)) <= 0)
		return 0;

	/* On the heap: a KpConf carries every repository's shelf list. */
	free(m->host_conf);
	m->host_conf = NULL;
	KpConf *c = kb_calloc(1, sizeof(*c));
	kp_conf_set_repos(c, repos);
	snprintf(c->pkgdb_dir, sizeof(c->pkgdb_dir),
		 "%.480s/build/fs/var/lib/kpkg/db", m->repo_root);
	c->strict_recipe = 1;

	int marked = 0;
	for (int i = 0; i < n; i++) {
		BStep *node = nodes[i];
		if (m->have_plan && kbuild_plan_forced(&m->plan, node->title))
			continue;
		char *dir = NULL, err[256];
		if (kp_port_find(c, node->title, &dir, err, sizeof(err)) != 1)
			continue;
		free(dir);
		if (!kp_installed_current(c, node->title))
			continue;
		node->status = ST_DONE;
		node->installed = 1;
		node->start_time = node->end_time = 0;
		kb_strlcpy(node->note, "installed", sizeof(node->note));
		marked++;
	}
	if (marked)
		m->host_conf = c;
	else
		free(c);
	return marked;
}

/* The run loop has reached a row mark_installed() took out: ask the same
 * question again. Still current, and the row is confirmed; not, and it goes
 * back to pending so the loop starts its step. */
static void recheck_installed(Manager *m, BStep *s)
{
	if (!m->host_conf || kp_installed_current(m->host_conf, s->title)) {
		s->confirmed = 1;
		return;
	}
	s->installed = 0;
	s->status = ST_PENDING;
	s->note[0] = '\0';
	mgr_notice(m, "%s changed while the phase ran; building it", s->title);
}

/* The step runs `bash -c <line>` (through the chroot wrapper for a chroot
 * phase); the node takes ownership of the line, which its argv points into. */
static void node_set_cmd(const Manager *m, const BStep *g, BStep *node,
			 char *line)
{
	free(node->cmd_line);
	node->cmd_line = line;
	memset(&node->cmd, 0, sizeof(node->cmd));
	cmd_prefix(m, g->step_type, &node->cmd);
	kb_argv_add(&node->cmd, node->cmd_line);
	kb_argv_end(&node->cmd);
	node->have_cmd = 1;
}

/* Whether the kpkg the phase runs knows `install --build-only`. A restored
 * 10_bootstrap or 20_selfhost snapshot can carry a kpkg older than this
 * orchestrator, and that one would take the flag for a port name. */
static int kpkg_splits(const Manager *m, const BStep *g)
{
	KbArgv a = {0};
	cmd_prefix(m, g->step_type, &a);
	kb_argv_add(&a, "kpkg help");
	kb_argv_end(&a);
	KbBuf out = {0}, err = {0};
	run_capture2(&a, &out, &err);
	int yes = out.p && strstr(out.p, "--build-only") != NULL;
	kb_buf_free(&out);
	kb_buf_free(&err);
	return yes;
}

typedef struct {
	const char *name;
	int pos;
} NameAt;

static int name_at_cmp(const void *a, const void *b)
{
	return strcmp(((const NameAt *)a)->name, ((const NameAt *)b)->name);
}

/* A port's position in the phase's order, -1 when the phase has no such port. */
static int name_pos(const NameAt *idx, int n, const char *name)
{
	NameAt key = { name, 0 };
	const NameAt *hit = bsearch(&key, idx, (size_t)n, sizeof(*idx),
				    name_at_cmp);
	return hit ? hit->pos : -1;
}

/*
 * THE LEVEL GRAPH of a package phase run with --port-jobs above 1.
 *
 * A port's edges are the names on its recipe's `depends` line that are ports
 * of this phase, read on the host through the phase's repositories
 * (kbuild_phase_repos) by kpkg's own kp_depends(). Only an edge to a port
 * earlier in kpkgdepends's order counts: the order is already topological,
 * and one pointing forward is a cycle it broke. Two rules add edges:
 *
 *   - a port the host cannot resolve to one directory depends on every port
 *     before it, since nothing is known of what it needs;
 *   - the order run (kbuild_packages_order_run) is a serial prefix: every
 *     port up to the last of its names in the resolved order depends on the
 *     one before it, and every later port depends on that last one. What a
 *     comment pinned one at a time stays one at a time, and before the rest.
 *
 * A port's level is one more than the highest level among its edges. What
 * this does NOT see is a dependency a recipe fails to declare — a configure
 * that finds a library when it happens to be installed. One at a time, the
 * list order hides that; by level, the port builds without it or fails.
 *
 * Every port of the order takes part, installed or not: a port found installed
 * and current is asked again when its level opens, as the serial runner asks
 * when it reaches it, and the levels are the same on every tree.
 *
 * Fills `out` with the level's ports, in serial order, then that level's
 * commit step, level after level, and returns the count; 0 leaves the phase
 * to run serially, with a notice saying why.
 */
static int expand_levels(Manager *m, BStep *g, BStep **nodes, int n,
			 BStep **out)
{
	const char *dir_name = g->meta->dir_name;
	if (!kpkg_splits(m, g)) {
		mgr_notice(m, "%s: its kpkg has no --build-only; building one "
			   "port at a time", dir_name);
		return 0;
	}
	char repos[4096];
	if (kbuild_phase_repos(g->meta, m->repo_root, repos, sizeof(repos)) <= 0) {
		mgr_notice(m, "%s: its repositories do not fit; building one "
			   "port at a time", dir_name);
		return 0;
	}
	/* On the heap: a KpConf carries every repository's shelf list. */
	KpConf *c = kb_calloc(1, sizeof(*c));
	kp_conf_set_repos(c, repos);

	NameAt *idx = kb_calloc((size_t)n, sizeof(*idx));
	for (int i = 0; i < n; i++)
		idx[i] = (NameAt){ nodes[i]->title, i };
	qsort(idx, (size_t)n, sizeof(*idx), name_at_cmp);

	char **run = NULL;
	int nrun = kbuild_packages_order_run(g->meta, &run);
	int run_end = -1;
	for (int r = 0; r < nrun; r++) {
		int at = name_pos(idx, n, run[r]);
		if (at > run_end)
			run_end = at;
	}
	kb_strv_free(run);

	int *lvl = kb_calloc((size_t)n, sizeof(*lvl));
	int top = 0;
	for (int i = 0; i < n; i++) {
		int base = 0;
		if (i <= run_end)
			base = i ? lvl[i - 1] : 0;
		else if (run_end >= 0)
			base = lvl[run_end];
		char *dir = kp_port_dir(c, nodes[i]->title);
		if (!dir) {
			base = top;
		} else {
			char deps[KP_MAX_DEPS][128];
			int nd = kp_depends(dir, deps, KP_MAX_DEPS);
			for (int d = 0; d < nd; d++) {
				int at = name_pos(idx, n, deps[d]);
				if (at >= 0 && at < i && lvl[at] > base)
					base = lvl[at];
			}
			free(dir);
		}
		lvl[i] = base + 1;
		if (lvl[i] > top)
			top = lvl[i];
	}
	free(idx);
	free(c);

	if (n + top > KB_MAX_PKGS) {
		mgr_notice(m, "%s: %d ports in %d levels exceed %d rows; "
			   "building one port at a time", dir_name, n, top,
			   KB_MAX_PKGS);
		free(lvl);
		return 0;
	}

	/* The job count the ports of a level divide: KDOS_JOBS as the build
	 * was given it, else every CPU this process may use. */
	const char *jv = getenv("KDOS_JOBS");
	int jobs = jv && *jv ? atoi(jv) : 0;
	if (jobs < 1)
		jobs = m->ncpu > 0 ? m->ncpu : 1;
	m->port_k = jobs / m->port_jobs > 0 ? jobs / m->port_jobs : 1;

	char env[600];
	env_source(m, g, env, sizeof(env));
	int k = 0;
	for (int L = 1; L <= top; L++) {
		int count = 0;
		for (int i = 0; i < n; i++) {
			if (lvl[i] != L)
				continue;
			BStep *node = nodes[i];
			int forced = m->have_plan &&
				     kbuild_plan_forced(&m->plan, node->title);
			KbBuf cl = {0};
			kb_buf_printf(&cl, "export KDOS_JOBS=%d && %sexport "
				      "KPKG_OVERWRITE=1 && kpkg install "
				      "--build-only%s %s", m->port_k, env,
				      forced ? " -f" : "", node->title);
			node_set_cmd(m, g, node, cl.p);
			node->level = L;
			out[k++] = node;
			count++;
		}
		char path[600];
		snprintf(path, sizeof(path), "%s/L%d.commit", g->meta->dir_path,
			 L);
		BStep *cs = step_new(path, 0);
		snprintf(cs->title, sizeof(cs->title), "install L%d (%d)", L,
			 count);
		cs->parent = g;
		cs->step_type = SX_CUSTOM;
		cs->is_commit = 1;
		cs->level = L;
		cs->serial_pos = n;
		out[k++] = cs;
	}
	free(lvl);
	mgr_notice(m, "%s: %d ports in %d levels, up to %d at a time with "
		   "KDOS_JOBS=%d each", dir_name, n, top, m->port_jobs,
		   m->port_k);
	return k;
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
	BStep **nodes = NULL;
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

	nodes = kb_calloc(KB_MAX_PKGS, sizeof(*nodes));
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
		/* A CAP HERE IS A PHASE THAT SILENTLY BUILDS PART OF ITSELF.
		 * A `break` on a package list whose resolved order outgrows
		 * the array loses every package past it with nothing said —
		 * the phase reports COMPLETE having never reached the tail of
		 * its own list. */
		if (n == KB_MAX_PKGS) {
			snprintf(detail, sizeof(detail),
				 "kpkgdepends resolved more than %d packages; "
				 "raise KB_MAX_PKGS (and libkpkg's KP_MAX_ORDER "
				 "with it) rather than building part of the phase",
				 KB_MAX_PKGS);
			fail_expansion(m, g, "package resolution", detail);
			goto fail;
		}

		char nodepath[600];
		snprintf(nodepath, sizeof(nodepath), "%s/%02d_%s.install",
			 g->meta->dir_path, n, tok);
		BStep *node = step_new(nodepath, 0);
		kb_strlcpy(node->title, tok, sizeof(node->title));
		node->parent = g;
		node->step_type = SX_CUSTOM;
		node->serial_pos = n;

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
		 * genuinely overlaps — toybox ships sed, find, xargs, awk, expr
		 * and ln, and GNU sed, findutils, gawk and coreutils ship the
		 * same paths over them — and the rule is that whoever comes
		 * last in the dependency order wins. It is NOT -f: nothing is rebuilt by it, and the path
		 * changes hands in the database instead of being claimed twice.
		 *
		 * It goes in the ENVIRONMENT rather than on the command line
		 * because the kpkg in the tree is not necessarily the kpkg this
		 * orchestrator was built beside: a restored 10_bootstrap or
		 * 20_selfhost snapshot carries an older kpkg, which takes
		 * `--overwrite` for a package name and dies with `Port not
		 * found: --overwrite` on the first package of the phase. An
		 * unknown env var is ignored by every version. */
		KbBuf cl = {0};
		kb_buf_printf(&cl, "%sexport KPKG_OVERWRITE=1 && kpkg install%s %s",
			      env, forced ? " -f" : "", tok);
		node_set_cmd(m, g, node, cl.p);

		nodes[n++] = node;
	}

	if (!n) {
		snprintf(detail, sizeof(detail), "kpkgdepends returned nothing%s%.500s",
			 err.n ? "\nstderr:\n" : "", err.p ? err.p : "");
		fail_expansion(m, g, "package resolution", detail);
		goto fail;
	}

	int installed = mark_installed(m, g, nodes, n);
	if (installed)
		mgr_notice(m, "%s: %d of %d ports installed and current",
			   g->meta->dir_name, installed, n);

	if (m->port_jobs > 1 && n > 1) {
		BStep **lv = kb_calloc(KB_MAX_PKGS, sizeof(*lv));
		int nl = expand_levels(m, g, nodes, n, lv);
		if (nl > 0) {
			free(nodes);
			nodes = lv;
			n = nl;
		} else {
			free(lv);
		}
	}

	for (int i = 0; i < n && g->nchild < (int)(sizeof(g->child) /
						   sizeof(g->child[0])); i++)
		g->child[g->nchild++] = nodes[i];
	order_insert(m, idx + 1, nodes, n);
	renumber(m);

	free(nodes);
	kb_buf_free(&out);
	kb_buf_free(&err);
	g->status = ST_DONE;
	return 1;
fail:
	free(nodes);
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

/* A row found installed counts only once the run loop has confirmed it: until
 * then its step can still come back. */
static int child_finished(const BStep *s)
{
	return s->status == ST_DONE && (!s->installed || s->confirmed);
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

	/* The snapshot's OWN size: a layer's archives hold only what changed
	 * since its base, and the kind says which base that is. */
	KbuildSnapshot *all = kb_calloc(KBUILD_MAX_SNAPS, sizeof(*all));
	int nall = kbuild_snap_list_all(m->snap_root, all, KBUILD_MAX_SNAPS);
	const KbuildSnapshot *sn = kbuild_snap_find(all, nall, meta->dir_name);
	long long total = 0;
	char kind[256] = "";
	if (sn) {
		for (int i = 0; i < sn->nentries; i++)
			total += sn->entry[i].bytes_compressed;
		snap_kind(all, nall, sn, kind, sizeof(kind));
	}
	free(all);

	int done = 0;
	for (int i = 0; i < g->nchild; i++)
		if (child_finished(g->child[i]))
			done++;
	int complete = !g->nchild || done == g->nchild;

	if (complete) {
		set_snap_state(m, g, "ok", human_bytes(total));
		mgr_notice(m, "snapshot %s -> %s (%s)", meta->dir_name,
			   human_bytes(total), kind);
	} else {
		char detail[64];
		snprintf(detail, sizeof(detail), "%s @ %d/%d",
			 human_bytes(total), done, g->nchild);
		set_snap_state(m, g, "partial", detail);
		mgr_notice(m, "PARTIAL snapshot %s at step %d/%d -> %s (%s; "
			   "restoring it re-runs the phase)", meta->dir_name,
			   done, g->nchild, human_bytes(total), kind);
	}
}

/* Every child finished, whether it ran or was confirmed installed. */
static int children_done(const BStep *g)
{
	for (int i = 0; i < g->nchild; i++)
		if (!child_finished(g->child[i]))
			return 0;
	return 1;
}

static void finish_phase(Manager *m, BStep *g)
{
	free(m->host_conf);
	m->host_conf = NULL;
	if (m->timings && g->meta) {
		tm_record_phase(m->timings, g->meta->dir_name, step_duration(g));
		tm_save(m->timings);
	}
	do_snapshot(m, g, 0);
}

/* Stop waiting on a child that will not die. Its group has had SIGKILL; if
 * the pipe is still open something outside the group inherited it, and the
 * build must not hang on that. */
static void abandon_child(Manager *m, BWorker *wk)
{
	BStep *s = wk->step;
	if (wk->fd >= 0) {
		close(wk->fd);
		wk->fd = -1;
	}
	int status = 0;
	waitpid(wk->pid, &status, WNOHANG);
	s->end_time = kb_now_s();
	s->return_code = 143;		/* terminated */
	set_family_status(s, ST_FAIL);
	step_log(s, "step terminated on request");
	if (!m->error_step)
		m->error_step = s;
	release_worker(m, wk);
	mgr_notice(m, "step killed; its process group did not exit");
}

/* A step's child has exited: record it. A failure in a level drains the
 * level — its running siblings finish, nothing new starts, and the level is
 * not committed, so what they built stays in the package cache as `.pending`
 * records that a resumed build reuses. Any other failure stops the build. */
static void step_exited(Manager *m, BStep *s)
{
	int in_level = s->level && !s->is_commit;
	if (m->timings && s->return_code == 0) {
		char key[192];
		step_timing_key(s, key, sizeof(key));
		tm_record_step(m->timings, key, step_duration(s));
	}
	if (s->return_code != 0) {
		set_family_status(s, ST_FAIL);
		if (!m->error_step)
			m->error_step = s;
		if (in_level) {
			if (!m->draining && !m->stop_requested && m->nrunning)
				mgr_notice(m, "%s failed; level %d is not "
					   "committed, waiting for its %d "
					   "running port(s)", s->title,
					   s->level, m->nrunning);
			m->draining = 1;
		} else {
			m->stop_requested = 1;
		}
		return;
	}
	set_family_status(s, ST_DONE);
	if (in_level)
		return;
	BStep *p = s->parent;
	if (p && p->nchild && children_done(p))
		m->pending_finish = p;
	m->cursor++;
}

/* MemAvailable is at least a quarter of MemTotal. A level starts another port
 * only then: two linkers of a large C++ port can take the rest. Unreadable
 * reads as enough. */
static int memory_ok(void)
{
	size_t len = 0;
	char *data = kb_read_all("/proc/meminfo", &len);
	if (!data)
		return 1;
	long long total = 0, avail = -1;
	for (char *line = data, *next; line && *line; line = next) {
		char *nl = strchr(line, '\n');
		next = nl ? nl + 1 : NULL;
		if (nl)
			*nl = 0;
		if (!strncmp(line, "MemTotal:", 9))
			total = strtoll(line + 9, NULL, 10);
		else if (!strncmp(line, "MemAvailable:", 13))
			avail = strtoll(line + 13, NULL, 10);
	}
	free(data);
	return total <= 0 || avail < 0 || avail * 4 >= total;
}

/* The cursor has reached the first port of a level: find its commit step and
 * ask again about every port of it the host found installed. */
static void open_level(Manager *m)
{
	const BStep *first = m->order[m->cursor];
	int c = m->cursor;
	while (c < m->norder && !(m->order[c]->is_commit &&
				  m->order[c]->parent == first->parent &&
				  m->order[c]->level == first->level))
		c++;
	m->lvl_from = m->cursor;
	m->lvl_commit = c;
	for (int i = m->lvl_from; i < c; i++) {
		BStep *s = m->order[i];
		if (s->installed && !s->confirmed)
			recheck_installed(m, s);
	}
}

/*
 * Start what the level can: into every free slot up to --port-jobs, the
 * pending port with the longest recorded time first — the one that decides
 * when the level ends — and on a tie the earlier in the serial order. A slot
 * is filled beside a running port only while memory_ok() holds; with nothing
 * running one always starts, so a level cannot stall. Returns whether any
 * started.
 */
static int fill_level(Manager *m)
{
	int started = 0;
	while (m->nrunning < m->port_jobs && !m->draining) {
		BStep *best = NULL;
		double best_est = -1;
		for (int i = m->lvl_from; i < m->lvl_commit; i++) {
			BStep *s = m->order[i];
			if (s->status != ST_PENDING)
				continue;
			double est = 0;
			if (m->timings) {
				char key[192];
				step_timing_key(s, key, sizeof(key));
				est = tm_step_est(m->timings, key, 0);
			}
			if (!best || est > best_est) {
				best = s;
				best_est = est;
			}
		}
		if (!best || (m->nrunning && !memory_ok()))
			break;
		int w = 0;
		while (w < m->port_jobs && m->worker[w].step)
			w++;
		start_step_on(m, w, best);
		started = 1;
	}
	return started;
}

/* The commit step of a level about to run: the ports it installs are the
 * level's that built something, in serial order. 0 when there are none — every
 * port was confirmed installed, or --build-only found it current — and the
 * step is then finished without running. */
static int prepare_commit(Manager *m, BStep *cs)
{
	const BStep *g = cs->parent;
	free(cs->commits);
	cs->commits = kb_calloc((size_t)g->nchild + 1, sizeof(*cs->commits));
	cs->ncommits = cs->ncommitted = 0;
	KbBuf names = {0};
	for (int i = 0; i < g->nchild; i++) {
		BStep *c = g->child[i];
		if (c->is_commit || c->level != cs->level || c->installed ||
		    c->no_commit || c->status != ST_DONE)
			continue;
		cs->commits[cs->ncommits++] = c;
		kb_buf_printf(&names, " %s", c->title);
	}
	if (!cs->ncommits) {
		kb_buf_free(&names);
		cs->status = ST_DONE;
		kb_strlcpy(cs->note, "nothing to commit", sizeof(cs->note));
		set_family_status(cs, ST_DONE);
		return 0;
	}
	char env[600];
	env_source(m, g, env, sizeof(env));
	KbBuf cl = {0};
	kb_buf_printf(&cl, "%sexport KPKG_OVERWRITE=1 && kpkg install --commit%s",
		      env, names.p);
	kb_buf_free(&names);
	node_set_cmd(m, g, cs, cl.p);
	return 1;
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

	/* Drain every running child, and record each one that exited. */
	int finished = 0;
	for (int w = 0; w < KB_MAX_WORKERS; w++) {
		BWorker *wk = &m->worker[w];
		if (!wk->step)
			continue;
		BStep *s = wk->step;
		/* The whole GROUP, because the step is `bash -c` and the work
		 * is its descendants. SIGTERM first, SIGKILL if the tree
		 * ignores it, then stop waiting entirely: a process that
		 * inherited the pipe and outlived its group would otherwise
		 * hold the build open forever. */
		if ((m->stop_requested || m->force_quit) && !wk->killed) {
			kill(-wk->pid, m->force_quit ? SIGKILL : SIGTERM);
			wk->killed = 1;
			wk->killed_at = kb_now_s();
		} else if (wk->killed) {
			double waited = kb_now_s() - wk->killed_at;
			if (m->force_quit || waited > 5)
				kill(-wk->pid, SIGKILL);
			if (waited > 10) {
				abandon_child(m, wk);
				finished = 1;
				continue;
			}
		}
		if (pump_child(m, wk))
			continue;
		step_exited(m, s);
		finished = 1;
	}

	if (m->stop_requested || m->draining) {
		/* Still going. pump_child drained everything readable, so the
		 * caller waits rather than spinning — returning 0 here pinned a
		 * core for the whole build. */
		if (m->nrunning)
			return finished ? 0 : 20;
		m->stop_requested = 1;
		m->is_running = 0;
		return 30;
	}

	/* A [S] snapshot waits for a moment when nothing is building. */
	if (finished && !m->nrunning && m->snapshot_request) {
		BStep *t = m->snapshot_request;
		m->snapshot_request = NULL;
		do_snapshot(m, t, 1);
	}

	if (m->lvl_commit >= 0) {
		int started = fill_level(m);
		if (m->nrunning || m->draining)
			return finished || started ? 0 : 20;
		/* Every port of the level built: its commit is next. */
		m->cursor = m->lvl_commit;
		m->lvl_from = m->lvl_commit = -1;
		return 0;
	}
	if (m->nrunning)
		return finished ? 0 : 20;
	if (finished)
		return 0;

	while (m->cursor < m->norder) {
		BStep *s = m->order[m->cursor];

		if (s->status == ST_SKIPPED) {
			m->cursor++;
			continue;
		}

		if (!s->is_group && s->level && !s->is_commit) {
			open_level(m);
			return 0;
		}
		if (!s->is_group && s->installed && !s->confirmed) {
			recheck_installed(m, s);
			if (s->confirmed) {
				m->cursor++;
				BStep *p = s->parent;
				if (p && p->nchild && children_done(p)) {
					m->pending_finish = p;
					return 0;
				}
				continue;
			}
		}
		/* Only a group or a step that starts becomes current: a front
		 * end announces current_step, and a row that never runs would
		 * be announced as running and never closed. */
		if (!s->is_group && s->status == ST_DONE) {
			m->cursor++;
			continue;
		}
		if (s->is_commit && !prepare_commit(m, s)) {
			m->cursor++;
			BStep *p = s->parent;
			if (p && p->nchild && children_done(p)) {
				m->pending_finish = p;
				return 0;
			}
			continue;
		}

		m->current_step = s;
		if (s->is_group)
			m->current_phase = s;

		if (s->is_group && s->packages_dir[0] && !s->nchild) {
			if (!expand_packages(m, s, m->cursor))
				return 30;
			/* A phase whose ports are all installed and current is
			 * finished by the walk that confirms the last of them. */
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
		if (s->is_group) {
			m->cursor++;
			continue;
		}

		start_step_on(m, 0, s);
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
