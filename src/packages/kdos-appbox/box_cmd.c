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
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/vfs.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

/* A box name is a container name, a profile filename and a path component. */
static int name_ok(const char *s)
{
	if (!s || !*s || strlen(s) >= 64)
		return 0;
	if (*s == '.' || *s == '-')
		return 0;
	for (const char *c = s; *c; c++)
		if (!isalnum((unsigned char)*c) && *c != '.' && *c != '_' &&
		    *c != '-')
			return 0;
	return 1;
}

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

static int cmd_box_list(void)
{
	char *buf = kb_calloc(1, 1 << 16);
	KbArgv a = {0};
	char *line, *save;
	char **profiles;
	char *pdir = kb_calloc(1, MAX_LINE);
	int seen[64] = {0};
	int nseen = 0;
	char names[64][64];

	printf("%-16s %-24s %-10s %-11s %8s  %s\n", "BOX", "BASE", "STATE",
	       "PERSIST", "DISK", "ACCENT");

	kb_argv_add(&a, "podman");
	kb_argv_add(&a, "ps");
	kb_argv_add(&a, "--all");
	kb_argv_add(&a, "--format");
	kb_argv_add(&a, "{{.Names}}\t{{.State}}");
	kb_argv_end(&a);
	if (kb_run_capture(&a, buf, 1 << 16) != 0)
		buf[0] = 0;

	for (line = strtok_r(buf, "\n", &save); line;
	     line = strtok_r(NULL, "\n", &save)) {
		char *tab = strchr(line, '\t');
		Profile p;
		char *bd;

		if (!tab)
			continue;
		*tab++ = 0;
		profile_load(&p, line);
		bd = box_dir(line);
		printf("%-16s %-24s %-10s %-11s %8s  %s\n", line,
		       p.base[0] ? p.base : p.image[0] ? p.image : "-",
		       tab, persist_name(p.persist),
		       kb_human_size(dir_bytes(bd)),
		       p.accent[0] ? p.accent : "session");
		free(bd);
		if (nseen < 64)
			kb_strlcpy(names[nseen++], line, 64);
	}
	free(buf);

	/*
	 * A box with a profile and no container is still a box — it has been
	 * described and not yet created. Leaving it out of `list` is how
	 * somebody ends up writing the same profile twice.
	 */
	snprintf(pdir, MAX_LINE, "%s/.config/kdos/boxes", kb_home_dir());
	profiles = kb_listdir(pdir, NULL);
	for (char **f = profiles; f && *f; f++) {
		char nm[64];
		size_t l = strlen(*f);
		int known = 0;
		Profile p;

		if (l < 6 || strcmp(*f + l - 5, ".conf"))
			continue;
		if (l - 5 >= sizeof(nm))
			continue;
		memcpy(nm, *f, l - 5);
		nm[l - 5] = 0;
		for (int i = 0; i < nseen; i++)
			if (!strcmp(names[i], nm))
				known = 1;
		if (known)
			continue;
		profile_load(&p, nm);
		printf("%-16s %-24s %-10s %-11s %8s  %s\n", nm,
		       p.base[0] ? p.base : p.image[0] ? p.image : "-",
		       "not created",
		       persist_name(p.persist), "-",
		       p.accent[0] ? p.accent : "session");
	}
	kb_strv_free(profiles);
	free(pdir);
	(void)seen;
	return 0;
}

/* ── create ────────────────────────────────────────────────────────────── */

/*
 * `image:` IS AN ONLINE OPERATION AND SAYS SO BEFORE IT DOES ANYTHING. It
 * fetches unsigned content from somebody else's registry; KDOS_REQUIRE_SIG
 * does not cover it and pretending otherwise would be dishonest. The medium's
 * own bases are the trusted set.
 */
static int base_pull(const char *ref)
{
	const char *w[3];

	fprintf(stderr,
		"==> %s is an OCI image: this reaches the network and fetches\n"
		"    content nobody in /etc/kdos/keys has signed. The packs on\n"
		"    your medium are the offline, verifiable alternative.\n", ref);
	w[0] = "pull";
	w[1] = ref;
	return podman(w, 2);
}

static int cmd_box_create(int argc, char **argv)
{
	Profile p;
	const char *box;
	char merged[MAX_LINE];
	int rc;

	if (argc < 1) {
		fprintf(stderr, "usage: kdos-box create <name> [key=value ...]\n");
		return 2;
	}
	/*
	 * As with `create`: this verb is a person at a prompt, and when the
	 * container refuses to start, podman's own sentence is the diagnosis.
	 * `kb_run` swallows a child's stderr by default, which is right for
	 * the LAUNCH path and leaves `could not start <box>` as the only thing
	 * anybody sees here.
	 */
	kb_proc_verbose = 1;
	box = argv[0];
	if (!name_ok(box))
		kb_die("a box name is [A-Za-z0-9._-]");
	if (box_exists(box)) {
		kb_warn("%s already exists", box);
		return 1;
	}

	profile_load(&p, box);
	for (int i = 1; i < argc; i++)
		if (profile_set(&p, argv[i]) != 0)
			kb_warn("unknown key in '%s'", argv[i]);
	profile_save(&p);

	/*
	 * PODMAN'S OWN ERROR IS THE POINT OF THIS VERB. `kb_run` sends a
	 * child's stderr to /dev/null unless kb_proc_verbose is set, which is
	 * right for the launch path — a click must not spray container
	 * plumbing at a desktop — and exactly wrong here: `kdos-box create` is
	 * a person asking for a box once, and when podman refuses, its
	 * sentence is the whole diagnosis. Four separate failures in this lane
	 * presented as `exit 125` and nothing else because of this line.
	 * `pack_box_ensure`, which is what a launch calls, is left quiet.
	 */
	kb_proc_verbose = 1;

	if (!strncmp(p.base, "pack:", 5)) {
		rc = pack_compose(box, p.base + 5, merged, sizeof(merged));
		if (rc != 0) {
			if (rc < 0)
				kb_die("kdos-packd is not running");
			kb_die("%s", merged);
		}
		rc = pack_box_create(&p, merged);
		if (rc != 0) {
			pack_decompose(box);
			/*
			 * A CREATE THAT FAILS MUST SAY SO. podman's own
			 * diagnostic goes to stderr and there are runs where
			 * it prints nothing at all — a bare exit 125 with a
			 * profile left on disk reads as a box that exists and
			 * will not start. The one cause this program can name
			 * for itself is the live session: an overlay upper
			 * cannot sit on overlayfs, which is what $HOME is on a
			 * booted ISO, and it is the same reading `kdos doctor`
			 * already reports. Anything else is podman's to
			 * explain and this says which exit status it gave.
			 */
			struct statfs st;
			const char *h = kb_home_dir();

			if (statfs(h, &st) == 0 &&
			    (unsigned long)st.f_type == 0x794c7630UL)
				kb_warn("%s: $HOME is on overlayfs (a live "
					"session), so a box's overlay upper "
					"has nowhere to go — a persistent box "
					"needs an installed system", box);
			else
				kb_warn("%s: podman could not create the box "
					"(exit %d)", box, rc);
		}
	} else if (!strncmp(p.base, "image:", 6)) {
		if (base_pull(p.base + 6) != 0)
			kb_die("could not fetch %s", p.base + 6);
		kb_strlcpy(p.image, p.base + 6, sizeof(p.image));
		rc = box_create(&p);
	} else if (!strncmp(p.base, "box:", 4)) {
		/* Clone another box's LOWER stack, not its upper: a clone
		 * starts from the same software with an empty workspace, which
		 * is what somebody asking for one means. `kdos-box clone`
		 * copies the upper as well and is the other verb. */
		Profile src;
		if (!name_ok(p.base + 4))
			kb_die("box:<name> — and a name is [A-Za-z0-9._-]");
		profile_load(&src, p.base + 4);
		if (!src.base[0])
			kb_die("%s has no pack or image base to clone",
			       p.base + 4);
		kb_strlcpy(p.base, src.base, sizeof(p.base));
		profile_save(&p);
		return cmd_box_create(argc, argv);
	} else {
		kb_die("%s: a box is made of something — base=pack:<id>, "
		       "base=image:<ref> or base=box:<name>", box);
	}
	if (rc != 0)
		return rc;

	printf("%s created\n", box);
	profile_print(&p);
	return 0;
}

/* ── enter ─────────────────────────────────────────────────────────────── */

/*
 * A TERMINAL THAT SAYS WHERE IT IS. foot cannot reload its configuration, but
 * it takes overrides on the command line and `kdos theme` already generates a
 * palette per accent — so a box with `accent = amber` opens an amber terminal
 * and there is never a question about which window is which. A starship module
 * reads KDOS_BOX and puts the name in the prompt.
 */
static int cmd_box_enter(int argc, char **argv)
{
	Profile p;
	KbArgv a = {0};
	const char *box;
	char theme[MAX_LINE];

	if (argc < 1) {
		fprintf(stderr, "usage: kdos-box enter <name> [command ...]\n");
		return 2;
	}
	box = argv[0];
	if (!name_ok(box))
		kb_die("a box name is [A-Za-z0-9._-]");
	profile_load(&p, box);

	if (!box_exists(box))
		kb_die("%s does not exist — `kdos-box create %s`", box, box);
	if (pack_box_start(box) != 0)
		kb_die("could not start %s", box);
	if (!box_setup_done(box))
		box_wait_ready(box, 30);

	/* Under a terminal already, exec straight in: wrapping a shell in
	 * another terminal window is what `enter` is for, not what it means. */
	if (!isatty(STDIN_FILENO) || argc > 1 || getenv("KDOS_BOX_NOTERM")) {
		kb_argv_add(&a, "podman");
		kb_argv_add(&a, "exec");
		kb_argv_add(&a, "--interactive");
		if (isatty(STDIN_FILENO))
			kb_argv_add(&a, "--tty");
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

	kb_argv_add(&a, "foot");
	kb_argv_addf(&a, "--title=%s — KDOS box", box);
	if (p.accent[0]) {
		/* `kdos theme` writes a foot palette per accent into
		 * ~/.config/foot/themes; naming it here is the one way a
		 * terminal can wear an accent foot cannot reload into. */
		snprintf(theme, sizeof(theme), "%s/.config/foot/themes/%s",
			 kb_home_dir(), p.accent);
		if (kb_path_exists(theme)) {
			kb_argv_add(&a, "--config");
			kb_argv_add(&a, theme);
		}
	}
	kb_argv_add(&a, "--");
	kb_argv_add(&a, "kdos-box");
	kb_argv_add(&a, "enter");
	kb_argv_add(&a, box);
	kb_argv_add(&a, "--shell");
	kb_argv_end(&a);
	setenv("KDOS_BOX_NOTERM", "1", 1);
	execvp(a.v[0], (char *const *)a.v);
	kb_die("foot: not found");
	return 1;
}

/* ── apps, export ──────────────────────────────────────────────────────── */

static int cmd_box_apps(const char *box)
{
	KbArgv a = {0};
	char *buf = kb_calloc(1, 1 << 16);

	if (!name_ok(box))
		kb_die("a box name is [A-Za-z0-9._-]");
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
 * THE DEFAULT BOX KEEPS UPSTREAM'S OWN DESKTOP ID; a secondary box's app gets
 * `<upstream-id>.<box>.desktop` and `Name=GIMP (arch)`. That is a deliberate
 * refinement of the rule that a launcher must be named with upstream's id: the
 * rule exists because a dock matches a toplevel to an entry by file id, and
 * the panel now has a better key than the filename — the box the window's
 * security context names. Nothing that works today changes, because the
 * default box's launchers are untouched.
 *
 * The shim gets `@<box>` for the same reason: the busybox-style basename
 * dispatch already accepts any name, and `@` is an ordinary filename
 * character, so `gimp@arch photo.png` costs no new machinery.
 */
static int cmd_box_export(const char *box, const char *app, int undo)
{
	char *skel, *entry, *shim, *bin;
	char id[256];

	if (!name_ok(box) || !app || !*app)
		kb_die("usage: kdos-box export <box> <app>");

	skel = kb_path_join(kb_home_dir(), ".local/share/applications");
	kb_mkdir_p(skel);
	snprintf(id, sizeof(id), "%s.%s.desktop", app, box);
	entry = kb_path_join(skel, id);
	bin = kb_path_join(kb_home_dir(), ".local/bin");
	kb_mkdir_p(bin);
	snprintf(id, sizeof(id), "%s@%s", app, box);
	shim = kb_path_join(bin, id);

	if (undo) {
		int n = 0;
		n += unlink(entry) == 0;
		n += unlink(shim) == 0;
		printf("%s@%s: %d removed\n", app, box, n);
	} else {
		KbBuf d = {0};
		KbBuf line = {0};
		const char *words[4] = { "kdos-box", "run", box, app };

		/*
		 * Quoted word by word through the same helper genlaunchers
		 * uses, so what is written here reads back through
		 * kxdg_exec_split as the four arguments it was built from — a
		 * box or an app name with a space in it is otherwise three
		 * arguments the moment somebody clicks it.
		 */
		for (int i = 0; i < 4; i++) {
			char q[512];
			if (kxdg_exec_quote(words[i], q, sizeof(q)) != 0)
				continue;
			kb_buf_printf(&line, "%s%s", i ? " " : "", q);
		}

		kb_buf_printf(&d, "[Desktop Entry]\nType=Application\n");
		kb_buf_printf(&d, "Name=%s (%s)\n", app, box);
		kb_buf_printf(&d, "Exec=%s %%U\n", line.p ? line.p : "");
		kb_buf_printf(&d, "Icon=%s\n", app);
		kb_buf_printf(&d, "Terminal=false\n");
		kb_buf_printf(&d, "StartupWMClass=%s\n", app);
		kb_buf_printf(&d, "Categories=X-KDOS-Box;\n");
		kb_write_all(entry, d.p, d.n);
		kb_buf_free(&d);
		kb_buf_free(&line);

		unlink(shim);
		if (symlink("/usr/local/bin/kdos-appbox", shim) != 0)
			kb_warn("%s: %s", shim, strerror(errno));
		printf("%s exported as \"%s (%s)\" and as `%s@%s`\n", app, app,
		       box, app, box);
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
		kb_die("a box name is [A-Za-z0-9._-]");
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
		kb_die("a box name is [A-Za-z0-9._-]");
	if (!tag) {
		time_t t = time(NULL);
		struct tm tm;
		gmtime_r(&t, &tm);
		strftime(stamp, sizeof(stamp), "%Y%m%d-%H%M%S", &tm);
		tag = stamp;
	}
	if (!name_ok(tag))
		kb_die("a tag is [A-Za-z0-9._-]");

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

static int cmd_box_stop(const char *box)
{
	const char *w[3];

	if (!name_ok(box))
		kb_die("a box name is [A-Za-z0-9._-]");
	w[0] = "stop";
	w[1] = "-t";
	w[2] = "10";
	{
		KbArgv a = {0};
		kb_argv_add(&a, "podman");
		for (int i = 0; i < 3; i++)
			kb_argv_add(&a, w[i]);
		kb_argv_add(&a, box);
		kb_argv_end(&a);
		return kb_run(&a);
	}
}

static int cmd_box_remove(const char *box, int force)
{
	KbArgv a = {0};

	if (!name_ok(box))
		kb_die("a box name is [A-Za-z0-9._-]");
	kb_argv_add(&a, "podman");
	kb_argv_add(&a, "rm");
	if (force)
		kb_argv_add(&a, "-f");
	kb_argv_add(&a, box);
	kb_argv_end(&a);
	kb_run(&a);
	/* The overlay goes with it; the PACKS do not, because another box may
	 * be using the same runtime. kdos-packd reference-counts them. */
	pack_decompose(box);
	printf("%s removed — its profile and its writable layer are still in "
	       "~/.local/share/kdos/boxes/%s\n", box, box);
	return 0;
}

/*
 * gc — an idle box costs a conmon, a shell and its page cache. One `waitpid`
 * and a clock, on the ksvc timer the session already runs, rather than a
 * seventh root daemon for a job this size.
 *
 * A BOX WITH A MAPPED WINDOW IS NEVER IDLE, whatever its clock says.
 *
 * THREE ANSWERS, NOT TWO: 1 has a window, 0 has none, -1 there was nothing to
 * ask. The third is not the second. A collector that read "cannot tell" as "no
 * window" would stop every warmed box on a desktop it did not know how to
 * question — which is exactly what happened to the console desktop, whose
 * session is not a compositor and does not answer `kdos hey`.
 *
 * The console publishes its window set as a file beside its socket, because
 * teaching this binary that protocol would pull libkcon and the whole cell
 * model into something that is on every image.
 */
static int box_has_window(const char *box)
{
	const char *con = getenv("KDOS_CON");
	KbArgv a = {0};
	char *buf;
	int found;

	if (con && *con) {
		char path[256];
		size_t n = strlen(con);

		if (n < 6 || strcmp(con + n - 5, ".sock"))
			return -1;
		snprintf(path, sizeof(path), "%.*s.windows", (int)(n - 5), con);

		char win[4096];

		if (kb_read_file(path, win, sizeof(win)) < 0)
			return -1;	/* the session has not published yet */
		return strstr(win, box) != NULL;
	}

	buf = kb_calloc(1, 1 << 16);
	kb_argv_add(&a, "kdos");
	kb_argv_add(&a, "hey");
	kb_argv_add(&a, "boxes");
	kb_argv_end(&a);
	if (kb_run_capture(&a, buf, 1 << 16) != 0) {
		free(buf);
		return -1;	/* no session to ask */
	}
	found = strstr(buf, box) != NULL;
	free(buf);
	return found;
}

static int cmd_box_gc(int dry)
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

		if (!t1)
			continue;
		*t1++ = 0;
		t2 = strchr(t1, '\t');
		if (!t2)
			continue;
		*t2++ = 0;
		if (strcmp(t1, "running"))
			continue;
		profile_load(&p, line);
		if (!p.autostop_s)
			continue;
		started = atol(t2);
		if (started <= 0 || time(NULL) - started < p.autostop_s)
			continue;
		int win = box_has_window(line);

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
		if (dry) {
			printf("  %-16s idle past %ds — would stop\n", line,
			       p.autostop_s);
		} else {
			printf("  %-16s idle past %ds — stopping\n", line,
			       p.autostop_s);
			cmd_box_stop(line);
		}
		stopped++;
	}
	free(buf);
	if (!stopped)
		printf("  nothing idle\n");
	return 0;
}

/* ── the dispatch ──────────────────────────────────────────────────────── */

static int box_usage(void)
{
	fprintf(stderr,
"usage: kdos-box list\n"
"       kdos-box create <name> [base=pack:<id>|image:<ref>|box:<name>] [key=value ...]\n"
"       kdos-box enter  <name> [command ...]\n"
"       kdos-box run    <name> <app> [args ...]\n"
"       kdos-box apps   <name>\n"
"       kdos-box export <name> <app>        also: unexport\n"
"       kdos-box freeze <name> [out.kpack]  the writable layer, as one pack\n"
"       kdos-box import <file.kpack> [as <name>]\n"
"       kdos-box clone  <src> <dst>\n"
"       kdos-box snapshot <name> [tag]      also: snapshots, rollback\n"
"       kdos-box start | stop | restart | remove <name>\n"
"       kdos-box profile <name> [key=value ...]\n"
"       kdos-box gc [--dry-run]\n"
"\nAn app box and a dev box differ in three profile keys, not in kind:\n"
"`persistence`, `base` and `export`.\n");
	return 2;
}

int box_main(int argc, char **argv)
{
	const char *cmd;

	if (argc < 1)
		return box_usage();
	cmd = argv[0];

#define BCMD(s) (!strcmp(cmd, s))
	if (BCMD("list") || BCMD("ls"))
		return cmd_box_list();
	if (BCMD("create"))
		return cmd_box_create(argc - 1, argv + 1);
	if (BCMD("enter")) {
		/* `--shell` is what the foot wrapper re-enters with; it means
		 * "you are already in a terminal, do not open another". */
		if (argc > 2 && !strcmp(argv[2], "--shell")) {
			setenv("KDOS_BOX_NOTERM", "1", 1);
			return cmd_box_enter(1, argv + 1);
		}
		return cmd_box_enter(argc - 1, argv + 1);
	}
	if (BCMD("run") && argc > 2) {
		g_box = argv[1];
		return cmd_run(argc - 2, argv + 2);
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

		if (!name_ok(argv[1]) || !name_ok(argv[2]))
			kb_die("a box name is [A-Za-z0-9._-]");
		profile_load(&src, argv[1]);
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
	if (BCMD("start") && argc > 1)
		return pack_box_start(argv[1]);
	if (BCMD("stop") && argc > 1)
		return cmd_box_stop(argv[1]);
	if (BCMD("restart") && argc > 1) {
		cmd_box_stop(argv[1]);
		return pack_box_start(argv[1]);
	}
	if (BCMD("remove") && argc > 1)
		return cmd_box_remove(argv[1], argc > 2 &&
					       !strcmp(argv[2], "--force"));
	if (BCMD("profile") && argc > 1) {
		Profile p;
		if (!name_ok(argv[1]))
			kb_die("a box name is [A-Za-z0-9._-]");
		profile_load(&p, argv[1]);
		if (argc > 2) {
			for (int i = 2; i < argc; i++)
				profile_set(&p, argv[i]);
			profile_save(&p);
			printf("%s: written — namespaces and volumes apply at "
			       "CREATE time, so run `kdos-box remove %s` and "
			       "create it again for those to take effect.\n",
			       argv[1], argv[1]);
		}
		profile_print(&p);
		return 0;
	}
	if (BCMD("gc"))
		return cmd_box_gc(argc > 1 && !strcmp(argv[1], "--dry-run"));
	return box_usage();
}
