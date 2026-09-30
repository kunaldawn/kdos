/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   A long child a surface watches — restic restore, xorriso, sha256sum -c
 *
 * THE SURFACE KEEPS DRAWING. A restore, a burn and a checksum run over a disc
 * are minutes each, and a surface that waited on one would be a frozen window
 * nobody can tell from a hung one. The child's output arrives on one
 * non-blocking pipe and is read a line at a time from the surface's own loop.
 *
 * STDOUT AND STDERR ARE ONE PIPE. Every program this runs reports its progress
 * on one and its result on the other, and the order between them is the
 * story; two pipes read in turn would reorder it.
 *
 * AN ARGUMENT VECTOR AND NEVER A SHELL: every path these surfaces pass came
 * from a person, a drop or a command line.
 * ---------------------------------
 */

#define _POSIX_C_SOURCE 200809L
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

#include "kbase.h"
#include "shell.h"

int sh_job_start(ShJob *j, const char *const argv[], const char *cwd)
{
	int p[2];
	pid_t pid;

	j->pid = -1;
	j->fd = -1;
	j->running = 0;
	j->status = -1;
	j->npart = 0;
	j->last[0] = '\0';
	if (!kb_have_prog(argv[0])) {
		snprintf(j->last, sizeof(j->last), "%s is not on this machine",
			 argv[0]);
		return -1;
	}
	if (pipe(p) != 0)
		return -1;
	pid = fork();
	if (pid < 0) {
		close(p[0]);
		close(p[1]);
		return -1;
	}
	if (pid == 0) {
		int nul = open("/dev/null", O_RDONLY);

		if (nul >= 0) {
			dup2(nul, STDIN_FILENO);
			if (nul > STDERR_FILENO)
				close(nul);
		}
		close(p[0]);
		dup2(p[1], STDOUT_FILENO);
		dup2(p[1], STDERR_FILENO);
		if (p[1] > STDERR_FILENO)
			close(p[1]);
		/* Its own process group, so a signal aimed at the surface's
		 * group — the terminal a surface was started from, a session
		 * ending — does not also land on a burn half-way through. */
		setpgid(0, 0);
		kb_child_reset_signals();
		/* A JOB OUTLIVES ITS WINDOW. Closing the surface closes the
		 * pipe's read end, and a burn or a restore killed by the
		 * SIGPIPE of its next progress line would leave half a disc
		 * or half a folder; ignored, the write fails and the work
		 * goes on. */
		signal(SIGPIPE, SIG_IGN);
		if (cwd && chdir(cwd) != 0)
			_exit(126);
		execvp(argv[0], (char *const *)argv);
		_exit(127);
	}
	close(p[1]);
	fcntl(p[0], F_SETFL, O_NONBLOCK);
	fcntl(p[0], F_SETFD, FD_CLOEXEC);
	j->pid = pid;
	j->fd = p[0];
	j->running = 1;
	return 0;
}

static void job_line(ShJob *j)
{
	j->part[j->npart] = '\0';
	j->npart = 0;
	if (!j->part[0])
		return;
	snprintf(j->last, sizeof(j->last), "%s", j->part);
	if (j->line)
		j->line(j->part, j->user);
}

int sh_job_pump(ShJob *j)
{
	char buf[4096];
	ssize_t r;
	int moved = 0, st;

	if (j->fd >= 0) {
		while ((r = read(j->fd, buf, sizeof(buf))) > 0) {
			/* A carriage return ends a line too: a progress
			 * meter redraws in place with one, and the line
			 * wanted is the newest. */
			for (ssize_t i = 0; i < r; i++) {
				if (buf[i] == '\n' || buf[i] == '\r')
					job_line(j);
				else if (j->npart + 1 < sizeof(j->part))
					j->part[j->npart++] = buf[i];
			}
			moved = 1;
		}
		if (r == 0 || (r < 0 && errno != EAGAIN && errno != EINTR)) {
			job_line(j);
			close(j->fd);
			j->fd = -1;
			moved = 1;
		}
	}
	if (j->running && j->fd < 0 && waitpid(j->pid, &st, 0) == j->pid) {
		j->running = 0;
		j->status = WIFEXITED(st) ? WEXITSTATUS(st) : -1;
		moved = 1;
	}
	return moved;
}

int sh_job_wait(ShJob *j)
{
	while (j->running) {
		if (j->fd >= 0) {
			int fl = fcntl(j->fd, F_GETFL);

			fcntl(j->fd, F_SETFL, fl & ~O_NONBLOCK);
		}
		sh_job_pump(j);
	}
	return j->status;
}
