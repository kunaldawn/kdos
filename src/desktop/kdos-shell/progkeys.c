/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   The focused program's own keys, for the few that publish them
 *
 * The key card's second page. Three readers, one per program, each silent
 * when its program or its source is absent — and NO OTHER PROGRAM IS GUESSED
 * AT. A page of keys that a program does not actually answer to is worse than
 * no page: it is a card being confidently wrong, which is the one thing a help
 * surface must not be.
 *
 * EACH READS WHAT ITS PROGRAM PUBLISHES, and the three are not alike:
 * `tmux` is asked, and answers with a live list including anything the person
 * rebound; `mc` has a keymap FILE, which is the bindings it was configured
 * with rather than the ones compiled into it; and `micro` has neither — its
 * defaults are compiled in and no flag prints them — so its reader shows the
 * overrides file and nothing on a machine where nobody has rebound anything.
 * Each says so where it is written.
 *
 * HELIX IS NOT HERE, and that is measured rather than an omission. The plan
 * names it, but `helix` is in no `packages.txt` at all: the port exists and
 * has never been built or shipped, so a reader for it could not run and could
 * not be checked. It goes in when the port does.
 */

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "kbase.h"
#include "progkeys.h"

static int pk_tmux(struct sh_progkey *out, int max)
{
	KbBuf b = { 0 };
	KbArgv a = { 0 };
	char pfx[SH_PK_KEY] = "";
	char *p;
	int n = 0;

	if (max <= 0 || !kb_have_prog("tmux"))
		return 0;

	/*
	 * THE PREFIX TABLE ONLY. The other three tables are not what a person
	 * means by "tmux's keys": `root` is the mouse and a handful of bare
	 * keys, and the two copy-mode tables are live only while copy mode is
	 * up — 161 of the 267 default bindings, none of them reachable from the
	 * prompt the card is drawn over.
	 *
	 * `-N` LISTS ONLY THE BINDINGS THAT CARRY A NOTE, and prints the note
	 * in place of the command. That is the whole reason this reader is
	 * possible: without it the second column is `confirm-before -p
	 * "kill-pane #P? (y/n)" kill-pane`, which is a command, not a
	 * description. `-aN` would add the un-noted bindings back with their
	 * commands as the text; a row nobody wrote a description for has none.
	 */
	kb_argv_add(&a, "tmux");
	kb_argv_add(&a, "list-keys");
	kb_argv_add(&a, "-N");
	kb_argv_add(&a, "-T");
	kb_argv_add(&a, "prefix");
	kb_argv_end(&a);
	if (kb_run_capture_buf(&a, &b) != 0 || !b.p) {
		kb_buf_free(&b);
		return 0;
	}

	/*
	 * THE PREFIX ITSELF, because a row reading `c` is not what anybody
	 * presses. It is a server option, so this needs the server the focused
	 * tmux is attached to; `list-keys` above answers from the built-in
	 * defaults when there is none. C-b when it cannot be asked — tmux's own
	 * default, and the wrong guess costs one wrong modifier rather than the
	 * page. `None` means the user unbound it and the keys are pressed bare.
	 */
	a.n = 0;
	kb_argv_add(&a, "tmux");
	kb_argv_add(&a, "show-options");
	kb_argv_add(&a, "-gv");
	kb_argv_add(&a, "prefix");
	kb_argv_end(&a);
	if (kb_run_capture(&a, pfx, sizeof pfx) != 0)
		kb_strlcpy(pfx, "C-b", sizeof pfx);
	if (!pfx[0] || !strcmp(pfx, "None"))
		pfx[0] = '\0';

	/*
	 * One binding per line, `<key><padding><note>`. The key column is
	 * widened to the longest key in the table, so the split is at the first
	 * run of spaces and never at a fixed column; the note is the rest of
	 * the line verbatim, since it carries spaces and quotes of its own.
	 */
	for (p = b.p; *p && n < max; ) {
		char *eol = strchr(p, '\n'), *key = p, *desc;
		size_t klen;

		if (eol)
			*eol = '\0';

		while (*key == ' ' || *key == '\t')
			key++;
		desc = key;
		while (*desc && *desc != ' ' && *desc != '\t')
			desc++;
		klen = (size_t)(desc - key);
		while (*desc == ' ' || *desc == '\t')
			desc++;

		/* A key with no note is not a row: `-N` should not produce one,
		 * and a blank right-hand column would read as a broken card. */
		if (klen && klen < SH_PK_KEY && *desc) {
			if (pfx[0])
				snprintf(out[n].key, SH_PK_KEY, "%s %.*s",
					 pfx, (int)klen, key);
			else
				snprintf(out[n].key, SH_PK_KEY, "%.*s",
					 (int)klen, key);
			kb_strlcpy(out[n].desc, desc, SH_PK_DESC);
			n++;
		}

		if (!eol)
			break;
		p = eol + 1;
	}

	kb_buf_free(&b);
	return n;
}

/*
 * MC'S PANEL KEYS, out of its keymap file.
 *
 * THE [filemanager] SECTION AND NOTHING ELSE. That section is the panel
 * screen — the F-row and the commands a person means by "mc's keys".
 * [filemanager:xmap] is reachable only after the Ctrl-x prefix, which this
 * page already names, and [panel], [dialog], [menu], [input], [editor] and
 * [viewer] rebind a widget or another program that is not what the panel in
 * front of the reader answers to.
 *
 * THE FILES ARE READ IN MC'S OWN ORDER AND EACH OVERRIDES THE LAST (man mc,
 * "Redefine hotkey bindings"): the upstream location, then /etc/mc — where
 * this port's --sysconfdir=/etc puts the shipped keymap, and the only one of
 * the two that exists here — then the user's copy under $XDG_CONFIG_HOME.
 * Merging rather than stopping at the first file is what mc does: a user file
 * that rebinds two actions leaves the other thirty-six at their system
 * bindings, and a card built from that file alone would show two keys and
 * imply the rest were gone.
 */
static int pk_mc(struct sh_progkey *out, int max)
{
	/* mc's key names, spelled as the key is pressed — the `shortcut`
	 * column of its key_name_conv_tab, with the keypad entries kept
	 * distinct: [filemanager] binds Select to kpplus while [panel] binds
	 * it to alt-plus, and rendering both as "+" names one key twice. */
	static const struct {
		const char *tok, *label;
	} keys[] = {
		{ "tab", "Tab" }, { "enter", "Enter" }, { "kpenter", "Enter" },
		{ "space", "Space" }, { "escape", "Esc" }, { "esc", "Esc" },
		{ "backspace", "Backspace" }, { "bs", "Backspace" },
		{ "insert", "Ins" }, { "ins", "Ins" },
		{ "delete", "Del" }, { "del", "Del" },
		{ "home", "Home" }, { "end", "End" },
		{ "up", "Up" }, { "down", "Down" },
		{ "left", "Left" }, { "right", "Right" },
		{ "pgup", "PgUp" }, { "pgdn", "PgDn" },
		{ "backtab", "Shift-Tab" }, { "complete", "Alt-Tab" },
		{ "a1", "A1" }, { "c1", "C1" },
		{ "kpplus", "Keypad +" }, { "kpminus", "Keypad -" },
		{ "kpasterisk", "Keypad *" }, { "kpslash", "Keypad /" },
		{ "asterisk", "*" }, { "minus", "-" }, { "plus", "+" },
		{ "dot", "." }, { "comma", "," }, { "equal", "=" },
		{ "lt", "<" }, { "gt", ">" }, { "colon", ":" },
		{ "semicolon", ";" }, { "exclamation", "!" },
		{ "question", "?" }, { "ampersand", "&" }, { "dollar", "$" },
		{ "quota", "\"" }, { "apostrophe", "'" }, { "percent", "%" },
		{ "caret", "^" }, { "tilda", "~" }, { "prime", "`" },
		{ "underline", "_" }, { "understrike", "_" }, { "pipe", "|" },
		{ "lparenthesis", "(" }, { "rparenthesis", ")" },
		{ "lbracket", "[" }, { "rbracket", "]" },
		{ "lbrace", "{" }, { "rbrace", "}" },
		{ "slash", "/" }, { "backslash", "\\" },
		{ "number", "#" }, { "hash", "#" }, { "at", "@" },
	};
	/* Wording from mc's own manual and menus. An action not listed here is
	 * shown as its token, spaced out: a guess at what an action does is
	 * worse on this card than the name mc gives it. */
	static const struct {
		const char *act, *desc;
	} names[] = {
		{ "ChangePanel", "Change panel" },
		{ "Help", "Help" },
		{ "UserMenu", "User menu" },
		{ "View", "View file" },
		{ "Edit", "Edit file" },
		{ "Copy", "Copy" },
		{ "Move", "Rename or move" },
		{ "MakeDir", "Make directory" },
		{ "Delete", "Delete" },
		{ "Menu", "Menu bar" },
		{ "Quit", "Quit" },
		{ "Find", "Find file" },
		{ "CdQuick", "Quick cd" },
		{ "HotList", "Directory hotlist" },
		{ "Reread", "Reread the directory" },
		{ "DirSize", "Directory size" },
		{ "Suspend", "Suspend mc" },
		{ "Swap", "Swap the panels" },
		{ "History", "Command history" },
		{ "ShowHidden", "Show hidden files" },
		{ "Shell", "Show the shell output" },
		{ "PutCurrentPath", "Current path to the command line" },
		{ "PutOtherPath", "Other path to the command line" },
		{ "ViewFiltered", "View a command's output" },
		{ "Select", "Select a group of files" },
		{ "Unselect", "Unselect a group" },
		{ "SelectInvert", "Invert the selection" },
		{ "ScreenList", "Screen list" },
		{ "ExtendedKeyMap", "Prefix for the second key table" },
	};
	/* One slot per action, so a later file replaces a binding in place
	 * instead of the card listing the action twice. */
	enum { PK_MC_ROWS = 96, PK_MC_ACT = 40 };
	char acts[PK_MC_ROWS][PK_MC_ACT];
	char user[512];
	const char *files[3];
	const char *xdg;
	int nfiles = 0, n = 0, fi, i, j;

	if (max <= 0 || !kb_have_prog("mc"))
		return 0;
	if (max > PK_MC_ROWS)
		max = PK_MC_ROWS;

	files[nfiles++] = "/usr/share/mc/mc.keymap";
	files[nfiles++] = "/etc/mc/mc.keymap";
	xdg = getenv("XDG_CONFIG_HOME");
	if (xdg && *xdg)
		snprintf(user, sizeof user, "%s/mc/mc.keymap", xdg);
	else
		snprintf(user, sizeof user, "%s/.config/mc/mc.keymap",
			 kb_home_dir());
	files[nfiles++] = user;

	for (fi = 0; fi < nfiles; fi++) {
		char *txt = kb_read_whole(files[fi], NULL), *p;
		int inseg = 0;

		if (!txt)
			continue;
		for (p = txt; *p;) {
			char *s = p, *eol = strchr(p, '\n'), *eq, *v, *e;
			char row[SH_PK_KEY], act[PK_MC_ACT], desc[SH_PK_DESC];
			int slot;

			if (eol) {
				*eol = '\0';
				p = eol + 1;
			} else {
				p += strlen(p);
			}
			while (*s == ' ' || *s == '\t')
				s++;
			e = s + strlen(s);
			while (e > s && (e[-1] == ' ' || e[-1] == '\t' ||
					 e[-1] == '\r'))
				*--e = '\0';
			if (!*s || *s == '#')
				continue;
			if (*s == '[') {
				inseg = strcmp(s, "[filemanager]") == 0;
				continue;
			}
			if (!inseg || !(eq = strchr(s, '=')))
				continue;

			/* Left of '=' is the action; nothing else names it. */
			e = eq;
			while (e > s && (e[-1] == ' ' || e[-1] == '\t'))
				e--;
			if (e == s || (size_t)(e - s) >= sizeof act)
				continue;
			memcpy(act, s, (size_t)(e - s));
			act[e - s] = '\0';

			/* Right of it, one or more keys separated by ';'. */
			row[0] = '\0';
			for (v = eq + 1; *v;) {
				char tok[64], one[SH_PK_KEY], *t, *end, *dash;
				size_t used;

				end = strchr(v, ';');
				if (!end)
					end = v + strlen(v);
				t = v;
				v = *end ? end + 1 : end;
				while (t < end && (*t == ' ' || *t == '\t'))
					t++;
				while (end > t && (end[-1] == ' ' ||
						   end[-1] == '\t'))
					end--;
				if (end == t || (size_t)(end - t) >= sizeof tok)
					continue;
				memcpy(tok, t, (size_t)(end - t));
				tok[end - t] = '\0';

				/* Modifiers first and the key name last: no
				 * name in mc's table contains a dash, so the
				 * final segment is always the key itself. */
				one[0] = '\0';
				for (t = tok;; t = dash + 1) {
					const char *lab = NULL;
					char seg[64];
					size_t sn;

					dash = strchr(t, '-');
					sn = dash ? (size_t)(dash - t)
						  : strlen(t);
					if (!sn || sn >= sizeof seg) {
						one[0] = '\0';
						break;
					}
					memcpy(seg, t, sn);
					seg[sn] = '\0';
					if (dash) {
						if (kb_str_ieq(seg, "ctrl") ||
						    kb_str_ieq(seg, "control"))
							lab = "Ctrl";
						else if (kb_str_ieq(seg, "alt") ||
							 kb_str_ieq(seg, "meta") ||
							 kb_str_ieq(seg, "ralt"))
							lab = "Alt";
						else if (kb_str_ieq(seg, "shift"))
							lab = "Shift";
					} else {
						for (i = 0; !lab && i <
						     (int)(sizeof keys /
							   sizeof keys[0]); i++)
							if (kb_str_ieq(seg,
								keys[i].tok))
								lab = keys[i].label;
						if (!lab) {
							/* fN is the only
							 * name mc spells in
							 * lower case that a
							 * person reads in
							 * upper. */
							int fk = (seg[0] == 'f' ||
								  seg[0] == 'F') &&
								 seg[1] != '\0';

							for (j = 1; fk && seg[j]; j++)
								if (!isdigit((unsigned char)seg[j]))
									fk = 0;
							if (fk)
								seg[0] = 'F';
							else if (sn > 1)
								seg[0] = (char)toupper(
									(unsigned char)seg[0]);
						}
					}
					if (!lab)
						lab = seg;
					used = strlen(one);
					/* A chord that does not fit is
					 * dropped whole: half a chord is a
					 * key nobody can press. */
					if (used + (used ? 1 : 0) +
					    strlen(lab) >= sizeof one) {
						one[0] = '\0';
						break;
					}
					if (used)
						one[used++] = '-';
					kb_strlcpy(one + used, lab,
						   sizeof one - used);
					if (!dash)
						break;
				}
				if (!one[0])
					continue;
				used = strlen(row);
				if (!used)
					kb_strlcpy(row, one, sizeof row);
				else if (used + 2 + strlen(one) < sizeof row) {
					row[used++] = ',';
					row[used++] = ' ';
					kb_strlcpy(row + used, one,
						   sizeof row - used);
				} else
					break;	/* keep the row a PREFIX of
						 * mc's own list: skipping one
						 * alternative and printing a
						 * later one reads as an order
						 * mc does not have */
			}
			if (!row[0])
				continue;

			desc[0] = '\0';
			for (i = 0; i < (int)(sizeof names /
					      sizeof names[0]); i++)
				if (strcmp(act, names[i].act) == 0) {
					kb_strlcpy(desc, names[i].desc,
						   sizeof desc);
					break;
				}
			if (!desc[0]) {
				/* "PutCurrentSelected" -> "Put current
				 * selected": the action token spaced out, so
				 * an unlisted action still reads as words and
				 * still says only what mc calls it. */
				for (i = 0, j = 0; act[i] &&
				     j + 2 < (int)sizeof desc; i++) {
					if (i && isupper((unsigned char)act[i]))
						desc[j++] = ' ';
					desc[j++] = i ? (char)tolower(
						(unsigned char)act[i]) : act[i];
				}
				desc[j] = '\0';
			}

			for (slot = 0; slot < n; slot++)
				if (strcmp(acts[slot], act) == 0)
					break;
			if (slot == n) {
				if (n == max)
					continue;
				kb_strlcpy(acts[n], act, sizeof acts[0]);
				n++;
			}
			kb_strlcpy(out[slot].key, row, sizeof out[slot].key);
			kb_strlcpy(out[slot].desc, desc, sizeof out[slot].desc);
		}
		free(txt);
	}
	return n;
}

/*
 * One json5 string, unescaped into `out`; returns the byte after the closing
 * quote, or NULL if the string never closes. Either quote opens one, and the
 * opener is the closer.
 *
 * A \u escape above 0x7f becomes '?': every key name and action name micro
 * accepts is ASCII, and the one escape above it that occurs in practice is a
 * raw terminal sequence, which pk_micro drops rather than prints.
 */
static const char *pkm_str(const char *p, char *out, size_t n)
{
	char q = *p++;
	size_t i = 0;

	for (; *p && *p != q; p++) {
		char c = *p;

		if (c == '\\') {
			switch (*++p) {
			case '\0':
				return NULL;
			case 'n': c = '\n'; break;
			case 't': c = '\t'; break;
			case 'r': c = '\r'; break;
			case 'b': c = '\b'; break;
			case 'f': c = '\f'; break;
			case 'u': {
				unsigned cp = 0;

				for (int d = 0; d < 4; d++) {
					char h = p[1 + d];

					if (!isxdigit((unsigned char)h))
						return NULL;
					cp = cp * 16 + (unsigned)(h <= '9'
						? h - '0'
						: (h | 0x20) - 'a' + 10);
				}
				p += 4;
				c = cp < 0x80 ? (char)cp : '?';
				break;
			}
			default: c = *p; break;
			}
		}
		if (i + 1 < n)
			out[i++] = c;
	}
	if (*p != q)
		return NULL;
	out[i] = '\0';
	return p + 1;
}

/*
 * MICRO'S KEYS — AND ONLY THE ONES SOMEBODY REBOUND.
 *
 * micro's hundred-odd bindings are compiled into the binary, and no flag
 * prints them: `-options` prints options, `> help defaultkeys` needs a running
 * editor on a terminal, and the image ships no micro configuration of any
 * kind. The single thing readable from outside is `bindings.json`, which holds
 * the user's OVERRIDES and nothing else — micro creates it as `{}` on first
 * run — so a machine where nobody has rebound anything yields no rows and the
 * card shows no page for micro. That is the honest result: a page listing two
 * rows beside a hundred unlisted defaults teaches the reader that Ctrl-s is
 * not bound.
 *
 * The file is json5 — comments, single quotes and trailing commas are legal —
 * and micro rewrites it as strict JSON whenever `> bind` runs. Both shapes
 * parse here. An unquoted key, which json5 also allows and nothing writes,
 * does not, and neither does anything else malformed: the reader then yields
 * NOTHING rather than the rows it read before the error, because a list that
 * stops early is a list nobody can tell is short.
 */
static int pk_micro(struct sh_progkey *out, int max)
{
	static const struct { const char *act, *desc; } words[] = {
		{ "Save", "save the file" },
		{ "SaveAll", "save every buffer" },
		{ "SaveAs", "save as" },
		{ "Quit", "close the buffer" },
		{ "QuitAll", "quit micro" },
		{ "ForceQuit", "quit without saving" },
		{ "OpenFile", "open a file" },
		{ "Find", "search" },
		{ "FindNext", "next match" },
		{ "FindPrevious", "previous match" },
		{ "Undo", "undo" },
		{ "Redo", "redo" },
		{ "Copy", "copy" },
		{ "Cut", "cut" },
		{ "Paste", "paste" },
		{ "CutLine", "cut the line" },
		{ "DuplicateLine", "duplicate the line" },
		{ "DeleteLine", "delete the line" },
		{ "SelectAll", "select all" },
		{ "CommandMode", "the command bar" },
		{ "ShellMode", "run a shell command" },
		{ "ToggleHelp", "help" },
		{ "ToggleRuler", "line numbers on / off" },
		{ "AddTab", "new tab" },
		{ "NextTab", "next tab" },
		{ "PreviousTab", "previous tab" },
		{ "VSplit", "split beside" },
		{ "HSplit", "split below" },
		{ "NextSplit", "next split" },
		{ "Unsplit", "close the split" },
		{ "JumpLine", "go to a line" },
		{ "ToggleMacro", "record a macro" },
		{ "PlayMacro", "play the macro" },
		{ "Suspend", "suspend micro" },
		{ "Escape", "cancel" },
		{ "None", "unbound" },
	};
	char path[512], pane[32] = "", raw[SH_PK_KEY * 2], act[SH_PK_DESC];
	const char *env, *p;
	char *txt;
	int n = 0, in_pane = 0;

	if (max <= 0 || !kb_have_prog("micro"))
		return 0;

	/* micro's own search order. A card reading a different directory from
	 * the editor reports bindings that are not the ones in force. */
	if ((env = getenv("MICRO_CONFIG_HOME")) && *env)
		snprintf(path, sizeof path, "%s/bindings.json", env);
	else if ((env = getenv("XDG_CONFIG_HOME")) && *env)
		snprintf(path, sizeof path, "%s/micro/bindings.json", env);
	else
		snprintf(path, sizeof path, "%s/.config/micro/bindings.json",
			 kb_home_dir());

	txt = kb_read_whole(path, NULL);
	if (!txt)
		return 0;

	/* Comments out first, so the scan below sees only whitespace and
	 * tokens. A quote inside a comment and a slash inside a string each
	 * look like the other, so this pass tracks which one it is inside. */
	for (char *s = txt; *s; s++) {
		if (*s == '"' || *s == '\'') {
			char q = *s;

			for (s++; *s && *s != q; s++)
				if (*s == '\\' && s[1])
					s++;
			if (!*s)
				break;
		} else if (s[0] == '/' && s[1] == '/') {
			while (*s && *s != '\n')
				*s++ = ' ';
			if (!*s)
				break;
			s--;
		} else if (s[0] == '/' && s[1] == '*') {
			char *e = strstr(s + 2, "*/");

			if (!e)
				goto done;	/* unterminated: no rows */
			while (s < e + 2)
				*s++ = ' ';
			s--;
		}
	}

	for (p = txt; isspace((unsigned char)*p); p++)
		;
	if (*p++ != '{')
		goto done;

	for (;;) {
		while (isspace((unsigned char)*p))
			p++;
		if (*p == ',') {		/* json5 trailing comma */
			p++;
			continue;
		}
		if (*p == '}') {
			p++;
			if (!in_pane)
				break;		/* the outer object closed */
			in_pane = 0;
			pane[0] = '\0';
			continue;
		}
		if (*p != '"' && *p != '\'')
			goto fail;
		p = pkm_str(p, raw, sizeof raw);
		if (!p)
			goto fail;
		while (isspace((unsigned char)*p))
			p++;
		if (*p++ != ':')
			goto fail;
		while (isspace((unsigned char)*p))
			p++;

		/* A key whose value is an object names a PANE — buffer,
		 * command or terminal — and its members are the bindings.
		 * micro allows no deeper nesting, so neither does this. */
		if (*p == '{') {
			if (in_pane)
				goto fail;
			in_pane = 1;
			kb_strlcpy(pane, raw, sizeof pane);
			p++;
			continue;
		}
		if (*p != '"' && *p != '\'') {
			/* Not a binding; micro rejects it too. Step over the
			 * value and keep reading the entries that are. */
			while (*p && *p != ',' && *p != '}')
				p++;
			continue;
		}
		p = pkm_str(p, act, sizeof act);
		if (!p)
			goto fail;

		if (n >= max)
			continue;
		/* A raw escape sequence is a real binding and an impossible
		 * row: it would print the bytes the terminal sends, which is
		 * not something a person can be told to press. */
		if (!raw[0] || !act[0] || raw[0] == '\x1b')
			continue;

		{
			struct sh_progkey *r = &out[n];
			char *d = r->key;
			size_t left = sizeof r->key - 1;
			const char *s = raw;

			/* `<Ctrl-w><Ctrl-w>` is micro's spelling of a two-step
			 * sequence: the brackets group it and are not pressed,
			 * so a space separates the steps instead. A modifier
			 * written without its dash gets one — micro reads
			 * CtrlY, Ctrl-Y and Ctrl-y as the same key. */
			while (*s && left) {
				static const char *const mod[] = {
					"Ctrl", "Alt", "Shift", NULL
				};
				int wrote_mod = 0;

				if (*s == '<') {
					s++;
					continue;
				}
				if (*s == '>') {
					if (*++s) {
						*d++ = ' ';
						left--;
					}
					continue;
				}
				for (int i = 0; mod[i]; i++) {
					size_t seg = strlen(mod[i]);

					if (strncmp(s, mod[i], seg) ||
					    s[seg] == '\0' || s[seg] == '-' ||
					    s[seg] == '>')
						continue;
					wrote_mod = 1;
					if (left < seg + 1) {
						left = 0;
						break;
					}
					memcpy(d, s, seg);
					d += seg;
					*d++ = '-';
					left -= seg + 1;
					s += seg;
					break;
				}
				if (wrote_mod)
					continue;
				*d++ = *s++;
				left--;
			}
			*d = '\0';
			/* Whatever is left of `s` did not fit, and half a
			 * chord names a key nobody has: drop the row rather
			 * than print a prefix of one. */
			if (*s || !r->key[0])
				continue;

			/* THE ACTION IS COPIED WHOLE UNLESS IT IS ONE PLAIN
			 * NAME. `Save,Quit`, `Autocomplete|IndentSelection`,
			 * `command:pwd` and `lua:initlua.bar` each mean
			 * something the table would have to invent, and an
			 * invented row is worse than micro's own spelling. A
			 * name the table has not heard of is printed as itself
			 * for the same reason: a row saying `ToggleDiffGutter`
			 * is one a person can go and look up, and a dropped
			 * row is a key nobody knows exists. */
			r->desc[0] = '\0';
			if (!strpbrk(act, ",|&:"))
				for (size_t i = 0;
				     i < sizeof words / sizeof words[0]; i++)
					if (!strcmp(act, words[i].act)) {
						kb_strlcpy(r->desc,
							   words[i].desc,
							   sizeof r->desc);
						break;
					}
			if (!r->desc[0])
				kb_strlcpy(r->desc, act, sizeof r->desc);

			/* Which pane a binding acts in is half of what it does:
			 * Escape on a buffer and Escape in the command bar are
			 * two rows that would otherwise read the same. The
			 * buffer is the unmarked case. */
			if (pane[0] && strcmp(pane, "buffer")) {
				size_t at = strlen(r->desc);

				snprintf(r->desc + at, sizeof r->desc - at,
					 " (%s pane)", pane);
			}
			n++;
		}
	}
	goto done;
fail:
	n = 0;
done:
	free(txt);
	return n;
}

/* pk_micro needs <ctype.h> (isspace/isxdigit) beside the stdio/stdlib/string
 * that keys.c already includes; nothing else and no shell. */

/*
 * WHICH READER, BY THE PROGRAM'S NAME. A table rather than a chain, because
 * the answer to "is there a reader for this" and the answer to "run it" have
 * to be the same list — a Tab offered for a program with no reader is a page
 * that opens empty.
 */
static const struct {
	const char *prog;
	int (*read)(struct sh_progkey *out, int max);
} PK[] = {
	{ "tmux",  pk_tmux },
	{ "mc",    pk_mc },
	{ "micro", pk_micro },
};
#define PK_N ((int)(sizeof(PK) / sizeof(PK[0])))

int sh_progkeys_known(const char *prog)
{
	if (!prog || !*prog)
		return 0;
	for (int i = 0; i < PK_N; i++)
		if (!strcmp(PK[i].prog, prog))
			return 1;
	return 0;
}

int sh_progkeys(const char *prog, struct sh_progkey *out, int max)
{
	if (!prog || !*prog || !out || max <= 0)
		return 0;
	for (int i = 0; i < PK_N; i++)
		if (!strcmp(PK[i].prog, prog))
			return PK[i].read(out, max);
	return 0;
}
