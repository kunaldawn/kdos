/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   kdos-shot [region|screen|full|window|qr]
 *
 * Screenshot to the clipboard AND to disk, through Spectacle. One keystroke,
 * region selected with the mouse, image on the clipboard ready to paste, a
 * copy filed under ~/Pictures/Screenshots, and a notification so you know it
 * worked.
 *
 * SPECTACLE TAKES THE PICTURE, in its background mode (`-b`), without its own
 * notification (`-n`): it is the one program KWin hands a screen to, through
 * its ScreenShot2 interface, and there is no other capture path on this
 * desktop. What is left here is the naming, the clipboard, the one
 * notification and `qr`.
 * ---------------------------------
 */

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/wait.h>

#include "kdos-tools.h"

/* Best effort — a screenshot must never fail on a notification. */
static void toast(const char *summary, const char *body)
{
	kb_notify("kdos-shot", summary, body);
}

/* wl-copy reads the image on stdin. */
static void clip(const char *file)
{
	if (!kb_have_prog("wl-copy"))
		return;
	pid_t pid = fork();
	if (pid < 0)
		return;
	if (pid == 0) {
		int fd = open(file, O_RDONLY | O_CLOEXEC);
		if (fd >= 0)
			dup2(fd, STDIN_FILENO);
		int null = open("/dev/null", O_WRONLY);
		if (null >= 0) {
			dup2(null, STDOUT_FILENO);
			dup2(null, STDERR_FILENO);
		}
		execlp("wl-copy", "wl-copy", "--type", "image/png", (char *)NULL);
		_exit(127);
	}
	int st;
	while (waitpid(pid, &st, 0) < 0)
		;
}

/*
 * A QR CODE ON THE SCREEN, DECODED — AND THE PICTURE NEVER KEPT.
 *
 * A QR is a secret more often than not: a wifi password, a one-time code, a
 * pairing token. So the image goes to a temp file `zbarimg` can open, the text
 * comes back, and the file is removed on every path out of here — including
 * the ones where nothing was found and where zbarimg was not installed.
 *
 * THE TEXT GOES TO THE CLIPBOARD AND NOWHERE ELSE. Not to ~/Pictures, not to
 * the toast body, not to stdout when a toast is possible: a decoded code
 * printed into a terminal is a code in that terminal's scrollback, which is
 * the same mistake one level along.
 */
static int shot_qr(const char *png)
{
	KbArgv z = {0};
	char out[1024] = {0};
	int rc;

	if (!kb_have_prog("zbarimg")) {
		toast("QR", "zbarimg is not installed");
		unlink(png);
		return 1;
	}

	kb_argv_add(&z, "zbarimg");
	kb_argv_add(&z, "-q");		/* no per-symbol chatter */
	kb_argv_add(&z, "--raw");	/* the payload, without the QR-Code: prefix */
	kb_argv_add(&z, png);
	kb_argv_end(&z);
	rc = kb_run_capture(&z, out, sizeof(out));

	/* BEFORE ANYTHING ELSE. Every path below returns, and a picture of
	 * somebody's login code left in /tmp is the failure this verb exists
	 * to avoid. */
	unlink(png);

	if (rc != 0 || !out[0]) {
		toast("QR", "no code found in that region");
		return 1;
	}

	/* zbarimg ends the payload with a newline; a clipboard that carried it
	 * would paste a stray Enter into whatever is focused — which at a
	 * shell prompt runs the line. */
	size_t n = strlen(out);

	while (n && (out[n - 1] == '\n' || out[n - 1] == '\r'))
		out[--n] = '\0';
	if (!n) {
		toast("QR", "no code found in that region");
		return 1;
	}

	if (!kdt_clip_put(out)) {
		toast("QR", "nothing here can reach the clipboard");
		return 1;
	}
	/* Never the payload: a toast is drawn on a screen somebody else may be
	 * looking at. */
	toast("QR copied", "the code is on the clipboard");
	return 0;
}

static void shot_usage(FILE *o)
{
	fprintf(o, "usage: kdos-shot [region|screen|full|window|qr]\n");
}

/*
 * EVERY ARGUMENT IS READ BEFORE ANYTHING IS DONE. The default mode is region,
 * which waits for a drag: an argument taken for "no mode given" would leave a
 * script that asked for help, or passed a typo, waiting on a pointer nobody is
 * moving. So an option is either one of the two below or an error, and a
 * refusal comes before the screenshot directory is made.
 */
int shot_main(int argc, char **argv)
{
	static const struct { const char *mode, *flag; } MODES[] = {
		{ "region", "-r" },
		{ "qr", "-r" },
		{ "screen", "-m" },
		{ "full", "-f" },
		{ "window", "-a" },
	};
	const char *mode = NULL, *flag = NULL;

	for (int i = 1; i < argc; i++) {
		const char *a = argv[i];
		if (!strcmp(a, "-h") || !strcmp(a, "--help")) {
			shot_usage(stdout);
			return 0;
		}
		if (!strcmp(a, "--geom")) {
			fprintf(stderr, "kdos-shot: --geom is refused: this "
				"desktop selects with the pointer\n");
			return 1;
		}
		if (a[0] == '-' || mode) {
			shot_usage(stderr);
			return 1;
		}
		mode = a;
	}
	if (!mode)
		mode = "region";
	for (size_t i = 0; i < sizeof(MODES) / sizeof(MODES[0]); i++)
		if (!strcmp(mode, MODES[i].mode))
			flag = MODES[i].flag;
	if (!flag) {
		shot_usage(stderr);
		return 1;
	}
	if (!kb_have_prog("spectacle")) {
		fprintf(stderr, "kdos-shot: spectacle is not installed\n");
		return 1;
	}

	const char *pics = getenv("XDG_PICTURES_DIR");
	char *base = pics && *pics ? kb_strdup(pics)
				   : kb_path_join(kb_home_dir(), "Pictures");
	char *dir = kb_path_join(base, "Screenshots");
	free(base);
	/* NOT FOR `qr`. That verb writes nothing that survives it, and a
	 * directory appearing under ~/Pictures is still a trace of a secret
	 * having been read. */
	if (strcmp(mode, "qr"))
		kb_mkdir_p(dir);

	time_t now = time(NULL);
	struct tm tm;
	localtime_r(&now, &tm);
	char leaf[64];
	strftime(leaf, sizeof(leaf), "kdos-%Y%m%d-%H%M%S.png", &tm);
	/* A QR goes to the runtime directory — 0700 and this person's own —
	 * and never to ~/Pictures, because it is deleted the moment it has
	 * been read. */
	char qrpath[256];
	char *file;

	if (!strcmp(mode, "qr")) {
		snprintf(qrpath, sizeof(qrpath), "%s/kdos-qr.png",
			 kb_runtime_dir());
		file = kb_strdup(qrpath);
	} else {
		file = kb_path_join(dir, leaf);
	}
	unlink(file);

	KbArgv g = {0};
	kb_argv_add(&g, "spectacle");
	kb_argv_add(&g, "-b");
	kb_argv_add(&g, "-n");
	kb_argv_add(&g, flag);
	kb_argv_add(&g, "-o");
	kb_argv_add(&g, file);
	kb_argv_end(&g);
	/* A selection cancelled with Escape writes no file, and is a person's
	 * decision rather than a failure: nothing is said. */
	if (kb_run(&g) != 0 || !kb_path_exists(file)) {
		if (strcmp(mode, "region") && strcmp(mode, "qr"))
			toast("Screenshot failed", "Spectacle could not "
				  "capture the screen");
		unlink(file);
		free(dir);
		free(file);
		return strcmp(mode, "region") && strcmp(mode, "qr") ? 1 : 0;
	}

	if (!strcmp(mode, "qr")) {
		int rc = shot_qr(file);	/* removes the picture, always */

		free(dir);
		free(file);
		return rc;
	}

	if (kb_have_prog("wl-copy")) {
		clip(file);
		toast("Screenshot copied", kb_basename(file));
	} else {
		toast("Screenshot saved", kb_basename(file));
	}

	printf("%s\n", file);
	free(dir);
	free(file);
	return 0;
}
