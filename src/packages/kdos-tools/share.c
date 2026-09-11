/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   kdos-share — a file to another machine, over one code word
 *
 * `croc` IS THE TRANSFER AND THIS IS THE DESKTOP AROUND IT. No account, no
 * server of ours, no protocol written here: the code word is the whole of the
 * pairing and the two ends derive a key from it.
 *
 * IT IS `--local` ON THIS IMAGE. `/usr/bin/croc` is a wrapper that pins the
 * flag, so a transfer stays on this network and never reaches the public
 * relay. That is why the code word is offered and the `getcroc.com` link croc
 * also prints is not: that page is the public relay's, and on this image there
 * is no public relay to reach.
 *
 * THE TRANSFER STAYS IN FRONT OF THE PERSON. croc runs in the foreground of a
 * terminal window and prints its own progress; this program relays every byte
 * of it untouched rather than summarising, because what a transfer is doing is
 * croc's to say. All it adds is the QR and the toast, and it adds them from
 * INSIDE the same window — reading croc's output and running croc where
 * somebody can watch it are the same process or they are two.
 *
 * `--ignore-stdin` IS NOT OPTIONAL. croc reads stdin for a piped payload, so a
 * `croc send` whose stdin is anything but a closed file blocks before it prints
 * anything at all — no code, no error, on either stream. It is a GLOBAL flag
 * and the wrapper's own arguments come first, so it goes before `send`.
 *
 * THE CODE WORD ARRIVES ON STDERR. So does the progress. stdout carries
 * nothing, and the two are not separable — which is why the pipe is on stderr
 * and this program's own writing goes there too.
 * ---------------------------------
 */

#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/wait.h>

#include "kdos-tools.h"

#define SHARE_MAX  32		/* paths in one transfer            */
#define QR_MAX	   8192		/* a code word's QR, drawn as text  */
#define CLIP_MAX   65536

static int usage(void)
{
	fprintf(stderr,
		"usage: kdos-share [--clipboard] [FILE|file://URI]...\n"
		"With no file, kdos-pick asks for one.\n"
		"Sends over croc: one code word, any machine on this network,\n"
		"no account. Run it again on the other machine as `croc CODE`.\n");
	return 2;
}

/*
 * NOTHING NAMED, SO ASK. `share.file` is a menu row and a menu row has no
 * argument to give, so the chooser is what turns the row into a file — and
 * `kdos-share` typed with nothing after it means the same thing.
 *
 * IT PRINTS A URI. That is kdos-pick's whole interface, and kb_uri_path is
 * what turns it back into a path; exit 1 is a cancel and not a failure, so
 * nothing is said about it.
 */
static const char *pick_one(void)
{
	static char uri[4096];
	KbArgv a = { 0 };

	if (!kb_have_prog("kdos-pick"))
		kb_die("nothing to send, and kdos-pick is not installed");
	kb_argv_add(&a, "kdos-pick");
	kb_argv_add(&a, "--title");
	kb_argv_add(&a, "Share");
	kb_argv_end(&a);
	if (kb_run_capture(&a, uri, sizeof(uri)) != 0 || !uri[0])
		return NULL;
	return uri;
}

/*
 * THE CLIPBOARD, AS A FILE, because croc sends files and a clipboard is not
 * one. It goes in the runtime directory — 0700 and this person's own — rather
 * than beside their documents: it is a copy nobody asked to keep, and one left
 * in a home directory is a copy nobody knows they have.
 *
 * `clipboard.txt` is a NAME AND NOT A SCRATCH FILE: it is what arrives at the
 * other end, so a person receiving one knows what they got.
 */
static char *clip_file(void)
{
	static char path[512];
	char *dir = kb_path_join(kb_runtime_dir(), "kdos");
	char text[CLIP_MAX];

	if (!kdt_clip_take(text, sizeof(text))) {
		free(dir);
		kb_die("the clipboard is empty");
	}
	kb_mkdir_p(dir);
	snprintf(path, sizeof(path), "%s/clipboard.txt", dir);
	free(dir);
	if (kb_write_file(path, text) != 0)
		kb_die("%s: %s", path, strerror(errno));
	if (chmod(path, 0600) != 0)
		kb_die("%s: %s", path, strerror(errno));
	return path;
}

/*
 * THE CODE WORD OUT OF ONE LINE, or NULL.
 *
 * croc writes `  croc <code>` under `On the other computer, run:` — the line a
 * person is meant to type, which is the line worth reading. The `getcroc.com`
 * line beside it is the public relay's and this image has none.
 */
static const char *code_of(const char *line)
{
	static char code[64];
	const char *p = line;
	size_t n = 0;

	while (*p == ' ' || *p == '\t')
		p++;
	if (strncmp(p, "croc ", 5) != 0)
		return NULL;
	p += 5;
	while (*p == ' ')
		p++;
	while (p[n] && p[n] != ' ' && p[n] != '\r' && p[n] != '\n' &&
	       n < sizeof(code) - 1)
		n++;
	if (!n || p[n] == ' ')
		return NULL;	/* two words is a sentence, not a code */
	memcpy(code, p, n);
	code[n] = '\0';
	return code;
}

/*
 * THE CODE AS A QR, IN FULL BLOCKS.
 *
 * `ASCIIi` AND NOT `UTF8`. UTF8 draws with half blocks and the console font is
 * 512 glyphs and carries none of them, so that rendering is a blank rectangle
 * on `tty1` — which is not a worse QR, it is no QR. `ASCIIi` is one character
 * per module doubled across, and `#` there is a LIGHT module: replacing it with
 * `█` puts the light of the code on a dark screen, which is the printed
 * convention rendered in emitted light rather than an inverted code.
 *
 * THE CODE GOES IN ON STDIN. qrencode takes its payload as an argument and
 * this one is the transfer's whole secret; `/proc/<pid>/cmdline` is readable by
 * every process on the machine for as long as the child lives.
 *
 * DRAWN WHOLE OR NOT AT ALL. A QR wider or taller than the window is a picture
 * no camera can read, and half of one is worse than the code word alone.
 */
static void qr_draw(const char *code)
{
	char out[QR_MAX];
	struct winsize ws = { 0 };
	KbArgv a = { 0 };
	int rows = 0, cols = 0, w = 0;

	if (!kb_have_prog("qrencode"))
		return;

	kb_argv_add(&a, "qrencode");
	kb_argv_add(&a, "-t");
	kb_argv_add(&a, "ASCIIi");
	kb_argv_add(&a, "-m");
	kb_argv_add(&a, "1");
	kb_argv_add(&a, "-o");
	kb_argv_add(&a, "-");
	kb_argv_end(&a);
	if (kb_run_feed_capture(&a, code, strlen(code), out, sizeof(out)) != 0 ||
	    !out[0])
		return;

	for (const char *p = out; *p; p++) {
		if (*p != '\n') {
			w++;
			continue;
		}
		rows++;
		if (w > cols)
			cols = w;
		w = 0;
	}
	if (w) {
		rows++;
		if (w > cols)
			cols = w;
	}

	if (ioctl(STDERR_FILENO, TIOCGWINSZ, &ws) != 0 || !ws.ws_row)
		return;
	if (rows + 2 > ws.ws_row || cols > ws.ws_col) {
		fprintf(stderr,
			"  (the window is %dx%d and the code's QR needs "
			"%dx%d — type the words instead)\n",
			ws.ws_col, ws.ws_row, cols, rows + 2);
		return;
	}

	fputc('\n', stderr);
	for (const char *p = out; *p; p++)
		fputs(*p == '#' ? "█" : (char[]){ *p, 0 }, stderr);
	fputc('\n', stderr);
}

/*
 * RELAY EVERY BYTE, and read the code out of the stream on the way past.
 *
 * Not line buffered: croc draws its progress with carriage returns and no
 * newline, so a relay that waited for one would show nothing until the
 * transfer ended. Lines are accumulated ALONGSIDE the relay, purely to find
 * the code word, and a line longer than the buffer is simply not the one.
 */
static void relay(int fd, const char **code)
{
	char line[256];
	size_t n = 0;
	char buf[4096];
	ssize_t r;

	while ((r = read(fd, buf, sizeof(buf))) != 0) {
		if (r < 0) {
			if (errno == EINTR)
				continue;
			break;
		}
		fwrite(buf, 1, (size_t)r, stderr);
		fflush(stderr);
		for (ssize_t i = 0; i < r; i++) {
			if (buf[i] != '\n' && buf[i] != '\r') {
				if (n < sizeof(line) - 1)
					line[n++] = buf[i];
				continue;
			}
			line[n] = '\0';
			n = 0;
			if (*code)
				continue;

			const char *c = code_of(line);

			if (!c)
				continue;
			*code = kb_strdup(c);
			qr_draw(*code);
			{
				char body[128];

				/* THE CODE IS THE WHOLE OF THE PAIRING, and a
				 * toast that named a transfer without it would
				 * be a toast that says nothing usable. It is
				 * already on this person's own screen in the
				 * window below. */
				snprintf(body, sizeof(body),
					 "on the other machine: croc %s",
					 *code);
				kb_notify("kdos-share", "Sharing", body);
			}
		}
	}
}

/*
 * THE SEND. croc keeps stdin and stdout; only stderr is taken, because that is
 * where croc writes and this program has to both show it and read it.
 */
static int send_files(const char *const *paths, int n)
{
	KbArgv a = { 0 };
	const char *code = NULL;
	int fd[2];
	pid_t pid;
	int st;

	if (!kb_have_prog("croc"))
		kb_die("croc is not installed");

	kb_argv_add(&a, "croc");
	/* GLOBAL, so before the verb: `croc send --ignore-stdin` is refused by
	 * croc's own option parser. */
	kb_argv_add(&a, "--ignore-stdin");
	kb_argv_add(&a, "send");
	for (int i = 0; i < n; i++)
		kb_argv_add(&a, paths[i]);
	kb_argv_end(&a);

	if (pipe(fd) < 0)
		kb_die("pipe: %s", strerror(errno));
	pid = fork();
	if (pid < 0)
		kb_die("fork: %s", strerror(errno));
	if (pid == 0) {
		int null = open("/dev/null", O_RDONLY);

		/* Closed and not inherited: croc reads stdin for a piped
		 * payload, and `--ignore-stdin` is the flag that stops it —
		 * this is the belt beside it. */
		if (null >= 0) {
			dup2(null, STDIN_FILENO);
			if (null > STDERR_FILENO)
				close(null);
		}
		dup2(fd[1], STDERR_FILENO);
		close(fd[0]);
		close(fd[1]);
		execvp(a.v[0], (char *const *)a.v);
		_exit(127);
	}
	close(fd[1]);
	relay(fd[0], &code);
	close(fd[0]);

	while (waitpid(pid, &st, 0) < 0)
		if (errno != EINTR)
			return 1;
	return WIFEXITED(st) ? WEXITSTATUS(st) : 1;
}

/*
 * A WINDOW OF ITS OWN WHEN THERE IS NO TERMINAL TO USE.
 *
 * The Share verb is reached from a desktop icon, the file chooser and `mc`'s
 * `F2`, and the first two have no terminal at all — a transfer started there
 * with nowhere to draw is a transfer nobody can see finish or cancel. Typed at
 * a prompt it runs where it was typed, which is what a person who typed it
 * meant.
 *
 * THE TERMINAL FOLLOWS THE DESKTOP, through `kb_terminal()`: `foot` needs a
 * compositor and `kdos-term` is what the console has.
 */
static int in_terminal(const char *const *paths, int n)
{
	KbArgv a = { 0 };

	const char *term = kb_terminal();

	/* NOWHERE TO OPEN A WINDOW is not nowhere to run: a bare virtual
	 * terminal has neither emulator, and the transfer belongs on the
	 * screen the caller already has. */
	if (!term)
		return send_files(paths, n);
	kb_argv_add(&a, term);
	kb_argv_add(&a, "--title");
	kb_argv_add(&a, "Share");
	kb_argv_add(&a, "-e");
	kb_argv_add(&a, "kdos-share");
	kb_argv_add(&a, "--here");
	for (int i = 0; i < n; i++)
		kb_argv_add(&a, paths[i]);
	kb_argv_end(&a);
	return kb_run(&a) == 0 ? 0 : 1;
}

int share_main(int argc, char **argv)
{
	/*
	 * STATIC, because kb_argv_add stores the pointer it is given and does
	 * not copy: a word handed to it has to outlive the exec.
	 */
	static char resolved[SHARE_MAX][4096];
	const char *paths[SHARE_MAX];
	int n = 0, here = 0;

	for (int i = 1; i < argc; i++) {
		const char *a = argv[i];

		if (!strcmp(a, "-h") || !strcmp(a, "--help"))
			return usage();
		if (!strcmp(a, "--here")) {
			here = 1;
			continue;
		}
		if (!strcmp(a, "--clipboard")) {
			if (n >= SHARE_MAX)
				kb_die("too many files in one transfer");
			snprintf(resolved[n], sizeof(resolved[n]), "%s",
				 clip_file());
			paths[n] = resolved[n];
			n++;
			continue;
		}
		if (a[0] == '-')
			return usage();
		if (n >= SHARE_MAX)
			kb_die("too many files in one transfer");
		/* A path from a command line and a `file://` URI from
		 * `kdos-pick` are the same argument to this program. */
		if (!kb_uri_path(a, resolved[n], sizeof(resolved[n])))
			kb_die("%s: not a file on this machine", a);
		if (access(resolved[n], R_OK) != 0)
			kb_die("%s: %s", resolved[n], strerror(errno));
		paths[n] = resolved[n];
		n++;
	}

	if (!n) {
		const char *uri = pick_one();

		if (!uri)
			return 0;	/* cancelled */
		if (!kb_uri_path(uri, resolved[0], sizeof(resolved[0])))
			kb_die("%s: not a file on this machine", uri);
		if (access(resolved[0], R_OK) != 0)
			kb_die("%s: %s", resolved[0], strerror(errno));
		paths[0] = resolved[0];
		n = 1;
	}

	/*
	 * THE CLIPBOARD IS READ ONCE, HERE. Re-reading it inside the window
	 * would send whatever had been copied by the time the window opened,
	 * which is not what the person asked to share.
	 */
	if (!here && !isatty(STDERR_FILENO))
		return in_terminal(paths, n);

	int rc = send_files(paths, n);

	/*
	 * A WINDOW THIS PROGRAM OPENED, IT ALSO HOLDS OPEN. `kdos-term` has no
	 * `--hold` and the window closes with the command, so a transfer that
	 * finished would take its own result off the screen with it — and
	 * whether it finished is the one thing the person was watching for.
	 * Nothing waits where the person already had a prompt.
	 */
	if (here) {
		fputs("\n[done — press Enter to close]", stderr);
		fflush(stderr);
		getchar();
	}
	return rc;
}
