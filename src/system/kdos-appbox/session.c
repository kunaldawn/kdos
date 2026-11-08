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
 * kdos-appbox session save | restore — the boxes come back with the session.
 *
 *   save     run by plasma-shutdown (plasma-workspace/shutdown/kdos-boxes.sh)
 *            at Log Out, Restart and Shut Down
 *   restore  run at login by the autostart entry kdos-boxes-restore.desktop
 *
 * WHAT KDE ALREADY RESTORES IS LEFT TO KDE. Its fallback session save records
 * the desktop entry of every open window, and a boxed window's entry is its
 * launcher `<launcher-id>.desktop`, whose Exec starts the box; so a boxed
 * application with a launcher comes back through KDE's own restore, and this
 * program skips every launcher KDE's record names. What is left is what KDE
 * cannot see: a box that was running with no window open, and an application
 * that runs in a box under no launcher of its own (`kdos-box run`, an
 * exported entry) — those come back from here.
 *
 * THE RECORD IS THE LAUNCH LEDGER, NOT THE WINDOWS. By the time plasma-shutdown
 * runs this, KWin has closed every window, so the question "what was open" has
 * to have been answered as the applications closed: the keeper marks a launch
 * that ends while ksmserver is logging out, and `save` takes those marks and
 * the launches still running. Restoring follows the login mode KDE's own
 * session restore follows: only "restore the previous session" restores, and
 * an application in ksmserverrc's excludeApps is never started.
 *
 * $XDG_STATE_HOME/kdos/session-boxes, one line each:
 *
 *   box<TAB><name>
 *   app<TAB><box><TAB><launcher-id><TAB><argument>...
 */

#include "kdos-appbox.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

/* A launch closed by the logout counts for this long after it closed: the
 * windows close seconds before the shutdown scripts run, and a mark older
 * than this belongs to a logout that was cancelled. */
#define LOGOUT_GRACE 300
#define RESTORE_MAX  16

static int session_path(char *out, size_t n)
{
	return kb_state_path("kdos/session-boxes", out, n) ? 0 : -1;
}

static int running_boxes(char names[][64], int max)
{
	KbArgv a = {0};
	KbBuf out = {0};
	int n = 0;

	kb_argv_add(&a, "podman");
	kb_argv_add(&a, "ps");
	kb_argv_add(&a, "--filter");
	kb_argv_add(&a, "status=running");
	kb_argv_add(&a, "--format");
	kb_argv_add(&a, "{{.Names}}");
	kb_argv_end(&a);
	if (kb_run_capture_buf(&a, &out) != 0) {
		kb_buf_free(&out);
		return -1;
	}
	for (char *l = out.p, *nl; l && *l && n < max; l = nl) {
		nl = strchr(l, '\n');
		if (nl)
			*nl++ = '\0';
		if (box_name_ok(l))
			kb_strlcpy(names[n++], l, 64);
	}
	kb_buf_free(&out);
	return n;
}

static int session_save(void)
{
	char names[128][64], path[MAX_LINE], sd[MAX_LINE];
	KbBuf rec = {0};
	int nb = running_boxes(names, 128), nbox = 0, napp = 0;
	long now = (long)time(NULL);

	if (session_path(path, sizeof(path)) != 0)
		return 1;
	kb_buf_str(&rec, "# kdos-appbox session save — what the next login "
			 "restores\n");
	if (nb < 0) {
		/* No container engine to ask: write nothing, so a restore
		 * starts nothing it cannot account for. */
		nb = 0;
	}
	if (box_state_dir(sd, sizeof(sd)) != 0)
		sd[0] = '\0';
	for (int i = 0; i < nb; i++) {
		Profile p;
		char lpath[2 * MAX_LINE + 80];
		size_t len = 0;
		char *text;

		profile_load(&p, names[i]);
		/* An ephemeral box is gone with its container by design, and
		 * bringing it back would be bringing back what it was for
		 * forgetting. */
		if (p.persist == PERSIST_EPHEMERAL)
			continue;
		kb_buf_printf(&rec, "box\t%s\n", names[i]);
		nbox++;
		if (!sd[0])
			continue;
		snprintf(lpath, sizeof(lpath), "%s/%s.apps", sd, names[i]);
		text = kb_read_all(lpath, &len);
		for (char *line = text, *nl; line && *line; line = nl) {
			long pid;
			unsigned long long start;
			char *rest, *flag, *sp;
			int keep = 0;

			nl = strchr(line, '\n');
			if (nl)
				*nl++ = '\0';
			pid = strtol(line, &rest, 10);
			start = strtoull(rest, &rest, 10);
			while (*rest == ' ')
				rest++;
			flag = rest;
			sp = strchr(rest, ' ');
			if (!sp)
				continue;
			*sp = '\0';
			rest = sp + 1;
			if (!strcmp(flag, "run"))
				keep = pid > 1 && proc_start_time(pid) == start;
			else if (!strncmp(flag, "logout:", 7))
				keep = now - strtol(flag + 7, NULL, 10) <= LOGOUT_GRACE;
			if (!keep || !*rest)
				continue;
			kb_buf_printf(&rec, "app\t%s\t%s\n", names[i], rest);
			napp++;
		}
		free(text);
	}
	if (kb_write_file_atomic(path, rec.p) != 0) {
		kb_buf_free(&rec);
		kb_warn("cannot write %s", path);
		return 1;
	}
	kb_buf_free(&rec);
	fprintf(stderr, "kdos-appbox: session save: %d box(es), %d application(s)\n",
		nbox, napp);
	return 0;
}

/*
 * ONE KEY OF ONE KCONFIG FILE, with KConfig's cascade: the user's file, then
 * each XDG_CONFIG_DIRS entry, the first that has the key winning. Enough of
 * the format for a [General] scalar, which is all that is read here.
 */
static int kconfig_value(const char *file, const char *group, const char *key,
			 char *out, size_t n)
{
	char dirs[MAX_LINE + 2048], user[MAX_LINE];
	const char *xdg = getenv("XDG_CONFIG_HOME");
	const char *cd = getenv("XDG_CONFIG_DIRS");
	char *save = NULL;

	if (xdg && *xdg == '/')
		snprintf(user, sizeof(user), "%s", xdg);
	else
		snprintf(user, sizeof(user), "%s/.config", kb_home_dir());
	snprintf(dirs, sizeof(dirs), "%s:%s", user,
		 cd && *cd ? cd : "/etc/xdg");
	for (char *d = strtok_r(dirs, ":", &save); d;
	     d = strtok_r(NULL, ":", &save)) {
		char path[MAX_LINE + 64];
		size_t len = 0;
		char *text;
		int in = 0, found = 0;

		snprintf(path, sizeof(path), "%s/%s", d, file);
		text = kb_read_all(path, &len);
		for (char *l = text, *nl; l && *l && !found; l = nl) {
			nl = strchr(l, '\n');
			if (nl)
				*nl++ = '\0';
			if (*l == '[') {
				in = !strncmp(l + 1, group, strlen(group)) &&
				     l[1 + strlen(group)] == ']';
				continue;
			}
			if (in && !strncmp(l, key, strlen(key)) &&
			    (l[strlen(key)] == '=' || l[strlen(key)] == ' ')) {
				const char *v = strchr(l, '=');

				if (!v)
					continue;
				v++;
				while (*v == ' ')
					v++;
				kb_strlcpy(out, v, n);
				found = 1;
			}
		}
		free(text);
		if (found)
			return 1;
	}
	return 0;
}

/* Whether `want` is one entry of a list split on any of `seps`. */
static int in_list_of(const char *list, const char *seps, const char *want)
{
	char *copy = kb_strdup(list), *save = NULL;
	int hit = 0;

	for (char *t = strtok_r(copy, seps, &save); t && !hit;
	     t = strtok_r(NULL, seps, &save)) {
		while (*t == ' ')
			t++;
		hit = !strcmp(t, want);
	}
	free(copy);
	return hit;
}

/* The desktop ids KDE's fallback session restore is about to launch. */
static char *kde_restored(void)
{
	char path[MAX_LINE];
	KbBuf ids = {0};
	size_t len = 0;
	char *text;

	if (!kb_state_path("plasmasessionrestorestaterc", path, sizeof(path)))
		return kb_strdup("");
	text = kb_read_all(path, &len);
	for (char *l = text, *nl; l && *l; l = nl) {
		nl = strchr(l, '\n');
		if (nl)
			*nl++ = '\0';
		if (!strncmp(l, "appId=", 6))
			kb_buf_printf(&ids, "%s\n", l + 6);
	}
	free(text);
	if (!ids.p)
		return kb_strdup("");
	return ids.p;
}

static int session_restore(void)
{
	char path[MAX_LINE], mode[64] = "restorePreviousLogout", exclude[2048] = "";
	char self[MAX_LINE];
	size_t len = 0;
	char *text, *kde;
	ssize_t sl;
	int started = 0, launched = 0;

	kconfig_value("ksmserverrc", "General", "loginMode", mode, sizeof(mode));
	if (strcmp(mode, "restorePreviousLogout")) {
		fprintf(stderr, "kdos-appbox: session restore: loginMode is %s — "
			"nothing restored\n", mode);
		return 0;
	}
	kconfig_value("ksmserverrc", "General", "excludeApps", exclude,
		      sizeof(exclude));
	if (session_path(path, sizeof(path)) != 0)
		return 0;
	text = kb_read_all(path, &len);
	if (!text)
		return 0;
	sl = readlink("/proc/self/exe", self, sizeof(self) - 1);
	if (sl <= 0) {
		free(text);
		return 1;
	}
	self[sl] = '\0';
	kde = kde_restored();

	/* The boxes first, so an application below lands in a box that is
	 * already up rather than paying for its start. */
	for (int pass = 0; pass < 2; pass++) {
		char *copy = kb_strdup(text);

		for (char *l = copy, *nl; l && *l; l = nl) {
			char *f[KB_MAX_ARGV];
			int nf = 0;
			char *save = NULL;

			nl = strchr(l, '\n');
			if (nl)
				*nl++ = '\0';
			if (*l == '#' || !*l)
				continue;
			for (char *t = strtok_r(l, "\t", &save); t && nf < KB_MAX_ARGV - 8;
			     t = strtok_r(NULL, "\t", &save))
				f[nf++] = t;
			/* Started the way the warmup starts a box, keeper
			 * and all, so the first launch finds its bus up. */
			if (pass == 0 && nf == 2 && !strcmp(f[0], "box") &&
			    box_name_ok(f[1])) {
				char why[256];

				if (pack_box_start(f[1]) == 0) {
					started++;
					if (keeper_ensure(f[1], why, sizeof(why)) != 0)
						tracef("restore %s: %s", f[1], why);
				}
				continue;
			}
			if (pass == 1 && nf >= 4 && !strcmp(f[0], "app") &&
			    box_name_ok(f[1]) && box_name_ok(f[2]) &&
			    launched < RESTORE_MAX) {
				char desk[160];
				KbArgv a = {0};

				snprintf(desk, sizeof(desk), "%s.desktop", f[2]);
				if (in_list_of(kde, "\n", desk) ||
				    in_list_of(exclude, ",:", f[2]) ||
				    in_list_of(exclude, ",:", desk))
					continue;
				kb_argv_add(&a, self);
				kb_argv_add(&a, "-b");
				kb_argv_add(&a, f[1]);
				kb_argv_add(&a, "-l");
				kb_argv_add(&a, f[2]);
				kb_argv_add(&a, "run");
				for (int i = 3; i < nf; i++)
					kb_argv_add(&a, f[i]);
				kb_argv_end(&a);
				kb_run_detach(&a);
				launched++;
				/* Staggered: several launches at once contend
				 * for the same boxes' starts. */
				sleep(2);
			}
		}
		free(copy);
	}
	free(kde);
	free(text);
	fprintf(stderr, "kdos-appbox: session restore: %d box(es) started, "
		"%d application(s) launched\n", started, launched);
	return 0;
}

int cmd_session(int argc, char **argv)
{
	if (argc == 1 && !strcmp(argv[0], "save"))
		return session_save();
	if (argc == 1 && !strcmp(argv[0], "restore"))
		return session_restore();
	fprintf(stderr, "usage: kdos-appbox session save|restore\n");
	return 2;
}
