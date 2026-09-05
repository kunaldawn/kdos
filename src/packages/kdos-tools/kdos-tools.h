/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   kdos-tools — the target's own commands
 *
 * One binary, dispatched on its own basename the way busybox does, installed
 * under every name it answers to. These were nine bash scripts.
 * ---------------------------------
 */

#ifndef KDOS_TOOLS_H
#define KDOS_TOOLS_H

#include <stdarg.h>

#include "kbase.h"
#include "kcolor.h"

/* Overridable at compile time so the service logic can be exercised outside a
 * booted system. Not an environment hook: these are paths a root tool builds
 * from, and nothing that runs as root should take them from the caller. */
#ifndef INIT_DIR
#define INIT_DIR     "/etc/init.d"
#endif
#ifndef DISABLED_DIR
#define DISABLED_DIR "/etc/service.disabled"
#endif
#ifndef RUN_DIR
#define RUN_DIR      "/run"
#endif

int kdos_main(int argc, char **argv);		/* kdos                     */
int ksvc_main(int argc, char **argv);		/* service, ksvc            */
int getty_main(int argc, char **argv);		/* kdos-getty               */
/* A/B root slots (bootctl.c). The initramfs runs `select`, the end of rcS runs
 * `mark-good`, and everything else is administration. */
int bootctl_main(int argc, char **argv);	/* kdos-bootctl             */

/* `kdos rebuild` (rebuild.c): the stick rebuilding the stick. Finds the sources
 * an ISO built with KDOS_ISO_SOURCES=1 carries, checks the machine can do the
 * work, and hands over to kdosbuild. */
int rebuild_main(int argc, char **argv);

/*
 * `kdos march` (march.c): build a port both ways, benchmark it, and keep the
 * flags only where the win beat the noise. A port with no `bench =` line in its
 * recipe is unmeasurable and is never counted as a winner.
 */
int march_main(int argc, char **argv);

/*
 * `kdos clone` (clone.c): the stick writes the stick. A raw copy of the boot
 * medium onto another device, verified by reading it back — so the boot
 * arrangement is whatever the medium already carries and there is no second
 * opinion about how a KDOS stick is laid out.
 */
int clone_main(int argc, char **argv);
int shot_main(int argc, char **argv);		/* kdos-shot                */
int banner_main(int argc, char **argv);		/* kdos-banner              */
int fetch_app_main(int argc, char **argv);	/* kdos-fetch-app           */
int fetch_static_main(int argc, char **argv);	/* kdos-fetch-static        */
int sandbox_main(int argc, char **argv);	/* kdos sandbox             */
/*
 * The switches a desktop needs at hand, as flag files under
 * ~/.local/state/kdos/toggles/. Present means on; see toggle.c for why they
 * are state rather than configuration keys.
 */
int cmd_toggle(int argc, char **argv);
/* A toast is `kb_notify()` in libkbase — one sender for the whole tree, so a
 * terminal's OSC 9 and `kdos notify` cannot drift apart. `kdos-notify` is the
 * notification CENTRE, a viewer, and is not the sender. */
int kdt_toggle_on(const char *name);

int why_main(int argc, char **argv);		/* kdos why                 */
int explain_main(int argc, char **argv);		/* kdos explain             */
int appid_main(int argc, char **argv);		/* kdos appid               */
int restarts_main(int argc, char **argv);	/* kdos restarts            */
int stutter_main(int argc, char **argv);	/* kdos stutter             */
int oracle_main(int argc, char **argv);		/* kdos oracle              */
/* `kdos hey` (hey.c): the compositor answering questions from a prompt, over
 * $XDG_RUNTIME_DIR/kdos-cmd.sock. */
int hey_main(int argc, char **argv);
/* The app_ids of the windows open right now, deduplicated: `kdos appid`'s
 * fallback when no ledger has been recorded yet. -1 when the compositor is
 * not reachable, which is NOT the same answer as 0. See hey.c. */
int hey_app_ids(char ***out);
/* kdos-sfx (sfx.c): four synthesized noises. Its own basename, because the
 * things that play them are init scripts and keybinds, not `kdos` users. */
int sfx_main(int argc, char **argv);

/*
 * The reasons corpus, shared by `kdos why`, `kdos explain` and `kdos oracle`.
 * $KDOS_REASONS beats the installed copy, which is how the tree can be queried
 * from a checkout and how preflight reads it.
 */
const char *kdt_reason_dir(void);
char *kdt_reason_header(const char *text, const char *key, int nth);

/* "the aphorism · the scar", picked by (day XOR pid). NULL when nothing is
 * installed. The caller frees. */
char *kdt_oracle_line(void);

/* The accent name in $XDG_CACHE_HOME/kdos/theme, or "phosphor". Points at a
 * static buffer. */
const char *kdt_current_accent(void);

/*
 * `kdos update` (update.c). Orchestration of kpkg, kdos-bootctl and the theme
 * generators — no new package format and no new trust path. `theme` is handed
 * kdos.c's own `kdos theme` entry point rather than reimplementing it.
 */
int kdt_update(int argc, char **argv, int (*theme)(int, char **));

/* $XDG_CONFIG_HOME/<rest>, falling back to ~/.config. Shared rather than
 * copied: two XDG helpers in one binary is how the two drift apart. */
char *kdt_cfg_home(const char *rest);
char *kdt_data_home(const char *rest);

/*
 * `kdos theme --audit` (themeaudit.c). Regenerates every themed artefact into a
 * scratch $HOME with `apply` and compares it byte for byte with what is
 * installed — the palette claim, checked rather than asserted. Returns 0 when
 * everything matches, 1 on drift, 2 when the audit itself could not run.
 */
int kdt_theme_audit(const KcolScheme *sc, void (*apply)(const KcolScheme *),
		    const char *tty_accent, const char *tty_reset);

/*
 * `kdos cve` (cve.c). Compares what is pinned or installed against the vendored
 * Alpine security database — offline, by version comparison, never by scanning
 * a binary. 0 clean, 1 something is behind a recorded fix, 2 no database.
 */
int kdt_cve(int argc, char **argv, const char *tty_accent,
	    const char *tty_warn, const char *tty_reset);

/*
 * `kdos app` (app.c). The applications this machine has and the ones on the
 * medium beside it — a handful of verbs over kdos-packd, not a store. Every
 * verb hands the daemon an id out of the list the daemon published.
 */
int kdt_app(int argc, char **argv);
int kdt_trash(int argc, char **argv);
int kdt_places(int argc, char **argv);
int kdt_thumb(int argc, char **argv);

/* $XDG_CACHE_HOME/<rest>, and the parent of a path. Shared because `kdos
 * theme` and `kdos thumb` write into the same cache root, and two answers to
 * where that is would put one program's files where nothing else looks. */
char *kdt_cache_home(const char *rest);
void kdt_mkparent(const char *path);

#endif /* KDOS_TOOLS_H */
