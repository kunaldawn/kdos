/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   kdos-fetch-app / kdos-fetch-static
 *
 * kdos-fetch-app installs an alien app into an ad-hoc distrobox and exports it
 * to the host. kdos-fetch-static drops a checksummed upstream binary into
 * /usr/local/bin.
 *
 * The app name reaching kdos-fetch-app is never part of a script's text. The
 * package manager fallback is shell, because that is what it is, but the name
 * arrives as a POSITIONAL PARAMETER. Interpolated into a `bash -c` string, an
 * app name containing a quote would break out of the quoting and run as the
 * box's root.
 *
 * THE BOX IS CREATED WITH --no-entry. distrobox otherwise writes a launcher
 * for the box itself into ~/.local/share/applications, which `podman rm`
 * never takes away: the menu would keep an entry for a box that is gone.
 *
 * --remove UNPINS WHAT IT DELETES. The task manager keeps an
 * `applications:` launcher whose desktop file is gone, and draws it as a
 * blank document; so every launcher the remove deleted is taken out of each
 * task manager's General/launchers through plasmashell's scripting
 * (unpin_launchers). Outside a session there is nothing to unpin.
 * ---------------------------------
 */

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/wait.h>

#include "kdos-tools.h"

#define BOX_DEFAULT   "kdos-debian"
#define IMAGE_DEFAULT "debian:stable"
#define STATIC_DEST   "/usr/local/bin"

/* ──────────────────────────────────────────────────────────────────────── */

/*
 * Fixed script text, app name passed as $1. Nothing here is built from the
 * user's argument, so there is nothing to escape.
 *
 * apt-get, never apt: apt warns on every call that it "does not have a stable
 * CLI interface", and that warning would reach the user's terminal. apt-get
 * runs as `sudo env DEBIAN_FRONTEND=noninteractive`: the script has no
 * terminal on its standard input, and debconf's default frontend would have
 * dpkg-preconfigure try to reopen one and print "unable to re-open stdin".
 * The variable goes through env because sudo clears the caller's. A box with
 * none of the three managers exits NO_PKG_MANAGER, which the caller tells from
 * a package that failed to install.
 *
 * Each manager branch names its manager on stderr before the first manager
 * command (apt-get's before `apt-get update`, which is part of the wait):
 * the manager's own output stays off the terminal, and a first install in a
 * new box is minutes of it, which would otherwise show as a hang after
 * distrobox's last setup step. The line must follow the sudo check, which
 * stays the script's first line.
 *
 * A box without sudo exits BOX_UNSET before anything else: distrobox-init
 * installs sudo with the rest of its dependencies on the box's first start,
 * and distrobox-enter prints "Container Setup Complete!" even when that
 * install failed for want of a network. A box without sudo is a box whose
 * first setup never finished, and every install in it would end in "sudo: not
 * found". That line is distrobox's, not a result: the enter running this
 * script drops it (run_box_relay), so fetch-app's own lines report the outcome.
 */
#define NO_PKG_MANAGER 69
#define BOX_UNSET      70

static const char INSTALL_SH[] =
	"command -v sudo >/dev/null || exit 70\n"
	"set -e\n"
	"if command -v apt-get >/dev/null; then\n"
	"  echo '==> Running apt-get inside the box (a few minutes the first time)' >&2\n"
	"  sudo env DEBIAN_FRONTEND=noninteractive apt-get update\n"
	"  sudo env DEBIAN_FRONTEND=noninteractive apt-get install -y -- \"$1\"\n"
	"elif command -v dnf >/dev/null; then\n"
	"  echo '==> Running dnf inside the box (a few minutes the first time)' >&2\n"
	"  sudo dnf install -y -- \"$1\"\n"
	"elif command -v pacman >/dev/null; then\n"
	"  echo '==> Running pacman inside the box (a few minutes the first time)' >&2\n"
	"  sudo pacman -Sy --noconfirm -- \"$1\"\n"
	"else\n"
	"  exit 69\n"
	"fi\n";

static const char REMOVE_SH[] =
	"if command -v apt-get >/dev/null; then\n"
	"  sudo env DEBIAN_FRONTEND=noninteractive apt-get -y remove -- \"$1\"\n"
	"elif command -v dnf >/dev/null; then sudo dnf remove -y -- \"$1\"\n"
	"elif command -v pacman >/dev/null; then sudo pacman -Rns --noconfirm -- \"$1\"\n"
	"else echo 'none of apt-get, dnf or pacman in the box' >&2; exit 1; fi\n";

/* 0 when the package is installed in the box. A remove asks this first, so a
 * name the box never had is refused with a sentence of its own instead of
 * three package managers' errors and a success. */
static const char INSTALLED_SH[] =
	"if command -v dpkg-query >/dev/null; then\n"
	"  case \"$(dpkg-query -W -f='${Status}' -- \"$1\" 2>/dev/null)\" in\n"
	"    *' installed') exit 0 ;;\n"
	"  esac\n"
	"  exit 1\n"
	"elif command -v rpm >/dev/null; then rpm -q -- \"$1\" >/dev/null 2>&1\n"
	"elif command -v pacman >/dev/null; then pacman -Q -- \"$1\" >/dev/null 2>&1\n"
	"else exit 1; fi\n";

/* The installed package's own file list, one absolute path a line. */
static const char FILES_SH[] =
	"if command -v dpkg-query >/dev/null; then dpkg-query -L -- \"$1\"\n"
	"elif command -v rpm >/dev/null; then rpm -ql -- \"$1\"\n"
	"elif command -v pacman >/dev/null; then pacman -Qlq -- \"$1\"\n"
	"fi\n";

static void box_sh(KbArgv *a, const char *box, const char *script,
		   const char *arg)
{
	kb_argv_add(a, "distrobox");
	kb_argv_add(a, "enter");
	kb_argv_add(a, box);
	kb_argv_add(a, "--");
	kb_argv_add(a, "sh");
	kb_argv_add(a, "-c");
	kb_argv_add(a, script);
	kb_argv_add(a, "sh");	/* $0 */
	kb_argv_add(a, arg);	/* $1 — the app, as data                   */
	kb_argv_end(a);
}

/*
 * WHAT IS EXPORTED IS READ FROM THE PACKAGE'S FILES, never guessed from its
 * name: Arch's neovim installs nvim.desktop and /usr/bin/nvim, and a package
 * name handed to `distrobox-export --app` finds no desktop file at all.
 *
 * EACH DESKTOP FILE GOES TO `--app` BY ITS FULL IN-BOX PATH. Any other
 * argument is matched by distrobox as a substring of every desktop file's
 * Exec= and Name= lines: org.kde.kate.desktop (Exec=kate) would match nothing,
 * and `nvim` would also take nvim-qt.desktop, whose launcher a remove would
 * then delete with neovim's. Only desktop files directly under
 * /usr/share/applications and entries directly under /usr/bin are exported, so
 * a remove reading the same list deletes exactly the exports the install made.
 */
typedef struct {
	char **app, **bin;
	int napp, nbin;
} PkgExports;

static void list_add(char ***v, int *n, const char *s, size_t len)
{
	*v = kb_realloc(*v, (size_t)(*n + 1) * sizeof(**v));
	char *d = kb_calloc(1, len + 1);
	memcpy(d, s, len);
	(*v)[(*n)++] = d;
}

static void pkg_exports(const char *box, const char *app, PkgExports *x)
{
	static const char apps[] = "/usr/share/applications/";
	static const char bins[] = "/usr/bin/";
	KbArgv a = {0};
	KbBuf out = {0};

	memset(x, 0, sizeof(*x));
	box_sh(&a, box, FILES_SH, app);
	int verbose = kb_proc_verbose;
	kb_proc_verbose = 0;
	int rc = kb_run_capture_buf(&a, &out);
	kb_proc_verbose = verbose;
	if (rc != 0 || !out.p) {
		kb_buf_free(&out);
		return;
	}

	for (char *line = strtok(out.p, "\n"); line; line = strtok(NULL, "\n")) {
		size_t n = strlen(line);
		while (n && (line[n - 1] == '\r' || line[n - 1] == ' '))
			line[--n] = 0;
		if (!strncmp(line, apps, sizeof(apps) - 1)) {
			const char *f = line + sizeof(apps) - 1;
			size_t m = strlen(f);
			if (m > 8 && !strchr(f, '/') &&
			    !strcmp(f + m - 8, ".desktop"))
				list_add(&x->app, &x->napp, line, n);
		} else if (!strncmp(line, bins, sizeof(bins) - 1)) {
			const char *f = line + sizeof(bins) - 1;
			if (*f && !strchr(f, '/'))
				list_add(&x->bin, &x->nbin, line, n);
		}
	}
	kb_buf_free(&out);
}

static void pkg_exports_free(PkgExports *x)
{
	for (int i = 0; i < x->napp; i++)
		free(x->app[i]);
	for (int i = 0; i < x->nbin; i++)
		free(x->bin[i]);
	free(x->app);
	free(x->bin);
}

/* A launcher's name for the ==> lines: its desktop file's name without
 * `.desktop`, into a buffer of the caller's. */
static const char *app_stem(const char *path, char *buf, size_t cap)
{
	snprintf(buf, cap, "%s", kb_basename(path));
	size_t n = strlen(buf);
	if (n > 8 && !strcmp(buf + n - 8, ".desktop"))
		buf[n - 8] = 0;
	return buf;
}

/*
 * THE EXPORTED LAUNCHER'S StartupWMClass IS THE STEM, not the path it was
 * exported by. distrobox appends `StartupWMClass=<its --app argument>` to a
 * desktop file that carries none, and the exported file is named
 * `<box>-<file>`, so the task manager has only that line to tie a window's
 * app id (org.kde.kate, nvim) to the launcher: left as a path, the window
 * gets no launcher icon and is not grouped with it. A file that names its own
 * StartupWMClass is left as distrobox wrote it.
 */
static void fix_wm_class(const char *box, const char *path, const char *stem)
{
	char rel[1024];
	snprintf(rel, sizeof(rel), ".local/share/applications/%s-%s", box,
		 kb_basename(path));
	char *host = kb_path_join(kb_home_dir(), rel);
	char *text = kb_read_whole(host, NULL);
	if (!text) {
		free(host);
		return;
	}

	KbBuf want = {0};
	kb_buf_printf(&want, "StartupWMClass=%s\n", path);
	char *at = strstr(text, want.p);
	if (at && (at == text || at[-1] == '\n')) {
		KbBuf out = {0};
		kb_buf_printf(&out, "%.*sStartupWMClass=%s\n%s", (int)(at - text),
			      text, stem, at + want.n);
		if (kb_write_file_atomic(host, out.p) != 0)
			kb_warn("could not set StartupWMClass in %s", host);
		kb_buf_free(&out);
	}
	kb_buf_free(&want);
	free(text);
	free(host);
}

/*
 * THE TITLE BAR'S ICON COMES FROM A DESKTOP FILE NAMED AFTER THE APP ID.
 * KWin resolves a window to `<app id>.desktop` by file name alone, never by
 * StartupWMClass, and distrobox names the export `<box>-<file>`, so a boxed
 * foot (app id `foot`) finds no entry and gets the generic Wayland icon. When
 * no `<stem>.desktop` is in XDG_DATA_HOME or XDG_DATA_DIRS, the export is
 * copied to ~/.local/share/applications/<stem>.desktop, marked
 * X-KDOS-Fetch-Box=<box> so --remove deletes only a copy this box's export
 * made, and the export itself gets NoDisplay=true. The copy is then the one
 * entry the menu lists, a pin names and the task manager resolves the
 * window to, so a pinned launcher and its running window stay one task; two
 * visible entries would be two URLs and two task-manager icons. A host
 * entry of that name is the host's application's, and is never shadowed.
 */
#define ALIAS_MARK "X-KDOS-Fetch-Box="

/* `text` with `add` (a whole line, or NULL) first in its [Desktop Entry]
 * group and every line of that group starting with `drop` or with the mark
 * left out. */
static void entry_rewrite(KbBuf *o, const char *text, const char *add,
			  const char *drop)
{
	int main_group = 0;
	for (const char *l = text; *l;) {
		size_t n = strcspn(l, "\n");
		if (*l == '[') {
			main_group = !strncmp(l, "[Desktop Entry]", 15);
			kb_buf_printf(o, "%.*s\n", (int)n, l);
			if (main_group && add)
				kb_buf_printf(o, "%s\n", add);
		} else if (!(main_group &&
			     ((drop && !strncmp(l, drop, strlen(drop))) ||
			      !strncmp(l, ALIAS_MARK,
				       sizeof(ALIAS_MARK) - 1)))) {
			kb_buf_printf(o, "%.*s\n", (int)n, l);
		}
		l += n;
		if (*l == '\n')
			l++;
	}
}

static char *alias_path(const char *stem)
{
	char rel[512];
	snprintf(rel, sizeof(rel), "applications/%s.desktop", stem);
	const char *dh = getenv("XDG_DATA_HOME");
	if (dh && *dh)
		return kb_path_join(dh, rel);
	char *base = kb_path_join(kb_home_dir(), ".local/share");
	char *p = kb_path_join(base, rel);
	free(base);
	return p;
}

/* 1 when one of XDG_DATA_DIRS holds applications/<stem>.desktop. */
static int host_entry(const char *stem)
{
	const char *dirs = getenv("XDG_DATA_DIRS");
	if (!dirs || !*dirs)
		dirs = "/usr/local/share:/usr/share";
	for (const char *d = dirs; *d;) {
		size_t n = strcspn(d, ":");
		if (n) {
			char p[1024];
			snprintf(p, sizeof(p), "%.*s/applications/%s.desktop",
				 (int)n, d, stem);
			if (kb_path_exists(p))
				return 1;
		}
		d += n;
		if (*d == ':')
			d++;
	}
	return 0;
}

/* 1 when `path` is the copy this box's export made. */
static int alias_ours(const char *path, const char *box)
{
	char *text = kb_read_whole(path, NULL);
	if (!text)
		return 0;
	KbBuf want = {0};
	kb_buf_printf(&want, "\n" ALIAS_MARK "%s\n", box);
	int ours = strstr(text, want.p) != NULL;
	kb_buf_free(&want);
	free(text);
	return ours;
}

static void write_alias(const char *box, const char *path, const char *stem)
{
	char rel[1024];
	snprintf(rel, sizeof(rel), ".local/share/applications/%s-%s", box,
		 kb_basename(path));
	char *exp = kb_path_join(kb_home_dir(), rel);
	char *alias = alias_path(stem);
	char *text = NULL;

	if (host_entry(stem) ||
	    (kb_path_exists(alias) && !alias_ours(alias, box)) ||
	    !(text = kb_read_whole(exp, NULL)))
		goto out;

	/* The copy first: the export is hidden only once the entry that
	 * replaces it in the menu is on disk. */
	KbBuf o = {0}, mark = {0}, hid = {0};
	kb_buf_printf(&mark, ALIAS_MARK "%s", box);
	entry_rewrite(&o, text, mark.p, NULL);
	if (kb_write_file_atomic(alias, o.p ? o.p : "") != 0) {
		kb_warn("could not write %s", alias);
	} else {
		entry_rewrite(&hid, text, "NoDisplay=true", "NoDisplay=");
		if (kb_write_file_atomic(exp, hid.p ? hid.p : "") != 0)
			kb_warn("could not hide %s", exp);
	}
	kb_buf_free(&hid);
	kb_buf_free(&mark);
	kb_buf_free(&o);
out:
	free(text);
	free(alias);
	free(exp);
}

/*
 * THE ALIAS IS DECIDED BEFORE THE EXPORT IS DELETED. `distrobox-export --app
 * <x> --delete` removes `applications/<stem>.desktop` as well as the box's
 * `<box>-<stem>.desktop` — the name the alias has — so the alias is gone by
 * the time the export's delete returns, and a check made then finds nothing
 * and leaves the alias's pin behind as a blank icon. alias_take looks at the
 * name first, without following a link: this box's alias (a regular file,
 * as write_alias leaves it) is noted for alias_put to unlink and unpin; any
 * other file of that name (the user's own entry, another box's alias) is
 * kept, bytes and mode, and a symbolic link is kept as its target, for
 * alias_put to put back as it was. A file that cannot be read is named in a
 * warning, since the delete takes it.
 */
typedef struct {
	char *path;
	char *text;	/* a file that is not this box's alias, or NULL */
	char *link;	/* the target of a symbolic link of that name, or NULL */
	mode_t mode;
	int ours;
} AliasHold;

static void alias_take(AliasHold *h, const char *box, const char *stem)
{
	struct stat st;

	h->path = alias_path(stem);
	h->text = NULL;
	h->link = NULL;
	h->mode = 0644;
	h->ours = 0;
	if (lstat(h->path, &st) != 0)
		return;
	if (S_ISLNK(st.st_mode)) {
		char t[4096];
		ssize_t n = readlink(h->path, t, sizeof(t) - 1);
		if (n < 0 || (size_t)n >= sizeof(t) - 1) {
			kb_warn("could not read the link %s; removing the "
				"export removes it", h->path);
			return;
		}
		t[n] = '\0';
		h->link = strdup(t);
		return;
	}
	if (!S_ISREG(st.st_mode))
		return;
	h->ours = alias_ours(h->path, box);
	if (h->ours)
		return;
	h->text = kb_read_whole(h->path, NULL);
	h->mode = st.st_mode & 07777;
	if (!h->text)
		kb_warn("could not read %s; removing the export removes it",
			h->path);
}

/* 1 when this box's alias was there: a pin of `<stem>.desktop` is then this
 * box's launcher, and goes with it. */
static int alias_put(AliasHold *h)
{
	struct stat st;
	int ours = h->ours;

	if (ours && unlink(h->path) != 0 && errno != ENOENT)
		kb_warn("could not remove %s: %s", h->path, strerror(errno));
	if ((h->text || h->link) && lstat(h->path, &st) != 0) {
		if (h->link ? symlink(h->link, h->path) != 0
			    : (kb_write_file_atomic(h->path, h->text) != 0 ||
			       chmod(h->path, h->mode) != 0))
			kb_warn("could not put back %s: %s", h->path,
				strerror(errno));
	}
	free(h->link);
	free(h->text);
	free(h->path);
	return ours;
}

/*
 * THE PINS OF DELETED LAUNCHERS, OUT OF EVERY TASK MANAGER. `drop` is a JS
 * array literal of the launcher URLs, each built by kb_json_str and never
 * spliced in as raw text: a desktop file's name is the package's, and a
 * quote in it would otherwise end the string and run as script. An entry
 * is matched on its URL: a pin kept to some activities is stored as
 * `[<activity ids>]\n<url>`, and compared whole it would survive the
 * removal as a blank icon in those activities. A list is written back, and
 * the applet told to reread it, only where it changed.
 */
static const char UNPIN_JS[] =
	"let n = 0;"
	"panels().concat(desktops()).forEach(c => c.widgets().forEach(x => {"
	" if (x.type != 'org.kde.plasma.icontasks' &&"
	"     x.type != 'org.kde.plasma.taskmanager') return;"
	" x.currentConfigGroup = ['General'];"
	" let v = x.readConfig('launchers', []);"
	" let a = typeof v == 'string' ? v.split(',').filter(u => u)"
	"  : Array.from(v, String);"
	" let k = a.filter(u =>"
	"  drop.indexOf(u.replace(/^\\[[^\\]]*\\]\\n/, '')) < 0);"
	" if (k.length == a.length) return;"
	" x.writeConfig('launchers', k); x.reloadConfig(); n++; }));"
	"print(n);";

/* `applications:<file>` and the file:// URL of its path in the user's
 * applications directory, the two forms a pin of it takes. */
static void unpin_add(KbBuf *drop, const char *file)
{
	KbBuf u = {0};
	char *dir = alias_path("x");
	char *slash = strrchr(dir, '/');
	if (slash)
		*slash = 0;
	kb_buf_printf(&u, "applications:%s", file);
	if (drop->n && drop->p[drop->n - 1] != '[')
		kb_buf_str(drop, ",");
	kb_json_str(drop, u.p);
	u.n = 0;
	kb_buf_printf(&u, "file://%s/%s", dir, file);
	kb_buf_str(drop, ",");
	kb_json_str(drop, u.p);
	kb_buf_free(&u);
	free(dir);
}

/* Quiet when plasmashell is not answering: no session, nothing pinned. */
static void unpin_launchers(KbBuf *drop)
{
	KdtBus *b = kdt_bus_open(0);
	if (b && kdt_bus_has_owner(b, "org.kde.plasmashell") == 1) {
		char err[256] = "";
		KbBuf js = {0};
		kb_buf_printf(&js, "%s];%s", drop->p, UNPIN_JS);
		char *out = kdt_bus_call_s(b, "org.kde.plasmashell",
					   "/PlasmaShell", "org.kde.PlasmaShell",
					   "evaluateScript", err, sizeof(err),
					   "s", js.p);
		if (out && atoi(out) > 0)
			printf("==> Unpinned from %d task manager%s\n",
			       atoi(out), atoi(out) == 1 ? "" : "s");
		free(out);
		kb_buf_free(&js);
	}
	kdt_bus_close(b);
}

/* `distrobox enter <box> -- distrobox-export <kind> <what> [extra]`. */
static int box_export(const char *box, const char *kind, const char *what,
		      const char *extra, const char *extra2)
{
	KbArgv a = {0};
	kb_argv_add(&a, "distrobox");
	kb_argv_add(&a, "enter");
	kb_argv_add(&a, box);
	kb_argv_add(&a, "--");
	kb_argv_add(&a, "distrobox-export");
	kb_argv_add(&a, kind);
	kb_argv_add(&a, what);
	if (extra)
		kb_argv_add(&a, extra);
	if (extra2)
		kb_argv_add(&a, extra2);
	kb_argv_end(&a);
	return kb_run(&a);
}

/*
 * A distrobox command with its stderr relayed through this process, to take
 * out lines of distrobox's own that would contradict what follows them.
 *
 * BOX_CREATE: `distrobox create`, with its closing advice taken out. It ends
 * on stderr with "Distrobox '<box>' successfully created." and "To enter,
 * run:", and puts the command that line introduces on a stdout that goes to
 * /dev/null: relayed whole, the user is left a sentence pointing at nothing.
 * Every other line reaches stderr as distrobox wrote it. The dropped line
 * opens with the reset of the "[ OK ]" before it, so the reset alone is
 * written in its place, or the terminal would stay green.
 *
 * BOX_ENTER: the enter that runs INSTALL_SH. Its stdout goes to /dev/null as
 * kb_run's does, so the package manager's output stays off the terminal as
 * the --remove enter's does; the script names the manager on stderr before
 * it runs, and that line reaches the terminal through this relay. The enter
 * runs verbose: an unverbose kb_run sends stderr to /dev/null as well, and
 * the user would see nothing for the manager's minutes. On a box's first
 * start distrobox-enter prints its setup steps and then
 * "\nContainer Setup Complete!\n" on stderr, and prints that line even when
 * distrobox-init's install of its dependencies failed; fetch-app's own lines
 * report the outcome, so those bytes alone are dropped. stderr is relayed in
 * read() chunks, not lines: each step label is printed without its newline
 * while the step runs, and must show while it does. At most the length of the
 * dropped text is held back, and only while the tail could still begin it.
 */
enum box_relay { BOX_CREATE, BOX_ENTER };

static const char SETUP_DONE[] = "\nContainer Setup Complete!\n";

static void relay_create(FILE *f)
{
	char *line = NULL;
	size_t cap = 0;
	int after_enter = 0;
	while (getline(&line, &cap, f) >= 0) {
		if (after_enter && (!strcmp(line, "\n") || !strcmp(line, "\r\n"))) {
			after_enter = 0;
			continue;
		}
		after_enter = 0;
		if (strstr(line, "To enter, run:")) {
			after_enter = 1;
			continue;
		}
		if (strstr(line, "' successfully created.")) {
			if (strchr(line, '\033'))
				fputs("\033[0m", stderr);
			continue;
		}
		fputs(line, stderr);
	}
	free(line);
}

static void relay_put(const char *p, size_t n)
{
	fwrite(p, 1, n, stderr);
	fflush(stderr);
}

static void relay_enter(int fd)
{
	const size_t plen = sizeof(SETUP_DONE) - 1;
	char w[4096 + sizeof(SETUP_DONE)];
	size_t held = 0;
	for (;;) {
		ssize_t r = read(fd, w + held, sizeof(w) - held);
		if (r < 0 && errno == EINTR)
			continue;
		if (r <= 0)
			break;
		size_t n = held + (size_t)r, pos = 0;
		for (;;) {
			char *m = memmem(w + pos, n - pos, SETUP_DONE, plen);
			if (!m)
				break;
			relay_put(w + pos, (size_t)(m - (w + pos)));
			pos = (size_t)(m - w) + plen;
		}
		size_t keep = n;
		for (size_t k = n > pos + plen - 1 ? n - (plen - 1) : pos; k < n; k++)
			if (!memcmp(w + k, SETUP_DONE, n - k)) {
				keep = k;
				break;
			}
		relay_put(w + pos, keep - pos);
		held = n - keep;
		memmove(w, w + keep, held);
	}
	relay_put(w, held);
}

static int run_box_relay(const KbArgv *a, enum box_relay mode)
{
	if (mode == BOX_ENTER && !kb_proc_verbose)
		return kb_run(a);

	int pfd[2];
	if (pipe(pfd) != 0)
		kb_die("pipe: %s", strerror(errno));

	fflush(stdout);
	fflush(stderr);
	pid_t pid = fork();
	if (pid < 0)
		kb_die("fork: %s", strerror(errno));
	if (pid == 0) {
		int null = open("/dev/null", O_RDWR);
		if (null >= 0) {
			dup2(null, STDIN_FILENO);
			dup2(null, STDOUT_FILENO);
			if (null > STDERR_FILENO)
				close(null);
		}
		dup2(pfd[1], STDERR_FILENO);
		close(pfd[0]);
		close(pfd[1]);
		kb_child_reset_signals();
		execvp(a->v[0], (char *const *)a->v);
		_exit(127);
	}
	close(pfd[1]);

	if (mode == BOX_CREATE) {
		FILE *f = fdopen(pfd[0], "r");
		if (!f)
			kb_die("fdopen: %s", strerror(errno));
		relay_create(f);
		fclose(f);
	} else {
		relay_enter(pfd[0]);
		close(pfd[0]);
	}

	int st;
	while (waitpid(pid, &st, 0) < 0)
		if (errno != EINTR)
			return -1;
	return WIFEXITED(st) ? WEXITSTATUS(st) : 128 + WTERMSIG(st);
}

static int box_exists_named(const char *box)
{
	KbArgv a = {0};
	char buf[1 << 15];
	kb_argv_add(&a, "podman");
	kb_argv_add(&a, "ps");
	kb_argv_add(&a, "--all");
	kb_argv_add(&a, "--format");
	kb_argv_add(&a, "{{.Names}}");
	kb_argv_end(&a);
	if (kb_run_capture(&a, buf, sizeof(buf)) != 0)
		return 0;

	for (char *line = strtok(buf, "\n"); line; line = strtok(NULL, "\n"))
		if (!strcmp(line, box))
			return 1;
	return 0;
}

static void box_create(const char *box, const char *image)
{
	printf("==> Creating distrobox '%s' (image: %s)\n", box, image);
	KbArgv a = {0};
	kb_argv_add(&a, "distrobox");
	kb_argv_add(&a, "create");
	kb_argv_add(&a, "--name");
	kb_argv_add(&a, box);
	kb_argv_add(&a, "--image");
	kb_argv_add(&a, image);
	kb_argv_add(&a, "--yes");
	kb_argv_add(&a, "--no-entry");
	kb_argv_end(&a);
	if (run_box_relay(&a, BOX_CREATE) != 0)
		kb_die("could not create the box");
}

/* `distrobox rm --force <box>`; a failure is a warning, since the caller is
 * already on its way to an error of its own. */
static void box_remove(const char *box)
{
	printf("==> Removing distrobox '%s'\n", box);
	KbArgv r = {0};
	kb_argv_add(&r, "distrobox");
	kb_argv_add(&r, "rm");
	kb_argv_add(&r, "--force");
	kb_argv_add(&r, box);
	kb_argv_end(&r);
	int verbose = kb_proc_verbose;
	kb_proc_verbose = 0;
	if (kb_run(&r) != 0)
		kb_warn("could not remove the box '%s'", box);
	kb_proc_verbose = verbose;
}

static void fetch_app_usage(void)
{
	printf("Usage: kdos-fetch-app [--box <name>] [--image <image>] <app>\n"
	       "       kdos-fetch-app --remove [--box <name>] <app>\n"
	       "\n"
	       "Defaults:\n"
	       "  --box    %s\n"
	       "  --image  %s  (only used if the box doesn't exist yet)\n"
	       "\n"
	       "The image needs apt-get, dnf or pacman.\n"
	       "\n"
	       "Examples:\n"
	       "  kdos-fetch-app firefox-esr\n"
	       "  kdos-fetch-app --box arch --image archlinux:latest neovim\n"
	       "  kdos-fetch-app --remove firefox-esr\n",
	       BOX_DEFAULT, IMAGE_DEFAULT);
}

int fetch_app_main(int argc, char **argv)
{
	const char *box = BOX_DEFAULT, *image = IMAGE_DEFAULT, *app = NULL;
	int remove = 0, created = 0;

	/* Line-buffered, because every `==>` line is followed by a child that
	 * writes to the same terminal or pipe: fully buffered, the lines would
	 * arrive after the children's output, or after the error they announce. */
	setvbuf(stdout, NULL, _IOLBF, 0);

	for (int i = 1; i < argc; i++) {
		if (!strcmp(argv[i], "--box") && i + 1 < argc)
			box = argv[++i];
		else if (!strcmp(argv[i], "--image") && i + 1 < argc)
			image = argv[++i];
		else if (!strcmp(argv[i], "--remove"))
			remove = 1;
		else if (!strcmp(argv[i], "-h") || !strcmp(argv[i], "--help")) {
			fetch_app_usage();
			return 0;
		} else if (argv[i][0] == '-')
			kb_die("unknown flag: %s", argv[i]);
		else
			app = argv[i];
	}

	if (!app) {
		fetch_app_usage();
		return 1;
	}
	if (!kb_have_prog("distrobox"))
		kb_die("distrobox not installed");
	if (!kb_have_prog("podman"))
		kb_die("podman not installed");

	if (!box_exists_named(box)) {
		if (remove)
			kb_die("box '%s' does not exist", box);
		box_create(box, image);
		created = 1;
	}
	kb_proc_verbose = 1;

	PkgExports x;

	if (remove) {
		KbArgv q = {0};
		box_sh(&q, box, INSTALLED_SH, app);
		kb_proc_verbose = 0;
		if (kb_run(&q) != 0)
			kb_die("'%s' is not installed inside '%s'", app, box);

		/* Quiet: distrobox-export's errors for an export the user
		 * already deleted by hand say nothing the line below does not. */
		pkg_exports(box, app, &x);
		KbBuf drop = {0};
		kb_buf_str(&drop, "let drop = [");
		for (int i = 0; i < x.napp; i++) {
			char stem[256], file[512];
			printf("==> Removing exported launcher '%s'\n",
			       app_stem(x.app[i], stem, sizeof(stem)));
			AliasHold hold;
			alias_take(&hold, box, stem);
			if (box_export(box, "--app", x.app[i], "--delete",
				       NULL) != 0)
				printf("    (no launcher export to remove)\n");
			snprintf(file, sizeof(file), "%s-%s", box,
				 kb_basename(x.app[i]));
			unpin_add(&drop, file);
			if (alias_put(&hold)) {
				snprintf(file, sizeof(file), "%s.desktop", stem);
				unpin_add(&drop, file);
			}
		}
		if (x.napp)
			unpin_launchers(&drop);
		kb_buf_free(&drop);
		for (int i = 0; i < x.nbin; i++) {
			printf("==> Removing CLI wrapper '%s'\n",
			       kb_basename(x.bin[i]));
			if (box_export(box, "--bin", x.bin[i], "--delete",
				       NULL) != 0)
				printf("    (no CLI wrapper to remove)\n");
		}
		pkg_exports_free(&x);
		kb_proc_verbose = 1;

		printf("==> Uninstalling '%s' inside '%s'\n", app, box);
		KbArgv c = {0};
		box_sh(&c, box, REMOVE_SH, app);
		return kb_run(&c);
	}

	printf("==> Installing '%s' inside '%s'\n", app, box);
	KbArgv ins = {0};
	box_sh(&ins, box, INSTALL_SH, app);
	int rc = run_box_relay(&ins, BOX_ENTER);
	/*
	 * A box whose first setup never finished is removed when this run
	 * made it, so the next run online starts it afresh. The default box
	 * found in that state is recreated once: it holds nothing of the
	 * user's, since no install in it can have succeeded. A box named
	 * with --box is the user's, and is left for them to remove.
	 */
	if (rc == BOX_UNSET && !created) {
		if (strcmp(box, BOX_DEFAULT))
			kb_die("'%s' never finished its first setup and has no "
			       "sudo; remove it with `distrobox rm %s` and run "
			       "again when online", box, box);
		printf("==> '%s' never finished its first setup\n", box);
		box_remove(box);
		/* A box the remove could not take would make the create
		 * below fail with nothing to say how to get out. */
		if (box_exists_named(box))
			kb_die("'%s' never finished its first setup and could "
			       "not be removed; remove it with `distrobox rm "
			       "%s` and run again when online", box, box);
		box_create(box, image);
		created = 1;
		printf("==> Installing '%s' inside '%s'\n", app, box);
		rc = run_box_relay(&ins, BOX_ENTER);
	}
	if (rc == BOX_UNSET) {
		box_remove(box);
		kb_die("the box %s did not finish its first setup — the "
		       "network is needed the first time; run again when "
		       "online", box);
	}
	if (rc == NO_PKG_MANAGER) {
		/* A box this run made for an image it cannot drive is of no
		 * use to anyone and would otherwise stay running. A box that
		 * was already there is the user's, and stays. */
		if (!created)
			kb_die("'%s' has none of apt-get, dnf or pacman", box);
		box_remove(box);
		kb_die("%s has none of apt-get, dnf or pacman", image);
	}
	if (rc != 0)
		kb_die("install failed inside '%s'", box);

	/* Neither kind of export is fatal on its own: a GUI app has a desktop
	 * file but often no command of its own, a CLI tool has the reverse,
	 * and plenty have both. */
	int exported = 0, launchers = 0;
	pkg_exports(box, app, &x);

	for (int i = 0; i < x.napp; i++) {
		char stem[256];
		app_stem(x.app[i], stem, sizeof(stem));
		printf("==> Exporting launcher '%s'\n", stem);
		if (box_export(box, "--app", x.app[i], NULL, NULL) == 0) {
			fix_wm_class(box, x.app[i], stem);
			write_alias(box, x.app[i], stem);
			launchers++;
		} else {
			printf("    could not export the launcher '%s'\n", stem);
		}
	}

	char *dir = kb_path_join(kb_home_dir(), ".local/bin");
	if (x.nbin)
		kb_mkdir_p(dir);
	KbBuf cmds = {0};
	for (int i = 0; i < x.nbin; i++) {
		const char *name = kb_basename(x.bin[i]);
		printf("==> Exporting command '%s'\n", name);
		if (box_export(box, "--bin", x.bin[i], "--export-path",
			       dir) == 0) {
			exported++;
			kb_buf_printf(&cmds, "%s'%s'", cmds.n ? ", " : "",
				      name);
		} else {
			printf("    could not export a CLI wrapper for '%s'\n",
			       name);
		}
	}
	free(dir);
	pkg_exports_free(&x);

	if (!exported && !launchers)
		kb_die("'%s' installs no desktop file under "
		       "/usr/share/applications and no command under /usr/bin",
		       app);

	printf("\nDone.\n");
	if (launchers)
		printf("  launcher : ~/.local/share/applications/  "
		       "(Super opens the KDOS launcher)\n");
	if (cmds.n)
		printf("  command  : %s — ~/.local/bin is on PATH\n", cmds.p);
	kb_buf_free(&cmds);
	return 0;
}

/* ──────────────────────────────────────────────────────────────────────── */

static int ends_with(const char *s, const char *suffix)
{
	size_t n = strlen(s), m = strlen(suffix);
	return n >= m && !strcmp(s + n - m, suffix);
}

int fetch_static_main(int argc, char **argv)
{
	if (argc != 4) {
		printf("Usage: kdos-fetch-static <name> <url> <sha256>\n"
		       "\n"
		       "Downloads <url>, checks its sha256, and installs the "
		       "binary\n(or extracts <name> from a tarball) into %s/"
		       "<name>.\n"
		       "\n"
		       "Tarball detection by extension: .tar, .tar.gz, .tgz, "
		       ".tar.xz, .tar.bz2, .tar.zst\nPlain binary: anything "
		       "else.\n", STATIC_DEST);
		return 1;
	}

	const char *name = argv[1], *url = argv[2], *want = argv[3];
	if (geteuid() != 0)
		kb_die("must run as root (writes to %s)", STATIC_DEST);

	char tmp[] = "/tmp/kdos-fetch-static.XXXXXX";
	if (!mkdtemp(tmp))
		kb_die("cannot create a work directory: %s", strerror(errno));

	char *archive = kb_path_join(tmp, kb_basename(url));

	printf("==> Downloading %s\n", url);
	kb_proc_verbose = 1;
	KbArgv c = {0};
	kb_argv_add(&c, "curl");
	kb_argv_add(&c, "-fL");
	kb_argv_add(&c, "-o");
	kb_argv_add(&c, archive);
	kb_argv_add(&c, "--");
	kb_argv_add(&c, url);
	kb_argv_end(&c);
	if (kb_run(&c) != 0)
		kb_die("download failed");

	printf("==> Verifying sha256\n");
	char sum[256] = {0};
	KbArgv s = {0};
	kb_argv_add(&s, "sha256sum");
	kb_argv_add(&s, "--");
	kb_argv_add(&s, archive);
	kb_argv_end(&s);
	if (kb_run_capture(&s, sum, sizeof(sum)) != 0)
		kb_die("sha256sum failed");
	char *sp = strchr(sum, ' ');
	if (sp)
		*sp = 0;
	if (strcmp(sum, want))
		kb_die("sha256 mismatch\n  expected: %s\n  actual:   %s", want,
		       sum);

	char *dest = kb_path_join(STATIC_DEST, name);

	const char *taropt = NULL;
	if (ends_with(archive, ".tar.gz") || ends_with(archive, ".tgz"))
		taropt = "-xzf";
	else if (ends_with(archive, ".tar.xz"))
		taropt = "-xJf";
	else if (ends_with(archive, ".tar.bz2"))
		taropt = "-xjf";
	else if (ends_with(archive, ".tar.zst"))
		taropt = "--zstd";
	else if (ends_with(archive, ".tar"))
		taropt = "-xf";

	if (!taropt) {
		KbArgv i = {0};
		kb_argv_add(&i, "install");
		kb_argv_add(&i, "-Dm755");
		kb_argv_add(&i, archive);
		kb_argv_add(&i, dest);
		kb_argv_end(&i);
		if (kb_run(&i) != 0)
			kb_die("could not install %s", dest);
		printf("==> Installed %s\n", dest);
		return 0;
	}

	KbArgv t = {0};
	kb_argv_add(&t, "tar");
	if (!strcmp(taropt, "--zstd")) {
		kb_argv_add(&t, "--zstd");
		kb_argv_add(&t, "-xf");
	} else {
		kb_argv_add(&t, taropt);
	}
	kb_argv_add(&t, archive);
	kb_argv_add(&t, "-C");
	kb_argv_add(&t, tmp);
	kb_argv_end(&t);
	if (kb_run(&t) != 0)
		kb_die("could not extract %s", archive);

	/* Locate <name> anywhere under the extraction dir. */
	char found[1024] = {0};
	KbArgv f = {0};
	kb_argv_add(&f, "find");
	kb_argv_add(&f, tmp);
	kb_argv_add(&f, "-type");
	kb_argv_add(&f, "f");
	kb_argv_add(&f, "-name");
	kb_argv_add(&f, name);
	kb_argv_add(&f, "-perm");
	kb_argv_add(&f, "-u+x");
	kb_argv_end(&f);
	kb_proc_verbose = 0;
	kb_run_capture(&f, found, sizeof(found));
	kb_proc_verbose = 1;
	char *nl = strchr(found, '\n');
	if (nl)
		*nl = 0;
	if (!found[0])
		kb_die("could not find binary '%s' inside the tarball", name);

	KbArgv i = {0};
	kb_argv_add(&i, "install");
	kb_argv_add(&i, "-Dm755");
	kb_argv_add(&i, found);
	kb_argv_add(&i, dest);
	kb_argv_end(&i);
	if (kb_run(&i) != 0)
		kb_die("could not install %s", dest);

	printf("==> Installed %s\n", dest);
	free(archive);
	free(dest);
	return 0;
}
