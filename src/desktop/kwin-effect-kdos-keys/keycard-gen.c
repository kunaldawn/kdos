/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   kdos-keycard-gen — the key card's groups, from groups.conf
 *
 *   kdos-keycard-gen <groups.conf> <out.js>
 *   kdos-keycard-gen --check <groups.conf> FILE...
 *
 * The first form compiles the table into the JavaScript library the key
 * card reads: the group order and each rule as a pair of anchored regular
 * expressions. The card matches a live shortcut against them in order, so
 * the order of groups.conf is the order of the output.
 *
 * The second form reads the shortcuts KDOS SHIPS and fails on any that only
 * a fallback rule places. Each FILE is one of
 *
 *   a kglobalshortcutsrc     every [component] and [services][<id>] entry;
 *   a desktop entry          <file name> `_launch`, and each [Desktop Action];
 *   a KWin package's .qml    the `name:` of each ShortcutHandler, as `kwin`;
 *   a KWin package's .js     the first argument of each registerShortcut(),
 *                            as `kwin`.
 *
 * It runs at build time and in testing/selftest.sh, never in a session.
 * Exit 0 clean, 1 an unplaced shortcut, 2 a table or file it cannot read.
 * ---------------------------------
 */

#define _POSIX_C_SOURCE 200809L
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define KG_MAX_RULES	512
#define KG_LINE		1024

/* The groups the card draws, in the order it draws them. A rule naming any
 * other group is a table error: the card would have no heading to put it
 * under and would drop the shortcut. */
static const char *const kg_groups[] = {
	"windows", "desktops", "launch", "kdos", "accessibility", "system",
};
#define KG_NGROUPS (sizeof(kg_groups) / sizeof(kg_groups[0]))

typedef struct {
	char group[32];
	char component[256];
	char action[512];
	int fallback;
	int line;
} KgRule;

static KgRule kg_rules[KG_MAX_RULES];
static int kg_nrules;

static int kg_errors;

static void kg_error(const char *file, int line, const char *msg, const char *what)
{
	if (line > 0)
		fprintf(stderr, "kdos-keycard-gen: %s:%d: %s%s%s\n", file, line, msg,
			what ? ": " : "", what ? what : "");
	else
		fprintf(stderr, "kdos-keycard-gen: %s: %s%s%s\n", file, msg,
			what ? ": " : "", what ? what : "");
	kg_errors++;
}

static char *kg_trim(char *s)
{
	while (*s == ' ' || *s == '\t')
		s++;
	size_t n = strlen(s);

	while (n && (s[n - 1] == ' ' || s[n - 1] == '\t' || s[n - 1] == '\n'
		     || s[n - 1] == '\r'))
		s[--n] = '\0';
	return s;
}

/* One field: a run of non-blank characters, or a double-quoted string that
 * may hold blanks. Returns the rest of the line, or NULL when there is no
 * field or a quote is left open. */
static char *kg_field(char *s, char *out, size_t cap)
{
	s = kg_trim(s);
	if (!*s)
		return NULL;

	size_t n = 0;

	if (*s == '"') {
		s++;
		while (*s && *s != '"') {
			if (n + 1 >= cap)
				return NULL;
			out[n++] = *s++;
		}
		if (*s != '"')
			return NULL;
		s++;
	} else {
		while (*s && *s != ' ' && *s != '\t') {
			if (n + 1 >= cap)
				return NULL;
			out[n++] = *s++;
		}
	}
	out[n] = '\0';
	return n ? s : NULL;
}

static int kg_known_group(const char *g)
{
	for (size_t i = 0; i < KG_NGROUPS; i++)
		if (!strcmp(kg_groups[i], g))
			return 1;
	return 0;
}

static int kg_load(const char *path)
{
	FILE *f = fopen(path, "r");

	if (!f) {
		kg_error(path, 0, "cannot open", NULL);
		return -1;
	}

	char buf[KG_LINE];
	int line = 0, fallback = 0, start = kg_errors;

	while (fgets(buf, sizeof(buf), f)) {
		line++;
		if (!strchr(buf, '\n') && !feof(f)) {
			kg_error(path, line, "line too long", NULL);
			break;
		}

		char *s = kg_trim(buf);

		if (!*s || *s == '#')
			continue;
		if (!strcmp(s, "---")) {
			if (fallback)
				kg_error(path, line, "a second --- line", NULL);
			fallback = 1;
			continue;
		}
		if (kg_nrules == KG_MAX_RULES) {
			kg_error(path, line, "too many rules", NULL);
			break;
		}

		KgRule *r = &kg_rules[kg_nrules];
		char *rest = kg_field(s, r->group, sizeof(r->group));

		if (rest)
			rest = kg_field(rest, r->component, sizeof(r->component));
		if (!rest) {
			kg_error(path, line, "want <group> <component> <action>", NULL);
			continue;
		}
		rest = kg_trim(rest);
		if (!*rest || strlen(rest) >= sizeof(r->action)) {
			kg_error(path, line, "want <group> <component> <action>", NULL);
			continue;
		}
		strcpy(r->action, rest);
		if (!kg_known_group(r->group)) {
			kg_error(path, line, "unknown group", r->group);
			continue;
		}
		r->fallback = fallback;
		r->line = line;
		kg_nrules++;
	}
	fclose(f);
	if (kg_errors == start && !kg_nrules)
		kg_error(path, 0, "no rules", NULL);
	return kg_errors == start ? 0 : -1;
}

/* `*` is any run of characters and `?` any one; everything else is itself.
 * The emitted regular expressions follow the same two rules, so the build's
 * check and the card agree on every name. */
static int kg_match(const char *pat, const char *s)
{
	const char *star = NULL, *mark = NULL;

	while (*s) {
		if (*pat == '*') {
			star = pat++;
			mark = s;
		} else if (*pat == '?' || *pat == *s) {
			pat++;
			s++;
		} else if (star) {
			pat = star + 1;
			s = ++mark;
		} else {
			return 0;
		}
	}
	while (*pat == '*')
		pat++;
	return !*pat;
}

static const KgRule *kg_place(const char *component, const char *action)
{
	for (int i = 0; i < kg_nrules; i++)
		if (kg_match(kg_rules[i].component, component)
		    && kg_match(kg_rules[i].action, action))
			return &kg_rules[i];
	return NULL;
}

/* ── the JavaScript library ───────────────────────────────────────── */

/* A pattern as an anchored regular expression inside a JavaScript string
 * literal: the regex escape and then the string escape, so `.` reaches the
 * engine as `\.` and a quote cannot end the literal. */
static void kg_emit_regex(FILE *o, const char *pat)
{
	fputs("\"^", o);
	for (const unsigned char *p = (const unsigned char *)pat; *p; p++) {
		if (*p == '*')
			fputs(".*", o);
		else if (*p == '?')
			fputc('.', o);
		else if (*p == '\\')
			fputs("\\\\\\\\", o);
		else if (strchr("^$.|+()[]{}/", *p))
			fprintf(o, "\\\\%c", *p);
		else if (*p == '"')
			fputs("\\\"", o);
		else if (*p < 0x20)
			fprintf(o, "\\u%04x", *p);
		else
			fputc(*p, o);
	}
	fputs("$\"", o);
}

static int kg_write_js(const char *path)
{
	char tmp[4096];

	if (snprintf(tmp, sizeof(tmp), "%s.tmp", path) >= (int)sizeof(tmp)) {
		kg_error(path, 0, "path too long", NULL);
		return -1;
	}

	FILE *o = fopen(tmp, "w");

	if (!o) {
		kg_error(tmp, 0, "cannot write", NULL);
		return -1;
	}
	fputs("// Generated by kdos-keycard-gen from groups.conf; edit that file.\n"
	      ".pragma library\n"
	      "\n"
	      "var groups = [", o);
	for (size_t i = 0; i < KG_NGROUPS; i++)
		fprintf(o, "%s\"%s\"", i ? ", " : "", kg_groups[i]);
	fputs("];\n\n// [group, component, action], first match wins.\nvar rules = [\n", o);
	for (int i = 0; i < kg_nrules; i++) {
		fprintf(o, "    [\"%s\", ", kg_rules[i].group);
		kg_emit_regex(o, kg_rules[i].component);
		fputs(", ", o);
		kg_emit_regex(o, kg_rules[i].action);
		fputs("],\n", o);
	}
	fputs("];\n", o);
	if (fclose(o) != 0 || rename(tmp, path) != 0) {
		remove(tmp);
		kg_error(path, 0, "cannot write", NULL);
		return -1;
	}
	return 0;
}

/* ── the check ────────────────────────────────────────────────────── */

static int kg_unplaced;

static void kg_check_one(const char *file, int line, const char *component,
			 const char *action)
{
	const KgRule *r = kg_place(component, action);

	if (r && !r->fallback)
		return;
	fprintf(stderr, "kdos-keycard-gen: %s:%d: [%s] %s is placed by %s\n",
		file, line, component, action,
		r ? "a fallback rule only" : "no rule");
	kg_unplaced++;
}

static int kg_ends_with(const char *s, const char *tail)
{
	size_t n = strlen(s), t = strlen(tail);

	return n >= t && !strcmp(s + n - t, tail);
}

/* A kglobalshortcutsrc. `[kwin]` is the component kwin; `[services][<id>]`
 * is the desktop entry <id>; any other nesting is a shortcut context, which
 * the card does not list. */
static void kg_check_shortcutsrc(const char *path, FILE *f)
{
	char buf[KG_LINE], component[256] = "";
	int line = 0, active = 0;

	while (fgets(buf, sizeof(buf), f)) {
		line++;

		char *s = kg_trim(buf);

		if (!*s || *s == '#')
			continue;
		if (*s == '[') {
			size_t n = strlen(s);

			active = 0;
			if (n < 3 || s[n - 1] != ']') {
				kg_error(path, line, "a group header that does not close", NULL);
				continue;
			}
			s[n - 1] = '\0';
			s++;
			if (!strncmp(s, "services][", 10)) {
				s += 10;
				if (strchr(s, ']') || strchr(s, '['))
					continue;
			} else if (strchr(s, ']') || strchr(s, '[')) {
				continue;
			}
			if (strlen(s) >= sizeof(component)) {
				kg_error(path, line, "component name too long", NULL);
				continue;
			}
			strcpy(component, s);
			active = 1;
			continue;
		}
		if (!active)
			continue;

		char *eq = strchr(s, '=');

		if (!eq) {
			kg_error(path, line, "an entry with no =", NULL);
			continue;
		}
		*eq = '\0';

		char *key = kg_trim(s);
		char *loc = strchr(key, '[');

		if (loc)
			*loc = '\0';
		if (!*key || !strcmp(key, "_k_friendly_name"))
			continue;
		kg_check_one(path, line, component, key);
	}
}

static void kg_check_desktop(const char *path, FILE *f)
{
	const char *base = strrchr(path, '/');
	char buf[KG_LINE];
	int line = 0;

	base = base ? base + 1 : path;
	kg_check_one(path, 1, base, "_launch");
	while (fgets(buf, sizeof(buf), f)) {
		line++;

		char *s = kg_trim(buf);

		if (!strncmp(s, "[Desktop Action ", 16)) {
			size_t n = strlen(s);

			if (n > 17 && s[n - 1] == ']') {
				s[n - 1] = '\0';
				kg_check_one(path, line, base, s + 16);
			}
		}
	}
}

/* The start of an object of QML type `type`, bare or behind an import
 * qualifier — `ShortcutHandler {` and `KWin.ShortcutHandler {` alike. */
static const char *kg_qml_type(const char *s, const char *type)
{
	size_t n = strlen(type);
	const char *p = s;

	while (isalnum((unsigned char)*p) || *p == '_')
		p++;
	if (*p == '.' && p > s)
		s = p + 1;
	if (strncmp(s, type, n))
		return NULL;
	s += n;
	while (*s == ' ' || *s == '\t')
		s++;
	return *s == '{' ? s : NULL;
}

/* The ShortcutHandler blocks of a KWin script or effect: each one's `name:`
 * is an action of the component kwin. The handler is a QML object, so its
 * name is the first `name:` property after the type name. */
static void kg_check_qml(const char *path, FILE *f)
{
	char buf[KG_LINE];
	int line = 0, open = 0;

	while (fgets(buf, sizeof(buf), f)) {
		line++;

		char *s = kg_trim(buf);

		const char *h = kg_qml_type(s, "ShortcutHandler");

		if (h) {
			open = 1;
			continue;
		}
		if (!open || strncmp(s, "name:", 5))
			continue;
		open = 0;

		char *q = strchr(s, '"');
		char *e = q ? strchr(q + 1, '"') : NULL;

		if (!e) {
			kg_error(path, line, "a ShortcutHandler name that is not a string literal", NULL);
			continue;
		}
		*e = '\0';
		kg_check_one(path, line, "kwin", q + 1);
	}
	if (open)
		kg_error(path, line, "a ShortcutHandler with no name", NULL);
}

/* The registerShortcut() calls of a KWin JavaScript script: the first
 * argument, a string literal in either quote on the line of the call, is an
 * action of the component kwin. A name built at run time cannot be checked,
 * so it is an error rather than a pass. A line that opens as a comment is
 * skipped, so prose naming the call is not read as one. */
static void kg_check_js(const char *path, FILE *f)
{
	static const char call[] = "registerShortcut(";
	char buf[KG_LINE];
	int line = 0;

	while (fgets(buf, sizeof(buf), f)) {
		line++;

		char *s = kg_trim(buf);

		if (!strncmp(s, "//", 2) || *s == '*' || !strncmp(s, "/*", 2))
			continue;
		while ((s = strstr(s, call)) != NULL) {
			s += sizeof(call) - 1;
			while (*s == ' ' || *s == '\t')
				s++;

			char quote = *s;
			char *e = (quote == '"' || quote == '\'') ? strchr(s + 1, quote) : NULL;

			if (!e) {
				kg_error(path, line, "a registerShortcut name that is not a string literal", NULL);
				break;
			}
			*e = '\0';
			kg_check_one(path, line, "kwin", s + 1);
			s = e + 1;
		}
	}
}

static void kg_check_file(const char *path)
{
	FILE *f = fopen(path, "r");

	if (!f) {
		kg_error(path, 0, "cannot open", NULL);
		return;
	}
	if (kg_ends_with(path, ".desktop"))
		kg_check_desktop(path, f);
	else if (kg_ends_with(path, ".qml"))
		kg_check_qml(path, f);
	else if (kg_ends_with(path, ".js"))
		kg_check_js(path, f);
	else
		kg_check_shortcutsrc(path, f);
	fclose(f);
}

static void kg_usage(FILE *to)
{
	fputs("usage: kdos-keycard-gen <groups.conf> <out.js>\n"
	      "       kdos-keycard-gen --check <groups.conf> FILE...\n", to);
}

int main(int argc, char **argv)
{
	if (argc >= 2 && (!strcmp(argv[1], "-h") || !strcmp(argv[1], "--help"))) {
		kg_usage(stdout);
		return 0;
	}
	if (argc >= 4 && !strcmp(argv[1], "--check")) {
		if (kg_load(argv[2]) < 0)
			return 2;
		for (int i = 3; i < argc; i++)
			kg_check_file(argv[i]);
		if (kg_errors)
			return 2;
		return kg_unplaced ? 1 : 0;
	}
	if (argc != 3 || argv[1][0] == '-') {
		kg_usage(stderr);
		return 2;
	}
	if (kg_load(argv[1]) < 0 || kg_write_js(argv[2]) < 0)
		return 2;
	return 0;
}
