/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   kdos-shot [region|screen|window]
 *
 * Screenshot to the clipboard AND to disk. COSMIC's own PrtSc opens an
 * interactive picker that saves a file; this is the other half — one
 * keystroke, region selected with the mouse, image on the clipboard ready to
 * paste, a copy filed under ~/Pictures/Screenshots, and a notification so you
 * know it worked.
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
	if (!kb_have_prog("gdbus"))
		return;
	KbArgv a = {0};
	kb_argv_add(&a, "gdbus");
	kb_argv_add(&a, "call");
	kb_argv_add(&a, "--session");
	kb_argv_add(&a, "--dest");
	kb_argv_add(&a, "org.freedesktop.Notifications");
	kb_argv_add(&a, "--object-path");
	kb_argv_add(&a, "/org/freedesktop/Notifications");
	kb_argv_add(&a, "--method");
	kb_argv_add(&a, "org.freedesktop.Notifications.Notify");
	kb_argv_add(&a, "kdos-shot");
	kb_argv_add(&a, "0");
	kb_argv_add(&a, "");
	kb_argv_add(&a, summary);
	kb_argv_add(&a, body);
	kb_argv_add(&a, "[]");
	kb_argv_add(&a, "{}");
	kb_argv_add(&a, "5000");
	kb_argv_end(&a);
	kb_run(&a);
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

/* cosmic-comp does not implement wlr-screencopy, so grim only works on
 * compositors that do. The portal path (cosmic-screenshot) is the native way,
 * and this is how we find out which world we are in. */
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

int shot_main(int argc, char **argv)
{
	const char *mode = argc > 1 ? argv[1] : "region";
	if (!strcmp(mode, "window"))
		mode = "region";	/* per-window needs the compositor's help */

	if (strcmp(mode, "region") && strcmp(mode, "screen") &&
	    strcmp(mode, "full")) {
		fprintf(stderr, "usage: kdos-shot [region|screen|window]\n");
		return 1;
	}

	const char *pics = getenv("XDG_PICTURES_DIR");
	char *base = pics && *pics ? kb_strdup(pics)
				   : kb_path_join(kb_home_dir(), "Pictures");
	char *dir = kb_path_join(base, "Screenshots");
	free(base);
	kb_mkdir_p(dir);

	if (!grim_works() && kb_have_prog("cosmic-screenshot")) {
		char out[1024] = {0};
		KbArgv a = {0};
		kb_argv_add(&a, "cosmic-screenshot");
		kb_argv_add(&a, "--interactive=false");
		kb_argv_add(&a, "--save-dir");
		kb_argv_add(&a, dir);
		kb_argv_end(&a);
		if (kb_run_capture(&a, out, sizeof(out)) != 0)
			kb_die("cosmic-screenshot failed");
		if (out[0] && access(out, R_OK) == 0) {
			clip(out);
			toast("Screenshot copied", kb_basename(out));
			printf("%s\n", out);
			return 0;
		}
	}

	if (!kb_have_prog("grim"))
		kb_die("grim is not installed");

	time_t now = time(NULL);
	struct tm tm;
	localtime_r(&now, &tm);
	char leaf[64];
	strftime(leaf, sizeof(leaf), "kdos-%Y%m%d-%H%M%S.png", &tm);
	char *file = kb_path_join(dir, leaf);

	KbArgv g = {0};
	kb_argv_add(&g, "grim");

	if (!strcmp(mode, "region")) {
		if (!kb_have_prog("slurp"))
			kb_die("slurp is not installed");
		char geom[128] = {0};
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
		if (kb_run_capture(&s, geom, sizeof(geom)) != 0 || !geom[0])
			return 0;
		kb_argv_add(&g, "-g");
		kb_argv_add(&g, geom);
	}

	kb_argv_add(&g, file);
	kb_argv_end(&g);
	if (kb_run(&g) != 0) {
		toast("Screenshot failed", "grim could not capture");
		return 1;
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
