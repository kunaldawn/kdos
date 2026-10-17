/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   the staged tree, tidied and checked before it is rolled
 *
 * kp_pkg_python() and kp_pkg_lint() run on $PKG after build.sh and before the
 * archive is written (build.c). The first changes what the package holds; the
 * second only reads it, and a failure refuses the package: nothing it finds
 * can be repaired after the tree is installed, because every one is a fact
 * about the files a user's machine receives.
 *
 * What the lint FAILS on:
 *
 *   layout    a payload path outside bin boot etc lib sbin usr var, or under a
 *             denied prefix (LAYOUT below). `usr/lib64` and `lib64` are denied
 *             because the package database folds only top-level `usr/<same
 *             name>` aliases (kp_canon_load): with `usr/lib64 -> lib`,
 *             `./usr/lib64/x` and `./usr/lib/x` would be two ownership keys
 *             for one file.
 *   modes     a regular file without o+r that is neither setuid nor setgid; a
 *             regular file under a bin directory with no x bit; a non-sticky
 *             o+w path; a `*.orig` or `*.rej` file. Each outside its
 *             allowlist below.
 *   pkg-config  a `.pc` with an empty Version, `/usr/local` or
 *             `/var/cache/kpkg` in it, or the tree LDFLAGS in a Libs line; and
 *             any module `pkgconf --cflags --libs` cannot resolve as uid 65534.
 *   ELF       an x86-64 executable or shared object with an executable stack,
 *             a TEXTREL, or an RPATH/RUNPATH entry under /var/cache, /workspace
 *             or /tmp or neither absolute nor $ORIGIN-based; any ELF that names
 *             `/usr/etc` or `/usr/var`.
 *
 * What it WARNS on, one line per kind with a count and the first paths:
 * group-writable paths outside setgid and /var/games, an object without
 * BIND_NOW, a plain /usr/lib RPATH entry, and an empty RPATH component.
 *
 * KP_STORE_FORMAT (kpkg.h) moves with any change here that changes what a
 * package contains; a lint rule that only refuses does not move it.
 * ---------------------------------
 */

#include <elf.h>
#include <errno.h>
#include <fcntl.h>
#include <fnmatch.h>
#include <ftw.h>
#include <grp.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/wait.h>

#include "kdos-kpkg.h"

/* ── Allowlists ─────────────────────────────────────────────────────────
 *
 * Paths are canonical: relative to the package root, with `bin/`, `sbin/`
 * and `lib/` read as their `usr/` names. Patterns are fnmatch(3) without
 * FNM_PATHNAME, so `*` crosses directories.
 */

/* Unreadable to users by design: credentials, and files whose own program
 * refuses to start when others can read them. */
static const char *const ALLOW_UNREADABLE[] = {
	"etc/shadow*",
	"etc/gshadow*",
	"etc/sudoers",
	"etc/sudoers.dist",
	"etc/sudoers.d/*",
	"etc/ppp/*-secrets",
	"etc/cups/*.conf*",
	"usr/lib/cups/backend/*",
	"etc/freeipmi/*.conf",
	"etc/fwupd/fwupd.conf",
	"etc/mumble/mumble-server.ini",
	"etc/skel/.msmtprc",
	"etc/skel/.mbsyncrc",
	NULL
};

/* Not programs, though upstream installs them beside its programs: modules
 * those programs import or source, and data they read. */
static const char *const ALLOW_NOEXEC[] = {
	"usr/bin/icebox.py",		/* icestorm: imported by icebox_*      */
	"usr/bin/iceboxdb.py",		/* icestorm: imported by icebox_*      */
	"usr/bin/onelab.py",		/* gmsh: the ONELAB client module      */
	"usr/bin/brltty-prologue.*",	/* brltty: sourced by its tools        */
	"usr/bin/brltty-config.sh",	/* brltty: sourced by its tools        */
	"usr/bin/sdcdb*.el",		/* sdcc: Emacs Lisp for sdcdb          */
	"usr/bin/ALLCALL7.TXT",		/* wsjtx: read beside its programs     */
	"usr/bin/sounds/*",		/* wsjtx: read beside its programs     */
	NULL
};

/* World-writable on purpose. tetris-bsd is not setgid, so its score file
 * must be writable by every player or no score is ever saved. */
static const char *const ALLOW_WORLD_WRITABLE[] = {
	"var/games/tetris-bsd.scores",
	NULL
};

/* `*.orig` files that are upstream content, not patch leftovers: a published
 * crate's and an npm package's manifest as written before normalisation.
 * Cargo lists Cargo.toml.orig in each vendored crate's checksum file, so
 * removing one breaks every build against that vendor directory. */
static const char *const ALLOW_ORIG[] = {
	"*/Cargo.toml.orig",
	"*/package.json.orig",
	NULL
};

/* ELF files that name `/usr/etc` as something other than a sysconfdir. */
static const char *const ALLOW_USR_ETC[] = {
	"usr/bin/whereis",		/* one entry of its search list        */
	"usr/sbin/useradd",		/* --usr-skel's default vendor skel    */
	"usr/bin/orca-slicer",		/* a path on the printer, sent to it   */
	NULL
};

/* Payload layout. The first matching prefix decides; a top-level directory
 * that no entry names is outside the layout. `kdos` marks a prefix that the
 * KDOS programs (kdos-*) may use and no port may. */
static const struct {
	const char *prefix;
	int allow;
	int kdos;
	const char *why;
} LAYOUT[] = {
	{ "usr/lib64", 0, 0, "the gcc runtime directory is usr/lib" },
	{ "lib64", 0, 0, "the gcc runtime directory is usr/lib" },
	{ "usr/etc", 0, 0, "configuration lives in /etc (--sysconfdir=/etc)" },
	{ "usr/var", 0, 0, "state lives in /var (--localstatedir=/var)" },
	{ "usr/doc", 0, 0, "documentation lives in /usr/share/doc" },
	{ "usr/com", 0, 0, "shared state lives in /var/lib" },
	{ "usr/local", 0, 1, "/usr/local belongs to the machine's owner" },
	{ "usr/lib/systemd", 0, 0, "KDOS has no systemd" },
	{ "lib/systemd", 0, 0, "KDOS has no systemd" },
	{ "etc/X11", 0, 1, "KDOS has no X server configuration" },
	{ "etc/grub.d", 0, 0, "KDOS boots with Limine, not GRUB" },
	{ "etc/bash_completion.d",
	  0, 0, "completions live in /usr/share/bash-completion/completions" },
	{ "var/run", 0, 1, "/var/run is the runtime tmpfs" },
	{ "run", 0, 1, "/run is a tmpfs: anything staged here is lost" },
	{ "usr", 1, 0, NULL },
	{ "etc", 1, 0, NULL },
	{ "var", 1, 0, NULL },
	{ "boot", 1, 0, NULL },
	{ "bin", 1, 0, NULL },
	{ "sbin", 1, 0, NULL },
	{ "lib", 1, 0, NULL },
	{ NULL, 0, 0, NULL }
};

/* The tree LDFLAGS (script/env/chroot.env) a `.pc` must not hand to every
 * consumer's link line. */
static const char *const PC_LDFLAGS[] = {
	"--sort-common", "-z,now", "pack-relative-relocs", NULL
};

/* ── Reporting ─────────────────────────────────────────────────────────── */

#define LINT_SHOW 20	/* failures printed per run before "and N more" */
#define WARN_SHOW 5	/* paths printed per warning kind               */

enum { W_GROUP, W_BINDNOW, W_USRLIB, W_EMPTY, W_N };

static const char *const WARN_WHAT[W_N] = {
	"group-writable outside setgid and /var/games",
	"no BIND_NOW (-Wl,-z,now)",
	"a redundant /usr/lib RPATH entry",
	"an empty RPATH component",
};

typedef struct {
	const char *pkg;
	size_t pkglen;
	const char *name;
	int kdos;
	int fails;
	int nwarn[W_N];
	char *warn[W_N][WARN_SHOW];
	char lastdeny[1024];
	KbBuf pcdirs;	/* ':'-joined directories in $PKG holding a .pc */
	KbBuf pcmods;	/* NUL-separated module names                   */
	int npc;
} Lint;

static Lint *L;

static void fail(const char *fmt, ...) __attribute__((format(printf, 1, 2)));

static void fail(const char *fmt, ...)
{
	if (L->fails++ >= LINT_SHOW)
		return;
	va_list ap;
	fputs("ERROR: lint: ", stderr);
	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);
	fputc('\n', stderr);
}

static void note(int kind, const char *path)
{
	int n = L->nwarn[kind]++;
	if (n < WARN_SHOW)
		L->warn[kind][n] = kb_strdup(path);
}

static int matches(const char *const *list, const char *path)
{
	for (int i = 0; list[i]; i++)
		if (!fnmatch(list[i], path, 0))
			return 1;
	return 0;
}

static int under(const char *path, const char *prefix)
{
	size_t n = strlen(prefix);
	return !strncmp(path, prefix, n) && (path[n] == '/' || !path[n]);
}

/* `bin/x`, `sbin/x` and `lib/x` are the merged-/usr names of `usr/...`. */
static void canon(const char *rel, char *out, size_t cap)
{
	if (under(rel, "bin") || under(rel, "sbin") || under(rel, "lib"))
		snprintf(out, cap, "usr/%s", rel);
	else
		kb_strlcpy(out, rel, cap);
}

/* ── Layout ───────────────────────────────────────────────────────────── */

static void check_layout(const char *rel)
{
	/* One report per denied directory, not one per file under it. */
	if (L->lastdeny[0] && under(rel, L->lastdeny))
		return;
	for (int i = 0; LAYOUT[i].prefix; i++) {
		if (!under(rel, LAYOUT[i].prefix))
			continue;
		if (LAYOUT[i].allow || (LAYOUT[i].kdos && L->kdos))
			return;
		fail("%s: outside the layout: %s", rel, LAYOUT[i].why);
		kb_strlcpy(L->lastdeny, LAYOUT[i].prefix, sizeof(L->lastdeny));
		return;
	}
	char top[256];
	kb_strlcpy(top, rel, sizeof(top));
	char *s = strchr(top, '/');
	if (s)
		*s = 0;
	fail("%s: outside the layout: /%s is not a payload directory", rel, top);
	kb_strlcpy(L->lastdeny, top, sizeof(L->lastdeny));
}

/* ── Modes ────────────────────────────────────────────────────────────── */

static void check_mode(const char *rel, const char *cpath, const struct stat *st)
{
	mode_t m = st->st_mode;

	if (S_ISREG(m) && !(m & S_IROTH) && !(m & (S_ISUID | S_ISGID)) &&
	    !matches(ALLOW_UNREADABLE, cpath))
		fail("%s: mode %04o: not readable by users", rel, m & 07777);

	if (S_ISREG(m) && !(m & 0111) &&
	    (under(cpath, "usr/bin") || under(cpath, "usr/sbin")) &&
	    !matches(ALLOW_NOEXEC, cpath))
		fail("%s: mode %04o: a program with no x bit", rel, m & 07777);

	if (S_ISLNK(m))
		return;

	if ((m & S_IWOTH) && !(m & S_ISVTX) &&
	    !matches(ALLOW_WORLD_WRITABLE, cpath))
		fail("%s: mode %04o: world-writable", rel, m & 07777);

	if ((m & S_IWGRP) && !(m & S_ISGID) && !under(cpath, "var/games"))
		note(W_GROUP, rel);

	if (!S_ISDIR(m)) {
		const char *b = kb_basename(rel);
		size_t n = strlen(b);
		if (((n > 5 && !strcmp(b + n - 5, ".orig")) ||
		     (n > 4 && !strcmp(b + n - 4, ".rej"))) &&
		    !matches(ALLOW_ORIG, cpath))
			fail("%s: a patch leftover", rel);
	}
}

/* ── pkg-config ───────────────────────────────────────────────────────── */

static void check_pc(const char *path, const char *rel)
{
	size_t len = 0;
	char *body = kb_read_all(path, &len);
	if (!body)
		return;	/* unreadable: check_mode has already said so */

	int version = 0;
	for (char *line = body, *next; line; line = next) {
		next = strchr(line, '\n');
		if (next)
			*next++ = 0;
		if (!strncmp(line, "Version:", 8)) {
			const char *v = line + 8;
			while (*v == ' ' || *v == '\t')
				v++;
			if (!*v)
				fail("%s: an empty Version", rel);
			version = 1;
		}
		if (strstr(line, "/usr/local"))
			fail("%s: names /usr/local: %s", rel, line);
		/* A prefix map points AWAY from the work directory, so it
		 * names it on purpose and leaks nothing. */
		const char *w = strstr(line, "/var/cache/kpkg");
		if (w && !(w > line && w[-1] == '=' && strstr(line, "prefix-map=")))
			fail("%s: names the build tree: %s", rel, line);
		if (!strncmp(line, "Libs", 4))
			for (int i = 0; PC_LDFLAGS[i]; i++)
				if (strstr(line, PC_LDFLAGS[i])) {
					fail("%s: the tree LDFLAGS (%s) in %s",
					     rel, PC_LDFLAGS[i], line);
					break;
				}
	}
	if (!version)
		fail("%s: no Version", rel);
	free(body);

	/* The module name, and its directory for PKG_CONFIG_PATH. */
	const char *b = kb_basename(path);
	kb_buf_add(&L->pcmods, b, strlen(b) - 3);
	kb_buf_add(&L->pcmods, "", 1);
	L->npc++;

	char dir[4096];
	kb_strlcpy(dir, path, sizeof(dir));
	*strrchr(dir, '/') = 0;
	char needle[4100];
	snprintf(needle, sizeof(needle), ":%s:", dir);
	char *have = L->pcdirs.p ? strstr(L->pcdirs.p, needle) : NULL;
	if (!have) {
		if (!L->pcdirs.n)
			kb_buf_str(&L->pcdirs, ":");
		kb_buf_str(&L->pcdirs, dir);
		kb_buf_str(&L->pcdirs, ":");
	}
}

/*
 * Every module this package ships must resolve for an ordinary user, with its
 * own directories searched first. Run as uid 65534 when kpkg is root, so a
 * file the package or one of its Requires left unreadable fails here rather
 * than for every user's build. A $PKG uid 65534 cannot reach (a WORK_DIR under
 * a private home) is checked as root instead; check_mode still reads its
 * modes. Exit 126 is that case: pkgconf itself exits 0 or 1.
 */
static int run_pkgconf(const char *m, const char *path, int as_user)
{
	fflush(stdout);
	fflush(stderr);
	pid_t pid = fork();
	if (pid < 0)
		return -1;
	if (pid == 0) {
		setenv("PKG_CONFIG_PATH", path, 1);
		unsetenv("PKG_CONFIG_SYSROOT_DIR");
		unsetenv("PKG_CONFIG_LIBDIR");
		if (as_user) {
			int ok = setgroups(0, NULL) == 0 && setgid(65534) == 0 &&
				 setuid(65534) == 0;
			if (!ok || access(L->pkg, R_OK | X_OK) != 0)
				_exit(126);
		}
		int null = open("/dev/null", O_WRONLY);
		if (null >= 0)
			dup2(null, 1);
		execlp("pkgconf", "pkgconf", "--cflags", "--libs", m, (char *)NULL);
		_exit(127);
	}
	int st = 0;
	while (waitpid(pid, &st, 0) < 0 && errno == EINTR)
		;
	return WIFEXITED(st) ? WEXITSTATUS(st) : -1;
}

static void resolve_pc(void)
{
	if (!L->npc)
		return;
	if (!kb_have_prog("pkgconf")) {
		kp_msg("lint: no pkgconf yet; %d pkg-config module(s) not resolved",
		       L->npc);
		return;
	}

	KbBuf path = {0};
	/* pcdirs is ":a:b:"; the outer colons are dropped. */
	kb_buf_add(&path, L->pcdirs.p + 1, L->pcdirs.n - 2);
	const char *old = getenv("PKG_CONFIG_PATH");
	if (old && *old) {
		kb_buf_str(&path, ":");
		kb_buf_str(&path, old);
	}

	int as_user = geteuid() == 0;
	for (const char *m = L->pcmods.p; m < L->pcmods.p + L->pcmods.n;
	     m += strlen(m) + 1) {
		int rc = run_pkgconf(m, path.p, as_user);
		if (rc == 126 && as_user) {
			as_user = 0;
			rc = run_pkgconf(m, path.p, 0);
		}
		if (rc != 0)
			fail("pkg-config module %s does not resolve for a user "
			     "(pkgconf --cflags --libs %s)", m, m);
	}
	kb_buf_free(&path);
}

/* ── ELF ──────────────────────────────────────────────────────────────── */

static int has_word(const unsigned char *p, size_t n, const char *s)
{
	size_t k = strlen(s);
	const unsigned char *q = p, *end = p + n;
	while ((q = memmem(q, (size_t)(end - q), s, k))) {
		const unsigned char *after = q + k;
		if (after >= end || *after == '/' || *after == 0)
			return 1;
		q++;
	}
	return 0;
}

/* The file offset of a virtual address, through the PT_LOAD that maps it. */
static long vaddr_off(const unsigned char *f, size_t n, const Elf64_Ehdr *e,
		      Elf64_Addr va)
{
	for (int i = 0; i < e->e_phnum; i++) {
		const Elf64_Phdr *ph =
			(const Elf64_Phdr *)(f + e->e_phoff + (size_t)i * sizeof(*ph));
		if (ph->p_type == PT_LOAD && va >= ph->p_vaddr &&
		    va < ph->p_vaddr + ph->p_filesz) {
			Elf64_Off o = ph->p_offset + (va - ph->p_vaddr);
			return o < n ? (long)o : -1;
		}
	}
	return -1;
}

static void check_rpath(const char *rel, const char *rp)
{
	char buf[4096];
	kb_strlcpy(buf, rp, sizeof(buf));
	char *s = buf;
	int empty = 0;
	for (;;) {
		char *colon = strchr(s, ':');
		if (colon)
			*colon = 0;
		if (!*s) {
			if (!empty++)
				note(W_EMPTY, rel);
		}
		else if (!strncmp(s, "$ORIGIN", 7) || !strncmp(s, "${ORIGIN}", 9))
			;
		else if (*s != '/')
			fail("%s: RPATH entry '%s' is relative to the working "
			     "directory", rel, s);
		else if (under(s, "/var/cache") || under(s, "/workspace") ||
			 under(s, "/tmp"))
			fail("%s: RPATH entry '%s' points into a build tree",
			     rel, s);
		else if (!strcmp(s, "/usr/lib") || !strcmp(s, "/usr/lib/") ||
			 !strcmp(s, "/lib"))
			note(W_USRLIB, rel);
		if (!colon)
			break;
		s = colon + 1;
	}
}

static void check_elf(const char *path, const char *rel, const char *cpath,
		      off_t size)
{
	if (size < (off_t)sizeof(Elf64_Ehdr))
		return;
	int fd = open(path, O_RDONLY | O_CLOEXEC);
	if (fd < 0)
		return;
	unsigned char magic[4];
	if (read(fd, magic, 4) != 4 || memcmp(magic, ELFMAG, SELFMAG)) {
		close(fd);
		return;
	}
	size_t n = (size_t)size;
	const unsigned char *f = mmap(NULL, n, PROT_READ, MAP_PRIVATE, fd, 0);
	close(fd);
	if (f == MAP_FAILED)
		return;

	if ((has_word(f, n, "/usr/etc") || has_word(f, n, "/usr/var")) &&
	    !matches(ALLOW_USR_ETC, cpath))
		fail("%s: names /usr/etc or /usr/var (build with "
		     "--sysconfdir=/etc --localstatedir=/var)", rel);

	const Elf64_Ehdr *e = (const Elf64_Ehdr *)f;
	if (f[EI_CLASS] != ELFCLASS64 || f[EI_DATA] != ELFDATA2LSB ||
	    e->e_machine != EM_X86_64 ||
	    (e->e_type != ET_EXEC && e->e_type != ET_DYN) ||
	    e->e_phentsize != sizeof(Elf64_Phdr) ||
	    e->e_phoff > n || (size_t)e->e_phnum * sizeof(Elf64_Phdr) > n - e->e_phoff ||
	    strstr(cpath, "/testdata/")) {
		munmap((void *)f, n);
		return;
	}

	const Elf64_Phdr *dynph = NULL;
	int interp = 0;
	for (int i = 0; i < e->e_phnum; i++) {
		const Elf64_Phdr *ph =
			(const Elf64_Phdr *)(f + e->e_phoff + (size_t)i * sizeof(*ph));
		if (ph->p_type == PT_GNU_STACK && (ph->p_flags & PF_X))
			fail("%s: an executable stack", rel);
		else if (ph->p_type == PT_DYNAMIC)
			dynph = ph;
		else if (ph->p_type == PT_INTERP)
			interp = 1;
	}

	if (dynph && dynph->p_offset < n &&
	    dynph->p_filesz <= n - dynph->p_offset) {
		const Elf64_Dyn *d = (const Elf64_Dyn *)(f + dynph->p_offset);
		size_t nd = dynph->p_filesz / sizeof(*d);
		Elf64_Addr strtab = 0;
		Elf64_Xword strsz = 0;
		long rpath[2] = { -1, -1 };
		int now = 0, textrel = 0, needed = 0;
		for (size_t i = 0; i < nd && d[i].d_tag != DT_NULL; i++) {
			switch (d[i].d_tag) {
			case DT_STRTAB: strtab = d[i].d_un.d_ptr; break;
			case DT_STRSZ: strsz = d[i].d_un.d_val; break;
			case DT_RPATH: rpath[0] = (long)d[i].d_un.d_val; break;
			case DT_RUNPATH: rpath[1] = (long)d[i].d_un.d_val; break;
			case DT_NEEDED: needed = 1; break;
			case DT_TEXTREL: textrel = 1; break;
			case DT_BIND_NOW: now = 1; break;
			case DT_FLAGS:
				if (d[i].d_un.d_val & DF_TEXTREL)
					textrel = 1;
				if (d[i].d_un.d_val & DF_BIND_NOW)
					now = 1;
				break;
			case DT_FLAGS_1:
				if (d[i].d_un.d_val & DF_1_NOW)
					now = 1;
				break;
			}
		}
		if (textrel)
			fail("%s: text relocations (TEXTREL)", rel);
		if (!now && (needed || interp))
			note(W_BINDNOW, rel);

		long so = strtab ? vaddr_off(f, n, e, strtab) : -1;
		for (int k = 0; k < 2 && so >= 0; k++) {
			if (rpath[k] < 0 || (Elf64_Xword)rpath[k] >= strsz ||
			    (size_t)(so + rpath[k]) >= n)
				continue;
			const char *s = (const char *)f + so + rpath[k];
			size_t max = n - (size_t)(so + rpath[k]);
			if (memchr(s, 0, max))
				check_rpath(rel, s);
		}
	}
	munmap((void *)f, n);
}

/* ── The walk ─────────────────────────────────────────────────────────── */

static int visit(const char *path, const struct stat *st, int flag,
		 struct FTW *ftw)
{
	(void)flag;
	if (ftw->level == 0)
		return 0;
	const char *rel = path + L->pkglen + 1;
	if (!strcmp(rel, ".POSTINSTALL"))
		return 0;

	char cpath[4096];
	canon(rel, cpath, sizeof(cpath));

	check_layout(rel);
	check_mode(rel, cpath, st);

	if (S_ISREG(st->st_mode)) {
		size_t n = strlen(rel);
		if (n > 3 && !strcmp(rel + n - 3, ".pc") && strstr(rel, "pkgconfig/"))
			check_pc(path, rel);
		check_elf(path, rel, cpath, st->st_size);
	}
	return 0;
}

int kp_pkg_lint(const char *pkg, const char *name)
{
	Lint l;
	memset(&l, 0, sizeof(l));
	l.pkg = pkg;
	l.pkglen = strlen(pkg);
	l.name = name;
	l.kdos = !strncmp(name, "kdos-", 5);
	L = &l;

	kp_msg("Checking the staged tree...");
	nftw(pkg, visit, 32, FTW_PHYS);
	resolve_pc();

	for (int k = 0; k < W_N; k++) {
		if (!l.nwarn[k])
			continue;
		kb_warn("warning: lint: %d path(s) with %s:", l.nwarn[k], WARN_WHAT[k]);
		for (int i = 0; i < l.nwarn[k] && i < WARN_SHOW; i++) {
			fprintf(stderr, "    %s\n", l.warn[k][i]);
			free(l.warn[k][i]);
		}
		if (l.nwarn[k] > WARN_SHOW)
			fprintf(stderr, "    ... and %d more\n",
				l.nwarn[k] - WARN_SHOW);
	}
	if (l.fails > LINT_SHOW)
		kp_err("lint: ... and %d more", l.fails - LINT_SHOW);
	kb_buf_free(&l.pcdirs);
	kb_buf_free(&l.pcmods);
	L = NULL;

	if (l.fails) {
		kp_err("%s: the staged tree failed %d lint check(s); the "
		       "rules are in docs/kdos/03-architecture/packaging.md",
		       name, l.fails);
		return -1;
	}
	return 0;
}

/* ── Python ─────────────────────────────────────────────────────────────
 *
 * Bytecode is part of the package. /usr is read-only to a running program
 * (squashfs on the medium, root's on a disk), so a module without a .pyc is
 * compiled on every import, on every start, and the cache is never written.
 * Compiled here, it is in the package and in the database, so an upgrade
 * replaces it and a removal takes it away.
 *
 * checked-hash: the .pyc records a hash of its source instead of an mtime, so
 * no timestamp enters the package, and the interpreter checks the hash on
 * import, so a source an upgrade replaces is recompiled in memory rather than
 * served stale. -f rewrites any .pyc the build left, which may be
 * timestamp-based and is then never valid once the archive pins every mtime.
 * -s/-p record the installed path, not the work directory, in each code
 * object.
 *
 * Skipped: the bin directories, whose scripts are run rather than imported
 * (their __pycache__ is deleted); etc and var, whose .py files are
 * configuration a program executes; and a package that ships an interpreter's
 * own library (python3), which its own install compiles with the interpreter
 * it ships. The exit status is not the assertion: a file that is not Python 3
 * fails its compile while every other file is written. 70_image's
 * 020_cleanup.sh checks the result.
 */
static int py_seen;

static int py_find(const char *path, const struct stat *st, int flag,
		   struct FTW *ftw)
{
	(void)ftw;
	if (flag != FTW_F || !S_ISREG(st->st_mode))
		return 0;
	const char *rel = path + L->pkglen + 1;
	char cpath[4096];
	canon(rel, cpath, sizeof(cpath));
	if (under(cpath, "usr/bin") || under(cpath, "usr/sbin") ||
	    under(cpath, "etc") || under(cpath, "var"))
		return 0;
	if (!fnmatch("usr/lib/python3.*/os.py", cpath, FNM_PATHNAME))
		py_seen = -1;
	else if (py_seen >= 0) {
		size_t n = strlen(cpath);
		if (n > 3 && !strcmp(cpath + n - 3, ".py"))
			py_seen = 1;
	}
	return py_seen < 0;	/* stop the walk at an interpreter */
}

/* `*.dist-info/direct_url.json` names the work directory pip installed from;
 * the bin directories' __pycache__ holds bytecode for scripts nothing
 * imports from there. */
static int py_junk(const char *path, const struct stat *st, int flag,
		   struct FTW *ftw)
{
	(void)st;
	(void)ftw;
	const char *rel = path + L->pkglen + 1;
	if (flag == FTW_F && !strcmp(kb_basename(rel), "direct_url.json")) {
		char dir[4096];
		kb_strlcpy(dir, rel, sizeof(dir));
		char *s = strrchr(dir, '/');
		size_t n;
		if (s && (*s = 0, n = strlen(dir), n > 10) &&
		    !strcmp(dir + n - 10, ".dist-info"))
			unlink(path);
	}
	return 0;
}

static void regex_escape(KbBuf *b, const char *s)
{
	for (; *s; s++) {
		if (strchr("\\.^$|?*+()[]{}", *s))
			kb_buf_add(b, "\\", 1);
		kb_buf_add(b, s, 1);
	}
}

void kp_pkg_python(const char *pkg)
{
	Lint l;
	memset(&l, 0, sizeof(l));
	l.pkg = pkg;
	l.pkglen = strlen(pkg);
	L = &l;

	nftw(pkg, py_junk, 32, FTW_PHYS);
	static const char *const BINCACHE[] = {
		"usr/bin/__pycache__", "usr/sbin/__pycache__",
		"bin/__pycache__", "sbin/__pycache__", NULL
	};
	for (int i = 0; BINCACHE[i]; i++) {
		char *p = kb_path_join(pkg, BINCACHE[i]);
		if (kb_is_dir(p) && !kb_is_link(p))
			kb_rmtree(p);
		free(p);
	}

	py_seen = 0;
	nftw(pkg, py_find, 32, FTW_PHYS);
	if (py_seen <= 0 || !kb_have_prog("python3")) {
		if (py_seen > 0)
			kp_msg("No python3 yet; Python bytecode not compiled");
		L = NULL;
		return;
	}

	KbBuf rx = {0};
	kb_buf_str(&rx, "^");
	regex_escape(&rx, pkg);
	kb_buf_str(&rx, "/((usr/)?s?bin|etc|var)/");
	kb_buf_add(&rx, "", 1);

	kp_msg("Compiling Python bytecode (checked-hash)...");
	KbArgv a = {0};
	kb_argv_add(&a, "python3");
	kb_argv_add(&a, "-m");
	kb_argv_add(&a, "compileall");
	kb_argv_add(&a, "-q");
	kb_argv_add(&a, "-f");
	kb_argv_add(&a, "--invalidation-mode");
	kb_argv_add(&a, "checked-hash");
	kb_argv_add(&a, "-s");
	kb_argv_add(&a, pkg);
	kb_argv_add(&a, "-p");
	kb_argv_add(&a, "/");
	kb_argv_add(&a, "-x");
	kb_argv_add(&a, rx.p);
	kb_argv_add(&a, pkg);
	kb_argv_end(&a);
	kb_proc_verbose = 1;
	fflush(stdout);
	kb_run_tty(&a);
	kb_buf_free(&rx);
	L = NULL;
}
