/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   kdos-shot [region|screen|window]
 *
 * Screenshot to the clipboard AND to disk. One keystroke, region selected
 * with the mouse, image on the clipboard ready to paste, a copy filed under
 * ~/Pictures/Screenshots, and a notification so you know it worked.
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

/* grim needs ext-image-copy-capture or, failing that, wlr-screencopy.
 * kdos-comp implements both (wlroots supplies them), so this probe should
 * always succeed — it stays because the failure it catches is otherwise mute:
 * grim exits non-zero with nothing useful on stderr when neither global is
 * there. */
static int grim_works(void)
{
	KbArgv a = {0};
	kb_argv_add(&a, "grim");
	kb_argv_add(&a, "-t");
	kb_argv_add(&a, "png");
	kb_argv_add(&a, "/dev/null");
	kb_argv_end(&a);
	return kb_run(&a) == 0;
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
	/* The LENGTH, never the payload: a toast is drawn on a screen somebody
	 * else may be looking at. */
	toast("QR copied", "the code is on the clipboard");
	return 0;
}

int shot_main(int argc, char **argv)
{
	const char *mode = argc > 1 && argv[1][0] != '-' ? argv[1] : "region";
	const char *geom = NULL;

	if (!strcmp(mode, "window"))
		mode = "region";	/* per-window needs the compositor's help */

	if (strcmp(mode, "region") && strcmp(mode, "screen") &&
	    strcmp(mode, "full") && strcmp(mode, "qr")) {
		fprintf(stderr,
			"usage: kdos-shot [region|screen|window|qr] "
			"[--geom X,Y,W,H]\n");
		return 1;
	}

	for (int i = 1; i < argc; i++) {
		if (strcmp(argv[i], "--geom") || i + 1 >= argc)
			continue;
		geom = argv[++i];
	}
	if (geom) {
		int g[4];

		if (sscanf(geom, "%d,%d,%d,%d", &g[0], &g[1], &g[2], &g[3]) !=
		    4) {
			fprintf(stderr,
				"kdos-shot: --geom wants X,Y,W,H\n");
			return 1;
		}
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

	if (geom)
		kb_die("--geom is a cell rectangle and this desktop selects "
		       "with slurp");

	if (!kb_have_prog("grim"))
		kb_die("grim is not installed");
	if (!grim_works())
		kb_die("grim cannot capture — the compositor does not offer "
		       "wlr-screencopy");

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

	KbArgv g = {0};
	kb_argv_add(&g, "grim");

	/* A QR is selected the way a region is: it is a rectangle of the
	 * screen, and there is no other way to say which one. */
	if (!strcmp(mode, "region") || !strcmp(mode, "qr")) {
		if (!kb_have_prog("slurp"))
			kb_die("slurp is not installed");
		char sel[128] = {0};
		KbArgv s = {0};
		kb_argv_add(&s, "slurp");
		kb_argv_add(&s, "-b");
		kb_argv_add(&s, "000a03cc");
		kb_argv_add(&s, "-c");
		kb_argv_add(&s, "39ff14ff");
		kb_argv_add(&s, "-s");
		kb_argv_add(&s, "39ff1420");
		kb_argv_add(&s, "-w");
		kb_argv_add(&s, "2");
		kb_argv_end(&s);
		/* slurp writes nothing and exits non-zero when the selection is
		 * cancelled with Escape — that is a user decision, not a
		 * failure. */
		if (kb_run_capture(&s, sel, sizeof(sel)) != 0 || !sel[0])
			return 0;
		kb_argv_add(&g, "-g");
		kb_argv_add(&g, sel);
	}

	kb_argv_add(&g, file);
	kb_argv_end(&g);
	if (kb_run(&g) != 0) {
		toast("Screenshot failed", "grim could not capture");
		return 1;
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
