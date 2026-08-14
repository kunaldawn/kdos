/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   kdos-kpkg — one binary, five names
 *
 * kpkg, kpkgadd, kpkgbuild, kpkgdel and kpkgdepends are the same executable
 * dispatched on its own basename. The names are unchanged: 393 recipes, the
 * build orchestrator, the test runner and a decade of muscle memory all call
 * them by those names.
 * ---------------------------------
 */

#ifndef KDOS_KPKG_H
#define KDOS_KPKG_H

#include "kbase.h"
#include "kpkg.h"
#include "ksig.h"

/* `==> ` on stdout, `ERROR: ` on stderr — the two the shell version had, and
 * the only two anything downstream has ever seen. */
void kp_msg(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
void kp_err(const char *fmt, ...) __attribute__((format(printf, 1, 2)));

/* The five paths a recipe is built against. */
typedef struct {
	const char *port;	/* $PORT_SRC — the port directory          */
	const char *src_root;	/* $SRC_ROOT                               */
	const char *src;	/* $SRC — cwd for every build step         */
	const char *pkg;	/* $PKG — the staging tree                 */
} KpPaths;

typedef struct KpDecl KpDecl;

KpDecl *kp_decl_parse(const char *path);
void kp_decl_free(KpDecl *d);
const char *kp_decl_name(const KpDecl *d);
const char *kp_decl_version(const KpDecl *d);
const char *kp_decl_release(const KpDecl *d);
const char *kp_decl_source(const KpDecl *d);
const char *kp_decl_sha256(const KpDecl *d);
const char *kp_decl_description(const KpDecl *d);
const char *kp_decl_depends(const KpDecl *d);

/* `$var`, `${var}` and the parameter forms the recipes use — `${v#p}`,
 * `${v##p}`, `${v%p}`, `${v%%p}`, `${v/a/b}`, `${v//a/b}`. `p` may be NULL
 * when the four build paths are not known yet (metadata read at parse time). */
char *kp_decl_expand(const KpDecl *d, const char *text, const KpPaths *p);

/* The shell assignments a build.sh expects: the metadata keys plus every
 * recipe helper, each single-quoted. */
void kp_decl_prelude(const KpDecl *d, KbBuf *b);
void kp_decl_meta(const KpDecl *d);	/* the same, to stdout, for ports/fetch */

int depends_main(int argc, char **argv);
int add_main(int argc, char **argv);
int del_main(int argc, char **argv);
int build_main(int argc, char **argv);
int front_main(int argc, char **argv);

/* ────────────────────────────────────────────────────────────────────────
 * The binary repository (binhost.c)
 * ──────────────────────────────────────────────────────────────────────── */

/* No index is ever this large — 396 ports plus room. The cap exists so the
 * parser cannot be made to allocate by a hostile index. */
#define KP_MAX_INDEX 4096

int kp_cmd_index(const KpConf *c, int argc, char **argv);
int kp_cmd_keygen(int argc, char **argv);
int kp_cmd_verify_index(int argc, char **argv);
int kp_cmd_binhost(const KpConf *c, int argc, char **argv);
int kp_cmd_verify_pkg(int argc, char **argv);

/* Deltas (delta.c). Made and applied over the UNCOMPRESSED tars — two .tar.xz
 * files share almost no bytes even when their contents are nearly identical. */
int kp_delta_make(const char *oldpkg, const char *newpkg, const char *out);
int kp_delta_apply(const char *oldpkg, const char *delta, const char *out);
int kp_cmd_delta(int argc, char **argv);
int kp_cmd_apply_delta(int argc, char **argv);

/*
 * Check `<path>.sig` against the trusted keys. Returns 0 verified, 1 "there is
 * nothing to check" (no sidecar, or no keyring, and `required` is 0), -1 a
 * signature that does not verify — which is never the same as an absent one.
 */
int kp_verify_package(const char *path, int required, char who[KSIG_ID_HEX]);

#endif /* KDOS_KPKG_H */
