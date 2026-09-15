/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   The focused program's own keys — see progkeys.c
 *
 * A header of its own, the `background.h` precedent: `shell.h` drags in
 * Wayland and reading another program's key list needs no display at all,
 * which is what lets these readers be driven from a test.
 * ---------------------------------
 */

#ifndef SH_PROGKEYS_H
#define SH_PROGKEYS_H

#define SH_PK_KEY  48
#define SH_PK_DESC 128

struct sh_progkey {
	char key[SH_PK_KEY];	/* "C-b c", "F5", "Alt-g" — as it is pressed */
	char desc[SH_PK_DESC];	/* what it does                              */
};

/*
 * The keys of `prog`, a bare command name. Returns how many rows were filled,
 * and 0 when there is nothing — the program is absent, its file is absent, or
 * nothing parsed. NEVER AN ERROR AND NEVER A MESSAGE: the card shows this page
 * only when a reader answers with rows, so silence is the whole failure mode
 * and it has to be quiet.
 */
int sh_progkeys(const char *prog, struct sh_progkey *out, int max);

/* Whether any reader exists for `prog` at all — asked before the page is
 * offered, so a program nobody wrote a reader for is not a Tab that leads to
 * an empty screen. */
int sh_progkeys_known(const char *prog);

#endif /* SH_PROGKEYS_H */
