/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   kinstall-core — the files that make the machine a KDOS Plasma desktop
 *
 * The login, the look and the keyboard of the installed machine are each a
 * handful of small files that other programs own the format of: SDDM reads
 * kde_settings.conf, startplasma reads kdeglobals, layout.js reads kdosrc,
 * kdos-settingsd reads xkb.conf, KWin reads kxkbrc. Every writer here edits
 * the file it is given IN PLACE where the image ships one — the key it owns
 * replaced, every other line and comment kept — because the shipped files
 * carry the explanation of what they do, and a file rewritten whole would
 * leave the installed machine with a configuration nobody can read.
 *
 * THE /etc/xdg FILES ARE THE INSTALLER'S AND NOBODY ELSE'S. KDOS's defaults
 * live in /usr/share/kdos/xdg, last in XDG_CONFIG_DIRS; /etc/xdg is the
 * machine owner's, and the installer is the one writer that puts the owner's
 * answers there — the accent, the desk and the keyboard — where they outrank
 * the distribution's and are themselves outranked by each person's own.
 * ---------------------------------
 */

#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>

#include "kinstall-core.h"

void (*ki_log_hook)(const char *line);

static void say(const char *fmt, ...) __attribute__((format(printf, 1, 2)));

static void say(const char *fmt, ...)
{
	char buf[1024];
	va_list ap;

	if (!ki_log_hook)
		return;
	va_start(ap, fmt);
	vsnprintf(buf, sizeof(buf), fmt, ap);
	va_end(ap);
	ki_log_hook(buf);
}

static int oops(char *err, size_t n, const char *fmt, ...)
	__attribute__((format(printf, 3, 4)));

static int oops(char *err, size_t n, const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	vsnprintf(err, n, fmt, ap);
	va_end(ap);
	return -1;
}

/* `rel` under `root`. A path that would not fit is refused, never cut: a
 * shortened path names a different file. */
static int under(char *out, size_t cap, const char *root, const char *rel)
{
	int w = snprintf(out, cap, "%s%s", root, rel);

	return w < 0 || (size_t)w >= cap ? -1 : 0;
}

static int parent_dirs(const char *path)
{
	char dir[1024];
	char *slash;

	kb_strlcpy(dir, path, sizeof(dir));
	slash = strrchr(dir, '/');
	if (!slash || slash == dir)
		return 0;
	*slash = 0;
	return kb_mkdir_p(dir);
}

/* The whole file, replaced by a rename, then given `mode`. */
static int put(const char *root, const char *rel, const char *text, mode_t mode,
	       char *err, size_t n)
{
	char p[1024];

	if (under(p, sizeof(p), root, rel) < 0)
		return oops(err, n, "%s%s: path too long", root, rel);
	say("write %s", p);
	if (cfg.dry_run)
		return 0;
	if (parent_dirs(p) < 0 || kb_write_file_atomic(p, text) < 0)
		return oops(err, n, "cannot write %s: %s", p, strerror(errno));
	if (chmod(p, mode) < 0)
		return oops(err, n, "cannot set the mode of %s: %s", p,
			    strerror(errno));
	return 0;
}

static void drop(const char *root, const char *rel)
{
	char p[1024];

	if (under(p, sizeof(p), root, rel) < 0 || !kb_path_exists(p))
		return;
	say("remove %s", p);
	if (!cfg.dry_run)
		unlink(p);
}

/* The file under `root`, or "" when there is none: an absent file is edited
 * as an empty one. The caller frees. */
static char *slurp(const char *root, const char *rel)
{
	char p[1024];
	char *s;

	if (under(p, sizeof(p), root, rel) < 0)
		return kb_strdup("");
	s = kb_read_all(p, NULL);
	return s ? s : kb_strdup("");
}

/* ── INI and shell-variable files, edited a line at a time ─────────────── */

/* One line of `s`: its start and length without the newline. 0 at the end. */
static int next_line(const char **s, const char **line, size_t *len)
{
	const char *p = *s, *nl;

	if (!*p)
		return 0;
	nl = strchr(p, '\n');
	*line = p;
	*len = nl ? (size_t)(nl - p) : strlen(p);
	*s = nl ? nl + 1 : p + *len;
	return 1;
}

/* Whether a line is the header of group `g`, written `[g]` at its start. */
static int is_header(const char *line, size_t len, const char *g)
{
	size_t gl = strlen(g);

	return len >= gl + 2 && line[0] == '[' && !strncmp(line + 1, g, gl) &&
	       line[gl + 1] == ']';
}

static int is_any_header(const char *line, size_t len)
{
	return len && line[0] == '[';
}

/* The key a `key=value` line sets, matched whole: `Layout` is not
 * `LayoutList`. */
static int is_key(const char *line, size_t len, const char *k)
{
	size_t kl = strlen(k);

	while (len && (*line == ' ' || *line == '\t')) {
		line++;
		len--;
	}
	if (len <= kl || strncmp(line, k, kl))
		return 0;
	line += kl;
	len -= kl;
	while (len && (*line == ' ' || *line == '\t')) {
		line++;
		len--;
	}
	return len && *line == '=';
}

/* Trailing blank lines off a buffer, so a group appended after them is
 * separated by exactly one. */
static void trim_blank_tail(KbBuf *b)
{
	while (b->n >= 2 && b->p[b->n - 1] == '\n' && b->p[b->n - 2] == '\n')
		b->p[--b->n] = 0;
}

/*
 * GROUP `g` REPLACED WHOLE BY `body`, or removed when `body` is NULL. A group
 * is its header and every line up to the next header; the lines before the
 * first header and every other group are kept as they are.
 */
static void ini_group(const char *text, const char *g, const char *body,
		      KbBuf *out)
{
	const char *s = text, *line;
	size_t len;
	int skipping = 0;

	kb_buf_str(out, "");
	while (next_line(&s, &line, &len)) {
		if (is_any_header(line, len))
			skipping = is_header(line, len, g);
		if (skipping)
			continue;
		kb_buf_add(out, line, len);
		kb_buf_str(out, "\n");
	}
	if (!body)
		return;
	trim_blank_tail(out);
	if (out->n && out->p[out->n - 1] != '\n')
		kb_buf_str(out, "\n");
	if (out->n)
		kb_buf_str(out, "\n");
	kb_buf_printf(out, "[%s]\n%s", g, body);
}

/*
 * ONE KEY OF GROUP `g` SET TO `v`: the first `k=` line of the group replaced,
 * every later one dropped (KConfig honours the last, so a second line would
 * win), and the key added at the group's end — or the group at the file's
 * end — when absent.
 */
static void ini_key(const char *text, const char *g, const char *k,
		    const char *v, KbBuf *out)
{
	const char *s = text, *line;
	size_t len;
	int in = 0, done = 0, seen_group = 0;

	kb_buf_str(out, "");
	while (next_line(&s, &line, &len)) {
		if (is_any_header(line, len)) {
			if (in && !done) {
				trim_blank_tail(out);
				kb_buf_printf(out, "%s=%s\n\n", k, v);
				done = 1;
			}
			in = is_header(line, len, g);
			seen_group |= in;
		} else if (in && is_key(line, len, k)) {
			if (!done)
				kb_buf_printf(out, "%s=%s\n", k, v);
			done = 1;
			continue;
		}
		kb_buf_add(out, line, len);
		kb_buf_str(out, "\n");
	}
	if (done)
		return;
	if (seen_group && in) {
		trim_blank_tail(out);
		kb_buf_printf(out, "%s=%s\n", k, v);
		return;
	}
	trim_blank_tail(out);
	if (out->n)
		kb_buf_str(out, "\n");
	kb_buf_printf(out, "[%s]\n%s=%s\n", g, k, v);
}

/* A `KEY=value` line of a file that is also shell, set; appended when
 * absent. Comment lines are never matched, so the documentation of a key
 * survives its value being changed. */
static void shell_key(const char *text, const char *k, const char *v,
		      KbBuf *out)
{
	const char *s = text, *line;
	size_t len, kl = strlen(k);
	int done = 0;

	kb_buf_str(out, "");
	while (next_line(&s, &line, &len)) {
		if (len > kl && !strncmp(line, k, kl) && line[kl] == '=') {
			if (!done)
				kb_buf_printf(out, "%s=%s\n", k, v);
			done = 1;
			continue;
		}
		kb_buf_add(out, line, len);
		kb_buf_str(out, "\n");
	}
	if (!done)
		kb_buf_printf(out, "%s=%s\n", k, v);
}

/* ── the login ─────────────────────────────────────────────────────────── */

#define SDDM_SETTINGS "/etc/sddm.conf.d/kde_settings.conf"
#define AUTOLOGIN_XDG "/etc/kdos/autologin-xdg"
#define PLASMA_SETUP_DONE "/etc/plasma-setup-done"

/*
 * kde_settings.conf IS THE ONE HOME OF [Autologin], the file System
 * Settings → Login Screen writes too, so the installer and the settings page
 * never disagree about whether the machine logs in by itself. The image ships
 * the live medium's group (the live account, Relogin=true); every install
 * replaces it — with this account's on the encrypted single-account machine
 * that asked for it, and with nothing anywhere else, which shows the login
 * screen. `Relogin=false`: a person who logs out meant to.
 *
 * THE LOCK IS NOT A KEY OF kscreenlockerrc ANYWHERE A PASSWORD LOGIN READS.
 * SDDM's autologin PAM stack puts AUTOLOGIN_XDG first in XDG_CONFIG_DIRS for
 * an automatic login and for nothing else, and its kscreenlockerrc carries
 * `LockOnStart=true` alone: the session comes up behind the lock screen.
 * Turning autologin off in System Settings later then cannot leave every
 * password login starting locked as well. "No password after the disk
 * passphrase" is the directory left out.
 *
 * /etc/plasma-setup-done IS WHAT KEEPS plasma-setup ASLEEP. Its boot step
 * arms the first-boot wizard on a machine without the file, so every install
 * that made an account writes it, and an OEM install leaves it absent.
 */
int ki_write_login(const char *root, char *err, size_t n)
{
	char *old = slurp(root, SDDM_SETTINGS);
	KbBuf out = {0};
	char body[256];
	int rc;
	int auto_on = cfg.autologin && !cfg.oem && cfg.luks;

	snprintf(body, sizeof(body), "Relogin=false\nSession=plasma\nUser=%s\n",
		 cfg.username);
	ini_group(old, "Autologin", auto_on ? body : NULL, &out);
	free(old);
	rc = put(root, SDDM_SETTINGS, out.p, 0644, err, n);
	kb_buf_free(&out);
	if (rc)
		return rc;

	if (auto_on && cfg.autologin_lock) {
		rc = put(root, AUTOLOGIN_XDG "/kscreenlockerrc",
			 "[Daemon]\nLockOnStart=true\n", 0644, err, n);
		if (rc)
			return rc;
	} else {
		char p[1024];

		drop(root, AUTOLOGIN_XDG "/kscreenlockerrc");
		if (!cfg.dry_run && under(p, sizeof(p), root, AUTOLOGIN_XDG) == 0)
			rmdir(p);
	}

	if (cfg.oem) {
		drop(root, PLASMA_SETUP_DONE);
		return 0;
	}
	return put(root, PLASMA_SETUP_DONE, "", 0644, err, n);
}

/* ── the look ──────────────────────────────────────────────────────────── */

#define GREETER_SETS "/usr/share/kdos/greeter"
#define GREETER_USER "sddm"

static const char *const GREETER_FILES[] = {
	"kdeglobals", "plasmarc", "kcminputrc", NULL
};

/* The account's uid, gid and home out of the TARGET's passwd, which is the
 * machine that will run the greeter; the live system's may differ. Split by
 * position, because an empty field is two colons in a row. */
static int target_account(const char *root, const char *name, uid_t *uid,
			  gid_t *gid, char *home, size_t hcap)
{
	char *pw = slurp(root, "/etc/passwd");
	const char *s = pw, *line;
	size_t len, nl = strlen(name);
	int found = -1;

	while (found < 0 && next_line(&s, &line, &len)) {
		char l[1024];
		char *field[7] = { 0 };
		char *p;
		int nf = 0;

		if (len < nl + 1 || strncmp(line, name, nl) || line[nl] != ':' ||
		    len >= sizeof(l))
			continue;
		memcpy(l, line, len);
		l[len] = 0;
		for (p = l; nf < 7;) {
			char *c = strchr(p, ':');

			field[nf++] = p;
			if (!c)
				break;
			*c = 0;
			p = c + 1;
		}
		if (nf < 6)
			continue;
		*uid = (uid_t)strtoul(field[2], NULL, 10);
		*gid = (gid_t)strtoul(field[3], NULL, 10);
		kb_strlcpy(home, field[5], hcap);
		found = 0;
	}
	free(pw);
	return found;
}

/* kdos-dm's rule for the greeter's home: /var/lib/<something>, with no `.`
 * or `..` component, so a home of / or one outside /var/lib is never handed
 * to the account. */
static int greeter_home_ok(const char *h)
{
	if (strncmp(h, "/var/lib/", 9) || !h[9])
		return 0;
	for (const char *p = h; (p = strchr(p, '/')); p++)
		if (p[1] == '.' &&
		    (!p[2] || p[2] == '/' ||
		     (p[2] == '.' && (!p[3] || p[3] == '/'))))
			return 0;
	return 1;
}

/*
 * HANDED TO THE ACCOUNT ONLY BY ROOT. An install runs as root and always
 * hands the files over; a process that is not root cannot install at all
 * (the Prepare step refuses it), so the one caller that skips the hand-over
 * is a test writing into a fixture it owns.
 */
static int hand_over(const char *path, uid_t uid, gid_t gid)
{
	return geteuid() == 0 ? chown(path, uid, gid) : 0;
}

static int copy_owned(const char *src, const char *dst, uid_t uid, gid_t gid)
{
	say("copy %s -> %s", src, dst);
	if (cfg.dry_run)
		return 0;
	if (parent_dirs(dst) < 0 || kb_copy_file(src, dst) < 0)
		return -1;
	if (chmod(dst, 0644) < 0 || hand_over(dst, uid, gid) < 0)
		return -1;
	return 0;
}

/*
 * THE LOGIN SCREEN IN THE ACCENT, as kdos-powerd's `accent` verb lays it
 * down: the accent's set of three files into the sddm account's ~/.config and
 * its background beside them, owned by the account, whose greeter KWin writes
 * its output configuration there. ALL OR NOTHING ON THE SOURCE: a set missing
 * a file leaves the greeter in the image's accent rather than half in two. A
 * machine with no such account or no sets has no KDOS login screen to paint,
 * which is not a failure of the install.
 *
 * A DRY RUN ASKS THE LIVE ROOT. Its target was never copied into, so the sets
 * and the account are looked up in the live root, what the copy would have
 * carried; asked of the empty target, every dry run would report a login
 * screen the real install paints as missing.
 */
static void greeter_set(const char *root, const KcolScheme *sc)
{
	char set[1024], home[256], src[1280], dst[1280], dir[1024];
	const char *facts = cfg.dry_run ? "" : root;
	uid_t uid = 0;
	gid_t gid = 0;

	if (under(set, sizeof(set), facts, GREETER_SETS) < 0)
		return;
	kb_strlcpy(set + strlen(set), "/", sizeof(set) - strlen(set));
	kb_strlcpy(set + strlen(set), sc->name, sizeof(set) - strlen(set));
	for (int i = 0; GREETER_FILES[i]; i++) {
		snprintf(src, sizeof(src), "%s/%s", set, GREETER_FILES[i]);
		if (!kb_path_exists(src)) {
			say("no login-screen set for %s (%s missing); the "
			    "login screen keeps the image's accent",
			    sc->name, src);
			return;
		}
	}
	snprintf(src, sizeof(src), "%s/background.png", set);
	if (!kb_path_exists(src)) {
		say("no login-screen background for %s; the login screen "
		    "keeps the image's accent", sc->name);
		return;
	}
	if (target_account(facts, GREETER_USER, &uid, &gid, home,
			   sizeof(home)) < 0 ||
	    !uid || !greeter_home_ok(home)) {
		say("no %s account with a home under /var/lib; the login "
		    "screen keeps the image's accent", GREETER_USER);
		return;
	}
	if (under(dir, sizeof(dir), root, home) < 0)
		return;
	say("login screen: %s set into %s", sc->name, dir);
	if (!cfg.dry_run) {
		kb_mkdir_p(dir);
		if (hand_over(dir, uid, gid) < 0 || chmod(dir, 0750) < 0) {
			say("cannot hand %s to %s: %s", dir, GREETER_USER,
			    strerror(errno));
			return;
		}
		snprintf(dst, sizeof(dst), "%s/.config", dir);
		kb_mkdir_p(dst);
		if (hand_over(dst, uid, gid) < 0)
			say("cannot hand %s to %s: %s", dst, GREETER_USER,
			    strerror(errno));
	}
	for (int i = 0; GREETER_FILES[i]; i++) {
		snprintf(src, sizeof(src), "%s/%s", set, GREETER_FILES[i]);
		snprintf(dst, sizeof(dst), "%s/.config/%s", dir,
			 GREETER_FILES[i]);
		if (copy_owned(src, dst, uid, gid) < 0)
			say("cannot copy %s: %s", src, strerror(errno));
	}
	snprintf(src, sizeof(src), "%s/background.png", set);
	snprintf(dst, sizeof(dst), "%s/kdos-background.png", dir);
	if (copy_owned(src, dst, uid, gid) < 0)
		say("cannot copy %s: %s", src, strerror(errno));
}

/*
 * THE ACCENT AND THE DESK, as the first login takes them.
 *
 *   /etc/xdg/kdeglobals [KDE] LookAndFeelPackage   the global theme; at the
 *       first login startplasma applies it in Defaults mode (colours, Plasma
 *       style, icons, cursor, splash, fonts, the window decoration)
 *   /etc/xdg/kdosrc [Desk] Layout                   the panel layout the global
 *       theme's layout script lays out
 *   /etc/xdg/kwinrc [Plugins] krohnkiteEnabled      true, for the tiling desk
 *       only: KWin loads its scripts before plasmashell's first-login layout
 *       writes the same key into ~/.config/kwinrc, so without it auto-tiling
 *       would start at the second login
 *   /etc/kdos/accent                                kdos-splash's accent, read
 *       by rcS before the splash's second half
 *   /etc/vtrgb                                      the text consoles' palette
 *
 * plus the login screen's set. The boot menu's colours are written with
 * limine.conf itself. The theme ids are `org.kdos.<accent>.desktop`, one per
 * libkcolor scheme, so any name kcol_find accepts names an installed theme.
 */
int ki_write_look(const char *root, char *err, size_t n)
{
	const KcolScheme *sc = kcol_find(cfg.theme);
	char lnf[96], vt[512], desk[256];
	char *old;
	KbBuf out = {0};
	int rc;

	if (!sc)
		sc = kcol_default();
	snprintf(lnf, sizeof(lnf), "org.kdos.%s.desktop", sc->name);

	old = slurp(root, "/etc/xdg/kdeglobals");
	ini_key(old, "KDE", "LookAndFeelPackage", lnf, &out);
	free(old);
	rc = put(root, "/etc/xdg/kdeglobals", out.p, 0644, err, n);
	kb_buf_free(&out);
	if (rc)
		return rc;

	snprintf(desk, sizeof(desk),
		 "# Written by the KDOS installer: the panel layout a first "
		 "login lays out.\n"
		 "# `kdos desk` and the welcome tour set a person's own in "
		 "~/.config/kdosrc.\n"
		 "\n"
		 "[Desk]\n"
		 "Layout=%s\n",
		 ki_desk_ok(cfg.desk) ? cfg.desk : ki_desks[0]);
	rc = put(root, "/etc/xdg/kdosrc", desk, 0644, err, n);
	if (rc)
		return rc;
	if (!strcmp(ki_desk_ok(cfg.desk) ? cfg.desk : ki_desks[0], "tiling")) {
		old = slurp(root, "/etc/xdg/kwinrc");
		ini_key(old, "Plugins", "krohnkiteEnabled", "true", &out);
		free(old);
		rc = put(root, "/etc/xdg/kwinrc", out.p, 0644, err, n);
		kb_buf_free(&out);
		if (rc)
			return rc;
	}

	rc = put(root, "/etc/kdos/accent", sc->name, 0644, err, n);
	if (rc)
		return rc;

	if (kcol_vtrgb(sc, vt, sizeof(vt)) >= (int)sizeof(vt))
		return oops(err, n, "the console palette does not fit");
	rc = put(root, "/etc/vtrgb", vt, 0644, err, n);
	if (rc)
		return rc;

	greeter_set(root, sc);
	return 0;
}

/* ── the keyboard ──────────────────────────────────────────────────────── */

/*
 * THE CONSOLE AND THE SESSION, FROM ONE CHOICE.
 *
 * /etc/keymap is the console's, which kdos-getty loads on every text console.
 * An answer that named only an XKB layout gets the console map nearest it.
 *
 * With an XKB layout, /etc/kdos/xkb.conf is kdos-settingsd's store for
 * locale1, which the login screen's KWin reads before anybody types and which
 * startplasma hands every session; /etc/xdg/kxkbrc is the same layout as the
 * Keyboard page of System Settings shows it. Without one, xkb.conf is left
 * empty, which kdos-settingsd reads as "the layout nearest the console
 * keymap", and no kxkbrc is written.
 */
int ki_write_keyboard(const char *root, char *err, size_t n)
{
	char line[96], share[1024];
	int rc;

	if (!cfg.keymap_set && cfg.xkb_layout[0]) {
		if (under(share, sizeof(share), root, "/usr/share") < 0)
			return oops(err, n, "%s: path too long", root);
		ki_xkb_to_vt(share, cfg.xkb_layout, cfg.xkb_variant, cfg.keymap,
			     sizeof(cfg.keymap));
		say("console keymap %s, nearest the layout %s%s%s%s",
		    cfg.keymap, cfg.xkb_layout, cfg.xkb_variant[0] ? " (" : "",
		    cfg.xkb_variant, cfg.xkb_variant[0] ? ")" : "");
	}
	snprintf(line, sizeof(line), "%s\n", cfg.keymap);
	rc = put(root, "/etc/keymap", line, 0644, err, n);
	if (rc)
		return rc;

	if (!cfg.xkb_layout[0]) {
		drop(root, "/etc/xdg/kxkbrc");
		return 0;
	}
	if (!ki_xkb_word_ok(cfg.xkb_layout) ||
	    (cfg.xkb_variant[0] && !ki_xkb_word_ok(cfg.xkb_variant)))
		return oops(err, n, "the keyboard layout '%s' is not an XKB "
				    "name", cfg.xkb_layout);

	{
		char *old = slurp(root, "/etc/kdos/xkb.conf");
		KbBuf a = {0}, b = {0};

		shell_key(old, "XKBLAYOUT", cfg.xkb_layout, &a);
		shell_key(a.p, "XKBVARIANT", cfg.xkb_variant, &b);
		free(old);
		rc = put(root, "/etc/kdos/xkb.conf", b.p, 0644, err, n);
		kb_buf_free(&a);
		kb_buf_free(&b);
		if (rc)
			return rc;
	}

	{
		KbBuf k = {0};

		kb_buf_printf(&k,
			      "[Layout]\n"
			      "LayoutList=%s\n"
			      "VariantList=%s\n"
			      "Use=true\n",
			      cfg.xkb_layout, cfg.xkb_variant);
		rc = put(root, "/etc/xdg/kxkbrc", k.p, 0644, err, n);
		kb_buf_free(&k);
	}
	return rc;
}
