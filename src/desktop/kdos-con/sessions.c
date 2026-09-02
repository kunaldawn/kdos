/* kdos-con — named sessions. See con.h.
 *
 * A session is two sockets in $XDG_RUNTIME_DIR/kdos, named after it:
 *
 *   <name>.sock    surfaces — programs that draw windows in the session
 *   <name>.view    views — a display, which holds no window state at all
 *
 * TWO SOCKETS BECAUSE ONLY ONE OF THEM IS SAFE TO FORWARD. `kdos con forward`
 * carries the view socket to another machine, and a display is trusted with
 * nothing: it is handed cells and reports events. The surface socket is the
 * right to place a window in your session and never leaves the machine.
 *
 * The kind of a client is decided by which socket it reached, not by what it
 * says — see kcon_server_listen().
 */

#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <unistd.h>

#include "con.h"
#include "kbase.h"

/*
 * $XDG_RUNTIME_DIR/kdos, made 0700.
 *
 * A DIRECTORY THAT ALREADY EXISTS WITH THE WRONG MODE OR OWNER IS A REFUSAL,
 * not something to chmod. If it is not ours, something else made it, and
 * quietly taking it over is how a socket ends up in a path another account
 * chose — after which the peer-credential check is guarding the wrong door.
 */
int con_rundir(char *out, size_t cap)
{
	const char *run = getenv("XDG_RUNTIME_DIR");
	struct stat st;

	if (!run || !*run) {
		fprintf(stderr,
			"kdos-con: no XDG_RUNTIME_DIR, so nowhere private to\n"
			"          put a socket. A session needs one.\n");
		return -1;
	}
	snprintf(out, cap, "%s/kdos", run);

	if (mkdir(out, 0700) != 0 && errno != EEXIST) {
		fprintf(stderr, "kdos-con: cannot make %s: %s\n", out,
			strerror(errno));
		return -1;
	}
	if (lstat(out, &st) != 0 || !S_ISDIR(st.st_mode)) {
		fprintf(stderr, "kdos-con: %s is not a directory\n", out);
		return -1;
	}
	if (st.st_uid != getuid() || (st.st_mode & 0077)) {
		fprintf(stderr,
			"kdos-con: %s is not private to you (mode %04o, uid %u).\n"
			"          Refusing rather than changing it: something\n"
			"          else made it.\n",
			out, (unsigned)(st.st_mode & 07777),
			(unsigned)st.st_uid);
		return -1;
	}
	return 0;
}

int con_session_paths(const char *name, char *sock, size_t scap,
		      char *view, size_t vcap)
{
	char dir[96];

	/* A name is a FILENAME, so it may not contain a path separator and may
	 * not be a traversal. Everything else is the user's business. */
	if (!name || !*name || strchr(name, '/') || !strcmp(name, ".") ||
	    !strcmp(name, "..")) {
		fprintf(stderr, "kdos-con: '%s' is not a session name\n",
			name ? name : "");
		return -1;
	}
	if (con_rundir(dir, sizeof(dir)) != 0)
		return -1;

	snprintf(sock, scap, "%s/%s.sock", dir, name);
	snprintf(view, vcap, "%s/%s.view", dir, name);
	return 0;
}

/*
 * Is anything listening? A socket file outlives the process that made it if
 * that process was killed, so the file's existence proves nothing and a
 * connect does. Non-destructive: the connection is closed without a hello, and
 * the server drops a peer that never sends one.
 */
static int sock_live(const char *path)
{
	struct sockaddr_un sa;
	int fd = socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0);
	int ok;

	if (fd < 0)
		return 0;
	memset(&sa, 0, sizeof(sa));
	sa.sun_family = AF_UNIX;
	snprintf(sa.sun_path, sizeof(sa.sun_path), "%s", path);
	ok = connect(fd, (struct sockaddr *)&sa, sizeof(sa)) == 0;
	close(fd);
	return ok;
}

int con_sessions_list(void)
{
	char dir[96];
	char **names;
	int n = 0, found = 0;

	if (con_rundir(dir, sizeof(dir)) != 0)
		return 1;

	names = kb_listdir(dir, &n);
	if (!names) {
		printf("no sessions\n");
		return 0;
	}

	for (int i = 0; i < n; i++) {
		size_t len = strlen(names[i]);
		char base[64], sock[192], view[192];

		if (len < 6 || len - 5 >= sizeof(base) ||
		    strcmp(names[i] + len - 5, ".sock"))
			continue;
		snprintf(base, sizeof(base), "%.*s", (int)(len - 5), names[i]);
		snprintf(sock, sizeof(sock), "%s/%s", dir, names[i]);
		snprintf(view, sizeof(view), "%s/%s.view", dir, base);

		if (!sock_live(sock)) {
			/* A socket whose session is gone is litter, and
			 * listing it would send someone to attach to nothing.
			 * Both files go: the view socket is the same corpse. */
			unlink(sock);
			unlink(view);
			continue;
		}
		found++;
		printf("%-16s %s\n", base,
		       sock_live(view) ? "attachable" : "no view socket");
	}
	kb_strv_free(names);

	if (!found)
		printf("no sessions\n");
	return 0;
}

int con_session_kill(const char *name)
{
	char sock[192], view[192];

	if (con_session_paths(name, sock, sizeof(sock), view,
			      sizeof(view)) != 0)
		return 2;
	if (!sock_live(sock)) {
		fprintf(stderr, "kdos-con: no session '%s'\n", name);
		return 1;
	}

	/*
	 * UNLINK, DO NOT SIGNAL. There is no pid in a socket path and looking
	 * one up by name would kill whichever process happened to match. A
	 * session whose sockets are gone loses its clients and its supervisor
	 * ends it — the same path a logout takes.
	 */
	unlink(sock);
	unlink(view);
	return 0;
}
