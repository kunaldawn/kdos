/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   kdos-a11y — the console desktop, spoken
 *
 * A pixel desktop reconstructs a tree of accessible objects and hopes it
 * matches what was drawn. This one does not have to: the session holds the
 * literal text of every cell, knows which window is focused and — because
 * every widget says so — which control within it. So a reader is a client
 * that listens and speaks, and there is no second description of the screen
 * anywhere for the two to disagree about.
 *
 * IT CONNECTS TO THE READER'S SOCKET, which is the third one and grants the
 * least: its clients are views that may not drive. A reader that could type
 * would be a reader that could be made to.
 *
 * ESPEAK-NG IS A SUBPROCESS AND IS NEVER LINKED. It is GPL-3.0 and this
 * program is not; a pipe keeps that question out of the binary, and the tree
 * already starts a child this way. THE WRITE IS NON-BLOCKING AND DROPS ON
 * EAGAIN: a synthesiser that stopped reading would otherwise hold this process
 * open on a pipe nobody is draining, and a reader that stops reading the
 * session is a reader that says nothing at all.
 *
 * WHAT IT SAYS IT SAYS ONCE. An identical record two frames running is the
 * same control — a widget is right every frame and dropping the repeat is the
 * reader's job.
 * ---------------------------------
 */

#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/wait.h>

#include "kbase.h"
#include "kcon.h"
#include "ktui.h"

static const char *role_word(int role)
{
	switch (role) {
	case KT_A11Y_BUTTON:	return "button";
	case KT_A11Y_CHECK:	return "check box";
	case KT_A11Y_RADIO:	return "radio";
	case KT_A11Y_INPUT:	return "field";
	case KT_A11Y_LIST:	return "list";
	case KT_A11Y_TABLE:	return "table";
	case KT_A11Y_TAB:	return "tab";
	case KT_A11Y_CHOICE:	return "choice";
	case KT_A11Y_TEXT:	return "line";
	case KT_A11Y_WINDOW:	return "window";
	default:		return "";
	}
}

/* ── the voice ─────────────────────────────────────────────────────────── */

static pid_t voice_pid = -1;
static int voice_fd = -1;

/*
 * `espeak-ng --stdin` reads a line at a time and says it. Started once and
 * kept: a process per phrase would spend more time starting than speaking, and
 * the queue inside it is what makes two announcements in one frame sound like
 * a sentence rather than an interruption.
 */
static int voice_open(void)
{
	int fd[2];

	if (pipe(fd) != 0)
		return -1;
	voice_pid = fork();
	if (voice_pid < 0) {
		close(fd[0]);
		close(fd[1]);
		return -1;
	}
	if (!voice_pid) {
		dup2(fd[0], 0);
		close(fd[0]);
		close(fd[1]);
		execlp("espeak-ng", "espeak-ng", "--stdin", (char *)NULL);
		_exit(127);
	}
	close(fd[0]);
	voice_fd = fd[1];
	fcntl(voice_fd, F_SETFL, O_NONBLOCK);
	return 0;
}

static void say(const char *text)
{
	size_t n;
	ssize_t w;

	if (voice_fd < 0 || !text || !*text)
		return;
	n = strlen(text);
	w = write(voice_fd, text, n);
	if (w < 0 && errno != EAGAIN && errno != EWOULDBLOCK) {
		/* The synthesiser is gone. Saying nothing is the failure this
		 * program has: it must not become a program that also stops
		 * reading the session. */
		close(voice_fd);
		voice_fd = -1;
		return;
	}
	if (w > 0)
		w = write(voice_fd, "\n", 1);
	(void)w;
}

/* ── the session ───────────────────────────────────────────────────────── */

static int connect_a11y(const char *path)
{
	struct sockaddr_un a;
	int fd = socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0);

	if (fd < 0)
		return -1;
	memset(&a, 0, sizeof(a));
	a.sun_family = AF_UNIX;
	if (strlen(path) >= sizeof(a.sun_path)) {
		close(fd);
		return -1;
	}
	memcpy(a.sun_path, path, strlen(path));
	if (connect(fd, (struct sockaddr *)&a, sizeof(a)) != 0) {
		close(fd);
		return -1;
	}
	return fd;
}

static void usage(FILE *f)
{
	fprintf(f,
"kdos-a11y — say what the console desktop is showing\n"
"\n"
"  --socket PATH   the reader's socket; $KDOS_CON with .a11y otherwise\n"
"  --print         write each announcement to stdout instead of speaking,\n"
"                  which is how a script checks what a desktop would say\n"
"  --help\n");
}

int main(int argc, char **argv)
{
	const char *sock = NULL;
	char derived[256];
	int print = 0;

	for (int i = 1; i < argc; i++) {
		if (!strcmp(argv[i], "--socket") && i + 1 < argc)
			sock = argv[++i];
		else if (!strcmp(argv[i], "--print"))
			print = 1;
		else if (!strcmp(argv[i], "--help") || !strcmp(argv[i], "-h")) {
			usage(stdout);
			return 0;
		} else {
			fprintf(stderr, "kdos-a11y: unknown option '%s'\n",
				argv[i]);
			usage(stderr);
			return 2;
		}
	}

	/*
	 * $KDOS_CON is the SURFACE socket a program inside the session
	 * inherits; the reader's is beside it. Derived rather than a second
	 * variable, so a session that moved its run directory moves this with
	 * it.
	 */
	if (!sock) {
		const char *env = getenv("KDOS_CON");
		size_t len = env ? strlen(env) : 0;

		if (len > 5 && !strcmp(env + len - 5, ".sock") &&
		    snprintf(derived, sizeof(derived), "%.*s.a11y",
			     (int)(len - 5), env) < (int)sizeof(derived))
			sock = derived;
	}
	if (!sock) {
		fprintf(stderr, "kdos-a11y: no session. Set $KDOS_CON or pass "
				"--socket.\n");
		return 2;
	}

	int fd = connect_a11y(sock);

	if (fd < 0) {
		fprintf(stderr, "kdos-a11y: cannot reach %s\n", sock);
		return 1;
	}

	KconConn *conn = kcon_conn_new(fd);

	if (!conn) {
		close(fd);
		return 1;
	}

	signal(SIGPIPE, SIG_IGN);

	KconBuf b = { 0 };

	/*
	 * A VIEW THAT IMPOSES NOTHING. Zero columns and rows: a reader that
	 * asked for a size would resize the desktop of the person it is
	 * reading to, which is the opposite of what it is for.
	 */
	kcon_put_u16(&b, KCON_VERSION);
	kcon_put_u16(&b, KCON_KIND_VIEW);
	kcon_put_u16(&b, 0);
	kcon_put_u16(&b, 0);
	kcon_put_u16(&b, 0);
	kcon_put_u16(&b, KCON_RIGHTS_OBSERVE);
	kcon_send(conn, KCON_OP_HELLO, &b);
	kcon_buf_reset(&b);
	kcon_put_u16(&b, 0);
	kcon_put_u16(&b, 0);
	kcon_send(conn, KCON_OP_VIEW_SIZE, &b);
	kcon_buf_free(&b);
	kcon_flush(conn);

	if (!print && voice_open() != 0) {
		fprintf(stderr, "kdos-a11y: cannot start espeak-ng\n");
		kcon_conn_free(conn);
		return 1;
	}

	char last[256] = "";

	for (;;) {
		KconMsg m;
		int r = kcon_recv(conn, &m);

		if (r < 0)
			break;
		if (r == 0) {
			struct pollfd p = { kcon_conn_fd(conn), POLLIN, 0 };

			if (poll(&p, 1, 200) < 0 && errno != EINTR)
				break;
			continue;
		}
		if (m.op == KCON_OP_BYE)
			break;
		if (m.op != KCON_OP_ANNOUNCE)
			continue;

		KconRd rd;
		char line[256];

		kcon_rd_init(&rd, m.payload, m.len);

		int role = (int)kcon_get_u16(&rd);
		int index = (int)kcon_get_u16(&rd);
		int count = (int)kcon_get_u16(&rd);

		(void)kcon_get_u16(&rd);	/* the window's rectangle: */
		(void)kcon_get_u16(&rd);	/* carried for a reader    */
		(void)kcon_get_u16(&rd);	/* that routes braille by  */
		(void)kcon_get_u16(&rd);	/* position; speech has no */
						/* use for it              */

		const char *label = kcon_get_str(&rd);
		char labelbuf[128];

		kb_strlcpy(labelbuf, label ? label : "", sizeof(labelbuf));

		const char *value = kcon_get_str(&rd);

		if (rd.err)
			continue;

		/*
		 * ROLE, NAME, VALUE, THEN THE POSITION — the order somebody
		 * listening needs it in: what it is, which one, what it says,
		 * and where it sits. The position comes from the widget rather
		 * than from counting, so "3 of 9" is a fact.
		 */
		snprintf(line, sizeof(line), "%s%s%s%s%s",
			 role_word(role),
			 labelbuf[0] ? " " : "", labelbuf,
			 value && *value ? ", " : "", value ? value : "");
		if (index > 0 && count > 0) {
			size_t at = strlen(line);

			snprintf(line + at, sizeof(line) - at, ", %d of %d",
				 index, count);
		}

		/* SAID ONCE. A widget is right every frame; the repeat is
		 * this program's to drop. */
		if (!strcmp(line, last))
			continue;
		kb_strlcpy(last, line, sizeof(last));

		if (print)
			printf("%s\n", line);
		else
			say(line);
		fflush(stdout);
	}

	if (voice_fd >= 0)
		close(voice_fd);
	if (voice_pid > 0) {
		kill(voice_pid, SIGTERM);
		waitpid(voice_pid, NULL, 0);
	}
	kcon_conn_free(conn);
	return 0;
}
