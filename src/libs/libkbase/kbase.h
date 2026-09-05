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
#include <sys/types.h>

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

/*
 * THE PATH A KDOS BOX PRESENTS, and it has to be one string because TWO
 * programs set it for two different sets of processes: kdos-boxinit exports it
 * for pid 1's children, and kdos-appbox puts it in front of the command it
 * execs — `podman exec` does NOT inherit pid 1's environment, so a PATH set
 * only by boxinit leaves every `podman exec` with podman's own default.
 *
 * `/usr/games` is the reason the list is written out rather than left to the
 * container: Debian puts aisleriot, supertux, wesnoth and the rest there, and
 * without it the shim dies on `env: 'sol': No such file or directory` for
 * every game in the catalogue.
 */
#define KB_BOX_PATH \
	"/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin:" \
	"/usr/games:/usr/local/games"

int kb_write_file(const char *path, const char *data);
/* Replace a state file atomically: temp, fsync the file, rename, fsync the
 * directory. Use this wherever an empty file is a LOSS rather than a retry. */
int kb_write_file_atomic(const char *path, const char *data);
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
 * the freedesktop trash — one implementation, because `kdos-desk`'s Delete
 * key and `kdos trash` at a prompt must mean the same thing.
 *
 * Every call returns -1 on failure with errno as the syscall left it, so a
 * caller can say EXDEV ("it is on another filesystem") rather than "failed".
 */
#define KB_TRASH_PATH 1024
#define KB_TRASH_NAME 256

typedef struct {
	char name[KB_TRASH_NAME];	/* the name inside files/           */
	char orig[KB_TRASH_PATH];	/* where it came from, unescaped    */
	char when[32];			/* the record's DeletionDate        */
	int isdir;
	unsigned long long bytes;
} KbTrashItem;

int kb_trash_dirs(char *files, size_t fn, char *info, size_t in);
int kb_trash_put(const char *path);
int kb_trash_list(KbTrashItem **out);	/* count; caller free()s *out       */
int kb_trash_restore(const char *name, char *to, size_t tn);
int kb_trash_remove(const char *name);
int kb_trash_empty(void);		/* items removed, or -1             */

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

/* A virtual machine, by the DMI vendor string. The idle timers and the lid
 * policy both default to off here: a blanked screen over VNC cannot be told
 * from a crash, and a VM's lid event is a stray ACPI report. Misses a
 * hypervisor not on its list, which fails in the safe direction. */
int kb_in_vm(void);
char *kb_path_join(const char *a, const char *b);	/* malloc'd        */
int kb_mkdir_p(const char *path);

/* flock() wrapper. Returns the held fd, or -1. Close to release. */
int kb_lock_file(const char *path, int nonblock);

/*
 * IS A DESKTOP TOGGLE ON? `kdos toggle <name>` writes a flag file under
 * `$XDG_STATE_HOME/kdos/toggles/` and its presence is the whole state.
 *
 * A FLAG FILE RATHER THAN A CONFIGURATION KEY, because the configuration is
 * documented as read once when a session starts: a runtime writer would make
 * half a program's answers come from before an edit and half from after.
 *
 * STAT'ED PER CALL, NEVER CACHED. Every toggle is set by a different process —
 * a chord, a menu row, a script before a long build — so a program holding a
 * copy is one that has to be told, and there is nothing to tell it with.
 */
int kb_toggle_on(const char *name);

/*
 * THE FIRST NAME IN $XDG_CURRENT_DESKTOP, which is the prefix a desktop's own
 * `<desktop>-mimeapps.list` is spelled with — lowercased, because the variable
 * is `KDOS-Console:KDOS` and the file the spec asks for is
 * `kdos-console-mimeapps.list`.
 *
 * Returns 0 when the variable is unset or empty, and the caller then searches
 * only the plain lists: a machine with no desktop declared has no per-desktop
 * choices to honour, and inventing a prefix would look for a file nobody wrote.
 */
int kb_desktop_prefix(char *out, size_t n);

/*
 * WHICH TERMINAL A `Terminal=true` ENTRY IS RUN IN, and it follows the desktop:
 * `kdos-term` inside a console session, `foot` under the compositor. Both take
 * `-e`, so the name is the whole of the difference.
 *
 * IT IS NOT A PREFERENCE. `foot` is a Wayland client, so a console session that
 * wrapped an entry in it would resolve the right program and then fail to open
 * a window for it — which reads as the handler being wrong rather than the
 * terminal being unreachable.
 */
const char *kb_terminal(void);

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
/* Same as kb_run_feed, except the child keeps the caller's stdout and stderr.
 * That is the difference between feeding a CHECKER and feeding a PAGER: the
 * one place kb_run_feed is used sends the child's output to /dev/null, which
 * is right for kdos-checkpass and would make `kdos help --pager` render the
 * help text into nothing. Same reason kb_run_tty exists beside kb_run. */
int kb_run_feed_tty(const KbArgv *a, const char *in, size_t n);
/* Same, but the child INHERITS stdin/stdout/stderr. A package build writes
 * straight to the build log, unbuffered and interleaved, and that is what the
 * per-port logs are. */
int kb_run_tty(const KbArgv *a);
/* Same, but copy up to n-1 bytes of stdout into buf, NUL terminated with any
 * trailing newline stripped. */
int kb_run_capture(const KbArgv *a, char *buf, size_t n);

/*
 * A DESKTOP NOTIFICATION, over `gdbus`.
 *
 * Best effort and detached: a program that emitted OSC 9 has finished, and a
 * terminal that blocked raising a toast about it would be a terminal that
 * stopped drawing to say something had stopped. Nothing here links a bus
 * library — `gdbus` is on every image for the portal — and a machine without
 * it silently raises nothing, which is what a machine with no notification
 * daemon should do.
 */
void kb_notify(const char *app, const char *summary, const char *body);
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

/* Membership of a group in /etc/group, counting the group's own gid as well as
 * its member list. The authorisation both root daemons here are built on, in
 * one place: two copies of a security decision eventually disagree. */
int kb_user_in_group(const char *user, gid_t primary, const char *group);

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

/*
 * MD5 — a FILE NAME, never a security claim.
 *
 * The thumbnail standard names its cache files by the MD5 of the source URI,
 * and every other program on the machine that writes one does the same. A
 * stronger hash here would produce a cache nothing else could read and would
 * read nothing else's, in exchange for a property nothing here relies on:
 * `kb_sha256_*` is what authenticates.
 */
typedef struct {
	uint32_t h[4];
	uint64_t len;
	uint8_t buf[64];
	size_t n;
} KbMd5;

void kb_md5_init(KbMd5 *s);
void kb_md5_update(KbMd5 *s, const void *data, size_t n);
void kb_md5_final(KbMd5 *s, char out[33]);	/* lowercase hex */
void kb_md5_str(const char *s, char out[33]);

/*
 * A path as a `file://` URI. THE ESCAPE SET IS NOT A CHOICE: the thumbnail
 * cache is named by the MD5 of this string and the cache is SHARED, so a
 * character escaped differently is a thumbnail nothing else can find. The set
 * is glib's `G_URI_RESERVED_CHARS_ALLOWED_IN_PATH` plus the unreserved ones,
 * in uppercase hex, which is what `g_filename_to_uri()` writes.
 */
void kb_uri_file(const char *path, char *out, size_t n);

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

/*
 * Base64, decode only. Returns the byte count, or -1 when the input is not
 * base64 or would not fit — refused whole rather than partially decoded, so a
 * caller never pastes half a selection.
 */
int kb_b64_decode(const char *in, size_t inlen, char *out, size_t outsz,
		  size_t *outlen);

#endif /* KBASE_H */
