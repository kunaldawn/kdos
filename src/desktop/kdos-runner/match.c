/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   match.c — the KDOS command palette in KRunner
 *
 * What one query can become, highest first:
 *
 *   ff                two letters a favourite carries (`code=FF`) open it
 *   route:setup.wifi  that route, exactly; `kdos menu` asks for one this way
 *   accent amber      a kdos verb with its arguments filled in from the
 *   box stop www      words typed; a verb typed in part offers each way of
 *                     finishing it (`accent` lists `next` and `prev`,
 *                     then the accents in the boot menu's order; KRunner
 *                     shows eight rows of a runner, so the last two
 *                     accents need `accent <letters>`)
 *   lib photosynth    KRunner opened again with the offline library alone
 *   setup network     a route of menu.conf, by the words of its name, shown
 *                     as `Set up network`
 *   install gimp      a search in Discover, and a pack or set on the boot
 *                     medium, opened in Discover to install from there;
 *                     where the installer's entry is installed (the live
 *                     medium), `install kdos` and each prefix of it answer
 *                     nothing, so that entry, Install KDOS, is the row
 *                     Enter starts
 *   keys tile         the chords whose action matches, as KGlobalAccel has
 *                     them now
 *
 *   KDOS Look         a KDOS System Settings page, by its whole title
 *                     only, opened in System Settings
 *
 * A KDOS settings page answers here only to its whole title. Otherwise
 * System Settings' runner finds it, by its name and the X-KDE-Keywords of
 * its metadata, and a row from each would be the same page twice. The
 * whole title is the exception because krunnerrc ranks Applications above
 * System Settings and the Applications runner forgives a letter in each
 * word: without a palette row, `KDOS Look` + Enter opens The KDOS Book.
 *
 * THE PALETTE RANKS ABOVE APPLICATIONS only because krunnerrc names `kdos`
 * first among KRunner's favourite runners (/usr/share/kdos/xdg/krunnerrc);
 * a category's relevance never lifts it over a favourite's. That is also
 * why a medium pack answers only after `install`: its row would otherwise
 * sit above the installed application of the same name; and why `install
 * kdos` answers nothing while the installer's entry is there, or Enter
 * would search Discover for the installer.
 *
 * EVERY ROW IS AN ARGUMENT VECTOR built here, word by word. A word the user
 * typed — a box name, a path for `why`, a search for Discover — is one
 * element of it and nothing else: a `;` or a `$()` in it reaches the program
 * as those characters (run.c execs without a shell).
 * ---------------------------------
 */

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>

#include "kcolor.h"
#include "runner.h"

/* ── the verbs ─────────────────────────────────────────────────────────── */

/* What a slot in a verb accepts. */
enum {
	SL_NONE,
	SL_ACCENT,		/* next, prev, then the libkcolor schemes   */
	SL_ONOFF,		/* on, off                                  */
	SL_DESK,		/* kdos, dock, tiling                       */
	SL_BOX,			/* a box with a profile                     */
	SL_REST,		/* everything after the verb, as typed      */
};

typedef struct {
	const char *words;	/* the literal words, space-separated       */
	int slot;		/* SL_*; the slot follows the words         */
	const char *text;	/* %s is the slot's value                   */
	const char *icon;
	int how;		/* RN_RUN_*                                 */
	/* The vector; "%s" is replaced by the slot's value, whole, as one
	 * element. A RN_RUN_REQUERY verb's is the runner id and the query. */
	const char *argv[8];
} Verb;

static const Verb verbs[] = {
	{ "accent", SL_ACCENT, "Switch the accent to %s",
	  "preferences-desktop-color", RN_RUN_DETACH,
	  { "kdos", "theme", "%s" } },
	{ "theme", SL_ACCENT, "Switch the accent to %s",
	  "preferences-desktop-color", RN_RUN_DETACH,
	  { "kdos", "theme", "%s" } },
	{ "phosphor", SL_ONOFF, "Phosphor mode %s", "preferences-desktop-effects",
	  RN_RUN_DETACH, { "kdos", "phosphor", "%s" } },
	{ "tiling", SL_ONOFF, "Automatic tiling %s", "window-duplicate",
	  RN_RUN_DETACH, { "kdos", "tiling", "%s" } },
	{ "desk", SL_DESK, "Lay the desktop out as the %s desk",
	  "preferences-desktop-plasma", RN_RUN_DETACH,
	  { "kdos", "desk", "%s" } },
	{ "night light", SL_NONE, "Turn Night Light on or off", "redshift-status-on",
	  RN_RUN_DETACH, { "kdos", "toggle", "night-light" } },
	{ "stay awake", SL_NONE, "Keep the screen awake, or let it sleep again",
	  "system-suspend-inhibited", RN_RUN_DETACH,
	  { "kdos", "toggle", "stay-awake" } },
	{ "reduced motion", SL_NONE, "Reduced motion on or off",
	  "preferences-desktop-accessibility", RN_RUN_DETACH,
	  { "kdos", "toggle", "reduced-motion" } },
	{ "do not disturb", SL_NONE, "Do Not Disturb on or off",
	  "notifications-disabled", RN_RUN_DETACH, { "kdos", "toggle", "dnd" } },
	{ "dnd", SL_NONE, "Do Not Disturb on or off", "notifications-disabled",
	  RN_RUN_DETACH, { "kdos", "toggle", "dnd" } },
	{ "update check", SL_NONE, "Check for a system update", "system-software-update",
	  RN_RUN_TERM, { "kdos", "update", "check" } },
	{ "update", SL_NONE, "Software updates in Discover", "system-software-update",
	  RN_RUN_DETACH, { "plasma-discover", "--mode", "update" } },
	{ "doctor", SL_NONE, "Check this system for problems", "tools-report-bug",
	  RN_RUN_TERM, { "kdos", "doctor" } },
	{ "persist", SL_NONE, "What this live system keeps across a restart",
	  "document-save", RN_RUN_TERM, { "kdos", "persist" } },
	{ "status", SL_NONE, "This system's state at a glance", "dialog-information",
	  RN_RUN_TERM, { "kdos", "status" } },
	{ "restarts", SL_NONE, "Programs still running replaced code",
	  "view-refresh", RN_RUN_TERM, { "kdos", "restarts" } },
	{ "cve", SL_NONE, "Known vulnerabilities in installed packages",
	  "security-medium", RN_RUN_TERM, { "kdos", "cve" } },
	{ "box start", SL_BOX, "Start the box %s", "media-playback-start",
	  RN_RUN_DETACH, { "kdos-box", "start", "%s" } },
	{ "box stop", SL_BOX, "Stop the box %s", "media-playback-stop",
	  RN_RUN_DETACH, { "kdos-box", "stop", "%s" } },
	{ "box restart", SL_BOX, "Restart the box %s", "view-refresh",
	  RN_RUN_DETACH, { "kdos-box", "restart", "%s" } },
	/* `--window` is kdos-box's own Konsole window for the box, in the
	 * box's accent profile and titled with its name, which it opens by
	 * itself only when started from a terminal. */
	{ "box enter", SL_BOX, "A shell in the box %s", "utilities-terminal",
	  RN_RUN_DETACH, { "kdos-box", "enter", "%s", "--window" } },
	{ "install", SL_REST, "Find “%s” in Discover", "plasmadiscover",
	  RN_RUN_DETACH, { "plasma-discover", "--search", "%s" } },
	{ "why", SL_REST, "Why is %s the way it is?", "help-about",
	  RN_RUN_TERM, { "kdos", "why", "%s" } },
	{ "lib", SL_REST, "Search the offline library alone for “%s”",
	  "accessories-dictionary", RN_RUN_REQUERY, { "kdos-library", "%s" } },
};

#define NVERBS ((int)(sizeof(verbs) / sizeof(verbs[0])))

static const char *const onoff[] = { "on", "off", NULL };
static const char *const desks[] = { "kdos", "dock", "tiling", NULL };

static void fill_vector(RnMatch *m, const Verb *v, const char *value)
{
	for (int i = 0; i < 8 && v->argv[i]; i++)
		rn_arg(m, !strcmp(v->argv[i], "%s") ? value : v->argv[i]);
}

static void add_verb(RnList *out, const Verb *v, const char *value,
		     const char *shown, double rel)
{
	RnMatch *m = rn_add(out);

	if (!m)
		return;
	snprintf(m->text, sizeof(m->text), v->text, shown ? shown : "");
	kb_strlcpy(m->icon, v->icon, sizeof(m->icon));
	m->catrel = RN_CAT_HIGH;
	m->rel = rel;
	m->how = v->how;
	m->term_action = v->how == RN_RUN_DETACH && !strcmp(v->argv[0], "kdos");
	fill_vector(m, v, value);
	if (v->how == RN_RUN_REQUERY) {
		kb_strlcpy(m->subtext, "KRunner, with no other runner",
			   sizeof(m->subtext));
		return;
	}
	/* The subtext is the command the row runs, so the palette teaches the
	 * command line it stands in front of. */
	{
		size_t o = 0;

		for (int i = 0; i < m->argc && o + 2 < sizeof(m->subtext); i++) {
			int r = snprintf(m->subtext + o, sizeof(m->subtext) - o,
					 "%s%s", i ? " " : "", m->argv[i]);
			if (r < 0)
				break;
			o += (size_t)r;
			if (o >= sizeof(m->subtext))
				o = sizeof(m->subtext) - 1;
		}
	}
}

/* The candidates of a slot, by kind; `n` is how many. */
static const char *const *slot_values(int slot, int *n)
{
	static const char *accents[KCOL_IDX_COUNT + 3];
	const char *const *v = NULL;

	switch (slot) {
	case SL_ACCENT: {
		int k = 0;

		/* `kdos theme next|prev` step through the schemes in the
		 * order the boot menu shows them. They lead: KRunner shows
		 * only a runner's first eight rows, and after eight accents
		 * the two steps would never be on screen for a bare
		 * `accent`. */
		accents[k++] = "next";
		accents[k++] = "prev";
		for (int i = 0; i < kcol_nscheme && i < KCOL_IDX_COUNT; i++)
			accents[k++] = kcol_schemes[i].name;
		accents[k] = NULL;
		v = accents;
		break;
	}
	case SL_ONOFF:
		v = onoff;
		break;
	case SL_DESK:
		v = desks;
		break;
	case SL_BOX:
		rn_boxes(&v);
		break;
	default:
		break;
	}
	*n = 0;
	while (v && v[*n])
		(*n)++;
	return v;
}

/* How a slot's value reads in a sentence. */
static void slot_shown(int slot, const char *value, char *out, size_t n)
{
	if (slot == SL_ACCENT) {
		const KcolScheme *s = kcol_find(value);

		if (!s && !strcmp(value, "next")) {
			kb_strlcpy(out, "the next one", n);
			return;
		}
		if (!s && !strcmp(value, "prev")) {
			kb_strlcpy(out, "the previous one", n);
			return;
		}

		/* "Amber", not "AMBER": the label is for the boot menu's
		 * capitals, the sentence wants a word. */
		snprintf(out, n, "%s", s ? s->name : value);
		if (*out >= 'a' && *out <= 'z')
			*out = (char)(*out - 'a' + 'A');
		return;
	}
	if (slot == SL_DESK) {
		/* The names the desk picker gives them. */
		snprintf(out, n, "%s", !strcmp(value, "kdos") ? "KDOS"
				       : !strcmp(value, "dock") ? "Dock"
				       : !strcmp(value, "tiling") ? "Tiling"
								  : value);
		return;
	}
	kb_strlcpy(out, value, n);
}

/*
 * `install` followed by `kdos`, or a prefix of it, is the installer's name
 * being typed. The palette ranks above Applications, so a Discover row here
 * would take Enter from the Install KDOS entry. Only the live medium has that
 * entry; elsewhere the words search Discover like any other.
 */
static int is_installer_name(const RnQuery *q, int at)
{
	char path[512];

	return q->nw == at + 1 && rn_prefix("kdos", q->w[at]) &&
	       access(rn_sys("/usr/share/applications/kdos-installer.desktop",
			     path, sizeof(path)), F_OK) == 0;
}

/*
 * One verb against the query. The verb's literal words must be typed in
 * order, each at least begun (`up ch` is `update check`); the word after
 * them, if any, picks among the slot's values by prefix. Nothing typed for
 * the slot offers every value.
 */
static void match_verb(const Verb *v, const RnQuery *q, RnList *out)
{
	char lit[64], *save = NULL;
	const char *words[4];
	int nl = 0, at = 0, exact = 1;

	kb_strlcpy(lit, v->words, sizeof(lit));
	for (char *w = strtok_r(lit, " ", &save); w && nl < 4;
	     w = strtok_r(NULL, " ", &save))
		words[nl++] = w;
	for (int i = 0; i < nl; i++) {
		if (at >= q->nw)
			return;
		if (!rn_prefix(words[i], q->w[at]))
			return;
		if (strlen(q->w[at]) != strlen(words[i]))
			exact = 0;
		at++;
	}

	if (v->slot == SL_NONE) {
		if (at != q->nw)
			return;
		add_verb(out, v, NULL, NULL, exact ? 1.0 : 0.7);
		return;
	}
	if (v->slot == SL_REST) {
		const char *rest = rn_query_rest(q, at);

		/* Only the whole word starts a free-text verb: `in` would
		 * otherwise offer to search Discover for nothing. */
		if (!exact || !*rest)
			return;
		if (!strcmp(v->words, "install") && is_installer_name(q, at))
			return;
		add_verb(out, v, rest, rest, 0.9);
		return;
	}

	{
		int n, shown = 0;
		const char *const *vals = slot_values(v->slot, &n);
		const char *typed = at < q->nw ? q->w[at] : NULL;

		if (at + (typed ? 1 : 0) != q->nw)
			return;
		for (int i = 0; i < n; i++) {
			char label[128];
			double rel;

			if (typed && !rn_prefix(vals[i], typed))
				continue;
			rel = typed ? (strlen(typed) == strlen(vals[i]) ? 1.0 : 0.8)
				    : 0.6;
			if (!exact)
				rel -= 0.1;
			/* Rows of one relevance reach KRunner in no order it
			 * keeps, and it shows only the first few of a runner;
			 * a step per row shown holds the slot's own order:
			 * `next` and `prev`, then the accents as the boot menu
			 * lists them. At most twelve
			 * steps, so no row falls to the tier below its own. */
			rel -= shown * 0.01;
			slot_shown(v->slot, vals[i], label, sizeof(label));
			add_verb(out, v, vals[i], label, rel);
			if (++shown >= 12)
				break;
		}
	}
}

/* ── favourites ────────────────────────────────────────────────────────── */

static void launch_vector(RnMatch *m, const char *id)
{
	rn_arg(m, "kstart");
	rn_arg(m, "--application");
	rn_arg(m, id);
}

static void match_codes(const RnQuery *q, RnList *out)
{
	const RnFavorite *f;
	int n = rn_favorites(&f);

	if (q->nw != 1 || strlen(q->w[0]) != 2)
		return;
	for (int i = 0; i < n; i++) {
		RnMatch *m;

		if (!f[i].code[0] || strcmp(f[i].code, q->w[0]))
			continue;
		if (!(m = rn_add(out)))
			return;
		kb_strlcpy(m->text, f[i].name, sizeof(m->text));
		snprintf(m->subtext, sizeof(m->subtext), "Favourite, code %c%c",
			 toupper((unsigned char)f[i].code[0]),
			 toupper((unsigned char)f[i].code[1]));
		kb_strlcpy(m->icon, f[i].icon, sizeof(m->icon));
		kb_strlcpy(m->category, "Favourites", sizeof(m->category));
		m->catrel = RN_CAT_HIGHEST;
		m->rel = 1.0;
		launch_vector(m, f[i].id);
	}
}

/* ── routes ────────────────────────────────────────────────────────────── */

/* How the verb of a route's name reads at the head of a sentence. A verb not
 * listed reads as itself, capitalised; a user's menu.conf may name any. */
static const struct {
	const char *verb;
	const char *says;
} route_verbs[] = {
	{ "setup", "Set up" },	   { "style", "Style:" },
	{ "system", "System:" },   { "learn", "Learn" },
	{ "capture", "Capture" },  { "share", "Share" },
	{ "toggle", "Toggle" },	   { "about", "About" },
};

/*
 * The row's title, from the name alone: `setup.network` is "Set up
 * network", the noun's dots and underscores read as spaces. The subtext is
 * `route:<name>`, the form a script or `kdos menu summon` uses, so the row
 * teaches the name and not the command it happens to run today.
 */
static void route_title(const char *name, char *out, size_t n)
{
	const char *dot = strchr(name, '.');
	size_t vl = dot ? (size_t)(dot - name) : strlen(name);
	const char *noun = dot ? dot + 1 : "";
	const char *says = NULL;
	size_t o;

	for (size_t i = 0; i < sizeof(route_verbs) / sizeof(route_verbs[0]); i++)
		if (strlen(route_verbs[i].verb) == vl &&
		    !strncmp(route_verbs[i].verb, name, vl))
			says = route_verbs[i].says;
	if (says)
		snprintf(out, n, "%s%s", says, *noun ? " " : "");
	else
		snprintf(out, n, "%.*s%s", (int)vl, name, *noun ? " " : "");
	o = strlen(out);
	for (const char *c = noun; *c && o + 1 < n; c++)
		out[o++] = *c == '.' || *c == '_' ? ' ' : *c;
	out[o] = '\0';
	if (*out >= 'a' && *out <= 'z')
		*out = (char)(*out - 'a' + 'A');
}

static void route_row(RnList *out, const RnRoute *r, double rel, int catrel)
{
	char val[512], *save = NULL;
	RnMatch *m = rn_add(out);

	if (!m)
		return;
	route_title(r->name, m->text, sizeof(m->text));
	snprintf(m->subtext, sizeof(m->subtext), "route:%s", r->name);
	kb_strlcpy(m->icon, "go-jump", sizeof(m->icon));
	kb_strlcpy(m->category, "Routes", sizeof(m->category));
	m->catrel = catrel;
	m->rel = rel;
	/* The value is an argument vector split on spaces with no quoting, as
	 * menu.conf documents: a route that needed a shell could run anything
	 * a menu file said. */
	kb_strlcpy(val, r->value, sizeof(val));
	for (char *w = strtok_r(val, " \t", &save); w;
	     w = strtok_r(NULL, " \t", &save))
		rn_arg(m, w);
	if (!m->argc) {
		rn_match_free(m);
		out->n--;
	}
}

static void match_routes(const RnQuery *q, RnList *out)
{
	const RnRoute *r;
	int n = rn_routes(&r);

	if (q->nw == 1 && !strncmp(q->w[0], "route:", 6)) {
		for (int i = 0; i < n; i++)
			if (!strcmp(r[i].name, q->raw + 6))
				route_row(out, &r[i], 1.0, RN_CAT_HIGHEST);
		return;
	}
	for (int i = 0; i < n; i++) {
		int s = rn_words_match(r[i].name, q);

		if (s)
			route_row(out, &r[i], s >= 60 ? 0.8 : 0.4 + s / 200.0,
				  RN_CAT_MOD);
	}
}

/* ── KDOS's settings pages ─────────────────────────────────────────────── */

/*
 * Every installed kcm_kdos_*.desktop with a Name, the entry by which System
 * Settings opens a page alone, read once: one listing of the applications
 * directory and one parse per page, so a caller holding many titles reads
 * them all against one call rather than one call each. The library reads its
 * rows' titles against this list, so that a row and the page it shares a
 * name with cannot disagree about what a settings title is.
 */
int rn_settings_load(RnSetting *v, int cap)
{
	char dir[512];
	char **files;
	int n = 0;

	files = kb_listdir(rn_sys("/usr/share/applications", dir, sizeof(dir)),
			   NULL);
	for (char **f = files; f && *f && n < cap; f++) {
		size_t l = strlen(*f);

		if (strncmp(*f, "kcm_kdos_", 9) || l <= 9 + 8 ||
		    strcmp(*f + l - 8, ".desktop") || l - 8 >= sizeof(v[n].id))
			continue;
		memcpy(v[n].id, *f, l - 8);
		v[n].id[l - 8] = '\0';
		if (rn_desktop_entry(v[n].id, v[n].name, sizeof(v[n].name),
				     v[n].icon, sizeof(v[n].icon)) &&
		    *v[n].name)
			n++;
	}
	kb_strv_free(files);
	return n;
}

/* The page of V[0..N) whose Name is TEXT, whole and in any case, or NULL. */
const RnSetting *rn_settings_find(const RnSetting *v, int n, const char *text)
{
	for (int i = 0; i < n; i++)
		if (!strcasecmp(v[i].name, text))
			return &v[i];
	return NULL;
}

/*
 * The query, whole, as a settings page's Name. A partial title gives
 * nothing: System Settings' own runner answers it. The library calls this
 * too, to rank its rows under the page's. The row reads `Open <Name>` over
 * `System Settings`: System Settings' own runner lists the page under its
 * bare Name, and two rows of one title read as one answer shown twice.
 */
const RnSetting *rn_settings_title(const RnQuery *q, const RnSetting *v,
				   int n)
{
	return rn_settings_find(v, n, q->raw);
}

static void match_settings(const RnQuery *q, RnList *out)
{
	RnSetting pages[RN_SETTINGS_MAX];
	const RnSetting *p;
	RnMatch *m;

	p = rn_settings_title(q, pages, rn_settings_load(pages, RN_SETTINGS_MAX));
	if (!p || !(m = rn_add(out)))
		return;
	snprintf(m->text, sizeof(m->text), "Open %s", p->name);
	kb_strlcpy(m->subtext, "System Settings", sizeof(m->subtext));
	kb_strlcpy(m->icon, *p->icon ? p->icon : "preferences-system",
		   sizeof(m->icon));
	m->catrel = RN_CAT_HIGHEST;
	m->rel = 1.0;
	rn_arg(m, "systemsettings");
	rn_arg(m, p->id);
}

/* ── install from the boot medium ──────────────────────────────────────── */

/*
 * A pack the boot medium carries, by the words of its id after `install`:
 * Discover opens the file itself (--local-filename), which its KDOS backend
 * lists under the `medium` origin, and installing it there copies nothing
 * from a network. A bare `gimp` does not ask: the palette ranks above
 * Applications, and the pack would sit above the installed GIMP. Nor does
 * `install kdos` where the installer's own entry is installed.
 */
static void match_medium(const RnQuery *q, RnList *out)
{
	const RnMediumPack *p;
	int n = rn_medium(&p), shown = 0;
	RnQuery rest;

	if (!n || q->nw < 2 || strcmp(q->w[0], "install") ||
	    is_installer_name(q, 1))
		return;
	rn_query(&rest, rn_query_rest(q, 1));
	for (int i = 0; i < n && shown < 8; i++) {
		int s = rn_words_match(p[i].id, &rest);
		RnMatch *m;

		if (!s || !(m = rn_add(out)))
			continue;
		snprintf(m->text, sizeof(m->text), "%s (on medium)", p[i].id);
		kb_strlcpy(m->subtext, p[i].set
			   ? "Install this set from the boot medium in Discover"
			   : "Install from the boot medium in Discover",
			   sizeof(m->subtext));
		kb_strlcpy(m->icon, "media-optical", sizeof(m->icon));
		kb_strlcpy(m->category, "Software", sizeof(m->category));
		m->catrel = RN_CAT_MOD;
		m->rel = s >= 60 ? 0.8 : 0.4 + s / 200.0;
		rn_arg(m, "plasma-discover");
		rn_arg(m, "--local-filename");
		rn_arg(m, p[i].path);
		shown++;
	}
}

/* ── chords ────────────────────────────────────────────────────────────── */

static int is_keys_word(const char *w)
{
	return !strcmp(w, "keys") || !strcmp(w, "key") ||
	       !strcmp(w, "shortcut") || !strcmp(w, "shortcuts") ||
	       !strcmp(w, "chord") || !strcmp(w, "chords");
}

static void match_chords(const RnQuery *q, RnList *out)
{
	const RnChord *c;
	RnQuery rest;
	int n, shown = 0;

	if (q->nw < 2 || !is_keys_word(q->w[0]))
		return;
	rn_query(&rest, rn_query_rest(q, 1));
	n = rn_chords(&c);
	for (int i = 0; i < n && shown < 16; i++) {
		int s = rn_words_match(c[i].label, &rest);
		RnMatch *m;

		if (!s)
			s = rn_words_match(c[i].action, &rest);
		if (!s)
			continue;
		if (!(m = rn_add(out)))
			return;
		if (snprintf(m->text, sizeof(m->text), "%s: %s", c[i].label,
			     c[i].keys[0] ? c[i].keys : "no key") >=
		    (int)sizeof(m->text))
			m->text[sizeof(m->text) - 1] = '\0';
		kb_strlcpy(m->subtext, c[i].component, sizeof(m->subtext));
		kb_strlcpy(m->icon, "preferences-desktop-keyboard-shortcut",
			   sizeof(m->icon));
		kb_strlcpy(m->category, "Shortcuts", sizeof(m->category));
		m->catrel = RN_CAT_MOD;
		m->rel = c[i].keys[0] ? 0.5 + s / 400.0 : 0.3;
		if (m->rel > 1.0)
			m->rel = 1.0;
		rn_arg(m, "systemsettings");
		rn_arg(m, "kcm_keys");
		shown++;
	}
}

/* ── the palette ───────────────────────────────────────────────────────── */

void rn_palette_match(const char *text, RnList *out)
{
	RnQuery q;

	rn_query(&q, text);
	if (!q.nw)
		return;
	match_codes(&q, out);
	match_settings(&q, out);
	match_routes(&q, out);
	if (!strncmp(q.low, "route:", 6))
		return;
	for (int i = 0; i < NVERBS; i++)
		match_verb(&verbs[i], &q, out);
	match_medium(&q, out);
	match_chords(&q, out);
	rn_list_sort(out);
}
