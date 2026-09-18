/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   boxcheck — a launcher names a box, and is that box there
 *
 * `apps.c`'s two questions, asserted against a fixture store. A BINARY OF ITS
 * OWN because `selftest.c` links libraries and these live in `kdos-shell`;
 * linking the whole shell into the library self-test to reach two functions
 * would drag a Wayland client and a font renderer in with them. Four stubs are
 * the whole cost of not doing that — they are the launch path, which nothing
 * here calls.
 *
 * WHAT IS ACTUALLY BEING PROTECTED IS THE ASYMMETRY. `sh_box_missing` answers
 * 1 only where absence is PROVED, and every unknown resolves to "present":
 * hiding an application somebody installed is a worse failure than showing one
 * whose pack has gone. That rule reads like an incomplete check and is not one,
 * so it is pinned here — the cases below are the ones a tidy-up would delete.
 * ---------------------------------
 */

#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "launch.h"

/* The launch path, which none of this reaches. */
int kcon_run(const char *sock, char *const argv[], const char *title,
	     int floating, const char *size);
int sh_spawn(char *const argv[]);
void sh_strip_field_codes(char *s);
int sh_term_argv_in(const char *term, const char *cmd, char **argv, int max);

int kcon_run(const char *sock, char *const argv[], const char *title,
	     int floating, const char *size)
{
	(void)sock; (void)argv; (void)title; (void)floating; (void)size;
	return -1;
}
int sh_spawn(char *const argv[]) { (void)argv; return -1; }
void sh_strip_field_codes(char *s) { (void)s; }
int sh_term_argv_in(const char *term, const char *cmd, char **argv, int max)
{
	(void)term; (void)cmd; (void)argv; (void)max;
	return 0;
}

static int fails;

static void t(const char *what, int got, int want)
{
	printf("  %-52s %s\n", what, got == want ? "ok" : "FAIL");
	if (got != want)
		fails++;
}

int main(void)
{
	const char *store = getenv("BOXCHECK_STORE");
	const char *home = getenv("BOXCHECK_HOME");
	char b[128];

	if (!store || !home) {
		fprintf(stderr, "boxcheck: BOXCHECK_STORE and BOXCHECK_HOME "
				"name the fixture\n");
		return 2;
	}

	/* ── which box does this Exec line name ─────────────────────────── */
	t("a plain command is not boxed",
	  sh_exec_box("gedit %U", b, sizeof b), 0);
	t("  and names no box", b[0] == 0, 1);
	t("`kdos-appbox run` is boxed",
	  sh_exec_box("kdos-appbox run gimp", b, sizeof b), 1);
	t("  and names no box", b[0] == 0, 1);
	t("`-b` names the box",
	  sh_exec_box("kdos-appbox -b app.gimp run gimp-3.0", b, sizeof b), 1);
	t("  which is app.gimp", !strcmp(b, "app.gimp"), 1);
	t("`--box` behind an absolute path names it too",
	  sh_exec_box("/usr/bin/kdos-appbox --box app.x run y", b, sizeof b), 1);
	t("  which is app.x", !strcmp(b, "app.x"), 1);
	t("sh_exec_is_boxed asks only the first half",
	  sh_exec_is_boxed("kdos-appbox -b app.gimp run gimp-3.0"), 1);

	/* ── and is it there ────────────────────────────────────────────── */
	setenv("HOME", home, 1);

	/* NO STORE MEANS THE QUESTION HAS NO ANSWER, not that the answer is
	 * no. A machine with the pack subsystem absent would otherwise show an
	 * empty Start menu. */
	setenv("KDOS_PACK_STORE", "/nonexistent-kdos-pack-store", 1);
	t("no pack store: every box counts as present",
	  sh_box_missing("app.gimp"), 0);

	setenv("KDOS_PACK_STORE", store, 1);
	t("a row naming no box is present", sh_box_missing(""), 0);
	/* A name with a slash cannot have come from genlaunchers, so it is
	 * hand-written — and the one thing not to do with it is build a path
	 * out of it. */
	t("a name with a slash is not an id, so it is not aimed at",
	  sh_box_missing("../../etc/shadow"), 0);
	t("an installed .kpack is present", sh_box_missing("app.here"), 0);
	/* A store-BUILT box is a podman image the pack store has never heard
	 * of; its profile is the only thing on disk that names it. */
	t("a box profile with no pack file is present",
	  sh_box_missing("app.built"), 0);
	t("neither signal: provably absent", sh_box_missing("app.gone"), 1);

	printf("%s\n", fails ? "  FAILED" : "  every case holds");
	return fails != 0;
}
