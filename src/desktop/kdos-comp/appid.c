/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   observed app_ids — the empirical half of the desktop-id gate
 *
 * Every taskbar-icon bug of the form "my app shows a second grey cog beside
 * its pinned icon" is one seam: a shell matches a running window to a desktop
 * entry by the entry's FILE ID, and the toolkit reports something else. It is
 * endemic — Bitwarden #17760, Godot #96074, Mozilla bug 1782448, Firefox
 * shipping `Firefox-esr` against `firefox-esr.desktop`, a mismatch of one
 * capital letter. KDE's answer is a hand-written window rule per app.
 *
 * The seam exists because the app author, the toolkit, the packager and the
 * shell author are four different people. KDOS is three of them, so it can
 * close it — but only with a MEASURED app_id, never a guessed one. KDOS
 * already paid for that lesson: GIMP's entry says `StartupWMClass=gimp-3.0`
 * while its toplevel calls `set_app_id("gimp")`, and the only way anyone found
 * out was WAYLAND_DEBUG=1.
 *
 * SO THIS FILE JUDGES NOTHING. It records what app_ids actually appeared, and
 * that is all. Comparing them against installed desktop entries is `kdos
 * appid`'s job, one process away, where libkxdg already lives and where being
 * wrong costs a warning rather than a compositor.
 *
 * The file is append-only, deduplicated in memory for the session, and a
 * failure to write it is ignored: a full disk or a read-only home must never
 * cost the user a window.
 */

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "kdos-comp.h"

/*
 * mkdir -p on the file's parent.
 *
 * Not optional and not defensive padding: `~/.local/share/kdos` does not exist
 * on a fresh home, fopen(…, "a") does not create parents, and without this the
 * whole feature silently records nothing at all — which is exactly how it
 * failed the first time it was run. A feature that does nothing quietly is
 * worse than one that is missing.
 */
static void mkdir_parents(const char *file)
{
	char buf[512];
	if (snprintf(buf, sizeof(buf), "%s", file) >= (int)sizeof(buf))
		return;
	char *slash = strrchr(buf, '/');
	if (!slash)
		return;
	*slash = '\0';
	for (char *p = buf + 1; *p; p++) {
		if (*p != '/')
			continue;
		*p = '\0';
		if (mkdir(buf, 0755) < 0 && errno != EEXIST)
			return;
		*p = '/';
	}
	mkdir(buf, 0755);
}

struct kc_seen {
	struct wl_list link;
	char *app_id;
};

static bool observed_path(char *buf, size_t len)
{
	const char *xdg = getenv("XDG_DATA_HOME");
	if (xdg && *xdg)
		return snprintf(buf, len, "%s/kdos/observed-app-ids", xdg) < (int)len;
	const char *home = getenv("HOME");
	if (!home || !*home)
		return false;
	return snprintf(buf, len, "%s/.local/share/kdos/observed-app-ids", home)
	       < (int)len;
}

/*
 * An app_id comes from a client and is therefore hostile input. It ends up in
 * a line-oriented file that another program parses, so the two things that
 * would break that file are refused outright: anything non-printable, and a
 * newline. Length is capped for the same reason a config line is.
 */
static bool app_id_sane(const char *id)
{
	if (!id || !*id || strlen(id) > 128)
		return false;
	for (const char *p = id; *p; p++)
		if ((unsigned char)*p < 0x20 || (unsigned char)*p == 0x7f)
			return false;
	return true;
}

void kc_appid_observe(struct kc_server *s, const char *app_id)
{
	if (!app_id_sane(app_id))
		return;

	struct kc_seen *e;
	wl_list_for_each(e, &s->seen_app_ids, link)
		if (!strcmp(e->app_id, app_id))
			return;		/* already recorded this session */

	e = calloc(1, sizeof(*e));
	if (!e)
		return;
	e->app_id = strdup(app_id);
	if (!e->app_id) {
		free(e);
		return;
	}
	wl_list_insert(&s->seen_app_ids, &e->link);

	/*
	 * Append rather than rewrite. Two sessions can be running (a nested one
	 * for development is the normal case), and "a" on a small line is
	 * atomic enough on every filesystem KDOS ships — whereas a
	 * read-modify-write would have one session's table quietly overwrite the
	 * other's. Duplicates across sessions are the reader's problem, and the
	 * reader has to deduplicate anyway.
	 */
	char path[512];
	if (!observed_path(path, sizeof(path)))
		return;
	mkdir_parents(path);
	FILE *f = fopen(path, "a");
	if (!f)
		return;		/* not worth a line in the log, let alone a failure */
	fprintf(f, "%s\n", app_id);
	fclose(f);
}

void kc_appid_init(struct kc_server *s)
{
	wl_list_init(&s->seen_app_ids);
}

void kc_appid_free(struct kc_server *s)
{
	struct kc_seen *e, *tmp;
	wl_list_for_each_safe(e, tmp, &s->seen_app_ids, link) {
		wl_list_remove(&e->link);
		free(e->app_id);
		free(e);
	}
}
