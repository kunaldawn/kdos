/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   kdos update — a newer recipe tree, and the packages it pins, built here
 *
 *   kdos update check     what is installed, against what the ports tree pins
 *   kdos update fetch     replace /ports with the newest signed recipe tree
 *   kdos update apply     build what is behind from source; A/B aware
 *   kdos update theme     re-run the generators for $HOME after an art upgrade
 *
 * ── What "an update" means here, which is not what it means elsewhere ──
 *
 * A recipe pins its version, so what is newer arrives with the PORTS TREE and
 * nowhere else. Every image carries the recipe-only tree at /ports — each
 * `kpkgbuild`, `build.sh` and patch, `shelves`, `sources.idx` and our own
 * programs under `src/`, and no source tarball. So an update is two steps:
 *
 *   fetch   a newer tree arrives over the network as one signed tarball, is
 *           verified against /etc/kdos/keys/tree, and replaces /ports whole
 *   apply   every package whose recipe now pins something newer is BUILT ON
 *           THIS MACHINE; kpkg downloads each source as it goes (the
 *           sources-<N> archive first, then upstream) and checks it against
 *           the recipe's sha256
 *
 * There is no binary package host. An update needs the network for the tree
 * and the sources, and compiles on the machine — for hours, on a large set.
 *
 * WHAT CAN GO WRONG IS RECORDED, so a panel badge can say it: no tree on the
 * machine, no network, a fetch that failed (a download, a signature, a tree
 * older than the one installed), a build that failed. `fetch` and `apply`
 * write their outcome to /var/lib/kdos/update.state and `check` reports it
 * beside the list of what is behind.
 * ---------------------------------
 */

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/syscall.h>

#include "kdos-tools.h"
#include "kpkg.h"
#include "ksig.h"

#ifndef RENAME_EXCHANGE
#define RENAME_EXCHANGE (1 << 1)
#endif

#define UPDATE_CONF  "/etc/kdos/update.conf"
#define UPDATE_STATE "/var/lib/kdos/update.state"
#define UPDATE_BADGE "/var/lib/kdos/update.json"
#define TREE_KEYS    "/etc/kdos/keys/tree"
/* The repository's latest release is always a KDOS system release — the
 * source archive's volumes are pre-releases and never "latest" — so this URL
 * follows whatever was published last. */
#define TREE_BASE    "https://github.com/kunaldawn/kdos/releases/latest/download"
#define TREE_FILE    "kdos-ports.tar.xz"
#define TREE_MARK    "kdos-tree 1"

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

/* ── where things are ──────────────────────────────────────────────────── */

/* $KDOS_UPDATE_STATE beats the default, so a test run records its outcome
 * away from the machine's own and leaves the badge document alone
 * (badge_refresh). The lock and the last tree's signature live beside it. */
static const char *state_path(void)
{
	const char *e = getenv("KDOS_UPDATE_STATE");
	return e && *e ? e : UPDATE_STATE;
}

/*
 * The tree's root: the directory holding the first repository of PORT_REPO,
 * which is /ports for /ports/core. Every repository kpkg.conf names lives under
 * it, so one replacement moves all of them together.
 */
static void ports_root(const KpConf *c, char *out, size_t cap)
{
	kb_strlcpy(out, c->nrepos > 0 ? c->repos[0] : "/ports/core", cap);
	char *slash = strrchr(out, '/');
	if (slash && slash != out)
		*slash = 0;
}

/* One `key = value` line of update.conf; NULL when absent. */
static char *conf_value(const char *key)
{
	char *conf = kb_read_all(UPDATE_CONF, NULL);
	if (!conf)
		return NULL;
	size_t kl = strlen(key);
	char *out = NULL;
	for (char *l = conf; l && *l;) {
		char *nl = strchr(l, '\n');
		if (nl)
			*nl = 0;
		while (*l == ' ' || *l == '\t')
			l++;
		if (*l != '#' && !strncmp(l, key, kl) &&
		    (l[kl] == ' ' || l[kl] == '\t' || l[kl] == '=')) {
			char *v = l + kl;
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

/*
 * Where a newer tree is fetched from: `--from`, then $KDOS_UPDATE_TREE, then
 * `tree =` in update.conf, then the latest KDOS release. A value with no
 * scheme is a DIRECTORY holding the two files — a stick, an NFS mount — and is
 * read through curl as a file:// URL, so both cases are one code path and one
 * signature check.
 */
static void tree_base(const char *from, char *out, size_t cap)
{
	char *conf = NULL;
	const char *v = from;
	if (!v || !*v)
		v = getenv("KDOS_UPDATE_TREE");
	if (!v || !*v)
		v = conf = conf_value("tree");
	if (!v || !*v)
		v = TREE_BASE;
	if (strstr(v, "://"))
		kb_strlcpy(out, v, cap);
	else if (v[0] == '/')
		snprintf(out, cap, "file://%s", v);
	else {
		char cwd[512];
		snprintf(out, cap, "file://%s/%s",
			 getcwd(cwd, sizeof(cwd)) ? cwd : "", v);
	}
	size_t n = strlen(out);
	while (n > 1 && out[n - 1] == '/')
		out[--n] = 0;
	free(conf);
}

/* ── the tree's identity ───────────────────────────────────────────────── */

/*
 * `<tree>/TREE`, written by whatever made the tree:
 *
 *   kdos-tree 1
 *   commit <hex>
 *   time <unix seconds of that commit>
 *
 * The commit says WHICH tree; the time orders two of them, because a machine
 * has no history to ask which of two commits came later. 0 parsed, -1 not.
 */
static int tree_read(const char *dir, char *commit, size_t ccap, long long *t)
{
	char *p = kb_path_join(dir, "TREE");
	char *data = kb_read_all(p, NULL);
	free(p);
	commit[0] = 0;
	*t = 0;
	if (!data)
		return -1;
	int ok = !strncmp(data, TREE_MARK "\n", sizeof(TREE_MARK));
	for (char *l = data; ok && l && *l;) {
		char *nl = strchr(l, '\n');
		if (nl)
			*nl = 0;
		if (!strncmp(l, "commit ", 7))
			kb_strlcpy(commit, l + 7, ccap);
		else if (!strncmp(l, "time ", 5))
			*t = atoll(l + 5);
		l = nl ? nl + 1 : NULL;
	}
	free(data);
	if (!ok || !commit[0])
		return -1;
	for (const char *c = commit; *c; c++)
		if (!((*c >= '0' && *c <= '9') || (*c >= 'a' && *c <= 'f') ||
		      *c == '-'))
			return -1;
	return 0;
}

/* One commit names the other when either is a prefix of the other: an image
 * records the short hash, a published tree the full one. */
static int same_commit(const char *a, const char *b)
{
	size_t la = strcspn(a, "-"), lb = strcspn(b, "-");
	size_t n = la < lb ? la : lb;
	return n >= 7 && !strncmp(a, b, n);
}

/* ── the recorded outcome ──────────────────────────────────────────────── */

/*
 * update.state, two lines at most, one per step:
 *
 *   fetch <code> <unix time> <detail>
 *   apply <code> <unix time> <detail>
 *
 * <code> is `ok`, `no-network`, `fetch-failed` or `build-failed`; `apply`
 * records a source it could not download as `no-network` or `fetch-failed` on
 * its own line. Each step replaces only its own line, so a fetch tonight does
 * not erase the build that failed yesterday.
 */
static void state_set(const char *step, const char *code, const char *detail)
{
	const char *path = state_path();
	char *old = kb_read_all(path, NULL);
	KbBuf b = {0};
	size_t sl = strlen(step);
	for (char *l = old; l && *l;) {
		char *nl = strchr(l, '\n');
		if (nl)
			*nl = 0;
		if (*l && !(strncmp(l, step, sl) == 0 && l[sl] == ' '))
			kb_buf_printf(&b, "%s\n", l);
		l = nl ? nl + 1 : NULL;
	}
	free(old);
	kb_buf_printf(&b, "%s %s %lld %s\n", step, code, (long long)time(NULL),
		      detail && *detail ? detail : "-");
	if (kb_write_file_atomic(path, b.p) != 0)
		fprintf(stderr, "kdos update: cannot record the outcome in %s: "
			"%s\n", path, strerror(errno));
	kb_buf_free(&b);
}

static int state_get(const char *step, char *code, size_t ccap, char *detail,
		     size_t dcap)
{
	char *data = kb_read_all(state_path(), NULL);
	size_t sl = strlen(step);
	int found = 0;
	code[0] = detail[0] = 0;
	for (char *l = data; l && *l && !found;) {
		char *nl = strchr(l, '\n');
		if (nl)
			*nl = 0;
		if (!strncmp(l, step, sl) && l[sl] == ' ') {
			char *p = l + sl + 1;
			size_t cl = strcspn(p, " ");
			snprintf(code, ccap, "%.*s", (int)cl, p);
			p += cl;
			while (*p == ' ')
				p++;
			p += strcspn(p, " ");	/* the time */
			while (*p == ' ')
				p++;
			kb_strlcpy(detail, strcmp(p, "-") ? p : "", dcap);
			found = 1;
		}
		l = nl ? nl + 1 : NULL;
	}
	free(data);
	return found;
}

/* The signature of the tree last installed, kept beside the state. */
static void last_sig_path(char *out, size_t cap)
{
	snprintf(out, cap, "%s.sig", state_path());
}

/* 1 when `sig` holds the same bytes as the last installed tree's signature. */
static int same_as_last_sig(const char *sig)
{
	char lp[600];
	size_t a = 0, b = 0;
	last_sig_path(lp, sizeof(lp));
	char *x = kb_read_all(sig, &a);
	char *y = kb_read_all(lp, &b);
	int same = x && y && a == b && a > 0 && !memcmp(x, y, a);
	free(x);
	free(y);
	return same;
}

static void keep_sig(const char *sig)
{
	char lp[600];
	last_sig_path(lp, sizeof(lp));
	kb_copy_file(sig, lp);
}

/* One lock for fetch and apply: a tree must never be replaced under a build
 * that is reading it. */
static int update_lock(void)
{
	char lp[600];
	snprintf(lp, sizeof(lp), "%s.lock", state_path());
	int fd = kb_lock_file(lp, 1);
	if (fd < 0)
		fprintf(stderr, "kdos update: another `kdos update fetch` or "
				"`apply` is running\n");
	return fd;
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
		/* `.recipe/` and `.lock` live beside the entries and are not
		 * packages; counted, each would be an orphan. */
		if (names[i][0] == '.')
			continue;
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

#define NO_TREE_TEXT "this machine has no ports tree, so nothing can say " \
		     "what is newer; `kdos update fetch` installs one"

/*
 * The one problem worth a badge, worst first: no tree at all, then a fetch
 * that failed (the machine cannot learn of updates), then a build that failed.
 * `code` is empty when there is none.
 */
static void problem(const KpConf *c, char *code, size_t ccap, char *detail,
		    size_t dcap)
{
	char fc[32], fd[256], ac[32], ad[256];
	code[0] = detail[0] = 0;
	if (!have_ports(c)) {
		kb_strlcpy(code, "no-tree", ccap);
		kb_strlcpy(detail, NO_TREE_TEXT, dcap);
		return;
	}
	if (state_get("fetch", fc, sizeof(fc), fd, sizeof(fd)) &&
	    strcmp(fc, "ok")) {
		kb_strlcpy(code, fc, ccap);
		kb_strlcpy(detail, fd, dcap);
		return;
	}
	if (state_get("apply", ac, sizeof(ac), ad, sizeof(ad)) &&
	    strcmp(ac, "ok")) {
		kb_strlcpy(code, ac, ccap);
		kb_strlcpy(detail, ad, dcap);
	}
}

/* ── check ─────────────────────────────────────────────────────────────── */

/*
 * THE SAME SURVEY AS ONE LINE OF JSON, for a surface rather than a person.
 *
 * A panel badge and a settings page need a number, a list and the problem, and
 * the alternative is parsing the human output — which is coloured, wrapped,
 * and written to be read. A second command computing the same answer would be
 * a second answer; this is the same `survey()` printed differently.
 *
 * IT ANSWERS IN JSON EVEN WHEN IT CANNOT ANSWER: no tree is `"behind":0` with
 * `"problem":"no-tree"`, never a paragraph a caller cannot tell from a parse
 * failure. ITS EXIT STATUS IS THE HUMAN FORM'S: 1 when something is behind, 2
 * when nothing could be surveyed.
 */
static int check_json(const KpConf *c, FILE *o)
{
	char pc[32], pd[256], commit[64] = "", root[512];
	long long t;
	Behind *v = NULL;
	int orphans = 0, n = 0, rc;

	problem(c, pc, sizeof(pc), pd, sizeof(pd));
	ports_root(c, root, sizeof(root));
	tree_read(root, commit, sizeof(commit), &t);

	if (!strcmp(pc, "no-tree")) {
		n = 0;
		rc = 2;
	} else if ((n = survey(c, &v, &orphans)) < 0) {
		n = 0;
		kb_strlcpy(pc, "no-database", sizeof(pc));
		kb_strlcpy(pd, "no package database", sizeof(pd));
		rc = 2;
	} else {
		rc = n ? 1 : 0;
	}

	KbBuf b = {0};
	kb_buf_printf(&b, "{\"behind\":%d,\"orphans\":%d,\"tree\":", n,
		      orphans);
	if (commit[0])
		kb_json_str(&b, commit);
	else
		kb_buf_str(&b, "null");
	kb_buf_str(&b, ",\"problem\":");
	if (pc[0])
		kb_json_str(&b, pc);
	else
		kb_buf_str(&b, "null");
	kb_buf_str(&b, ",\"detail\":");
	kb_json_str(&b, pd);
	kb_buf_str(&b, ",\"packages\":[");
	for (int i = 0; i < n; i++) {
		kb_buf_str(&b, i ? ",{\"name\":" : "{\"name\":");
		kb_json_str(&b, v[i].name);
		kb_buf_str(&b, ",\"have\":");
		kb_json_str(&b, v[i].have);
		kb_buf_printf(&b, ",\"want\":\"%s-%s\"}", v[i].want,
			      v[i].want_rel[0] ? v[i].want_rel : "1");
	}
	kb_buf_str(&b, "]}\n");
	fputs(b.p, o);
	kb_buf_free(&b);
	free(v);
	return rc;
}

/*
 * THE DOCUMENT TO A PATH, ATOMICALLY — so a scheduled job needs no shell.
 *
 * `kdos update check --json > file` is a redirection, and a redirection means
 * the timer table has to be a shell script. Everything in this tree execs
 * through an argument vector precisely so that no configuration file has to
 * be one.
 *
 * TEMP AND RENAME, because a reader arrives on its own schedule: the panel
 * badge reads this file on a tick, and half a document parses as nothing
 * behind — a machine that is out of date reporting that it is not. The
 * document is written whatever the answer, a missing tree included, so the
 * badge shows the problem rather than nothing.
 */
static int check_json_to(const KpConf *c, const char *path)
{
	char tmp[512];
	FILE *o;
	int rc;

	if (snprintf(tmp, sizeof(tmp), "%s.new", path) >= (int)sizeof(tmp)) {
		fprintf(stderr, "kdos update: path too long\n");
		return 2;
	}
	o = fopen(tmp, "w");
	if (!o) {
		fprintf(stderr, "kdos update: cannot write %s: %s\n", tmp,
			strerror(errno));
		return 2;
	}
	rc = check_json(c, o);
	fflush(o);
	fsync(fileno(o));
	if (fclose(o) != 0 || rename(tmp, path) != 0) {
		remove(tmp);
		fprintf(stderr, "kdos update: cannot write %s: %s\n", path,
			strerror(errno));
		return 2;
	}
	return rc;
}

/*
 * The badge document, rewritten after a fetch or an apply as well as by the
 * nightly timer: a person who has just fixed what the badge complained about
 * should not see the complaint until 04:17. The running root's answer, never
 * the slot's — the badge describes the system it is drawn on.
 */
static void badge_refresh(void)
{
	if (geteuid() != 0 || getenv("KDOS_UPDATE_STATE") ||
	    !kb_is_dir("/var/lib/kdos"))
		return;
	unsetenv("KPKG_ROOT");
	KpConf c;
	kp_conf_load(&c);
	check_json_to(&c, UPDATE_BADGE);
}

static int cmd_check_human(const KpConf *c)
{
	char pc[32], pd[256], commit[64] = "", root[512];
	long long t;

	problem(c, pc, sizeof(pc), pd, sizeof(pd));
	if (!strcmp(pc, "no-tree")) {
		fprintf(stderr, "kdos update: %s\n", NO_TREE_TEXT);
		return 2;
	}

	Behind *v = NULL;
	int orphans = 0;
	int n = survey(c, &v, &orphans);
	if (n < 0) {
		fprintf(stderr, "kdos update: no package database\n");
		return 2;
	}

	printf("%sKDOS update%s  %s—  the ports tree against what is "
	       "installed%s\n\n", A, O, D, O);

	for (int i = 0; i < n; i++)
		printf("  %-24s %-14s → %s-%s\n", v[i].name, v[i].have,
		       v[i].want, v[i].want_rel[0] ? v[i].want_rel : "1");
	if (n)
		putchar('\n');

	printf("  %d behind, %d with no recipe on this machine\n", n, orphans);
	ports_root(c, root, sizeof(root));
	if (tree_read(root, commit, sizeof(commit), &t) == 0) {
		char day[16] = "";
		time_t tt = (time_t)t;
		struct tm tm;
		if (t > 0 && gmtime_r(&tt, &tm))
			strftime(day, sizeof(day), "%Y-%m-%d", &tm);
		printf("  %stree %s%s%s at %s%s\n", D, commit, day[0] ? ", " : "",
		       day, root, O);
	} else {
		printf("  %s%s carries no TREE stamp — `kdos update fetch` "
		       "cannot tell an older tree from a newer one%s\n", W,
		       root, O);
	}
	if (pc[0])
		printf("  %s%s: %s%s\n", W, pc, pd, O);
	free(v);
	return n ? 1 : 0;
}

/* ── fetch ─────────────────────────────────────────────────────────────── */

enum { GOT_OK, GOT_FAIL, GOT_OFFLINE };

/* curl's exit codes for "never reached a server": a name that did not resolve
 * (5, 6), a connection refused or unroutable (7), a timeout (28). Anything
 * else reached somebody, and is a failure of that copy. */
static int download(const char *url, const char *out, int quiet)
{
	KbArgv a = {0};
	kb_argv_add(&a, "curl");
	kb_argv_add(&a, "-fL");
	kb_argv_add(&a, !quiet && isatty(STDOUT_FILENO) ? "-#" : "-sS");
	kb_argv_add(&a, "--retry");
	kb_argv_add(&a, "2");
	kb_argv_add(&a, "--connect-timeout");
	kb_argv_add(&a, "30");
	kb_argv_add(&a, "-o");
	kb_argv_add(&a, out);
	kb_argv_add(&a, "--");
	kb_argv_add(&a, url);
	kb_argv_end(&a);
	int rc = kb_run_tty(&a);
	if (rc == 0)
		return GOT_OK;
	unlink(out);
	return rc == 5 || rc == 6 || rc == 7 || rc == 28 ? GOT_OFFLINE
							   : GOT_FAIL;
}

/*
 * `staged` and `live` change places in one rename(2) when the filesystem can
 * exchange them, so there is no instant at which /ports is missing. Where it
 * cannot, the live tree is moved aside first and put back if the second
 * rename fails. Either way `staged` ends up holding the old tree.
 *
 * A live tree that cannot be renamed at all (EXDEV: the live ISO's /ports is
 * a directory of the overlay's read-only lower layer, which overlayfs moves
 * only with redirect_dir) is replaced entry by entry instead: each old entry
 * removed, each new one renamed in. That is not atomic, and `staged` ends up
 * empty.
 */
static int swap_contents(const char *staged, const char *live)
{
	int n = 0;
	char **old = kb_listdir(live, &n);
	for (int i = 0; old && i < n; i++) {
		char *p = kb_path_join(live, old[i]);
		kb_rmtree(p);
		free(p);
	}
	kb_strv_free(old);
	char **neu = kb_listdir(staged, &n);
	if (!neu)
		return -1;
	int rc = 0;
	for (int i = 0; i < n && rc == 0; i++) {
		char *from = kb_path_join(staged, neu[i]);
		char *to = kb_path_join(live, neu[i]);
		rc = rename(from, to);
		free(from);
		free(to);
	}
	kb_strv_free(neu);
	return rc;
}

static int swap_dirs(const char *staged, const char *live)
{
	if (!kb_is_dir(live))
		return rename(staged, live);
	if (syscall(SYS_renameat2, AT_FDCWD, staged, AT_FDCWD, live,
		    RENAME_EXCHANGE) == 0)
		return 0;
	if (errno == EXDEV)
		return swap_contents(staged, live);
	if (errno != EINVAL && errno != ENOSYS)
		return -1;

	char old[600];
	snprintf(old, sizeof(old), "%s.old", live);
	kb_rmtree(old);
	if (rename(live, old) != 0)
		return -1;
	if (rename(staged, live) != 0) {
		rename(old, live);
		return -1;
	}
	return rename(old, staged);
}

static int fetch_fail(const char *code, const char *stage, const char *fmt,
		      ...) __attribute__((format(printf, 3, 4)));
static int fetch_fail(const char *code, const char *stage, const char *fmt,
		      ...)
{
	char msg[256];
	va_list ap;
	va_start(ap, fmt);
	vsnprintf(msg, sizeof(msg), fmt, ap);
	va_end(ap);
	fprintf(stderr, "kdos update: %s\n", msg);
	state_set("fetch", code, msg);
	if (stage)
		kb_rmtree(stage);
	return 1;
}

/*
 * The newest signed tree, verified, in place of /ports. 0 replaced or already
 * current, 1 not replaced (the reason is in update.state), 2 not attempted.
 *
 * NOTHING UNVERIFIED IS EVER UNPACKED. The tarball and its signature are
 * downloaded beside the live tree, the signature is checked against
 * /etc/kdos/keys/tree — a ring of its own, so a key trusted for packages or
 * packs does not thereby vouch for recipes — and only then is the archive
 * opened. A tree is also refused when its TREE stamp is older than the
 * installed one's: replaying a genuine old tree would pin old versions, and
 * an older tree is what an A/B rollback is for.
 */
static int tree_fetch(const KpConf *c, const char *from, int quiet)
{
	if (geteuid() != 0) {
		fprintf(stderr, "kdos update fetch replaces the ports tree and "
				"needs root\n");
		return 2;
	}
	int lock = update_lock();
	if (lock < 0)
		return 2;

	char root[512], stage[600], base[1024];
	ports_root(c, root, sizeof(root));
	if (root[0] != '/' || !root[1]) {
		close(lock);
		fprintf(stderr, "kdos update: %s is not a ports tree to "
				"replace\n", root);
		return 2;
	}
	/* A tree inside a git checkout belongs to git: replacing it would
	 * discard whatever the checkout holds that no release does. */
	char gitp[600];
	snprintf(gitp, sizeof(gitp), "%s/../.git", root);
	if (kb_path_exists(gitp)) {
		close(lock);
		fprintf(stderr, "kdos update: %s is part of a git checkout — "
				"update it with git\n", root);
		return 2;
	}

	snprintf(stage, sizeof(stage), "%s.new", root);
	kb_rmtree(stage);
	if (kb_mkdir_p(stage) != 0) {
		close(lock);
		return fetch_fail("fetch-failed", NULL, "cannot create %s: %s",
				  stage, strerror(errno));
	}
	tree_base(from, base, sizeof(base));

	char url[1200], tar[700], sig[720];
	snprintf(tar, sizeof(tar), "%s/%s", stage, TREE_FILE);
	snprintf(sig, sizeof(sig), "%s.sig", tar);

	if (!quiet)
		printf("%s==>%s fetching the ports tree from %s\n", A, O, base);
	/* The signature first: it is small, and the cheapest way to learn the
	 * network is down. A signature byte for byte the one the installed tree
	 * came with is the same tree, so the nightly run downloads a few hundred
	 * bytes and not the whole tree when nothing was released. */
	snprintf(url, sizeof(url), "%s/%s.sig", base, TREE_FILE);
	int got = download(url, sig, quiet);
	char ic[64];
	long long it;
	if (got == GOT_OK && same_as_last_sig(sig) &&
	    tree_read(root, ic, sizeof(ic), &it) == 0) {
		kb_rmtree(stage);
		close(lock);
		state_set("fetch", "ok", "tree current");
		if (!quiet)
			printf("the ports tree is current (%s)\n", ic);
		return 0;
	}
	if (got == GOT_OK) {
		snprintf(url, sizeof(url), "%s/%s", base, TREE_FILE);
		got = download(url, tar, quiet);
	}
	if (got != GOT_OK) {
		close(lock);
		return got == GOT_OFFLINE
			? fetch_fail("no-network", stage, "the network could "
				     "not be reached (%s)", base)
			: fetch_fail("fetch-failed", stage, "%s/%s could not be "
				     "downloaded", base, TREE_FILE);
	}

	const char *kd = getenv("KDOS_TREE_KEYS");
	KsigRing ring;
	char who[KSIG_ID_HEX];
	if (ksig_ring_load(&ring, kd && *kd ? kd : TREE_KEYS) <= 0) {
		close(lock);
		return fetch_fail("fetch-failed", stage, "no trusted tree key "
				  "in %s", kd && *kd ? kd : TREE_KEYS);
	}
	if (ksig_verify_path(&ring, tar, sig, who) != 0) {
		close(lock);
		return fetch_fail("fetch-failed", stage, "the tree's signature "
				  "does not verify against %s",
				  kd && *kd ? kd : TREE_KEYS);
	}

	char tree[620];
	snprintf(tree, sizeof(tree), "%s/tree", stage);
	kb_mkdir_p(tree);
	KbArgv x = {0};
	kb_argv_add(&x, "tar");
	kb_argv_add(&x, "-xJf");
	kb_argv_add(&x, tar);
	kb_argv_add(&x, "-C");
	kb_argv_add(&x, tree);
	kb_argv_end(&x);
	if (kb_run_tty(&x) != 0) {
		close(lock);
		return fetch_fail("fetch-failed", stage, "the signed tree did "
				  "not unpack");
	}

	char ncommit[64], ocommit[64];
	long long nt, ot;
	char *core = kb_path_join(tree, "core");
	char *idx = kb_path_join(tree, "sources.idx");
	int whole = kb_is_dir(core) && kb_path_exists(idx);
	free(core);
	free(idx);
	if (tree_read(tree, ncommit, sizeof(ncommit), &nt) != 0 || !whole) {
		close(lock);
		return fetch_fail("fetch-failed", stage, "the signed archive "
				  "is not a ports tree (no TREE, core/ or "
				  "sources.idx)");
	}
	if (tree_read(root, ocommit, sizeof(ocommit), &ot) == 0) {
		if (same_commit(ncommit, ocommit)) {
			keep_sig(sig);
			kb_rmtree(stage);
			close(lock);
			state_set("fetch", "ok", "tree current");
			if (!quiet)
				printf("the ports tree is current (%s)\n",
				       ocommit);
			return 0;
		}
		if (nt < ot) {
			close(lock);
			return fetch_fail("fetch-failed", stage, "the published "
					  "tree %.12s is older than this "
					  "machine's %.12s", ncommit, ocommit);
		}
	}

	if (swap_dirs(tree, root) != 0) {
		close(lock);
		return fetch_fail("fetch-failed", stage, "%s could not be "
				  "replaced: %s", root, strerror(errno));
	}
	keep_sig(sig);
	kb_rmtree(stage);
	close(lock);

	char msg[128];
	snprintf(msg, sizeof(msg), "tree %.12s, signed by %s", ncommit, who);
	state_set("fetch", "ok", msg);
	if (!quiet)
		printf("%s now holds tree %s (signed by key %s)\n", root,
		       ncommit, who);
	return 0;
}

static int cmd_fetch(const KpConf *c, int argc, char **argv)
{
	const char *from = NULL;
	for (int i = 0; i < argc; i++) {
		if (!strcmp(argv[i], "--from") && i + 1 < argc)
			from = argv[++i];
		else {
			fprintf(stderr, "usage: kdos update fetch [--from "
					"URL|DIR]\n");
			return 2;
		}
	}
	int rc = tree_fetch(c, from, 0);
	if (rc != 2)
		badge_refresh();
	return rc;
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

/*
 * The tree that built the slot goes into the slot, so the system that boots
 * from it holds the recipes its packages were built from and its next
 * `kdos update check` compares against them. Copied beside the slot's own tree
 * and exchanged with it, as `fetch` does.
 */
static int tree_into_slot(const char *root, const char *mnt)
{
	char dst[700], stage[720];
	snprintf(dst, sizeof(dst), "%s%s", mnt, root);
	snprintf(stage, sizeof(stage), "%s.new", dst);
	kb_rmtree(stage);

	KbArgv a = {0};
	kb_argv_add(&a, "cp");
	kb_argv_add(&a, "-a");
	kb_argv_add(&a, "--");
	kb_argv_add(&a, root);
	kb_argv_add(&a, stage);
	kb_argv_end(&a);
	if (kb_run_tty(&a) != 0 || swap_dirs(stage, dst) != 0) {
		kb_rmtree(stage);
		return -1;
	}
	kb_rmtree(stage);
	return 0;
}

/* ── apply ─────────────────────────────────────────────────────────────── */

static int run_kpkg(const char *verb, const char *pkg)
{
	KbArgv a = {0};
	kb_argv_add(&a, "kpkg");
	kb_argv_add(&a, verb);
	if (!strcmp(verb, "install"))
		kb_argv_add(&a, "-f");
	kb_argv_add(&a, pkg);
	kb_argv_end(&a);
	return kb_run_tty(&a);
}

/* `a, b, c` and `…` past what fits, for a state line a badge shows. */
static void name_list(char *out, size_t cap, const char *const *names, int n)
{
	size_t at = 0;
	out[0] = 0;
	for (int i = 0; i < n; i++) {
		size_t need = strlen(names[i]) + 2;
		if (at + need + 4 >= cap) {
			snprintf(out + at, cap - at, "%s…", i ? ", " : "");
			return;
		}
		at += (size_t)snprintf(out + at, cap - at, "%s%s",
				       i ? ", " : "", names[i]);
	}
}

static int cmd_apply(const KpConf *c, int argc, char **argv)
{
	int dry = 0, in_place = 0;
	const char *root = NULL;

	for (int i = 0; i < argc; i++) {
		if (!strcmp(argv[i], "--dry-run"))
			dry = 1;
		else if (!strcmp(argv[i], "--in-place"))
			in_place = 1;
		else if (!strcmp(argv[i], "--root") && i + 1 < argc)
			root = argv[++i];
		else {
			fprintf(stderr, "usage: kdos update apply [--dry-run] "
					"[--root DIR] [--in-place]\n");
			return 2;
		}
	}

	if (!have_ports(c)) {
		fprintf(stderr, "kdos update: %s\n", NO_TREE_TEXT);
		return 2;
	}
	if (!dry && geteuid() != 0) {
		fprintf(stderr, "kdos update apply installs packages and needs "
				"root (--dry-run does not)\n");
		return 2;
	}
	int lock = dry ? -1 : update_lock();
	if (!dry && lock < 0)
		return 2;

	char tree[512];
	ports_root(c, tree, sizeof(tree));

	/*
	 * A/B: an inactive slot exists to receive an update WITHOUT touching
	 * the system you are running, so installing in place on a machine that
	 * has one throws away the rollback it was set up for. It is refused
	 * rather than silently done, and --in-place is the way to say you meant
	 * it. The slot is marked as the try slot only after everything
	 * installed and checked: a candidate that got half an update is not a
	 * candidate.
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
			if (lock >= 0)
				close(lock);
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
		if (n == 0 && !dry)
			state_set("apply", "ok", "nothing behind");
		free(v);
		free(mnt);
		if (lock >= 0)
			close(lock);
		return n < 0 ? 2 : 0;
	}

	if (dry) {
		for (int i = 0; i < n; i++)
			printf("  %-24s %-14s → %s: would fetch its sources "
			       "and build it\n", v[i].name, v[i].have,
			       v[i].want);
		free(v);
		free(mnt);
		return 0;
	}

	/*
	 * EVERY SOURCE BEFORE ANY BUILD. A machine that is offline, or a source
	 * that has gone from both the archive and upstream, is known in minutes
	 * — not after hours of compiling the packages that happened to sort
	 * first, with a slot left half updated.
	 */
	const char **bad = kb_calloc((size_t)n, sizeof(*bad));
	int nbad = 0, offline = 0;
	char list[200];
	printf("\n%s==>%s fetching the sources of %d package%s\n", A, O, n,
	       n == 1 ? "" : "s");
	for (int i = 0; i < n && !offline; i++) {
		int rc = run_kpkg("fetch", v[i].name);
		if (rc == 3)
			offline = 1;
		else if (rc != 0)
			bad[nbad++] = v[i].name;
	}
	if (offline || nbad) {
		name_list(list, sizeof(list), bad, nbad);
		/* On the APPLY line: the tree's own fetch may well have
		 * worked, and the next one would overwrite this. */
		if (offline)
			state_set("apply", "no-network", "the network could not "
				  "be reached for the sources");
		else
			state_set("apply", "fetch-failed", list);
		badge_refresh();
		fprintf(stderr, "\nkdos update: %s — nothing was built\n",
			offline ? "the network could not be reached"
				: "these sources could not be fetched");
		if (nbad)
			fprintf(stderr, "             %s\n", list);
		free(bad);
		free(v);
		free(mnt);
		close(lock);
		return 1;
	}

	/* A dependency this tree adds is built by kpkg install on the way, and
	 * KPKG_FETCH=1 has kpkg fetch ITS sources as it reaches it. */
	setenv("KPKG_FETCH", "1", 1);

	int done = 0;
	for (int i = 0; i < n; i++) {
		printf("\n%s==>%s %s %s → %s\n", A, O, v[i].name, v[i].have,
		       v[i].want);
		if (run_kpkg("install", v[i].name) == 0)
			done++;
		else
			bad[nbad++] = v[i].name;
	}
	printf("\n%d of %d built and installed, %d failed\n", done, n, nbad);

	char why[256] = "";
	if (nbad) {
		name_list(list, sizeof(list), bad, nbad);
		snprintf(why, sizeof(why), "%d of %d failed: %s", nbad, n,
			 list);
	}

	/*
	 * THE SLOT IS CHECKED BEFORE IT IS TRIED: `kpkg check` compares every
	 * file the slot's database records with what is on its disk, and a
	 * root with a missing file, or a changed one outside etc/, is not a
	 * candidate, whatever every install reported. Edited configuration
	 * under etc/ is listed and passes: failing on it would refuse every
	 * configured machine.
	 */
	if (ab && !nbad) {
		KbArgv k = {0};
		kb_argv_add(&k, "kpkg");
		kb_argv_add(&k, "check");
		kb_argv_add(&k, "--all");
		kb_argv_add(&k, "--root");
		kb_argv_add(&k, mnt);
		kb_argv_end(&k);
		if (kb_run_tty(&k) != 0)
			snprintf(why, sizeof(why), "slot %s failed `kpkg check`",
				 slot);
	}
	if (ab && !why[0] && tree_into_slot(tree, mnt) != 0)
		snprintf(why, sizeof(why), "the ports tree could not be copied "
			 "into slot %s", slot);

	/*
	 * THE SLOT'S KERNEL GOES ONTO THE ESP BEFORE THE SLOT IS TRIED, every
	 * time and whether or not `linux` was in this run. Each slot boots the
	 * kernel in its own ESP directory, and the modules for it exist only
	 * in that root; a slot whose directory was never filled, or holds a
	 * kernel older than its /lib/modules, boots with no modular driver.
	 * A deploy that fails leaves the slot untried.
	 */
	if (ab && !why[0]) {
		KbArgv d = {0};
		kb_argv_add(&d, "kdos-bootctl");
		kb_argv_add(&d, "deploy");
		kb_argv_add(&d, mnt);
		kb_argv_add(&d, slot);
		kb_argv_end(&d);
		if (kb_run_tty(&d) != 0)
			snprintf(why, sizeof(why), "slot %s's kernel could not "
				 "be put on the ESP", slot);
	}

	if (ab && !why[0]) {
		KbArgv t = {0};
		kb_argv_add(&t, "kdos-bootctl");
		kb_argv_add(&t, "try");
		kb_argv_add(&t, slot);
		kb_argv_end(&t);
		if (kb_run_tty(&t) == 0)
			printf("slot %s is the candidate: a boot that does not "
			       "confirm it rolls back\n", slot);
		else
			snprintf(why, sizeof(why), "slot %s could not be marked "
				 "as the candidate", slot);
	}

	if (why[0]) {
		state_set("apply", "build-failed", why);
		fprintf(stderr, "kdos update: %s%s\n", why,
			ab ? " — the slot will not be booted" : "");
	} else {
		char ok[64];
		snprintf(ok, sizeof(ok), "%d package%s", done,
			 done == 1 ? "" : "s");
		state_set("apply", "ok", ok);
	}

	free(bad);
	free(v);
	free(mnt);
	close(lock);
	badge_refresh();
	return why[0] ? 1 : 0;
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

	if (!strcmp(what, "check")) {
		int json = 0, fetch = 0;
		const char *out = NULL;

		for (int i = 0; i < rest; i++) {
			if (!strcmp(restv[i], "--json"))
				json = 1;
			else if (!strcmp(restv[i], "--fetch"))
				fetch = 1;
			else if (!strcmp(restv[i], "--out") && i + 1 < rest)
				out = restv[++i];
		}
		/* --fetch first, so the survey reads the tree it fetched. Its
		 * failure is recorded and reported as the problem; the check
		 * still answers against the tree already here. */
		if (fetch) {
			tree_fetch(&c, NULL, json || out);
			kp_conf_load(&c);
		}
		/* `--out` IMPLIES `--json`: there is no other document to
		 * write, and a file of coloured, wrapped human text is not
		 * something anything reads back. */
		if (out)
			return check_json_to(&c, out);
		if (json)
			return check_json(&c, stdout);
		return cmd_check_human(&c);
	}
	if (!strcmp(what, "fetch"))
		return cmd_fetch(&c, rest, restv);
	if (!strcmp(what, "apply"))
		return cmd_apply(&c, rest, restv);

	fprintf(stderr, "usage: kdos update {check|fetch|apply|theme}\n"
			"  check   what the ports tree pins that is not "
			"installed, and what went wrong;\n"
			"          --json for a surface, --out PATH to write "
			"that document atomically,\n"
			"          --fetch to fetch the newest tree first\n"
			"  fetch   replace the ports tree with the newest "
			"signed one; --from URL|DIR\n"
			"  apply   build what is behind from source, fetching "
			"its sources; A/B aware\n"
			"  theme   re-run the theme generators for $HOME after "
			"an art upgrade\n");
	return 2;
}
