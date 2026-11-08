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
 * A box's colour is its security domain: one KWin window rule per box.
 *
 * WHY PER BOX AND NOT PER APPLICATION. KWin gives every window of a tagged
 * client the security context's app id — the box's launcher id — as its
 * resourceClass unless the window's own app_id starts with it, so a rule
 * keyed on an application's upstream class matches no boxed window at all.
 * Keyed on the box, one rule covers every window of every launcher in it:
 * the class is `<box>` or `<box>.<stem>`, which the anchored expression
 * below matches and nothing else does.
 *
 * WHAT IT SETS is `decocolor`, forced: the title bar of a server-side
 * decorated window takes the colour scheme KDOS<Accent> of the box's accent,
 * or KDOSBox for a box with none. A client-side decorated window draws its
 * own title bar and takes no decoration colour; KWin 6.8 has no window rule
 * that outlines one instead.
 *
 * THE FILE IS KWin's AND THE USER'S. Only groups named `kdos-box-<box>` are
 * this program's: they are dropped and written again whole, every other group
 * and key is kept exactly as it was, and the [General] order lists keep the
 * user's rules where they were with this program's after them. KWin's own
 * reader deletes every group a legacy `rules=` list does not name, so where
 * that list is still present it is kept in step too.
 */

#include "kdos-appbox.h"
#include "kcolor.h"
#include "kxdg.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define GROUP_PREFIX "kdos-box-"

void rules_scheme(const char *accent, char *out, size_t n)
{
	const KcolScheme *sc = accent && *accent ? kcol_find(accent) : NULL;

	/* The scheme names kdos-theme emits are KDOS plus the title after the
	 * theme name's `KDOS-`; derived here from the same libkcolor row so
	 * the two cannot disagree. */
	if (sc && !strncmp(sc->theme_name, "KDOS-", 5) && sc->theme_name[5])
		snprintf(out, n, "KDOS%s", sc->theme_name + 5);
	else
		snprintf(out, n, "KDOSBox");
}

/* A KConfig value: a backslash is written as two. */
static void kc_value(KbBuf *b, const char *v)
{
	for (; *v; v++) {
		if (*v == '\\')
			kb_buf_str(b, "\\\\");
		else
			kb_buf_add(b, v, 1);
	}
}

/* `^<box>(\.[^.]+)?$`, with the box's own dots escaped. */
static void class_pattern(KbBuf *b, const char *box)
{
	KbBuf re = {0};

	kb_buf_str(&re, "^");
	for (const char *c = box; *c; c++) {
		if (*c == '.')
			kb_buf_str(&re, "\\.");
		else
			kb_buf_add(&re, c, 1);
	}
	kb_buf_str(&re, "(\\.[^.]+)?$");
	kc_value(b, re.p);
	kb_buf_free(&re);
}

static int is_ours(const char *group)
{
	return !strncmp(group, GROUP_PREFIX, sizeof(GROUP_PREFIX) - 1);
}

/*
 * A KConfig string list, rewritten: every entry that is not one of this
 * program's kept in its place, ours appended. Entries are split on commas a
 * backslash does not escape, and written back exactly as they were read.
 */
static void rewrite_list(KbBuf *out, const char *key, const char *value,
			 const char *const *boxes, int n)
{
	int first = 1;
	const char *s = value;

	kb_buf_printf(out, "%s=", key);
	while (s && *s) {
		const char *e = s;
		size_t len;

		while (*e && *e != ',') {
			if (*e == '\\' && e[1])
				e++;
			e++;
		}
		len = (size_t)(e - s);
		if (len && !(len >= sizeof(GROUP_PREFIX) - 1 &&
			     !strncmp(s, GROUP_PREFIX, sizeof(GROUP_PREFIX) - 1))) {
			if (!first)
				kb_buf_str(out, ",");
			kb_buf_add(out, s, len);
			first = 0;
		}
		s = *e ? e + 1 : e;
	}
	for (int i = 0; i < n; i++) {
		kb_buf_printf(out, "%s%s%s", first ? "" : ",", GROUP_PREFIX,
			      boxes[i]);
		first = 0;
	}
	kb_buf_str(out, "\n");
}

static void kwinrules_path(char *out, size_t n)
{
	const char *xdg = getenv("XDG_CONFIG_HOME");

	if (xdg && *xdg == '/')
		snprintf(out, n, "%s/kwinrulesrc", xdg);
	else
		snprintf(out, n, "%s/.config/kwinrulesrc", kb_home_dir());
}

int rules_write(const char *path, const char *const *boxes,
		const char *const *accents, int n)
{
	char def[MAX_LINE];
	size_t len = 0;
	char *text;
	KbBuf out = {0};
	int in_general = 0, skip = 0, saw_general = 0, saw_order = 0;

	if (!path) {
		kwinrules_path(def, sizeof(def));
		path = def;
	}
	text = kb_read_all(path, &len);
	/* A user with no boxes and no rules file gets no file: KWin reads an
	 * absent one as no rules, and an empty one would be clutter. */
	if (!text && n == 0)
		return 0;

	for (char *line = text, *nl; line && *line; line = nl) {
		char *t = line;

		nl = strchr(line, '\n');
		if (nl)
			*nl++ = '\0';
		while (*t == ' ' || *t == '\t')
			t++;
		if (*t == '[') {
			char name[256];
			const char *end = strchr(t, ']');
			size_t gl = end ? (size_t)(end - t - 1) : 0;

			if (in_general && !saw_order)
				rewrite_list(&out, "Order", "", boxes, n);
			in_general = 0;
			snprintf(name, sizeof(name), "%.*s",
				 (int)(gl < sizeof(name) ? gl : sizeof(name) - 1),
				 t + 1);
			/* A nested group `[a][b]` belongs to `a`. */
			skip = is_ours(name);
			if (!strcmp(name, "General") && end && end[1] == '\0') {
				in_general = saw_general = 1;
				saw_order = 0;
			}
			if (!skip)
				kb_buf_printf(&out, "%s\n", line);
			continue;
		}
		if (skip)
			continue;
		if (in_general) {
			char key[64];
			const char *eq = strchr(t, '=');
			size_t kl = eq ? (size_t)(eq - t) : 0;

			while (kl && (t[kl - 1] == ' ' || t[kl - 1] == '\t'))
				kl--;
			snprintf(key, sizeof(key), "%.*s",
				 (int)(kl < sizeof(key) ? kl : sizeof(key) - 1), t);
			if (eq && (!strcmp(key, "Order") || !strcmp(key, "rules"))) {
				const char *v = eq + 1;

				while (*v == ' ' || *v == '\t')
					v++;
				rewrite_list(&out, key, v, boxes, n);
				if (!strcmp(key, "Order"))
					saw_order = 1;
				continue;
			}
		}
		kb_buf_printf(&out, "%s\n", line);
	}
	if (in_general && !saw_order)
		rewrite_list(&out, "Order", "", boxes, n);
	free(text);

	if (!saw_general) {
		KbBuf head = {0};

		kb_buf_str(&head, "[General]\n");
		rewrite_list(&head, "Order", "", boxes, n);
		kb_buf_str(&head, "\n");
		if (out.p)
			kb_buf_add(&head, out.p, out.n);
		kb_buf_free(&out);
		out = head;
	}

	for (int i = 0; i < n; i++) {
		char scheme[64];

		rules_scheme(accents ? accents[i] : NULL, scheme, sizeof(scheme));
		if (out.n && out.p[out.n - 1] != '\n')
			kb_buf_str(&out, "\n");
		kb_buf_printf(&out,
			      "\n[%s%s]\n"
			      "Description=KDOS box %s\n"
			      "decocolor=%s\n"
			      "decocolorrule=2\n"
			      "wmclass=", GROUP_PREFIX, boxes[i], boxes[i],
			      scheme);
		class_pattern(&out, boxes[i]);
		kb_buf_str(&out, "\nwmclasscomplete=false\nwmclassmatch=3\n");
	}

	{
		char dir[MAX_LINE];
		char *slash;

		snprintf(dir, sizeof(dir), "%s", path);
		slash = strrchr(dir, '/');
		if (slash) {
			*slash = '\0';
			kb_mkdir_p(dir);
		}
	}
	if (kb_write_file_atomic(path, out.p ? out.p : "") != 0) {
		kb_buf_free(&out);
		return -1;
	}
	kb_buf_free(&out);
	return n;
}

/*
 * THE BOXES A USER HAS: every box a generated launcher names, and every box
 * with a profile, by name and accent. Rewritten whenever either changes —
 * genlaunchers, and `kdos-box profile` setting an accent.
 */
int box_rules_refresh(void)
{
	char apps[MAX_LINE], pdir[MAX_LINE];
	const char *xdg = getenv("XDG_DATA_HOME");
	char **names;
	char (*boxes)[64] = kb_calloc(256, 64);
	char (*accents)[16] = kb_calloc(256, 16);
	const char *bp[256], *ap[256];
	int n = 0, rc;

	if (xdg && *xdg)
		snprintf(apps, sizeof(apps), "%s/applications", xdg);
	else
		snprintf(apps, sizeof(apps), "%s/.local/share/applications",
			 kb_home_dir());
	snprintf(pdir, sizeof(pdir), "%s/.config/kdos/boxes", kb_home_dir());

#define ADD_BOX(b)                                                            \
	do {                                                                  \
		int seen_ = 0;                                                \
		for (int k_ = 0; k_ < n && !seen_; k_++)                     \
			seen_ = !strcmp(boxes[k_], (b));                      \
		if (!seen_ && n < 256 && box_name_ok(b))                      \
			kb_strlcpy(boxes[n++], (b), 64);                       \
	} while (0)

	names = kb_listdir(apps, NULL);
	for (char **f = names; f && *f; f++) {
		size_t l = strlen(*f);
		char *path;
		KxdgEntry e;

		if (l < 9 || strcmp(*f + l - 8, ".desktop"))
			continue;
		path = kb_path_join(apps, *f);
		if (kxdg_load(&e, path, "Desktop Entry") == 0) {
			const char *b = kxdg_get(&e, "X-KDOS-Box", "");

			if (b && *b)
				ADD_BOX(b);
			kxdg_free(&e);
		}
		free(path);
	}
	kb_strv_free(names);

	names = kb_listdir(pdir, NULL);
	for (char **f = names; f && *f; f++) {
		size_t l = strlen(*f);
		char nm[64];

		if (l < 6 || l - 5 >= sizeof(nm) || strcmp(*f + l - 5, ".conf"))
			continue;
		memcpy(nm, *f, l - 5);
		nm[l - 5] = '\0';
		ADD_BOX(nm);
	}
	kb_strv_free(names);
#undef ADD_BOX

	for (int i = 0; i < n; i++) {
		Profile p;

		profile_load(&p, boxes[i]);
		kb_strlcpy(accents[i], p.accent, 16);
		bp[i] = boxes[i];
		ap[i] = accents[i];
	}
	rc = rules_write(NULL, bp, ap, n);
	free(boxes);
	free(accents);
	if (rc >= 0)
		kwin_reconfigure();
	return rc;
}

/* KWin reads kwinrulesrc once and again on reconfigure, which is what the
 * Window Rules page asks for after it saves. Best effort: with no KWin there
 * is nothing to tell, and the next one reads the file at start. */
void kwin_reconfigure(void)
{
	KbArgv a = {0};

	if (!kb_have_prog("gdbus"))
		return;
	kb_argv_add(&a, "gdbus");
	kb_argv_add(&a, "call");
	kb_argv_add(&a, "--session");
	kb_argv_add(&a, "--timeout");
	kb_argv_add(&a, "3");
	kb_argv_add(&a, "--dest");
	kb_argv_add(&a, "org.kde.KWin");
	kb_argv_add(&a, "--object-path");
	kb_argv_add(&a, "/KWin");
	kb_argv_add(&a, "--method");
	kb_argv_add(&a, "org.kde.KWin.reconfigure");
	kb_argv_end(&a);
	kb_run(&a);
}
