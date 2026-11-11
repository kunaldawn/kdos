/*
 * ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   KD's Homebrew Linux Distro
 * ---------------------------------
 *
 * Process, path and locking helpers belong in libkbase and not here: a second
 * copy of them beside the installer's drifts from it quietly, and the two
 * disagree about the cases nobody tests. What stays in this file is what only
 * appbox has a use for.
 *
 * The no-shell rule holds here as it does there: app names, package names
 * and file arguments all reach this program from .desktop files and the
 * command line, and a shell in the middle turns any of them into an injection
 * point.
 */

#include "kdos-appbox.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/statvfs.h>
#include <sys/vfs.h>
#include <time.h>

/* A process's start time in clock ticks, field 22 of /proc/<pid>/stat: what
 * tells the pid a ledger line names from a later process given the same
 * number. 0 when the process is gone. The command name is skipped past its
 * LAST `)`, because a name may itself contain one. */
unsigned long long proc_start_time(long pid)
{
	char path[64], st[1024], *rp, *tok, *save = NULL;
	int field = 3;

	snprintf(path, sizeof(path), "/proc/%ld/stat", pid);
	if (kb_read_file(path, st, sizeof(st)) <= 0)
		return 0;
	rp = strrchr(st, ')');
	if (!rp || !rp[1])
		return 0;
	for (tok = strtok_r(rp + 2, " ", &save); tok;
	     tok = strtok_r(NULL, " ", &save), field++)
		if (field == 22)
			return strtoull(tok, NULL, 10);
	return 0;
}

/* Stage timings, so "why did that take so long" stays answerable afterwards. */
void tracef(const char *fmt, ...)
{
	char *path = kb_path_join(kb_runtime_dir(), "kdos-appbox.trace");
	FILE *f = fopen(path, "a");
	struct timespec ts;
	va_list ap;

	free(path);
	if (!f)
		return;
	clock_gettime(CLOCK_REALTIME, &ts);
	fprintf(f, "%ld.%06ld ", (long)ts.tv_sec, ts.tv_nsec / 1000);
	va_start(ap, fmt);
	vfprintf(f, fmt, ap);
	va_end(ap);
	fputc('\n', f);
	fclose(f);
}

/*
 * Desktop notification straight over the bus, through gdbus, which glib
 * already puts on the host: this program compiles in its libk* sources and
 * links no library of anybody else's, libnotify included. Only reachable at
 * all because the session bus lives at a fixed path in $XDG_RUNTIME_DIR,
 * which the appbox shares.
 */
void notify(const char *summary, const char *body)
{
	KbArgv a = {0};
	kb_argv_add(&a, "gdbus");
	kb_argv_add(&a, "call");
	kb_argv_add(&a, "--timeout");
	kb_argv_add(&a, "2");
	kb_argv_add(&a, "--session");
	kb_argv_add(&a, "--dest");
	kb_argv_add(&a, "org.freedesktop.Notifications");
	kb_argv_add(&a, "--object-path");
	kb_argv_add(&a, "/org/freedesktop/Notifications");
	kb_argv_add(&a, "--method");
	kb_argv_add(&a, "org.freedesktop.Notifications.Notify");
	kb_argv_add(&a, "KDOS");
	kb_argv_add(&a, "0");
	kb_argv_add(&a, "distributor-logo-kdos");
	kb_argv_add(&a, summary);
	kb_argv_add(&a, body);
	kb_argv_add(&a, "[]");
	kb_argv_add(&a, "{}");
	kb_argv_add(&a, "8000");
	kb_argv_end(&a);
	kb_run_detach(&a);
}

/*
 * THE FILESYSTEM A PATH'S WRITES WOULD LAND ON, asked of the nearest directory
 * that exists: a box's layer directory or podman's storage is often not made
 * yet when the question is asked, and its parent is where it will be made.
 */
static int nearest_dir(const char *path, char *out, size_t n)
{
	kb_strlcpy(out, path, n);
	while (out[0] && !kb_is_dir(out)) {
		char *slash = strrchr(out, '/');

		if (!slash)
			return -1;
		if (slash == out) {
			out[1] = '\0';
			break;
		}
		*slash = '\0';
	}
	return out[0] ? 0 : -1;
}

/*
 * AN OVERLAY IS MEMORY UNLESS ITS UPPER IS THE PERSISTENCE STORE. A live
 * session whose `kdos persist` store is in use mounts it at /mnt/persist and
 * puts the overlay's upper there, so its writes outlast the session; any other
 * overlay on a live medium has a tmpfs upper.
 */
int fs_in_memory(const char *path)
{
	char dir[MAX_LINE];
	struct statfs st;

	if (nearest_dir(path, dir, sizeof(dir)) != 0 || statfs(dir, &st) != 0)
		return 0;
	if ((unsigned long)st.f_type == FS_TMPFS_MAGIC)
		return 1;
	if ((unsigned long)st.f_type != FS_OVERLAY_MAGIC)
		return 0;
	{
		size_t len = 0;
		char *mounts = kb_read_all("/proc/mounts", &len);
		int persist = mounts && strstr(mounts, " /mnt/persist ") != NULL;

		free(mounts);
		return !persist;
	}
}

int fs_is_overlay(const char *path)
{
	char dir[MAX_LINE];
	struct statfs st;

	if (nearest_dir(path, dir, sizeof(dir)) != 0 || statfs(dir, &st) != 0)
		return 0;
	return (unsigned long)st.f_type == FS_OVERLAY_MAGIC;
}

unsigned long long fs_free_bytes(const char *path)
{
	char dir[MAX_LINE];
	struct statvfs st;

	if (nearest_dir(path, dir, sizeof(dir)) != 0 || statvfs(dir, &st) != 0)
		return 0;
	return (unsigned long long)st.f_bavail * st.f_frsize;
}

/* rootless podman's default graph root: $XDG_DATA_HOME/containers/storage,
 * else ~/.local/share/containers/storage. */
void podman_storage_dir(char *out, size_t n)
{
	const char *xdg = getenv("XDG_DATA_HOME");

	if (xdg && *xdg == '/')
		snprintf(out, n, "%s/containers/storage", xdg);
	else
		snprintf(out, n, "%s/.local/share/containers/storage",
			 kb_home_dir());
}

int run_last_line(const KbArgv *a, char *why, size_t n)
{
	char err[4096];
	int rc = kb_run_feed_env(a, NULL, 0, "", 0, err, sizeof(err));
	char *end, *line;

	if (rc == 0)
		return 0;
	end = err + strlen(err);
	while (end > err && (end[-1] == '\n' || end[-1] == '\r' ||
			     end[-1] == ' '))
		*--end = '\0';
	line = strrchr(err, '\n');
	line = line ? line + 1 : err;
	/* podman's own prefix, and a KDOS program's name before its colon. */
	if (!strncmp(line, "Error: ", 7))
		line += 7;
	else if (!strncmp(line, "kdos-box: ", 10))
		line += 10;
	else if (!strncmp(line, "kdos-appbox: ", 13))
		line += 13;
	if (*line)
		kb_strlcpy(why, line, n);
	else
		snprintf(why, n, "%s exited %d", a->v[0], rc);
	return rc;
}
