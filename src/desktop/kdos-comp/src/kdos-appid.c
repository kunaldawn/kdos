// SPDX-License-Identifier: GPL-2.0-only
/*
 * kdos-appid.c — the app_id ledger `kdos appid` checks against.
 *
 * A shell matches a running window to its launcher by the desktop entry's FILE
 * ID, and when the two differ by so much as a capital letter the launcher and
 * the window become two icons instead of one. `kdos appid` reports that, and
 * what makes it a MEASUREMENT rather than a guess is this file: the right-hand
 * side of the comparison is the set of app_ids that real windows have actually
 * presented, recorded by the compositor that saw them.
 *
 * A checker built on `StartupWMClass` would confidently bless the broken case
 * — GIMP's entry declares `gimp-3.0` while its toplevel calls
 * `set_app_id("gimp")`, and nothing short of WAYLAND_DEBUG=1 says so.
 *
 * THE RECORD IS WRITTEN AT MAP, not when an app_id is set. A client may name
 * itself and then never present a window; that is not an observation. The
 * later hook covers the other order — a client that maps first and names
 * itself afterwards — and is gated on the view being mapped for the same
 * reason.
 *
 * IT IS APPEND-ONLY AND DEDUPLICATED IN MEMORY. The set is small (the number
 * of distinct applications a person runs), the file is read once on the first
 * observation of a session, and a name already in it costs a string compare.
 * Nothing here may cost a frame: `view_impl_map()` is on the path that puts a
 * window on the screen.
 */

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "common/mem.h"
#include "kdos.h"
#include "view.h"

#include <wlr/util/log.h>

/* A ceiling so a client that renames itself in a loop cannot grow this without
 * bound. Far above the number of distinct applications an install has. */
#define APPID_MAX 512

static char *seen[APPID_MAX];
static int nseen;
static bool loaded;
static char ledger[512];

/* $HOME is the only place this can live: the compositor runs as the user and
 * the file is the user's own history. No path is ever taken from a client. */
static bool
ledger_path(void)
{
	const char *home = getenv("HOME");

	if (ledger[0]) {
		return true;
	}
	if (!home || !*home) {
		return false;
	}
	snprintf(ledger, sizeof(ledger), "%s/.local/share/kdos", home);
	if (mkdir(ledger, 0700) != 0 && errno != EEXIST) {
		ledger[0] = '\0';
		return false;
	}
	snprintf(ledger, sizeof(ledger), "%s/.local/share/kdos/observed-app-ids",
		home);
	return true;
}

static bool
known(const char *app_id)
{
	for (int i = 0; i < nseen; i++) {
		if (!strcmp(seen[i], app_id)) {
			return true;
		}
	}
	return false;
}

/*
 * The file as it stands, so a name recorded in an earlier session is not
 * appended again. A file that cannot be read reads as EMPTY rather than as an
 * error: the worst that costs is a duplicate line, and `kdos appid` folds
 * duplicates. Refusing to record because the history could not be read would
 * lose the observation instead.
 */
static void
load(void)
{
	char line[256];
	FILE *f;

	loaded = true;
	if (!(f = fopen(ledger, "r"))) {
		return;
	}
	while (nseen < APPID_MAX && fgets(line, sizeof(line), f)) {
		size_t n = strlen(line);

		while (n && (line[n - 1] == '\n' || line[n - 1] == '\r')) {
			line[--n] = '\0';
		}
		if (n && !known(line)) {
			seen[nseen++] = xstrdup(line);
		}
	}
	fclose(f);
}

void
kdos_appid_observe(const char *app_id)
{
	FILE *f;

	/*
	 * An empty app_id is the absence of one, not a name. Newlines and
	 * control bytes are refused rather than escaped: the file is one name
	 * per line and a name carrying a newline would be read back as two.
	 */
	if (!app_id || !*app_id) {
		return;
	}
	for (const char *p = app_id; *p; p++) {
		if ((unsigned char)*p < 0x20) {
			return;
		}
	}
	if (!ledger_path()) {
		return;
	}
	if (!loaded) {
		load();
	}
	if (known(app_id) || nseen >= APPID_MAX) {
		return;
	}
	seen[nseen++] = xstrdup(app_id);

	if (!(f = fopen(ledger, "a"))) {
		wlr_log(WLR_INFO, "kdos: cannot record app_id in %s", ledger);
		return;
	}
	fprintf(f, "%s\n", app_id);
	fclose(f);
}

void
kdos_appid_observe_view(struct view *view)
{
	if (view && view->mapped) {
		kdos_appid_observe(view->app_id);
	}
}
