/* kdos-con — the chord table. See con.h.
 *
 * THE SAME CHORDS rc.xml BINDS. The graphical desktop reads labwc's XML and
 * this one reads no XML at all, so the defaults live twice in two syntaxes —
 * but they are the same defaults, because a person who learns Super+Return on
 * one desktop must not have to unlearn it on the other. Changing a default
 * here changes it in `fs/etc/skel/.config/kdos-comp/rc.xml` too.
 *
 * ~/.config/kdos-con/keys.conf overrides the table below, one `chord = action`
 * per line. A file that names no chord for an action leaves that action on its
 * default, so a user rebinds one key without restating forty.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "con.h"
#include "kbase.h"

typedef struct {
	const char *name;
	int action;
	int arg;
	int key;
	int mods;
} Bind;

/*
 * Every chord the session keeps for itself. Super is the desktop's modifier —
 * a chord without it belongs to whatever program has the focus, and the two
 * Alt+Tab rows are the exception a lifetime of muscle memory earns.
 */
static Bind binds[] = {
	{ "terminal",	CON_ACT_TERM,	 0, KT_K_ENTER,	KT_MOD_SUPER },
	{ "close",	CON_ACT_CLOSE,	 0, 'q',	KT_MOD_SUPER },
	{ "quit",	CON_ACT_QUIT,	 0, 'q',	KT_MOD_SUPER | KT_MOD_SHIFT },
	{ "maximise",	CON_ACT_MAX,	 0, 'm',	KT_MOD_SUPER },
	{ "fullscreen",	CON_ACT_FULL,	 0, 'f',	KT_MOD_SUPER },
	{ "minimise",	CON_ACT_MIN,	 0, 'n',	KT_MOD_SUPER },
	/* The way back. A minimise with no restore is a one-way door, and the
	 * shifted form of the chord that closed it is where a hand looks. */
	{ "restore",	CON_ACT_RESTORE, 0, 'n',	KT_MOD_SUPER | KT_MOD_SHIFT },
	{ "menu",	CON_ACT_EXEC,	 CON_CMD_MENU,	  ' ', KT_MOD_SUPER },
	{ "launcher",	CON_ACT_EXEC,	 CON_CMD_LAUNCHER, 'd', KT_MOD_SUPER },
	{ "lock",	CON_ACT_EXEC,	 CON_CMD_LOCK,	  'l', KT_MOD_SUPER },
	/* Blanking without locking, on the shifted form of the chord that
	 * locks: the idle timer is otherwise the only thing that can draw the
	 * saver, so a person leaving the machine has no way to ask. */
	{ "saver",	CON_ACT_EXEC,	 CON_CMD_SAVER,	  'l',
	  KT_MOD_SUPER | KT_MOD_SHIFT },
	{ "next",	CON_ACT_NEXT,	 0, KT_K_TAB,	KT_MOD_SUPER },
	{ "prev",	CON_ACT_PREV,	 0, KT_K_BTAB,	KT_MOD_SUPER | KT_MOD_SHIFT },
	{ "next-alt",	CON_ACT_NEXT,	 0, KT_K_TAB,	KT_MOD_ALT },
	{ "prev-alt",	CON_ACT_PREV,	 0, KT_K_BTAB,	KT_MOD_ALT | KT_MOD_SHIFT },
	{ "snap-left",	CON_ACT_SNAP,	 KWM_EDGE_LEFT,	  KT_K_LEFT,  KT_MOD_SUPER },
	{ "snap-right",	CON_ACT_SNAP,	 KWM_EDGE_RIGHT,  KT_K_RIGHT, KT_MOD_SUPER },
	{ "snap-up",	CON_ACT_SNAP,	 KWM_EDGE_TOP,	  KT_K_UP,    KT_MOD_SUPER },
	{ "snap-down",	CON_ACT_SNAP,	 KWM_EDGE_BOTTOM, KT_K_DOWN,  KT_MOD_SUPER },

	/*
	 * THE ARROW FAMILIES ARE SHARED WITH `rc.xml` AND EACH MEANS ONE
	 * THING. Super+arrow snaps on both desktops, Super+Alt+arrow moves the
	 * focused window — MoveToEdge there and a swap with the neighbour here
	 * — and Super+Ctrl+arrow grows a window on the compositor, so nothing
	 * here may take it.
	 *
	 * SUPER+SHIFT+ARROW IS UNBOUND ON THE COMPOSITOR because it has no
	 * directional-focus action at all. An unbound chord is the right
	 * answer there: pressing it does nothing rather than something else.
	 */
	{ "focus-left",	CON_ACT_FOCUS_DIR, KWM_EDGE_LEFT,   KT_K_LEFT,
	  KT_MOD_SUPER | KT_MOD_SHIFT },
	{ "focus-right", CON_ACT_FOCUS_DIR, KWM_EDGE_RIGHT, KT_K_RIGHT,
	  KT_MOD_SUPER | KT_MOD_SHIFT },
	{ "focus-up",	CON_ACT_FOCUS_DIR, KWM_EDGE_TOP,    KT_K_UP,
	  KT_MOD_SUPER | KT_MOD_SHIFT },
	{ "focus-down",	CON_ACT_FOCUS_DIR, KWM_EDGE_BOTTOM, KT_K_DOWN,
	  KT_MOD_SUPER | KT_MOD_SHIFT },
	{ "swap-left",	CON_ACT_SWAP_DIR, KWM_EDGE_LEFT,    KT_K_LEFT,
	  KT_MOD_SUPER | KT_MOD_ALT },
	{ "swap-right",	CON_ACT_SWAP_DIR, KWM_EDGE_RIGHT,   KT_K_RIGHT,
	  KT_MOD_SUPER | KT_MOD_ALT },
	{ "swap-up",	CON_ACT_SWAP_DIR, KWM_EDGE_TOP,	    KT_K_UP,
	  KT_MOD_SUPER | KT_MOD_ALT },
	{ "swap-down",	CON_ACT_SWAP_DIR, KWM_EDGE_BOTTOM,  KT_K_DOWN,
	  KT_MOD_SUPER | KT_MOD_ALT },

	/* Past the empty ones. Super+1..9 reaches a workspace by number and
	 * these reach the next one somebody is using. */
	{ "workspace-prev", CON_ACT_WS_STEP, 1, KT_K_PGUP, KT_MOD_SUPER },
	{ "workspace-next", CON_ACT_WS_STEP, 0, KT_K_PGDN, KT_MOD_SUPER },

	/*
	 * THE ONE CHORD THAT IS NOT ON SUPER, and it cannot be: it exists for
	 * the views where Super never arrives. A terminal that does not
	 * implement the kitty keyboard protocol — xterm, most VTEs, and the
	 * Linux VT a `--tty` view on tty1 runs in — reports no Super at all,
	 * which leaves every chord above unreachable. Ctrl+A is the prefix a
	 * lifetime of `screen` has taught; press it twice to send the literal
	 * to the window that has the focus.
	 */
	{ "leader",	CON_ACT_LEADER,	 0, 'a',	KT_MOD_CTRL },
};

#define NBINDS ((int)(sizeof(binds) / sizeof(binds[0])))

static int loaded;

static int key_named(const char *s)
{
	static const struct { const char *n; int k; } tab[] = {
		{ "Return", KT_K_ENTER }, { "Enter", KT_K_ENTER },
		{ "Space", ' ' }, { "Tab", KT_K_TAB }, { "Escape", KT_K_ESC },
		{ "Left", KT_K_LEFT }, { "Right", KT_K_RIGHT },
		{ "Up", KT_K_UP }, { "Down", KT_K_DOWN },
		{ "Home", KT_K_HOME }, { "End", KT_K_END },
		{ "PageUp", KT_K_PGUP }, { "PageDown", KT_K_PGDN },
		{ "Insert", KT_K_INS }, { "Delete", KT_K_DEL },
		{ "F1", KT_K_F1 }, { "F2", KT_K_F2 }, { "F3", KT_K_F3 },
		{ "F4", KT_K_F4 }, { "F5", KT_K_F5 }, { "F6", KT_K_F6 },
		{ "F7", KT_K_F7 }, { "F8", KT_K_F8 }, { "F9", KT_K_F9 },
		{ "F10", KT_K_F10 }, { "F11", KT_K_F11 }, { "F12", KT_K_F12 },
	};

	for (int i = 0; i < (int)(sizeof(tab) / sizeof(tab[0])); i++)
		if (!strcasecmp(tab[i].n, s))
			return tab[i].k;
	/* A single character is itself, lowercased: a chord is Super+q whether
	 * or not the layout delivers it with Shift, and Shift is a modifier
	 * the caller matched already. */
	if (s[0] && !s[1])
		return s[0] >= 'A' && s[0] <= 'Z' ? s[0] + 32 : s[0];
	return 0;
}

/*
 * "Super+Shift+Tab" -> key and mods. Zero on a chord naming no key, which is
 * how a typo leaves the default standing rather than unbinding the action.
 */
static int chord_parse(const char *s, int *key, int *mods)
{
	char buf[64], *tok, *save;

	snprintf(buf, sizeof(buf), "%s", s);
	*key = 0;
	*mods = 0;
	for (tok = strtok_r(buf, "+", &save); tok;
	     tok = strtok_r(NULL, "+", &save)) {
		while (*tok == ' ' || *tok == '\t')
			tok++;
		char *e = tok + strlen(tok);
		while (e > tok && (e[-1] == ' ' || e[-1] == '\t'))
			*--e = '\0';

		if (!strcasecmp(tok, "Super") || !strcasecmp(tok, "Win"))
			*mods |= KT_MOD_SUPER;
		else if (!strcasecmp(tok, "Shift"))
			*mods |= KT_MOD_SHIFT;
		else if (!strcasecmp(tok, "Alt"))
			*mods |= KT_MOD_ALT;
		else if (!strcasecmp(tok, "Ctrl") || !strcasecmp(tok, "Control"))
			*mods |= KT_MOD_CTRL;
		else
			*key = key_named(tok);
	}

	/* Shift is part of the chord for a NAMED key and part of the character
	 * for a typed one: "Super+Shift+1" must match the '1' the keyboard
	 * sends with Shift held, not '!'. */
	return *key != 0;
}

static void keys_load(void)
{
	char buf[4096], path[256];
	const char *xdg = getenv("XDG_CONFIG_HOME"), *home;
	char *line, *save;

	if (loaded)
		return;
	loaded = 1;

	if (xdg && *xdg)
		snprintf(path, sizeof(path), "%s/kdos-con/keys.conf", xdg);
	else {
		home = kb_home_dir();
		snprintf(path, sizeof(path), "%s/.config/kdos-con/keys.conf",
			 home ? home : "/root");
	}
	if (kb_read_file(path, buf, sizeof(buf)) <= 0)
		return;

	for (line = strtok_r(buf, "\n", &save); line;
	     line = strtok_r(NULL, "\n", &save)) {
		char *hash = strchr(line, '#'), *eq;
		int key, mods;

		if (hash)
			*hash = '\0';
		eq = strchr(line, '=');
		if (!eq)
			continue;
		*eq = '\0';

		char *chord = line, *act = eq + 1;
		while (*chord == ' ' || *chord == '\t')
			chord++;
		while (*act == ' ' || *act == '\t')
			act++;
		char *e = act + strlen(act);
		while (e > act && (e[-1] == ' ' || e[-1] == '\t' ||
				   e[-1] == '\r'))
			*--e = '\0';
		e = chord + strlen(chord);
		while (e > chord && (e[-1] == ' ' || e[-1] == '\t'))
			*--e = '\0';

		if (!chord_parse(chord, &key, &mods))
			continue;
		for (int i = 0; i < NBINDS; i++) {
			if (strcmp(binds[i].name, act))
				continue;
			binds[i].key = key;
			binds[i].mods = mods;
			break;
		}
	}
}

/*
 * SHIFT IS ENCODED TWICE and only one of them is reliable. A backend that
 * reads a terminal cannot report Shift at all — it sees the shifted character
 * and nothing else — so for a key whose shifted form is a DIFFERENT key
 * (Tab/BackTab) or a different character (a digit's symbol), the character is
 * the evidence and the modifier bit is not compared.
 */
static int mods_match(int want, int got, int key)
{
	if (key == KT_K_BTAB)
		return (want & ~KT_MOD_SHIFT) == (got & ~KT_MOD_SHIFT);
	return want == got;
}

/*
 * The workspace a digit names, or -1. The shifted row is matched by character
 * because that is what a terminal backend delivers; the table is the US one,
 * so send-to-workspace on a layout that shifts its digits elsewhere needs a
 * keys.conf line. `*shifted` says which of the two forms matched.
 */
static int digit_of(int key, int *shifted)
{
	static const char sh[] = "!@#$%^&*(";
	const char *p;

	*shifted = 0;
	if (key >= '1' && key <= '9')
		return key - '1';
	if (key > 0 && key < 128 && (p = strchr(sh, key)) != NULL && *p) {
		*shifted = 1;
		return (int)(p - sh);
	}
	return -1;
}

/*
 * The action a chord runs, or CON_ACT_NONE. Workspace switching is not in the
 * table: nine digits and nine shifted digits would be eighteen rows saying the
 * same thing, and a user who rebinds Super to something else wants all of them
 * moved together, which a table cannot express and this can.
 */
/*
 * A CHORD AS A PERSON READS IT — the inverse of chord_parse, and the reason
 * the key card can be right.
 *
 * The card is a different program and must not carry a second copy of this
 * table: a copy is a copy that goes stale, and a card that names a chord the
 * session does not bind is worse than no card. So the table that binds the
 * chords is the table that prints them.
 */
static void chord_name(int key, int mods, char *out, size_t n)
{
	static const struct { int k; const char *n; } named[] = {
		{ KT_K_ENTER, "Return" }, { ' ', "Space" },
		{ KT_K_TAB, "Tab" }, { KT_K_BTAB, "Tab" },
		{ KT_K_ESC, "Escape" },
		{ KT_K_LEFT, "Left" }, { KT_K_RIGHT, "Right" },
		{ KT_K_UP, "Up" }, { KT_K_DOWN, "Down" },
		{ KT_K_HOME, "Home" }, { KT_K_END, "End" },
		{ KT_K_PGUP, "PageUp" }, { KT_K_PGDN, "PageDown" },
		{ KT_K_INS, "Insert" }, { KT_K_DEL, "Delete" },
		{ KT_K_F1, "F1" }, { KT_K_F2, "F2" }, { KT_K_F3, "F3" },
		{ KT_K_F4, "F4" }, { KT_K_F5, "F5" }, { KT_K_F6, "F6" },
		{ KT_K_F7, "F7" }, { KT_K_F8, "F8" }, { KT_K_F9, "F9" },
		{ KT_K_F10, "F10" }, { KT_K_F11, "F11" }, { KT_K_F12, "F12" },
	};
	const char *kn = NULL;
	char one[2] = { 0, 0 };
	size_t o = 0;

	out[0] = '\0';
	if (mods & KT_MOD_SUPER)
		o += (size_t)snprintf(out + o, n - o, "Super+");
	if (mods & KT_MOD_CTRL)
		o += (size_t)snprintf(out + o, n - o, "Ctrl+");
	if (mods & KT_MOD_ALT)
		o += (size_t)snprintf(out + o, n - o, "Alt+");
	/* Back-tab carries Shift in the key itself; naming it twice would
	 * print Super+Shift+Shift+Tab. */
	if ((mods & KT_MOD_SHIFT) || key == KT_K_BTAB)
		o += (size_t)snprintf(out + o, n - o, "Shift+");

	for (size_t i = 0; i < sizeof(named) / sizeof(named[0]); i++)
		if (named[i].k == key) {
			kn = named[i].n;
			break;
		}
	if (!kn && key > 0x20 && key < 0x7f) {
		one[0] = (char)key;
		kn = one;
	}
	snprintf(out + o, n - o, "%s", kn ? kn : "?");
}

/*
 * `kdos-con --keys` — the bindings, after the keys.conf overlay, one per line
 * as `action<TAB>chord`. It is what the key card reads on this desktop.
 */
void keys_print(void)
{
	char chord[64];

	keys_load();
	for (int i = 0; i < NBINDS; i++) {
		chord_name(binds[i].key, binds[i].mods, chord, sizeof(chord));
		printf("%s\t%s\n", binds[i].name, chord);
	}
}

int keys_action(int key, int mods, int *arg)
{
	keys_load();
	*arg = 0;

	for (int i = 0; i < NBINDS; i++) {
		if (binds[i].key == key &&
		    mods_match(binds[i].mods, mods, key)) {
			*arg = binds[i].arg;
			return binds[i].action;
		}
	}

	if (mods & KT_MOD_SUPER) {
		int shifted, ws = digit_of(key, &shifted);

		if (ws >= 0) {
			*arg = ws;
			return (shifted || (mods & KT_MOD_SHIFT)) ?
				CON_ACT_SEND : CON_ACT_WS;
		}
	}

	return CON_ACT_NONE;
}
