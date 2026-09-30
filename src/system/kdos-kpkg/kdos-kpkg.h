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

/* `==> ` on stdout, `ERROR: ` on stderr — the only two prefixes anything
 * downstream matches on, so neither string is free to change. */
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

/* `$var`, `${var}` and the parameter forms the recipes use — `${v#p}`,
 * `${v##p}`, `${v%p}`, `${v%%p}`, `${v/a/b}`, `${v//a/b}`. `p` may be NULL
 * when the four build paths are not known yet (metadata read at parse time). */
char *kp_decl_expand(const KpDecl *d, const char *text, const KpPaths *p);

/* The shell assignments a build.sh expects: the metadata keys plus every
 * recipe helper, each single-quoted. */
void kp_decl_prelude(const KpDecl *d, KbBuf *b);
void kp_decl_meta(const KpDecl *d);	/* the same, to stdout, for ports/fetch */

/* The shared indexes a manifest feeds (triggers.c). Note what an install
 * placed with _note and what it or a removal took off the disk with _gone —
 * for an upgrade, the new manifest and each orphan it removed — then run
 * once, after the database is written. _run releases what the notes kept. */
typedef struct {
	unsigned hit;
	unsigned gone;
	KbBuf man;	/* placed manual pages, NUL-separated, man-dir-relative */
	int nman;
} KpTriggers;

void kp_triggers_note(KpTriggers *t, const char *manifest);
void kp_triggers_gone(KpTriggers *t, const char *manifest);
void kp_triggers_run(KpTriggers *t, const char *root);

/*
 * The package compressor (build.c). Two pinned xz settings, one table:
 *
 *   kept       xz -9 -T0 --block-size=32MiB --no-adjust
 *   transient  xz -0 -T0 --block-size=8MiB --no-adjust
 *
 * The bytes xz writes depend on the preset and the block size, never on the
 * thread count: any -T of one or more in multi-threaded mode is the same
 * encoder, so -T0 is as reproducible as one core. -T1 is a DIFFERENT encoder
 * mode with different output and must never appear here. --no-adjust makes a
 * memory limit that would shrink the dictionary an error instead of a silently
 * different package. Both xz builds in the tree are threaded, which the
 * multi-threaded mode needs.
 *
 * `transient` is the cheap setting for a package kpkg install deletes the
 * moment kpkgadd has read it; it is set only by cmd_install (front.c) and only
 * while no cache is being kept. Every package anything else can read — a kept
 * cache, a binhost, `kpkg verify --repro`, a delta — is the kept setting.
 *
 * kp_xz_args() is a NULL-terminated argv; kp_xz_cmd() is the same words
 * space-separated, for tar's --use-compress-program. Every xz run goes between
 * kp_xz_env_hide() and kp_xz_env_restore(): XZ_OPT=-e or --check changes the
 * bytes, and an XZ_DEFAULTS memory limit fails the -9 run.
 */
extern int kp_pack_transient;
const char *const *kp_xz_args(int transient);
const char *kp_xz_cmd(int transient);
void kp_xz_env_hide(char *saved[2]);
void kp_xz_env_restore(char *saved[2]);

int depends_main(int argc, char **argv);
int add_main(int argc, char **argv);
int del_main(int argc, char **argv);
int build_main(int argc, char **argv);
/*
 * Build the port in `portdir` and, on success, copy the path of the package
 * file it wrote into `pkgout` (may be NULL). That path is the only way to name
 * the package: a scan of PACKAGE_DIR by `<name>-` prefix cannot tell
 * `binutils` from `binutils-avr` once the cache holds both. Returns 0 on
 * success; the working directory is restored either way.
 */
int kp_build_port(const char *portdir, char *pkgout, size_t cap);
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
