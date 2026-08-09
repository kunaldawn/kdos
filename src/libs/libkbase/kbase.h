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
/* Same, but the child INHERITS stdin/stdout/stderr. A package build writes
 * straight to the build log, unbuffered and interleaved, and that is what the
 * per-port logs are. */
int kb_run_tty(const KbArgv *a);
/* Same, but copy up to n-1 bytes of stdout into buf, NUL terminated with any
 * trailing newline stripped. */
int kb_run_capture(const KbArgv *a, char *buf, size_t n);
/* Double-forked fire and forget: the caller can never block on the child or
 * collect a zombie. gdbus's default reply timeout is 25 seconds and a
 * notification must never be able to gate an app launch behind that. */
void kb_run_detach(const KbArgv *a);

/* ────────────────────────────────────────────────────────────────────────
 * Time
 * ──────────────────────────────────────────────────────────────────────── */

double kb_now_s(void);

#endif /* KBASE_H */
