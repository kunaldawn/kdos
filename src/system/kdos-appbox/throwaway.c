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
 * kdos-appbox throwaway [--pack <id>] <file> — one file, in a box that is
 * gone when it closes.
 *
 * Dolphin's "Open in a throwaway box" (the service menu genlaunchers writes)
 * runs this. The file is opened by a BOXED application — the boxed handler of
 * its type, or the one in `--pack` — in a fresh box made only for it:
 *
 *   network = none         no interface but loopback
 *   home = private         an empty home inside the box, not the user's
 *   persistence = ephemeral  `--rootfs …:O --rm` over a pack box's stack,
 *                          `--rm` over a store box's image: whatever the
 *                          application writes is destroyed with the container
 *   devices, processes, ipc = private, and a /tmp of its own: no camera, no
 *                          view of the session's processes or of what the
 *                          host keeps in /tmp
 *   the file, read-only    bound into the box at its own path and nothing
 *                          beside it, so a document cannot reach its folder
 *
 * The box is named `<launcher-id>.tw<pid>`, so its windows take the
 * application's own launcher id — its icon, its pin, its box's title-bar
 * colour — while KWin's grant check finds the throwaway's own profile, which
 * grants nothing. When the application exits, the box is stopped and removed,
 * and its profile, its private runtime directory, its socket holder and its
 * layer directory go with it.
 *
 * A FILE IS BOUND, NOT HANDED THROUGH THE DOCUMENT PORTAL. The portal's Add
 * takes an open file descriptor, which nothing here can pass without a D-Bus
 * library, and a read-only bind of one file is the same boundary: the box
 * sees that file and nothing of the directory it is in.
 */

#include "kdos-appbox.h"
#include "kxdg.h"

#include <errno.h>
#include <limits.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#define TW_CAND 32

/*
 * THE BOXED HANDLER OF A TYPE. The candidates are the freedesktop resolution
 * `kdos-appbox open` uses; the first whose entry names a box is the one, so a
 * host default does not win a file somebody asked to keep out of the host.
 * With `pack`, only that box's launchers count, and its first launcher is used
 * when none of them claims the type.
 */
static int pick_launcher(const char *mime, const char *pack, BoxLauncher *out,
			 char *box, size_t bn)
{
	KxdgHandler cand[TW_CAND];
	int ncand = kxdg_handlers(mime, cand, TW_CAND, NULL);

	for (int i = 0; i < ncand; i++) {
		KxdgEntry e;
		char b[64] = "";

		if (kxdg_load(&e, cand[i].path, "Desktop Entry") != 0)
			continue;
		kb_strlcpy(b, kxdg_get(&e, "X-KDOS-Box", ""), sizeof(b));
		kxdg_free(&e);
		if (!b[0] || !box_name_ok(b) || (pack && strcmp(b, pack)))
			continue;
		{
			BoxLauncher *bl = kb_calloc(64, sizeof(*bl));
			int n = box_launchers(b, bl, 64);
			char id[192];
			const char *leaf = kb_basename(cand[i].path);
			size_t l = strlen(leaf);

			snprintf(id, sizeof(id), "%.*s", (int)(l > 8 ? l - 8 : l),
				 leaf);
			for (int k = 0; k < n; k++)
				if (!strcmp(bl[k].id, id)) {
					*out = bl[k];
					kb_strlcpy(box, b, bn);
					free(bl);
					return 1;
				}
			free(bl);
		}
	}
	if (pack) {
		BoxLauncher *bl = kb_calloc(64, sizeof(*bl));
		int n = box_launchers(pack, bl, 64);

		if (n > 0) {
			*out = bl[0];
			kb_strlcpy(box, pack, bn);
		}
		free(bl);
		return n > 0;
	}
	return 0;
}

int throwaway_launcher(const char *box, BoxLauncher *out)
{
	char id[128], src[128];
	const char *tw = strrchr(box, '.');
	char *dot;

	if (!tw || strncmp(tw, ".tw", 3) || !tw[3] ||
	    strspn(tw + 3, "0123456789") != strlen(tw + 3) ||
	    (size_t)(tw - box) >= sizeof(id))
		return 0;
	snprintf(id, sizeof(id), "%.*s", (int)(tw - box), box);
	kb_strlcpy(src, id, sizeof(src));
	/* The launcher id is its box, or its box and one stem. */
	for (int up = 0; up < 2; up++) {
		BoxLauncher *bl = kb_calloc(64, sizeof(*bl));
		int n = box_launchers(src, bl, 64);

		for (int k = 0; k < n; k++)
			if (!strcmp(bl[k].id, id)) {
				*out = bl[k];
				free(bl);
				return 1;
			}
		free(bl);
		dot = strrchr(src, '.');
		if (!dot)
			break;
		*dot = '\0';
	}
	return 0;
}

/*
 * WHETHER THE APPLICATION ALREADY RUNS IN ITS OWN BOX, by that box's launch
 * ledger. An application that keeps one instance hands a second launch's file
 * to the first over the bus, and the throwaway box's proxy lets it, because
 * the application's own name is one it may own and therefore reach: the file
 * would open in the running copy, with the network and the home the throwaway
 * box exists to withhold.
 */
static int running_in_box(const char *box, const char *launcher)
{
	char sd[MAX_LINE], path[MAX_LINE + 80];
	size_t len = 0, ll = strlen(launcher);
	char *text;
	int hit = 0;

	if (box_state_dir(sd, sizeof(sd)) != 0)
		return 0;
	snprintf(path, sizeof(path), "%s/%s.apps", sd, box);
	text = kb_read_all(path, &len);
	for (char *l = text, *nl; l && *l && !hit; l = nl) {
		long pid;
		unsigned long long st;
		char *rest;

		nl = strchr(l, '\n');
		if (nl)
			*nl++ = '\0';
		pid = strtol(l, &rest, 10);
		st = strtoull(rest, &rest, 10);
		while (*rest == ' ')
			rest++;
		if (strncmp(rest, "run ", 4) || strncmp(rest + 4, launcher, ll) ||
		    (rest[4 + ll] != '\t' && rest[4 + ll] != '\0'))
			continue;
		hit = pid > 1 && proc_start_time(pid) == st;
	}
	free(text);
	return hit;
}

/*
 * The socket holders kdos-boxsock left for a box: each lock file beside the
 * box's private directory carries `<pid> <engine>`, and a throwaway's holders
 * would otherwise stay for the rest of the session, one per file opened.
 */
static void boxsock_release(const char *box)
{
	char dir[MAX_LINE];
	char **names;
	char prefix[96];
	size_t pl;

	snprintf(dir, sizeof(dir), "%s/kdos/box", box_rundir());
	snprintf(prefix, sizeof(prefix), ".%s.wl", box);
	pl = strlen(prefix);
	names = kb_listdir(dir, NULL);
	for (char **f = names; f && *f; f++) {
		char path[MAX_LINE + 128], text[128];
		size_t l = strlen(*f);
		long pid;

		if (strncmp(*f, prefix, pl) || l < 5 || strcmp(*f + l - 5, ".lock"))
			continue;
		snprintf(path, sizeof(path), "%s/%s", dir, *f);
		if (kb_read_file(path, text, sizeof(text)) > 0) {
			pid = strtol(text, NULL, 10);
			if (pid > 1)
				kill((pid_t)pid, SIGTERM);
		}
		unlink(path);
	}
	kb_strv_free(names);
	snprintf(prefix, sizeof(prefix), ".%s.keeper.lock", box);
	{
		char path[MAX_LINE + 128];

		snprintf(path, sizeof(path), "%s/%s", dir, prefix);
		unlink(path);
	}
}

/*
 * Everything of the throwaway box that is not the container itself: its
 * profile, its state, its socket holders, its private runtime directory and
 * its layer directory, `<data>/kdos/boxes/<box>`, where kdos-packd made the
 * upper and work directories it composed the box with. The kernel's own
 * work/work inside is root's and empty, so it is removed by name before the
 * rest of the tree.
 */
static void throwaway_clear(const char *box)
{
	char pdir[MAX_LINE], ww[MAX_LINE + 16];
	char *pp = profile_path(box);
	char *layer = profile_home(box);

	unlink(pp);
	free(pp);
	box_state_clear(box);
	boxsock_release(box);
	if (box_private_dir(box, pdir, sizeof(pdir)) == 0)
		kb_rmtree(pdir);
	snprintf(ww, sizeof(ww), "%s/work/work", layer);
	rmdir(ww);
	kb_rmtree(layer);
	free(layer);
}

/* The container, the stack kdos-packd composed for a pack box's throwaway
 * (a store box's has none), and everything throwaway_clear takes. */
static void throwaway_rm(const char *tw, int store)
{
	KbArgv a = {0};

	kb_argv_add(&a, "podman");
	kb_argv_add(&a, "rm");
	kb_argv_add(&a, "-f");
	kb_argv_add(&a, "-t");
	kb_argv_add(&a, "2");
	kb_argv_add(&a, tw);
	kb_argv_end(&a);
	kb_run(&a);
	if (!store)
		pack_decompose(tw);
	throwaway_clear(tw);
}

/* A throwaway that fails says so on the desktop as well: it is started from
 * Dolphin, where nothing reads this program's stderr. */
static void __attribute__((noreturn)) tw_fail(const char *why)
{
	notify("Not opened in a throwaway box", why);
	kb_die("not opened in a throwaway box: %s", why);
	exit(1);
}

static int usage(void)
{
	fprintf(stderr, "usage: kdos-appbox throwaway [--pack <id>] <file>\n");
	return 2;
}

int cmd_throwaway(int argc, char **argv)
{
	const char *pack = NULL, *arg = NULL;
	char real[PATH_MAX], mime[128], box[64], tw[64];
	char store[2048];
	const char *words[KB_MAX_ARGV];
	char *files[1], *vec[KB_MAX_ARGV];
	BoxLauncher bl;
	Profile src, p;
	struct stat st;
	int n, rc, image;

	for (int i = 0; i < argc; i++) {
		if (!strcmp(argv[i], "--pack") && i + 1 < argc)
			pack = argv[++i];
		else if (argv[i][0] == '-' || arg)
			return usage();
		else
			arg = argv[i];
	}
	if (!arg)
		return usage();
	if (pack && !box_name_ok(pack))
		kb_die("'%s' is not a box name", pack);
	/* A path, made absolute and resolved, because it is bound into the box
	 * at the path it really has: a symlink's target is what the box gets. */
	if (!realpath(kxdg_mime_for_arg(arg, mime, sizeof(mime)), real) ||
	    stat(real, &st) != 0 || !S_ISREG(st.st_mode))
		tw_fail("it is not a file a box can open");
	kxdg_mime_for_path(real, mime, sizeof(mime));

	if (!pick_launcher(mime, pack, &bl, box, sizeof(box))) {
		notify("Nothing to open it with",
		       "No boxed application opens this kind of file.");
		kb_die("no boxed application opens %s (%s)%s%s", real, mime,
		       pack ? " in " : "", pack ? pack : "");
	}

	if (strchr(bl.upstream, '.') && running_in_box(box, bl.id)) {
		notify("Not opened in a throwaway box",
		       "The application is open in its own box, which would "
		       "receive the file. Close it first.");
		kb_die("%s is running in %s, and a second copy would hand %s to "
		       "it — close it first", bl.id, box, real);
	}

	/* `<launcher-id>.tw<pid>`: unique while this process lives, which is
	 * the throwaway box's whole life. */
	if (snprintf(tw, sizeof(tw), "%s.tw%ld", bl.id, (long)getpid()) >=
		    (int)sizeof(tw) || !box_name_ok(tw))
		kb_die("%s: the launcher id is too long for a throwaway box name",
		       bl.id);

	/*
	 * ITS OWN PROFILE, MADE FROM THE APPLICATION'S BOX. The base, the
	 * accent, the renderer and the X11 display are the source box's, so an
	 * X11-only viewer opens its window here too; the network, the home,
	 * the persistence and the grants are this command's and not the
	 * source's to widen.
	 */
	/*
	 * A STORE BOX'S THROWAWAY IS MADE FROM ITS IMAGE. `base=image:kdos/<id>`
	 * names an image this machine built, and a container created over it
	 * with `--rm` keeps nothing past its stop, which is what a pack box's
	 * `--rootfs …:O --rm` gives; kdos-packd has never heard of the box and
	 * composes nothing for it. A box with no recorded base is a pack box
	 * named after its pack.
	 */
	profile_load(&src, box);
	profile_defaults(&p, tw);
	if (!strncmp(src.base, "pack:", 5) || box_lane(&src) == BOX_LANE_STORE)
		kb_strlcpy(p.base, src.base, sizeof(p.base));
	else
		snprintf(p.base, sizeof(p.base), "pack:%s", box);
	image = box_lane(&p) == BOX_LANE_STORE;
	if (image)
		kb_strlcpy(p.image, p.base + 6, sizeof(p.image));
	kb_strlcpy(p.accent, src.accent, sizeof(p.accent));
	kb_strlcpy(p.render, src.render, sizeof(p.render));
	kb_strlcpy(p.x11, src.x11, sizeof(p.x11));
	p.persist = PERSIST_EPHEMERAL;
	p.netns = p.netnone = 1;
	p.privhome = 1;
	/* Its own /dev, process and IPC namespaces: the host's /dev carries the
	 * camera and every other device, and the host's process table and
	 * shared memory are the rest of the session. The card is bound back
	 * when the source box has it, so rendering is what it was there. */
	p.devsys = p.process = p.ipc = 1;
	p.gpu = src.gpu;
	p.grant[0] = '\0';
	if (profile_save(&p) != 0)
		tw_fail("its profile could not be written");

	/*
	 * THE ONE FILE, READ-ONLY, at its own path. podman's --mount syntax
	 * splits on commas and --volume on colons, so a path carrying either is
	 * not bound: it is copied into the throwaway's own runtime directory
	 * under a name that carries neither, where the box already sees it, at
	 * /run/user/<uid>/<name>, and the copy is what the application opens.
	 */
	{
		char spec[2 * PATH_MAX + 64], merged[MAX_LINE];
		const char *extra[2];
		int nextra = 0, created;

		if (rundir_prepare(&p) != 0) {
			throwaway_clear(tw);
			tw_fail("its runtime directory could not be made");
		}
		if (strpbrk(real, ":,")) {
			char clean[NAME_MAX + 1], pdir[MAX_LINE];
			char staged[MAX_LINE + NAME_MAX + 2];
			const char *b = kb_basename(real);
			size_t k = 0;

			for (; *b && k + 1 < sizeof(clean); b++)
				clean[k++] = (*b == ':' || *b == ',') ? '_' : *b;
			clean[k] = '\0';
			if (box_private_dir(tw, pdir, sizeof(pdir)) != 0)
				tw_fail("its runtime directory could not be made");
			snprintf(staged, sizeof(staged), "%s/%s", pdir, clean);
			if (kb_copy_file(real, staged) != 0 || chmod(staged, 0400) != 0) {
				throwaway_clear(tw);
				tw_fail("the file could not be copied into it");
			}
			if (rundir_inbox_path(tw, 1, clean, real, sizeof(real)) != 0) {
				throwaway_clear(tw);
				tw_fail("the file's name is too long for it");
			}
		} else {
			snprintf(spec, sizeof(spec),
				 "type=bind,source=%s,destination=%s,ro", real, real);
			extra[nextra++] = "--mount";
			extra[nextra++] = spec;
		}

		if (!image) {
			rc = pack_compose(tw, p.base + 5, merged, sizeof(merged));
			if (rc != 0) {
				throwaway_clear(tw);
				tw_fail(rc < 0 ? "kdos-packd is not running" : merged);
			}
		}
		created = pack_box_create(&p, image ? NULL : merged, extra,
					  nextra, 1) == 0;
		if (!created) {
			if (!image)
				pack_decompose(tw);
			throwaway_clear(tw);
			tw_fail(pack_box_error());
		}
	}

	/* The launcher's command, its field codes given the one file. */
	files[0] = real;
	n = kxdg_exec_split(bl.exec, (const char *const *)files, 1, store,
			    sizeof(store), words, KB_MAX_ARGV - 2);
	if (n <= 0) {
		throwaway_rm(tw, image);
		tw_fail("the application has no command to open a file with");
	}
	{
		int has_code = 0;

		for (const char *c = strchr(bl.exec, '%'); c && c[1];
		     c = strchr(c + 2, '%'))
			has_code |= c[1] == 'f' || c[1] == 'F' || c[1] == 'u' ||
				    c[1] == 'U';
		for (int i = 0; i < n; i++)
			vec[i] = (char *)words[i];
		if (!has_code)
			vec[n++] = real;
		vec[n] = NULL;
	}

	g_box = kb_strdup(tw);
	g_launcher = kb_strdup(bl.id);
	tracef("throwaway: %s opens %s in %s", bl.id, real, tw);
	rc = cmd_run_wait(n, vec);

	/* The application has exited. The box goes, and every trace of it. */
	throwaway_rm(tw, image);
	return rc;
}
