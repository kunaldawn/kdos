/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   the tree policy: one rule in kpkg instead of one loop per recipe
 *
 * Four steps the build runs on every port (build.c), so no recipe carries
 * them:
 *
 *   kp_src_refresh_config  before build.sh: every config.guess and
 *                          config.sub in the unpacked source older than
 *                          automake's is replaced by automake's, except
 *                          inside a vendored Rust crate
 *   kp_pkg_licences        the source root's top-level licence files into
 *                          usr/share/licenses/<name>/
 *   kp_pkg_prune           HTML and PDF manuals, and every non-English
 *                          message catalog, handbook and man page
 *   kp_pkg_symtab          .symtab/.strtab out of ELF executables and
 *                          shared objects over 32 MiB
 *
 * The three package steps run on $PKG before the manifest exists, so a path
 * they remove is never in the package and never owned in the database.
 * Changing what any of them removes or adds changes what a package contains,
 * and moves KP_STORE_FORMAT (kpkg.h) with it.
 *
 * The policy, its exceptions and its cost are in writing-ports.md, "Size and
 * language policy"; a port does not repeat any of it in its build.sh.
 * ---------------------------------
 */

#include <elf.h>
#include <fcntl.h>
#include <fnmatch.h>
#include <ftw.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>
#include <sys/stat.h>

#include "kdos-kpkg.h"

/* nftw passes no user pointer; each walk below sets this first. */
static size_t walk_rootlen;

static int under(const char *rel, const char *dir)
{
	size_t n = strlen(dir);
	return !strncmp(rel, dir, n) && (rel[n] == '/' || !rel[n]);
}

static int has_suffix_ci(const char *s, const char *suf)
{
	size_t a = strlen(s), b = strlen(suf);
	return a >= b && !strcasecmp(s + a - b, suf);
}

/* ── Locale tags ─────────────────────────────────────────────────────── */

/* `ll`, `lll`, then optionally `_CC`/`-CC`, `_Ssss` (script) or `_DDD` (UN
 * region), then after a region optionally `.codeset`, then optionally
 * `@variant`: de, pt_BR, zh_Hant, es_419, zh_TW.Big5, sr@latin, ca@valencia.
 * Anything else is not a tag, and a file it names is kept. A codeset with
 * no region is refused, or `org.kde.foo` would read as the language `org`. */
static int locale_tag(const char *s, size_t n, int *english, int *bare3)
{
	size_t i = 0;
	while (i < n && s[i] >= 'a' && s[i] <= 'z')
		i++;
	if (i < 2 || i > 3)
		return 0;
	*english = i == 2 && s[0] == 'e' && s[1] == 'n';
	*bare3 = i == 3 && i == n;
	int region = 0;
	if (i < n && (s[i] == '_' || s[i] == '-')) {
		size_t j = ++i;
		while (i < n && s[i] != '.' && s[i] != '@')
			i++;
		size_t m = i - j;
		const char *t = s + j;
		int ok = 0;
		if (m == 2 && t[0] >= 'A' && t[0] <= 'Z' && t[1] >= 'A' &&
		    t[1] <= 'Z')
			ok = 1;
		else if (m == 3 && t[0] >= '0' && t[0] <= '9' && t[1] >= '0' &&
			 t[1] <= '9' && t[2] >= '0' && t[2] <= '9')
			ok = 1;
		else if (m == 4 && t[0] >= 'A' && t[0] <= 'Z') {
			ok = 1;
			for (size_t k = 1; k < 4; k++)
				if (t[k] < 'a' || t[k] > 'z')
					ok = 0;
		}
		if (!ok)
			return 0;
		region = 1;
	}
	if (region && i < n && s[i] == '.') {
		size_t j = ++i;
		while (i < n && s[i] != '@')
			i++;
		if (i == j)
			return 0;
	}
	if (i < n && s[i] == '@') {
		size_t j = ++i;
		while (i < n && s[i] >= 'a' && s[i] <= 'z')
			i++;
		if (i == j)
			return 0;
	}
	return i == n;
}

/* A directory named for a language other than English. */
static int foreign_dir(const char *name)
{
	int en = 0, b3 = 0;
	return locale_tag(name, strlen(name), &en, &b3) && !en;
}

/* The language a catalog's file name ends in: `de.mo`, `app_pt_BR.qm`,
 * `qtgnuplot_fr.qm`. The longest tag that follows a `_`, `-` or `.`, or the
 * whole stem, decides; a stem with none (`qmt_rgb2pct.qm`) is not a
 * translation and stays. A bare three-letter word is not taken for a
 * language here, because `sed.mo` and `app_foo.qm` would be; outside a
 * locale directory a three-letter language needs its region (`ast_ES`). */
static int foreign_stem(const char *base)
{
	const char *dot = strrchr(base, '.');
	size_t n = dot ? (size_t)(dot - base) : strlen(base);
	for (size_t p = 0; p < n; p++) {
		if (p && base[p - 1] != '_' && base[p - 1] != '-' &&
		    base[p - 1] != '.')
			continue;
		int en = 0, b3 = 0;
		if (locale_tag(base + p, n - p, &en, &b3) && !b3)
			return !en;
	}
	return 0;
}

/* ── kp_pkg_prune ────────────────────────────────────────────────────── */

/* HTML a program opens itself, so pruning it breaks a feature rather than
 * dropping a manual nobody's program reads:
 *   - the manuals that are HTML and nothing else: mednafen's, the book
 *     kdos-doc renders, and KDOS from Scratch, which kdos-kfs renders with
 *     its one PDF beside it;
 *   - R's help.start() index and the R.css every page of its dynamic help
 *     server links;
 *   - the manuals behind a Help menu: geany's (Help > Help, html/index.html),
 *     gimagereader's (manual.html), texstudio's (User Manual), wsjtx's
 *     (Local User Guide), Wireshark's (Help > Manual Pages) and hatari-ui's;
 *   - the pages an About dialog shows: FreeCAD's LICENSE.html and
 *     ThirdPartyLibraries.html, SMPlayer's gpl.html.
 * Each path is where that port installs and where the program looks; a port
 * that moves its docdir moves its line here. */
static const char *const DOC_KEEP[] = {
	"usr/share/doc/mednafen",
	"usr/share/doc/kdos",
	"usr/share/doc/kfs",
	"usr/share/doc/R/html",
	"usr/share/doc/geany/html",
	"usr/share/doc/gimagereader",
	"usr/share/doc/texstudio",
	"usr/share/doc/wsjtx",
	"usr/share/doc/wireshark",
	"usr/share/doc/hatari",
	"usr/share/doc/freecad",
	"usr/share/doc/packages/smplayer",
	NULL
};

static int doc_tree(const char *rel)
{
	if (!under(rel, "usr/share/doc") && !under(rel, "usr/share/texmf-dist/doc"))
		return 0;
	for (int i = 0; DOC_KEEP[i]; i++)
		if (under(rel, DOC_KEEP[i]))
			return 0;
	return 1;
}

/* 1 when the path goes. A directory that goes takes its whole subtree. */
static int prune_this(const char *rel, int isdir)
{
	if (under(rel, "usr/share/licenses"))
		return 0;

	const char *base = kb_basename(rel);
	char parent[4096];
	kb_strlcpy(parent, rel, sizeof(parent));
	char *slash = strrchr(parent, '/');
	if (slash)
		*slash = 0;
	else
		parent[0] = 0;

	if (doc_tree(rel)) {
		/* KDE handbooks: `HTML/<lang>/` is a translation, `HTML/en/`
		 * the one khelpcenter shows. The name is upper case, so the
		 * `html` rule below never matches it. */
		if (isdir && !strcmp(parent, "usr/share/doc/HTML"))
			return foreign_dir(base);
		if (isdir)
			return !strcmp(base, "html");
		return has_suffix_ci(base, ".html") || has_suffix_ci(base, ".htm") ||
		       has_suffix_ci(base, ".pdf");
	}

	/* Translated man pages, and any cat* directory: man reads only the
	 * man<section> directories without a locale. */
	if (isdir && !strcmp(parent, "usr/share/man"))
		return fnmatch("man[0-9n]*", base, 0) != 0;

	if (isdir)
		return 0;

	int mo = has_suffix_ci(base, ".mo") || has_suffix_ci(base, ".gmo");
	int qm = has_suffix_ci(base, ".qm");
	if (!mo && !qm)
		return 0;

	/* `<dir>/<lang>/LC_MESSAGES/<file>`: the language is the directory,
	 * whatever the file is called and whatever holds it — `locale/`, R's
	 * `po/` and `translations/`, a Python package's `locales/`. */
	const char *lcm = strrchr(parent, '/');
	if (lcm && !strcmp(lcm + 1, "LC_MESSAGES")) {
		char lang[4096];
		size_t n = (size_t)(lcm - parent);
		memcpy(lang, parent, n);
		lang[n] = 0;
		const char *l = strrchr(lang, '/');
		int en = 0, b3 = 0;
		const char *ln = l ? l + 1 : lang;
		if (locale_tag(ln, strlen(ln), &en, &b3))
			return !en;
	}
	return foreign_stem(base);
}

static KbBuf prune_list;
static int prune_n;
static char prune_skip[4096];

static int prune_visit(const char *path, const struct stat *st, int flag,
		       struct FTW *ftw)
{
	(void)st;
	if (ftw->level == 0)
		return 0;
	const char *rel = path + walk_rootlen + 1;
	if (prune_skip[0] && under(rel, prune_skip))
		return 0;
	int isdir = flag == FTW_D || flag == FTW_DNR;
	if (!prune_this(rel, isdir))
		return 0;
	if (isdir)
		kb_strlcpy(prune_skip, rel, sizeof(prune_skip));
	kb_buf_add(&prune_list, rel, strlen(rel) + 1);
	prune_n++;
	return 0;
}

/* Directories the prune emptied go too, up to the package root: a locale
 * tree with every catalog gone is a dozen empty directories in the manifest,
 * and an empty `usr/share/doc/<name>` reads as a manual that is missing. */
static void rmdir_up(const char *pkg, const char *rel)
{
	char d[4096];
	kb_strlcpy(d, rel, sizeof(d));
	for (char *s = strrchr(d, '/'); s; s = strrchr(d, '/')) {
		*s = 0;
		char *p = kb_path_join(pkg, d);
		int rc = rmdir(p);
		free(p);
		if (rc != 0)
			return;
	}
}

void kp_pkg_prune(const char *pkg)
{
	walk_rootlen = strlen(pkg);
	prune_list.n = 0;
	prune_n = 0;
	prune_skip[0] = 0;
	nftw(pkg, prune_visit, 32, FTW_PHYS);
	if (!prune_n) {
		kb_buf_free(&prune_list);
		return;
	}
	for (const char *r = prune_list.p; r < prune_list.p + prune_list.n;
	     r += strlen(r) + 1) {
		char *p = kb_path_join(pkg, r);
		kb_rmtree(p);
		free(p);
		rmdir_up(pkg, r);
	}
	kp_msg("Pruned %d manual, translation and catalog path(s)", prune_n);
	kb_buf_free(&prune_list);
}

/* ── kp_pkg_licences ─────────────────────────────────────────────────── */

/* A licence text by its name: `LICENSE`, `COPYING.LIB`, `Licence-MIT.txt`.
 * A source file that only starts like one (`license.c`, `copyright.h`,
 * `COPYING.in`) is code or a template, not the text. */
static int licence_name(const char *n)
{
	static const char *const P[] = {
		"LICENSE", "LICENCE", "COPYING", "COPYRIGHT", "NOTICE", NULL
	};
	static const char *const CODE[] = {
		".c", ".h", ".cc", ".cpp", ".cxx", ".hh", ".hpp", ".py", ".go",
		".rs", ".js", ".ts", ".java", ".rb", ".pl", ".pm", ".sh", ".in",
		".am", ".m4", ".cmake", ".o", NULL
	};
	int hit = 0;
	for (int i = 0; P[i] && !hit; i++)
		hit = !strncasecmp(n, P[i], strlen(P[i]));
	if (!hit)
		return 0;
	for (int i = 0; CODE[i]; i++)
		if (has_suffix_ci(n, CODE[i]))
			return 0;
	return 1;
}

/* `real` is `top` or below it; both are resolved paths. */
static int inside(const char *real, const char *top)
{
	size_t n = strlen(top);
	return !strncmp(real, top, n) && (real[n] == '/' || !real[n]);
}

/* What a licence entry of the source really is, or 0 to skip it. A symlink
 * is followed only into the source tree or /usr/share — automake's
 * `--add-missing` links COPYING to its own copy — so an archive cannot
 * make the package carry an arbitrary file of the builder. */
static int licence_source(const char *path, const char *srcreal,
			  struct stat *st)
{
	if (lstat(path, st) != 0)
		return 0;
	if (S_ISLNK(st->st_mode)) {
		char *real = realpath(path, NULL);
		int ok = real && (inside(real, srcreal) ||
				  inside(real, "/usr/share")) &&
			 stat(real, st) == 0;
		free(real);
		if (!ok)
			return 0;
	}
	return S_ISREG(st->st_mode) || S_ISDIR(st->st_mode);
}

/* Every component of `rel` under `pkg` is a real directory or does not
 * exist yet. A staged `usr/share/licenses/<name>` (or any parent) that is a
 * symlink would have the directories made and the copy written wherever it
 * points, on the builder. */
static int real_dirs(const char *pkg, const char *rel)
{
	char d[1024];
	kb_strlcpy(d, rel, sizeof(d));
	for (char *s = d;; s++) {
		if (*s && *s != '/')
			continue;
		char c = *s;
		*s = 0;
		char *p = kb_path_join(pkg, d);
		struct stat st;
		int r = lstat(p, &st);
		free(p);
		*s = c;
		if (r != 0)
			return 1;
		if (!S_ISDIR(st.st_mode))
			return 0;
		if (!c)
			return 1;
	}
}

/* Into `<pkg>/<reldir>/<name>`. An existing entry of any kind — a file
 * build.sh installed, or a symlink, dangling or not — is left as it is, and
 * never written through. */
static int copy_licence(const char *from, const char *pkg, const char *reldir,
			const char *name)
{
	if (!real_dirs(pkg, reldir))
		return 0;
	char *dir = kb_path_join(pkg, reldir);
	char *to = kb_path_join(dir, name);
	struct stat st;
	int done = 0;
	if (kb_mkdir_p(dir) == 0 && lstat(to, &st) != 0 &&
	    kb_copy_file(from, to) == 0) {
		chmod(to, 0644);
		done = 1;
	}
	free(to);
	free(dir);
	return done;
}

void kp_pkg_licences(const char *pkg, const char *name, const char *src)
{
	char *srcreal = realpath(src, NULL);
	char **v = srcreal ? kb_listdir(src, NULL) : NULL;
	if (!v) {
		free(srcreal);
		return;
	}
	char rel[512];
	snprintf(rel, sizeof(rel), "usr/share/licenses/%s", name);
	int n = 0;
	for (char **e = v; *e; e++) {
		if (!licence_name(*e))
			continue;
		char *from = kb_path_join(src, *e);
		struct stat st;
		if (!licence_source(from, srcreal, &st)) {
			free(from);
			continue;
		}
		if (S_ISREG(st.st_mode)) {
			n += copy_licence(from, pkg, rel, *e);
		} else {
			/* REUSE's `LICENSES/`: one text per SPDX identifier
			 * and often no top-level file beside it. */
			char sub[1024];
			snprintf(sub, sizeof(sub), "%s/%s", rel, *e);
			char **w = kb_listdir(from, NULL);
			for (char **f = w; f && *f; f++) {
				char *ff = kb_path_join(from, *f);
				struct stat fst;
				if (licence_source(ff, srcreal, &fst) &&
				    S_ISREG(fst.st_mode))
					n += copy_licence(ff, pkg, sub, *f);
				free(ff);
			}
			kb_strv_free(w);
		}
		free(from);
	}
	kb_strv_free(v);
	free(srcreal);
	if (n)
		kp_msg("Added %d licence file(s) to /%s", n, rel);
}

/* ── kp_pkg_symtab ───────────────────────────────────────────────────── */

#define SYMTAB_MIN (32LL * 1024 * 1024)

/* An ELF executable or shared object with a .symtab, and nothing in the
 * file past its last section and its section header table. Relocatable
 * objects, kernel modules and static archives are left alone: their symbol
 * table is what a link reads. A file with bytes past both is a program that
 * carries an appended payload (a self-extracting archive, a bundled
 * runtime), which objcopy would drop; it keeps its symbols. */
#define SYMTAB_SCAN(EH, SH)                                                    \
	do {                                                                   \
		EH eh;                                                         \
		if (pread(fd, &eh, sizeof(eh), 0) != (ssize_t)sizeof(eh) ||    \
		    (eh.e_type != ET_EXEC && eh.e_type != ET_DYN) ||           \
		    eh.e_shentsize != sizeof(SH) || !eh.e_shoff)               \
			goto out;                                              \
		unsigned long long num = eh.e_shnum;                           \
		SH sh;                                                         \
		if (!num) {                                                    \
			/* Extended numbering: the count is in section 0. */  \
			if (pread(fd, &sh, sizeof(sh), (off_t)eh.e_shoff) !=   \
			    (ssize_t)sizeof(sh))                               \
				goto out;                                      \
			num = sh.sh_size;                                      \
		}                                                              \
		unsigned long long end =                                       \
			eh.e_shoff + num * (unsigned long long)sizeof(SH);     \
		if (end > (unsigned long long)size)                            \
			goto out;                                              \
		for (unsigned long long i = 0; i < num; i++) {                 \
			if (pread(fd, &sh, sizeof(sh),                         \
				  (off_t)(eh.e_shoff + i * sizeof(sh))) !=     \
			    (ssize_t)sizeof(sh))                               \
				goto out;                                      \
			if (sh.sh_type == SHT_SYMTAB)                          \
				found = 1;                                     \
			if (sh.sh_type != SHT_NOBITS &&                        \
			    sh.sh_offset + sh.sh_size > end)                   \
				end = sh.sh_offset + sh.sh_size;               \
		}                                                              \
		if (end < (unsigned long long)size)                            \
			found = 0;                                             \
	} while (0)

static int has_symtab(const char *path, off_t size)
{
	int fd = open(path, O_RDONLY | O_CLOEXEC);
	if (fd < 0)
		return 0;
	int found = 0;
	unsigned char id[EI_NIDENT];
	if (pread(fd, id, sizeof(id), 0) != (ssize_t)sizeof(id) ||
	    memcmp(id, ELFMAG, SELFMAG) || id[EI_DATA] != ELFDATA2LSB)
		goto out;
	if (id[EI_CLASS] == ELFCLASS64)
		SYMTAB_SCAN(Elf64_Ehdr, Elf64_Shdr);
	else if (id[EI_CLASS] == ELFCLASS32)
		SYMTAB_SCAN(Elf32_Ehdr, Elf32_Shdr);
	close(fd);
	return found;
out:
	close(fd);
	return 0;
}

static KbBuf big_list;
static KbBuf big_inodes;	/* ino_t per listed path: one strip per inode */

static int big_visit(const char *path, const struct stat *st, int flag,
		     struct FTW *ftw)
{
	(void)ftw;
	if (flag != FTW_F || !S_ISREG(st->st_mode) || st->st_size <= SYMTAB_MIN)
		return 0;
	const ino_t *seen = (const ino_t *)(void *)big_inodes.p;
	for (size_t i = 0; i < big_inodes.n / sizeof(ino_t); i++)
		if (seen[i] == st->st_ino)
			return 0;
	if (!has_symtab(path, st->st_size))
		return 0;
	kb_buf_add(&big_inodes, &st->st_ino, sizeof(st->st_ino));
	kb_buf_add(&big_list, path, strlen(path) + 1);
	return 0;
}

/*
 * --strip-all with every debug section kept removes exactly .symtab and
 * .strtab from an executable or shared object: .gnu_debuglink, the notes and
 * the build ID stay, and the dynamic symbol table a loader reads is not
 * touched. `--strip-unneeded` and `--strip-debug` are not this: the first
 * also drops the debug sections, the second keeps the symbol table.
 *
 * The output is copied back OVER the staged file rather than renamed onto
 * it, so its inode, its hard links and its mode survive.
 */
void kp_pkg_symtab(const char *pkg)
{
	big_list.n = 0;
	big_inodes.n = 0;
	nftw(pkg, big_visit, 32, FTW_PHYS);
	kb_buf_free(&big_inodes);
	if (!big_list.n) {
		kb_buf_free(&big_list);
		return;
	}
	if (!kb_have_prog("objcopy")) {
		kp_msg("No objcopy yet; symbol tables over 32 MiB are kept");
		kb_buf_free(&big_list);
		return;
	}
	for (const char *p = big_list.p; p < big_list.p + big_list.n;
	     p += strlen(p) + 1) {
		struct stat st;
		if (stat(p, &st) != 0)
			continue;
		char tmp[4200];
		snprintf(tmp, sizeof(tmp), "%s.kpkg-symtab", p);
		KbArgv a = {0};
		kb_argv_add(&a, "objcopy");
		kb_argv_add(&a, "--strip-all");
		kb_argv_add(&a, "--keep-section=.debug_*");
		kb_argv_add(&a, "--keep-section=.zdebug_*");
		kb_argv_add(&a, p);
		kb_argv_add(&a, tmp);
		kb_argv_end(&a);
		if (kb_run(&a) == 0 && kb_copy_file(tmp, p) == 0) {
			chmod(p, st.st_mode & 07777);
			struct stat nst;
			stat(p, &nst);
			kp_msg("Symbol table stripped: /%s (%lld MB -> %lld MB)",
			       p + strlen(pkg) + 1,
			       (long long)st.st_size / (1024 * 1024),
			       (long long)nst.st_size / (1024 * 1024));
		} else {
			kb_warn("warning: could not strip the symbol table of %s", p);
		}
		unlink(tmp);
	}
	kb_buf_free(&big_list);
}

/* ── kp_src_refresh_config ───────────────────────────────────────────── */

/* `timestamp='YYYY-MM-DD'`, the line both scripts carry and bump. */
static int script_stamp(const char *path, char out[16])
{
	size_t n = 0;
	char *data = kb_read_all(path, &n);
	out[0] = 0;
	if (!data)
		return -1;
	for (char *l = data; l && *l;) {
		if (!strncmp(l, "timestamp='", 11)) {
			char *e = strchr(l + 11, '\'');
			if (e && e - (l + 11) < 16) {
				memcpy(out, l + 11, (size_t)(e - (l + 11)));
				out[e - (l + 11)] = 0;
			}
			break;
		}
		l = strchr(l, '\n');
		if (l)
			l++;
	}
	free(data);
	return out[0] ? 0 : -1;
}

static struct {
	char path[512];
	char stamp[16];
} fresh[2];		/* [0] config.guess, [1] config.sub */
static int refreshed;
static const char *refresh_root;

/* A vendored Rust crate records every file's hash in .cargo-checksum.json at
 * its root, and cargo refuses the whole build when one differs, so nothing
 * under such a directory is replaced. Its config.guess, if it runs at all,
 * runs from a build script that names the target itself. */
static int in_checksummed_crate(const char *path)
{
	char d[4096];
	size_t root = strlen(refresh_root);
	kb_strlcpy(d, path, sizeof(d));
	for (;;) {
		char *slash = strrchr(d, '/');
		if (!slash || (size_t)(slash - d) < root)
			return 0;
		*slash = 0;
		char f[4200];
		snprintf(f, sizeof(f), "%s/.cargo-checksum.json", d);
		if (access(f, F_OK) == 0)
			return 1;
	}
}

static int refresh_visit(const char *path, const struct stat *st, int flag,
			 struct FTW *ftw)
{
	(void)ftw;
	if (flag != FTW_F || !S_ISREG(st->st_mode))
		return 0;
	const char *b = kb_basename(path);
	int k = !strcmp(b, "config.guess") ? 0 : !strcmp(b, "config.sub") ? 1 : -1;
	if (k < 0 || !fresh[k].stamp[0])
		return 0;
	char have[16];
	if (script_stamp(path, have) != 0 || strcmp(have, fresh[k].stamp) >= 0)
		return 0;
	if (in_checksummed_crate(path))
		return 0;
	if (kb_copy_file(fresh[k].path, path) == 0) {
		chmod(path, (st->st_mode & 07777) | 0111);
		refreshed++;
	}
	return 0;
}

void kp_src_refresh_config(const char *src_root)
{
	static const char *const N[2] = { "config.guess", "config.sub" };
	memset(fresh, 0, sizeof(fresh));
	char **v = kb_listdir("/usr/share", NULL);
	for (char **e = v; e && *e; e++) {
		if (strncmp(*e, "automake-", 9))
			continue;
		for (int k = 0; k < 2; k++) {
			char p[512], s[16];
			snprintf(p, sizeof(p), "/usr/share/%s/%s", *e, N[k]);
			if (script_stamp(p, s) == 0 && strcmp(s, fresh[k].stamp) > 0) {
				kb_strlcpy(fresh[k].path, p, sizeof(fresh[k].path));
				kb_strlcpy(fresh[k].stamp, s, sizeof(fresh[k].stamp));
			}
		}
	}
	kb_strv_free(v);
	if (!fresh[0].stamp[0] && !fresh[1].stamp[0])
		return;
	refreshed = 0;
	refresh_root = src_root;
	nftw(src_root, refresh_visit, 32, FTW_PHYS);
	if (refreshed)
		kp_msg("Refreshed %d config.guess/config.sub from automake (%s)",
		       refreshed, fresh[0].stamp[0] ? fresh[0].stamp : fresh[1].stamp);
}
