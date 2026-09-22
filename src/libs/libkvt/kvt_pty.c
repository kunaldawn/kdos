/*
 * SHL - PTY Helpers
 *
 * Copyright (c) 2011-2014 David Herrmann <dh.herrmann@gmail.com>
 * Dedicated to the Public Domain
 */

/*
 * PTY Helpers
 */

#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <pty.h>
#include <signal.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/ioctl.h>
#include <sys/uio.h>
#include <termios.h>
#include <unistd.h>
#include "kvt_macro.h"
#include "kvt_pty.h"
#include "kvt_ring.h"

#define KVT_SHL_PTY_BUFSIZE 16384

/*
 * PTY
 * A PTY object represents a single PTY connection between a master and a
 * child. The child process is fork()ed so the caller controls what program
 * will be run.
 *
 * Programs like /bin/login tend to perform a vhangup() on their TTY
 * before running the login procedure. This also causes the pty master
 * to get a EPOLLHUP event as long as no client has the TTY opened.
 * This means, we cannot use the TTY connection as reliable way to track
 * the client. Instead, we _must_ rely on the PID of the client to track
 * them.
 * However, this has the side effect that if the client forks and the
 * parent exits, we loose them and restart the client. But this seems to
 * be the expected behavior so we implement it here.
 *
 * While the vhangup() is ongoing the master reports HUP continuously, so a
 * caller that polls this fd level-triggered never sleeps. Poll it
 * edge-triggered, or gate the poll on kvt_shl_pty_is_open().
 *
 * kvt_shl_pty does not track SIGHUP: the caller does that itself and calls
 * kvt_shl_pty_close() once the client exited.
 */

struct kvt_shl_pty {
	unsigned long ref;
	int fd;
	pid_t child;
	char in_buf[KVT_SHL_PTY_BUFSIZE];
	struct kvt_shl_ring out_buf;

	kvt_shl_pty_input_fn fn_input;
	void *fn_input_data;
};

enum kvt_shl_pty_msg {
	KVT_SHL_PTY_FAILED,
	KVT_SHL_PTY_SETUP,
};

static char pty_recv(int fd)
{
	int r;
	char d;

	do {
		r = read(fd, &d, 1);
	} while (r < 0 && (errno == EINTR || errno == EAGAIN));

	return (r <= 0) ? KVT_SHL_PTY_FAILED : d;
}

static int pty_send(int fd, char d)
{
	int r;

	do {
		r = write(fd, &d, 1);
	} while (r < 0 && (errno == EINTR || errno == EAGAIN));

	return (r == 1) ? 0 : -EINVAL;
}

static int pty_setup_child(int slave,
			   unsigned short term_width,
			   unsigned short term_height)
{
	struct termios attr;
	struct winsize ws;

	/* get terminal attributes */
	if (tcgetattr(slave, &attr) < 0)
		return -errno;

	/* erase character should be normal backspace, PLEASEEE! */
	attr.c_cc[VERASE] = 010;
	/* always set UTF8 flag */
	attr.c_iflag |= IUTF8;

	/* set changed terminal attributes */
	if (tcsetattr(slave, TCSANOW, &attr) < 0)
		return -errno;

	memset(&ws, 0, sizeof(ws));
	ws.ws_col = term_width;
	ws.ws_row = term_height;

	if (ioctl(slave, TIOCSWINSZ, &ws) < 0)
		return -errno;

	if (dup2(slave, STDIN_FILENO) != STDIN_FILENO ||
	    dup2(slave, STDOUT_FILENO) != STDOUT_FILENO ||
	    dup2(slave, STDERR_FILENO) != STDERR_FILENO)
		return -errno;

	return 0;
}

static int pty_init_child(int fd)
{
	int r;
	sigset_t sigset;
	char *slave_name;
	int slave, i;
	pid_t pid;

	/* unlockpt() requires unset signal-handlers */
	sigemptyset(&sigset);
	r = sigprocmask(SIG_SETMASK, &sigset, NULL);
	if (r < 0)
		return -errno;

	for (i = 1; i < SIGSYS; ++i)
		signal(i, SIG_DFL);

	r = grantpt(fd);
	if (r < 0)
		return -errno;

	r = unlockpt(fd);
	if (r < 0)
		return -errno;

	slave_name = ptsname(fd);
	if (!slave_name)
		return -errno;

	/* open slave-TTY */
	slave = open(slave_name, O_RDWR | O_CLOEXEC | O_NOCTTY);
	if (slave < 0)
		return -errno;

	/* open session so we loose our controlling TTY */
	pid = setsid();
	if (pid < 0) {
		close(slave);
		return -errno;
	}

	/* set controlling TTY */
	r = ioctl(slave, TIOCSCTTY, 0);
	if (r < 0) {
		close(slave);
		return -errno;
	}

	return slave;
}

pid_t kvt_shl_pty_open(struct kvt_shl_pty **out,
		   kvt_shl_pty_input_fn fn_input,
		   void *fn_input_data,
		   unsigned short term_width,
		   unsigned short term_height)
{
	_shl_pty_unref_ struct kvt_shl_pty *pty = NULL;
	_shl_close_ int fd = -1;
	int slave, r, comm[2];
	pid_t pid;
	char d;

	if (!out)
		return -EINVAL;

	pty = calloc(1, sizeof(*pty));
	if (!pty)
		return -ENOMEM;

	pty->ref = 1;
	pty->fd = -1;
	pty->fn_input = fn_input;
	pty->fn_input_data = fn_input_data;

	fd = posix_openpt(O_RDWR | O_NOCTTY | O_CLOEXEC | O_NONBLOCK);
	if (fd < 0)
		return -errno;

	r = pipe2(comm, O_CLOEXEC);
	if (r < 0)
		return -errno;

	pid = fork();
	if (pid < 0) {
		/* error */
		pid = -errno;
		close(comm[0]);
		close(comm[1]);
		return pid;
	} else if (!pid) {
		/* child */
		slave = pty_init_child(fd);
		if (slave < 0)
			exit(1);

		close(comm[0]);
		close(fd);
		fd = -1;
		free(pty);
		pty = NULL;

		r = pty_setup_child(slave, term_width, term_height);
		if (r < 0)
			exit(1);

		/* close slave if it's not one of the std-fds */
		if (slave > 2)
			close(slave);

		/* wake parent */
		pty_send(comm[1], KVT_SHL_PTY_SETUP);
		close(comm[1]);

		*out = NULL;
		return pid;
	}

	/* parent */
	pty->fd = fd;
	pty->child = pid;

	close(comm[1]);
	fd = -1;

	/* wait for child setup */
	d = pty_recv(comm[0]);
	close(comm[0]);
	if (d != KVT_SHL_PTY_SETUP)
		return -EINVAL;

	*out = pty;
	pty = NULL;
	return pid;
}

void kvt_shl_pty_unref(struct kvt_shl_pty *pty)
{
	if (!pty || !pty->ref || --pty->ref)
		return;

	kvt_shl_pty_close(pty);
	kvt_shl_ring_clear(&pty->out_buf);
	free(pty);
}

void kvt_shl_pty_close(struct kvt_shl_pty *pty)
{
	if (!pty || pty->fd < 0)
		return;

	close(pty->fd);
	pty->fd = -1;
}

bool kvt_shl_pty_is_open(struct kvt_shl_pty *pty)
{
	return pty && pty->fd >= 0;
}

int kvt_shl_pty_get_fd(struct kvt_shl_pty *pty)
{
	if (!pty)
		return -EINVAL;

	return pty->fd >= 0 ? pty->fd : -EPIPE;
}

/*
 * HOW MUCH EITHER DIRECTION MOVES IN ONE DISPATCH. A transfer with no ceiling
 * hands the loop to whichever side of the pty is faster; past the budget it
 * stops with the descriptor still ready, so the caller's poll returns at once
 * and the next turn continues — one more frame, not one more byte.
 */
#define KVT_SHL_PTY_BUDGET (1u << 20)

/*
 * AND THE READ SIDE IS BOUNDED IN TIME, NOT IN BYTES, because every byte read
 * is parsed before the next one is: the ceiling that matters is how long the
 * caller is kept out of its own loop, and that is the parse rate times the
 * bytes — a number this library does not know. A megabyte of a truecolour
 * stream is some ten milliseconds inside one dispatch, and a caller that
 * checks a sixteen-millisecond frame deadline between turns then misses it,
 * so a program writing as fast as the terminal reads costs frames rather than
 * saving them. Two milliseconds keeps the coalescing the greedy drain exists
 * for — one render per dispatch instead of thirty — and still returns in time
 * for the deadline.
 */
#define KVT_SHL_PTY_SLICE_NS 2000000ull

static unsigned long long pty_mono_ns(void)
{
	struct timespec t;

	clock_gettime(CLOCK_MONOTONIC, &t);
	return (unsigned long long)t.tv_sec * 1000000000ull +
	       (unsigned long long)t.tv_nsec;
}

/*
 * DRAIN THE QUEUE, up to that budget.
 *
 * The descriptor is edge-triggered: a write that stopped while the queue still
 * held data and the pty still had room is woken by nothing, so the remainder
 * waits for whatever else wakes the caller's loop. A child that echoes
 * nothing — an editor in insert mode, a password prompt, `cat > file` —
 * produces no readable byte to be that wakeup, so a fixed number of writev
 * calls per dispatch turns a paste into a trickle paced by the loop's timeout.
 * The loop therefore stops only on an empty queue, on a full pty, on an error
 * or on the budget.
 */
static int pty_write(struct kvt_shl_pty *pty)
{
	struct iovec vec[2];
	size_t num;
	ssize_t r;
	size_t sent = 0;

	for (;;) {
		num = kvt_shl_ring_peek(&pty->out_buf, vec);
		if (!num)
			return 0;

		r = writev(pty->fd, vec, (int)num);
		if (r < 0) {
			if (errno == EAGAIN)
				return -EAGAIN;	/* the pty will take no more */
			if (errno == EINTR)
				return -EAGAIN;

			return -errno;
		} else if (!r) {
			return -EPIPE;
		}

		kvt_shl_ring_pull(&pty->out_buf, (size_t)r);
		sent += (size_t)r;
		if (sent >= KVT_SHL_PTY_BUDGET)
			return kvt_shl_ring_get_size(&pty->out_buf) > 0 ?
			       -EAGAIN : 0;
	}
}

/*
 * DRAIN THE DESCRIPTOR, up to a budget.
 *
 * A dispatch that stopped after two reads handed the caller 32 KiB of a
 * producer that had far more waiting, and every caller renders a whole frame
 * per dispatch — so a program writing a megabyte a second was rendered,
 * diffed, serialised and painted thirty times for one of its own frames, and
 * the desktop worked hardest at showing the frames nobody would see.
 *
 * The budget is what keeps that from becoming the opposite problem: a child
 * writing without pause must not hold this loop for ever while input goes
 * unread.
 */
static int pty_read(struct kvt_shl_pty *pty)
{
	unsigned long long t0 = pty_mono_ns();

	for (;;) {
		ssize_t len = read(pty->fd, pty->in_buf,
				   sizeof(pty->in_buf) - 1);

		if (len < 0) {
			if (errno == EAGAIN)
				return 0;	/* the child has written all it has */
			if (errno == EINTR)
				return -EAGAIN;

			return -errno;
		} else if (!len) {
			return -EPIPE;
		}

		if (pty->fn_input) {
			/* set terminating zero for debugging safety */
			pty->in_buf[len] = 0;
			pty->fn_input(pty, pty->fn_input_data, pty->in_buf,
				      (size_t)len);
		}
		if (pty_mono_ns() - t0 >= KVT_SHL_PTY_SLICE_NS)
			return -EAGAIN;
	}
}

int kvt_shl_pty_dispatch(struct kvt_shl_pty *pty)
{
	int r;

	if (!kvt_shl_pty_is_open(pty))
		return -ENODEV;

	r = pty_read(pty);
	pty_write(pty);
	return r;
}

/*
 * HOW MUCH THE CHILD HAS NOT TAKEN YET.
 *
 * A write that the pty would not accept stays in the ring, and nothing else
 * wakes the loop to retry it: the descriptor is polled for POLLIN, and a
 * child that is not writing produces no readable byte to be that wakeup. A
 * paste larger than the pty buffer then moves only when the child happens to
 * say something. The caller asks this and polls for POLLOUT while it is
 * non-zero.
 */
size_t kvt_shl_pty_pending(struct kvt_shl_pty *pty)
{
	if (!kvt_shl_pty_is_open(pty))
		return 0;
	return kvt_shl_ring_get_size(&pty->out_buf);
}

int kvt_shl_pty_write(struct kvt_shl_pty *pty, const char *u8, size_t len)
{
	if (!kvt_shl_pty_is_open(pty))
		return -ENODEV;

	return kvt_shl_ring_push(&pty->out_buf, u8, len);
}

int kvt_shl_pty_signal(struct kvt_shl_pty *pty, int sig)
{
	if (!kvt_shl_pty_is_open(pty))
		return -ENODEV;

	return ioctl(pty->fd, TIOCSIG, sig) < 0 ? -errno : 0;
}

int kvt_shl_pty_resize(struct kvt_shl_pty *pty,
		   unsigned short term_width,
		   unsigned short term_height)
{
	struct winsize ws;

	if (!kvt_shl_pty_is_open(pty))
		return -ENODEV;

	memset(&ws, 0, sizeof(ws));
	ws.ws_col = term_width;
	ws.ws_row = term_height;

	/*
	 * This will send SIGWINCH to the pty slave foreground process group.
	 * We will also get one, but we don't need it.
	 */
	return ioctl(pty->fd, TIOCSWINSZ, &ws) < 0 ? -errno : 0;
}
