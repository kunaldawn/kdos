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
 * kdos-box — a named environment is a first-class object.
 *
 * A SECOND NAME ON THIS BINARY, basename-dispatched exactly as kpkg/kpkgadd
 * and ksvc/service already are. No new program: the box verbs get a namespace
 * of their own instead of crowding the app runtime's, and everything they need
 * — the profile, the launch path, the packd client — is already here.
 *
 * AN APP BOX AND A DEV BOX DIFFER IN THREE PROFILE KEYS, NOT IN KIND. One
 * `compose`, one supervisor, one launcher pipeline, one set of readings; what
 * decides whether an environment is a product or a workshop is `persistence`,
 * `base` and `export`. That is the whole reason one manager over two lanes is
 * honest rather than a wrapper over two systems.
 *
 * AND A BOX YOU BUILT IS A FILE YOU CAN HAND TO SOMEBODY. `freeze` packs the
 * writable upper — only what you changed, deduped against its base by
 * construction — signs it, and it imports anywhere. `podman export` gives a
 * flattened tarball of the whole world; distrobox has no export at all; a
 * derivation needs a network. This gives the diff.
 */

#include "kdos-appbox.h"
#include "kxdg.h"

#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <pwd.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

/* box.c's rule: a container name, a profile filename and a path component. */
#define name_ok box_name_ok

static char *box_dir(const char *box)
{
	char *p = kb_calloc(1, MAX_LINE);
	snprintf(p, MAX_LINE, "%s/.local/share/kdos/boxes/%s", kb_home_dir(), box);
	return p;
}

static int podman(const char *const *words, int n)
{
	KbArgv a = {0};
	kb_argv_add(&a, "podman");
	for (int i = 0; i < n; i++)
		kb_argv_add(&a, words[i]);
	kb_argv_end(&a);
	return kb_run(&a);
}

/* ── list ──────────────────────────────────────────────────────────────── */

static unsigned long long dir_bytes(const char *path)
{
	char **names = kb_listdir(path, NULL);
	unsigned long long total = 0;

	for (char **n = names; n && *n; n++) {
		char *p = kb_path_join(path, *n);
		struct stat st;
		if (lstat(p, &st) == 0) {
			if (S_ISDIR(st.st_mode))
				total += dir_bytes(p);
			else
				total += (unsigned long long)st.st_size;
		}
		free(p);
	}
	kb_strv_free(names);
	return total;
}

/* The confinement the box's last launch recorded, or "" for none. */
static void box_confinement(const char *box, char *out, size_t n)
{
	char sd[MAX_LINE], path[MAX_LINE + 80], text[2048];

	out[0] = '\0';
	if (box_state_dir(sd, sizeof(sd)) != 0)
		return;
	snprintf(path, sizeof(path), "%s/%s.state", sd, box);
	if (kb_read_file(path, text, sizeof(text)) <= 0)
		return;
	for (char *l = text, *nl; l && *l; l = nl) {
		nl = strchr(l, '\n');
		if (nl)
			*nl++ = '\0';
		if (!strncmp(l, "confinement", 11)) {
			const char *v = strchr(l, '=');

			if (v) {
				v++;
				while (*v == ' ')
					v++;
				kb_strlcpy(out, v, n);
			}
			return;
		}
	}
}

typedef struct {
	char name[64];
	char state[32];
	char manager[32];
	long long created;
	unsigned long long rw;	/* what the container itself has written */
} ListRow;

/*
 * WHAT EACH CONTAINER HAS WRITTEN, from one `podman ps --all --size` for the
 * whole list. The template reads `.ListContainer.Size.RwSize`: the reporter's
 * `Size` method returns podman's human string and shadows the struct beneath
 * it, which is reached through the embedded ListContainer. A store box and a
 * distrobox keep everything they install there. A pack box (`base=pack:`) is
 * a `--rootfs` container, whose RwSize podman takes by walking the whole
 * composed rootfs, so the list never adds it: that box's writes are in its
 * upper, under its directory, and dir_bytes counts them. A podman that
 * cannot say leaves every row's figure at what its directory holds.
 */
static void list_sizes(ListRow *rows, int nrows)
{
	KbBuf out = {0};
	KbArgv a = {0};
	char *line, *save;

	kb_argv_add(&a, "podman");
	kb_argv_add(&a, "ps");
	kb_argv_add(&a, "--all");
	kb_argv_add(&a, "--size");
	kb_argv_add(&a, "--format");
	kb_argv_add(&a, "{{.Names}}\t{{.ListContainer.Size.RwSize}}");
	kb_argv_end(&a);
	if (kb_run_capture_buf(&a, &out) != 0) {
		kb_buf_free(&out);
		return;
	}
	kb_buf_add(&out, "", 0);
	for (line = strtok_r(out.p, "\n", &save); line;
	     line = strtok_r(NULL, "\n", &save)) {
		char *tab = strchr(line, '\t');

		if (!tab)
			continue;
		*tab++ = '\0';
		for (int i = 0; i < nrows; i++)
			if (!strcmp(rows[i].name, line)) {
				rows[i].rw = strtoull(tab, NULL, 10);
				break;
			}
	}
	kb_buf_free(&out);
}

/*
 * `kdos-box list [--json]`. The table is for a person; the JSON array is for
 * the Boxes page, one object per box with `name`, `base`, `state`,
 * `persistence`, `accent`, `disk` (bytes), `created` (seconds since the
 * epoch, 0 for a box with no container; `disk` is what its directory under
 * ~/.local/share/kdos/boxes holds and what its container has written, see
 * list_sizes), `confinement` (what the box's
 * last launch recorded, "" when nothing is recorded) and `manager` (the
 * container's `manager` label: "distrobox" for one distrobox made, kdos-box's
 * own boxes included, "" for none).
 *
 * THE LABEL IS READ WITH `{{.Label "manager"}}`. `podman ps` formats a row
 * through its reporter type, whose `Labels` METHOD returns the labels as one
 * comma-joined string and shadows the map beneath it, so `index .Labels` and
 * `.Labels.manager` both fail the template on every row; `Label <name>` is
 * the reporter's own lookup, "" for a container without that label.
 *
 * A PODMAN THAT FAILS IS AN ERROR, NOT AN EMPTY MACHINE. Read as "no
 * containers", a failed query would list every profiled box as `not
 * created` — a running one included, which the Boxes page would then offer
 * to start — and leave out every box with no profile. The whole rows podman
 * printed before it failed are kept and listed, the row it stopped inside
 * is not, podman's own sentence goes to stderr, a line saying the list is
 * incomplete follows the rows, and the exit status is 1.
 */
static int cmd_box_list(int json)
{
	KbBuf out = {0};
	KbArgv a = {0};
	char *line, *save;
	char **profiles;
	char *pdir = kb_calloc(1, MAX_LINE);
	ListRow *rows = kb_calloc(256, sizeof(*rows));
	int nrows = 0, failed, verbose;
	KbBuf j = {0};

	kb_argv_add(&a, "podman");
	kb_argv_add(&a, "ps");
	kb_argv_add(&a, "--all");
	kb_argv_add(&a, "--format");
	kb_argv_add(&a, "{{.Names}}\t{{.State}}\t{{.Created.Unix}}\t{{.Label \"manager\"}}");
	kb_argv_end(&a);
	verbose = kb_proc_verbose;
	kb_proc_verbose = 1;
	failed = kb_run_capture_buf(&a, &out) != 0;
	kb_proc_verbose = verbose;
	kb_buf_add(&out, "", 0);
	/* Every whole row ends in a newline, so on a failure the text after
	 * the last one is the row podman was printing when it stopped — cut
	 * short anywhere, inside its last column included — and is not
	 * listed. */
	if (failed && out.n && out.p[out.n - 1] != '\n') {
		char *nl = memrchr(out.p, '\n', out.n);

		*(nl ? nl + 1 : out.p) = 0;
	}

	for (line = strtok_r(out.p, "\n", &save); line && nrows < 256;
	     line = strtok_r(NULL, "\n", &save)) {
		char *tab = strchr(line, '\t'), *tab2, *tab3 = NULL;

		if (!tab)
			continue;
		*tab++ = 0;
		tab2 = strchr(tab, '\t');
		if (tab2)
			*tab2++ = 0;
		if (tab2 && (tab3 = strchr(tab2, '\t')))
			*tab3++ = 0;
		if (!name_ok(line) || (failed && !tab3))
			continue;
		kb_strlcpy(rows[nrows].name, line, sizeof(rows[0].name));
		kb_strlcpy(rows[nrows].state, tab, sizeof(rows[0].state));
		rows[nrows].created = tab2 ? atoll(tab2) : 0;
		kb_strlcpy(rows[nrows].manager, tab3 ? tab3 : "", sizeof(rows[0].manager));
		nrows++;
	}
	kb_buf_free(&out);

	/*
	 * A box with a profile and no container is still a box — it has been
	 * described and not yet created. Leaving it out of `list` is how
	 * somebody ends up writing the same profile twice.
	 */
	snprintf(pdir, MAX_LINE, "%s/.config/kdos/boxes", kb_home_dir());
	profiles = kb_listdir(pdir, NULL);
	for (char **f = profiles; f && *f && nrows < 256; f++) {
		char nm[64];
		size_t l = strlen(*f);
		int known = 0;

		if (l < 6 || strcmp(*f + l - 5, ".conf") || l - 5 >= sizeof(nm))
			continue;
		memcpy(nm, *f, l - 5);
		nm[l - 5] = 0;
		for (int i = 0; i < nrows && !known; i++)
			known = !strcmp(rows[i].name, nm);
		if (known || !name_ok(nm))
			continue;
		kb_strlcpy(rows[nrows].name, nm, sizeof(rows[0].name));
		kb_strlcpy(rows[nrows].state, "not created", sizeof(rows[0].state));
		rows[nrows].created = 0;
		nrows++;
	}
	kb_strv_free(profiles);
	free(pdir);
	if (!failed)
		list_sizes(rows, nrows);

	/* The BOX and BASE columns are as wide as their longest entry: a
	 * store box's base is image:kdos/<id>, longer than any fixed width,
	 * and one overlong cell would push every column after it. */
	int wname = 16, wbase = 24;

	for (int i = 0; !json && i < nrows; i++) {
		Profile p;
		const char *b;
		int l;

		profile_load(&p, rows[i].name);
		b = p.base[0] ? p.base : p.image[0] ? p.image : "-";
		if ((l = (int)strlen(rows[i].name)) > wname)
			wname = l;
		if ((l = (int)strlen(b)) > wbase)
			wbase = l;
	}
	if (!json)
		printf("%-*s %-*s %-11s %-11s %8s  %s\n", wname, "BOX", wbase,
		       "BASE", "STATE", "PERSIST", "DISK", "ACCENT");
	else
		kb_buf_str(&j, "[");
	for (int i = 0; i < nrows; i++) {
		Profile p;
		char *bd = box_dir(rows[i].name);
		int created = strcmp(rows[i].state, "not created") != 0;
		unsigned long long disk;
		const char *base;
		char conf[32];

		profile_load(&p, rows[i].name);
		disk = !created ? 0 : dir_bytes(bd) +
		       (strncmp(p.base, "pack:", 5) ? rows[i].rw : 0);
		free(bd);
		base = p.base[0] ? p.base : p.image[0] ? p.image : "-";
		if (!json) {
			printf("%-*s %-*s %-11s %-11s %8s  %s\n", wname,
			       rows[i].name, wbase, base, rows[i].state,
			       persist_name(p.persist),
			       created ? kb_human_size(disk) : "-",
			       accent_shown(p.accent));
			continue;
		}
		box_confinement(rows[i].name, conf, sizeof(conf));
		kb_buf_str(&j, i ? ",{\"name\":" : "{\"name\":");
		kb_json_str(&j, rows[i].name);
		kb_buf_str(&j, ",\"base\":");
		kb_json_str(&j, strcmp(base, "-") ? base : "");
		kb_buf_str(&j, ",\"state\":");
		kb_json_str(&j, rows[i].state);
		kb_buf_str(&j, ",\"persistence\":");
		kb_json_str(&j, persist_name(p.persist));
		kb_buf_str(&j, ",\"accent\":");
		kb_json_str(&j, p.accent[0] ? p.accent : "session");
		kb_buf_printf(&j, ",\"disk\":%llu,\"created\":%lld,\"confinement\":",
			      disk, rows[i].created);
		kb_json_str(&j, conf);
		kb_buf_str(&j, ",\"manager\":");
		kb_json_str(&j, rows[i].manager);
		kb_buf_str(&j, "}");
	}
	if (json) {
		kb_buf_str(&j, "]\n");
		fputs(j.p, stdout);
		kb_buf_free(&j);
	}
	free(rows);
	if (failed) {
		/* Last, after the rows: the Boxes page reports the final line
		 * of a failed listing as its error. */
		fflush(stdout);
		kb_warn("podman ps failed, so this list is incomplete: a box "
			"with no profile is missing and a box's state may be "
			"wrong");
		return 1;
	}
	return 0;
}

/* ── create ────────────────────────────────────────────────────────────── */

/*
 * `image:` IS AN ONLINE OPERATION AND SAYS SO BEFORE IT DOES ANYTHING. It
 * fetches unsigned content from somebody else's registry; KDOS_REQUIRE_SIG
 * does not cover it and pretending otherwise would be dishonest. The medium's
 * own bases are the trusted set.
 *
 * AN IMAGE ALREADY IN THE LOCAL STORE IS USED AS IT IS. `podman pull` always
 * asks a registry, even for an image built here: `kdos/<id>` resolves to
 * `localhost/kdos/<id>`, no registry answers at localhost, and every store
 * install would fail at the box after its image had built.
 */
static int base_pull(const char *ref)
{
	const char *w[3];

	w[0] = "image";
	w[1] = "exists";
	w[2] = ref;
	if (podman(w, 3) == 0)
		return 0;

	fprintf(stderr,
		"==> %s is an OCI image: this reaches the network and fetches\n"
		"    content nobody in /etc/kdos/keys has signed. The packs on\n"
		"    your medium are the offline, verifiable alternative.\n", ref);
	w[0] = "pull";
	w[1] = ref;
	return podman(w, 2);
}

/* The profile as a create found it: deleted when the create wrote it, its
 * old text back when one was there (a profile `remove` kept). */
static void profile_restore(const char *box, const char *was)
{
	char *pp = profile_path(box);

	if (was)
		kb_write_file_atomic(pp, was);
	else
		unlink(pp);
	free(pp);
}

/*
 * NOTHING IS WRITTEN UNTIL THE COMMAND LINE IS KNOWN GOOD. Every key=value is
 * read into the profile before its file is touched, and a word with no `=`,
 * an unknown key or a base that names nothing — none at all, `image:` with no
 * reference, `pack:` with no id, `box:` of a box with no pack or image base —
 * ends the create with one line and leaves the file as it was. A create that
 * fails after that puts the profile back as it found it (profile_restore), so
 * neither a refusal nor a failure leaves a box behind for `list` and the
 * Boxes page to show as not created. One that succeeds writes the boxes'
 * window rules again, so the new box's title bar takes its accent at once.
 */
static int cmd_box_create(int argc, char **argv)
{
	Profile p;
	const char *box;
	char merged[MAX_LINE];
	char *pp, *was;
	int rc;

	if (argc < 1) {
		fprintf(stderr, "usage: kdos-box create <name> [key=value ...]\n");
		return 2;
	}
	/* podman's stderr reaches the person from the first podman call on —
	 * see PODMAN'S OWN ERROR below. */
	kb_proc_verbose = 1;
	box = argv[0];
	if (!name_ok(box))
		kb_die(BOX_NAME_RULE);
	if (box_exists(box)) {
		kb_warn("%s already exists", box);
		return 1;
	}

	profile_load(&p, box);
	for (int i = 1; i < argc; i++) {
		if (!strchr(argv[i], '='))
			kb_die("'%s' is not key=value — nothing was written",
			       argv[i]);
		if (profile_set(&p, argv[i]) != 0)
			kb_die("unknown key in '%s' — nothing was written",
			       argv[i]);
	}

	/*
	 * `box:<src>` RESOLVES TO <src>'S OWN BASE, IN PLACE. Clone another
	 * box's LOWER stack, not its upper: a clone starts from the same
	 * software with an empty workspace, which is what somebody asking for
	 * one means. `kdos-box clone` copies the upper as well and is the other
	 * verb. The resolved base is what is saved and what the lanes below
	 * take; saving `box:` would resolve it again for ever.
	 */
	if (!strncmp(p.base, "box:", 4)) {
		Profile src;
		if (!name_ok(p.base + 4))
			kb_die("box:<name> — " BOX_NAME_RULE);
		profile_load(&src, p.base + 4);
		if ((strncmp(src.base, "pack:", 5) || !src.base[5]) &&
		    (strncmp(src.base, "image:", 6) || !src.base[6]))
			kb_die("%s has no pack or image base to clone — nothing "
			       "was written", p.base + 4);
		kb_strlcpy(p.base, src.base, sizeof(p.base));
	}
	if (!strcmp(p.base, "pack:") || !strcmp(p.base, "image:"))
		kb_die("%s: base=%s names no %s — nothing was written", box,
		       p.base, p.base[0] == 'p' ? "pack id" : "image reference");
	if (strncmp(p.base, "pack:", 5) && strncmp(p.base, "image:", 6))
		kb_die("%s: a box is made of something — base=pack:<id>, "
		       "base=image:<ref> or base=box:<name>", box);
	if (strpbrk(p.base, " \t"))
		kb_die("%s: base=%s is not one word — nothing was written", box,
		       p.base);

	pp = profile_path(box);
	was = kb_read_whole(pp, NULL);
	free(pp);
	profile_save(&p);

	/*
	 * PODMAN'S OWN ERROR IS THE POINT OF THIS VERB. `kb_run` sends a
	 * child's stderr to /dev/null unless kb_proc_verbose is set, which is
	 * right for the launch path — a click must not spray container
	 * plumbing at a desktop — and exactly wrong here: `kdos-box create` is
	 * a person asking for a box once, and when podman refuses, its
	 * sentence is the whole diagnosis; without this a failed create shows
	 * only `exit 125`. `pack_box_ensure`, which is what a launch calls, is
	 * left quiet.
	 */
	kb_proc_verbose = 1;

	/* The box's private runtime directory is a volume source of every
	 * create below but distrobox's, and podman refuses a missing one. */
	if (box_lane(&p) != BOX_LANE_DISTROBOX && rundir_prepare(&p) != 0) {
		rc = 1;
	} else if (!strncmp(p.base, "pack:", 5)) {
		rc = pack_compose(box, p.base + 5, merged, sizeof(merged));
		if (rc != 0) {
			if (rc < 0)
				kb_warn("kdos-packd is not running");
			else
				kb_warn("%s", merged);
			rc = 1;
		} else if ((rc = pack_box_create(&p, merged, NULL, 0, 0)) != 0) {
			pack_decompose(box);
			/*
			 * A CREATE THAT FAILS MUST SAY SO. pack_box_create
			 * prints podman's last line, and there are runs where
			 * podman prints nothing at all — a bare exit 125 reads
			 * as nothing. The one cause this program can name
			 * for itself is the live session: an overlay upper
			 * cannot sit on overlayfs, which is what $HOME is on a
			 * booted ISO, and it is the same reading `kdos doctor`
			 * already reports. Anything else is podman's to
			 * explain and this says which exit status it gave.
			 */
			if (fs_is_overlay(kb_home_dir()))
				kb_warn("%s: $HOME is on overlayfs (a live "
					"session), so a box's overlay upper "
					"has nowhere to go — a persistent box "
					"needs an installed system", box);
			else
				kb_warn("%s: podman could not create the box "
					"(exit %d)", box, rc);
		}
	} else if (base_pull(p.base + 6) != 0) {
		kb_warn("could not fetch %s", p.base + 6);
		rc = 1;
	} else {
		kb_strlcpy(p.image, p.base + 6, sizeof(p.image));
		/* An image this machine built for the store is made the way a
		 * pack box is, with its own runtime directory; anybody else's
		 * image is distrobox's, whose init expects the session's. */
		rc = box_lane(&p) == BOX_LANE_STORE
			? pack_box_create(&p, NULL, NULL, 0, 0) : box_create(&p);
	}
	if (rc != 0) {
		profile_restore(box, was);
		free(was);
		return rc;
	}
	free(was);
	box_rules_refresh();

	/* The line that enters a box is this program's, printed here for
	 * every lane; box_create drops distrobox's own "To enter, run:" and
	 * the `distrobox enter` under it. */
	printf("%s created — enter it with: kdos-box enter %s\n", box, box);
	profile_print(&p);
	return 0;
}

/* ── enter ─────────────────────────────────────────────────────────────── */

/* `--window`: the accent's Konsole window even when this process has no
 * terminal, which is how KRunner and other launchers open a box's shell. */
static int enter_window;

/*
 * A TERMINAL THAT SAYS WHERE IT IS. Konsole takes a profile at start, and the
 * identity kit installs one per accent (`KDOS-<Title>.profile`, from
 * kdos-theme's generator) — so a box with `accent = amber` opens an amber
 * Konsole whose tab is titled with the box's name, and there is never a
 * question about which window is which. `--separate` gives it a process of
 * its own, so the window is started with this environment and this profile
 * rather than handed to a Konsole already running with another. Inside,
 * KDOS_BOX carries the name for a prompt to show; the shipped starship.toml
 * does not read it.
 *
 * Three cases, by what this process is:
 *  - a bare `enter` typed at a shell in a terminal, or `--window` from a
 *    launcher with no terminal at all: the accent's Konsole window, which
 *    re-enters with `--shell`;
 *  - the terminal's own program (`konsole -e kdos-box enter <box>`), which is
 *    the leader of the session that terminal made, getsid(0) == getpid(),
 *    where a command typed at a shell is the shell's child and never its
 *    session's leader: in place, titled `<box> — KDOS box` first — a new
 *    window would leave the calling terminal open and empty behind it.
 *    Konsole's OSC 30 replaces the session's tab title format, which its
 *    default "%d : %n" otherwise keeps over any OSC 0, and a later OSC 0
 *    from the box's shell cannot undo it; OSC 0 titles other terminals,
 *    which ignore OSC 30;
 *  - input that is not a terminal, a command given, or KDOS_BOX_NOTERM set
 *    (the window above re-entering): in place, the title left alone.
 */
/*
 * A BOX NOT YET READY IS WAITED FOR IN PLACE, AND SAYS SO PAST A GRACE. Its
 * first start runs distrobox-init, which installs the box's user and
 * packages and can take a minute or more on a slow mirror; podman exec
 * before this start's `container_setup_done` lands in a half-built user. A
 * restart of a box set up before is ready in about half a second, so the
 * first ENTER_QUIET_WAIT seconds of the wait, by the monotonic clock, print
 * nothing; a box still not ready then gets one line, picked by
 * box_setup_ever: "setting up <box> for its first use; this can take a
 * minute" when no start of it ever finished a setup, "waiting for <box> to
 * start" when one did; a wait shorter than the grace prints it as its time
 * runs out. The line comes from inside the one wait, under its wait mark, so
 * a stop at any point of it is heard. Only the in-place cases wait: the
 * window case opens its Konsole at once and the window's own `--shell`
 * re-entry, an in-place case, waits there, under the title, with the line on
 * its screen. The wait is ENTER_SETUP_WAIT seconds, or KDOS_BOX_SETUP_WAIT's
 * (1 to 3600): a person is watching and can stop it, and a shorter wait
 * would fail a first setup that a slow mirror stretches past it. A wait
 * the time runs out on exits 1 naming where to look and never execs into
 * the half-built box; in the window's re-entry it then holds the window
 * until Enter, since Konsole closes it as the command exits and the line
 * would go with it. One ended by `kdos-box stop` exits 1
 * with a line of its own, so the shell reports a status and no
 * "Terminated". The window case waits for its Konsole rather than exec'ing
 * it, so the shell that typed the bare `enter` hears the outcome too: the
 * re-entry finds KDOS_BOX_ENTER_STATUS, $XDG_RUNTIME_DIR/kdos/boxes/
 * <box>.enter.<pid> of the waiting process, and writes `stopped` or
 * `timeout <seconds>` there before it exits 1; the waiting process prints
 * the same line on the calling terminal, removes the file and exits 1, and
 * with no file exits with Konsole's status. SIGTERM is ignored around the
 * wait, which makes box_wait_ready_note return -2 for it rather than re-raise
 * it, and the disposition found is put back after, since an ignored signal
 * stays ignored across the exec into the box.
 */
#define ENTER_SETUP_WAIT 120
#define ENTER_QUIET_WAIT 3

static int enter_shell;

static void enter_say_stopped(const char *box)
{
	fprintf(stderr, "kdos-box: %s was stopped before it was ready\n", box);
}

static void enter_say_timeout(const char *box, int secs)
{
	fprintf(stderr, "kdos-box: %s did not finish setting up in %d s — its "
			"init's output is in `podman logs %s`\n", box, secs, box);
}

static void enter_say_waiting(const char *box)
{
	if (box_setup_ever(box))
		fprintf(stderr, "kdos-box: waiting for %s to start\n", box);
	else
		fprintf(stderr, "kdos-box: setting up %s for its first use; "
				"this can take a minute\n", box);
}

static void enter_wait(const char *box)
{
	struct sigaction ign, old;
	const char *env = getenv("KDOS_BOX_SETUP_WAIT");
	const char *status = getenv("KDOS_BOX_ENTER_STATUS");
	int rc, secs = ENTER_SETUP_WAIT;

	if (box_setup_done(box))
		return;
	if (env && *env) {
		char *end;
		long v = strtol(env, &end, 10);

		if (!*end && v >= 1 && v <= 3600)
			secs = (int)v;
	}
	memset(&ign, 0, sizeof(ign));
	ign.sa_handler = SIG_IGN;
	sigemptyset(&ign.sa_mask);
	sigaction(SIGTERM, &ign, &old);
	rc = box_wait_ready_note(box, secs, ENTER_QUIET_WAIT, enter_say_waiting);
	sigaction(SIGTERM, &old, NULL);
	if (rc != 0 && enter_shell && status && *status) {
		char rec[32];

		if (rc == -2)
			snprintf(rec, sizeof(rec), "stopped\n");
		else
			snprintf(rec, sizeof(rec), "timeout %d\n", secs);
		kb_write_file(status, rec);
	}
	if (rc == -2) {
		enter_say_stopped(box);
		exit(1);
	}
	if (rc != 0) {
		enter_say_timeout(box, secs);
		if (enter_shell && isatty(STDIN_FILENO)) {
			char line[8];

			fprintf(stderr, "press Enter to close\n");
			if (!fgets(line, sizeof(line), stdin))
				line[0] = '\0';
		}
		exit(1);
	}
}

static int cmd_box_enter(int argc, char **argv)
{
	Profile p;
	KbArgv a = {0};
	const char *box;
	char theme[MAX_LINE], title[MAX_LINE + 48];
	int own;

	if (argc < 1) {
		fprintf(stderr, "usage: kdos-box enter <name> [command ...] "
				"| --window\n");
		return 2;
	}
	box = argv[0];
	if (!name_ok(box))
		kb_die(BOX_NAME_RULE);
	profile_load(&p, box);

	if (!box_exists(box))
		kb_die("there is no box named %s — `kdos-box create %s "
		       "base=...` makes one", box, box);
	if (pack_box_start(box) != 0)
		kb_die("could not start %s", box);

	/* The three cases are in the comment above. */
	own = isatty(STDIN_FILENO) && argc == 1 &&
	      !getenv("KDOS_BOX_NOTERM") && getsid(0) == getpid();
	if (!enter_window &&
	    (!isatty(STDIN_FILENO) || argc > 1 || getenv("KDOS_BOX_NOTERM") ||
	     own)) {
		if (own && isatty(STDOUT_FILENO)) {
			printf("\033]30;%s — KDOS box\007"
			       "\033]0;%s — KDOS box\007", box, box);
			fflush(stdout);
		}
		enter_wait(box);
		kb_argv_add(&a, "podman");
		kb_argv_add(&a, "exec");
		/* A terminal is handed on only from its foreground group, and a
		 * background job gets /dev/null instead: main.c's run path says
		 * why. */
		if (tty_foreground(STDIN_FILENO)) {
			kb_argv_add(&a, "--interactive");
			kb_argv_add(&a, "--tty");
		} else if (isatty(STDIN_FILENO)) {
			int null = open("/dev/null", O_RDONLY | O_CLOEXEC);

			if (null >= 0) {
				dup2(null, STDIN_FILENO);
				close(null);
			}
		} else {
			kb_argv_add(&a, "--interactive");
		}
		kb_argv_addf(&a, "--user=%u:%u", (unsigned)getuid(),
			     (unsigned)getgid());
		kb_argv_addf(&a, "--workdir=%s", kb_home_dir());
		kb_argv_addf(&a, "--env=KDOS_BOX=%s", box);
		kb_argv_add(&a, box);
		if (argc > 1) {
			for (int i = 1; i < argc; i++)
				kb_argv_add(&a, argv[i]);
		} else {
			kb_argv_add(&a, "/bin/bash");
			kb_argv_add(&a, "-l");
		}
		kb_argv_end(&a);
		execvp(a.v[0], (char *const *)a.v);
		kb_die("podman: not found");
	}

	kb_argv_add(&a, "konsole");
	kb_argv_add(&a, "--separate");
	{
		/* The accent's profile by its file name, which Konsole looks up
		 * in its profile directories; a scheme name derived from the
		 * same libkcolor row the generator wrote it from, so the two
		 * cannot disagree. */
		char scheme[64];

		rules_scheme(p.accent, scheme, sizeof(scheme));
		if (strcmp(scheme, "KDOSBox")) {
			snprintf(theme, sizeof(theme),
				 "/usr/share/konsole/KDOS-%s.profile", scheme + 4);
			if (kb_path_exists(theme)) {
				kb_argv_add(&a, "--profile");
				kb_argv_add(&a, kb_basename(theme));
			}
		}
	}
	kb_argv_add(&a, "-p");
	snprintf(title, sizeof(title), "LocalTabTitleFormat=%s — KDOS box", box);
	kb_argv_add(&a, title);
	kb_argv_add(&a, "-e");
	kb_argv_add(&a, "kdos-box");
	kb_argv_add(&a, "enter");
	kb_argv_add(&a, box);
	kb_argv_add(&a, "--shell");
	kb_argv_end(&a);
	setenv("KDOS_BOX_NOTERM", "1", 1);
	/* The window's terminal program writes to its own window: its output
	 * and its diagnostics (Qt's warnings as the window closes) would
	 * otherwise land in the calling terminal and interleave with its
	 * prompt, so the Konsole child's stdout and stderr go to /dev/null and
	 * this process keeps its own. `--separate` makes Konsole return when
	 * its window closes, and the re-entry's outcome file (A BOX NOT YET
	 * SET UP) is read after it. */
	{
		char sd[MAX_LINE], status[MAX_LINE + 80], rec[32] = "";
		int have = box_state_dir(sd, sizeof(sd)) == 0;
		int st = 0;
		pid_t pid;

		if (have) {
			snprintf(status, sizeof(status), "%s/%s.enter.%ld", sd,
				 box, (long)getpid());
			unlink(status);
			setenv("KDOS_BOX_ENTER_STATUS", status, 1);
		}
		fflush(stdout);
		fflush(stderr);
		pid = fork();
		if (pid < 0)
			kb_die("fork: %s", strerror(errno));
		if (pid == 0) {
			int devnull = open("/dev/null", O_WRONLY | O_CLOEXEC);

			if (devnull >= 0) {
				dup2(devnull, STDOUT_FILENO);
				dup2(devnull, STDERR_FILENO);
				close(devnull);
			}
			execvp(a.v[0], (char *const *)a.v);
			_exit(127);
		}
		while (waitpid(pid, &st, 0) < 0 && errno == EINTR)
			;
		if (have && kb_read_file(status, rec, sizeof(rec)) > 0) {
			unlink(status);
			if (!strncmp(rec, "stopped", 7))
				enter_say_stopped(box);
			else if (!strncmp(rec, "timeout ", 8))
				enter_say_timeout(box, atoi(rec + 8));
			return 1;
		}
		if (have)
			unlink(status);
		if (WIFEXITED(st) && WEXITSTATUS(st) == 127)
			kb_die("konsole: not found");
		return WIFEXITED(st) ? WEXITSTATUS(st) : 1;
	}
}

/* ── apps, export ──────────────────────────────────────────────────────── */

static int cmd_box_apps(const char *box)
{
	KbArgv a = {0};
	char *buf = kb_calloc(1, 1 << 16);

	if (!name_ok(box))
		kb_die(BOX_NAME_RULE);
	if (pack_box_start(box) != 0)
		kb_die("could not start %s", box);
	kb_argv_add(&a, "podman");
	kb_argv_add(&a, "exec");
	kb_argv_add(&a, box);
	kb_argv_add(&a, "/bin/sh");
	kb_argv_add(&a, "-c");
	/* The one shell in this file, and it runs INSIDE the box over a fixed
	 * string with nothing interpolated into it. */
	kb_argv_add(&a, "ls /usr/share/applications 2>/dev/null | sed 's/\\.desktop$//'");
	kb_argv_end(&a);
	if (kb_run_capture(&a, buf, 1 << 16) == 0)
		fputs(buf, stdout);
	free(buf);
	return 0;
}

/*
 * AN EXPORTED LAUNCHER IS NAMED WITH ITS LAUNCHER ID, `<box>.<stem>.desktop`,
 * the stem being the command it runs: KWin gives every window of the launch
 * that id as its desktopFileName, and the Task Manager, the window's icon and
 * the box's window rule all look for a desktop entry of exactly that name. The
 * entry says `Name=GIMP (arch)`, carries `X-KDOS-Box` and `X-KDOS-Export`,
 * and its Exec passes the id with `-l`. Its one category is X-KDOS-Box: the
 * box's own categories are guesses about another system, and
 * /etc/xdg/menus/applications-merged/kdos-boxes.menu files X-KDOS-Box under
 * the menu's Boxes, where it would otherwise land in Lost & Found.
 *
 * The shim gets `@<box>`, and is a symlink to kdos-appbox: the basename
 * dispatch hands `gimp@arch` to run_as_shim, which finds no alien-apps row of
 * that name (export writes none) and splits it at the last `@` into a command
 * and a box, running what `kdos-box run <box> <command>` runs — and that launch
 * finds this entry among the box's launchers by its command, so its windows
 * take the same id. A shim name is therefore a COMMAND: for an exported
 * desktop id it is the program the entry's Exec runs.
 *
 * <app> IS A DESKTOP ID OR A COMMAND. `kdos-box apps` lists the box's desktop
 * ids, and an id is not a program — `org.gnome.TextEditor` runs
 * `gnome-text-editor` — so an id that names an entry in the box's
 * /usr/share/applications has its Exec, Name, Comment and Icon read from that
 * entry, and its icon copied out of the box (export_box_icon). A word that
 * names no entry is taken as the command itself.
 */
struct box_entry {
	char exec[512];
	char name[128];
	char comment[256];
	char icon[256];
	char wmclass[128];
};

static int box_read_entry(const char *box, const char *app,
			  struct box_entry *e, int start)
{
	KbArgv a = {0};
	char *buf, *line, *save;
	char state[64];
	int in_main = 0;

	memset(e, 0, sizeof(*e));
	if (strchr(app, '/') || app[0] == '.')
		return 0;
	if (start) {
		if (pack_box_start(box) != 0)
			return 0;
	} else {
		box_state(box, state, sizeof(state));
		if (strcmp(state, "running"))
			return 0;
	}
	buf = kb_calloc(1, 1 << 16);
	kb_argv_add(&a, "podman");
	kb_argv_add(&a, "exec");
	kb_argv_add(&a, box);
	kb_argv_add(&a, "cat");
	kb_argv_addf(&a, "/usr/share/applications/%s.desktop", app);
	kb_argv_end(&a);
	if (kb_run_capture(&a, buf, 1 << 16) != 0) {
		free(buf);
		return 0;
	}
	for (line = strtok_r(buf, "\n", &save); line;
	     line = strtok_r(NULL, "\n", &save)) {
		if (*line == '[') {
			in_main = !strncmp(line, "[Desktop Entry]", 15);
			continue;
		}
		if (!in_main)
			continue;
		if (!strncmp(line, "Exec=", 5) && !e->exec[0])
			kb_strlcpy(e->exec, line + 5, sizeof(e->exec));
		else if (!strncmp(line, "Name=", 5) && !e->name[0])
			kb_strlcpy(e->name, line + 5, sizeof(e->name));
		else if (!strncmp(line, "Comment=", 8) && !e->comment[0])
			kb_strlcpy(e->comment, line + 8, sizeof(e->comment));
		else if (!strncmp(line, "Icon=", 5) && !e->icon[0])
			kb_strlcpy(e->icon, line + 5, sizeof(e->icon));
		else if (!strncmp(line, "StartupWMClass=", 15) && !e->wmclass[0])
			kb_strlcpy(e->wmclass, line + 15, sizeof(e->wmclass));
	}
	free(buf);
	return e->exec[0] != 0;
}

/* The applications directory the Start menu and genlaunchers use:
 * $XDG_DATA_HOME's when it is set, ~/.local/share's otherwise. */
static char *export_apps_dir(void)
{
	const char *xdg = getenv("XDG_DATA_HOME");

	return xdg && *xdg ? kb_path_join(xdg, "applications")
			   : kb_path_join(kb_home_dir(), ".local/share/applications");
}

/* The data directory the icons go under: export_apps_dir's parent. */
static char *export_data_dir(void)
{
	const char *xdg = getenv("XDG_DATA_HOME");

	return xdg && *xdg ? kb_strdup(xdg)
			   : kb_path_join(kb_home_dir(), ".local/share");
}

/*
 * THE ICON IS COPIED OUT OF THE BOX, under the launcher id's name. An entry's
 * `Icon=` is a theme name the HOST's icon themes are searched for, and the
 * application's own icon is inside the box, so a launcher naming it shows the
 * menu's blank placeholder. The box is asked, with the name as the shell's
 * `$1` and never part of its script, which files of that name it has under
 * /usr/share/icons/hicolor/<size>/apps and /usr/share/pixmaps; each is read
 * out with `podman exec <box> cat`, which follows the symbolic links icon
 * themes are made of, into a scratch root under $XDG_RUNTIME_DIR, and
 * launcher_icons_copy (launchers.c) lays them out as the pack lane's
 * launchers have theirs: `<data>/icons/hicolor/<size>/apps/<lid>.<ext>`, or
 * `<data>/pixmaps/<lid>.<ext>` when the box has no hicolor copy. An absolute
 * `Icon=` path is copied as it is. Returns how many files were written; 0
 * leaves the entry naming the box's icon, which a theme on the host may have.
 */
static int export_box_icon(const char *box, const char *icon, const char *lid)
{
	KbArgv a = {0};
	char *list = kb_calloc(1, 1 << 15), *line, *save = NULL;
	char root[MAX_LINE];
	char *data;
	int n = 0;

	if (!icon[0] || (icon[0] != '/' && strchr(icon, '/')) ||
	    strstr(icon, "..")) {
		free(list);
		return 0;
	}
	kb_argv_add(&a, "podman");
	kb_argv_add(&a, "exec");
	kb_argv_add(&a, box);
	kb_argv_add(&a, "sh");
	kb_argv_add(&a, "-c");
	kb_argv_add(&a, "case $1 in /*) [ -f \"$1\" ] && echo \"$1\"; exit 0;; esac; "
			"for f in /usr/share/icons/hicolor/*/apps/\"$1\".* "
			"/usr/share/pixmaps/\"$1\".*; do "
			"[ -f \"$f\" ] && echo \"$f\"; done; exit 0");
	kb_argv_add(&a, "sh");
	kb_argv_add(&a, icon);
	kb_argv_end(&a);
	if (kb_run_capture(&a, list, 1 << 15) != 0 || !list[0]) {
		free(list);
		return 0;
	}
	snprintf(root, sizeof(root), "%s/kdos-export-%ld", kb_runtime_dir(),
		 (long)getpid());
	kb_rmtree(root);
	for (line = strtok_r(list, "\n", &save); line;
	     line = strtok_r(NULL, "\n", &save)) {
		KbArgv c = {0};
		char dst[MAX_LINE * 2], dir[MAX_LINE * 2], *slash;

		if (line[0] != '/' || strstr(line, "..") ||
		    (icon[0] != '/' && strncmp(line, "/usr/share/", 11)))
			continue;
		snprintf(dst, sizeof(dst), "%s%s", root, line);
		kb_strlcpy(dir, dst, sizeof(dir));
		if ((slash = strrchr(dir, '/')))
			*slash = '\0';
		kb_mkdir_p(dir);
		kb_argv_add(&c, "podman");
		kb_argv_add(&c, "exec");
		kb_argv_add(&c, box);
		kb_argv_add(&c, "cat");
		kb_argv_add(&c, "--");
		kb_argv_add(&c, line);
		kb_argv_end(&c);
		if (kb_run_to_file(&c, dst) != 0)
			unlink(dst);
	}
	free(list);
	data = export_data_dir();
	n = launcher_icons_copy(root, icon, lid, data, NULL);
	free(data);
	kb_rmtree(root);
	return n;
}

/*
 * EVERY EXPORT OF ONE BOX, for `kdos-box remove`: a launcher whose box is
 * gone is a Start menu row that opens nothing, and its shim a command that
 * fails. An entry is the box's when export wrote it — `X-KDOS-Export=true`
 * and `X-KDOS-Box=<box>` — and a shim when it is `<command>@<box>` in
 * ~/.local/bin linked to kdos-appbox; the icons export copied for an entry,
 * named with its launcher id, go with it. Nothing else in either directory is
 * touched, and a box whose name is another's with a suffix (`t4` and
 * `t4.x`) keeps its own. KService's cache is rebuilt when anything went, so
 * the menu drops the rows now. Returns how many files were removed.
 */
static int box_unexport_all(const char *box)
{
	char *dir = export_apps_dir();
	char *data = export_data_dir();
	char *bin = kb_path_join(kb_home_dir(), ".local/bin");
	char **names = kb_listdir(dir, NULL);
	size_t bl = strlen(box);
	int n = 0;

	for (char **f = names; f && *f; f++) {
		size_t l = strlen(*f);
		KxdgEntry e;
		char *p;
		int ours = 0;

		if (l < 8 || strcmp(*f + l - 8, ".desktop") ||
		    strncmp(*f, box, bl) || (*f)[bl] != '.')
			continue;
		p = kb_path_join(dir, *f);
		if (kxdg_load(&e, p, "Desktop Entry") == 0) {
			ours = kxdg_bool(&e, "X-KDOS-Export", 0) &&
			       !strcmp(kxdg_get(&e, "X-KDOS-Box", ""), box);
			kxdg_free(&e);
		}
		if (ours && unlink(p) == 0) {
			char lid[256];

			kb_strlcpy(lid, *f, sizeof(lid));
			lid[l - 8 < sizeof(lid) ? l - 8 : sizeof(lid) - 1] = '\0';
			launcher_icons_remove(lid, data);
			n++;
		}
		free(p);
	}
	kb_strv_free(names);

	names = kb_listdir(bin, NULL);
	for (char **s = names; s && *s; s++) {
		size_t l = strlen(*s);
		char target[256];
		char *p;
		ssize_t r;

		if (l <= bl + 1 || (*s)[l - bl - 1] != '@' ||
		    strcmp(*s + l - bl, box))
			continue;
		p = kb_path_join(bin, *s);
		r = readlink(p, target, sizeof(target) - 1);
		if (r > 0) {
			target[r] = '\0';
			if (!strcmp(target, "/usr/local/bin/kdos-appbox") &&
			    unlink(p) == 0)
				n++;
		}
		free(p);
	}
	kb_strv_free(names);
	free(bin);
	free(dir);
	free(data);

	if (n && kb_have_prog("kbuildsycoca6")) {
		KbArgv a = {0};
		int v = kb_proc_verbose;

		kb_argv_add(&a, "kbuildsycoca6");
		kb_argv_end(&a);
		kb_proc_verbose = 0;
		kb_run(&a);
		kb_proc_verbose = v;
	}
	return n;
}

/* A line of its own, after what remove keeps: printed inside the list of
 * kept things it would read as though they went too. */
static void print_unexported(const char *box, int n)
{
	if (n == 1)
		printf("%s: 1 exported launcher or command removed\n", box);
	else if (n > 1)
		printf("%s: %d exported launchers and commands removed\n",
		       box, n);
}

/*
 * WHETHER THE BOX HAS A COMMAND OF THAT NAME, asked of the box itself: the
 * name is the shell's `$1`, never part of its script, so nothing in it is
 * read as shell. 1 when the box has it, 0 when the box answered that it does
 * not, -1 when the box could not be started or asked — the engine's own
 * failure prints no answer word at all.
 */
static int box_has_command(const char *box, const char *app)
{
	KbArgv a = {0};
	char out[64] = "";

	if (pack_box_start(box) != 0)
		return -1;
	kb_argv_add(&a, "podman");
	kb_argv_add(&a, "exec");
	kb_argv_add(&a, box);
	kb_argv_add(&a, "sh");
	kb_argv_add(&a, "-c");
	kb_argv_add(&a, "if command -v -- \"$1\" >/dev/null; then echo found; "
			"else echo none; fi");
	kb_argv_add(&a, "sh");
	kb_argv_add(&a, app);
	kb_argv_end(&a);
	kb_run_capture(&a, out, sizeof(out));
	if (!strcmp(out, "found"))
		return 1;
	if (!strcmp(out, "none"))
		return 0;
	return -1;
}

static int box_known(const char *box);

static int cmd_box_export(const char *box, const char *app, int undo)
{
	char *skel, *entry, *shim, *bin;
	char id[512], key[256], lid[192];
	struct box_entry be;
	int found;

	/* An app that starts with a dash is an option this verb does not
	 * take: exported, it is a Start menu row that runs nothing and a shim
	 * whose name every shell reads as a flag. */
	if (!name_ok(box) || !app || !*app || app[0] == '-')
		kb_die("usage: kdos-box %s <box> <app>",
		       undo ? "unexport" : "export");
	/* A box that is not a container, a profile or an installed pack's id
	 * — the boxes `kdos-box run` accepts — is a typo, and an export of it
	 * would be a Start menu row and a command that open nothing. Asked
	 * before anything starts or writes. */
	if (!undo && !box_known(box))
		kb_die("there is no box named %s", box);

	/* An unexport reads the box's entry only from a box already up:
	 * starting one to learn which shim to delete costs a box start, and
	 * the exported entry below names the command as well. */
	found = box_read_entry(box, app, &be, !undo);
	/*
	 * A WORD THAT NAMES NO ENTRY MUST BE A COMMAND THE BOX HAS, or the
	 * launcher and the shim run nothing. Only an answer of "no" refuses:
	 * a box that cannot be started or asked is exported unchecked, with a
	 * warning, as its entry could not be read either.
	 */
	if (!undo && !found) {
		int has = box_has_command(box, app);

		if (has == 0) {
			kb_warn("the box %s has no command or application "
				"named %s", box, app);
			return 1;
		}
		if (has < 0)
			kb_warn("%s could not be asked whether it has %s; "
				"exported unchecked", box, app);
	}
	kb_strlcpy(key, app, sizeof(key));
	if (found) {
		app_exec_key(be.exec, key, sizeof(key));
		if (!key[0] || strchr(key, '/'))
			kb_strlcpy(key, app, sizeof(key));
	}

	skel = export_apps_dir();
	kb_mkdir_p(skel);
	launcher_id(box, key, 1, lid, sizeof(lid));
	/*
	 * A STOPPED BOX'S UNEXPORT FINDS THE ENTRY AMONG THE BOX'S OWN, by the
	 * desktop id it was exported as or by the command it runs, and takes
	 * the command from it — `kdos-box run -l <id> <box> <command> ...` —
	 * so the shim named after that command goes too rather than outliving
	 * its launcher.
	 */
	if (undo && !found) {
		BoxLauncher *bl = kb_calloc(64, sizeof(*bl));
		int nb = box_launchers(box, bl, 64);
		char want[192], k2[256];

		launcher_id(box, app, 1, want, sizeof(want));
		for (int i = 0; i < nb; i++) {
			app_exec_key(bl[i].exec, k2, sizeof(k2));
			if (strcmp(bl[i].upstream, app) && strcmp(bl[i].id, want) &&
			    strcmp(k2, app))
				continue;
			kb_strlcpy(lid, bl[i].id, sizeof(lid));
			if (k2[0] && !strchr(k2, '/'))
				kb_strlcpy(key, k2, sizeof(key));
			break;
		}
		free(bl);
	}
	snprintf(id, sizeof(id), "%s.desktop", lid);
	entry = kb_path_join(skel, id);
	bin = kb_path_join(kb_home_dir(), ".local/bin");
	kb_mkdir_p(bin);
	snprintf(id, sizeof(id), "%s@%s", key, box);
	shim = kb_path_join(bin, id);

	if (undo) {
		char *data = export_data_dir();
		int n = 0;
		n += unlink(entry) == 0;
		n += unlink(shim) == 0;
		launcher_icons_remove(lid, data);
		free(data);
		if (strcmp(key, app)) {
			char *alt;
			snprintf(id, sizeof(id), "%s@%s", app, box);
			alt = kb_path_join(bin, id);
			n += unlink(alt) == 0;
			free(alt);
		}
		printf("%s@%s: %d removed\n", app, box, n);
	} else {
		KbBuf d = {0};
		KbBuf line = {0};
		char store[1024];
		const char *cmdv[KB_MAX_ARGV];
		const char *words[KB_MAX_ARGV + 5];
		int nw = 0, nc = 0;

		if (found)
			nc = kxdg_exec_split(be.exec, NULL, 0, store,
					     sizeof(store), cmdv, KB_MAX_ARGV);
		if (nc <= 0) {
			memset(&be, 0, sizeof(be));
			cmdv[0] = app;
			nc = 1;
			found = 0;
		}
		words[nw++] = "kdos-box";
		words[nw++] = "run";
		words[nw++] = "-l";
		words[nw++] = lid;
		words[nw++] = box;
		for (int i = 0; i < nc; i++)
			words[nw++] = cmdv[i];

		/*
		 * Quoted word by word through the same helper genlaunchers
		 * uses, so what is written here reads back through
		 * kxdg_exec_split as the arguments it was built from — a box
		 * or an app name with a space in it is otherwise three
		 * arguments the moment somebody clicks it. The entry's own
		 * field codes are dropped by the split and `%U` is appended.
		 */
		for (int i = 0; i < nw; i++) {
			char q[512];
			if (kxdg_exec_quote(words[i], q, sizeof(q)) != 0)
				continue;
			kb_buf_printf(&line, "%s%s", i ? " " : "", q);
		}

		kb_buf_printf(&d, "[Desktop Entry]\nType=Application\n");
		kb_buf_printf(&d, "Name=%s (%s)\n", be.name[0] ? be.name : app,
			      box);
		kb_buf_printf(&d, "Exec=%s %%U\n", line.p ? line.p : "");
		/*
		 * A BARE COMMAND HAS NO WINDOW AND NO ICON OF ITS OWN: no theme
		 * holds an icon named after `sh`, so `Icon=<command>` is the
		 * menu's blank-file placeholder, and a command that only
		 * writes to a terminal opens nothing without one. It runs in
		 * the terminal, under the terminal's icon, and the comment is
		 * the menu's subtitle saying which box it runs in.
		 */
		if (found) {
			const char *icon = be.icon[0] ? be.icon : app;

			if (be.comment[0])
				kb_buf_printf(&d, "Comment=%s\n", be.comment);
			kb_buf_printf(&d, "Icon=%s\n",
				      export_box_icon(box, icon, lid) > 0
				      ? lid : icon);
			kb_buf_printf(&d, "Terminal=false\n");
		} else {
			kb_buf_printf(&d, "Comment=%s in the box %s\n", app,
				      box);
			kb_buf_printf(&d, "Icon=utilities-terminal\n");
			kb_buf_printf(&d, "Terminal=true\n");
		}
		kb_buf_printf(&d, "StartupWMClass=%s\n", lid);
		kb_buf_printf(&d, "Categories=X-KDOS-Box;\n");
		kb_buf_printf(&d, "Keywords=box;%s;\n", box);
		kb_buf_printf(&d, "X-KDOS-Box=%s\nX-KDOS-Export=true\n", box);
		if (strchr(app, '.'))
			kb_buf_printf(&d, "X-KDOS-Upstream=%s\n", app);
		kb_write_all(entry, d.p, d.n);
		kb_buf_free(&d);
		kb_buf_free(&line);

		unlink(shim);
		if (symlink("/usr/local/bin/kdos-appbox", shim) != 0)
			kb_warn("%s: %s", shim, strerror(errno));
		printf("%s exported as \"%s (%s)\" and as `%s@%s`\n", app,
		       be.name[0] ? be.name : app, box, key, box);
		printf("  %s\n  %s\n", entry, shim);
	}
	free(entry);
	free(shim);
	free(bin);
	free(skel);
	return 0;
}

/* ── freeze / import ───────────────────────────────────────────────────── */

/*
 * The writable upper, as one pack. Because the upper contains ONLY what
 * changed, the artefact is the diff — and it deltas against a previous freeze
 * like any other pack.
 */
static int cmd_box_freeze(int argc, char **argv)
{
	Profile p;
	const char *box;
	char out[MAX_LINE], meta[MAX_LINE + 8], upper[MAX_LINE];
	char merged[MAX_LINE];
	KbArgv a = {0};
	KbBuf m = {0};
	struct stat st;

	if (argc < 1) {
		fprintf(stderr, "usage: kdos-box freeze <name> [out.kpack]\n");
		return 2;
	}
	box = argv[0];
	if (!name_ok(box))
		kb_die(BOX_NAME_RULE);
	profile_load(&p, box);

	/* The daemon knows where it put the upper; asking is the only way that
	 * cannot disagree with where the writes actually went. */
	if (pack_compose(box, p.base[0] && !strncmp(p.base, "pack:", 5)
				     ? p.base + 5 : box,
			 merged, sizeof(merged)) != 0)
		kb_warn("the box is not composed — freezing what is on disk");
	{
		char *bd = box_dir(box);
		snprintf(upper, sizeof(upper), "%s/upper", bd);
		free(bd);
	}
	if (!kb_is_dir(upper)) {
		char run[MAX_LINE];
		snprintf(run, sizeof(run), "/run/user/%u/kdos/boxes/%s/upper",
			 (unsigned)getuid(), box);
		if (!kb_is_dir(run))
			kb_die("%s has no writable layer to freeze", box);
		kb_strlcpy(upper, run, sizeof(upper));
		kb_warn("%s is running ephemerally — freezing the tmpfs upper",
			box);
	}

	if (argc > 1)
		kb_strlcpy(out, argv[1], sizeof(out));
	else
		snprintf(out, sizeof(out), "%s.kpack", box);

	/* The base chain goes into `requires`, so an import knows what the
	 * frozen layer sits on rather than producing a box that is missing its
	 * own libraries. */
	kb_buf_printf(&m, "id          = box.%s\n", box);
	kb_buf_printf(&m, "kind        = app\n");
	kb_buf_printf(&m, "name        = %s\n", box);
	kb_buf_printf(&m, "version     = %ld\n", (long)time(NULL));
	kb_buf_printf(&m, "release     = 1\n");
	kb_buf_printf(&m, "summary     = the writable layer of the box '%s'\n",
		      box);
	if (!strncmp(p.base, "pack:", 5))
		kb_buf_printf(&m, "requires    = %s\n", p.base + 5);
	snprintf(meta, sizeof(meta), "%s.meta", out);
	kb_write_all(meta, m.p, m.n);
	kb_buf_free(&m);

	kb_argv_add(&a, "kdos-pack");
	kb_argv_add(&a, "build");
	kb_argv_add(&a, upper);
	kb_argv_add(&a, meta);
	kb_argv_add(&a, out);
	kb_argv_end(&a);
	if (kb_run(&a) != 0) {
		unlink(meta);
		kb_die("kdos-pack could not build %s", out);
	}
	unlink(meta);

	if (stat(out, &st) == 0)
		printf("%s  %s — the DIFF, not the world: only what %s wrote\n",
		       out, kb_human_size((unsigned long long)st.st_size), box);
	printf("Sign it with `kdos-pack sign %s <key>`; import it with "
	       "`kdos-box import %s`.\n", out, out);
	return 0;
}

static int cmd_box_import(int argc, char **argv)
{
	char name[64] = "";
	const char *file;
	char *staging, *dst;
	char req[512], msg[512];
	const char *base;

	if (argc < 1) {
		fprintf(stderr, "usage: kdos-box import <file.kpack> [as <name>]\n");
		return 2;
	}
	file = argv[0];
	if (argc >= 3 && !strcmp(argv[1], "as"))
		kb_strlcpy(name, argv[2], sizeof(name));

	if (!kb_path_exists(file))
		kb_die("%s is not here", file);

	/*
	 * Through the daemon's staging directory, because verification happens
	 * where the mount happens. A client that checked the signature and then
	 * asked for a mount would have checked nothing.
	 */
	staging = kb_path_join(pack_store(), "staging");
	base = kb_basename(file);
	dst = kb_path_join(staging, base);
	if (kb_copy_file(file, dst) != 0)
		kb_die("cannot stage %s (is kdos-packd running?)", base);
	free(staging);

	snprintf(req, sizeof(req), "install %s", base);
	if (packd_ask(req, msg, sizeof(msg)) != 0) {
		unlink(dst);
		free(dst);
		kb_die("%s", msg[0] ? msg : "kdos-packd is not running");
	}
	free(dst);
	printf("%s\n", msg);

	if (name[0]) {
		char kv[256];
		char *cargv[2];
		/* `install` answered `<id> <version>`; the id is what a box's
		 * base names. */
		char *id = strtok(msg, " ");

		snprintf(kv, sizeof(kv), "base=pack:%s", id ? id : "");
		cargv[0] = name;
		cargv[1] = kv;
		return cmd_box_create(2, cargv);
	}
	return 0;
}

/* ── snapshots ─────────────────────────────────────────────────────────── */

/*
 * A SNAPSHOT IS A COPY OF THE UPPER, and the cost is stated rather than
 * hidden: on ext4 that is a full copy of everything the box has written. It is
 * not a pack, because a pack cannot be written back into an upper without
 * being mounted, and a rollback that needed the daemon to be running would
 * fail exactly when a box is broken. `freeze` is the pack export and is the
 * other verb.
 */
static int cmd_box_snapshot(const char *box, const char *tag)
{
	char *bd = box_dir(box);
	char from[MAX_LINE], to[MAX_LINE];
	char stamp[32];
	KbArgv a = {0};

	if (!name_ok(box))
		kb_die(BOX_NAME_RULE);
	if (!tag) {
		time_t t = time(NULL);
		struct tm tm;
		gmtime_r(&t, &tm);
		strftime(stamp, sizeof(stamp), "%Y%m%d-%H%M%S", &tm);
		tag = stamp;
	}
	if (!name_ok(tag))
		kb_die("a tag is 1 to 63 of the characters A-Z a-z 0-9 . _ -, "
		       "starting with a letter or a digit");

	snprintf(from, sizeof(from), "%s/upper", bd);
	snprintf(to, sizeof(to), "%s/snapshots/%s", bd, tag);
	free(bd);
	if (!kb_is_dir(from))
		kb_die("%s has nothing written to snapshot", box);
	if (kb_path_exists(to))
		kb_die("%s already has a snapshot called %s", box, tag);
	kb_mkdir_p(to);

	kb_argv_add(&a, "cp");
	kb_argv_add(&a, "-a");
	kb_argv_addf(&a, "%s/.", from);
	kb_argv_add(&a, to);
	kb_argv_end(&a);
	if (kb_run(&a) != 0)
		kb_die("could not copy %s", from);
	printf("%s snapshot %s (%s)\n", box, tag, kb_human_size(dir_bytes(to)));
	return 0;
}

static int cmd_box_snapshots(const char *box)
{
	char *bd = box_dir(box);
	char dir[MAX_LINE];
	char **names;

	snprintf(dir, sizeof(dir), "%s/snapshots", bd);
	free(bd);
	names = kb_listdir(dir, NULL);
	if (!names || !*names) {
		printf("%s has no snapshots\n", box);
		kb_strv_free(names);
		return 0;
	}
	for (char **n = names; *n; n++) {
		char *p = kb_path_join(dir, *n);
		printf("  %-24s %8s\n", *n, kb_human_size(dir_bytes(p)));
		free(p);
	}
	kb_strv_free(names);
	return 0;
}

static int cmd_box_rollback(const char *box, const char *tag)
{
	char *bd = box_dir(box);
	char from[MAX_LINE], upper[MAX_LINE];
	KbArgv a = {0};
	char state[64];

	if (!name_ok(box) || !tag || !name_ok(tag))
		kb_die("usage: kdos-box rollback <box> <tag>");
	snprintf(from, sizeof(from), "%s/snapshots/%s", bd, tag);
	snprintf(upper, sizeof(upper), "%s/upper", bd);
	free(bd);
	if (!kb_is_dir(from))
		kb_die("%s has no snapshot called %s", box, tag);

	/* The upper is a live overlay layer. Replacing it under a running
	 * container is how a box ends up with half of each. */
	box_state(box, state, sizeof(state));
	if (!strcmp(state, "running"))
		kb_die("%s is running — `kdos-box stop %s` first", box, box);

	if (kb_rmtree(upper) != 0)
		kb_warn("could not clear %s", upper);
	kb_mkdir_p(upper);
	kb_argv_add(&a, "cp");
	kb_argv_add(&a, "-a");
	kb_argv_addf(&a, "%s/.", from);
	kb_argv_add(&a, upper);
	kb_argv_end(&a);
	if (kb_run(&a) != 0)
		kb_die("could not restore %s", tag);
	printf("%s rolled back to %s\n", box, tag);
	return 0;
}

/* ── lifecycle ─────────────────────────────────────────────────────────── */

/*
 * `kdos-box stop <box>...`: box_stop_many, never `podman stop`, which in a box
 * that shares the host's PID namespace signals the init and its descendants
 * and leaves the applications its launches started running (box.c says why).
 * The boxes are stopped together, so the logout's stop of every box fits one
 * grace period rather than one per box. 0 when every one is down; otherwise
 * each box still up is named with its state, and 1. A name that is neither a
 * container nor a profile is refused by name and makes the status 1 too.
 */
static int cmd_box_stop(const char *const *boxes, int n)
{
	const char **up = kb_calloc((size_t)n + 1, sizeof(*up));
	char state[64];
	int left, nup = 0, missing = 0;

	for (int i = 0; i < n; i++)
		if (!name_ok(boxes[i]))
			kb_die(BOX_NAME_RULE);
	/* A box with no container has nothing running: one with a profile is
	 * stopped already, and a name with neither is no box at all. */
	for (int i = 0; i < n; i++) {
		char *pp;

		if (box_exists(boxes[i])) {
			up[nup++] = boxes[i];
			continue;
		}
		pp = profile_path(boxes[i]);
		if (!kb_path_exists(pp)) {
			kb_warn("there is no box named %s", boxes[i]);
			missing++;
		}
		free(pp);
	}
	left = nup ? box_stop_many(up, nup, 10) : 0;
	for (int i = 0; left && i < nup; i++) {
		box_state(up[i], state, sizeof(state));
		if (!strcmp(state, "running") || !strcmp(state, "stopping"))
			kb_warn("%s did not stop: it is still %s after SIGKILL",
				up[i], state);
		else if (box_signal_all(up[i], 0) > 0)
			kb_warn("%s stopped, and processes it started are still "
				"alive", up[i]);
	}
	free(up);
	return left || missing ? 1 : 0;
}

/* Every run of 32 or more hexadecimal digits in `s` — a container's or an
 * image's id, which podman's sentences name things by — replaced with
 * `word`, in place. */
static void fold_ids(char *s, size_t n, const char *word)
{
	char out[1024];
	size_t o = 0;

	for (const char *c = s; *c && o + 1 < sizeof(out);) {
		size_t h = strspn(c, "0123456789abcdef");

		if (h >= 32) {
			o += (size_t)snprintf(out + o, sizeof(out) - o, "%s", word);
			if (o >= sizeof(out))
				o = sizeof(out) - 1;
			c += h;
			continue;
		}
		if (!h)
			h = 1;
		while (h-- && *c && o + 1 < sizeof(out))
			out[o++] = *c++;
	}
	out[o] = '\0';
	kb_strlcpy(s, out, n);
}

/*
 * `--purge` OF A BOX OVER AN IMAGE RELEASES THE IMAGE, `base=image:<ref>`, a
 * distrobox's base or a store box's own: a removed Fedora box otherwise keeps
 * its few hundred megabytes for good. It is kept while another profile names
 * the same base, and `podman rmi` without --force refuses one a container
 * still uses — a box made outside kdos-box, or a running one — and that
 * refusal is the line printed, ids folded. A store application's runtime
 * images are its uninstall's: `kdos app remove` is the route that releases
 * them too.
 */
static void release_image(const Profile *p)
{
	char pdir[MAX_LINE], why[512] = "";
	char **names;
	const char *img;
	KbArgv r = {0};

	if (strncmp(p->base, "image:", 6) || !p->base[6])
		return;
	img = p->base + 6;
	snprintf(pdir, sizeof(pdir), "%s/.config/kdos/boxes", kb_home_dir());
	names = kb_listdir(pdir, NULL);
	for (char **f = names; f && *f; f++) {
		size_t l = strlen(*f);
		char nm[64];
		Profile o;

		if (l < 6 || l - 5 >= sizeof(nm) || strcmp(*f + l - 5, ".conf"))
			continue;
		memcpy(nm, *f, l - 5);
		nm[l - 5] = '\0';
		if (!strcmp(nm, p->name) || !name_ok(nm))
			continue;
		profile_load(&o, nm);
		if (!strcmp(o.base, p->base)) {
			printf("  its image %s is kept: the box %s is made of it "
			       "too\n", img, nm);
			kb_strv_free(names);
			return;
		}
	}
	kb_strv_free(names);
	if (!image_exists(img))
		return;
	kb_argv_add(&r, "podman");
	kb_argv_add(&r, "rmi");
	kb_argv_add(&r, img);
	kb_argv_end(&r);
	if (run_last_line(&r, why, sizeof(why)) == 0) {
		printf("  and its image %s, which no other box uses, is "
		       "released\n", img);
		return;
	}
	fold_ids(why, sizeof(why), "another container");
	printf("  its image %s is kept: %s\n", img, why);
}

/*
 * A PROFILE `remove` KEPT ON PURPOSE CARRIES BOX_KEPT_MARK AT ITS TOP (box.c),
 * and the `profile` verb writes it back for a box with no container. A remove
 * that finds no container deletes only a profile without it — one a failed
 * store install left — and keeps a marked one, so repeating a remove never
 * deletes what the first said it kept.
 *
 * The stack is released only once the container is gone: decomposing under a
 * container podman refused to remove (a running box without --force) pulls
 * the root out from under a live box. `purge` deletes the profile as well,
 * marked or not, and releases the box's image (release_image). Whatever
 * podman prints is caught, and a refusal is this program's one line with
 * podman's reason in it, its ids folded into the box's name; nothing of the
 * stop sequence's own podman calls reaches the terminal. A remove that took
 * anything writes the boxes' window rules again, so a removed box's rule
 * leaves kwinrulesrc.
 */
static int cmd_box_remove(const char *box, int force, int purge)
{
	KbArgv a = {0};
	KbArgv q = {0};
	Profile p;
	char why[512] = "";
	int rc;

	if (!name_ok(box))
		kb_die(BOX_NAME_RULE);
	profile_load(&p, box);
	/* `podman container exists` is 0 for yes, 1 for no and 125 for a
	 * podman that could not tell; only a 1 is "no container". box_exists
	 * folds the 125 into "no", and that would release the stack of a box
	 * that may be running. */
	kb_argv_add(&q, "podman");
	kb_argv_add(&q, "container");
	kb_argv_add(&q, "exists");
	kb_argv_add(&q, box);
	kb_argv_end(&q);
	rc = kb_run(&q);
	if (rc == 1) {
		/* A failed create leaves its profile, and can leave a composed
		 * stack, with no container. Nothing runs on either, so both go —
		 * unless an earlier remove kept the profile and said so, when
		 * only --purge deletes it. */
		char *pp = profile_path(box);
		int kept = !purge && profile_is_kept(box);
		int had = !kept && unlink(pp) == 0;
		int ex = box_unexport_all(box);

		pack_decompose(box);
		if (kept) {
			printf("%s has no container; its profile is kept in %s; "
			       "`kdos-box remove %s --purge` deletes it\n",
			       box, pp, box);
			print_unexported(box, ex);
			free(pp);
			return 0;
		}
		free(pp);
		if (had)
			printf("%s had no container — its profile is removed\n",
			       box);
		if (had && purge)
			release_image(&p);
		print_unexported(box, ex);
		if (had || ex) {
			box_rules_refresh();
			return 0;
		}
		kb_warn("there is no box named %s", box);
		return 1;
	}
	if (rc != 0) {
		kb_warn("%s was not removed (podman container exists exited %d)",
			box, rc);
		return rc;
	}
	/* `--force` is the stop sequence first, and so is a box that is down:
	 * an application a stopped box still runs holds an exec session that
	 * `podman rm` refuses over, and `rm -f`'s own stop reaches only the
	 * init and its descendants. */
	{
		char state[64];

		box_state(box, state, sizeof(state));
		if (force || (strcmp(state, "running") && strcmp(state, "stopping")))
			box_stop_many(&box, 1, 10);
	}
	kb_argv_add(&a, "podman");
	kb_argv_add(&a, "rm");
	if (force)
		kb_argv_add(&a, "-f");
	kb_argv_add(&a, box);
	kb_argv_end(&a);
	rc = run_last_line(&a, why, sizeof(why));
	if (rc != 0) {
		fold_ids(why, sizeof(why), box);
		kb_warn("%s was not removed: %s%s", box, why,
			force ? "" : " — a running box needs `kdos-box stop` "
				     "first, or --force");
		return rc;
	}
	/* The overlay goes with it; the PACKS do not, because another box may
	 * be using the same runtime. kdos-packd reference-counts them. */
	pack_decompose(box);
	/* What is KEPT, by where it actually is: the profile always, and the
	 * directory only where it exists — a pack box's upper, snapshots and
	 * private home live there, and a store box's writes went with its
	 * container. What `export` wrote goes: it launches the container that
	 * is gone. */
	{
		char *pp = profile_path(box);
		char *bd = box_dir(box);
		int ex = box_unexport_all(box);

		if (purge) {
			unlink(pp);
			printf("%s removed, and its profile %s deleted\n", box,
			       pp);
			release_image(&p);
		} else {
			profile_mark_kept(box);
			printf("%s removed — its profile is kept in %s\n", box,
			       pp);
		}
		if (kb_is_dir(bd))
			printf("  and its writable layer, snapshots and home in "
			       "%s\n", bd);
		print_unexported(box, ex);
		free(bd);
		free(pp);
	}
	box_rules_refresh();
	return 0;
}

/*
 * gc — an idle box costs a conmon, its keeper, its bus proxy and its page
 * cache. One question and a clock, on the user's ten-minute timer, rather than
 * a root daemon for a job this size.
 *
 * A BOX WITH A MAPPED WINDOW IS NEVER IDLE, whatever its clock says. KWin is
 * asked through `kdos hey boxes`, which lists the boxes that have a window on
 * the screen: a window belongs to the box its app id names (the box, or the
 * box and one more component — the launcher ids kdos-boxsock tagged its
 * socket with) or, for an X11 window, whose box its process runs in. It is
 * asked ONCE per run, not once per box.
 *
 * THREE ANSWERS, NOT TWO: 1 has a window, 0 has none, -1 there was nothing to
 * ask. The third is not the second. A collector that read "cannot tell" as "no
 * window" would stop every warmed box on a desktop it could not question.
 *
 * AT THE SESSION'S END there is nothing to ask and no window to find: the
 * shutdown scripts run after KWin has closed every window, so `--session-end`
 * takes every box as windowless and stops the ones past their `autostop`. A
 * box with no `autostop` keeps running past the logout.
 */
static char *g_windowed;	/* `kdos hey boxes`, once; NULL: not asked */
static int g_windowed_rc = 1;

static int box_has_window(const char *box)
{
	char *copy, *save = NULL;
	int found = 0;

	if (!g_windowed && g_windowed_rc) {
		KbArgv a = {0};

		g_windowed = kb_calloc(1, 1 << 16);
		kb_argv_add(&a, "kdos");
		kb_argv_add(&a, "hey");
		kb_argv_add(&a, "boxes");
		kb_argv_end(&a);
		g_windowed_rc = kb_run_capture(&a, g_windowed, 1 << 16);
	}
	if (g_windowed_rc != 0)
		return -1;	/* no session to ask */
	/* One box per line, indented. Matched whole: a substring test would
	 * read `app.xyz`'s window as one of `app.x`'s and never collect it. */
	copy = kb_strdup(g_windowed);
	for (char *line = strtok_r(copy, "\n", &save); line && !found;
	     line = strtok_r(NULL, "\n", &save)) {
		while (*line == ' ' || *line == '\t')
			line++;
		found = !strcmp(line, box);
	}
	free(copy);
	return found;
}

static int cmd_box_gc(int dry, int session_end)
{
	char *buf = kb_calloc(1, 1 << 16);
	KbArgv a = {0};
	char *line, *save;
	int stopped = 0;

	kb_argv_add(&a, "podman");
	kb_argv_add(&a, "ps");
	kb_argv_add(&a, "--format");
	kb_argv_add(&a, "{{.Names}}\t{{.State}}\t{{.StartedAt}}");
	kb_argv_end(&a);
	if (kb_run_capture(&a, buf, 1 << 16) != 0) {
		free(buf);
		return 1;
	}
	for (line = strtok_r(buf, "\n", &save); line;
	     line = strtok_r(NULL, "\n", &save)) {
		char *t1 = strchr(line, '\t'), *t2;
		Profile p;
		long started;
		int win;

		if (!t1)
			continue;
		*t1++ = 0;
		t2 = strchr(t1, '\t');
		if (!t2)
			continue;
		*t2++ = 0;
		if (strcmp(t1, "running") || !name_ok(line))
			continue;
		profile_load(&p, line);
		if (!p.autostop_s)
			continue;
		started = atol(t2);
		if (started <= 0 || time(NULL) - started < p.autostop_s)
			continue;
		win = session_end ? 0 : box_has_window(line);

		if (win > 0) {
			printf("  %-16s has a window open — left alone\n", line);
			continue;
		}
		if (win < 0) {
			/* NOT EVIDENCE OF NO WINDOW. Stopping a box because
			 * nothing could be asked would collect a running
			 * application from under whoever is using it. */
			printf("  %-16s no session to ask — left alone\n",
			       line);
			continue;
		}
		/* The clock is the container's start, not its last window:
		 * podman keeps no record of when a box was last used. */
		if (dry) {
			printf("  %-16s up past %ds, no window — would stop\n",
			       line, p.autostop_s);
		} else {
			printf("  %-16s up past %ds, no window — stopping\n",
			       line, p.autostop_s);
			const char *one = line;

			cmd_box_stop(&one, 1);
		}
		stopped++;
	}
	free(buf);
	free(g_windowed);
	g_windowed = NULL;
	if (!stopped)
		printf("  nothing to stop\n");
	return 0;
}

/* ── the dispatch ──────────────────────────────────────────────────────── */

static int box_usage(FILE *to, int rc)
{
	fprintf(to,
"usage: kdos-box list [--json]\n"
"       kdos-box create <name> [base=pack:<id>|image:<ref>|box:<name>] [key=value ...]\n"
"       kdos-box enter  <name> [command ...] | --window\n"
"       kdos-box run    [-l <launcher-id>] <name> <app> [args ...]\n"
"       kdos-box apps   <name>\n"
"       kdos-box export <name> <app>        also: unexport\n"
"       kdos-box freeze <name> [out.kpack]  the writable layer, as one pack\n"
"       kdos-box import <file.kpack> [as <name>]\n"
"       kdos-box clone  <src> <dst>\n"
"       kdos-box snapshot <name> [tag]      also: snapshots, rollback\n"
"       kdos-box start | restart <name>\n"
"       kdos-box stop   <name> ...          SIGTERM, 10 s, then SIGKILL\n"
"       kdos-box remove <name> [--force] [--purge]\n"
"                                         --force removes a running box,\n"
"                                         --purge deletes its profile and\n"
"                                         releases an image no box uses\n"
"       kdos-box profile <name> [key=value ...]\n"
"       kdos-box gc [--dry-run] [--session-end]\n"
"\nAn app box and a dev box differ in three profile keys, not in kind:\n"
"`persistence`, `base` and `export`.\n");
	return rc;
}

/* 1 when `box` is a container, a profile or an installed pack's id: what a
 * launch of it can start from. A pack box takes its name from its pack and
 * has neither until its first launch composes it. */
static int box_known(const char *box)
{
	char *pp = profile_path(box);
	int known = kb_path_exists(pp) || box_exists(box);
	char *list;

	free(pp);
	if (known || !(list = pack_list()))
		return known;
	for (char *line = list, *nl; line && *line && !known; line = nl) {
		size_t l = strcspn(line, "\t\n");

		nl = strchr(line, '\n');
		if (nl)
			nl++;
		known = l == strlen(box) && !strncmp(line, box, l);
	}
	free(list);
	return known;
}

int box_main(int argc, char **argv)
{
	const char *cmd;

	if (argc < 1)
		return box_usage(stderr, 2);
	cmd = argv[0];

#define BCMD(s) (!strcmp(cmd, s))
	if (BCMD("--help") || BCMD("-h") || BCMD("help"))
		return box_usage(stdout, 0);
	if (BCMD("list") || BCMD("ls")) {
		int json = 0;

		for (int i = 1; i < argc; i++) {
			if (strcmp(argv[i], "--json")) {
				fprintf(stderr, "usage: kdos-box list [--json]\n");
				return 2;
			}
			json = 1;
		}
		return cmd_box_list(json);
	}
	if (BCMD("create"))
		return cmd_box_create(argc - 1, argv + 1);
	if (BCMD("enter")) {
		/* `--shell` is what the terminal wrapper re-enters with; it
		 * means "you are already in a terminal, do not open another". */
		if (argc > 2 && !strcmp(argv[2], "--shell")) {
			setenv("KDOS_BOX_NOTERM", "1", 1);
			enter_shell = 1;
			return cmd_box_enter(1, argv + 1);
		}
		if (argc == 3 && !strcmp(argv[2], "--window")) {
			enter_window = 1;
			unsetenv("KDOS_BOX_NOTERM");
			return cmd_box_enter(1, argv + 1);
		}
		return cmd_box_enter(argc - 1, argv + 1);
	}
	if (BCMD("run")) {
		int at = 1;

		/* `-l <id>` is the launcher id the windows take, which is what
		 * an exported launcher passes; see resolve_launcher(). */
		if (argc > 3 && !strcmp(argv[1], "-l")) {
			g_launcher = argv[2];
			at = 3;
		}
		if (argc > at + 1) {
			if (!name_ok(argv[at]))
				kb_die(BOX_NAME_RULE);
			/* A name that is no box is refused before the launch
			 * path composes, creates or records anything. */
			if (!box_known(argv[at]))
				kb_die("there is no box named %s", argv[at]);
			g_box = argv[at];
			return cmd_run(argc - at - 1, argv + at + 1);
		}
		fprintf(stderr, "usage: kdos-box run [-l <launcher-id>] <name> "
				"<app> [args ...]\n");
		return 2;
	}
	if (BCMD("apps") && argc > 1)
		return cmd_box_apps(argv[1]);
	if (BCMD("export") && argc > 2)
		return cmd_box_export(argv[1], argv[2], 0);
	if (BCMD("unexport") && argc > 2)
		return cmd_box_export(argv[1], argv[2], 1);
	if (BCMD("freeze"))
		return cmd_box_freeze(argc - 1, argv + 1);
	if (BCMD("import"))
		return cmd_box_import(argc - 1, argv + 1);
	if (BCMD("clone") && argc > 2) {
		Profile src;
		char kv[320];
		char *cargv[2];
		char *bd, *nd;
		KbArgv a = {0};
		int has;

		if (!name_ok(argv[1]) || !name_ok(argv[2]))
			kb_die(BOX_NAME_RULE);
		has = profile_load(&src, argv[1]);
		if (!has && !box_exists(argv[1]))
			kb_die("there is no box named %s", argv[1]);
		if (!src.base[0])
			kb_die("%s has no pack or image base to clone", argv[1]);
		snprintf(kv, sizeof(kv), "base=%s", src.base);
		cargv[0] = argv[2];
		cargv[1] = kv;
		if (cmd_box_create(2, cargv) != 0)
			return 1;
		/* A clone copies the WORK as well as the software; that is the
		 * difference between `clone` and `create base=box:<name>`. */
		bd = box_dir(argv[1]);
		nd = box_dir(argv[2]);
		kb_argv_add(&a, "cp");
		kb_argv_add(&a, "-a");
		kb_argv_addf(&a, "%s/upper/.", bd);
		kb_argv_addf(&a, "%s/upper", nd);
		kb_argv_end(&a);
		kb_run(&a);
		free(bd);
		free(nd);
		printf("%s cloned to %s\n", argv[1], argv[2]);
		return 0;
	}
	if (BCMD("snapshot") && argc > 1)
		return cmd_box_snapshot(argv[1], argc > 2 ? argv[2] : NULL);
	if (BCMD("snapshots") && argc > 1)
		return cmd_box_snapshots(argv[1]);
	if (BCMD("rollback") && argc > 2)
		return cmd_box_rollback(argv[1], argv[2]);
	if (BCMD("start") && argc > 1) {
		if (!name_ok(argv[1]))
			kb_die(BOX_NAME_RULE);
		if (!box_exists(argv[1]))
			kb_die("there is no box named %s — `kdos-box create %s "
			       "base=...` makes one", argv[1], argv[1]);
		return pack_box_start(argv[1]);
	}
	if (BCMD("stop") && argc > 1)
		return cmd_box_stop((const char *const *)argv + 1, argc - 1);
	if (BCMD("restart") && argc > 1) {
		if (cmd_box_stop((const char *const *)argv + 1, 1) != 0)
			return 1;
		return pack_box_start(argv[1]);
	}
	if (BCMD("remove")) {
		int force = 0, purge = 0;
		const char *name = NULL;

		for (int i = 1; i < argc; i++) {
			if (!strcmp(argv[i], "--force"))
				force = 1;
			else if (!strcmp(argv[i], "--purge"))
				purge = 1;
			else if (!name && argv[i][0] != '-')
				name = argv[i];
			else
				name = NULL, i = argc;
		}
		if (!name || argc - 1 != 1 + force + purge) {
			fprintf(stderr, "usage: kdos-box remove <name> [--force] "
					"[--purge]\n");
			return 2;
		}
		return cmd_box_remove(name, force, purge);
	}
	if (BCMD("profile") && argc > 1) {
		Profile p;
		if (!name_ok(argv[1]))
			kb_die(BOX_NAME_RULE);
		profile_load(&p, argv[1]);
		if (argc > 2) {
			char before[sizeof(p.accent)];
			char d0[PROFILE_DIGEST_MAX], d1[PROFILE_DIGEST_MAX];
			char keys[256];
			Profile old = p;
			int only_accent, ncreate;
			int kept = profile_is_kept(argv[1]) &&
				   !box_exists(argv[1]);

			kb_strlcpy(before, p.accent, sizeof(before));
			for (int i = 2; i < argc; i++)
				profile_set(&p, argv[i]);
			profile_save(&p);
			/* An edit of a profile remove kept keeps it kept. A
			 * box with a container needs no mark — its remove
			 * writes one. */
			if (kept)
				profile_mark_kept(argv[1]);
			/* The title-bar colour is a KWin rule, not a create
			 * flag: it is written again now, and KWin told. */
			if (strcmp(before, p.accent))
				box_rules_refresh();
			/* Whether anything but the accent changed, field by
			 * field, and which create-time keys did: a launch-time
			 * key needs no new container. */
			memcpy(old.accent, p.accent, sizeof(old.accent));
			only_accent = strcmp(before, p.accent) &&
				      !memcmp(&old, &p, sizeof(p));
			profile_create_digest(&old, d0, sizeof(d0));
			profile_create_digest(&p, d1, sizeof(d1));
			ncreate = profile_digest_diff(d0, d1, keys, sizeof(keys));
			/* Only a box with a container is removed and made
			 * again: removing one without a container deletes the
			 * profile just written. */
			if (only_accent)
				printf("%s: written — the title-bar colour "
				       "changes now.\n", argv[1]);
			else if (!ncreate)
				printf("%s: written — it applies at the box's "
				       "next launch.\n", argv[1]);
			else if (box_exists(argv[1]))
				printf("%s: written — %s %s at CREATE time, so "
				       "run `kdos-box remove %s` and create it "
				       "again for %s to take effect.\n", argv[1],
				       keys, ncreate == 1 ? "applies" : "apply",
				       argv[1], ncreate == 1 ? "it" : "them");
			else
				printf("%s: written — it applies when the box "
				       "is created.\n", argv[1]);
		}
		profile_print(&p);
		return 0;
	}
	if (BCMD("gc")) {
		int dry = 0, end = 0;

		for (int i = 1; i < argc; i++) {
			if (!strcmp(argv[i], "--dry-run"))
				dry = 1;
			else if (!strcmp(argv[i], "--session-end"))
				end = 1;
			else
				return box_usage(stderr, 2);
		}
		return cmd_box_gc(dry, end);
	}
	return box_usage(stderr, 2);
}
