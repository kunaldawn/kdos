/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   kdos update — the user-facing half of machinery that already existed
 *
 *   kdos update check     what is installed, against what the ports tree pins
 *   kdos update apply     take it, from the binhost where possible
 *   kdos update theme     re-run the generators for $HOME after an art upgrade
 *
 * A/B root slots, a signed binhost, deltas and reproducible packages are all
 * built and all proved. What was missing was a sentence a user could type, and
 * that is all this is: ORCHESTRATION OF EXISTING TOOLS. No new package format,
 * no new trust path, no second index parser — `kpkg binhost` is exec'd through
 * an argv and its exit code is the answer, so the three equality tests and the
 * Ed25519 verification stay in the one program that owns them.
 *
 * ── What "an update" means here, which is not what it means elsewhere ──
 *
 * kpkg's `E:` is a hash over the RECIPE, and a recipe pins its version. So a
 * binhost package that matches this machine is by construction the same
 * version the ports tree would build — the binhost is a way to avoid
 * compiling, not a way to learn that something is newer. What is newer arrives
 * with the PORTS TREE. So `check` compares the installed database against the
 * recipes on this machine, and `apply` takes the prebuilt where the binhost
 * has one and compiles where it does not.
 *
 * That also means the honest refusal comes early: an image with no ports tree
 * cannot say what a newer version would be, and cannot use a binhost either —
 * kpkg computes no recipe hash without a recipe, so every stanza mismatches.
 * `kdos rebuild`'s KDOS_ISO_SOURCES=1 sticks are the ones that carry one.
 * ---------------------------------
 */

#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "kdos-tools.h"
#include "kpkg.h"

/*
 * The binhost is a PATH, not a URL. Nothing in this tree fetches over the
 * network at install time and adding a downloader here would be a new trust
 * surface for a feature the local case does not need — a binhost on a USB
 * stick, an NFS mount or a loopback share is a directory either way.
 */
#define UPDATE_CONF "/etc/kdos/update.conf"

static const char *A = "", *W = "", *D = "", *O = "";

static void up_colours(void)
{
	if (!isatty(STDOUT_FILENO))
		return;
	A = "\033[1;32m";
	W = "\033[1;33m";
	D = "\033[2;32m";
	O = "\033[0m";
}

/* $KDOS_BINHOST beats the config file, which is the shape every other
 * overridable path in this package has. */
static char *binhost_dir(void)
{
	const char *e = getenv("KDOS_BINHOST");
	if (e && *e)
		return kb_strdup(e);

	char *conf = kb_read_all(UPDATE_CONF, NULL);
	if (!conf)
		return NULL;
	char *out = NULL;
	for (char *l = conf; l && *l;) {
		char *nl = strchr(l, '\n');
		if (nl)
			*nl = 0;
		while (*l == ' ' || *l == '\t')
			l++;
		if (*l != '#' && !strncmp(l, "binhost", 7)) {
			char *v = l + 7;
			while (*v == ' ' || *v == '\t' || *v == '=')
				v++;
			v[strcspn(v, " \t\r")] = 0;
			if (*v) {
				out = kb_strdup(v);
				break;
			}
		}
		l = nl ? nl + 1 : NULL;
	}
	free(conf);
	return out;
}

static void no_binhost(void)
{
	fprintf(stderr,
		"kdos update: no binhost configured — nothing to take packages "
		"from.\n"
		"             Write `binhost = /path/to/repo` into %s, or set "
		"KDOS_BINHOST.\n"
		"             `kpkg index <dir> --sign <key>` is what makes a "
		"directory one.\n", UPDATE_CONF);
}

/* ── what is behind ────────────────────────────────────────────────────── */

typedef struct {
	char name[128];
	char have[64], have_rel[32];
	char want[64], want_rel[32];
} Behind;

/*
 * Installed against pinned, for every package with a recipe on this machine.
 * A package with no recipe is an ORPHAN and is counted separately rather than
 * reported as current — the same rule `kdos cve` keeps for a port Alpine has
 * never heard of, and for the same reason: a number that was not earned.
 */
static int survey(const KpConf *c, Behind **out, int *norphan)
{
	*norphan = 0;

	char *db = kp_db_dir(c);
	int ndb = 0;
	char **names = kb_listdir(db, &ndb);
	free(db);
	if (!names)
		return -1;

	Behind *v = kb_calloc((size_t)(ndb > 0 ? ndb : 1), sizeof(*v));
	int n = 0;

	for (int i = 0; i < ndb; i++) {
		char *pd = kp_port_dir(c, names[i]);
		if (!pd) {
			(*norphan)++;
			continue;
		}

		char rver[64] = "", rrel[32] = "";
		kp_recipe_key(pd, "version", rver, sizeof(rver));
		kp_recipe_key(pd, "release", rrel, sizeof(rrel));
		free(pd);
		if (!rver[0])
			continue;

		char iver[64] = "", irel[32] = "";
		kp_installed_version(c, names[i], iver, sizeof(iver), irel,
				     sizeof(irel));

		/* The version first, then the release — a rebuild of the same
		 * upstream with a fixed recipe is exactly what `release` is for
		 * and is a real update to take. */
		int cmp = kp_vercmp(iver, rver);
		if (cmp == 0)
			cmp = kp_vercmp(irel[0] ? irel : "0",
					rrel[0] ? rrel : "0");
		if (cmp >= 0)
			continue;

		kb_strlcpy(v[n].name, names[i], sizeof(v[n].name));
		kb_strlcpy(v[n].have, iver[0] ? iver : "?", sizeof(v[n].have));
		kb_strlcpy(v[n].have_rel, irel, sizeof(v[n].have_rel));
		kb_strlcpy(v[n].want, rver, sizeof(v[n].want));
		kb_strlcpy(v[n].want_rel, rrel, sizeof(v[n].want_rel));
		n++;
	}
	kb_strv_free(names);
	*out = v;
	return n;
}

/* Is there a ports tree at all? Without one nothing here can answer, and
 * saying so is a different answer from "everything is current". */
static int have_ports(const KpConf *c)
{
	int n = 0;
	char **p = kp_all_ports(c, &n);
	kb_strv_free(p);
	return n > 0;
}

static int check_ports_or_explain(const KpConf *c)
{
	if (have_ports(c))
		return 1;
	fprintf(stderr,
		"kdos update: this image carries no ports tree, so nothing can "
		"say what a\n"
		"             newer version would be — and `kpkg binhost` "
		"computes no recipe\n"
		"             hash without a recipe, so no prebuilt package can "
		"match either.\n"
		"             Build the stick with KDOS_ISO_SOURCES=1, or point "
		"PORT_REPO at\n"
		"             a checkout.\n");
	return 0;
}

/* ── check ─────────────────────────────────────────────────────────────── */

static int cmd_check(const KpConf *c)
{
	if (!check_ports_or_explain(c))
		return 2;

	Behind *v = NULL;
	int orphans = 0;
	int n = survey(c, &v, &orphans);
	if (n < 0) {
		fprintf(stderr, "kdos update: no package database\n");
		return 2;
	}

	char *dir = binhost_dir();
	printf("%sKDOS update%s  %s—  the ports tree against what is "
	       "installed%s\n\n", A, O, D, O);

	for (int i = 0; i < n; i++)
		printf("  %-24s %-14s → %s-%s\n", v[i].name, v[i].have,
		       v[i].want, v[i].want_rel[0] ? v[i].want_rel : "1");
	if (n)
		putchar('\n');

	printf("  %d behind, %d with no recipe on this machine\n", n, orphans);
	if (dir) {
		char *idx = kb_path_join(dir, "PACKAGES");
		char *sig = kb_path_join(dir, "PACKAGES.sig");
		if (kb_path_exists(idx) && kb_path_exists(sig))
			printf("  binhost %s — signed index present\n", dir);
		else if (kb_path_exists(idx))
			printf("  %sbinhost %s has an index and no signature — "
			       "`kdos update apply` will refuse it%s\n", W, dir,
			       O);
		else
			printf("  %sbinhost %s has no PACKAGES index — every "
			       "package will be built from source%s\n", W, dir,
			       O);
		free(idx);
		free(sig);
	} else {
		printf("  %sno binhost configured — `kdos update apply` would "
		       "compile everything%s\n", D, O);
	}
	free(dir);
	free(v);
	return n ? 1 : 0;
}

/* ── the A/B half ──────────────────────────────────────────────────────── */

/*
 * Where the inactive slot is mounted, or NULL.
 *
 * The state file names a slot by filesystem UUID and nothing on the machine
 * mounts it for us — installing into a slot that is not mounted would mean
 * this program picking a mountpoint and mounting a filesystem, which is a
 * different and much larger promise than "install packages". So the rule is:
 * find it if it is already mounted, and otherwise say exactly what to do.
 */
static char *slot_mountpoint(const char *uuid)
{
	if (!uuid || !*uuid)
		return NULL;

	char link[512];
	char *by = kb_path_join("/dev/disk/by-uuid", uuid);
	ssize_t ln = readlink(by, link, sizeof(link) - 1);
	free(by);
	if (ln <= 0)
		return NULL;
	link[ln] = 0;

	/* readlink gives ../../sda3; only the leaf is needed to match
	 * /proc/mounts, which spells devices absolutely. */
	const char *leaf = kb_basename(link);
	char devpath[128];
	snprintf(devpath, sizeof(devpath), "/dev/%s", leaf);

	char *mounts = kb_read_all("/proc/mounts", NULL);
	if (!mounts)
		return NULL;
	char *out = NULL;
	for (char *l = mounts; l && *l && !out;) {
		char *nl = strchr(l, '\n');
		if (nl)
			*nl = 0;
		char *sp = strchr(l, ' ');
		if (sp) {
			*sp = 0;
			if (!strcmp(l, devpath)) {
				char *mp = sp + 1;
				mp[strcspn(mp, " ")] = 0;
				if (strcmp(mp, "/"))	/* not the running root */
					out = kb_strdup(mp);
			}
		}
		l = nl ? nl + 1 : NULL;
	}
	free(mounts);
	return out;
}

/* `kdos-bootctl status --json`, read for two fields. Exec'd rather than linked
 * to the state parser directly so there is one reader of that file and it is
 * the one that owns its fsync discipline. */
static int bootctl_layout(char *other_slot, size_t oslen, char *other_uuid,
			  size_t oulen)
{
	other_slot[0] = other_uuid[0] = 0;
	if (!kb_have_prog("kdos-bootctl"))
		return 0;

	KbArgv a = {0};
	kb_argv_add(&a, "kdos-bootctl");
	kb_argv_add(&a, "status");
	kb_argv_add(&a, "--json");
	kb_argv_end(&a);

	char out[1024] = {0};
	if (kb_run_capture(&a, out, sizeof(out)) != 0 || !strstr(out, "\"configured\": true"))
		return 0;

	int active_b = strstr(out, "\"active\": \"b\"") != NULL;
	kb_strlcpy(other_slot, active_b ? "a" : "b", oslen);

	const char *key = active_b ? "\"slot_a\": \"" : "\"slot_b\": \"";
	const char *p = strstr(out, key);
	if (!p)
		return 0;
	p += strlen(key);
	const char *q = strchr(p, '"');
	if (!q || q == p)
		return 0;
	size_t n = (size_t)(q - p);
	if (n >= oulen)
		n = oulen - 1;
	memcpy(other_uuid, p, n);
	other_uuid[n] = 0;
	return 1;
}

/* ── apply ─────────────────────────────────────────────────────────────── */

/*
 * One package: the binhost first, source second.
 *
 * kpkg's exit codes carry the whole decision — 0 it took the prebuilt, 1 the
 * three equality tests did not all pass so build it, 2 it REFUSED (no trusted
 * signature, a checksum that did not match). A 2 stops the run: working around
 * a refused signature by compiling instead would be treating a security answer
 * as a performance hint.
 *
 * The alternate root travels in KPKG_ROOT rather than in argv. `kpkg install`
 * takes --root and `kpkg binhost` does not — its parser would read the flag as
 * the binhost directory — and the env var is what both honour, through the one
 * kp_conf_load every kpkg entry point already runs.
 */
static int apply_one(const char *dir, const char *pkg, int binhost_only, int dry)
{
	if (dir) {
		KbArgv a = {0};
		kb_argv_add(&a, "kpkg");
		kb_argv_add(&a, "binhost");
		kb_argv_add(&a, dir);
		kb_argv_add(&a, pkg);
		if (dry)
			kb_argv_add(&a, "--dry-run");
		kb_argv_end(&a);

		int rc = kb_run_tty(&a);
		if (rc == 0)
			return 0;
		if (rc == 2)
			return 2;
	}
	if (binhost_only) {
		printf("  %s%s: not in the binhost, and --binhost-only was "
		       "asked for%s\n", W, pkg, O);
		return 1;
	}
	if (dry) {
		printf("  %s: would be built from source\n", pkg);
		return 0;
	}

	KbArgv b = {0};
	kb_argv_add(&b, "kpkg");
	kb_argv_add(&b, "install");
	kb_argv_add(&b, "-f");
	kb_argv_add(&b, pkg);
	kb_argv_end(&b);
	return kb_run_tty(&b) == 0 ? 0 : 1;
}

static int cmd_apply(const KpConf *c, int argc, char **argv)
{
	int dry = 0, binhost_only = 0, in_place = 0;
	const char *root = NULL;

	for (int i = 0; i < argc; i++) {
		if (!strcmp(argv[i], "--dry-run"))
			dry = 1;
		else if (!strcmp(argv[i], "--binhost-only"))
			binhost_only = 1;
		else if (!strcmp(argv[i], "--in-place"))
			in_place = 1;
		else if (!strcmp(argv[i], "--root") && i + 1 < argc)
			root = argv[++i];
		else {
			fprintf(stderr, "usage: kdos update apply [--dry-run] "
					"[--binhost-only] [--root DIR] "
					"[--in-place]\n");
			return 2;
		}
	}

	if (!check_ports_or_explain(c))
		return 2;

	char *dir = binhost_dir();
	if (!dir && binhost_only) {
		no_binhost();
		free(dir);
		return 2;
	}

	/*
	 * A/B: an inactive slot exists to receive an update WITHOUT touching
	 * the system you are running, so installing in place on a machine that
	 * has one throws away the rollback it was set up for. It is refused
	 * rather than silently done, and --in-place is the way to say you meant
	 * it. The slot is marked as the try slot only after everything
	 * installed: a candidate that got half an update is not a candidate.
	 */
	char slot[8], uuid[96];
	char *mnt = NULL;
	int ab = 0, layout = 0;
	if (!root && (layout = bootctl_layout(slot, sizeof(slot), uuid,
					      sizeof(uuid)))) {
		mnt = slot_mountpoint(uuid);
		if (mnt) {
			root = mnt;
			ab = 1;
			printf("%sA/B:%s installing into slot %s at %s\n", A, O,
			       slot, mnt);
		} else if (!in_place) {
			fprintf(stderr,
				"kdos update: this machine has A/B root slots "
				"and slot %s (%s) is not\n"
				"             mounted, so there is nowhere to "
				"install the candidate.\n"
				"             Mount it and re-run, or pass "
				"--in-place to update the\n"
				"             running root and give up the "
				"rollback.\n", slot, uuid);
			free(dir);
			return 2;
		} else {
			printf("%sA/B:%s slot %s is not mounted — updating the "
			       "running root in place\n", W, O, slot);
		}
	}
	KpConf tconf;
	if (root) {
		setenv("KPKG_ROOT", root, 1);
		/* And re-read it, because the survey below walks kp_db_dir(c)
		 * — the conf loaded at startup, which captured KPKG_ROOT
		 * before the slot was known. Without this the worklist
		 * describes the RUNNING root while the installs land in the
		 * candidate, which after one A/B cycle is a different set of
		 * packages. */
		kp_conf_load(&tconf);
		c = &tconf;
	} else if (!layout)
		printf("%sno A/B slots — updating the running root in "
		       "place%s\n", D, O);

	Behind *v = NULL;
	int orphans = 0;
	int n = survey(c, &v, &orphans);
	if (n <= 0) {
		printf("nothing to take: everything installed matches the "
		       "ports tree\n");
		free(v);
		free(dir);
		free(mnt);
		return n < 0 ? 2 : 0;
	}

	int done = 0, failed = 0;
	for (int i = 0; i < n; i++) {
		printf("\n%s==>%s %s %s → %s\n", A, O, v[i].name, v[i].have,
		       v[i].want);
		int rc = apply_one(dir, v[i].name, binhost_only, dry);
		if (rc == 2) {
			fprintf(stderr, "\nkdos update: the binhost refused a "
					"package — stopping. A refused\n"
					"             signature is not "
					"something to work around by "
					"compiling.\n");
			failed++;
			break;
		}
		if (rc == 0)
			done++;
		else
			failed++;
	}

	printf("\n%d of %d taken, %d failed\n", done, n, failed);

	if (ab && !dry && !failed) {
		KbArgv t = {0};
		kb_argv_add(&t, "kdos-bootctl");
		kb_argv_add(&t, "try");
		kb_argv_add(&t, slot);
		kb_argv_end(&t);
		if (kb_run_tty(&t) == 0)
			printf("slot %s is the candidate: it gets three boots "
			       "to prove itself, then rolls back\n", slot);
		else
			fprintf(stderr, "kdos update: slot %s was updated but "
					"could not be marked as the candidate "
					"— it will not be booted\n", slot);
	}

	free(v);
	free(dir);
	free(mnt);
	return failed ? 1 : 0;
}

/* ── theme ─────────────────────────────────────────────────────────────── */

/*
 * The gap this closes is small and real: kdos-icons, kdos-cursors and
 * kdos-gtk-theme install their ART, and the copies under $HOME were generated
 * from an older version of it. Nothing re-runs the generators after a package
 * upgrade, so a home keeps stale icons forever. This is `kdos theme <current>`
 * and deliberately nothing more — a post-upgrade hook that did something
 * cleverer would be a second theming path to keep in step with the first.
 */
int kdt_update(int argc, char **argv, int (*theme)(int, char **))
{
	up_colours();

	const char *what = argc > 0 ? argv[0] : "check";
	int rest = argc > 0 ? argc - 1 : 0;
	char **restv = argv + 1;

	if (!strcmp(what, "theme")) {
		char *cur = kb_strdup(kdt_current_accent());
		char *av[1] = { cur };
		int rc = theme(1, av);
		free(cur);
		return rc;
	}

	KpConf c;
	kp_conf_load(&c);

	if (!strcmp(what, "check"))
		return cmd_check(&c);
	if (!strcmp(what, "apply"))
		return cmd_apply(&c, rest, restv);

	fprintf(stderr, "usage: kdos update {check|apply|theme}\n"
			"  check   what the ports tree pins that is not "
			"installed\n"
			"  apply   take it — binhost first, source second; "
			"A/B aware\n"
			"  theme   re-run the theme generators for $HOME after "
			"an art upgrade\n");
	return 2;
}
