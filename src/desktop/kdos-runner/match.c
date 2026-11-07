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
 *                     finishing it (`accent` lists the eight accents)
 *   firewall          a KDOS System Settings page, by name or by what it is
 *                     for (`previous version` finds System Versions)
 *   setup network     a route of menu.conf, by the words of its name
 *   gimp              a pack or set on the boot medium, opened in Discover
 *                     to install from there
 *   keys tile         the chords whose action matches, as KGlobalAccel has
 *                     them now
 *
 * EVERY ROW IS AN ARGUMENT VECTOR built here, word by word. A word the user
 * typed — a box name, a path for `why`, a search for Discover — is one
 * element of it and nothing else: a `;` or a `$()` in it reaches the program
 * as those characters (run.c execs without a shell).
 *
 * A QUERY BEGINNING `?` IS THE LIBRARY'S (library.c) and this side returns
 * nothing for it.
 * ---------------------------------
 */

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "kcolor.h"
#include "runner.h"

/* ── the verbs ─────────────────────────────────────────────────────────── */

/* What a slot in a verb accepts. */
enum {
	SL_NONE,
	SL_ACCENT,		/* the libkcolor schemes                    */
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
	 * element. */
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
	{ "install", SL_REST, "Find “%s” in Discover", "plasma-discover",
	  RN_RUN_DETACH, { "plasma-discover", "--search", "%s" } },
	{ "why", SL_REST, "Why is %s the way it is?", "help-about",
	  RN_RUN_TERM, { "kdos", "why", "%s" } },
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
	static const char *accents[KCOL_IDX_COUNT + 1];
	const char *const *v = NULL;

	switch (slot) {
	case SL_ACCENT:
		for (int i = 0; i < kcol_nscheme && i < KCOL_IDX_COUNT; i++)
			accents[i] = kcol_schemes[i].name;
		accents[kcol_nscheme < KCOL_IDX_COUNT ? kcol_nscheme :
							 KCOL_IDX_COUNT] = NULL;
		v = accents;
		break;
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

		/* "Amber", not "AMBER": the label is for the boot menu's
		 * capitals, the sentence wants a word. */
		snprintf(out, n, "%s", s ? s->name : value);
		if (*out >= 'a' && *out <= 'z')
			*out = (char)(*out - 'a' + 'A');
		return;
	}
	kb_strlcpy(out, value, n);
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
			slot_shown(v->slot, vals[i], label, sizeof(label));
			add_verb(out, v, vals[i], label, rel);
			if (++shown >= 12)
				break;
		}
	}
}

/* ── KDOS System Settings pages ────────────────────────────────────────── */

typedef struct {
	const char *kcm;
	const char *name;
	const char *icon;
	const char *words;	/* what a person might call it, `|`-separated */
} Page;

static const Page pages[] = {
	{ "kcm_kdos_system", "System Versions", "system-software-update",
	  "system versions|previous version|rollback|roll back|slots|"
	  "update channel|boot previous" },
	{ "kcm_kdos_boxes", "Boxes", "preferences-system-privacy",
	  "boxes|box permissions|sandbox|containers" },
	{ "kcm_kdos_firewall", "Firewall", "security-high",
	  "firewall|open a port|incoming connections" },
	{ "kcm_kdos_look", "KDOS Look", "preferences-desktop-theme-global",
	  "kdos look|phosphor mode|accent colour|accent color|crt" },
	{ "kcm_kdos_health", "Health", "tools-report-bug",
	  "health|system health|problems" },
};

static void match_pages(const RnQuery *q, RnList *out)
{
	for (size_t i = 0; i < sizeof(pages) / sizeof(pages[0]); i++) {
		char alts[256], *save = NULL;
		int best = 0;
		RnMatch *m;

		kb_strlcpy(alts, pages[i].words, sizeof(alts));
		for (char *a = strtok_r(alts, "|", &save); a;
		     a = strtok_r(NULL, "|", &save)) {
			int s = rn_words_match(a, q);

			if (s > best)
				best = s;
		}
		if (!best || !rn_kcm_installed(pages[i].kcm))
			continue;
		if (!(m = rn_add(out)))
			return;
		snprintf(m->text, sizeof(m->text), "%s", pages[i].name);
		kb_strlcpy(m->subtext, "System Settings", sizeof(m->subtext));
		kb_strlcpy(m->icon, pages[i].icon, sizeof(m->icon));
		kb_strlcpy(m->category, "System Settings", sizeof(m->category));
		m->catrel = RN_CAT_HIGH;
		m->rel = best >= 60 ? 0.9 : 0.5 + best / 200.0;
		rn_arg(m, "systemsettings");
		rn_arg(m, pages[i].kcm);
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

static void route_row(RnList *out, const RnRoute *r, double rel, int catrel)
{
	char val[512], *save = NULL;
	RnMatch *m = rn_add(out);

	if (!m)
		return;
	kb_strlcpy(m->text, r->name, sizeof(m->text));
	kb_strlcpy(m->subtext, r->value, sizeof(m->subtext));
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

/* ── install from the boot medium ──────────────────────────────────────── */

/*
 * A pack the boot medium carries, by the words of its id: Discover opens
 * the file itself (--local-filename), which its KDOS backend lists under the
 * `medium` origin, and installing it there copies nothing from a network.
 * `install gimp` asks for it as plainly as `gimp` does.
 */
static void match_medium(const RnQuery *q, RnList *out)
{
	const RnMediumPack *p;
	int n = rn_medium(&p), shown = 0;
	RnQuery rest;
	const RnQuery *use = q;

	if (!n)
		return;
	if (q->nw > 1 && !strcmp(q->w[0], "install")) {
		rn_query(&rest, rn_query_rest(q, 1));
		use = &rest;
	}
	for (int i = 0; i < n && shown < 8; i++) {
		int s = rn_words_match(p[i].id, use);
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
	if (!q.nw || q.raw[0] == '?')
		return;
	match_codes(&q, out);
	match_routes(&q, out);
	if (!strncmp(q.low, "route:", 6))
		return;
	for (int i = 0; i < NVERBS; i++)
		match_verb(&verbs[i], &q, out);
	match_pages(&q, out);
	match_medium(&q, out);
	match_chords(&q, out);
	rn_list_sort(out);
}
