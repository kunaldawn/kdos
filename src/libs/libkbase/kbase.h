/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   libkbase — the pieces every KDOS program had written for itself
 *
 * Links nothing but libc. Everything here existed twice before: once in the
 * installer and once in kdos-appbox, with `read_file` meaning two different
 * things depending on which one you were looking at.
 * ---------------------------------
 */

#ifndef KBASE_H
#define KBASE_H

#include <stddef.h>
#include <stdint.h>

/* ────────────────────────────────────────────────────────────────────────
 * Allocation
 *
 * A library does not own the exit path. The application registers what has
 * to happen before the process dies — dropping the terminal, closing a log —
 * instead of kb_calloc knowing that it lives inside a program called
 * "kinstall" and that a `term_shutdown` exists to call.
 * ──────────────────────────────────────────────────────────────────────── */

typedef void (*kb_oom_fn)(void);

void kb_set_oom_handler(kb_oom_fn fn);

/* Prefixes kb_die/kb_warn and the OOM message. */
void kb_set_progname(const char *name);
const char *kb_progname(void);

void kb_die(const char *fmt, ...)
	__attribute__((noreturn, format(printf, 1, 2)));
void kb_warn(const char *fmt, ...) __attribute__((format(printf, 1, 2)));

void *kb_calloc(size_t n, size_t sz);
char *kb_strdup(const char *s);

/* ────────────────────────────────────────────────────────────────────────
 * Strings
 * ──────────────────────────────────────────────────────────────────────── */

void kb_strlcpy(char *d, const char *s, size_t n);
int kb_str_ieq(const char *a, const char *b);
const char *kb_basename(const char *p);

/* 1024-based, one decimal below 10. Returns one of eight rotating static
 * buffers, so several calls can appear in the same printf. */
const char *kb_human_size(unsigned long long bytes);

/* ────────────────────────────────────────────────────────────────────────
 * Files
 * ──────────────────────────────────────────────────────────────────────── */

/* Reads at most cap-1 bytes and NUL-terminates. Returns the byte count, or
 * -1. Short reads are not retried: every caller is a /sys or /proc file. */
int kb_read_file(const char *path, char *buf, size_t cap);

/* First line, newline stripped. Returns its length, or -1. */
int kb_read_line_file(const char *path, char *buf, size_t cap);

int kb_write_file(const char *path, const char *data);
int kb_path_exists(const char *p);
int kb_have_prog(const char *name);

/* ────────────────────────────────────────────────────────────────────────
 * A growable byte buffer
 *
 * Everything KDOS generates — stylesheets, index.theme, launcher tables — is
 * built by appending in one linear pass and written once. This is that, and
 * nothing cleverer.
 * ──────────────────────────────────────────────────────────────────────── */

typedef struct {
	char *p;
	size_t n, cap;
} KbBuf;

void kb_buf_add(KbBuf *b, const void *s, size_t n);
void kb_buf_str(KbBuf *b, const char *s);
void kb_buf_printf(KbBuf *b, const char *fmt, ...)
	__attribute__((format(printf, 2, 3)));
void kb_buf_free(KbBuf *b);

/*
 * Append `s` to `b` as a JSON string, quotes included.
 *
 * Every KDOS `--json` output funnels through this. Package descriptions, file
 * paths and window titles all reach it, and every one of them can contain a
 * quote, a backslash or a control character — which is exactly how a program
 * emits output that looks like JSON and does not parse. Escapes the two
 * mandatory characters, the five shorthand controls, and anything else below
 * 0x20 as \u00xx; bytes >= 0x80 pass through, because the input is already
 * UTF-8 and JSON strings are Unicode.
 */
void kb_json_str(KbBuf *b, const char *s);

/* Whole-file read. malloc'd and NUL-terminated; *len excludes the NUL so the
 * buffer is usable as both a string and a blob. NULL on error. */
char *kb_read_all(const char *path, size_t *len);
int kb_write_all(const char *path, const void *data, size_t len);

int kb_is_dir(const char *p);
int kb_is_link(const char *p);
int kb_copy_file(const char *src, const char *dst);
int kb_rmtree(const char *path);

/* Names in a directory, sorted, without "." and "..". Returns a malloc'd,
 * NULL-terminated vector of malloc'd names; NULL if the directory cannot be
 * read. Sorted because the generated icon theme has to come out the same on
 * every filesystem, and readdir order is not a promise. */
char **kb_listdir(const char *path, int *count);
void kb_strv_free(char **v);

/* ────────────────────────────────────────────────────────────────────────
 * tar
 *
 * Enough ustar to take the appbox image apart and put it back together. The
 * only archives this sees are `podman save` output — regular files, short
 * names, no devices, no hard links.
 * ──────────────────────────────────────────────────────────────────────── */

typedef struct {
	char name[512];
	long long size;
	char typeflag;
} KbTarEntry;

typedef struct {
	int fd;
	long long remain;	/* payload left in the current member      */
	int pad;		/* padding left after it                   */
} KbTarIn;

int kb_tar_open(KbTarIn *t, const char *path);
int kb_tar_next(KbTarIn *t, KbTarEntry *e);	/* 1 got one, 0 end, -1 err */
int kb_tar_read(KbTarIn *t, void *buf, size_t n);
void kb_tar_close(KbTarIn *t);

int kb_tar_put_header(int fd, const char *name, long long size);
int kb_tar_pad(int fd, long long size);	/* zero-fill to the block boundary */
int kb_tar_finish(int fd);		/* the two zero blocks that end it */

const char *kb_runtime_dir(void);	/* $XDG_RUNTIME_DIR, or /tmp       */
const char *kb_home_dir(void);		/* $HOME, or /root                 */
char *kb_path_join(const char *a, const char *b);	/* malloc'd        */
int kb_mkdir_p(const char *path);

/* flock() wrapper. Returns the held fd, or -1. Close to release. */
int kb_lock_file(const char *path, int nonblock);

/* ────────────────────────────────────────────────────────────────────────
 * Processes
 *
 * There is no system() and no popen() here, and no KDOS program is allowed
 * one. App names, package names, device paths and file arguments all reach
 * this code from .desktop files, /sys and argv; a shell in the middle turns
 * every one of them into an injection point. Everything execs through an
 * argv that was built element by element.
 * ──────────────────────────────────────────────────────────────────────── */

#define KB_MAX_ARGV 256

typedef struct {
	const char *v[KB_MAX_ARGV];
	int n;
} KbArgv;

void kb_argv_add(KbArgv *a, const char *s);
void kb_argv_addf(KbArgv *a, const char *fmt, ...)
	__attribute__((format(printf, 2, 3)));
void kb_argv_end(KbArgv *a);

/* When 0, a child's stderr goes to /dev/null. */
extern int kb_proc_verbose;

int kb_run(const KbArgv *a);		/* exec, wait, return exit status   */
/* Same, but `in` (n bytes) is written to the child's stdin, which then sees
 * EOF. A password belongs here and never in argv: /proc/<pid>/cmdline is
 * world-readable for the life of the process. SIGPIPE is blocked around the
 * write, so a child that exits early cannot kill the caller. */
int kb_run_feed(const KbArgv *a, const char *in, size_t n);
/* Same, but the child INHERITS stdin/stdout/stderr. A package build writes
 * straight to the build log, unbuffered and interleaved, and that is what the
 * per-port logs are. */
int kb_run_tty(const KbArgv *a);
/* Same, but copy up to n-1 bytes of stdout into buf, NUL terminated with any
 * trailing newline stripped. */
int kb_run_capture(const KbArgv *a, char *buf, size_t n);
/* Same, unbounded: stdout is appended to a growing buffer, NUL terminated,
 * with the trailing newline left alone. Use this whenever the output has no
 * natural ceiling — a `tar -tf` listing does not. */
int kb_run_capture_buf(const KbArgv *a, KbBuf *out);
/* Same, but stdout is written to `path` (created 0644, truncated). The `>` a
 * shell would provide, without the shell. */
int kb_run_to_file(const KbArgv *a, const char *path);
/* Double-forked fire and forget: the caller can never block on the child or
 * collect a zombie. gdbus's default reply timeout is 25 seconds and a
 * notification must never be able to gate an app launch behind that. */
void kb_run_detach(const KbArgv *a);

/* ────────────────────────────────────────────────────────────────────────
 * Time
 * ──────────────────────────────────────────────────────────────────────── */

double kb_now_s(void);

/* ────────────────────────────────────────────────────────────────────────
 * SHA-256 (FIPS 180-4)
 *
 * For `sha256 =` in a recipe: kpkg has to check an archive before extracting
 * it, and kpkg links libkbase and nothing else.
 *
 * A hash, not a signature — it proves the bytes are the bytes the recipe
 * named, and nothing about who named them.
 * ──────────────────────────────────────────────────────────────────────── */

typedef struct {
	uint32_t h[8];
	uint64_t len;
	uint8_t buf[64];
	size_t n;
} KbSha256;

void kb_sha256_init(KbSha256 *s);
void kb_sha256_update(KbSha256 *s, const void *data, size_t n);
void kb_sha256_final(KbSha256 *s, char out[65]);	/* lowercase hex */

/* Streamed, so a 552 MB tarball costs one 64 K buffer. -1 on read error. */
int kb_sha256_file(const char *path, char out[65]);
/* 0 match, 1 mismatch, -1 unreadable. Comparison is case-insensitive. */
int kb_sha256_check(const char *path, const char *want);

/* ────────────────────────────────────────────────────────────────────────
 * Landlock — unprivileged self-sandboxing. Three syscalls, no library.
 *
 * Deny by default: a ruleset starts with NOTHING reachable, and each
 * kb_landlock_allow() opens one subtree back up. Enforcement is one-way and
 * inherited by every child — there is no unsandbox.
 *
 * Usage is fixed: new() -> allow()* -> enforce() -> exec(). Everything after
 * enforce() runs inside.
 * ──────────────────────────────────────────────────────────────────────── */

typedef struct {
	int fd;			/* ruleset fd, -1 once enforced or freed */
	int abi;		/* what the RUNNING kernel supports        */
	int nrules;
	int net_handled;	/* TCP is being policed at all             */
} KbLandlock;

/* ABI version the kernel reports, or -errno. -ENOSYS: no Landlock in this
 * kernel. -EOPNOTSUPP: compiled in but not enabled in CONFIG_LSM or lsm=,
 * which is the quiet failure worth naming — everything degrades to no
 * sandbox and nothing says so. */
int kb_landlock_abi(void);

/* Build a ruleset covering everything this ABI can police. net_off also
 * denies TCP bind and connect (needs ABI >= 4; silently not applied below
 * that, which kb_landlock_explain must report as unenforced). */
int kb_landlock_new(KbLandlock *ll, int net_off);

/* Allow one subtree, read-only or read-write. A missing path is -ENOENT and
 * is the caller's to decide about. */
int kb_landlock_allow(KbLandlock *ll, const char *path, int write);
int kb_landlock_allow_tcp(KbLandlock *ll, uint16_t port, int connect);

/* Sets PR_SET_NO_NEW_PRIVS then restricts. Irreversible. */
int kb_landlock_enforce(KbLandlock *ll);
void kb_landlock_free(KbLandlock *ll);

#endif /* KBASE_H */
