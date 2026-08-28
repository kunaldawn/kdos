/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   kdos-boxinit — pid 1 inside a pack box
 * ---------------------------------
 *
 * distrobox-init is 900 lines of shell that reconciles a container image with
 * the host it is running on: it creates the user, fixes up sudo, links host
 * paths and finally prints `container_setup_done`, which is the marker
 * `box_wait_ready()` polls for. A pack box needs a small, specific part of
 * that, because most of what distrobox-init reconciles is decided at bake time
 * here rather than found at run time.
 *
 * WHAT IT DOES, and each line of it is something the launch path already
 * depends on:
 *
 *   - creates the group and the user matching the HOST's, so `$HOME` — which
 *     is bind-mounted from the host and owned by that uid — belongs to
 *     somebody inside the container as well as outside it;
 *   - makes sure that home exists and is on the PATH's mind;
 *   - exports a PATH that includes /usr/games, because Debian's games live
 *     there and a launcher for one otherwise dies on "not found";
 *   - prints `container_setup_done` on stdout, where podman logs keep it, and
 *     `box_setup_done()` looks for it;
 *   - then stays alive as pid 1, reaping whatever the box's processes orphan.
 *
 * IT IS PID 1 AND MUST BEHAVE LIKE ONE. A pid 1 that exits takes the container
 * with it, and a pid 1 that does not reap leaves a zombie per exited app until
 * the pid table fills. Neither is a thing to discover in a box somebody is
 * working in.
 *
 * NO SHELL, ANYWHERE. The user name, the home path and the shell all arrive
 * from the environment the launcher set, and a shell in the middle of that is
 * an injection point in a program running as the container's root.
 */

#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

#include "kbase.h"

#define BOXINIT_PATH \
	"/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin:/usr/games"

static const char *env_or(const char *name, const char *dflt)
{
	const char *v = getenv(name);
	return v && *v ? v : dflt;
}

/*
 * Put a record into a colon-separated database, REPLACING one that is already
 * there for the same name or id rather than leaving it alone.
 *
 * A pack may carry its own entry for this user, and a WRONG one is worse than
 * none: the base pack ships `kdos:*:1000:1000:KDOS Test:/:/bin/sh`, whose home
 * is `/`, so `podman exec --user=1000` gives every application HOME=/ and none
 * of them reads ~/.config, ~/.themes or ~/.icons. Measured as a boxed GTK4 app
 * in libadwaita's own light theme on a phosphor desktop, with the palette
 * sitting correctly in a $HOME the app never looked at.
 *
 * Matching this user's HOST record is the whole reason this program creates
 * the account at all, so it wins over whatever the image put there. Appending
 * a second line instead would make every lookup ambiguous, which is what the
 * name/id check was there to prevent.
 *
 * Written atomically: /etc/passwd with a failed write in the middle of it is a
 * container with no users in it.
 */
static int ensure_record(const char *path, const char *name, const char *id,
			 const char *line)
{
	size_t len = 0;
	char *text = kb_read_all(path, &len);
	char *out, *cur, *save;
	size_t cap;
	int replaced = 0;
	int rc;

	if (!text) {
		FILE *f = fopen(path, "a");
		if (!f)
			return -1;
		fputs(line, f);
		return fclose(f) == 0 ? 0 : -1;
	}

	cap = len + strlen(line) + 2;
	out = calloc(1, cap);
	if (!out) {
		free(text);
		return -1;
	}

	for (cur = strtok_r(text, "\n", &save); cur;
	     cur = strtok_r(NULL, "\n", &save)) {
		char probe[512];
		char *colon, *second, *third;
		int match = 0;

		kb_strlcpy(probe, cur, sizeof(probe));
		colon = strchr(probe, ':');
		if (colon) {
			*colon = 0;
			if (!strcmp(probe, name)) {
				match = 1;
			} else {
				/* name:x:<id>: — the third field */
				second = strchr(colon + 1, ':');
				if (second) {
					third = strchr(second + 1, ':');
					if (third)
						*third = 0;
					if (!strcmp(second + 1, id))
						match = 1;
				}
			}
		}
		if (match) {
			if (!replaced) {
				strcat(out, line);
				replaced = 1;
			}
			/* a duplicate for the same user is dropped */
		} else {
			strcat(out, cur);
			strcat(out, "\n");
		}
	}
	if (!replaced)
		strcat(out, line);

	rc = kb_write_file_atomic(path, out);
	free(out);
	free(text);
	return rc;
}

static void reap(int sig)
{
	(void)sig;
	while (waitpid(-1, NULL, WNOHANG) > 0)
		;
}

static volatile sig_atomic_t stopping;

static void stop(int sig)
{
	(void)sig;
	stopping = 1;
}

int main(int argc, char **argv)
{
	const char *user = env_or("KDOS_BOX_USER", "kdos");
	const char *uid = env_or("KDOS_BOX_UID", "1000");
	const char *gid = env_or("KDOS_BOX_GID", "1000");
	const char *home = env_or("KDOS_BOX_HOME", "/home/kdos");
	const char *shell = env_or("KDOS_BOX_SHELL", "/bin/bash");
	const char *box = env_or("KDOS_BOX", "box");
	char line[1024];
	struct sigaction sa;

	kb_set_progname("kdos-boxinit");
	(void)argc;
	(void)argv;

	snprintf(line, sizeof(line), "%s:x:%s:\n", user, gid);
	ensure_record("/etc/group", user, gid, line);

	snprintf(line, sizeof(line), "%s:x:%s:%s::%s:%s\n", user, uid, gid, home,
		 shell);
	ensure_record("/etc/passwd", user, uid, line);

	/*
	 * A locked password, not an empty one. Nothing in this design logs in
	 * to a box — every entry is a `podman exec` from a process that is
	 * already the user — so an account that can be authenticated to is a
	 * surface with no purpose.
	 */
	snprintf(line, sizeof(line), "%s:!:20000:0:99999:7:::\n", user);
	ensure_record("/etc/shadow", user, uid, line);

	/* The home is bind-mounted from the host and already exists; making it
	 * covers the private-home case, where the profile pointed somewhere
	 * that does not exist yet. */
	kb_mkdir_p(home);

	setenv("PATH", BOXINIT_PATH, 1);
	setenv("HOME", home, 1);

	/*
	 * The readiness marker. `box_setup_done()` greps podman's logs for it,
	 * and `box_wait_ready()` polls that — so this line, on stdout,
	 * unbuffered, IS the contract between the container and the launcher.
	 * Printing it before the records above were written would let a launch
	 * exec into a box with no user in it.
	 */
	printf("container_setup_done\n");
	fflush(stdout);

	memset(&sa, 0, sizeof(sa));
	sa.sa_handler = reap;
	sa.sa_flags = SA_RESTART | SA_NOCLDSTOP;
	sigaction(SIGCHLD, &sa, NULL);
	sa.sa_handler = stop;
	sa.sa_flags = 0;
	sigaction(SIGTERM, &sa, NULL);
	sigaction(SIGINT, &sa, NULL);

	/*
	 * pid 1 for the life of the box. `pause()` rather than a sleep loop:
	 * there is nothing to poll, the signal handlers do the work, and a
	 * process that wakes up once a second inside every running box is a
	 * cost the energy report would eventually have to explain.
	 */
	while (!stopping)
		pause();
	fprintf(stderr, "kdos-boxinit: %s stopping\n", box);
	return 0;
}
