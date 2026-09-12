/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   The chord table, read from whichever desktop is running
 *
 * WHICH DESKTOP THIS IS DECIDES WHICH READER RUNS, and `$KDOS_CON` is the
 * discriminator, as it is everywhere else in this tree. `rc.xml` is the
 * COMPOSITOR'S file and must not be parsed on the console; `kdos-con --keys`
 * is the console session's table and must not be run under the compositor. A
 * surface that named the other desktop's chords would be confidently wrong,
 * which is worse than a surface that names none.
 *
 * ONE READER AND ONE WRITER. `kdos-con --keys` prints its table after the
 * `keys.conf` overlay, so the thing that binds the chords is the thing that
 * prints them, and `rc.xml` is read as the compositor loaded it. Nothing here
 * remembers a chord: a second copy of either table is the copy that goes
 * stale.
 *
 * THE LABWC PARSE IS A LINE-ORIENTED SCANNER rather than an XML library,
 * because the subset it reads is tag-regular: a `<keybind key="...">` followed
 * by the `<action name="..."/>` it runs. Comments are stripped FIRST — rc.xml
 * ships a commented-out run-or-raise example with a real `<keybind>` inside
 * it, and advertising a binding the compositor never loaded would be the exact
 * failure this file exists to prevent.
 *
 * THE WORDS AND THE GROUPS ARE THE SURFACE'S. A row carries the chord, the
 * action that names it and the one thing the source knows about what it runs.
 * What it is called on a card and which section it is filed under is the
 * card's — see keys.c.
 * ---------------------------------
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "kbase.h"
#include "chords.h"

/* The session's terminal, declared here rather than by including `shell.h`:
 * that header pulls in Wayland and the chrome band, and this file is compiled
 * and driven where neither exists. One definition still, in shell.c. */
const char *sh_term(void);

static struct sh_chord chords[SH_CHORD_MAX];
static int nchords;
static int loaded;
static int builtin;

static struct sh_chord *chord_add(void)
{
	struct sh_chord *c;

	if (nchords >= SH_CHORD_MAX)
		return NULL;
	c = &chords[nchords++];
	c->chord[0] = c->action[0] = c->detail[0] = '\0';
	return c;
}

/* ── the file ──────────────────────────────────────────────────────────── */

static char *read_all(const char *path, size_t *len)
{
	FILE *f = fopen(path, "rb");
	char *buf;
	long n;

	if (!f)
		return NULL;
	if (fseek(f, 0, SEEK_END) != 0 || (n = ftell(f)) < 0) {
		fclose(f);
		return NULL;
	}
	rewind(f);
	/* A rc.xml is kilobytes; a megabyte of it is not a configuration file
	 * and reading it would be answering a question nobody asked. */
	if (n > 1 << 20) {
		fclose(f);
		return NULL;
	}
	buf = malloc((size_t)n + 1);
	if (!buf) {
		fclose(f);
		return NULL;
	}
	*len = fread(buf, 1, (size_t)n, f);
	buf[*len] = '\0';
	fclose(f);
	return buf;
}

/* Comments out, in place and before anything else looks at the text. rc.xml
 * documents itself with commented-out bindings; every one of them would
 * otherwise be advertised as live. */
static void strip_comments(char *s)
{
	char *p = s;

	while ((p = strstr(p, "<!--"))) {
		char *e = strstr(p + 4, "-->");
		size_t n = e ? (size_t)(e + 3 - p) : strlen(p);
		memset(p, ' ', n);
		p += n;
	}
}

/* &amp; and friends, in place. Attribute values are commands and messages, and
 * an `&amp;&amp;` shown verbatim is a row that misquotes the machine. */
static void unescape(char *s)
{
	static const struct { const char *ent; char ch; } tbl[] = {
		{ "&amp;", '&' }, { "&lt;", '<' }, { "&gt;", '>' },
		{ "&quot;", '"' }, { "&apos;", '\'' },
	};
	char *r = s, *w = s;

	while (*r) {
		size_t i;
		for (i = 0; i < sizeof(tbl) / sizeof(tbl[0]); i++) {
			size_t n = strlen(tbl[i].ent);
			if (!strncmp(r, tbl[i].ent, n)) {
				*w++ = tbl[i].ch;
				r += n;
				break;
			}
		}
		if (i == sizeof(tbl) / sizeof(tbl[0]))
			*w++ = *r++;
	}
	*w = '\0';
}

/* One attribute out of one tag. `beg` points at the '<', `end` past the '>'. */
static int tag_attr(const char *beg, const char *end, const char *name,
		    char *out, size_t n)
{
	char pat[32];
	const char *p;

	snprintf(pat, sizeof(pat), "%s=\"", name);
	for (p = beg; (p = strstr(p, pat)); p++) {
		if (p >= end)
			break;
		/* `name` must be a whole attribute, or `id="x"` matches
		 * `app_id="x"` and the row names the wrong thing. */
		if (p > beg && p[-1] != ' ' && p[-1] != '\t' && p[-1] != '\n')
			continue;
		p += strlen(pat);
		const char *q = strchr(p, '"');
		if (!q || q > end)
			return -1;
		size_t len = (size_t)(q - p);
		if (len >= n)
			len = n - 1;
		memcpy(out, p, len);
		out[len] = '\0';
		unescape(out);
		return 0;
	}
	return -1;
}

/* ── one binding, as a person reads it ─────────────────────────────────── */

/* labwc's modifier letters, expanded. `W-S-comma` is what the file says and
 * `Super+Shift+comma` is what a person can act on. */
static void pretty_key(const char *in, char *out, size_t n)
{
	static const struct { const char *sym; const char *name; } named[] = {
		{ "Return", "Enter" }, { "Escape", "Esc" },
		{ "space", "Space" }, { "comma", "," }, { "period", "." },
		{ "grave", "`" }, { "minus", "-" }, { "equal", "=" },
		{ "slash", "/" }, { "backslash", "\\" },
		{ "XF86AudioRaiseVolume", "Volume Up" },
		{ "XF86AudioLowerVolume", "Volume Down" },
		{ "XF86AudioMute", "Mute" },
		{ "XF86AudioPlay", "Play" }, { "XF86AudioNext", "Next" },
		{ "XF86AudioPrev", "Previous" },
		{ "XF86MonBrightnessUp", "Brightness Up" },
		{ "XF86MonBrightnessDown", "Brightness Down" },
		{ "XF86Display", "Display" },
	};
	char buf[64];
	size_t used = 0;

	snprintf(buf, sizeof(buf), "%s", in);
	out[0] = '\0';

	char *save = NULL;
	for (char *tok = strtok_r(buf, "-", &save); tok;
	     tok = strtok_r(NULL, "-", &save)) {
		const char *word = tok;
		/* A single modifier letter, and only while something follows
		 * it: `W-s` is Super+s and the trailing `s` is the key. */
		if (!tok[1] && save && *save) {
			switch (*tok) {
			case 'W': word = "Super"; break;
			case 'A': word = "Alt"; break;
			case 'C': word = "Ctrl"; break;
			case 'S': word = "Shift"; break;
			default: break;
			}
		}
		if (word == tok) {
			size_t i;
			for (i = 0; i < sizeof(named) / sizeof(named[0]); i++)
				if (!strcmp(tok, named[i].sym)) {
					word = named[i].name;
					break;
				}
		}
		used += (size_t)snprintf(out + used, used < n ? n - used : 0,
					 "%s%s", used ? "+" : "", word);
		if (used >= n)
			break;
	}
}

/*
 * rc.xml IS THE COMPOSITOR'S FILE and names the compositor's terminal. This
 * table is read on both desktops, and on the console `foot` is a Wayland
 * client that cannot run — a row naming it points the reader at a program that
 * will not start. Only the leading word is rewritten, because that word is the
 * program and everything after it is arguments both emulators take alike.
 */
static void retermize(char *cmd, size_t n)
{
	const char *t = sh_term();
	char rest[192];

	if (strncmp(cmd, "foot", 4) != 0)
		return;
	if (cmd[4] != '\0' && cmd[4] != ' ')
		return;
	if (strcmp(t, "foot") == 0)
		return;
	snprintf(rest, sizeof(rest), "%s", cmd + 4);
	snprintf(cmd, n, "%s%s", t, rest);
}

/*
 * THE ONE THING THE SOURCE KNOWS about what a binding does, which is the whole
 * of what a surface has to word it with: the command an Execute runs, the
 * workspace a desktop action names, the edge an edge action moves to, the menu
 * a ShowMenu opens.
 *
 * Chosen BY THE ACTION and not by whichever attribute happens to be present,
 * so a tag carrying two of them still reports the one labwc acts on. An action
 * this list does not name falls back to the first attribute it carried: a row
 * with nothing beside it says less than the file did.
 */
static void act_detail(const char *act, const char *cmd, const char *to,
		       const char *dir, const char *menu, char *out, size_t n)
{
	const char *v;

	if (!strcmp(act, "Execute"))
		v = cmd;
	else if (!strcmp(act, "GoToDesktop") || !strcmp(act, "SendToDesktop"))
		v = to;
	else if (!strcmp(act, "SnapToEdge") || !strcmp(act, "MoveToEdge") ||
		 !strcmp(act, "GrowToEdge"))
		v = dir;
	else if (!strcmp(act, "ShowMenu"))
		v = menu;
	else if (cmd[0])
		v = cmd;
	else if (to[0])
		v = to;
	else if (dir[0])
		v = dir;
	else
		v = menu;

	snprintf(out, n, "%s", v);
}

/* ── the compositor's table ────────────────────────────────────────────── */

static int parse_rc(const char *path)
{
	size_t len = 0;
	char *buf = read_all(path, &len);
	const char *p;

	if (!buf)
		return -1;
	strip_comments(buf);

	p = buf;
	while (nchords < SH_CHORD_MAX && (p = strstr(p, "<keybind"))) {
		const char *tend = strchr(p, '>');
		char key[40] = "", act[48] = "", cmd[96] = "", to[24] = "",
		     dir[24] = "", menu[32] = "", need[64] = "";
		if (!tend)
			break;
		if (tag_attr(p, tend, "key", key, sizeof(key)) != 0 || !key[0]) {
			p = tend;
			continue;
		}

		const char *close = strstr(tend, "</keybind>");
		if (!close)
			close = buf + len;

		/*
		 * The FIRST action that actually does something — except that a
		 * container's own branch beats it. `If` and `ForEach` are
		 * containers: labwc runs the actions inside their <then>/<none>,
		 * so reporting one would tell the user that Super+Escape "ifs",
		 * which is not a thing a key can do. And the actions BEFORE a
		 * container are its preamble: W-Escape saves the session and
		 * then asks whether to Exit, so a row naming kdos-session-save
		 * would file the key that ends the session under `launch`.
		 */
		const char *q = tend;
		int in_cont = 0;
		while ((q = strstr(q, "<action")) && q < close) {
			const char *qe = strchr(q, '>');
			char nm[48] = "";
			if (!qe || qe > close)
				break;
			if (tag_attr(q, qe, "name", nm, sizeof(nm)) != 0) {
				q = qe;
				continue;
			}
			if (!strcmp(nm, "If") || !strcmp(nm, "ForEach")) {
				in_cont = 1;
				/* The view this ForEach looks for, which for a
				 * run-or-raise row is the program itself. The
				 * <query> is a tag of its own, past this
				 * <action>, so it is read from the block. */
				if (!strcmp(nm, "ForEach")) {
					const char *qr = strstr(qe, "<query");

					if (qr && qr < close) {
						const char *qre = strchr(qr,
									 '>');

						if (qre && qre < close)
							tag_attr(qr, qre,
								 "identifier",
								 need,
								 sizeof(need));
					}
				}
				q = qe;
				continue;
			}
			if (act[0] && !in_cont)
				break;
			snprintf(act, sizeof(act), "%s", nm);
			cmd[0] = to[0] = dir[0] = menu[0] = '\0';
			tag_attr(q, qe, "command", cmd, sizeof(cmd));
			tag_attr(q, qe, "to", to, sizeof(to));
			tag_attr(q, qe, "direction", dir, sizeof(dir));
			tag_attr(q, qe, "menu", menu, sizeof(menu));
			if (in_cont)
				break;
			q = qe;
		}
		if (!act[0]) {
			p = close;
			continue;
		}

		/*
		 * A RUN-OR-RAISE ROW IS THE ONE WHOSE CONTAINER BEGINS WITH
		 * `Focus`, and its <query identifier> is the program it
		 * reaches — the same rule the session's third `--keys` field
		 * carries. A row whose program is not installed is not added,
		 * because a card offering a key that opens nothing teaches the
		 * wrong thing.
		 *
		 * A CONTAINER BEGINNING WITH ANYTHING ELSE QUERIES A MARKER.
		 * The scratchpad's `W-grave` looks for an app id that no
		 * program is called, so gating it on `kb_have_prog` would drop
		 * a key that works.
		 */
		int runraise = need[0] && !strcmp(act, "Focus");

		if (runraise && !kb_have_prog(need)) {
			p = close;
			continue;
		}

		struct sh_chord *c = chord_add();

		if (!c)
			break;
		pretty_key(key, c->chord, sizeof(c->chord));
		if (runraise) {
			/*
			 * NAMED BY THE PROGRAM IT REACHES. The first action
			 * inside the container is `Focus`, which describes the
			 * mechanism rather than the key — a card row reading
			 * `focus` under a chord that opens the file manager
			 * teaches nothing.
			 */
			snprintf(c->needs, sizeof(c->needs), "%s", need);
			snprintf(c->action, sizeof(c->action), "ForEach");
			snprintf(c->detail, sizeof(c->detail), "%s", need);
			p = close;
			continue;
		}
		snprintf(c->action, sizeof(c->action), "%s", act);
		retermize(cmd, sizeof(cmd));
		act_detail(act, cmd, to, dir, menu, c->detail, sizeof(c->detail));
		/* A container that queried a MARKER carries the identifier as
		 * its detail: the app id is the only thing that separates the
		 * scratchpad's key from a plain flag toggle, and an action
		 * name alone would file the two under one description. */
		if (!c->detail[0] && need[0])
			snprintf(c->detail, sizeof(c->detail), "%s", need);
		p = close;
	}
	free(buf);
	return nchords ? 0 : -1;
}

/* ── the console session's table ───────────────────────────────────────── */

/*
 * THE SCRIPTS THAT EXIST, AND THE FIRST TEN KEYS OF EACH.
 *
 * A script is a letter and nothing else on the screen says which letters are
 * taken: `Super+Alt+r` then a letter with no script is a chord that does
 * nothing, and a letter with the wrong script types a paragraph into the wrong
 * window. A card is where a person looks, so the directory is read.
 *
 * READ HERE AND NOT ASKED FOR OVER THE SOCKET. A script is a file in the
 * person's own configuration, this program runs as that person, and the
 * session grew no verb that could be asked — which is the refusal that keeps a
 * client on the surface socket from learning what somebody has recorded.
 */
/* The letters of one script on one row, and then a count of the rest. A row is
 * one line of a surface and not the file: a list allowed to run past this is
 * cut by whatever draws it, and what the cut takes off the end is the count
 * that said there was more. */
#define SCRIPT_ROW_MAX 80

static void scripts_rows(void)
{
	const char *cfg = getenv("XDG_CONFIG_HOME"), *home = getenv("HOME");
	char dir[256];

	if (cfg && *cfg)
		snprintf(dir, sizeof(dir), "%s/kdos-con/scripts", cfg);
	else if (home && *home)
		snprintf(dir, sizeof(dir), "%s/.config/kdos-con/scripts", home);
	else
		return;

	for (int c = 'a'; c <= 'z'; c++) {
		char path[300], names[SCRIPT_ROW_MAX], *text, *line, *nl;
		size_t used = 0;
		int shown = 0, total = 0;
		struct sh_chord *ch;

		snprintf(path, sizeof(path), "%s/%c", dir, c);
		text = kb_read_whole(path, NULL);
		if (!text)
			continue;

		names[0] = '\0';
		for (line = text; line && *line; line = nl) {
			char *tab;

			nl = strchr(line, '\n');
			if (nl)
				*nl++ = '\0';
			if (*line == '#' || !*line)
				continue;
			total++;
			/* The name column, which is the chord spelled the way
			 * keys.conf spells it. Ten of them and then a count:
			 * the row is one line on a card, not the script. */
			tab = strchr(line, '\t');
			if (tab)
				*tab = '\0';
			if (shown < 10) {
				used += (size_t)snprintf(names + used,
							 sizeof(names) - used,
							 "%s%s",
							 shown ? " " : "", line);
				shown++;
				if (used >= sizeof(names) - 8) {
					shown = 10;
					used = sizeof(names) - 8;
				}
			}
		}
		free(text);
		if (!total)
			continue;
		if (total > shown)
			snprintf(names + used, sizeof(names) - used, " +%d",
				 total - shown);
		ch = chord_add();
		if (!ch)
			return;
		snprintf(ch->chord, sizeof(ch->chord), "Super+Alt+r %c", c);
		snprintf(ch->action, sizeof(ch->action), "script");
		snprintf(ch->detail, sizeof(ch->detail), "%s", names);
	}
}

static int parse_con_keys(void)
{
	char buf[4096];
	KbArgv a = { 0 };

	kb_argv_add(&a, "kdos-con");
	kb_argv_add(&a, "--keys");
	kb_argv_end(&a);
	if (kb_run_capture(&a, buf, sizeof(buf)) != 0 || !buf[0])
		return -1;

	for (char *line = strtok(buf, "\n"); line; line = strtok(NULL, "\n")) {
		char *tab = strchr(line, '\t');
		char *needs;
		struct sh_chord *c;

		if (!tab)
			continue;
		*tab = '\0';
		/* The optional third field: the program a run-or-raise row
		 * cannot work without. See keys_print(). */
		needs = strchr(tab + 1, '\t');
		if (needs)
			*needs++ = '\0';
		if (needs && *needs && !kb_have_prog(needs))
			continue;
		c = chord_add();
		if (!c)
			break;
		snprintf(c->action, sizeof(c->action), "%s", line);
		snprintf(c->chord, sizeof(c->chord), "%s", tab + 1);
		if (needs) {
			snprintf(c->needs, sizeof(c->needs), "%s", needs);
			snprintf(c->detail, sizeof(c->detail), "%s", needs);
		}
	}

	/*
	 * THE WORKSPACE CHORDS ARE NOT IN THE TABLE. The session answers a
	 * digit directly rather than binding nine actions, so they have no
	 * line to print and a surface would otherwise show a desktop with no
	 * workspaces at all.
	 */
	if (nchords && nchords + 2 <= SH_CHORD_MAX) {
		struct sh_chord *c = chord_add();

		snprintf(c->chord, sizeof(c->chord), "Super+1..9");
		snprintf(c->action, sizeof(c->action), "workspace-digit");
		c = chord_add();
		snprintf(c->chord, sizeof(c->chord), "Super+Shift+1..9");
		snprintf(c->action, sizeof(c->action), "workspace-send-digit");
	}

	scripts_rows();

	return nchords ? 0 : -1;
}

/* ── the defaults ──────────────────────────────────────────────────────── */

/* What neither desktop could be used without, and nothing else. Deliberately
 * short: a second copy of rc.xml here would itself go stale, and these rows
 * are only ever seen when the real table could not be read. */
static void builtin_table(void)
{
	static const struct { const char *chord, *act; } tbl[] = {
		{ "Super+d",		"launcher" },
		{ "Super+Enter",	"terminal" },
		{ "Alt+F2",		"run" },
		{ "Super+Space",	"menu" },
		{ "Super+q",		"close" },
		{ "Super+Tab",		"next" },
		{ "Super+m",		"maximise" },
		{ "Super+f",		"fullscreen" },
		{ "Super+n",		"minimise" },
		{ "Super+1..4",		"workspace-1-4" },
		{ "Super+l",		"lock" },
		{ "Super+F1",		"keys" },
		/* The one default the two desktops do not share: the console
		 * puts Shift on it, so the chord that closes a window and the
		 * chord that ends the desktop are not one slip apart. */
		{ NULL,			"quit" },
	};

	const char *con = getenv("KDOS_CON");

	nchords = 0;
	for (size_t i = 0; i < sizeof(tbl) / sizeof(tbl[0]); i++) {
		struct sh_chord *c = chord_add();

		if (!c)
			return;
		snprintf(c->chord, sizeof(c->chord), "%s",
			 tbl[i].chord ? tbl[i].chord
				      : (con && *con) ? "Super+Shift+q"
						      : "Super+Escape");
		snprintf(c->action, sizeof(c->action), "%s", tbl[i].act);
	}
}

/* ── loading ───────────────────────────────────────────────────────────── */

static int rc_path(char *buf, size_t n)
{
	const char *cfg = getenv("XDG_CONFIG_HOME");
	const char *home = getenv("HOME");

	if (cfg && *cfg)
		snprintf(buf, n, "%s/kdos-comp/rc.xml", cfg);
	else if (home && *home)
		snprintf(buf, n, "%s/.config/kdos-comp/rc.xml", home);
	else
		return -1;
	return 0;
}

int sh_chords_load(void)
{
	char path[512];
	const char *con;

	if (loaded)
		return nchords;
	loaded = 1;

	con = getenv("KDOS_CON");
	if (con && *con) {
		if (parse_con_keys() == 0)
			return nchords;
	} else if ((rc_path(path, sizeof(path)) == 0 && parse_rc(path) == 0) ||
		   /* /etc/skel's copy is what a home with no rc.xml is
		    * running: the compositor reads its own default from the
		    * same file the user's would have been copied from. */
		   parse_rc("/etc/skel/.config/kdos-comp/rc.xml") == 0) {
		return nchords;
	}

	builtin_table();
	builtin = 1;
	return nchords;
}

/*
 * THE BUILT-IN TABLE, ASKED FOR RATHER THAN FALLEN BACK TO.
 *
 * A reader can succeed and still leave a consumer with nothing it can use: the
 * key card words its rows from a table of its own, and a chord table whose
 * every action that table has no words for leaves the card empty. Empty is the
 * one thing the card must not be silently — "a help surface that silently
 * comes up empty is worse than one that admits it could not read its own
 * configuration" — and only the consumer can know it happened, because only
 * the consumer has the words.
 *
 * So the decision is the consumer's and the table is still this file's. After
 * this, sh_chords_builtin() answers yes, which is what puts the note on screen.
 */
int sh_chords_use_builtin(void)
{
	nchords = 0;
	builtin_table();
	builtin = 1;
	loaded = 1;
	return nchords;
}

int sh_chords_count(void)
{
	return nchords;
}

const struct sh_chord *sh_chord_at(int i)
{
	if (i < 0 || i >= nchords)
		return NULL;
	return &chords[i];
}

int sh_chords_builtin(void)
{
	return builtin;
}
