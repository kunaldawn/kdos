/*
 * ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   KD's Homebrew Linux Distro
 * ---------------------------------
 *
 * kdos-appbox boxes --selftest — the box policy's pure parts, asserted.
 *
 * Everything here decides what a box can reach without starting one: the
 * names, the launcher ids KWin is handed, the bus proxy's policy, the three
 * confinement states and the window rules written into a file that is also
 * KWin's and the user's. No podman, no compositor and no bus: the selftest
 * runs it on a bare host, and testing/selftest.sh drives the launch path
 * itself over a fake engine.
 */

#include "kdos-appbox.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static int has_word(char out[][192], int n, const char *w)
{
	for (int i = 0; i < n; i++)
		if (!strcmp(out[i], w))
			return 1;
	return 0;
}

static int has_prefix(char out[][192], int n, const char *w)
{
	for (int i = 0; i < n; i++)
		if (!strncmp(out[i], w, strlen(w)))
			return 1;
	return 0;
}

static void names(void)
{
	cat_chk(box_name_ok("app.gimp"), "app.gimp is a box name");
	cat_chk(box_name_ok("a-b_c.9"), "letters, digits, dot, dash, underscore");
	cat_chk(!box_name_ok("-x"), "a leading dash is an option, not a name");
	cat_chk(!box_name_ok(".x"), "a leading dot is not a name");
	cat_chk(!box_name_ok("a/b"), "a slash is never in a name");
	cat_chk(!box_name_ok(""), "an empty name is no name");
	cat_chk(!box_name_ok("a b"), "a space is never in a name");
	{
		char longname[80];

		memset(longname, 'a', 64);
		longname[64] = '\0';
		cat_chk(!box_name_ok(longname), "64 bytes is past the limit");
	}
}

static void launcher_ids(void)
{
	char id[192];

	launcher_id("app.maxima", "wxmaxima", 0, id, sizeof(id));
	cat_chk(!strcmp(id, "app.maxima"), "a box with one launcher is its own id");
	launcher_id("app.maxima", "wxmaxima", 1, id, sizeof(id));
	cat_chk(!strcmp(id, "app.maxima.wxmaxima"),
		"a box with several gives each <box>.<stem>");
	launcher_id("app.lo", "libreoffice.calc", 1, id, sizeof(id));
	cat_chk(!strcmp(id, "app.lo.libreoffice_calc"),
		"a stem carries no dot, so the box is the id less one component");
	launcher_id("app.x", "-y", 1, id, sizeof(id));
	cat_chk(!strcmp(id, "app.x._y"), "a stem never starts with a dash");
	launcher_id("app.x", "", 1, id, sizeof(id));
	cat_chk(!strcmp(id, "app.x"), "no stem is the box");
}

static void grants(void)
{
	Profile p;

	profile_defaults(&p, "app.t");
	cat_chk(!profile_granted(&p, "window-list"), "a default box is granted nothing");
	profile_set(&p, "grant = layer-shell window-list,secrets\tdpms");
	cat_chk(profile_granted(&p, "layer-shell"), "space-separated grants");
	cat_chk(profile_granted(&p, "window-list"), "and comma-separated");
	cat_chk(profile_granted(&p, "dpms"), "and tab-separated");
	cat_chk(!profile_granted(&p, "window"), "a grant is matched whole");
	cat_chk(!strcmp(p.x11, "yes"), "x11 is yes by default");
	cat_chk(profile_x11(&p) == 1, "a default box is handed the display");
	profile_set(&p, "x11 = false");
	cat_chk(!strcmp(p.x11, "no"), "x11 = false reads as no");
	cat_chk(profile_x11(&p) == 0, "x11 = no hands over no display");
	profile_set(&p, "x11 = maybe");
	cat_chk(!strcmp(p.x11, "yes"), "any other word is yes");
}

static void lanes(void)
{
	Profile p;

	profile_defaults(&p, "b");
	cat_chk(box_lane(&p) == BOX_LANE_PACK, "a box with no base is a pack box");
	profile_set(&p, "base=pack:app.x");
	cat_chk(box_lane(&p) == BOX_LANE_PACK, "base=pack: is the pack lane");
	profile_set(&p, "base=image:kdos/app.x");
	cat_chk(box_lane(&p) == BOX_LANE_STORE, "base=image:kdos/ is the store lane");
	profile_set(&p, "base=image:docker.io/library/debian:trixie");
	cat_chk(box_lane(&p) == BOX_LANE_DISTROBOX,
		"anybody else's image is distrobox's");
	cat_chk(profile_x11(&p) == 1, "and is handed the display");
}

static void policy(void)
{
	char out[64][192];
	Profile p;
	const char *owns[3] = { "org.gimp.GIMP", "gimp", "9bad.id" };
	int n;

	profile_defaults(&p, "app.gimp");
	n = proxy_policy(&p, owns, 3, 0, out, 64);
	cat_chk(n > 0 && !strcmp(out[0], "--filter"), "the proxy filters");
	cat_chk(has_word(out, n, "--talk=org.freedesktop.portal.*"),
		"the portals are reachable");
	cat_chk(has_prefix(out, n, "--call=org.freedesktop.Notifications="),
		"notifications are a scoped call, not talk");
	cat_chk(has_prefix(out, n, "--call=org.kde.StatusNotifierWatcher="),
		"the tray watcher is a scoped call");
	cat_chk(has_word(out, n, "--own=org.mpris.MediaPlayer2.*"),
		"a player may own its MPRIS name");
	cat_chk(has_word(out, n, "--own=org.gimp.GIMP"),
		"the application owns its own reverse-DNS id");
	cat_chk(!has_prefix(out, n, "--own=gimp"),
		"a bare name is no bus name and is not owned");
	cat_chk(!has_prefix(out, n, "--own=9bad"),
		"a component starting with a digit is not owned");
	cat_chk(!has_prefix(out, n, "--talk=org.kde.KWin") &&
		!has_prefix(out, n, "--call=org.kde.KWin") &&
		!has_prefix(out, n, "--see=org.kde.KWin"),
		"KWin (ScreenShot2, Scripting) is hidden");
	cat_chk(!has_prefix(out, n, "--talk=org.kde.plasmashell") &&
		!has_prefix(out, n, "--talk=org.kde.kglobalaccel") &&
		!has_prefix(out, n, "--talk=org.kde.klipper") &&
		!has_prefix(out, n, "--talk=org.kde.krunner") &&
		!has_prefix(out, n, "--talk=org.kde.ksmserver") &&
		!has_prefix(out, n, "--talk=org.freedesktop.systemd1"),
		"the shell, the shortcuts, the clipboard and the session are hidden");
	cat_chk(!has_word(out, n, "--talk=org.a11y.Bus"),
		"no accessibility bus without assistive technology");
	cat_chk(!has_word(out, n, "--talk=org.freedesktop.secrets"),
		"no password store without the grant");
	n = proxy_policy(&p, owns, 0, 1, out, 64);
	cat_chk(has_word(out, n, "--talk=org.a11y.Bus"),
		"the accessibility bus when assistive technology is in use");
	profile_set(&p, "grant=secrets");
	n = proxy_policy(&p, owns, 0, 0, out, 64);
	cat_chk(has_word(out, n, "--talk=org.freedesktop.secrets"),
		"the secrets grant opens the password store");
	profile_set(&p, "grant=session-bus window-list fake-input");
	n = proxy_policy(&p, owns, 0, 0, out, 64);
	cat_chk(n > 0 && !strcmp(out[0], "--filter") &&
		!has_prefix(out, n, "--talk=org.kde.KWin"),
		"no grant takes the filter off the bus");
}

static void states(void)
{
	BoxBoundary b;

	memset(&b, 0, sizeof(b));
	cat_chk(boundary_state(&b) == CONF_UNCONFINED, "no tag is unconfined");
	b.tagged = 1;
	cat_chk(boundary_state(&b) == CONF_TAGGED,
		"a tag over the session's runtime directory is tagged");
	b.private_rundir = 1;
	b.bus = 1;
	cat_chk(boundary_state(&b) == CONF_CONFINED,
		"tag, private directory and filtered bus is confined");
	b.x11 = 1;
	cat_chk(boundary_state(&b) == CONF_TAGGED,
		"a reachable X11 display makes it tagged");
	b.x11 = 0;
	b.bus = 0;
	cat_chk(boundary_state(&b) == CONF_TAGGED, "no filtered bus makes it tagged");
	cat_chk(!strcmp(confinement_name(CONF_CONFINED), "confined") &&
		!strcmp(confinement_name(CONF_TAGGED), "tagged") &&
		!strcmp(confinement_name(CONF_UNCONFINED), "unconfined"),
		"the three words the Boxes page reads");
}

static void rules(void)
{
	const char *tmp = getenv("TMPDIR");
	char dir[512], path[600];
	const char *boxes[2] = { "app.x", "app.y" };
	const char *accents[2] = { "amber", "" };
	size_t len = 0;
	char *text;
	char scheme[64];

	rules_scheme("amber", scheme, sizeof(scheme));
	cat_chk(!strcmp(scheme, "KDOSAmber"), "amber's title bar is KDOSAmber");
	rules_scheme("session", scheme, sizeof(scheme));
	cat_chk(!strcmp(scheme, "KDOSBox"), "no accent is the neutral KDOSBox");

	snprintf(dir, sizeof(dir), "%s/kdos-boxtest-XXXXXX",
		 tmp && *tmp ? tmp : "/tmp");
	if (!mkdtemp(dir)) {
		cat_chk(0, "a scratch directory for the rules file");
		return;
	}
	snprintf(path, sizeof(path), "%s/kwinrulesrc", dir);
	kb_write_file(path,
		      "[$Version]\nupdate_info=kwinrules.upd:replace-placement-string-to-enum\n\n"
		      "[General]\nOrder=mine,kdos-box-gone\n\n"
		      "[mine]\nDescription=Konsole on the left\nwmclass=org.kde.konsole\n"
		      "wmclassmatch=1\n\n"
		      "[kdos-box-gone]\nDescription=KDOS box gone\ndecocolor=KDOSIce\n");
	cat_chk(rules_write(path, boxes, accents, 2) == 2, "two rules written");
	text = kb_read_all(path, &len);
	cat_chk(text && strstr(text, "[mine]\nDescription=Konsole on the left"),
		"the user's own rule is kept as it was");
	cat_chk(text && strstr(text, "[$Version]\nupdate_info="),
		"KWin's own metadata group is kept");
	cat_chk(text && !strstr(text, "kdos-box-gone"),
		"a rule for a box that is gone is dropped, from the order too");
	cat_chk(text && strstr(text, "Order=mine,kdos-box-app.x,kdos-box-app.y\n"),
		"the user's rules first, the boxes' after them");
	cat_chk(text && strstr(text, "[kdos-box-app.x]\nDescription=KDOS box app.x\n"
			       "decocolor=KDOSAmber\ndecocolorrule=2\n"),
		"the accent's scheme, forced");
	cat_chk(text && strstr(text, "wmclass=^app\\\\.x(\\\\.[^.]+)?$\n"
			       "wmclasscomplete=false\nwmclassmatch=3\n"),
		"the class is the box or the box and one launcher stem, dots escaped");
	cat_chk(text && strstr(text, "[kdos-box-app.y]\nDescription=KDOS box app.y\n"
			       "decocolor=KDOSBox\n"),
		"a box with no accent wears the neutral scheme");
	free(text);

	/* Run again: the same file, not a second copy of every rule. */
	cat_chk(rules_write(path, boxes, accents, 2) == 2, "written again");
	text = kb_read_all(path, &len);
	{
		int count = 0;

		for (const char *c = text; c && (c = strstr(c, "[kdos-box-app.x]")); c++)
			count++;
		cat_chk(count == 1, "a rewrite replaces the rule, it does not add one");
	}
	free(text);

	/* The legacy list KWin migrates from deletes every group it does not
	 * name, so it is kept in step where it is still there. */
	kb_write_file(path, "[General]\ncount=1\nrules=mine\n\n[mine]\nDescription=x\n");
	rules_write(path, boxes, accents, 1);
	text = kb_read_all(path, &len);
	cat_chk(text && strstr(text, "rules=mine,kdos-box-app.x\n"),
		"a legacy rules= list names the box's rule too");
	free(text);

	/* No boxes and no file: nothing is written at all. */
	unlink(path);
	cat_chk(rules_write(path, boxes, accents, 0) == 0 && !kb_path_exists(path),
		"no boxes and no file writes no file");
	kb_rmtree(dir);
}

static void digests(void)
{
	Profile a, b;
	char da[PROFILE_DIGEST_MAX], db[PROFILE_DIGEST_MAX], keys[256];
	int n;

	profile_defaults(&a, "d");
	profile_set(&a, "base=image:docker.io/library/alpine:latest");
	b = a;
	profile_set(&b, "x11=no");
	profile_set(&b, "grant=secrets");
	profile_set(&b, "autostop=30m");
	profile_set(&b, "accent=amber");
	profile_set(&b, "wayland=no");
	profile_set(&b, "audio=no");
	profile_set(&b, "render=software");
	profile_set(&b, "image=other");
	profile_create_digest(&a, da, sizeof(da));
	profile_create_digest(&b, db, sizeof(db));
	cat_chk(!strcmp(da, db),
		"keys a launch reads are not in the create-time digest");
	cat_chk(profile_digest_diff(da, db, keys, sizeof(keys)) == 0 && !keys[0],
		"and no key is named as changed");
	cat_chk(strspn(da, "v0123456789abcdef.") == strlen(da) &&
		strlen(da) == 3 + 8 * 13,
		"the digest is one plain word, 8 hex digits a key");
	profile_set(&b, "network=none");
	profile_set(&b, "memory=2G");
	profile_create_digest(&b, db, sizeof(db));
	n = profile_digest_diff(da, db, keys, sizeof(keys));
	cat_chk(n == 2 && !strcmp(keys, "network, memory"),
		"a create-time change names exactly the keys that differ");
	n = profile_digest_diff("v0.x", db, keys, sizeof(keys));
	cat_chk(n == 13, "a digest of another version counts every key");
}

static void foreground(void)
{
	int fd[2];

	cat_chk(pipe(fd) == 0, "a pipe for the foreground check");
	cat_chk(!tty_foreground(fd[0]), "a pipe is no foreground terminal");
	close(fd[0]);
	close(fd[1]);
}

/* A profile remove kept, of a box with no container, gets no window rule;
 * a live profile and an exported launcher's box do. No podman is on the
 * PATH here, so no box has a container. */
static void rules_kept(void)
{
	const char *tmp = getenv("TMPDIR");
	char dir[512], apps[600], pdir[600], f[700];
	char *home = kb_strdup(getenv("HOME") ? getenv("HOME") : "");
	char *path = kb_strdup(getenv("PATH") ? getenv("PATH") : "");
	char (*boxes)[64] = kb_calloc(8, 64);
	char (*accents)[16] = kb_calloc(8, 16);
	int n, live = 0, gone = 0, exp = 0;

	snprintf(dir, sizeof(dir), "%s/kdos-boxtest-XXXXXX",
		 tmp && *tmp ? tmp : "/tmp");
	if (!mkdtemp(dir)) {
		cat_chk(0, "a scratch home for the rules");
		return;
	}
	snprintf(apps, sizeof(apps), "%s/.local/share/applications", dir);
	snprintf(pdir, sizeof(pdir), "%s/.config/kdos/boxes", dir);
	kb_mkdir_p(apps);
	kb_mkdir_p(pdir);
	snprintf(f, sizeof(f), "%s/live.conf", pdir);
	kb_write_file(f, "base=image:x\naccent=amber\n");
	snprintf(f, sizeof(f), "%s/gone.conf", pdir);
	kb_write_file(f, BOX_KEPT_MARK "base=image:x\n");
	snprintf(f, sizeof(f), "%s/exp.sh.desktop", apps);
	kb_write_file(f, "[Desktop Entry]\nType=Application\nName=sh\n"
			 "Exec=sh\nX-KDOS-Box=exp\nX-KDOS-Export=true\n");
	setenv("HOME", dir, 1);
	setenv("PATH", dir, 1);
	n = box_rules_collect(apps, pdir, boxes, accents, 8);
	for (int i = 0; i < n; i++) {
		live += !strcmp(boxes[i], "live") && !strcmp(accents[i], "amber");
		gone += !strcmp(boxes[i], "gone");
		exp += !strcmp(boxes[i], "exp");
	}
	setenv("HOME", home, 1);
	setenv("PATH", path, 1);
	cat_chk(live == 1, "a profile's box gets its rule, with its accent");
	cat_chk(exp == 1, "an exported launcher's box gets its rule");
	cat_chk(gone == 0, "a kept profile with no container gets none");
	kb_rmtree(dir);
	free(boxes);
	free(accents);
	free(home);
	free(path);
}

int boxes_selftest(void)
{
	cat_fail = 0;
	names();
	launcher_ids();
	grants();
	lanes();
	policy();
	states();
	rules();
	digests();
	foreground();
	rules_kept();
	if (cat_fail) {
		fprintf(stderr, "boxes --selftest: %d failure(s)\n", cat_fail);
		return 1;
	}
	printf("boxes --selftest: ok\n");
	return 0;
}
