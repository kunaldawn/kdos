/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   `fd` and `rga` on a pipe, a whole line at a time
 *
 * NEITHER SEARCH IS RE-IMPLEMENTED HERE, and neither is written twice. A
 * walker of our own would disagree with `fd` about hidden files, ignore rules
 * and symlinks, and a second copy of these argument vectors would make two
 * surfaces disagree with each other about the same question.
 *
 * ONE CHILD AT A TIME. Starting a search cancels the one running: two children
 * on two pipes interleave their lines into one list, and nothing downstream can
 * tell which search a row came from.
 *
 * THE PIPE IS NONBLOCKING AND READ FROM THE CALLER'S POLL LOOP. A search over a
 * home directory is seconds, and a surface that waited for it would be a window
 * that cannot be closed while it is doing the one thing it is for.
 * ---------------------------------
 */

#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stddef.h>
#include <sys/wait.h>
#include <unistd.h>

#include "kbase.h"

#include "filesearch.h"

/* The child's own limit, not only the caller's: it stops when it has found
 * this many rather than being killed with a full pipe. */
#define FS_MAX_HITS "200"

/* The running child, if any, and where its lines go. */
static pid_t kid = -1;
static int kid_fd = -1;
static int kid_tag;
static sh_fsearch_line kid_cb;
static void *kid_user;
static char partial[1024];
static size_t npartial;

void sh_fsearch_stop(void)
{
	if (kid > 0) {
		kill(kid, SIGTERM);
		waitpid(kid, NULL, 0);
	}
	if (kid_fd >= 0)
		close(kid_fd);
	kid = -1;
	kid_fd = -1;
	npartial = 0;
}

int sh_fsearch_fd(void)
{
	return kid_fd;
}

/*
 * Start `argv` with its output on a nonblocking pipe. Its own session, so the
 * signal that stops it reaches nothing else.
 */
static int kid_start(const char *const argv[], int tag, sh_fsearch_line cb,
		     void *user)
{
	int fds[2];
	pid_t pid;

	sh_fsearch_stop();
	if (!kb_have_prog(argv[0]))
		return -1;
	if (pipe(fds) != 0)
		return -1;
	pid = fork();
	if (pid < 0) {
		close(fds[0]);
		close(fds[1]);
		return -1;
	}
	if (pid == 0) {
		close(fds[0]);
		dup2(fds[1], STDOUT_FILENO);
		/* Its errors are not results: a permission denied on one
		 * directory would otherwise arrive as a row somebody can
		 * select. */
		int null = open("/dev/null", O_WRONLY);

		if (null >= 0) {
			dup2(null, STDERR_FILENO);
			close(null);
		}
		if (fds[1] > STDERR_FILENO)
			close(fds[1]);
		setsid();
		execvp(argv[0], (char *const *)argv);
		_exit(127);
	}
	close(fds[1]);
	fcntl(fds[0], F_SETFL, O_NONBLOCK);
	kid_fd = fds[0];
	kid = pid;
	kid_tag = tag;
	kid_cb = cb;
	kid_user = user;
	npartial = 0;
	return 0;
}

static void line_take(const char *line)
{
	if (kid_cb)
		kid_cb(line, kid_tag, kid_user);
}

int sh_fsearch_poll(void)
{
	char buf[4096];
	ssize_t n;

	if (kid_fd < 0)
		return 0;
	while ((n = read(kid_fd, buf, sizeof(buf))) > 0) {
		for (ssize_t i = 0; i < n; i++) {
			if (buf[i] == '\n' || npartial + 1 >= sizeof(partial)) {
				partial[npartial] = '\0';
				line_take(partial);
				npartial = 0;
				if (buf[i] != '\n')
					partial[npartial++] = buf[i];
				continue;
			}
			if (buf[i] >= 0x20 || buf[i] < 0)
				partial[npartial++] = buf[i];
		}
	}
	if (n == 0 || (n < 0 && errno != EAGAIN && errno != EWOULDBLOCK)) {
		if (npartial) {
			partial[npartial] = '\0';
			line_take(partial);
			npartial = 0;
		}
		close(kid_fd);
		kid_fd = -1;
		if (kid > 0) {
			waitpid(kid, NULL, 0);
			kid = -1;
		}
		return 0;
	}
	return 1;
}

void sh_fsearch_names(const char *dir, const char *query, int tag,
		      sh_fsearch_line cb, void *user)
{
	const char *argv[16];
	int n = 0;

	argv[n++] = "fd";
	/* Hidden files are included and VCS ignores are honoured: somebody
	 * looking for `.bashrc` means it, and nobody means `.git/objects`. */
	argv[n++] = "--hidden";
	argv[n++] = "--exclude";
	argv[n++] = ".git";
	argv[n++] = "--color";
	argv[n++] = "never";
	argv[n++] = "--max-results";
	argv[n++] = FS_MAX_HITS;
	/* A literal string, not a regex: a person typing `report.c` means the
	 * dot, and a pattern that swallowed it would match `reportxc`. */
	argv[n++] = "--fixed-strings";
	argv[n++] = "--";
	argv[n++] = query;
	argv[n++] = dir;
	argv[n] = NULL;
	kid_start(argv, tag, cb, user);
}

void sh_fsearch_contents(const char *dir, const char *query, int tag,
			 sh_fsearch_line cb, void *user)
{
	const char *argv[20];
	int n = 0;

	argv[n++] = "rga";
	argv[n++] = "--line-number";
	argv[n++] = "--no-heading";
	argv[n++] = "--color";
	argv[n++] = "never";
	argv[n++] = "--fixed-strings";
	argv[n++] = "--max-count";
	argv[n++] = "3";
	argv[n++] = "--max-columns";
	argv[n++] = "200";
	argv[n++] = "--";
	argv[n++] = query;
	argv[n++] = dir;
	argv[n] = NULL;
	kid_start(argv, tag, cb, user);
}
