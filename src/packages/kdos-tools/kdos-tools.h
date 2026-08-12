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
int shot_main(int argc, char **argv);		/* kdos-shot                */
int banner_main(int argc, char **argv);		/* kdos-banner              */
int fetch_app_main(int argc, char **argv);	/* kdos-fetch-app           */
int fetch_static_main(int argc, char **argv);	/* kdos-fetch-static        */
int sandbox_main(int argc, char **argv);	/* kdos sandbox             */
int why_main(int argc, char **argv);		/* kdos why                 */
int explain_main(int argc, char **argv);		/* kdos explain             */
int appid_main(int argc, char **argv);		/* kdos appid               */
int restarts_main(int argc, char **argv);	/* kdos restarts            */
int stutter_main(int argc, char **argv);	/* kdos stutter             */

/* $XDG_CONFIG_HOME/<rest>, falling back to ~/.config. Shared rather than
 * copied: two XDG helpers in one binary is how the two drift apart. */
char *kdt_cfg_home(const char *rest);

/*
 * `kdos theme --audit` (themeaudit.c). Regenerates every themed artefact into a
 * scratch $HOME with `apply` and compares it byte for byte with what is
 * installed — the palette claim, checked rather than asserted. Returns 0 when
 * everything matches, 1 on drift, 2 when the audit itself could not run.
 */
int kdt_theme_audit(const KcolScheme *sc, void (*apply)(const KcolScheme *),
		    const char *tty_accent, const char *tty_reset);

#endif /* KDOS_TOOLS_H */
