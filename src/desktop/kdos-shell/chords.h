/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   The chords this desktop binds, read from the desktop that binds them —
 *   see chords.c
 *
 * A HEADER OF ITS OWN, and not two lines in `shell.h`: that header pulls in
 * Wayland, the icon layer and the chrome band, and none of the three has
 * anything to do with reading a table of keys. Keeping the interface here is
 * what lets the readers be compiled and driven on a host with no compositor
 * at all, which is where a parse is actually checked.
 *
 * ONE READER FOR EVERY SURFACE THAT NAMES A CHORD. The key card and the
 * palette ask the same question of the same two sources, and a private copy in
 * either of them is the copy that goes stale — a surface naming a chord this
 * session does not bind is worse than a surface naming none.
 *
 * THE WORDS ARE NOT HERE. What a chord is CALLED and which group it belongs in
 * are the asking surface's: the card says "close the window" where the palette
 * shows the action's own name, and neither is the reader's business.
 * ---------------------------------
 */

#ifndef SH_CHORDS_H
#define SH_CHORDS_H

/* Room for either desktop's whole table, the console's recorded scripts and
 * the workspace rows the session answers without binding. A table past this is
 * truncated rather than grown: what is loaded is drawn by surfaces that scroll
 * it, and no keyboard has this many chords on it. */
#define SH_CHORD_MAX 160

struct sh_chord {
	char chord[48];		/* "Super+Shift+t", as a person reads it */
	char action[64];	/* the action name, or the labwc action */
	char detail[128];	/* what it runs when the source knows */
	/*
	 * THE PROGRAM THIS ROW CANNOT WORK WITHOUT, or empty.
	 *
	 * Only a run-or-raise row has one — the chord names a role and the
	 * program filling it may not be installed. A row whose `needs` is
	 * missing is never added by the reader, so no surface has to know the
	 * rule: see sh_chords_load(). Every other row is empty here, because
	 * a chord that opens this desktop's own surface is a chord this image
	 * always carries.
	 */
	char needs[64];
};

/*
 * Read the table of the desktop this is, and return how many rows it has. The
 * count is never negative: when neither reader answers, what is loaded is the
 * built-in defaults rather than nothing, because a surface with an empty chord
 * list teaches nobody the key that would have filled it.
 *
 * Idempotent — the first call reads and every later one returns the same
 * count, so a surface may ask per keystroke.
 */
int sh_chords_load(void);

int sh_chords_count(void);

/* NULL outside the loaded table. */
const struct sh_chord *sh_chord_at(int i);

/* Non-zero when neither desktop's reader answered and the built-in defaults
 * are what is loaded. A surface that tells the reader where its rows came from
 * — the key card prints the refusal above them — has to be able to tell the
 * defaults from a table that was really read. */
int sh_chords_builtin(void);

/* Discard what was read and load the built-in table instead, returning the
 * count. For a consumer that read rows and could use NONE of them: the reader
 * cannot know that, because only the consumer knows what it can use, and a
 * surface that came up empty without saying why is the failure this avoids.
 * sh_chords_builtin() answers yes afterwards, which is what shows the note. */
int sh_chords_use_builtin(void);

#endif /* SH_CHORDS_H */
