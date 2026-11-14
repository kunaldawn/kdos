/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   kdos update — a newer recipe tree, and the packages it pins, into a slot
 *
 *   kdos update check     installed packages older than their recipe, which of
 *                         them the signed binhost holds, whether a newer
 *                         signed slot image exists, and what is offered
 *   kdos update fetch     replace /ports with the newest signed recipe tree,
 *                         and fetch the signed binhost index and slot image
 *                         manifest that go with it
 *   kdos update apply     write the newer signed slot image into the inactive
 *                         slot, or install what is behind there from the
 *                         binhost; --source builds it here instead. Either
 *                         way the world set follows, each package by the
 *                         route its origin records
 *   kdos update install   one port and what it lacks into the running root:
 *                         prebuilt from the binhost, or built here with its
 *                         missing dependencies prebuilt or built too
 *   kdos update theme     re-apply the accent to $HOME after an update
 *
 * ── What "an update" means here ──
 *
 * A recipe pins its version, so what is newer arrives with the PORTS TREE and
 * nowhere else. Every image carries the recipe-only tree at /ports — each
 * `kpkgbuild`, `build.sh` and patch, `shelves`, `sources.idx`, our own
 * programs under `src/`, the two books under `docs/` and their renderer
 * under `script/book`, and no source tarball. So an update is two steps:
 *
 *   fetch   a newer tree arrives as one signed tarball, is verified against
 *           /etc/kdos/keys/tree, and replaces /ports whole; the release's
 *           signed RELEASE INDEX (kdos-update.idx, relidx.c) says where each
 *           file of its binhost and slot image is, and through it the signed
 *           binhost index (PACKAGES, PACKAGES.sig) is fetched into
 *           /var/lib/kdos/binhost and the slot image manifest into
 *           /var/lib/kdos/image/offer, each kept only when the index's hash
 *           matches and its own signature verifies against /etc/kdos/keys
 *   apply   the inactive A/B slot receives the newer release: as the signed
 *           SLOT IMAGE of it (image.c) when one is published, else as the
 *           PREBUILT packages the binhost index names for every package whose
 *           recipe now pins something newer — the two lanes a GUI starts —
 *           or, with --source, BUILT ON THIS MACHINE from its sources
 *
 * THE IMAGE LANE COMES FIRST. A slot image is the release as it was built and
 * tested, written whole; its delta from the running version is what a
 * machine downloads, and every file is checked against the signed manifest.
 * The binhost lane is the way when no image newer than the running system is
 * published, and --image, --binhost and --source pick one by name.
 *
 * A PREBUILT PACKAGE IS USED ONLY WHEN THE WHOLE SET IS PREBUILT. kpkg's three
 * equality tests (architecture, build-config hash, recipe hash) decide each
 * package; one that fails them refuses the whole binhost apply before the
 * slot is written, with "N packages need a source build", because a slot half
 * from the binhost and half from a compiler is a slot nobody tested. `check`
 * runs the same tests against the cached index, so what it OFFERS is exactly
 * what `apply` will take: kdos-sysupdated reads that offer as the version
 * Discover shows.
 *
 * THE ACQUIRE AND THE ARM ARE TWO STEPS, because Discover shows progress for
 * the first and not the second. `apply --acquire` does all the long work —
 * download, write or install into the slot, `kpkg check` — and records the
 * slot as acquired; `apply --arm` carries this machine's configuration into
 * it (kdt_image_carry), copies the tree into it unless an image brought its
 * own, records its version, puts its kernel on the ESP and asks kdos-bootctl
 * to try it. `apply` with neither does both, which is what a person types.
 *
 * EVERY DOWNLOAD IS NAMED BY THE RELEASE INDEX. A published binhost and slot
 * image are flat assets spread over releases of at most 1,000 assets each,
 * so no file's URL is spelled from its name: each is looked up in the
 * verified index, and kept only with the size and hash the index gives. An
 * index that is unsigned, signed by no trusted key or for another release is
 * refused, and so is everything it would have named.
 *
 * WHAT CAN GO WRONG IS RECORDED, so a surface can say it: no tree on the
 * machine, no network, a fetch that failed (a download, a signature, a tree
 * older than the one installed), a release index, binhost index or image
 * manifest that does not verify, a build, an install or an image write that
 * failed. `fetch` and `apply` write their outcome to
 * /var/lib/kdos/update.state and `check` reports it beside the list of what
 * is behind.
 * ---------------------------------
 */

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <sys/syscall.h>

#include "kdos-tools.h"
#include "kpkg.h"
#include "ksig.h"

#ifndef RENAME_EXCHANGE
#define RENAME_EXCHANGE (1 << 1)
#endif

#define UPDATE_CONF  "/etc/kdos/update.conf"
#define UPDATE_STATE "/var/lib/kdos/update.state"
#define UPDATE_DOC   "/var/lib/kdos/update.json"
#define TREE_KEYS    "/etc/kdos/keys/tree"
/* The repository's latest release is always a KDOS system release — the
 * source archive's volumes are pre-releases and never "latest" — so this URL
 * follows whatever was published last. */
#define TREE_BASE    "https://github.com/kunaldawn/kdos/releases/latest/download"
#define TREE_FILE    "kdos-ports.tar.xz"
#define TREE_MARK    "kdos-tree 1"
/* The download root of the repository's releases. A published tree's
 * release `<version>` carries its release index, kdos-update.idx, which
 * `ports/publish --release` writes; the binhost's files themselves are in
 * the releases `binhost-<version>-NN` the index names. */
#define BINHOST_BASE "https://github.com/kunaldawn/kdos/releases/download"
#define BINHOST_CACHE "/var/lib/kdos/binhost"
#define BINHOST_INDEX "PACKAGES"
#define BINHOST_SIG   "PACKAGES.sig"
#define BUILD_ENV    "/etc/kdos/chroot.env"
#define WORLD_FILE   "/var/lib/kdos/world"
#define RELEASE_FILE "/etc/kdos-release"
/* The slot image manifest last fetched and verified, and where it came
 * from; a root's own image record is IMAGE_RECORD. */
#define IMAGE_CACHE  "/var/lib/kdos/image/offer"
#define IMAGE_RECORD "/var/lib/kdos/image/MANIFEST"
/* Where `--mount` puts an inactive slot nobody mounted. */
#define SLOT_MNT     "/run/kdos/slot-"

/* The exit statuses of `apply`, which kdos-sysupdated reports as a job's. */
enum { AP_OK = 0, AP_FAIL = 1, AP_USAGE = 2, AP_REFUSED = 3, AP_CANCELLED = 4 };

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

/* kdos-bootctl's reading of a root's identity (bootctl.c), so the version a
 * slot is recorded under and the version this file compares are one rule. */
void bootctl_root_version(const char *root, char *out, size_t n);

/* ── where things are ──────────────────────────────────────────────────── */

/* $KDOS_UPDATE_STATE beats the default, so a test run records its outcome
 * away from the machine's own and leaves the update document alone
 * (doc_refresh). The lock and the last tree's signature live beside it. */
static const char *state_path(void)
{
	const char *e = getenv("KDOS_UPDATE_STATE");
	return e && *e ? e : UPDATE_STATE;
}

/* The binhost cache: the verified index, the packages downloaded for an
 * apply, and the keyring copy a slot's kpkg verifies them with.
 * $KDOS_UPDATE_CACHE moves it for a test. */
static const char *cache_dir(void)
{
	const char *e = getenv("KDOS_UPDATE_CACHE");
	return e && *e ? e : BINHOST_CACHE;
}

/* The verified image manifest and its origin. Beside the binhost cache under
 * $KDOS_UPDATE_CACHE, for a test. */
static void image_dir(char *out, size_t cap)
{
	const char *e = getenv("KDOS_UPDATE_CACHE");
	if (e && *e)
		snprintf(out, cap, "%s/image", e);
	else
		kb_strlcpy(out, IMAGE_CACHE, cap);
}

/* The host keyring kpkg trusts packages from, under kpkg's own variable so
 * the index this file verifies and the one `kpkg binhost` verifies are
 * verified against one ring. */
static const char *keyring_dir(void)
{
	const char *e = getenv("KPKG_KEYRING");
	return e && *e ? e : "/etc/kdos/keys";
}

/* THE KEY BEFORE THE NETWORK, for the release index as for the tree: a
 * ring with no key can verify no index, so a download would end only in
 * `unverified` where the truth is that this machine trusts no release. */
static int host_keyed(void)
{
	KsigRing ring;
	return ksig_ring_load(&ring, keyring_dir()) > 0;
}

/* The running root, for the files that say what it is: kdos-release,
 * os-release, the build environment record and the world set.
 * $KDOS_UPDATE_SYSROOT moves all four for a test. */
static const char *sys_root(void)
{
	const char *e = getenv("KDOS_UPDATE_SYSROOT");
	return e && *e ? e : "/";
}

static void sys_path(const char *rel, char *out, size_t cap)
{
	const char *r = sys_root();
	snprintf(out, cap, "%s%s", strcmp(r, "/") ? r : "", rel);
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

/* A place to fetch from, spelled as a URL: a value with no scheme is a
 * DIRECTORY — a stick, an NFS mount — and is read through curl as a file://
 * URL, so both cases are one code path and one signature check. */
static void as_url(const char *v, char *out, size_t cap)
{
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
}

/*
 * Where a newer tree is fetched from: `--from`, then $KDOS_UPDATE_TREE, then
 * `tree =` in update.conf, then the latest KDOS release.
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
	as_url(v, out, cap);
	free(conf);
}

/* ── the tree's identity ───────────────────────────────────────────────── */

/*
 * `<tree>/TREE`, written by whatever made the tree:
 *
 *   kdos-tree 1
 *   commit <hex>
 *   time <unix seconds of that commit>
 *   version <release>          (a published release's tree only)
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

/* A release name is the characters a version string is made of and nothing
 * else: it becomes part of a download URL and an AppStream attribute. */
static int version_ok(const char *v)
{
	size_t n = strlen(v);
	if (!n || n > 40)
		return 0;
	for (const char *c = v; *c; c++)
		if (!((*c >= '0' && *c <= '9') || (*c >= 'a' && *c <= 'z') ||
		      (*c >= 'A' && *c <= 'Z') || *c == '.' || *c == '+' ||
		      *c == '-' || *c == '_' || *c == '~'))
			return 0;
	return 1;
}

/*
 * The version a tree is offered under: its TREE `version` line, which a
 * published release's tree carries, else the first twelve characters of its
 * commit. `named` says which; only a named release has a binhost to find by
 * name. "" when the tree has no stamp at all.
 */
static void tree_version(const char *dir, char *out, size_t cap, int *named)
{
	char commit[64];
	long long t;

	out[0] = 0;
	if (named)
		*named = 0;
	if (tree_read(dir, commit, sizeof(commit), &t) != 0)
		return;
	char *p = kb_path_join(dir, "TREE");
	char *data = kb_read_all(p, NULL);
	free(p);
	for (char *l = data; l && *l;) {
		char *nl = strchr(l, '\n');
		if (nl)
			*nl = 0;
		if (!strncmp(l, "version ", 8) && version_ok(l + 8)) {
			kb_strlcpy(out, l + 8, cap);
			if (named)
				*named = 1;
			break;
		}
		l = nl ? nl + 1 : NULL;
	}
	free(data);
	if (!out[0])
		snprintf(out, cap, "%.12s", commit);
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
 * update.state, one line per step at most:
 *
 *   fetch <code> <unix time> <detail>
 *   binhost <code> <unix time> <detail>
 *   apply <code> <unix time> <detail>
 *
 * `fetch` codes: `ok`, `no-key` (refused before any download because
 * /etc/kdos/keys/tree holds no key), `no-network`, `fetch-failed`.
 * `binhost` codes: `ok`, `absent` (no binhost is published for this tree —
 * not a problem: `apply --source` is the way), `no-key` (refused before any
 * download because /etc/kdos/keys holds no key that could verify a release
 * index), `no-network`, `fetch-failed`, `unverified` (a release index or
 * binhost index whose signature no key in /etc/kdos/keys verifies, a release
 * index for another release, or a file other than the one the release index
 * names).
 * `image` codes: `ok`, `absent` (no slot image is published there),
 * `no-key` (as for `binhost`), `no-network`, `fetch-failed`, `unverified`.
 * `apply` codes: `ok`, `acquired` (a slot filled and checked, detail
 * `slot <x> <version>`, waiting for `--arm`), `cancelled`, `needs-source`,
 * `unverified`, `no-network`, `fetch-failed`, `build-failed`.
 *
 * Each step replaces only its own line, so a fetch tonight does not erase
 * the build that failed yesterday. kdos-sysupdated reads the `fetch` line's
 * time to refresh at most hourly.
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
 * that is reading it, nor an index under an install that is using it. */
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

/* ── progress, for kdos-sysupdated ─────────────────────────────────────── */

/*
 * `--progress FD`: one line per event, for a program rather than a person.
 *
 *   refused <reason>               nothing was written; the reason is the
 *                                  sentence a person is shown
 *   verified <key-id> <version> <items> <bytes>
 *                                  the index verified and covers every item;
 *                                  for `--arm`, `verified - <version> 0 0`:
 *                                  the slot holds that version's acquire
 *   item <done> <total> <name>     one more step of the long work
 *   error <reason>                 the run failed; the slot stays untried
 *   acquired <slot> <version>      the slot is filled and checked
 *   armed <slot> <version>         the slot is the next boot's candidate
 *
 * The descriptor is close-on-exec, so no package build holds the pipe open
 * after this process has finished. A reader that went away costs nothing:
 * SIGPIPE is ignored while the descriptor is in use.
 */
static int prog_fd = -1;

static void prog(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
static void prog(const char *fmt, ...)
{
	char line[1024];
	va_list ap;
	int n;

	if (prog_fd < 0)
		return;
	va_start(ap, fmt);
	n = vsnprintf(line, sizeof(line) - 1, fmt, ap);
	va_end(ap);
	if (n < 0)
		return;
	if ((size_t)n > sizeof(line) - 2)
		n = (int)sizeof(line) - 2;
	for (int i = 0; i < n; i++)
		if (line[i] == '\n' || line[i] == '\r')
			line[i] = ' ';
	line[n++] = '\n';
	for (int off = 0; off < n;) {
		ssize_t w = write(prog_fd, line + off, (size_t)(n - off));
		if (w < 0 && errno == EINTR)
			continue;
		if (w <= 0)
			break;
		off += (int)w;
	}
}

static int prog_open(const char *arg)
{
	char *end;
	long fd = strtol(arg, &end, 10);
	if (*end || fd < 3 || fd > 1024 || fcntl((int)fd, F_GETFD) < 0)
		return -1;
	fcntl((int)fd, F_SETFD, FD_CLOEXEC);
	signal(SIGPIPE, SIG_IGN);
	prog_fd = (int)fd;
	return 0;
}

/*
 * SIGTERM STOPS AN APPLY BETWEEN PACKAGES. kdos-sysupdated's Job.Cancel sends
 * it to this process alone, so the package being installed finishes — a
 * package interrupted halfway is a root with half of it — and the next one
 * is never started. The slot stays untried either way.
 */
static volatile sig_atomic_t g_stop;

static void on_term(int sig)
{
	(void)sig;
	g_stop = 1;
}

static void stop_on_term(void)
{
	struct sigaction sa;
	memset(&sa, 0, sizeof(sa));
	sa.sa_handler = on_term;
	sigemptyset(&sa.sa_mask);
	sigaction(SIGTERM, &sa, NULL);
}

/* ── what is behind ────────────────────────────────────────────────────── */

typedef struct {
	char name[128];
	char have[64], have_rel[32];
	char want[64], want_rel[32];
	int stanza;		/* the binhost index entry that covers it, or -1 */
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
		v[n].stanza = -1;
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

/* A package name, as kpkg spells one: it is joined to paths and argv. */
static int pkg_name_ok(const char *s)
{
	if (!*s || strlen(s) >= 128 || *s == '.' || *s == '-')
		return 0;
	for (const char *c = s; *c; c++)
		if (!((*c >= 'a' && *c <= 'z') || (*c >= '0' && *c <= '9') ||
		      (*c >= 'A' && *c <= 'Z') || *c == '+' || *c == '.' ||
		      *c == '_' || *c == '-'))
			return 0;
	return 1;
}

/*
 * THE WORLD SET: the packages somebody installed on purpose, one name per
 * line in /var/lib/kdos/world. A port installed into the running slot is in
 * that slot's database and no other, so an update computed from the inactive
 * slot's database alone would leave it out and the next boot would lose it;
 * every binhost apply installs each world package the slot lacks. NULL with
 * *n = 0 when the file is absent, which is a machine nobody added to.
 */
static char **world_load(int *n)
{
	char p[1024];
	char *data;
	char **v = NULL;
	int cap = 0;

	*n = 0;
	sys_path(WORLD_FILE, p, sizeof(p));
	if (!(data = kb_read_all(p, NULL)))
		return NULL;
	for (char *l = data; l && *l;) {
		char *nl = strchr(l, '\n');
		if (nl)
			*nl = 0;
		while (*l == ' ' || *l == '\t')
			l++;
		l[strcspn(l, " \t\r#")] = 0;
		if (*l && pkg_name_ok(l)) {
			if (*n + 1 >= cap) {
				cap = cap ? cap * 2 : 32;
				v = kb_realloc(v, (size_t)cap * sizeof(*v));
			}
			v[(*n)++] = kb_strdup(l);
			v[*n] = NULL;
		}
		l = nl ? nl + 1 : NULL;
	}
	free(data);
	return v;
}

/*
 * HOW EACH PACKAGE ARRIVED: one `<package> prebuilt|source` line per package
 * an install brought, in /var/lib/kdos/origin. An update keeps the route: a
 * package built here is built here again, one taken from the binhost is
 * taken from the next release's binhost. A package with no line is
 * `prebuilt`, which is how every package of the image arrived. The file is
 * machine state under /var/lib/kdos, carried into a slot at the arm with the
 * world set (image.c).
 */
#define ORIGIN_FILE "/var/lib/kdos/origin"
enum { ROUTE_PREBUILT = 0, ROUTE_SOURCE = 1 };

static const char *route_name(int route)
{
	return route == ROUTE_SOURCE ? "source" : "prebuilt";
}

static int route_parse(const char *s)
{
	if (!strcmp(s, "prebuilt"))
		return ROUTE_PREBUILT;
	if (!strcmp(s, "source"))
		return ROUTE_SOURCE;
	return -1;
}

static int origin_of(const char *pkg)
{
	char p[1024];
	sys_path(ORIGIN_FILE, p, sizeof(p));
	char *data = kb_read_all(p, NULL);
	int route = ROUTE_PREBUILT;
	size_t pl = strlen(pkg);
	for (char *l = data; l && *l;) {
		char *nl = strchr(l, '\n');
		if (nl)
			*nl = 0;
		if (!strncmp(l, pkg, pl) && l[pl] == ' ' &&
		    route_parse(l + pl + 1) == ROUTE_SOURCE)
			route = ROUTE_SOURCE;
		l = nl ? nl + 1 : NULL;
	}
	free(data);
	return route;
}

/* `pkg`'s line set to `route`, the file written whole: a file cut short by a
 * crash would turn every package after the cut back into `prebuilt`. */
static int origin_set(const char *pkg, int route)
{
	char p[1024];
	sys_path(ORIGIN_FILE, p, sizeof(p));
	char *data = kb_read_all(p, NULL);
	size_t pl = strlen(pkg);
	KbBuf b = {0};
	for (char *l = data; l && *l;) {
		char *nl = strchr(l, '\n');
		if (nl)
			*nl = 0;
		if (*l && !(!strncmp(l, pkg, pl) && l[pl] == ' '))
			kb_buf_printf(&b, "%s\n", l);
		l = nl ? nl + 1 : NULL;
	}
	kb_buf_printf(&b, "%s %s\n", pkg, route_name(route));
	char *dir = kb_strdup(p);
	char *sl = strrchr(dir, '/');
	if (sl)
		*sl = 0;
	int rc = kb_mkdir_p(dir) == 0 ? kb_write_file_atomic(p, b.p) : -1;
	free(dir);
	free(data);
	kb_buf_free(&b);
	return rc;
}

/* ── the build environment a binhost is matched under ──────────────────── */

/*
 * A PREBUILT PACKAGE MATCHES ONLY UNDER THE ENVIRONMENT THAT BUILT IT. kpkg's
 * build-config hash is taken from CC, CFLAGS, CXXFLAGS, LDFLAGS, KDOS_TARGET
 * and the compiler's version, so a client with an ordinary shell's
 * environment reports `different build config` for every package. The image
 * records the build's values in /etc/kdos/chroot.env, one `KEY=value` line
 * each, the value as it was after every expansion; this sets exactly those
 * six (unsetting any the file leaves out) in this process, which every kpkg it
 * runs inherits. -1 when the file is absent, and the environment is left as it
 * was.
 */
static const char *const BUILD_KEYS[] = {
	"CC", "CXX", "CFLAGS", "CXXFLAGS", "LDFLAGS", "KDOS_TARGET", NULL
};

static int build_env_load(const char *path)
{
	char *data = kb_read_all(path, NULL);
	int seen[8] = {0};

	if (!data)
		return -1;
	for (char *l = data; l && *l;) {
		char *nl = strchr(l, '\n');
		if (nl)
			*nl = 0;
		if (!strncmp(l, "export ", 7))
			l += 7;
		char *eq = strchr(l, '=');
		if (eq && *l != '#') {
			*eq = 0;
			char *v = eq + 1;
			size_t vl = strlen(v);
			while (vl && (v[vl - 1] == '\r' || v[vl - 1] == ' '))
				v[--vl] = 0;
			if (vl >= 2 && v[0] == '"' && v[vl - 1] == '"') {
				v[vl - 1] = 0;
				v++;
			}
			for (int k = 0; BUILD_KEYS[k]; k++)
				if (!strcmp(l, BUILD_KEYS[k])) {
					setenv(BUILD_KEYS[k], v, 1);
					seen[k] = 1;
				}
		}
		l = nl ? nl + 1 : NULL;
	}
	free(data);
	for (int k = 0; BUILD_KEYS[k]; k++)
		if (!seen[k])
			unsetenv(BUILD_KEYS[k]);
	return 0;
}

/* The record of `root` (a slot, or the running root when NULL), else the
 * running root's: a slot filled before the image wrote one is matched under
 * the environment of the system that is filling it. */
static int build_env_for(const char *root)
{
	char p[1024];
	if (root) {
		snprintf(p, sizeof(p), "%s%s", root, BUILD_ENV);
		if (build_env_load(p) == 0)
			return 0;
	}
	sys_path(BUILD_ENV, p, sizeof(p));
	return build_env_load(p);
}

/* ── the binhost index ─────────────────────────────────────────────────── */

typedef struct {
	char name[128];
	char version[64];
	char release[32];
	char arch[32];
	char file[256];
	char sha[65];
	char bhash[65];
	char ehash[65];
	char from[256];		/* set on a delta: the package it applies to */
	long long size;
} BhStanza;

typedef struct {
	BhStanza *st;
	int n;
	char who[KSIG_ID_HEX];	/* the key that verified it */
	char keyname[64];
	char origin[1024];	/* where it was fetched from */
	KdtRelIdx *rel;		/* the release index every file is found by */
} BhIndex;

static void bh_free(BhIndex *ix)
{
	free(ix->st);
	kdt_relidx_free(ix->rel);
	memset(ix, 0, sizeof(*ix));
}

/*
 * The index format is kpkg's (`kpkg index`, binhost.c): `K:value` lines, one
 * stanza per package, a blank line between. Read here for the fields a
 * client needs before any package is on the machine — which file, how big,
 * which hashes — and the same three equality tests are run again by `kpkg
 * binhost` at install time, which is the test that decides.
 */
static int bh_parse(const char *data, BhIndex *ix)
{
	BhStanza cur;
	int have = 0, cap = 0;

	memset(&cur, 0, sizeof(cur));
	for (const char *line = data;; ) {
		const char *nl = line ? strchr(line, '\n') : NULL;
		size_t len = !line ? 0 : nl ? (size_t)(nl - line) : strlen(line);
		char buf[1024];

		if (len >= sizeof(buf))
			len = sizeof(buf) - 1;
		if (line)
			memcpy(buf, line, len);
		buf[len] = 0;
		if (len && buf[len - 1] == '\r')
			buf[--len] = 0;

		if (!buf[0]) {
			if (have) {
				if (ix->n + 1 > cap) {
					cap = cap ? cap * 2 : 256;
					ix->st = kb_realloc(ix->st, (size_t)cap *
							    sizeof(*ix->st));
				}
				ix->st[ix->n++] = cur;
			}
			memset(&cur, 0, sizeof(cur));
			have = 0;
			if (!line || !*line || !nl)
				break;
			line = nl + 1;
			continue;
		}
		line = nl ? nl + 1 : NULL;
		if (buf[1] != ':')
			continue;
		const char *v = buf + 2;
		switch (buf[0]) {
		case 'P': kb_strlcpy(cur.name, v, sizeof(cur.name)); have = 1; break;
		case 'V': kb_strlcpy(cur.version, v, sizeof(cur.version)); break;
		case 'R': kb_strlcpy(cur.release, v, sizeof(cur.release)); break;
		case 'A': kb_strlcpy(cur.arch, v, sizeof(cur.arch)); break;
		case 'F': kb_strlcpy(cur.file, v, sizeof(cur.file)); break;
		case 'S': cur.size = atoll(v); break;
		case 'C': kb_strlcpy(cur.sha, v, sizeof(cur.sha)); break;
		case 'B': kb_strlcpy(cur.bhash, v, sizeof(cur.bhash)); break;
		case 'E': kb_strlcpy(cur.ehash, v, sizeof(cur.ehash)); break;
		case 'O': kb_strlcpy(cur.from, v, sizeof(cur.from)); break;
		default: break;
		}
	}
	return ix->n;
}

/* A file name from the index, before it is joined to a URL or a path: no
 * directory, no dot-file, nothing a shell or a URL would read twice. */
static int file_name_ok(const char *f)
{
	if (!*f || *f == '.' || strlen(f) >= 200)
		return 0;
	for (const char *c = f; *c; c++)
		if (!((*c >= 'a' && *c <= 'z') || (*c >= 'A' && *c <= 'Z') ||
		      (*c >= '0' && *c <= '9') || *c == '.' || *c == '-' ||
		      *c == '_' || *c == '+'))
			return 0;
	return 1;
}

/*
 * The cached index, VERIFIED against the host keyring before one line of it
 * is believed, and against the cached release index it was fetched by: the
 * PACKAGES in the cache must be the one that index names. -1 with the reason
 * in `why` when there is no index, no trusted key, a signature no trusted key
 * made, or a PACKAGES the release index does not name.
 */
static int bh_load(BhIndex *ix, char *why, size_t wcap)
{
	char ip[1024], sp[1024];
	size_t len = 0;
	KsigRing ring;

	memset(ix, 0, sizeof(*ix));
	why[0] = 0;
	snprintf(ip, sizeof(ip), "%s/" BINHOST_INDEX, cache_dir());
	snprintf(sp, sizeof(sp), "%s/" BINHOST_SIG, cache_dir());
	char *data = kb_read_all(ip, &len);
	if (!data) {
		snprintf(why, wcap, "no signed binhost index is on this machine");
		return -1;
	}
	if (ksig_ring_load(&ring, keyring_dir()) <= 0) {
		free(data);
		snprintf(why, wcap, "no trusted key in %s can verify the binhost "
			 "index", keyring_dir());
		return -1;
	}
	if (ksig_verify_file(&ring, sp, data, len, ix->who) != 0) {
		free(data);
		snprintf(why, wcap, "the binhost index's signature does not "
			 "verify against %s", keyring_dir());
		return -1;
	}
	for (int k = 0; k < ring.n; k++)
		if (!strcmp(ring.key[k].id, ix->who))
			kb_strlcpy(ix->keyname, ring.key[k].name,
				   sizeof(ix->keyname));
	free(data);
	ix->rel = kdt_relidx_load(cache_dir(), keyring_dir(), why, wcap);
	if (!ix->rel)
		return -1;
	if (!kdt_relidx_holds(ix->rel, "binhost/" BINHOST_INDEX, ip) ||
	    !kdt_relidx_holds(ix->rel, "binhost/" BINHOST_SIG, sp)) {
		kdt_relidx_free(ix->rel);
		ix->rel = NULL;
		snprintf(why, wcap, "the cached binhost index is not the one the "
			 "signed release index names");
		return -1;
	}
	data = kb_read_all(ip, NULL);
	if (data)
		bh_parse(data, ix);
	free(data);
	kb_strlcpy(ix->origin, kdt_relidx_origin(ix->rel), sizeof(ix->origin));
	return 0;
}

/*
 * The stanza that covers `name`, by kpkg's three tests: this machine's
 * architecture, the build-config hash of the environment build_env_for() set,
 * and the recipe hash of the port in the tree. -1, with which test failed in
 * `why`, otherwise.
 */
static int bh_match(const KpConf *c, const BhIndex *ix, const char *name,
		    char *why, size_t wcap)
{
	char arch[32], bhash[65], ehash[65] = "";
	const BhStanza *near = NULL;

	kp_arch(arch, sizeof(arch));
	kp_buildconfig_hash(bhash, NULL, 0);
	char *pd = kp_port_dir(c, name);
	if (pd) {
		kp_recipe_hash(pd, ehash);
		free(pd);
	}
	for (int i = 0; i < ix->n; i++) {
		const BhStanza *s = &ix->st[i];
		if (s->from[0] || strcmp(s->name, name))
			continue;
		near = s;
		if (strcmp(s->arch, arch) || strcmp(s->bhash, bhash) ||
		    !ehash[0] || strcmp(s->ehash, ehash) ||
		    !file_name_ok(s->file) || strlen(s->sha) != 64)
			continue;
		return i;
	}
	if (!near)
		snprintf(why, wcap, "not in the binhost");
	else if (strcmp(near->arch, arch))
		snprintf(why, wcap, "built for %s", near->arch);
	else if (strcmp(near->bhash, bhash))
		snprintf(why, wcap, "different build config");
	else
		snprintf(why, wcap, "different recipe");
	return -1;
}

/* 1 when the cache already holds `s`'s file, whole and with its hash. */
static int bh_cached(const BhStanza *s)
{
	char p[1024], sha[65] = "";
	snprintf(p, sizeof(p), "%s/%s", cache_dir(), s->file);
	return kb_sha256_file(p, sha) == 0 && !strcmp(sha, s->sha);
}

/* 1 when a file of `s`'s size is in the cache: what is left to download is
 * estimated by size, because hashing every cached package on every check
 * would read gigabytes to print one number. The download decides by hash. */
static int bh_present(const BhStanza *s)
{
	char p[1024];
	struct stat st;
	snprintf(p, sizeof(p), "%s/%s", cache_dir(), s->file);
	return stat(p, &st) == 0 && S_ISREG(st.st_mode) &&
	       (long long)st.st_size == s->size;
}

/* ── problems ──────────────────────────────────────────────────────────── */

/*
 * The one problem worth a notice, worst first: no tree at all, then a fetch
 * that failed (the machine cannot learn of updates), then a binhost index
 * that did not arrive or did not verify, then an apply that failed. `code` is
 * empty when there is none. A binhost that is simply not published is not a
 * problem; neither is an acquired slot waiting to be armed, nor a cancel.
 */
static void problem(const KpConf *c, char *code, size_t ccap, char *detail,
		    size_t dcap)
{
	char fc[32], fd[256], bc[20], bd[256], ac[32], ad[256], ic[20], id[256];
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
	if (state_get("binhost", bc, sizeof(bc), bd, sizeof(bd)) &&
	    strcmp(bc, "ok") && strcmp(bc, "absent")) {
		snprintf(code, ccap, "binhost-%s", bc);
		kb_strlcpy(detail, bd, dcap);
		return;
	}
	if (state_get("image", ic, sizeof(ic), id, sizeof(id)) &&
	    strcmp(ic, "ok") && strcmp(ic, "absent")) {
		snprintf(code, ccap, "image-%s", ic);
		kb_strlcpy(detail, id, dcap);
		return;
	}
	if (state_get("apply", ac, sizeof(ac), ad, sizeof(ad)) &&
	    strcmp(ac, "ok") && strcmp(ac, "acquired") &&
	    strcmp(ac, "cancelled")) {
		kb_strlcpy(code, ac, ccap);
		kb_strlcpy(detail, ad, dcap);
	}
}

/* ── the slot image ────────────────────────────────────────────────────── */

/* `TREE_TIME` of `root`'s kdos-release: when the release it holds was
 * made, which orders two releases; 0 when the root does not say. */
static long long release_time(const char *root)
{
	char p[1024];
	long long t = 0;
	snprintf(p, sizeof(p), "%s%s", strcmp(root, "/") ? root : "",
		 RELEASE_FILE);
	char *d = kb_read_all(p, NULL);
	for (char *l = d; l && *l;) {
		char *nl = strchr(l, '\n');
		if (nl)
			*nl = 0;
		if (!strncmp(l, "TREE_TIME=", 10))
			t = atoll(l + 10 + (l[10] == '"'));
		l = nl ? nl + 1 : NULL;
	}
	free(d);
	return t;
}

/* Where the cached image manifest was fetched from, or "". */
static void image_origin(char *out, size_t cap)
{
	char dir[1024], p[1100];
	image_dir(dir, sizeof(dir));
	snprintf(p, sizeof(p), "%s/ORIGIN", dir);
	char *o = kb_read_all(p, NULL);
	out[0] = 0;
	if (o) {
		o[strcspn(o, "\r\n")] = 0;
		kb_strlcpy(out, o, cap);
		free(o);
	}
}

/* The version of the image this system was written from, "" when it was
 * not: the delta a machine can use is the one from that version. */
static void image_installed(const char *root, char *out, size_t cap)
{
	char p[1024], tree[64];
	long long t;
	snprintf(p, sizeof(p), "%s%s", strcmp(root, "/") ? root : "",
		 IMAGE_RECORD);
	if (kdt_image_peek(p, out, cap, tree, sizeof(tree), &t) != 0)
		out[0] = 0;
}

/*
 * THE IMAGE ON OFFER: the cached manifest, verified against the host keyring
 * before it is read and named, with its hash, by the cached release index it
 * was fetched by, built for this machine's architecture, and of a release
 * NEWER than the one this system is — by its time when both say one, by its
 * name otherwise. NULL with the reason in `why` for anything else.
 */
static KdtImage *image_offer(char *why, size_t wcap)
{
	char dir[1024], mp[1100], sp[1100], arch[32], inst[64];
	image_dir(dir, sizeof(dir));
	snprintf(mp, sizeof(mp), "%s/MANIFEST", dir);
	snprintf(sp, sizeof(sp), "%s/MANIFEST.sig", dir);
	if (!kb_path_exists(mp)) {
		snprintf(why, wcap, "no slot image manifest is on this machine");
		return NULL;
	}
	KdtImage *m = kdt_image_load(mp, sp, keyring_dir(), why, wcap);
	if (!m)
		return NULL;
	/* Offered only together with the release index it was fetched by,
	 * which is where an apply finds every bundle. */
	KdtRelIdx *rel = kdt_relidx_load(dir, keyring_dir(), why, wcap);
	int had = rel != NULL;
	int bound = had && kdt_relidx_holds(rel, "image/kdos-image.manifest", mp) &&
		    !strcmp(kdt_relidx_version(rel), kdt_image_version(m));
	kdt_relidx_free(rel);
	if (!bound) {
		if (had)
			snprintf(why, wcap, "the cached slot image is not the one "
				 "the signed release index names");
		kdt_image_free(m);
		return NULL;
	}
	kp_arch(arch, sizeof(arch));
	bootctl_root_version(sys_root(), inst, sizeof(inst));
	/* The release time this root records, else that of the image it was
	 * written from: either orders an older image out. */
	long long it = release_time(sys_root());
	if (!it) {
		char rp[1024], rv[64], rt[64];
		snprintf(rp, sizeof(rp), "%s%s",
			 strcmp(sys_root(), "/") ? sys_root() : "", IMAGE_RECORD);
		kdt_image_peek(rp, rv, sizeof(rv), rt, sizeof(rt), &it);
	}
	if (strcmp(kdt_image_arch(m), arch)) {
		snprintf(why, wcap, "the slot image is built for %s",
			 kdt_image_arch(m));
	} else if (!strcmp(kdt_image_version(m), inst) ||
		   (it && kdt_image_time(m) && kdt_image_time(m) <= it) ||
		   ((!it || !kdt_image_time(m)) &&
		    (!inst[0] || kp_vercmp(kdt_image_version(m), inst) <= 0))) {
		/* With no time on either side the names decide, and a name
		 * that is not newer is refused: an older release's manifest
		 * is signed too, and offering it would be a downgrade. */
		snprintf(why, wcap, "this system already runs %s or newer",
			 kdt_image_version(m));
	} else {
		return m;
	}
	kdt_image_free(m);
	return NULL;
}

/* ── check ─────────────────────────────────────────────────────────────── */

/* The release notes a published tree carries in `<tree>/RELEASE`, plain
 * paragraphs; at most 16 KiB of them, so a document read on every check
 * stays small. NULL when there are none. */
static char *tree_notes(const char *root)
{
	char *p = kb_path_join(root, "RELEASE");
	size_t n = 0;
	char *d = kb_read_all(p, &n);
	free(p);
	if (!d)
		return NULL;
	if (n > 16384)
		d[16384] = 0;
	return d;
}

/*
 * THE SURVEY AS ONE LINE OF JSON, for a program rather than a person: the
 * update document kdos-sysupdated serves Discover from, the notices daemon
 * watches, and the System Versions page reads.
 *
 *   behind, orphans, packages[]   what is older than its recipe, each with
 *                                 `binary`: whether the binhost covers it
 *   tree, version, date           the tree in place, the version it is, and
 *                                 the day (UTC) its commit was made — or the
 *                                 offered image's, when the lane is `image` —
 *                                 which is the release date Discover shows
 *   installed                     the version this system says it is
 *   covered, uncovered, size      how much of `behind` the binhost covers,
 *                                 and the bytes still to download for it; a
 *                                 package whose origin is `source` counts as
 *                                 covered, since the update builds it here
 *   builds                        how many of `behind` the update builds
 *                                 here rather than downloads
 *   offer, lane                   the version an update would make this
 *                                 system, and from where: `image` when a
 *                                 verified slot image of a newer release is
 *                                 cached (`size` is then its download), else
 *                                 `binhost` when every behind package is
 *                                 covered by a verified index
 *   signed_by, verified, binhost  the key that signed what is offered, when
 *                                 it was verified, and where the binhost
 *                                 index came from
 *   image                         where the offered image is fetched from
 *   channel, notes, checked       the tree source in effect, the tree's
 *                                 release notes, the time of this check
 *   problem, detail               the worst recorded problem
 *
 * IT ANSWERS IN JSON EVEN WHEN IT CANNOT ANSWER: no tree is `"behind":0` with
 * `"problem":"no-tree"`, never a paragraph a caller cannot tell from a parse
 * failure. ITS EXIT STATUS IS THE HUMAN FORM'S: 1 when something is behind or
 * an image is offered, 2 when nothing could be surveyed and nothing is.
 */
static int check_json(const KpConf *c, FILE *o)
{
	char pc[32], pd[256], commit[64] = "", root[512];
	char ver[64] = "", inst[64] = "", why[256], base[1024];
	long long t;
	Behind *v = NULL;
	int orphans = 0, n = 0, rc, covered = 0, builds = 0;
	long long size = 0;
	BhIndex ix;
	int have_ix;

	problem(c, pc, sizeof(pc), pd, sizeof(pd));
	ports_root(c, root, sizeof(root));
	tree_read(root, commit, sizeof(commit), &t);
	tree_version(root, ver, sizeof(ver), NULL);
	bootctl_root_version(sys_root(), inst, sizeof(inst));
	tree_base(NULL, base, sizeof(base));

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

	have_ix = bh_load(&ix, why, sizeof(why)) == 0;
	for (int i = 0; i < n; i++)
		if (origin_of(v[i].name) == ROUTE_SOURCE) {
			v[i].stanza = -1;
			builds++;
		}
	covered = builds;
	if (have_ix && n) {
		build_env_for(NULL);
		for (int i = 0; i < n; i++) {
			if (origin_of(v[i].name) == ROUTE_SOURCE)
				continue;
			v[i].stanza = bh_match(c, &ix, v[i].name, why,
					       sizeof(why));
			if (v[i].stanza < 0)
				continue;
			covered++;
			if (!bh_present(&ix.st[v[i].stanza]))
				size += ix.st[v[i].stanza].size;
		}
	}
	char iwhy[256], irec[64], iorigin[1024] = "";
	KdtImage *im = image_offer(iwhy, sizeof(iwhy));
	int offered = !im && (have_ix || builds == n) && n > 0 && covered == n &&
		      ver[0];
	if (im) {
		image_installed(sys_root(), irec, sizeof(irec));
		image_origin(iorigin, sizeof(iorigin));
		size = kdt_image_download_size(im, irec);
		rc = 1;
	}

	KbBuf b = {0};
	kb_buf_printf(&b, "{\"behind\":%d,\"orphans\":%d,\"tree\":", n,
		      orphans);
	if (commit[0])
		kb_json_str(&b, commit);
	else
		kb_buf_str(&b, "null");
	kb_buf_str(&b, ",\"version\":");
	if (ver[0])
		kb_json_str(&b, ver);
	else
		kb_buf_str(&b, "null");
	/* The day of what is offered: the image's release when one is, else
	 * the tree's commit. */
	char day[16] = "";
	time_t tt = (time_t)(im && kdt_image_time(im) ? kdt_image_time(im) : t);
	struct tm tm;
	if ((im ? tt > 0 : commit[0] && t > 0) && gmtime_r(&tt, &tm))
		strftime(day, sizeof(day), "%Y-%m-%d", &tm);
	kb_buf_str(&b, ",\"date\":");
	if (day[0])
		kb_json_str(&b, day);
	else
		kb_buf_str(&b, "null");
	kb_buf_str(&b, ",\"installed\":");
	if (inst[0])
		kb_json_str(&b, inst);
	else
		kb_buf_str(&b, "null");
	kb_buf_str(&b, ",\"problem\":");
	if (pc[0])
		kb_json_str(&b, pc);
	else
		kb_buf_str(&b, "null");
	kb_buf_str(&b, ",\"detail\":");
	kb_json_str(&b, pd);
	kb_buf_printf(&b, ",\"covered\":%d,\"uncovered\":%d,\"size\":%lld,"
		      "\"builds\":%d", covered, n - covered, size, builds);
	kb_buf_str(&b, ",\"offer\":");
	if (im)
		kb_json_str(&b, kdt_image_version(im));
	else if (offered)
		kb_json_str(&b, ver);
	else
		kb_buf_str(&b, "null");
	kb_buf_str(&b, im ? ",\"lane\":\"image\""
			  : offered ? ",\"lane\":\"binhost\"" : ",\"lane\":null");
	kb_buf_str(&b, ",\"signed_by\":");
	if (im) {
		kb_buf_str(&b, "{\"key\":");
		kb_json_str(&b, kdt_image_signer(im));
		kb_buf_str(&b, ",\"name\":");
		kb_json_str(&b, kdt_image_signer_name(im));
		kb_buf_printf(&b, "},\"verified\":%lld,\"binhost\":",
			      (long long)time(NULL));
		if (have_ix && ix.origin[0])
			kb_json_str(&b, ix.origin);
		else
			kb_buf_str(&b, "null");
	} else if (have_ix) {
		kb_buf_str(&b, "{\"key\":");
		kb_json_str(&b, ix.who);
		kb_buf_str(&b, ",\"name\":");
		kb_json_str(&b, ix.keyname);
		kb_buf_printf(&b, "},\"verified\":%lld,\"binhost\":",
			      (long long)time(NULL));
		if (ix.origin[0])
			kb_json_str(&b, ix.origin);
		else
			kb_buf_str(&b, "null");
	} else {
		kb_buf_str(&b, "null,\"verified\":null,\"binhost\":null");
	}
	kb_buf_str(&b, ",\"image\":");
	if (im && iorigin[0])
		kb_json_str(&b, iorigin);
	else
		kb_buf_str(&b, "null");
	kb_buf_str(&b, ",\"channel\":");
	kb_json_str(&b, base);
	kb_buf_str(&b, ",\"notes\":");
	char *notes = tree_notes(root);
	if (notes)
		kb_json_str(&b, notes);
	else
		kb_buf_str(&b, "null");
	free(notes);
	kb_buf_printf(&b, ",\"checked\":%lld,\"packages\":[",
		      (long long)time(NULL));
	for (int i = 0; i < n; i++) {
		kb_buf_str(&b, i ? ",{\"name\":" : "{\"name\":");
		kb_json_str(&b, v[i].name);
		kb_buf_str(&b, ",\"have\":");
		kb_json_str(&b, v[i].have);
		kb_buf_printf(&b, ",\"want\":\"%s-%s\",\"binary\":%s}",
			      v[i].want,
			      v[i].want_rel[0] ? v[i].want_rel : "1",
			      v[i].stanza >= 0 ? "true" : "false");
	}
	kb_buf_str(&b, "]}\n");
	fputs(b.p, o);
	kb_buf_free(&b);
	bh_free(&ix);
	kdt_image_free(im);
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
 * TEMP AND RENAME, because a reader arrives on its own schedule:
 * kdos-sysupdated rereads this file the moment it is renamed into place, and
 * half a document parses as nothing behind — a machine that is out of date
 * reporting that it is not. The document is written whatever the answer, a
 * missing tree included, so the reader shows the problem rather than nothing.
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
 * The update document, rewritten after a fetch or an apply as well as by the
 * nightly timer: a person who has just fixed what a notice complained about
 * should not see the complaint until 04:17. The running root's answer, never
 * the slot's — the document describes the system it is read on.
 */
static void doc_refresh(void)
{
	if (geteuid() != 0 || getenv("KDOS_UPDATE_STATE") ||
	    !kb_is_dir("/var/lib/kdos"))
		return;
	unsetenv("KPKG_ROOT");
	KpConf c;
	kp_conf_load(&c);
	check_json_to(&c, UPDATE_DOC);
}

static int cmd_check_human(const KpConf *c)
{
	char pc[32], pd[256], commit[64] = "", root[512], why[256];
	char ver[64] = "", inst[64] = "";
	long long t;

	problem(c, pc, sizeof(pc), pd, sizeof(pd));
	if (!strcmp(pc, "no-tree")) {
		char iwhy[256];
		KdtImage *im = image_offer(iwhy, sizeof(iwhy));
		fprintf(stderr, "kdos update: %s\n", NO_TREE_TEXT);
		if (!im)
			return 2;
		printf("  image: %s, signed by key %s, is offered\n",
		       kdt_image_version(im), kdt_image_signer(im));
		kdt_image_free(im);
		return 1;
	}

	Behind *v = NULL;
	int orphans = 0;
	int n = survey(c, &v, &orphans);
	if (n < 0) {
		fprintf(stderr, "kdos update: no package database\n");
		return 2;
	}
	BhIndex ix;
	int have_ix = bh_load(&ix, why, sizeof(why)) == 0, covered = 0;
	if (have_ix && n) {
		build_env_for(NULL);
		for (int i = 0; i < n; i++)
			if ((v[i].stanza = bh_match(c, &ix, v[i].name, why,
						    sizeof(why))) >= 0)
				covered++;
	}

	printf("%sKDOS update%s  %s—  the ports tree against what is "
	       "installed%s\n\n", A, O, D, O);

	for (int i = 0; i < n; i++)
		printf("  %-24s %-14s → %s-%s%s\n", v[i].name, v[i].have,
		       v[i].want, v[i].want_rel[0] ? v[i].want_rel : "1",
		       have_ix && v[i].stanza < 0 ? "  (source build)" : "");
	if (n)
		putchar('\n');

	printf("  %d behind, %d with no recipe on this machine\n", n, orphans);
	ports_root(c, root, sizeof(root));
	tree_version(root, ver, sizeof(ver), NULL);
	bootctl_root_version(sys_root(), inst, sizeof(inst));
	if (inst[0])
		printf("  %sthis system is %s%s\n", D, inst, O);
	if (tree_read(root, commit, sizeof(commit), &t) == 0) {
		char day[16] = "";
		time_t tt = (time_t)t;
		struct tm tm;
		if (t > 0 && gmtime_r(&tt, &tm))
			strftime(day, sizeof(day), "%Y-%m-%d", &tm);
		printf("  %stree %s (%s)%s%s at %s%s\n", D, commit, ver,
		       day[0] ? ", " : "", day, root, O);
	} else {
		printf("  %s%s carries no TREE stamp — `kdos update fetch` "
		       "cannot tell an older tree from a newer one%s\n", W,
		       root, O);
	}
	if (!have_ix)
		printf("  %sbinhost: %s — `kdos update apply --source` builds "
		       "what is behind%s\n", D, why, O);
	else if (n && covered == n)
		printf("  binhost: all %d prebuilt, signed by %s (key %s); "
		       "offered as %s\n", n,
		       ix.keyname[0] ? ix.keyname : "a trusted key", ix.who,
		       ver);
	else if (n)
		printf("  %sbinhost: %d of %d prebuilt; the rest need "
		       "`kdos update apply --source`%s\n", W, covered, n, O);
	char iwhy[256];
	KdtImage *im = image_offer(iwhy, sizeof(iwhy));
	if (im) {
		char irec[64];
		image_installed(sys_root(), irec, sizeof(irec));
		printf("  image: %s, signed by %s (key %s); offered, %s to "
		       "download\n", kdt_image_version(im),
		       kdt_image_signer_name(im)[0] ? kdt_image_signer_name(im)
						    : "a trusted key",
		       kdt_image_signer(im),
		       kb_human_size((unsigned long long)
				     kdt_image_download_size(im, irec)));
		kdt_image_free(im);
	} else {
		printf("  %simage: %s%s\n", D, iwhy, O);
	}
	if (pc[0])
		printf("  %s%s: %s%s\n", W, pc, pd, O);
	bh_free(&ix);
	free(v);
	return n || im ? 1 : 0;
}

/* ── fetch ─────────────────────────────────────────────────────────────── */

enum { GOT_OK, GOT_FAIL, GOT_OFFLINE, GOT_ABSENT };

/* curl's exit codes for "never reached a server": a name that did not resolve
 * (5, 6), a connection refused or unroutable (7), a timeout (28). 22 is a
 * server that answered with an error — for a file that should be there, it is
 * not; 37 is a file:// path with nothing at it. Anything else reached
 * somebody, and is a failure of that copy.
 *
 * HTTPS OR A LOCAL PATH, NOTHING ELSE, and no redirect down to plain HTTP:
 * every file is verified by signature or by a signed hash anyway, but a
 * fetch an observer can read says which release a machine is taking. */
static int download(const char *url, const char *out, int quiet)
{
	KbArgv a = {0};
	kb_argv_add(&a, "curl");
	kb_argv_add(&a, "-fL");
	kb_argv_add(&a, !quiet && isatty(STDOUT_FILENO) ? "-#" : "-sS");
	kb_argv_add(&a, "--proto");
	kb_argv_add(&a, "=https,file");
	kb_argv_add(&a, "--proto-redir");
	kb_argv_add(&a, "=https");
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
	if (rc == 5 || rc == 6 || rc == 7 || rc == 28)
		return GOT_OFFLINE;
	return rc == 22 || rc == 37 ? GOT_ABSENT : GOT_FAIL;
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
 * Called with the update lock held.
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
	char root[512], stage[600], base[1024];
	ports_root(c, root, sizeof(root));
	if (root[0] != '/' || !root[1]) {
		fprintf(stderr, "kdos update: %s is not a ports tree to "
				"replace\n", root);
		return 2;
	}
	/* A tree inside a git checkout belongs to git: replacing it would
	 * discard whatever the checkout holds that no release does. */
	char gitp[600];
	snprintf(gitp, sizeof(gitp), "%s/../.git", root);
	if (kb_path_exists(gitp)) {
		fprintf(stderr, "kdos update: %s is part of a git checkout — "
				"update it with git\n", root);
		return 2;
	}

	/* The key before the network. An image built without a release key
	 * can verify no published tree, so downloading one only to refuse it
	 * would report a missing release, or a signature, where the truth is
	 * that this machine trusts no tree at all. */
	const char *kd = getenv("KDOS_TREE_KEYS");
	KsigRing ring;
	char who[KSIG_ID_HEX];
	if (ksig_ring_load(&ring, kd && *kd ? kd : TREE_KEYS) <= 0)
		return fetch_fail("no-key", NULL, "no trusted tree key in %s: "
				  "this image can verify no published tree; "
				  "`kdos update apply --source` builds from %s",
				  kd && *kd ? kd : TREE_KEYS, root);

	snprintf(stage, sizeof(stage), "%s.new", root);
	kb_rmtree(stage);
	if (kb_mkdir_p(stage) != 0)
		return fetch_fail("fetch-failed", NULL, "cannot create %s: %s",
				  stage, strerror(errno));
	tree_base(from, base, sizeof(base));

	char url[1200], tar[700], sig[720];
	snprintf(tar, sizeof(tar), "%s/%s", stage, TREE_FILE);
	snprintf(sig, sizeof(sig), "%s.sig", tar);

	if (!quiet)
		printf("%s==>%s fetching the ports tree from %s\n", A, O, base);
	/* The signature first: it is small, and the cheapest way to learn the
	 * network is down. A signature byte for byte the one the installed tree
	 * came with is the same tree, so a check downloads a few hundred bytes
	 * and not the whole tree when nothing was released. */
	snprintf(url, sizeof(url), "%s/%s.sig", base, TREE_FILE);
	int got = download(url, sig, quiet);
	char ic[64];
	long long it;
	if (got == GOT_OK && same_as_last_sig(sig) &&
	    tree_read(root, ic, sizeof(ic), &it) == 0) {
		kb_rmtree(stage);
		state_set("fetch", "ok", "tree current");
		if (!quiet)
			printf("the ports tree is current (%s)\n", ic);
		return 0;
	}
	if (got == GOT_OK) {
		snprintf(url, sizeof(url), "%s/%s", base, TREE_FILE);
		got = download(url, tar, quiet);
	}
	if (got != GOT_OK)
		return got == GOT_OFFLINE
			? fetch_fail("no-network", stage, "the network could "
				     "not be reached (%s)", base)
			: fetch_fail("fetch-failed", stage, "%s/%s could not be "
				     "downloaded", base, TREE_FILE);

	if (ksig_verify_path(&ring, tar, sig, who) != 0)
		return fetch_fail("fetch-failed", stage, "the tree's signature "
				  "does not verify against %s",
				  kd && *kd ? kd : TREE_KEYS);

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
	if (kb_run_tty(&x) != 0)
		return fetch_fail("fetch-failed", stage, "the signed tree did "
				  "not unpack");

	char ncommit[64], ocommit[64];
	long long nt, ot;
	char *core = kb_path_join(tree, "core");
	char *idx = kb_path_join(tree, "sources.idx");
	int whole = kb_is_dir(core) && kb_path_exists(idx);
	free(core);
	free(idx);
	if (tree_read(tree, ncommit, sizeof(ncommit), &nt) != 0 || !whole)
		return fetch_fail("fetch-failed", stage, "the signed archive "
				  "is not a ports tree (no TREE, core/ or "
				  "sources.idx)");
	if (tree_read(root, ocommit, sizeof(ocommit), &ot) == 0) {
		if (same_commit(ncommit, ocommit)) {
			keep_sig(sig);
			kb_rmtree(stage);
			state_set("fetch", "ok", "tree current");
			if (!quiet)
				printf("the ports tree is current (%s)\n",
				       ocommit);
			return 0;
		}
		if (nt < ot)
			return fetch_fail("fetch-failed", stage, "the published "
					  "tree %.12s is older than this "
					  "machine's %.12s", ncommit, ocommit);
	}

	if (swap_dirs(tree, root) != 0)
		return fetch_fail("fetch-failed", stage, "%s could not be "
				  "replaced: %s", root, strerror(errno));
	keep_sig(sig);
	kb_rmtree(stage);

	char msg[128];
	snprintf(msg, sizeof(msg), "tree %.12s, signed by %s", ncommit, who);
	state_set("fetch", "ok", msg);
	if (!quiet)
		printf("%s now holds tree %s (signed by key %s)\n", root,
		       ncommit, who);
	return 0;
}

/*
 * Where the release index of the tree in place is: `binhost =` in
 * update.conf, then $KDOS_UPDATE_BINHOST, then the release `<version>` of a
 * tree whose TREE names its release, which carries kdos-update.idx beside
 * the tree. 0 with the URL in `out`; -1 when nothing names one, which is
 * every tree that is not a published release's.
 */
static int binhost_base(const char *version, int named, char *out, size_t cap)
{
	char *conf = conf_value("binhost");
	const char *e = getenv("KDOS_UPDATE_BINHOST");
	int rc = 0;

	if (conf)
		as_url(conf, out, cap);
	else if (e && *e)
		as_url(e, out, cap);
	else if (named && version_ok(version))
		snprintf(out, cap, "%s/%s", BINHOST_BASE, version);
	else
		rc = -1;
	free(conf);
	return rc;
}

/* The cache keeps the two indexes, the packages the CURRENT index names and
 * the older packages its deltas apply to; a package nothing in the index
 * names is never installed again, so it goes. */
static void cache_prune(const BhIndex *ix)
{
	int n = 0;
	char **names = kb_listdir(cache_dir(), &n);

	for (int i = 0; names && i < n; i++) {
		const char *f = names[i];
		char base[256];
		if (!strcmp(f, BINHOST_INDEX) || !strcmp(f, BINHOST_SIG) ||
		    !strcmp(f, "INDEX") || !strcmp(f, "INDEX.sig") ||
		    !strcmp(f, "ORIGIN") || !strcmp(f, "keys"))
			continue;
		kb_strlcpy(base, f, sizeof(base));
		size_t bl = strlen(base);
		if (bl > 4 && !strcmp(base + bl - 4, ".sig"))
			base[bl - 4] = 0;
		int named = 0;
		for (int k = 0; ix && k < ix->n && !named; k++)
			named = !strcmp(ix->st[k].file, base) ||
				!strcmp(ix->st[k].from, base);
		if (!named) {
			char *p = kb_path_join(cache_dir(), f);
			if (!kb_is_dir(p))
				unlink(p);
			free(p);
		}
	}
	kb_strv_free(names);
}

#define BINHOST_CATALOGUE "swcatalog/xml/kdos-binhost.xml"

/*
 * THE BINHOST'S APPSTREAM CATALOGUE, beside its index: the ports an install
 * can add to this slot, which Discover's KDOS backend lists from
 * <cache>/swcatalog/xml/kdos-binhost.xml at every refresh. Taken through the
 * release index like every file, and verified against the host keyring like
 * the binhost index before it replaces the cached copy. A release index that
 * names none, or a catalogue that does not verify, leaves no copy: a
 * catalogue from an earlier release would offer ports this index does not
 * hold. A failed download keeps the copy in place, and none of the three is
 * an error of the fetch, because the ports lane then only offers less.
 */
static void catalogue_drop(void)
{
	char xp[1100], sp[1110];
	snprintf(xp, sizeof(xp), "%s/" BINHOST_CATALOGUE, cache_dir());
	snprintf(sp, sizeof(sp), "%s.sig", xp);
	unlink(xp);
	unlink(sp);
}

static void catalogue_fetch(const KdtRelIdx *rel, const KsigRing *ring)
{
	char dir[1024], xp[1100], sp[1110], xn[1200], sn[1200], why[256];

	snprintf(dir, sizeof(dir), "%s/swcatalog/xml", cache_dir());
	snprintf(xp, sizeof(xp), "%s/" BINHOST_CATALOGUE, cache_dir());
	snprintf(sp, sizeof(sp), "%s.sig", xp);
	snprintf(xn, sizeof(xn), "%s.new", xp);
	snprintf(sn, sizeof(sn), "%s.new", sp);
	if (kb_mkdir_p(dir) != 0)
		return;
	int got = kdt_relidx_take(rel, "binhost/" BINHOST_CATALOGUE ".sig", sn,
				  why, sizeof(why));
	if (got == KDT_GOT_OK)
		got = kdt_relidx_take(rel, "binhost/" BINHOST_CATALOGUE, xn, why,
				      sizeof(why));
	if (got != KDT_GOT_OK) {
		unlink(sn);
		unlink(xn);
		if (got == KDT_GOT_ABSENT || got == KDT_GOT_UNVERIFIED)
			catalogue_drop();
		return;
	}
	size_t len = 0;
	char *data = kb_read_all(xn, &len);
	char who[KSIG_ID_HEX];
	if (!data || ksig_verify_file(ring, sn, data, len, who) != 0 ||
	    rename(xn, xp) != 0 || rename(sn, sp) != 0) {
		unlink(sn);
		unlink(xn);
		unlink(xp);
		unlink(sp);
	}
	free(data);
}

static int binhost_refuse(const char *stage, const char *code, const char *msg)
{
	kb_rmtree(stage);
	state_set("binhost", code, msg);
	if (!strcmp(code, "unverified"))
		fprintf(stderr, "kdos update: %s — it was not kept\n", msg);
	return 1;
}

/*
 * The signed binhost index for the tree in place, into the cache, through
 * the release index. Both are downloaded into <cache>/new: the release index
 * is verified against the host keyring and must be for the tree's release,
 * PACKAGES and PACKAGES.sig are taken where it says and must hash as it
 * says, and PACKAGES must verify against the host keyring too. Only then do
 * the four files and ORIGIN replace the cached ones, so the cache never
 * holds an index no trusted key made, nor a pair that disagree. A release
 * with no release index, or one whose index names no binhost, is recorded
 * as `absent` and is not a failure. Called with the update lock held; the
 * outcome is the `binhost` line of update.state.
 */
static int binhost_fetch(const KpConf *c, int quiet)
{
	static const char *const keep[] = { "INDEX", "INDEX.sig", "ORIGIN",
					    BINHOST_SIG, BINHOST_INDEX, NULL };
	char root[512], ver[64], base[1024], stage[1100], why[512], msg[1400];
	int named = 0, how;

	ports_root(c, root, sizeof(root));
	tree_version(root, ver, sizeof(ver), &named);
	if (binhost_base(ver, named, base, sizeof(base)) != 0) {
		catalogue_drop();
		state_set("binhost", "absent", "no binhost is named for this "
			  "tree");
		return 0;
	}
	if (!host_keyed()) {
		snprintf(msg, sizeof(msg), "no trusted key in %s can verify a "
			 "release index", keyring_dir());
		state_set("binhost", "no-key", msg);
		return 1;
	}
	if (kb_mkdir_p(cache_dir()) != 0) {
		state_set("binhost", "fetch-failed", "cannot create the binhost "
			  "cache");
		return 1;
	}
	snprintf(stage, sizeof(stage), "%s/new", cache_dir());
	kb_rmtree(stage);

	if (!quiet)
		printf("%s==>%s fetching the release index from %s\n", A, O,
		       base);
	KdtRelIdx *rel = kdt_relidx_fetch(base, stage, keyring_dir(), &how,
					  why, sizeof(why));
	if (!rel) {
		if (how == KDT_GOT_ABSENT) {
			kb_rmtree(stage);
			catalogue_drop();
			snprintf(msg, sizeof(msg), "no binhost is published at "
				 "%s", base);
			state_set("binhost", "absent", msg);
			return 0;
		}
		return binhost_refuse(stage, how == KDT_GOT_OFFLINE ? "no-network"
				      : how == KDT_GOT_UNVERIFIED ? "unverified"
				      : "fetch-failed", why);
	}
	if (named && strcmp(kdt_relidx_version(rel), ver)) {
		snprintf(msg, sizeof(msg), "the release index at %s is for %s, "
			 "and this tree is %s", base, kdt_relidx_version(rel),
			 ver);
		kdt_relidx_free(rel);
		return binhost_refuse(stage, "unverified", msg);
	}
	char ip[1200], sp[1200];
	snprintf(ip, sizeof(ip), "%s/" BINHOST_INDEX, stage);
	snprintf(sp, sizeof(sp), "%s/" BINHOST_SIG, stage);
	if (kdt_relidx_url(rel, "binhost/" BINHOST_INDEX, msg, sizeof(msg), NULL,
			   NULL) != 0) {
		kdt_relidx_free(rel);
		kb_rmtree(stage);
		catalogue_drop();
		snprintf(msg, sizeof(msg), "the release at %s publishes no "
			 "binhost", base);
		state_set("binhost", "absent", msg);
		return 0;
	}
	int got = kdt_relidx_take(rel, "binhost/" BINHOST_SIG, sp, why,
				  sizeof(why));
	if (got == KDT_GOT_OK)
		got = kdt_relidx_take(rel, "binhost/" BINHOST_INDEX, ip, why,
				      sizeof(why));
	if (got != KDT_GOT_OK) {
		kdt_relidx_free(rel);
		return binhost_refuse(stage, got == KDT_GOT_OFFLINE ? "no-network"
				      : got == KDT_GOT_FAILED ? "fetch-failed"
				      : "unverified", why);
	}

	size_t len = 0;
	char *data = kb_read_all(ip, &len);
	KsigRing ring;
	char who[KSIG_ID_HEX];
	int ok = data && ksig_ring_load(&ring, keyring_dir()) > 0 &&
		 ksig_verify_file(&ring, sp, data, len, who) == 0;
	free(data);
	if (!ok) {
		kdt_relidx_free(rel);
		return binhost_refuse(stage, "unverified", "the binhost index's "
				      "signature does not verify against "
				      "/etc/kdos/keys");
	}
	for (int i = 0; keep[i]; i++) {
		char from[1300], to[1300];
		snprintf(from, sizeof(from), "%s/%s", stage, keep[i]);
		snprintf(to, sizeof(to), "%s/%s", cache_dir(), keep[i]);
		if (rename(from, to) != 0) {
			kdt_relidx_free(rel);
			return binhost_refuse(stage, "fetch-failed", "the binhost "
					      "index could not be put in the "
					      "cache");
		}
	}
	kb_rmtree(stage);
	catalogue_fetch(rel, &ring);
	kdt_relidx_free(rel);

	BhIndex ix;
	if (bh_load(&ix, why, sizeof(why)) == 0) {
		cache_prune(&ix);
		snprintf(msg, sizeof(msg), "%d packages, signed by %s", ix.n,
			 who);
		state_set("binhost", "ok", msg);
		if (!quiet)
			printf("binhost index: %d packages, signed by key %s\n",
			       ix.n, who);
		bh_free(&ix);
	}
	return 0;
}

/*
 * Where the slot image's release index is: `--from`, then `image =` in
 * update.conf, then $KDOS_UPDATE_IMAGE, then the latest KDOS release, which
 * carries kdos-update.idx beside the ISO and the tree.
 */
static void image_base(const char *from, char *out, size_t cap)
{
	char *conf = NULL;
	const char *v = from;
	if (!v || !*v)
		v = conf = conf_value("image");
	if (!v || !*v)
		v = getenv("KDOS_UPDATE_IMAGE");
	if (!v || !*v)
		v = TREE_BASE;
	as_url(v, out, cap);
	free(conf);
}

/*
 * The release index at the image's place and the signed slot image manifest
 * it names, into the cache, each verified against the host keyring and the
 * manifest against the index's hash before the pair replaces the one there
 * (kdt_image_fetch). Only the manifest: the bundles are read by an apply,
 * through the same cached index. A place with no index, or an index that
 * names no image, is recorded as `absent` and is not a failure — the binhost
 * is the way then. Called with the update lock held; the outcome is the
 * `image` line of update.state.
 */
static int image_fetch(const char *from, int quiet)
{
	char dir[1024], base[1024], why[512], msg[640];
	int how;

	if (!host_keyed()) {
		snprintf(msg, sizeof(msg), "no trusted key in %s can verify a "
			 "release index", keyring_dir());
		state_set("image", "no-key", msg);
		return 1;
	}
	image_base(from, base, sizeof(base));
	image_dir(dir, sizeof(dir));
	if (!quiet)
		printf("%s==>%s fetching the slot image manifest from %s\n", A, O,
		       base);
	KdtImage *m = kdt_image_fetch(base, dir, keyring_dir(), &how, why,
				      sizeof(why));
	if (!m) {
		static const char *const code[] = {
			[KDT_IMAGE_FAILED] = "fetch-failed",
			[KDT_IMAGE_OFFLINE] = "no-network",
			[KDT_IMAGE_ABSENT] = "absent",
			[KDT_IMAGE_UNVERIFIED] = "unverified",
		};
		state_set("image", code[how], why);
		if (how == KDT_IMAGE_UNVERIFIED)
			fprintf(stderr, "kdos update: %s — it was not kept\n",
				why);
		return how == KDT_IMAGE_ABSENT ? 0 : 1;
	}
	snprintf(msg, sizeof(msg), "image %s, signed by %s",
		 kdt_image_version(m), kdt_image_signer(m));
	state_set("image", "ok", msg);
	if (!quiet)
		printf("slot image: %s, signed by key %s\n", kdt_image_version(m),
		       kdt_image_signer(m));
	kdt_image_free(m);
	return 0;
}

static int cmd_fetch(const KpConf *c, int argc, char **argv)
{
	const char *from = NULL;
	int only_bh = 0;
	for (int i = 0; i < argc; i++) {
		if (!strcmp(argv[i], "--from") && i + 1 < argc)
			from = argv[++i];
		else if (!strcmp(argv[i], "--binhost-only"))
			only_bh = 1;
		else {
			fprintf(stderr, "usage: kdos update fetch [--from "
					"URL|DIR] [--binhost-only]\n");
			return 2;
		}
	}
	if (geteuid() != 0) {
		fprintf(stderr, "kdos update fetch replaces the ports tree and "
				"needs root\n");
		return 2;
	}
	int lock = update_lock();
	if (lock < 0)
		return 2;
	/* THE TREE IN PLACE, ITS OWN RELEASE'S BINHOST: what the live medium
	 * and the installer install from, whatever is newer. A newer tree's
	 * packages are built against a newer base than this one runs. */
	if (only_bh) {
		int brc = binhost_fetch(c, 0) != 0;
		close(lock);
		doc_refresh();
		return brc;
	}
	int rc = tree_fetch(c, from, 0);
	if (rc != 2) {
		KpConf fresh;
		kp_conf_load(&fresh);
		if (binhost_fetch(&fresh, 0) != 0 && rc == 0)
			rc = 1;
	}
	if (image_fetch(from, 0) != 0 && rc == 0)
		rc = 1;
	close(lock);
	if (rc != 2)
		doc_refresh();
	return rc;
}

/* ── the A/B half ──────────────────────────────────────────────────────── */

/*
 * Where the inactive slot is mounted, or NULL.
 *
 * The state file names a slot by filesystem UUID; `--mount` mounts it at
 * /run/kdos/slot-<x> when nobody has, and without it this finds it only if
 * it is already mounted, and otherwise says exactly what to do.
 * $KDOS_UPDATE_SLOT stands in for the lookup: the directory a test filled as
 * the inactive slot, which is never a block device's mountpoint.
 */
static char *slot_mountpoint(const char *uuid)
{
	const char *e = getenv("KDOS_UPDATE_SLOT");

	if (e && *e)
		return kb_is_dir(e) ? kb_strdup(e) : NULL;
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

/* A `"key": "value"` of one-line JSON, into out; 0 when found. */
static int json_field(const char *doc, const char *key, char *out, size_t cap)
{
	char k[64];
	snprintf(k, sizeof(k), "\"%s\": \"", key);
	const char *p = strstr(doc, k);
	out[0] = 0;
	if (!p)
		return -1;
	p += strlen(k);
	const char *q = strchr(p, '"');
	if (!q)
		return -1;
	size_t n = (size_t)(q - p);
	if (n >= cap)
		n = cap - 1;
	memcpy(out, p, n);
	out[n] = 0;
	return 0;
}

typedef struct {
	char slot[8];		/* the inactive slot's letter */
	char uuid[96];		/* its filesystem */
	char crypt[96];		/* its LUKS container, "" for none */
	char running[8];	/* the slot this boot runs, "" when unknown */
	char trying[8];		/* the slot armed or on trial, "" for none */
	int live;		/* no slots because this is the live medium */
} Layout;

/* `kdos-bootctl status --json`, read for the inactive slot. Exec'd rather
 * than linked to the state parser directly so there is one reader of that
 * file and it is the one that owns its fsync discipline. 1 when the machine
 * has slots and the inactive one has a root, 0 when it does not; on 0,
 * `live` says whether the reason is that this boot is the live medium.
 *
 * THE INACTIVE SLOT IS THE ONE THAT IS NOT CONFIRMED, which on a trial boot
 * is the root this boot runs; `running` and `trying` are read beside it so
 * apply_target can refuse that case and disarm an armed slot before it is
 * written (slot_disarm). */
static int bootctl_layout(Layout *L)
{
	memset(L, 0, sizeof(*L));
	if (!kb_have_prog("kdos-bootctl"))
		return 0;

	KbArgv a = {0};
	kb_argv_add(&a, "kdos-bootctl");
	kb_argv_add(&a, "status");
	kb_argv_add(&a, "--json");
	kb_argv_end(&a);

	KbBuf out = {0};
	int rc = kb_run_capture_buf(&a, &out);
	L->live = out.p && strstr(out.p, "\"live\": true") != NULL;
	if (rc != 0 || !out.p || !strstr(out.p, "\"configured\": true")) {
		kb_buf_free(&out);
		return 0;
	}
	int active_b = strstr(out.p, "\"active\": \"b\"") != NULL;
	kb_strlcpy(L->slot, active_b ? "a" : "b", sizeof(L->slot));
	json_field(out.p, active_b ? "slot_a" : "slot_b", L->uuid,
		   sizeof(L->uuid));
	json_field(out.p, active_b ? "crypt_a" : "crypt_b", L->crypt,
		   sizeof(L->crypt));
	json_field(out.p, "running", L->running, sizeof(L->running));
	json_field(out.p, "trying", L->trying, sizeof(L->trying));
	kb_buf_free(&out);
	return L->uuid[0] != 0;
}

/*
 * AN ARMED SLOT IS DISARMED BEFORE ITS FIRST BYTE IS REWRITTEN. A trial armed
 * by an earlier Install still points the next boot at that slot; a restart
 * while this run is filling it would boot a root that is half one update and
 * half the next. `kdos-bootctl cancel` first, so a power cut mid-fill comes up
 * on the confirmed slot, and the arm at the end of this run arms it again.
 * 0 when the slot is not armed or no longer is.
 */
static int slot_disarm(const Layout *L)
{
	if (!L->trying[0] || strcmp(L->trying, L->slot))
		return 0;
	KbArgv a = {0};
	kb_argv_add(&a, "kdos-bootctl");
	kb_argv_add(&a, "cancel");
	kb_argv_end(&a);
	return kb_run_tty(&a) == 0 ? 0 : -1;
}

/*
 * `--mount`: the inactive slot at /run/kdos/slot-<x>, for this run only. A
 * slot inside a LUKS container nobody opened has no filesystem to mount, and
 * is said so rather than guessed at: unlocking takes the passphrase, and no
 * daemon has one. NULL with the reason in `why` otherwise.
 */
static char *slot_mount(const Layout *L, char *why, size_t wcap)
{
	char dev[160], dir[64];
	struct stat st;

	snprintf(dev, sizeof(dev), "/dev/disk/by-uuid/%s", L->uuid);
	if (stat(dev, &st) != 0) {
		if (L->crypt[0])
			snprintf(why, wcap, "slot %s is inside LUKS container "
				 "%s, which is not open", L->slot, L->crypt);
		else
			snprintf(why, wcap, "slot %s's filesystem (%s) is not "
				 "on this machine", L->slot, L->uuid);
		return NULL;
	}
	snprintf(dir, sizeof(dir), SLOT_MNT "%s", L->slot);
	kb_mkdir_p("/run/kdos");
	if (mkdir(dir, 0700) != 0 && errno != EEXIST) {
		snprintf(why, wcap, "cannot create %s: %s", dir,
			 strerror(errno));
		return NULL;
	}
	KbArgv a = {0};
	kb_argv_add(&a, "mount");
	kb_argv_add(&a, dev);
	kb_argv_add(&a, dir);
	kb_argv_end(&a);
	if (kb_run_tty(&a) != 0) {
		rmdir(dir);
		snprintf(why, wcap, "slot %s (%s) could not be mounted",
			 L->slot, L->uuid);
		return NULL;
	}
	return kb_strdup(dir);
}

static void slot_unmount(const char *dir)
{
	if (umount2(dir, 0) != 0)
		umount2(dir, MNT_DETACH);
	rmdir(dir);
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

/*
 * `<root>/etc/kdos-release`: the version this root now is, which
 * kdos-sysupdated's GetVersion reports and `kdos-bootctl deploy` copies onto
 * the ESP for the System Versions page and the logout screen. Written when
 * the root holds every package of the tree, or every file of the image, it
 * was filled from — never before.
 */
static int release_fields(const char *mnt, const char *ver, const char *commit,
			  long long t, const char *how)
{
	char p[1024], day[16] = "";
	time_t tt = (time_t)t;
	struct tm tm;
	if (t > 0 && gmtime_r(&tt, &tm))
		strftime(day, sizeof(day), "%Y-%m-%d", &tm);
	KbBuf b = {0};
	kb_buf_printf(&b,
		      "# The KDOS release this root holds. Written by the image "
		      "build and by\n"
		      "# `kdos update` when it fills this root; read by "
		      "kdos-sysupdated and\n"
		      "# kdos-bootctl.\n"
		      "VERSION=\"%s\"\n"
		      "BUILD_ID=\"%.12s\"\n"
		      "BUILD_DATE=\"%s\"\n"
		      "TREE=\"%s\"\n"
		      "TREE_TIME=\"%lld\"\n"
		      "INSTALLED_BY=\"%s\"\n",
		      ver, commit, day, commit, t, how);
	snprintf(p, sizeof(p), "%s%s", strcmp(mnt, "/") ? mnt : "",
		 RELEASE_FILE);
	int rc = kb_write_file_atomic(p, b.p);
	kb_buf_free(&b);
	return rc;
}

/* The record for a root filled from the ports tree `tree`. */
static int release_write(const char *mnt, const char *tree, const char *how)
{
	char ver[64], commit[64];
	long long t = 0;

	tree_version(tree, ver, sizeof(ver), NULL);
	if (tree_read(tree, commit, sizeof(commit), &t) != 0)
		return -1;
	return release_fields(mnt, ver, commit, t, how);
}

/* ── apply ─────────────────────────────────────────────────────────────── */

/*
 * AN A/B INSTALL RUNS INSIDE THE SLOT. `kpkg install` with KPKG_ROOT set
 * installs into the root it names but compiles against the one it runs in,
 * so a package built after its dependency was upgraded in the slot would link
 * the running root's old copy, and nothing would rebuild it; and `kpkg
 * binhost` takes its build-config hash from the compiler it finds, which must
 * be the slot's. chroot()ing into the slot puts the slot's own compiler,
 * headers and libraries under every step.
 *
 * What the slot cannot supply is bound in: the kernel's filesystems, the
 * ports tree that names what to install (the slot holds the tree it was last
 * built from until tree_into_slot replaces it), the source cache a source
 * build fetched into, the binhost cache a binhost install reads, and
 * resolv.conf for a dependency whose source is fetched on the way. Each mount
 * is undone, in reverse, before the slot is checked.
 */
#define AB_MAXBIND 8
static char ab_bound[AB_MAXBIND][768];
static int ab_nbound;

static int ab_bind(const char *mnt, const char *src, const char *fstype,
		   int file)
{
	char dst[768];

	if (ab_nbound == AB_MAXBIND)
		return -1;
	snprintf(dst, sizeof(dst), "%s%s", mnt, src);
	if (file) {
		/* A regular file in the slot, never a link: a mount follows a
		 * symlink, and the slot's absolute link would be resolved on
		 * the running root. */
		struct stat st;
		if (!kb_path_exists(src) || lstat(dst, &st) != 0 ||
		    !S_ISREG(st.st_mode))
			return 0;
	} else {
		kb_mkdir_p(dst);
	}
	if (fstype ? mount(fstype, dst, fstype, 0, NULL)
		   : mount(src, dst, NULL, MS_BIND | MS_REC, NULL)) {
		fprintf(stderr, "kdos update: cannot mount %s on %s: %s\n",
			src, dst, strerror(errno));
		return -1;
	}
	kb_strlcpy(ab_bound[ab_nbound++], dst, sizeof(ab_bound[0]));
	return 0;
}

static void ab_unbind(void)
{
	while (ab_nbound > 0)
		umount2(ab_bound[--ab_nbound], MNT_DETACH);
}

/* What a step in the slot reads besides the kernel's filesystems and the
 * tree: the binhost cache a prebuilt install takes its files from, the source
 * cache a build compiles from, or both for a set that mixes the two. */
enum { AB_CACHE = 1, AB_SOURCES = 2 };

static int ab_enter(const char *mnt, const char *tree, const KpConf *c,
		    int binds)
{
	if (ab_bind(mnt, "/proc", "proc", 0) || ab_bind(mnt, "/sys", NULL, 0) ||
	    ab_bind(mnt, "/dev", NULL, 0) || ab_bind(mnt, tree, NULL, 0) ||
	    ((binds & AB_CACHE) && ab_bind(mnt, cache_dir(), NULL, 0)) ||
	    ((binds & AB_SOURCES) && ab_bind(mnt, c->source_dir, NULL, 0)) ||
	    ab_bind(mnt, "/etc/resolv.conf", NULL, 1)) {
		ab_unbind();
		return -1;
	}
	return 0;
}

/* The slot a step chroot()s into, or NULL to run in the running root. */
static const char *ab_chroot;

static int run_kpkg(const char *verb, const char *pkg)
{
	KbArgv a = {0};
	if (ab_chroot) {
		kb_argv_add(&a, "chroot");
		kb_argv_add(&a, ab_chroot);
	}
	kb_argv_add(&a, "kpkg");
	kb_argv_add(&a, verb);
	if (!strcmp(verb, "install"))
		kb_argv_add(&a, "-f");
	kb_argv_add(&a, pkg);
	kb_argv_end(&a);
	return kb_run_tty(&a);
}

/* `kpkg binhost [--root R] <cache> <pkg>`: inside the slot's chroot for an A/B
 * install, into `root` otherwise. */
static int run_binhost(const char *root, const char *pkg)
{
	KbArgv a = {0};
	if (ab_chroot) {
		kb_argv_add(&a, "chroot");
		kb_argv_add(&a, ab_chroot);
	}
	kb_argv_add(&a, "kpkg");
	kb_argv_add(&a, "binhost");
	if (!ab_chroot && root && strcmp(root, "/")) {
		kb_argv_add(&a, "--root");
		kb_argv_add(&a, root);
	}
	kb_argv_add(&a, cache_dir());
	kb_argv_add(&a, pkg);
	kb_argv_end(&a);
	return kb_run_tty(&a);
}

/* `a, b, c` and `…` past what fits, for a state line a notice shows. */
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

/* Everything one apply holds, so every way out of it unwinds the same. */
typedef struct {
	int dry, in_place, source, acquire, arm, mount;
	int want_image, want_binhost;	/* --image, --binhost */
	KdtImage *img;		/* the image lane's verified manifest */
	int from_image;		/* the slot holds an image, not packages */
	char itree[64];		/* that image's tree and time */
	long long itime;
	const char *root;	/* --root, or the slot's mountpoint */
	char *mnt;		/* the slot's mountpoint, when A/B */
	int mounted;		/* this run mounted it */
	int ab;			/* installing into the inactive slot */
	Layout L;
	char tree[512];		/* the ports tree's root */
	char version[64];	/* the tree's version */
	int lock;
	KpConf tconf;
	const KpConf *run;	/* the running root's */
} Apply;

/*
 * A SLOT WITH NOTHING BEHIND MAY STILL BE THE UPDATE. A slot an earlier run
 * filled, whose arm never happened, holds every package of the tree while the
 * running root is still behind it: that slot is the candidate, and is checked
 * and armed as one. Only when the running root is current too is there
 * nothing to take.
 */
static int slot_ahead(const Apply *ap)
{
	Behind *v = NULL;
	int orphans = 0;
	int n;

	if (!ap->ab || ap->dry)
		return 0;
	n = survey(ap->run, &v, &orphans);
	free(v);
	return n > 0;
}

/* Nothing to take: said to a person, recorded, and — to a program waiting to
 * reply to a request — a refusal, because there is no update to start. */
static int nothing_behind(Apply *ap);

static void apply_end(Apply *ap)
{
	kdt_image_free(ap->img);
	ap->img = NULL;
	ab_chroot = NULL;
	ab_unbind();
	if (ap->mounted && ap->mnt)
		slot_unmount(ap->mnt);
	free(ap->mnt);
	ap->mnt = NULL;
	if (ap->lock >= 0)
		close(ap->lock);
	ap->lock = -1;
}

/* A refusal: nothing was written, and the reason is the sentence a person
 * reads — on the terminal, in update.state and in Discover's banner. */
static int refuse(Apply *ap, const char *code, const char *fmt, ...)
	__attribute__((format(printf, 3, 4)));
static int refuse(Apply *ap, const char *code, const char *fmt, ...)
{
	char msg[512];
	va_list va;
	va_start(va, fmt);
	vsnprintf(msg, sizeof(msg), fmt, va);
	va_end(va);
	fprintf(stderr, "kdos update: %s\n", msg);
	prog("refused %s", msg);
	if (!ap->dry && code)
		state_set("apply", code, msg);
	apply_end(ap);
	if (!ap->dry && code)
		doc_refresh();
	return AP_REFUSED;
}

static int fail(Apply *ap, const char *code, const char *why)
{
	fprintf(stderr, "kdos update: %s%s\n", why,
		ap->ab ? " — the slot will not be booted" : "");
	prog("error %s", why);
	state_set("apply", code, why);
	apply_end(ap);
	doc_refresh();
	return AP_FAIL;
}

static int cancelled(Apply *ap)
{
	fprintf(stderr, "kdos update: stopped between packages — the slot "
			"will not be booted\n");
	prog("error cancelled");
	state_set("apply", "cancelled", "stopped between packages");
	apply_end(ap);
	doc_refresh();
	return AP_CANCELLED;
}

static int nothing_behind(Apply *ap)
{
	printf("nothing to take: everything installed matches the ports "
	       "tree\n");
	prog("refused nothing is behind: every package matches the ports "
	     "tree");
	if (!ap->dry)
		state_set("apply", "ok", "nothing behind");
	apply_end(ap);
	return AP_OK;
}

/*
 * THE TARGET: the inactive slot when the machine has one, the root --root
 * names, or the running root. An inactive slot exists to receive an update
 * WITHOUT touching the system you are running, so installing in place on a
 * machine that has one throws away the rollback it was set up for; it is
 * refused rather than silently done, and --in-place is the way to say you
 * meant it. The live medium is refused the same way: its root is an overlay
 * in RAM, and what an update installs there is gone at the next restart
 * unless a persistence store holds it.
 */
static int apply_target(Apply *ap, const KpConf *c)
{
	int layout = 0;
	char why[256];

	if (!ap->root && !ap->in_place && (layout = bootctl_layout(&ap->L))) {
		/* On a trial boot the slot that is not confirmed is the one
		 * running, and the confirmed one is the way back: neither may
		 * be written until the trial is confirmed or rolled back. */
		if (!strcmp(ap->L.running, ap->L.slot))
			return refuse(ap, NULL, "this boot runs slot %s on trial; "
				      "an update is installed once it is "
				      "confirmed or rolled back", ap->L.slot);
		ap->mnt = slot_mountpoint(ap->L.uuid);
		if (!ap->mnt && ap->mount && !ap->dry) {
			ap->mnt = slot_mount(&ap->L, why, sizeof(why));
			if (!ap->mnt)
				return refuse(ap, "build-failed", "%s", why);
			ap->mounted = 1;
		}
		if (!ap->mnt)
			return refuse(ap, NULL,
				      "this machine has A/B root slots and slot "
				      "%s (%s) is not mounted, so there is "
				      "nowhere to install the candidate. Mount "
				      "it, pass --mount, or pass --in-place to "
				      "update the running root and give up the "
				      "rollback", ap->L.slot, ap->L.uuid);
		ap->root = ap->mnt;
		ap->ab = 1;
		printf("%sA/B:%s installing into slot %s at %s\n", A, O,
		       ap->L.slot, ap->mnt);
	} else if (!ap->root && !layout) {
		if (!ap->in_place && ap->L.live)
			return refuse(ap, NULL, "this is the live medium: what "
				      "an update installs here is gone at the "
				      "next restart unless a persistence store "
				      "keeps it. Install KDOS to update it, or "
				      "pass --in-place to update this session "
				      "anyway");
		printf("%sno A/B slots — updating the running root in "
		       "place%s\n", D, O);
	}
	if ((ap->acquire || ap->arm) && !ap->ab)
		return refuse(ap, NULL, "--acquire and --arm fill and arm the "
			      "inactive A/B slot, and this run has none");
	ap->run = c;
	kp_conf_load(&ap->tconf);
	if (ap->root) {
		/* Re-read with KPKG_ROOT set, because the survey walks the
		 * TARGET's database: the conf loaded at startup describes the
		 * running root, which after one A/B cycle is a different set
		 * of packages. */
		setenv("KPKG_ROOT", ap->root, 1);
		kp_conf_load(&ap->tconf);
	}
	return 0;
}

/* Places set[i] after every member of the set it depends on. `st` is 0 for
 * unseen, 1 on the current path and 2 placed; a name met again while on the
 * path closes a cycle, which is cut there, as kp_resolve cuts it. A
 * dependency outside the set is one the target holds current and orders
 * nothing. */
static void wl_place(const KpConf *c, char **set, int ns, char *st, int i,
		     char **order, int *k)
{
	if (st[i])
		return;
	st[i] = 1;
	char *pd = kp_port_dir(c, set[i]);
	if (pd) {
		char (*deps)[128] = kb_calloc(KP_MAX_DEPS, sizeof(*deps));
		int nd = kp_depends(pd, deps, KP_MAX_DEPS);
		free(pd);
		for (int d = 0; d < nd; d++)
			for (int j = 0; j < ns; j++)
				if (!strcmp(set[j], deps[d])) {
					wl_place(c, set, ns, st, j, order, k);
					break;
				}
		free(deps);
	}
	st[i] = 2;
	order[(*k)++] = set[i];
}

/*
 * THE WORKLIST, in dependency order: every package behind in the target, every
 * world package the target lacks, and every dependency of either that the
 * target does not hold at all, each after what it depends on, so nothing is
 * installed before a dependency it was built against.
 *
 * kp_resolve finds the missing dependencies but CANNOT be the list: it passes
 * over every name the target has an entry for, and a package that is behind
 * has one, so a list taken from it holds nothing that is merely out of date
 * and an update installs nothing. The behind set is added to what it found,
 * and the union is ordered here.
 */
static int worklist(Apply *ap, Behind **out)
{
	Behind *v = NULL;
	int orphans = 0, nw = 0;
	int n = survey(&ap->tconf, &v, &orphans);
	char **world = world_load(&nw);

	if (n < 0) {
		kb_strv_free(world);
		return -1;
	}
	char **names = kb_calloc((size_t)(n + nw + 1), sizeof(*names));
	int m = 0;
	for (int i = 0; i < n; i++)
		names[m++] = v[i].name;
	for (int i = 0; i < nw; i++)
		if (!kp_installed(&ap->tconf, world[i]))
			names[m++] = world[i];
	KpOrder *ord = kb_calloc(1, sizeof(*ord));
	kp_resolve(&ap->tconf, names, m, ord);

	char **set = kb_calloc((size_t)(n + ord->n + 1), sizeof(*set));
	int ns = 0;
	for (int i = 0; i < n; i++)
		set[ns++] = v[i].name;
	for (int i = 0; i < ord->n; i++) {
		int j;
		for (j = 0; j < ns && strcmp(set[j], ord->order[i]); j++)
			;
		if (j == ns)
			set[ns++] = ord->order[i];
	}
	char *st = kb_calloc((size_t)ns + 1, 1);
	char **order = kb_calloc((size_t)ns + 1, sizeof(*order));
	int no = 0;
	for (int i = 0; i < ns; i++)
		wl_place(&ap->tconf, set, ns, st, i, order, &no);

	Behind *w = kb_calloc((size_t)(no + 1), sizeof(*w));
	int k = 0;
	for (int i = 0; i < no; i++) {
		const char *nm = order[i];
		int j;
		for (j = 0; j < n && strcmp(v[j].name, nm); j++)
			;
		if (j < n) {
			w[k] = v[j];
		} else {
			char *pd = kp_port_dir(&ap->tconf, nm);
			if (!pd)
				continue;	/* not a port: nothing to install */
			memset(&w[k], 0, sizeof(w[k]));
			kb_strlcpy(w[k].name, nm, sizeof(w[k].name));
			kb_strlcpy(w[k].have, "-", sizeof(w[k].have));
			kp_recipe_key(pd, "version", w[k].want,
				      sizeof(w[k].want));
			kp_recipe_key(pd, "release", w[k].want_rel,
				      sizeof(w[k].want_rel));
			free(pd);
		}
		w[k].stanza = -1;
		k++;
	}
	free(order);
	free(st);
	free(set);
	free(ord);
	free(names);
	free(v);
	kb_strv_free(world);
	*out = w;
	return k;
}

/* `<cache>/<file>` from where the release index says `binhost/<file>` is,
 * kept only with the index's size and hash: KDT_GOT_*. */
static int bh_take(const BhIndex *ix, const char *file, const char *dst,
		   char *why, size_t wcap)
{
	char path[300];
	snprintf(path, sizeof(path), "binhost/%s", file);
	return kdt_relidx_take(ix->rel, path, dst, why, wcap);
}

/*
 * A DELTA BEFORE THE PACKAGE, when the index names one from a package file
 * the cache still holds — the previous release's, which cache_prune keeps
 * while a delta names it. The delta is checked against its own hash in the
 * signed index and in the release index, and `kpkg apply-delta --expect`
 * checks the rebuilt package against the package's, so neither is trusted;
 * any failure leaves the whole package to download. 0 when the package is in
 * the cache with its sidecar.
 */
static int fetch_delta(const BhIndex *ix, const BhStanza *s)
{
	for (int k = 0; k < ix->n; k++) {
		const BhStanza *d = &ix->st[k];
		char old[1024], dp[1024], out[1100], dst[1024], sig[1100];
		char got[65] = "", why[256], sname[260];

		if (!d->from[0] || strcmp(d->name, s->name) ||
		    strcmp(d->version, s->version) ||
		    strcmp(d->release, s->release) || strcmp(d->arch, s->arch) ||
		    !file_name_ok(d->file) || !file_name_ok(d->from) ||
		    strlen(d->sha) != 64)
			continue;
		snprintf(old, sizeof(old), "%s/%s", cache_dir(), d->from);
		if (!kb_path_exists(old))
			continue;
		snprintf(dp, sizeof(dp), "%s/%s", cache_dir(), d->file);
		if (bh_take(ix, d->file, dp, why, sizeof(why)) != KDT_GOT_OK)
			continue;
		if (kb_sha256_file(dp, got) != 0 || strcmp(got, d->sha)) {
			unlink(dp);
			continue;
		}
		snprintf(dst, sizeof(dst), "%s/%s", cache_dir(), s->file);
		snprintf(out, sizeof(out), "%s.part", dst);
		KbArgv a = {0};
		kb_argv_add(&a, "kpkg");
		kb_argv_add(&a, "apply-delta");
		kb_argv_add(&a, old);
		kb_argv_add(&a, dp);
		kb_argv_add(&a, "-o");
		kb_argv_add(&a, out);
		kb_argv_add(&a, "--expect");
		kb_argv_add(&a, s->sha);
		kb_argv_end(&a);
		int rc = kb_run(&a);
		unlink(dp);
		if (rc != 0 || kb_sha256_file(out, got) != 0 ||
		    strcmp(got, s->sha) || rename(out, dst) != 0) {
			unlink(out);
			continue;
		}
		snprintf(sig, sizeof(sig), "%s.sig", dst);
		snprintf(sname, sizeof(sname), "%s.sig", s->file);
		if (bh_take(ix, sname, sig, why, sizeof(why)) != KDT_GOT_OK)
			return -1;
		return 0;
	}
	return -1;
}

/* One package file and its signature, into the cache, each from where the
 * release index says and kept only with its hash there; the package is
 * checked against the binhost index's hash too. A file that does not match
 * is deleted, not left for `kpkg binhost` to refuse later. */
static int fetch_package(const BhIndex *ix, const BhStanza *s, char *why,
			 size_t wcap)
{
	char dst[1024], sig[1100], sha[65] = "", sname[260];

	if (bh_cached(s)) {
		snprintf(sig, sizeof(sig), "%s/%s.sig", cache_dir(), s->file);
		if (kb_path_exists(sig))
			return 0;
	}
	if (!ix->rel) {
		snprintf(why, wcap, "no signed release index says where %s is",
			 s->file);
		return -1;
	}
	if (fetch_delta(ix, s) == 0)
		return 0;
	snprintf(dst, sizeof(dst), "%s/%s", cache_dir(), s->file);
	snprintf(sig, sizeof(sig), "%s.sig", dst);
	snprintf(sname, sizeof(sname), "%s.sig", s->file);
	int got = bh_take(ix, sname, sig, why, wcap);
	if (got == KDT_GOT_OK)
		got = bh_take(ix, s->file, dst, why, wcap);
	if (got != KDT_GOT_OK) {
		unlink(sig);
		return got == KDT_GOT_OFFLINE ? -2 : -1;
	}
	if (kb_sha256_file(dst, sha) != 0 || strcmp(sha, s->sha)) {
		unlink(dst);
		unlink(sig);
		snprintf(why, wcap, "%s does not match the signed index",
			 s->file);
		return -1;
	}
	return 0;
}

/* The host keyring, copied into the cache the slot's kpkg reads: the running
 * system decides which keys it trusts, and binding its /etc/kdos/keys into
 * the slot would let an install there write into this system's keyring. */
static int keyring_into_cache(void)
{
	char dst[1024];
	int n = 0;

	snprintf(dst, sizeof(dst), "%s/keys", cache_dir());
	kb_rmtree(dst);
	if (kb_mkdir_p(dst) != 0)
		return -1;
	char **names = kb_listdir(keyring_dir(), &n);
	for (int i = 0; names && i < n; i++) {
		size_t l = strlen(names[i]);
		if (l < 5 || strcmp(names[i] + l - 4, ".pub"))
			continue;
		char *s = kb_path_join(keyring_dir(), names[i]);
		char *d = kb_path_join(dst, names[i]);
		if (!kb_is_dir(s))
			kb_copy_file(s, d);
		free(s);
		free(d);
	}
	kb_strv_free(names);
	return 0;
}

/*
 * A SLOT WRITTEN BY PACKAGES NO LONGER HOLDS AN IMAGE. The record an earlier
 * image fill left would name a release the slot is no longer exactly, and an
 * arm would take the slot for that image; it goes before the first package
 * is written.
 */
static int slot_forget_image(const Apply *ap)
{
	char p[1024];
	snprintf(p, sizeof(p), "%s%s", ap->root, IMAGE_RECORD);
	return unlink(p) == 0 || errno == ENOENT ? 0 : -1;
}

static void image_tick(int done, int total, void *data)
{
	(void)data;
	prog("item %d %d image", done, total);
}

static int bh_fill(Apply *ap, int after_image);

/*
 * THE WORLD SET AFTER AN IMAGE. A slot image is the base as released and
 * carries none of the packages somebody installed on purpose, so the image
 * lane installs them into the slot it wrote, from the same release's
 * binhost, or builds the ones whose origin is `source`. Whether it can is
 * decided BEFORE the image is written: every world package and every
 * dependency of one, walked through the tree's recipes, must be built here
 * or covered by the signed index, or the image lane is refused with the
 * slot untouched. The walk checks dependencies the image carries as well,
 * which a release's binhost covers like every other package it built. The
 * names of the first few not covered go in `list`; the count is returned.
 */
static int world_uncovered(Apply *ap, char *list, size_t lcap)
{
	int nw = 0;
	char **world = world_load(&nw);
	list[0] = 0;
	if (!nw) {
		kb_strv_free(world);
		return 0;
	}
	BhIndex ix;
	char why[256];
	int have_ix = bh_load(&ix, why, sizeof(why)) == 0;
	build_env_for(NULL);
	char (*seen)[128] = kb_calloc(KP_MAX_ORDER, sizeof(*seen));
	int ns = 0, head = 0, nbad = 0;
	const char *bad[KP_MAX_ORDER] = { 0 };
	for (int i = 0; i < nw && ns < KP_MAX_ORDER; i++)
		kb_strlcpy(seen[ns++], world[i], sizeof(seen[0]));
	while (head < ns) {
		const char *nm = seen[head++];
		char *pd = kp_port_dir(&ap->tconf, nm);
		if (!pd)
			continue;
		char (*deps)[128] = kb_calloc(KP_MAX_DEPS, sizeof(*deps));
		int nd = kp_depends(pd, deps, KP_MAX_DEPS);
		free(pd);
		for (int d = 0; d < nd && ns < KP_MAX_ORDER; d++) {
			int j;
			for (j = 0; j < ns && strcmp(seen[j], deps[d]); j++)
				;
			if (j == ns)
				kb_strlcpy(seen[ns++], deps[d], sizeof(seen[0]));
		}
		free(deps);
		if (origin_of(nm) == ROUTE_SOURCE)
			continue;
		char w[96];
		if (!have_ix || bh_match(&ap->tconf, &ix, nm, w, sizeof(w)) < 0)
			bad[nbad++] = nm;
	}
	name_list(list, lcap, bad, nbad);
	free(seen);
	if (have_ix)
		bh_free(&ix);
	kb_strv_free(world);
	return nbad;
}

/*
 * WHAT THIS MACHINE KEEPS WHOLE stays whole in a slot an update fills.
 * Under FACETS=run every install, and the image's strip, leaves a `.facets`
 * record, empty when nothing was left out (kpkg.h), so an installed package
 * with none was installed whole on purpose: by `kdos update install --dev`,
 * by a build here, or as a build tool a build installed. A slot image carries
 * the base with its records and the world step installs under the slot's
 * FACETS=run, so without this list the slot would lose every development
 * file such a package brought back. The list is the running root's database
 * itself, never a file beside it: a slot this step filled says the same to
 * the next update, and a running root under FACETS=all, where nothing is
 * left out, has no list. A package whose manifest holds only run-time files
 * (a build here of a program with no headers) is not on it: installing it
 * again whole would change nothing.
 */
static int has_facets(const KpConf *c, const char *name)
{
	char *db = kp_db_dir(c);
	char *ep = kb_path_join(db, name);
	char *man = kb_read_all(ep, NULL);
	free(ep);
	free(db);
	if (!man)
		return 0;
	char globs[512] = "";
	char *pd = kp_port_dir(c, name);
	if (pd) {
		kp_recipe_key(pd, "facet_dev", globs, sizeof(globs));
		free(pd);
	}
	const char *root = c->root[0] ? c->root : sys_root();
	int found = 0;
	char *l = strchr(man, '\n');	/* line 1 is the version */
	for (l = l ? l + 1 : NULL; l && *l && !found;) {
		char *nl = strchr(l, '\n');
		if (nl)
			*nl = 0;
		char fp[4200], link[4096];
		const char *rel = !strncmp(l, "./", 2) ? l + 2 : l;
		snprintf(fp, sizeof(fp), "%s/%s", strcmp(root, "/") ? root : "",
			 rel);
		ssize_t k = readlink(fp, link, sizeof(link) - 1);
		if (k >= 0)
			link[k] = 0;
		found = kp_facet_of(l, k >= 0 ? link : NULL) != KP_FACET_RUN ||
			(globs[0] && kp_facet_glob(l, globs));
		l = nl ? nl + 1 : NULL;
	}
	free(man);
	return found;
}

/* The packages the running root keeps whole, NULL-terminated and sorted;
 * NULL with *n = 0 when there are none. */
static char **whole_load(const KpConf *run, int *n)
{
	*n = 0;
	if (!run || !run->facets_run)
		return NULL;
	char *db = kp_db_dir(run);
	int nn = 0;
	char **names = kb_listdir(db, &nn);
	free(db);
	char **v = NULL;
	for (int i = 0; names && i < nn; i++) {
		if (names[i][0] == '.' || !pkg_name_ok(names[i]) ||
		    !kp_installed(run, names[i]))
			continue;
		char *rec = kp_installed_facets(run, names[i], NULL);
		if (rec) {
			free(rec);
			continue;
		}
		if (!has_facets(run, names[i]))
			continue;
		v = kb_realloc(v, (size_t)(*n + 2) * sizeof(*v));
		v[(*n)++] = kb_strdup(names[i]);
		v[*n] = NULL;
	}
	kb_strv_free(names);
	return v;
}

/* 1 when `name` is in the NULL-terminated `v`. */
static int strv_has(char *const *v, const char *name)
{
	for (int i = 0; v && v[i]; i++)
		if (!strcmp(v[i], name))
			return 1;
	return 0;
}

/*
 * Whether the release can bring back what the running root keeps whole,
 * decided before the slot image is written, as world_uncovered decides the
 * world set: a package built here is built here again, any other must be
 * covered by the signed index. `print` writes the dry run's line per
 * package. The names of the first few not covered go in `list`; the count
 * is returned.
 */
static int whole_uncovered(Apply *ap, int print, char *list, size_t lcap)
{
	int nwh = 0;
	char **whole = whole_load(ap->run, &nwh);
	list[0] = 0;
	if (!nwh)
		return 0;
	BhIndex ix;
	char why[256];
	int have_ix = bh_load(&ix, why, sizeof(why)) == 0;
	build_env_for(NULL);
	const char **bad = kb_calloc((size_t)nwh + 1, sizeof(*bad));
	int nbad = 0;
	for (int i = 0; i < nwh; i++) {
		char w[96] = "not in the binhost";
		int src = origin_of(whole[i]) == ROUTE_SOURCE;
		int st = src || !have_ix ? -1
					 : bh_match(&ap->tconf, &ix, whole[i], w,
						    sizeof(w));
		if (!src && st < 0)
			bad[nbad++] = whole[i];
		if (print)
			printf("  %-24s kept whole: %s\n", whole[i],
			       src ? "built here again"
			       : st >= 0 ? ix.st[st].file
			       : have_ix ? w : "no binhost index here");
	}
	name_list(list, lcap, bad, nbad);
	free(bad);
	if (have_ix)
		bh_free(&ix);
	kb_strv_free(whole);
	return nbad;
}

/*
 * THE IMAGE LANE. The manifest was verified when it was cached and again
 * when image_offer read it; the slot is disarmed before its first byte is
 * written, then synchronised with the image (image.c): files the slot or the
 * running root already holds are copied, the rest come from the delta for
 * the version this system was written from, then from the full image. The
 * `item` lines count files in place.
 */
static int fill_image(Apply *ap)
{
	char why[512], dir[1024], from[64];
	int files = kdt_image_files(ap->img);

	if (!ap->ab && (!ap->root || ap->in_place))
		return refuse(ap, NULL, "a slot image is written into the "
			      "inactive A/B slot, and this run has none; `kdos "
			      "update apply --binhost` installs into the "
			      "running root");
	image_dir(dir, sizeof(dir));
	KdtRelIdx *rel = kdt_relidx_load(dir, keyring_dir(), why, sizeof(why));
	if (!rel)
		return refuse(ap, "unverified", "%s, so the slot image has no "
			      "place to download it from; `kdos update fetch` "
			      "fetches it again", why);
	char wlist[200];
	int wbad = world_uncovered(ap, wlist, sizeof(wlist));
	if (wbad) {
		kdt_relidx_free(rel);
		return refuse(ap, "needs-source", "%d package%s of what was "
			      "installed on purpose %s not in release %s's "
			      "binhost (%s), and the slot image does not carry "
			      "%s; nothing was written", wbad,
			      wbad == 1 ? "" : "s", wbad == 1 ? "is" : "are",
			      ap->version, wlist, wbad == 1 ? "it" : "them");
	}
	int hbad = whole_uncovered(ap, 0, wlist, sizeof(wlist));
	if (hbad) {
		kdt_relidx_free(rel);
		return refuse(ap, "needs-source", "%d package%s this system "
			      "keeps whole %s not in release %s's binhost "
			      "(%s), and the slot image carries %s without "
			      "%s development files; nothing was written",
			      hbad, hbad == 1 ? "" : "s",
			      hbad == 1 ? "is" : "are", ap->version, wlist,
			      hbad == 1 ? "it" : "them",
			      hbad == 1 ? "its" : "their");
	}
	image_installed(sys_root(), from, sizeof(from));
	long long bytes = kdt_image_download_size(ap->img, from);
	int delta = 0;
	if (from[0] && bytes != kdt_image_download_size(ap->img, NULL))
		delta = 1;
	if (ap->dry) {
		kdt_relidx_free(rel);
		printf("slot image %s, signed by %s (key %s): %d files, at "
		       "most %s to download (%s%s)\n", ap->version,
		       kdt_image_signer_name(ap->img)[0]
			       ? kdt_image_signer_name(ap->img) : "a trusted key",
		       kdt_image_signer(ap->img), files,
		       kb_human_size((unsigned long long)bytes),
		       delta ? "the delta from " : "the full image",
		       delta ? from : "");
		whole_uncovered(ap, 1, wlist, sizeof(wlist));
		apply_end(ap);
		return AP_OK;
	}
	prog("verified %s %s %d %lld", kdt_image_signer(ap->img), ap->version,
	     files, bytes);
	printf("%s==>%s slot image %s (signed by key %s), at most %s to "
	       "download\n", A, O, ap->version, kdt_image_signer(ap->img),
	       kb_human_size((unsigned long long)bytes));
	if (ap->ab && slot_disarm(&ap->L) != 0) {
		kdt_relidx_free(rel);
		snprintf(why, sizeof(why), "slot %s is armed for the next boot "
			 "and could not be disarmed", ap->L.slot);
		return fail(ap, "build-failed", why);
	}
	int frc = kdt_image_fill(ap->img, rel, ap->root, sys_root(), image_tick,
				 NULL, &g_stop, why, sizeof(why));
	kdt_relidx_free(rel);
	if (frc != 0) {
		if (g_stop)
			return cancelled(ap);
		return fail(ap, strstr(why, "could not be read") ? "fetch-failed"
								 : "build-failed",
			    why);
	}
	int nw = 0, nwh = 0;
	char **world = world_load(&nw);
	kb_strv_free(world);
	char **whole = whole_load(ap->run, &nwh);
	kb_strv_free(whole);
	return nw || nwh ? bh_fill(ap, 1) : AP_OK;
}

/*
 * THE BINHOST LANE, AND THE WORLD STEP OF THE IMAGE LANE. Each item of the
 * worklist takes the route its origin names (origin_of): `prebuilt` from the
 * signed index, `source` built in the target from its recipe. Every item must
 * be covered before anything is written — a prebuilt one by the index, a
 * source one by its sources, fetched first — and a prebuilt one the index
 * does not cover refuses the whole run with how many need a source build, the
 * slot left as it was. Then every file is downloaded and checked, and only
 * then is the first package installed, in dependency order, each by its
 * route, so an apply that loses the network loses it before the slot holds
 * half an update. A package the run adds records its route in the origin
 * file. `item` lines count both halves.
 *
 * INTO A SLOT, WHAT THE RUNNING ROOT KEEPS WHOLE (whole_load) STAYS WHOLE: a
 * package of the worklist on that list is installed under KPKG_FACETS=all,
 * and one the slot holds without its development files is installed again
 * whole after the worklist (dev_restore), its file fetched with the rest.
 *
 * after_image: the slot was just written from a slot image, the base as the
 * release built it, and what goes in now is the world set the image does not
 * carry and what this machine keeps whole. The image record stays, since the
 * base is still exactly that image, and an empty worklist is success.
 */
/* ── facets: what a build here compiles against ───────────────────────── */

/*
 * A MACHINE CARRIES THE RUN-TIME FACET OF ITS PACKAGES: the image's
 * kpkg.conf says FACETS=run, so headers, static libraries, pkg-config and
 * CMake files and the manuals are left out at install (kp_facet_of) and
 * listed in `<db>/.facets/<name>`. A BUILD HERE needs them back first, and
 * the build tools, which the base does not carry. Before the first source build of a run, every installed package
 * the build compiles against whose development facet is left out
 * (kp_facet_wanted: the declared closure, musl, linux and gcc) is installed
 * again
 * from the binhost with KPKG_FACETS=all, and every member of the
 * `build-tools` group that is not installed is installed prebuilt; a package of this machine's own
 * build (origin `source`) is built again instead. Everything such a run
 * installs, it installs whole, so a package built later in the run finds what
 * an earlier one placed. `kdos update install --dev` is the same restore,
 * asked for by name.
 */
enum { DEV_RESTORE, DEV_TOOL, DEV_REBUILD };

typedef struct {
	char name[128];
	int kind;
	int stanza;		/* the binhost entry, or -1 for DEV_REBUILD */
	int world;		/* a build-tools member: joins the world set */
} DevItem;

typedef struct {
	DevItem *it;
	int n, cap;
	int npre, nsrc;
	long long bytes;
	char bad[384];		/* why the plan cannot be carried out */
} DevPlan;

static char **build_tools(const KpConf *c);
static int world_add(const char *name);

static void dev_free(DevPlan *p)
{
	free(p->it);
	memset(p, 0, sizeof(*p));
}

static int dev_has(const DevPlan *p, const char *name)
{
	for (int i = 0; i < p->n; i++)
		if (!strcmp(p->it[i].name, name))
			return 1;
	return 0;
}

static void dev_push(DevPlan *p, const char *name, int kind, int stanza,
		     int world, const BhIndex *ix)
{
	if (p->n == p->cap) {
		p->cap = p->cap ? p->cap * 2 : 16;
		p->it = kb_realloc(p->it, (size_t)p->cap * sizeof(*p->it));
	}
	DevItem *d = &p->it[p->n++];
	memset(d, 0, sizeof(*d));
	kb_strlcpy(d->name, name, sizeof(d->name));
	d->kind = kind;
	d->stanza = stanza;
	d->world = world;
	if (stanza >= 0) {
		p->npre++;
		if (!bh_present(&ix->st[stanza]))
			p->bytes += ix->st[stanza].size;
	} else {
		p->nsrc++;
	}
}

/* `name`'s development facet into the plan when it is installed without
 * it. 0 added or nothing to do, -1 with `p->bad` set when it cannot be. */
static int dev_restore(const KpConf *c, const BhIndex *ix, int have_ix,
		       const char *name, DevPlan *p)
{
	if (dev_has(p, name) || !kp_installed(c, name))
		return 0;
	char *rec = kp_installed_facets(c, name, NULL);
	if (!rec)
		return 0;
	free(rec);
	if (origin_of(name) == ROUTE_SOURCE) {
		dev_push(p, name, DEV_REBUILD, -1, 0, ix);
		return 0;
	}
	char w[96] = "not in the binhost";
	int st = have_ix ? bh_match(c, ix, name, w, sizeof(w)) : -1;
	char iv[64] = "", ir[32] = "";
	kp_installed_version(c, name, iv, sizeof(iv), ir, sizeof(ir));
	if (st >= 0 && (strcmp(ix->st[st].version, iv) ||
			strcmp(ix->st[st].release, ir[0] ? ir : "1"))) {
		snprintf(p->bad, sizeof(p->bad), "%s is older than its recipe; "
			 "apply the system update first", name);
		return -1;
	}
	if (st < 0) {
		snprintf(p->bad, sizeof(p->bad), "the development files of %s "
			 "cannot be restored: %s", name,
			 have_ix ? w : "there is no binhost index here; `kdos "
			 "update fetch --binhost-only` takes this release's");
		return -1;
	}
	dev_push(p, name, DEV_RESTORE, st, 0, ix);
	return 0;
}

/*
 * The plan for building `src[0..n)` here: every member of the build tools
 * that is not installed, and what it lacks, prebuilt; then every
 * restore the build and the toolchain need. 0, or -1 with `p->bad` set.
 */
static int dev_prepare(const KpConf *c, const BhIndex *ix, int have_ix,
		       char *const *src, int n, DevPlan *p)
{
	memset(p, 0, sizeof(*p));
	char **tools = build_tools(c);
	int ntools = 0;
	while (tools[ntools])
		ntools++;
	{
		char **want = kb_calloc((size_t)ntools + 1, sizeof(*want));
		int nw = 0;
		for (int i = 0; i < ntools; i++)
			if (!kp_installed(c, tools[i]))
				want[nw++] = tools[i];
		KpOrder *ord = kb_calloc(1, sizeof(*ord));
		kp_resolve(c, want, nw, ord);
		for (int i = 0; i < ord->n && !p->bad[0]; i++) {
			char *pd = kp_port_dir(c, ord->order[i]);
			if (!pd || kp_installed(c, ord->order[i])) {
				free(pd);
				continue;
			}
			free(pd);
			char w[96] = "not in the binhost";
			int st = have_ix ? bh_match(c, ix, ord->order[i], w,
						    sizeof(w))
					 : -1;
			if (st < 0) {
				snprintf(p->bad, sizeof(p->bad), "%s of the "
					 "build tools cannot be installed "
					 "prebuilt: %s",
					 ord->order[i],
					 have_ix ? w : "no binhost index here");
				break;
			}
			int member = 0;
			for (int k = 0; k < nw; k++)
				member |= !strcmp(want[k], ord->order[i]);
			dev_push(p, ord->order[i], DEV_TOOL, st, member, ix);
		}
		free(ord);
		free(want);
	}
	for (int i = 0; i < ntools && !p->bad[0]; i++)
		dev_restore(c, ix, have_ix, tools[i], p);
	kb_strv_free(tools);
	for (int i = 0; i < n && !p->bad[0]; i++) {
		char **want = kp_facet_wanted(c, src[i]);
		for (int k = 0; want[k] && !p->bad[0]; k++)
			dev_restore(c, ix, have_ix, want[k], p);
		kb_strv_free(want);
	}
	return p->bad[0] ? -1 : 0;
}

/* The `kdos update install --dry-run` lines of a plan. */
static void dev_print(const KpConf *c, const BhIndex *ix, const DevPlan *p)
{
	for (int i = 0; i < p->n; i++) {
		const DevItem *d = &p->it[i];
		if (d->kind == DEV_REBUILD) {
			char v[64] = "", r[32] = "";
			kp_installed_version(c, d->name, v, sizeof(v), r,
					     sizeof(r));
			printf("%s dev-source %s-%s\n", d->name, v,
			       r[0] ? r : "1");
		} else {
			printf("%s %s %s %lld\n", d->name,
			       d->kind == DEV_TOOL ? "prebuilt" : "dev",
			       ix->st[d->stanza].file, ix->st[d->stanza].size);
		}
	}
}

/* The `kdos update apply --dry-run` lines of a plan. */
static void dev_lines(const BhIndex *ix, const DevPlan *p)
{
	for (int i = 0; i < p->n; i++) {
		const DevItem *d = &p->it[i];
		printf("  %-24s %s%s\n", d->name,
		       d->kind == DEV_TOOL ? "for building: "
		       : d->kind == DEV_REBUILD ? "development files: built "
						  "here again"
		       : "development files: ",
		       d->stanza >= 0 ? ix->st[d->stanza].file : "");
	}
}

/* Every package file of the plan into the cache, and the keyring beside
 * them. 0, -1 with `why` set, -2 when the network could not be reached. */
static int dev_fetch(const BhIndex *ix, const DevPlan *p, char *why,
		     size_t wcap)
{
	for (int i = 0; i < p->n; i++) {
		int fr;
		if (p->it[i].stanza >= 0)
			fr = fetch_package(ix, &ix->st[p->it[i].stanza], why,
					   wcap);
		else {
			int kr = run_kpkg("fetch", p->it[i].name);
			fr = kr == 3 ? -2 : kr ? -1 : 0;
			snprintf(why, wcap, "the sources of %s could not be "
				 "fetched", p->it[i].name);
		}
		if (fr != 0)
			return fr;
	}
	if (p->npre && keyring_into_cache() != 0) {
		snprintf(why, wcap, "the keyring could not be copied into %s",
			 cache_dir());
		return -1;
	}
	return 0;
}

/* The plan carried out, under KPKG_FACETS=all, into `root` (NULL: the
 * running root, or the slot run_* chroot into). 0, or -1 with `why` set. */
static int dev_install(const DevPlan *p, const char *root, char *why,
		       size_t wcap)
{
	char keys[1024];
	snprintf(keys, sizeof(keys), "%s/keys", cache_dir());
	setenv("KPKG_FACETS", "all", 1);
	for (int i = 0; i < p->n; i++) {
		const DevItem *d = &p->it[i];
		if (d->kind == DEV_TOOL)
			printf("==> installing %s (prebuilt, for building)\n",
			       d->name);
		else
			printf("==> restoring the development files of %s "
			       "(%s)\n", d->name,
			       d->kind == DEV_REBUILD ? "built here"
						      : "prebuilt");
		fflush(stdout);
		int ir;
		if (d->kind == DEV_REBUILD) {
			unsetenv("KPKG_REQUIRE_SIG");
			unsetenv("KPKG_KEYRING");
			ir = run_kpkg("install", d->name);
		} else {
			setenv("KPKG_KEYRING", keys, 1);
			setenv("KPKG_REQUIRE_SIG", "1", 1);
			ir = run_binhost(root, d->name);
		}
		if (ir != 0) {
			snprintf(why, wcap, "%s did not install", d->name);
			return -1;
		}
		if (d->world && world_add(d->name) != 0)
			fprintf(stderr, "kdos update: %s is installed and could "
					"not be added to %s\n", d->name,
				WORLD_FILE);
	}
	unsetenv("KPKG_REQUIRE_SIG");
	unsetenv("KPKG_KEYRING");
	return 0;
}

static int bh_fill(Apply *ap, int after_image)
{
	BhIndex ix;
	char why[512] = "";
	Behind *v = NULL;
	int n;

	int have_ix = bh_load(&ix, why, sizeof(why)) == 0;
	if (build_env_for(ap->ab ? ap->root : NULL) != 0)
		fprintf(stderr, "kdos update: no %s records the build's "
				"environment; matching under this one\n",
			BUILD_ENV);
	if ((n = worklist(ap, &v)) < 0) {
		if (have_ix)
			bh_free(&ix);
		return refuse(ap, "build-failed", "no package database in %s",
			      ap->root ? ap->root : "/");
	}
	int nwh = 0;
	char **whole = ap->ab ? whole_load(ap->run, &nwh) : NULL;
	if (n == 0 && !(after_image && nwh)) {
		free(v);
		kb_strv_free(whole);
		if (after_image) {
			if (have_ix)
				bh_free(&ix);
			return AP_OK;
		}
		if (!have_ix)
			return refuse(ap, "unverified", "%s; `kdos update apply "
				      "--source` builds the update here "
				      "instead", why);
		if (slot_ahead(ap)) {
			prog("verified %s %s 0 0", ix.who, ap->version);
			printf("slot %s already holds every package of %s\n",
			       ap->L.slot, ap->version);
			bh_free(&ix);
			return AP_OK;
		}
		bh_free(&ix);
		return nothing_behind(ap);
	}

	int *route = kb_calloc((size_t)n + 1, sizeof(*route));
	const char **bad = kb_calloc((size_t)n + 1, sizeof(*bad));
	int nbad = 0, nsrc = 0;
	long long bytes = 0;
	for (int i = 0; i < n; i++) {
		char w[96] = "not in the binhost";
		route[i] = origin_of(v[i].name);
		v[i].stanza = -1;
		if (route[i] == ROUTE_SOURCE)
			nsrc++;
		else if (have_ix &&
			 (v[i].stanza = bh_match(&ap->tconf, &ix, v[i].name, w,
						 sizeof(w))) >= 0) {
			if (!bh_present(&ix.st[v[i].stanza]))
				bytes += ix.st[v[i].stanza].size;
		} else {
			bad[nbad++] = v[i].name;
		}
		if (ap->dry)
			printf("  %-24s %-14s → %s-%s: %s\n", v[i].name,
			       v[i].have, v[i].want,
			       v[i].want_rel[0] ? v[i].want_rel : "1",
			       route[i] == ROUTE_SOURCE ? "built here"
			       : v[i].stanza >= 0 ? ix.st[v[i].stanza].file : w);
	}
	if (nbad) {
		char list[200];
		name_list(list, sizeof(list), bad, nbad);
		free(bad);
		free(route);
		free(v);
		kb_strv_free(whole);
		if (!have_ix)
			return refuse(ap, "unverified", "%s, and %d package%s "
				      "would come from it (%s); `kdos update "
				      "apply --source` builds the update here "
				      "instead", why, nbad, nbad == 1 ? "" : "s",
				      list);
		bh_free(&ix);
		return refuse(ap, "needs-source", "%d package%s need%s a source "
			      "build (%s): `kdos update apply --source`", nbad,
			      nbad == 1 ? "" : "s", nbad == 1 ? "s" : "", list);
	}
	int npre = n - nsrc;
	DevPlan dp;
	memset(&dp, 0, sizeof(dp));
	if (nsrc) {
		char **src = kb_calloc((size_t)n + 1, sizeof(*src));
		int ns = 0;
		for (int i = 0; i < n; i++)
			if (route[i] == ROUTE_SOURCE)
				src[ns++] = v[i].name;
		int pr = dev_prepare(&ap->tconf, &ix, have_ix, src, ns, &dp);
		free(src);
		if (pr != 0) {
			free(bad);
			free(route);
			free(v);
			kb_strv_free(whole);
			if (have_ix)
				bh_free(&ix);
			int r = refuse(ap, "build-failed", "%s", dp.bad);
			dev_free(&dp);
			return r;
		}
		if (ap->dry)
			dev_lines(&ix, &dp);
	}
	/* WHAT THIS MACHINE KEEPS WHOLE (whole_load), in a slot: a package of
	 * the worklist goes in whole, and one the slot holds without its
	 * development files is installed again whole after the worklist,
	 * prebuilt, or built here again for one whose origin is `source`. */
	DevPlan rp;
	memset(&rp, 0, sizeof(rp));
	for (int i = 0; i < nwh && !rp.bad[0]; i++) {
		int j;
		for (j = 0; j < n && strcmp(v[j].name, whole[i]); j++)
			;
		if (j == n && !dev_has(&dp, whole[i]))
			dev_restore(&ap->tconf, &ix, have_ix, whole[i], &rp);
	}
	if (rp.bad[0]) {
		free(bad);
		free(route);
		free(v);
		kb_strv_free(whole);
		if (have_ix)
			bh_free(&ix);
		int r = refuse(ap, "needs-source", "%s", rp.bad);
		dev_free(&dp);
		dev_free(&rp);
		return r;
	}
	if (n == 0 && rp.n == 0) {
		/* After an image: everything kept whole is whole already. */
		free(bad);
		free(route);
		free(v);
		kb_strv_free(whole);
		if (have_ix)
			bh_free(&ix);
		dev_free(&dp);
		return AP_OK;
	}
	if (ap->dry)
		dev_lines(&ix, &rp);
	if (ap->dry) {
		printf("%d package%s from the binhost, %s to download; %d "
		       "built here\n", npre + dp.npre + rp.npre,
		       npre + dp.npre + rp.npre == 1 ? "" : "s",
		       kb_human_size((unsigned long long)(bytes + dp.bytes +
							  rp.bytes)),
		       nsrc + dp.nsrc + rp.nsrc);
		dev_free(&dp);
		dev_free(&rp);
		kb_strv_free(whole);
		free(bad);
		free(route);
		free(v);
		if (have_ix)
			bh_free(&ix);
		apply_end(ap);
		return AP_OK;
	}
	if (!after_image)
		prog("verified %s %s %d %lld", have_ix ? ix.who : "source",
		     ap->version, n, bytes);
	printf("%s==>%s %d package%s from the binhost%s%s%s, %s to download; "
	       "%d built here\n", A, O, npre, npre == 1 ? "" : "s",
	       have_ix ? " (signed by key " : "", have_ix ? ix.who : "",
	       have_ix ? ")" : "", kb_human_size((unsigned long long)bytes),
	       nsrc);

	int total = 2 * n, done = 0, rc = AP_OK;
	char msg[512] = "";
	for (int i = 0; i < n && rc == AP_OK; i++) {
		if (g_stop) {
			rc = AP_CANCELLED;
			break;
		}
		if (route[i] == ROUTE_SOURCE) {
			int fr = run_kpkg("fetch", v[i].name);
			if (fr == 3) {
				dev_free(&dp);
				dev_free(&rp);
				kb_strv_free(whole);
				free(bad);
				free(route);
				free(v);
				if (have_ix)
					bh_free(&ix);
				return fail(ap, "no-network", "the network could "
					    "not be reached for the sources — "
					    "nothing was installed");
			}
			if (fr != 0) {
				snprintf(msg, sizeof(msg), "the sources of %s "
					 "could not be fetched", v[i].name);
				rc = AP_FAIL;
				break;
			}
		} else {
			int fr = fetch_package(&ix, &ix.st[v[i].stanza], msg,
					       sizeof(msg));
			if (fr == -2) {
				dev_free(&dp);
				dev_free(&rp);
				kb_strv_free(whole);
				free(bad);
				free(route);
				free(v);
				bh_free(&ix);
				return fail(ap, "no-network", msg);
			}
			if (fr != 0) {
				rc = AP_FAIL;
				break;
			}
		}
		prog("item %d %d %s", ++done, total, v[i].name);
	}
	for (int k = 0; k < 2 && rc == AP_OK; k++) {
		const DevPlan *fp = k ? &rp : &dp;
		if (!fp->n)
			continue;
		int fr = dev_fetch(&ix, fp, msg, sizeof(msg));
		if (fr == -2) {
			dev_free(&dp);
			dev_free(&rp);
			kb_strv_free(whole);
			free(bad);
			free(route);
			free(v);
			bh_free(&ix);
			return fail(ap, "no-network", msg);
		}
		if (fr != 0)
			rc = AP_FAIL;
	}
	if (rc == AP_OK && npre && keyring_into_cache() != 0) {
		snprintf(msg, sizeof(msg), "the keyring could not be copied "
			 "into %s", cache_dir());
		rc = AP_FAIL;
	}
	if (rc == AP_OK && ap->ab) {
		int binds = (npre || dp.npre || rp.npre ? AB_CACHE : 0) |
			    (nsrc || rp.nsrc ? AB_SOURCES : 0);
		if (slot_disarm(&ap->L) != 0) {
			snprintf(msg, sizeof(msg), "slot %s is armed for the next "
				 "boot and could not be disarmed", ap->L.slot);
			rc = AP_FAIL;
		} else if (!after_image && slot_forget_image(ap) != 0) {
			snprintf(msg, sizeof(msg), "slot %s's image record could "
				 "not be removed", ap->L.slot);
			rc = AP_FAIL;
		} else if (ab_enter(ap->root, ap->tree, &ap->tconf, binds) != 0) {
			snprintf(msg, sizeof(msg), "the slot could not be "
				 "prepared");
			rc = AP_FAIL;
		} else {
			ab_chroot = ap->root;
			unsetenv("KPKG_ROOT");
			if (nsrc || rp.nsrc)
				setenv("SOURCE_DIR", ap->tconf.source_dir, 1);
		}
	}
	char keys[1024];
	snprintf(keys, sizeof(keys), "%s/keys", cache_dir());
	/* A dependency a source build reaches that this run did not list is
	 * built on the way, its sources fetched as kpkg reaches it. */
	if (nsrc || rp.nsrc)
		setenv("KPKG_FETCH", "1", 1);
	if (rc == AP_OK && dp.n &&
	    dev_install(&dp, ap->ab ? NULL : ap->root, msg, sizeof(msg)) != 0)
		rc = AP_FAIL;
	nbad = 0;
	for (int i = 0; i < n && rc == AP_OK; i++) {
		if (g_stop) {
			rc = AP_CANCELLED;
			break;
		}
		int src = route[i] == ROUTE_SOURCE;
		printf("\n%s==>%s %s %s → %s (%s)\n", A, O, v[i].name,
		       v[i].have, v[i].want, src ? "built here" : "binhost");
		if (nsrc || strv_has(whole, v[i].name))
			setenv("KPKG_FACETS", "all", 1);
		else
			unsetenv("KPKG_FACETS");
		int ir;
		if (src) {
			/* A package built here carries no signature, and
			 * KPKG_REQUIRE_SIG would refuse it. */
			unsetenv("KPKG_REQUIRE_SIG");
			unsetenv("KPKG_KEYRING");
			ir = run_kpkg("install", v[i].name);
		} else {
			setenv("KPKG_KEYRING", keys, 1);
			setenv("KPKG_REQUIRE_SIG", "1", 1);
			ir = run_binhost(ap->ab ? NULL : ap->root, v[i].name);
		}
		if (ir != 0) {
			bad[nbad++] = v[i].name;
			char list[200];
			name_list(list, sizeof(list), bad, nbad);
			snprintf(msg, sizeof(msg), "%s did not install", list);
			rc = AP_FAIL;
		} else {
			if (!strcmp(v[i].have, "-"))
				origin_set(v[i].name, route[i]);
			prog("item %d %d %s", ++done, total, v[i].name);
		}
	}
	if (rc == AP_OK && rp.n &&
	    dev_install(&rp, ap->ab ? NULL : ap->root, msg, sizeof(msg)) != 0)
		rc = AP_FAIL;
	unsetenv("KPKG_REQUIRE_SIG");
	unsetenv("KPKG_KEYRING");
	unsetenv("KPKG_FETCH");
	unsetenv("KPKG_FACETS");
	ab_chroot = NULL;
	ab_unbind();
	if (ap->root)
		setenv("KPKG_ROOT", ap->root, 1);
	dev_free(&dp);
	dev_free(&rp);
	kb_strv_free(whole);
	free(bad);
	free(route);
	free(v);
	if (have_ix)
		bh_free(&ix);
	if (rc == AP_CANCELLED)
		return cancelled(ap);
	if (rc != AP_OK)
		return fail(ap, msg[0] && strstr(msg, "fetched") ? "fetch-failed"
							       : "build-failed",
			    msg);
	return AP_OK;
}

static int fill_binhost(Apply *ap)
{
	return bh_fill(ap, 0);
}

/*
 * THE SOURCE LANE: the update built on this machine. EVERY SOURCE BEFORE ANY
 * BUILD — a machine that is offline, or a source that has gone from both the
 * archive and upstream, is known in minutes, not after hours of compiling the
 * packages that happened to sort first, with a slot left half updated.
 */
static int fill_source(Apply *ap)
{
	Behind *v = NULL;
	int orphans = 0;
	int n = survey(&ap->tconf, &v, &orphans);

	if (n < 0) {
		char *db = kp_db_dir(&ap->tconf);
		fprintf(stderr, "kdos update: no package database at %s\n", db);
		free(db);
		apply_end(ap);
		return AP_USAGE;
	}
	if (n == 0) {
		free(v);
		if (slot_ahead(ap)) {
			prog("verified source %s 0 0", ap->version);
			printf("slot %s already holds every package of %s\n",
			       ap->L.slot, ap->version);
			return AP_OK;
		}
		return nothing_behind(ap);
	}
	/* What the builds compile with comes first (dev_prepare), from the
	 * binhost matched under the build's environment. */
	BhIndex ix;
	char ixwhy[256];
	int have_ix = bh_load(&ix, ixwhy, sizeof(ixwhy)) == 0;
	build_env_for(ap->ab ? ap->root : NULL);
	DevPlan dp;
	{
		char **src = kb_calloc((size_t)n + 1, sizeof(*src));
		for (int i = 0; i < n; i++)
			src[i] = v[i].name;
		int pr = dev_prepare(&ap->tconf, &ix, have_ix, src, n, &dp);
		free(src);
		if (pr != 0) {
			free(v);
			if (have_ix)
				bh_free(&ix);
			int r = refuse(ap, "build-failed", "%s", dp.bad);
			dev_free(&dp);
			return r;
		}
	}
	if (ap->dry) {
		dev_lines(&ix, &dp);
		for (int i = 0; i < n; i++)
			printf("  %-24s %-14s → %s: would fetch its sources "
			       "and build it\n", v[i].name, v[i].have,
			       v[i].want);
		dev_free(&dp);
		if (have_ix)
			bh_free(&ix);
		free(v);
		apply_end(ap);
		return AP_OK;
	}

	const char **bad = kb_calloc((size_t)n, sizeof(*bad));
	int nbad = 0, offline = 0;
	char list[200];
	prog("verified source %s %d 0", ap->version, n);
	printf("\n%s==>%s fetching the sources of %d package%s\n", A, O, n,
	       n == 1 ? "" : "s");
	for (int i = 0; i < n && !offline; i++) {
		int rc = run_kpkg("fetch", v[i].name);
		if (rc == 3)
			offline = 1;
		else if (rc != 0)
			bad[nbad++] = v[i].name;
		else
			prog("item %d %d %s", i + 1, 2 * n, v[i].name);
	}
	char dwhy[384] = "";
	int dfr = !offline && !nbad && dp.n ? dev_fetch(&ix, &dp, dwhy,
						       sizeof(dwhy))
					    : 0;
	if (dfr == -2)
		offline = 1;
	if (have_ix)
		bh_free(&ix);
	if (dfr == -1) {
		dev_free(&dp);
		free(bad);
		free(v);
		return fail(ap, "fetch-failed", dwhy);
	}
	if (offline || nbad) {
		dev_free(&dp);
		name_list(list, sizeof(list), bad, nbad);
		free(bad);
		free(v);
		/* On the APPLY line: the tree's own fetch may well have
		 * worked, and the next one would overwrite this. */
		if (offline)
			return fail(ap, "no-network", "the network could not be "
				    "reached for the sources — nothing was "
				    "built");
		char why[320];
		snprintf(why, sizeof(why), "these sources could not be fetched, "
			 "so nothing was built: %s", list);
		return fail(ap, "fetch-failed", why);
	}

	/* A dependency this tree adds is built by kpkg install on the way, and
	 * KPKG_FETCH=1 has kpkg fetch ITS sources as it reaches it. */
	setenv("KPKG_FETCH", "1", 1);

	/*
	 * DEPENDENCIES FIRST. The database lists names alphabetically, and a
	 * package built before a dependency that is behind with it compiles
	 * against the old one; wl_place puts every name after what it depends
	 * on among the behind set. Not kp_resolve: it passes over every name
	 * the target has an entry for, which is every name here.
	 */
	{
		char **names = kb_calloc((size_t)n + 1, sizeof(*names));
		char **order = kb_calloc((size_t)n + 1, sizeof(*order));
		char *st = kb_calloc((size_t)n + 1, 1);
		Behind *sorted = kb_calloc((size_t)n, sizeof(*sorted));
		int no = 0;

		for (int i = 0; i < n; i++)
			names[i] = v[i].name;
		for (int i = 0; i < n; i++)
			wl_place(&ap->tconf, names, n, st, i, order, &no);
		/* Each entry of `order` is one of the `names` pointers, and
		 * names[i] is v[i]'s name. */
		for (int k = 0; k < no; k++)
			for (int i = 0; i < n; i++)
				if (order[k] == names[i]) {
					sorted[k] = v[i];
					break;
				}
		memcpy(v, sorted, (size_t)n * sizeof(*v));
		free(sorted);
		free(st);
		free(order);
		free(names);
	}

	if (ap->ab) {
		if (slot_disarm(&ap->L) != 0) {
			dev_free(&dp);
			free(bad);
			free(v);
			return fail(ap, "build-failed", "the slot is armed for the "
				    "next boot and could not be disarmed");
		}
		if (slot_forget_image(ap) != 0) {
			dev_free(&dp);
			free(bad);
			free(v);
			return fail(ap, "build-failed", "the slot's image record "
				    "could not be removed");
		}
		if (ab_enter(ap->root, ap->tree, &ap->tconf,
			     AB_SOURCES | (dp.npre ? AB_CACHE : 0)) != 0) {
			dev_free(&dp);
			free(bad);
			free(v);
			return fail(ap, "build-failed", "the slot could not be "
				    "prepared for building");
		}
		/* Inside the chroot the slot IS the root; the source cache is
		 * named, because the slot's own kpkg.conf may say otherwise. */
		ab_chroot = ap->root;
		unsetenv("KPKG_ROOT");
		setenv("SOURCE_DIR", ap->tconf.source_dir, 1);
	}

	int done = 0, stopped = 0;
	setenv("KPKG_FACETS", "all", 1);
	if (dp.n && dev_install(&dp, ap->ab ? NULL : ap->root, dwhy,
				sizeof(dwhy)) != 0) {
		unsetenv("KPKG_FACETS");
		ab_chroot = NULL;
		ab_unbind();
		if (ap->root)
			setenv("KPKG_ROOT", ap->root, 1);
		dev_free(&dp);
		free(bad);
		free(v);
		return fail(ap, "build-failed", dwhy);
	}
	dev_free(&dp);
	for (int i = 0; i < n; i++) {
		if (g_stop) {
			stopped = 1;
			break;
		}
		printf("\n%s==>%s %s %s → %s\n", A, O, v[i].name, v[i].have,
		       v[i].want);
		if (run_kpkg("install", v[i].name) == 0) {
			done++;
			prog("item %d %d %s", n + i + 1, 2 * n, v[i].name);
		} else {
			bad[nbad++] = v[i].name;
		}
	}
	printf("\n%d of %d built and installed, %d failed\n", done, n, nbad);
	unsetenv("KPKG_FACETS");
	ab_chroot = NULL;
	ab_unbind();
	if (ap->root)
		setenv("KPKG_ROOT", ap->root, 1);

	char why[256] = "";
	if (nbad) {
		name_list(list, sizeof(list), bad, nbad);
		snprintf(why, sizeof(why), "%d of %d failed: %s", nbad, n,
			 list);
	}
	free(bad);
	free(v);
	if (stopped)
		return cancelled(ap);
	if (why[0])
		return fail(ap, "build-failed", why);
	if (!ap->ab) {
		char ok[64];
		snprintf(ok, sizeof(ok), "%d package%s", done,
			 done == 1 ? "" : "s");
		state_set("apply", "ok", ok);
	}
	return AP_OK;
}

/*
 * THE SLOT IS CHECKED BEFORE IT IS ACQUIRED: `kpkg check` compares every file
 * the slot's database records with what is on its disk, and a root with a
 * missing file, or a changed one outside etc/, is not a candidate, whatever
 * every install reported. Edited configuration under etc/ is listed and
 * passes: failing on it would refuse every configured machine.
 */
static int check_slot(Apply *ap)
{
	KbArgv k = {0};
	kb_argv_add(&k, "kpkg");
	kb_argv_add(&k, "check");
	kb_argv_add(&k, "--all");
	kb_argv_add(&k, "--root");
	kb_argv_add(&k, ap->root);
	kb_argv_end(&k);
	if (kb_run_tty(&k) != 0) {
		char why[128];
		snprintf(why, sizeof(why), "slot %s failed `kpkg check`",
			 ap->L.slot);
		return fail(ap, "build-failed", why);
	}
	return AP_OK;
}

/*
 * THE ARM: this machine's configuration, the tree, the version, the kernel,
 * the trial — the short half Discover shows no progress for. It takes only a
 * slot an acquire of THIS tree or image filled, so a slot from an older
 * acquire, or one filled by hand, is never armed by mistake.
 *
 * THE CONFIGURATION IS CARRIED HERE, not at the acquire: what a person
 * changes between the download and the restart — a password, a network —
 * is in the slot that boots. kdt_image_carry says what moves and what
 * stays the slot's.
 *
 * THE SLOT'S KERNEL GOES ONTO THE ESP BEFORE THE SLOT IS TRIED, every time
 * and whether or not `linux` was in this run. Each slot boots the kernel in
 * its own ESP directory, and the modules for it exist only in that root; a
 * slot whose directory was never filled, or holds a kernel older than its
 * /lib/modules, boots with no modular driver. A deploy that fails leaves the
 * slot untried.
 */
static int arm_slot(Apply *ap, const char *how, int check_acquired)
{
	char code[32], detail[256], want[160], why[256];

	snprintf(want, sizeof(want), "slot %s %s", ap->L.slot, ap->version);
	if (check_acquired &&
	    (!state_get("apply", code, sizeof(code), detail, sizeof(detail)) ||
	     strcmp(code, "acquired") || strcmp(detail, want)))
		return refuse(ap, NULL, "slot %s holds no acquired update for "
			      "%s: run `kdos update apply --acquire` first",
			      ap->L.slot, ap->version);
	/* kdos-sysupdated replies to Install with a job only once this line
	 * arrives; without it the call waits for the arm to end and fails. A
	 * full apply said `verified` when its fill began. */
	if (check_acquired)
		prog("verified - %s 0 0", ap->version);
	/* An image brought the tree it was built from; packages did not. */
	if (!ap->from_image && tree_into_slot(ap->tree, ap->root) != 0) {
		snprintf(why, sizeof(why), "the ports tree could not be copied "
			 "into slot %s", ap->L.slot);
		return fail(ap, "build-failed", why);
	}
	{
		/* The slot's root device by the UUID its boot state records:
		 * an image's fstab names none, and the running one names the
		 * running root's. */
		char cwhy[512], dev[128];
		char *db = kp_db_dir(ap->run);
		snprintf(dev, sizeof(dev), "UUID=%s", ap->L.uuid);
		int crc = kdt_image_carry(sys_root(), ap->root, db,
					  ap->L.uuid[0] ? dev : NULL, cwhy,
					  sizeof(cwhy));
		free(db);
		if (crc != 0)
			return fail(ap, "build-failed", cwhy);
	}
	if (ap->from_image
		? release_fields(ap->root, ap->version, ap->itree, ap->itime,
				 how) != 0
		: release_write(ap->root, ap->tree, how) != 0) {
		snprintf(why, sizeof(why), "slot %s's /etc/kdos-release could "
			 "not be written", ap->L.slot);
		return fail(ap, "build-failed", why);
	}
	KbArgv d = {0};
	kb_argv_add(&d, "kdos-bootctl");
	kb_argv_add(&d, "deploy");
	kb_argv_add(&d, ap->root);
	kb_argv_add(&d, ap->L.slot);
	kb_argv_end(&d);
	if (kb_run_tty(&d) != 0) {
		snprintf(why, sizeof(why), "slot %s's kernel could not be put "
			 "on the ESP", ap->L.slot);
		return fail(ap, "build-failed", why);
	}
	KbArgv t = {0};
	kb_argv_add(&t, "kdos-bootctl");
	kb_argv_add(&t, "try");
	kb_argv_add(&t, ap->L.slot);
	kb_argv_end(&t);
	if (kb_run_tty(&t) != 0) {
		snprintf(why, sizeof(why), "slot %s could not be marked as the "
			 "candidate", ap->L.slot);
		return fail(ap, "build-failed", why);
	}
	printf("slot %s is the candidate: a boot that does not confirm it "
	       "rolls back\n", ap->L.slot);
	prog("armed %s %s", ap->L.slot, ap->version);
	state_set("apply", "ok", want);
	apply_end(ap);
	doc_refresh();
	return AP_OK;
}

static int cmd_apply(const KpConf *c, int argc, char **argv)
{
	Apply ap;

	memset(&ap, 0, sizeof(ap));
	ap.lock = -1;
	for (int i = 0; i < argc; i++) {
		if (!strcmp(argv[i], "--dry-run"))
			ap.dry = 1;
		else if (!strcmp(argv[i], "--in-place"))
			ap.in_place = 1;
		else if (!strcmp(argv[i], "--source"))
			ap.source = 1;
		else if (!strcmp(argv[i], "--image"))
			ap.want_image = 1;
		else if (!strcmp(argv[i], "--binhost"))
			ap.want_binhost = 1;
		else if (!strcmp(argv[i], "--acquire"))
			ap.acquire = 1;
		else if (!strcmp(argv[i], "--arm"))
			ap.arm = 1;
		else if (!strcmp(argv[i], "--mount"))
			ap.mount = 1;
		else if (!strcmp(argv[i], "--root") && i + 1 < argc)
			ap.root = argv[++i];
		else if (!strcmp(argv[i], "--progress") && i + 1 < argc) {
			if (prog_open(argv[++i]) != 0) {
				fprintf(stderr, "kdos update: --progress takes "
						"an open descriptor of 3 or "
						"more\n");
				return AP_USAGE;
			}
		} else {
			fprintf(stderr, "usage: kdos update apply [--image|"
					"--binhost|--source] [--acquire|--arm]\n"
					"                         [--mount] "
					"[--dry-run] [--root DIR] [--in-place] "
					"[--progress FD]\n");
			return AP_USAGE;
		}
	}
	if (ap.want_image + ap.want_binhost + ap.source > 1) {
		fprintf(stderr, "kdos update: --image, --binhost and --source "
				"each name the one lane to take; pass one\n");
		return AP_USAGE;
	}
	if (ap.acquire && ap.arm) {
		fprintf(stderr, "kdos update: --acquire and --arm are the two "
				"halves of one apply; pass one, or neither\n");
		return AP_USAGE;
	}
	/* No tree leaves nothing to install from but an image; --image says
	 * whether there is one itself. */
	if (!have_ports(c) && !ap.want_image) {
		char iwhy[256];
		KdtImage *im = ap.source || ap.want_binhost
			       ? NULL : image_offer(iwhy, sizeof(iwhy));
		kdt_image_free(im);
		if (!im) {
			fprintf(stderr, "kdos update: %s\n", NO_TREE_TEXT);
			prog("refused %s", NO_TREE_TEXT);
			return AP_USAGE;
		}
	}
	if (!ap.dry && geteuid() != 0) {
		fprintf(stderr, "kdos update apply installs packages and needs "
				"root (--dry-run does not)\n");
		prog("refused kdos update apply needs root");
		return AP_USAGE;
	}
	if (!ap.dry && (ap.lock = update_lock()) < 0) {
		prog("refused another kdos update is running");
		return AP_USAGE;
	}
	stop_on_term();
	ports_root(c, ap.tree, sizeof(ap.tree));
	tree_version(ap.tree, ap.version, sizeof(ap.version), NULL);

	int rc = apply_target(&ap, c);
	if (rc != AP_OK)
		return rc;

	if (ap.arm) {
		/* Which lane filled the slot: an image leaves its record there,
		 * naming the version the acquire recorded. */
		char rec[1100], iv[64], code[32], detail[256], want[160];
		snprintf(rec, sizeof(rec), "%s%s", ap.root, IMAGE_RECORD);
		if (!ap.source &&
		    kdt_image_peek(rec, iv, sizeof(iv), ap.itree,
				   sizeof(ap.itree), &ap.itime) == 0 &&
		    state_get("apply", code, sizeof(code), detail,
			      sizeof(detail)) && !strcmp(code, "acquired")) {
			snprintf(want, sizeof(want), "slot %s %s", ap.L.slot, iv);
			if (!strcmp(detail, want)) {
				ap.from_image = 1;
				kb_strlcpy(ap.version, iv, sizeof(ap.version));
			}
		}
		return arm_slot(&ap, ap.from_image ? "image"
				: ap.source ? "source" : "binhost", 1);
	}

	/* THE LANE: an image newer than this system when one is cached and
	 * verified, unless --binhost or --source says otherwise. */
	if (!ap.source && !ap.want_binhost) {
		char iwhy[256];
		ap.img = image_offer(iwhy, sizeof(iwhy));
		if (!ap.img && ap.want_image)
			return refuse(&ap, NULL, "no slot image is on offer: %s",
				      iwhy);
		if (ap.img) {
			ap.from_image = 1;
			kb_strlcpy(ap.version, kdt_image_version(ap.img),
				   sizeof(ap.version));
			kb_strlcpy(ap.itree, kdt_image_tree(ap.img),
				   sizeof(ap.itree));
			ap.itime = kdt_image_time(ap.img);
		}
	}

	rc = ap.source ? fill_source(&ap)
	     : ap.img ? fill_image(&ap) : fill_binhost(&ap);
	if (rc != AP_OK || ap.dry || ap.lock < 0)
		return rc;

	if (!ap.ab) {
		/* In place, or --root: the root now holds this tree's packages
		 * or this image. */
		if (ap.from_image) {
			state_set("apply", "ok", "written from the slot image");
			release_fields(ap.root, ap.version, ap.itree, ap.itime,
				       "image");
		} else {
			if (!ap.source)
				state_set("apply", "ok",
					  "installed from the binhost");
			release_write(ap.root ? ap.root : "/", ap.tree,
				      ap.source ? "source" : "binhost");
		}
		apply_end(&ap);
		doc_refresh();
		return AP_OK;
	}
	if ((rc = check_slot(&ap)) != AP_OK)
		return rc;
	if (ap.acquire) {
		char got[160];
		snprintf(got, sizeof(got), "slot %s %s", ap.L.slot, ap.version);
		state_set("apply", "acquired", got);
		prog("acquired %s %s", ap.L.slot, ap.version);
		printf("slot %s holds %s, checked; `kdos update apply --arm` "
		       "makes it the next boot's candidate\n", ap.L.slot,
		       ap.version);
		apply_end(&ap);
		doc_refresh();
		return AP_OK;
	}
	return arm_slot(&ap, ap.from_image ? "image"
			    : ap.source ? "source" : "binhost", 0);
}

/* ── notes ─────────────────────────────────────────────────────────────── */

void bootctl_other_version(char *out, size_t n);

/*
 * `kdos update notes --json`: the release notes of the slot this boot runs,
 * for the What's New page plasma-welcome shows after a confirmed switch. One
 * object, {"version", "previous", "notes"}: the version running (the name
 * `check` calls "installed"), the version the other slot holds ("" when none
 * is recorded), and the RELEASE file of the tree this slot carries, as
 * Markdown. 1 with no document when the slot
 * carries no notes, which the page shows as such.
 */
static int cmd_notes(const KpConf *c, int argc, char **argv)
{
	int json = 0;
	for (int i = 0; i < argc; i++) {
		if (!strcmp(argv[i], "--json")) {
			json = 1;
		} else {
			fprintf(stderr, "usage: kdos update notes [--json]\n");
			return 2;
		}
	}
	char root[512], run[64], prev[64];
	ports_root(c, root, sizeof(root));
	char *notes = tree_notes(root);
	if (!notes) {
		fprintf(stderr, "kdos update: this slot carries no release "
				"notes\n");
		return 1;
	}
	bootctl_root_version(sys_root(), run, sizeof(run));
	bootctl_other_version(prev, sizeof(prev));
	if (!json) {
		printf("%sKDOS %s%s%s%s\n\n%s", A, run, O,
		       prev[0] ? ", updated from " : "", prev, notes);
		free(notes);
		return 0;
	}
	KbBuf b = {0};
	kb_buf_str(&b, "{\"version\":");
	kb_json_str(&b, run);
	kb_buf_str(&b, ",\"previous\":");
	kb_json_str(&b, prev);
	kb_buf_str(&b, ",\"notes\":");
	kb_json_str(&b, notes);
	kb_buf_str(&b, "}\n");
	fwrite(b.p, 1, b.n, stdout);
	kb_buf_free(&b);
	free(notes);
	return 0;
}

/* ── install ───────────────────────────────────────────────────────────── */

/* `name` added to the world set, once, written whole: a world file cut short
 * by a crash would drop every port named after the cut at the next update. */
static int world_add(const char *name)
{
	char p[1024];
	int n = 0;
	char **w = world_load(&n);
	for (int i = 0; i < n; i++)
		if (!strcmp(w[i], name)) {
			kb_strv_free(w);
			return 0;
		}
	sys_path(WORLD_FILE, p, sizeof(p));
	char *old = kb_read_all(p, NULL);
	KbBuf b = {0};
	if (old && *old) {
		kb_buf_str(&b, old);
		if (old[strlen(old) - 1] != '\n')
			kb_buf_str(&b, "\n");
	}
	kb_buf_printf(&b, "%s\n", name);
	char *dir = kb_strdup(p);
	char *sl = strrchr(dir, '/');
	if (sl)
		*sl = 0;
	int rc = kb_mkdir_p(dir) == 0 ? kb_write_file_atomic(p, b.p) : -1;
	free(dir);
	free(old);
	kb_buf_free(&b);
	kb_strv_free(w);
	return rc;
}

/* The first dependency of `name`, at any depth, that is installed and older
 * than its recipe, into `out`; 0 when none is. `seen` holds every port
 * walked, so a cycle ends where kp_resolve ends it. */
static int stale_dep(const KpConf *c, const char *name, const Behind *v,
		     int nv, char (*seen)[128], int *ns, int cap, char *out,
		     size_t ocap)
{
	for (int i = 0; i < *ns; i++)
		if (!strcmp(seen[i], name))
			return 0;
	if (*ns >= cap)
		return 0;
	kb_strlcpy(seen[(*ns)++], name, 128);
	char *pd = kp_port_dir(c, name);
	if (!pd)
		return 0;
	char (*deps)[128] = kb_calloc(KP_MAX_DEPS, sizeof(*deps));
	int nd = kp_depends(pd, deps, KP_MAX_DEPS);
	free(pd);
	int found = 0;
	for (int d = 0; d < nd && !found; d++) {
		for (int j = 0; j < nv && !found; j++)
			if (!strcmp(v[j].name, deps[d]) && strcmp(v[j].have, "-")) {
				kb_strlcpy(out, deps[d], ocap);
				found = 1;
			}
		if (!found)
			found = stale_dep(c, deps[d], v, nv, seen, ns, cap, out,
					  ocap);
	}
	free(deps);
	return found;
}

/* ── groups ────────────────────────────────────────────────────────────── */

/*
 * A GROUP IS A NAMED SET OF PORTS, read from the tree's `groups` file
 * (/ports/groups; ports/groups in the repository): a `group <id> <title>`
 * line, then the member ports, any number to a line, until the next group.
 * A member may be a meta port (`kde-gear-games`), which installs what it
 * names. A group is no package: `kdos update install <id>` installs each
 * member as the port it is, and each member joins the world set by its own
 * name, so a member a later tree drops from the group stays installed and a
 * member it adds arrives with the next `kdos update install <id>`.
 */
#define GROUPS_MAX 64
typedef struct {
	char id[64];
	char title[160];
	char **ports;
	int n;
} Group;

static void groups_free(Group *g, int n)
{
	for (int i = 0; i < n; i++)
		kb_strv_free(g[i].ports);
	free(g);
}

static int groups_load(const KpConf *c, Group **out)
{
	char root[512], p[600];
	ports_root(c, root, sizeof(root));
	snprintf(p, sizeof(p), "%s/groups", root);
	char *data = kb_read_all(p, NULL);
	Group *g = kb_calloc(GROUPS_MAX, sizeof(*g));
	int n = 0, cap = 0;
	for (char *l = data; l && *l;) {
		char *nl = strchr(l, '\n');
		if (nl)
			*nl = 0;
		char *h = strchr(l, '#');
		if (h)
			*h = 0;
		while (*l == ' ' || *l == '\t')
			l++;
		if (!strncmp(l, "group", 5) && (l[5] == ' ' || l[5] == '\t')) {
			if (n == GROUPS_MAX)
				break;
			char *id = l + 6;
			while (*id == ' ' || *id == '\t')
				id++;
			size_t il = strcspn(id, " \t");
			char *t = id + il;
			while (*t == ' ' || *t == '\t')
				t++;
			id[il] = 0;
			if (pkg_name_ok(id)) {
				kb_strlcpy(g[n].id, id, sizeof(g[n].id));
				kb_strlcpy(g[n].title, *t ? t : id, sizeof(g[n].title));
				g[n].title[strcspn(g[n].title, "\r")] = 0;
				n++;
				cap = 0;
			}
		} else if (n > 0) {
			Group *cur = &g[n - 1];
			for (char *w = strtok(l, " \t\r"); w; w = strtok(NULL, " \t\r")) {
				if (!pkg_name_ok(w))
					continue;
				if (cur->n + 1 >= cap) {
					cap = cap ? cap * 2 : 16;
					cur->ports = kb_realloc(cur->ports,
						(size_t)cap * sizeof(*cur->ports));
				}
				cur->ports[cur->n++] = kb_strdup(w);
				cur->ports[cur->n] = NULL;
			}
		}
		l = nl ? nl + 1 : NULL;
	}
	free(data);
	*out = g;
	return n;
}

static const Group *group_find(const Group *g, int n, const char *id)
{
	for (int i = 0; i < n; i++)
		if (!strcmp(g[i].id, id))
			return &g[i];
	return NULL;
}

/* The members of the `build-tools` group, or, in a tree that names no such
 * group, the ports no build starts without. kb_strv_free it. */
static char **build_tools(const KpConf *c)
{
	static const char *const fallback[] = { "gcc", "binutils", "make",
						"cmake", "meson", "ninja",
						"pkgconf", NULL };
	Group *g = NULL;
	int ng = groups_load(c, &g);
	const Group *gr = group_find(g, ng, "build-tools");
	int n = 0;
	if (!gr)
		while (fallback[n])
			n++;
	else
		n = gr->n;
	char **out = kb_calloc((size_t)n + 1, sizeof(*out));
	for (int i = 0; i < n; i++)
		out[i] = kb_strdup(gr ? gr->ports[i] : fallback[i]);
	groups_free(g, ng);
	return out;
}

/*
 * `kdos update groups [--json]`: every group, whether all of it is
 * installed, how many of its members are not, and — when the signed binhost
 * index is in place — how many packages installing it prebuilt would add
 * and their size. The size is null in the JSON when there is no index, or
 * when some package of it is not covered.
 */
static int cmd_groups(const KpConf *c, int argc, char **argv)
{
	int json = argc == 1 && !strcmp(argv[0], "--json");
	if (argc > 1 || (argc == 1 && !json)) {
		fprintf(stderr, "usage: kdos update groups [--json]\n");
		return 2;
	}
	Group *g = NULL;
	int ng = groups_load(c, &g);
	BhIndex ix;
	char why[256];
	int have_ix = bh_load(&ix, why, sizeof(why)) == 0;
	if (have_ix)
		build_env_for(NULL);
	KbBuf b = {0};
	if (json)
		kb_buf_str(&b, "[");
	for (int i = 0; i < ng; i++) {
		int missing = 0;
		char **want = kb_calloc((size_t)g[i].n + 1, sizeof(*want));
		for (int k = 0; k < g[i].n; k++)
			if (!kp_installed(c, g[i].ports[k]))
				want[missing++] = g[i].ports[k];
		KpOrder *ord = kb_calloc(1, sizeof(*ord));
		kp_resolve(c, want, missing, ord);
		long long size = 0;
		int npkg = 0, covered = have_ix;
		for (int k = 0; k < ord->n; k++) {
			char *pd = kp_port_dir(c, ord->order[k]);
			if (!pd)
				continue;
			free(pd);
			npkg++;
			char w[96];
			int s = have_ix ? bh_match(c, &ix, ord->order[k], w, sizeof(w))
					: -1;
			if (s < 0)
				covered = 0;
			else if (!bh_present(&ix.st[s]))
				size += ix.st[s].size;
		}
		if (json) {
			kb_buf_str(&b, i ? ",{\"id\":" : "{\"id\":");
			kb_json_str(&b, g[i].id);
			kb_buf_str(&b, ",\"title\":");
			kb_json_str(&b, g[i].title);
			kb_buf_str(&b, ",\"ports\":[");
			for (int k = 0; k < g[i].n; k++) {
				if (k)
					kb_buf_str(&b, ",");
				kb_json_str(&b, g[i].ports[k]);
			}
			kb_buf_printf(&b, "],\"installed\":%s,\"missing\":%d,"
				      "\"packages\":%d,\"size\":",
				      missing ? "false" : "true", missing, npkg);
			if (covered)
				kb_buf_printf(&b, "%lld}", size);
			else
				kb_buf_str(&b, "null}");
		} else if (!missing) {
			printf("  %-16s %s — installed\n", g[i].id, g[i].title);
		} else {
			printf("  %-16s %s — %d of %d to install, %d package%s",
			       g[i].id, g[i].title, missing, g[i].n, npkg,
			       npkg == 1 ? "" : "s");
			if (covered)
				printf(", %s to download\n",
				       kb_human_size((unsigned long long)size));
			else
				printf("%s\n", have_ix ? ", not all prebuilt"
						       : "");
		}
		free(ord);
		free(want);
	}
	if (json) {
		kb_buf_str(&b, "]\n");
		fputs(b.p, stdout);
	} else if (!ng) {
		printf("this tree names no groups\n");
	} else if (!have_ix) {
		printf("no binhost index here, so no sizes: `kdos update fetch "
		       "--binhost-only` takes this release's\n");
	}
	kb_buf_free(&b);
	if (have_ix)
		bh_free(&ix);
	groups_free(g, ng);
	return 0;
}

static int cmd_install(const KpConf *c, int argc, char **argv);

/*
 * `kdos update install --pending`: every group (or port) the installer
 * recorded in /var/lib/kdos/groups-pending because the binhost could not be
 * reached, installed by the route the flags or update.conf name. Each one
 * that installs leaves the file at once, and the file goes with the last,
 * so a run stopped half way picks up where it stopped.
 */
#define GROUPS_PENDING "/var/lib/kdos/groups-pending"
static int install_pending(const KpConf *c, int dry, int mode, int deps)
{
	char p[1024];
	sys_path(GROUPS_PENDING, p, sizeof(p));
	char *data = kb_read_all(p, NULL);
	char **want = NULL;
	int n = 0, worst = 0;
	for (char *l = data; l && *l;) {
		char *nl = strchr(l, '\n');
		if (nl)
			*nl = 0;
		l[strcspn(l, " \t\r#")] = 0;
		if (*l && pkg_name_ok(l)) {
			want = kb_realloc(want, (size_t)(n + 2) * sizeof(*want));
			want[n++] = kb_strdup(l);
			want[n] = NULL;
		}
		l = nl ? nl + 1 : NULL;
	}
	free(data);
	if (!n) {
		printf("nothing is pending\n");
		kb_strv_free(want);
		return 0;
	}
	for (int i = 0; i < n; i++) {
		char *av[6], dbuf[32];
		int an = 0;
		if (dry)
			av[an++] = "--dry-run";
		if (mode >= 0)
			av[an++] = mode == ROUTE_SOURCE ? "--source" : "--prebuilt";
		if (deps >= 0) {
			snprintf(dbuf, sizeof(dbuf), "--build-deps=%s",
				 route_name(deps));
			av[an++] = dbuf;
		}
		av[an++] = want[i];
		int r = cmd_install(c, an, av);
		if (r > worst)
			worst = r;
		if (r || dry)
			continue;
		/* Rewritten whole without what is installed: every entry that
		 * failed and every one not reached stays. */
		want[i][0] = 0;
		KbBuf b = {0};
		kb_buf_str(&b, "");
		for (int k = 0; k < n; k++)
			if (want[k][0])
				kb_buf_printf(&b, "%s\n", want[k]);
		if (!b.n)
			unlink(p);
		else
			kb_write_file_atomic(p, b.p);
		kb_buf_free(&b);
	}
	kb_strv_free(want);
	return worst;
}

/*
 * `kdos update install --dev <port|group>...`: the development and
 * documentation files of installed packages, which FACETS=run left out,
 * brought back by installing each package again whole (KPKG_FACETS=all):
 * prebuilt from the binhost, or built here again for a package whose origin
 * is `source`. A group restores every installed member. A package installed
 * whole is said to be, and one not installed is named and skipped. The
 * package then stays whole across updates (kpkg.h, the facet record) and
 * across a slot image (whole_load).
 * --dry-run prints `<pkg> dev <file> <bytes>` or `<pkg> dev-source
 * <version>-<release>` per package and `total <prebuilt> <bytes> <source>`.
 */
static int install_dev(const KpConf *c, char **names, int nnames, int dry,
		       const char *root)
{
	if (!dry && geteuid() != 0) {
		fprintf(stderr, "kdos update install --dev writes the running "
				"root and needs root\n");
		return 1;
	}
	char **want = NULL;
	int nw = 0, rc = 0;
	Group *g = NULL;
	int ng = groups_load(c, &g);
	for (int i = 0; i < nnames; i++) {
		const Group *gr = NULL;
		char *pd = kp_port_dir(c, names[i]);
		if (!pd && !(gr = group_find(g, ng, names[i]))) {
			fprintf(stderr, "kdos update: no port or group named %s "
					"in the tree\n", names[i]);
			rc = 1;
			continue;
		}
		free(pd);
		int nm = gr ? gr->n : 1;
		for (int k = 0; k < nm; k++) {
			const char *p = gr ? gr->ports[k] : names[i];
			if (!kp_installed(c, p)) {
				fprintf(stderr, "kdos update: %s is not "
						"installed; `kdos update install "
						"%s` installs it\n", p, p);
				if (!gr)
					rc = 1;
				continue;
			}
			char *rec = kp_installed_facets(c, p, NULL);
			if (!rec) {
				printf("every file of %s is installed\n", p);
				continue;
			}
			free(rec);
			want = kb_realloc(want, (size_t)(nw + 2) * sizeof(*want));
			want[nw++] = kb_strdup(p);
			want[nw] = NULL;
		}
	}
	groups_free(g, ng);
	if (!nw) {
		kb_strv_free(want);
		return rc;
	}

	int lock = dry ? -1 : update_lock();
	if (!dry && lock < 0) {
		kb_strv_free(want);
		return 1;
	}
	BhIndex ix;
	char why[512] = "";
	int have_ix = bh_load(&ix, why, sizeof(why)) == 0;
	build_env_for(NULL);
	DevPlan dp;
	memset(&dp, 0, sizeof(dp));
	for (int i = 0; i < nw && !dp.bad[0]; i++)
		dev_restore(c, &ix, have_ix, want[i], &dp);
	if (!dp.bad[0] && root && dp.nsrc)
		snprintf(dp.bad, sizeof(dp.bad), "a package built here is built "
			 "again here, never into another root");
	if (dp.bad[0]) {
		fprintf(stderr, "kdos update: %s\n", dp.bad);
		rc = 1;
	} else if (dry) {
		dev_print(c, &ix, &dp);
		printf("total %d %lld %d\n", dp.npre, dp.bytes, dp.nsrc);
	} else {
		int fr = dev_fetch(&ix, &dp, why, sizeof(why));
		if (fr == 0 && dp.nsrc)
			setenv("KPKG_FETCH", "1", 1);
		if (fr != 0 || dev_install(&dp, root, why, sizeof(why)) != 0) {
			fprintf(stderr, "kdos update: %s\n",
				fr == -2 ? "the network could not be reached"
					 : why);
			rc = 1;
		}
		unsetenv("KPKG_FACETS");
		unsetenv("KPKG_FETCH");
	}
	dev_free(&dp);
	if (have_ix)
		bh_free(&ix);
	if (lock >= 0)
		close(lock);
	kb_strv_free(want);
	return rc;
}

/*
 * The route an install takes when its command line names none, from
 * update.conf: `install =` for the port itself, `build-deps =` for each
 * package a source install lacks. Both default to `prebuilt`. -1 with the
 * reason on stderr when a value is neither word.
 */
static int conf_route(const char *key)
{
	char *v = conf_value(key);
	if (!v)
		return ROUTE_PREBUILT;
	int r = route_parse(v);
	if (r < 0)
		fprintf(stderr, "kdos update: %s = %s in %s is neither prebuilt "
				"nor source\n", key, v, UPDATE_CONF);
	free(v);
	return r;
}

/*
 * `kdos update install [--dry-run] [--prebuilt | --source]
 *  [--build-deps=prebuilt|source] <port>`: one port, and every package it
 * lacks, into the RUNNING root. Discover's ports helper runs it as root
 * through pkexec.
 *
 * THE ROUTE. --prebuilt takes the port and everything it lacks from the
 * release binhost `kdos update fetch` keeps in /var/lib/kdos/binhost, and a
 * package the signed index does not cover refuses the whole install: an
 * install button that starts an hours-long compile is not the button that
 * was pressed. --source builds the port here from its recipe, and each
 * package it lacks comes from the binhost (--build-deps=prebuilt, a package
 * the index does not cover built instead) or is built too
 * (--build-deps=source). With neither flag, `install =` and `build-deps =` in
 * update.conf decide, and both default to prebuilt. An install never rebuilds
 * a package that is installed: the base stays what the release built and
 * tested. A port already installed by the other route is installed again by
 * the one asked for, alone.
 *
 * A RUN THAT BUILDS READIES THE MACHINE FIRST (dev_prepare): each missing
 * member of the build tools, and the development facet of everything the builds
 * compile against, both prebuilt and printed before the order; a tool the
 * order also names comes from the plan. Everything such a run installs goes
 * in under KPKG_FACETS=all. --dev (install_dev) is the restore alone.
 *
 * A dependency already installed and older than its recipe refuses the
 * install, since the new package is built, here or for the binhost, against
 * the newer one; the system update comes first. Every package file and every
 * source is fetched and checked before anything is installed, and a prebuilt
 * package goes in through `kpkg binhost` under KPKG_REQUIRE_SIG with the
 * keyring copied into the cache. Each package records its route in the origin
 * file as it is installed, so an install stopped half way keeps what
 * finished; the port joins the world set only after everything installed.
 *
 * stdout: one `==> installing <pkg> (<route>)` line per package. --dry-run
 * decides everything above and touches nothing, so it needs no root, and
 * prints one line per package, `<pkg> prebuilt <file> <bytes>`, `<pkg>
 * source <version>-<release>`, or for a restore `<pkg> dev <file> <bytes>`
 * or `<pkg> dev-source <version>-<release>`, then `total <prebuilt> <bytes>
 * <source>`. Exit 0
 * installed (or already was), 1 refused or failed with the reason on stderr,
 * 2 usage.
 */
static int cmd_install(const KpConf *c, int argc, char **argv)
{
	int dry = 0, mode = -1, deps = -1, pending = 0, dev = 0, ndev = 0;
	const char *name = NULL, *root = NULL;
	char **devnames = kb_calloc((size_t)argc + 1, sizeof(*devnames));
	for (int i = 0; i < argc; i++) {
		if (!strcmp(argv[i], "--dry-run"))
			dry = 1;
		else if (!strcmp(argv[i], "--pending"))
			pending = 1;
		else if (!strcmp(argv[i], "--dev"))
			dev = 1;
		else if (dev && argv[i][0] != '-' && pkg_name_ok(argv[i]))
			name = devnames[ndev++] = argv[i];
		else if (!strcmp(argv[i], "--root") && i + 1 < argc &&
			 argv[i + 1][0] == '/')
			root = argv[++i];
		else if (!strcmp(argv[i], "--prebuilt"))
			mode = ROUTE_PREBUILT;
		else if (!strcmp(argv[i], "--source"))
			mode = ROUTE_SOURCE;
		else if (!strncmp(argv[i], "--build-deps=", 13) &&
			 route_parse(argv[i] + 13) >= 0)
			deps = route_parse(argv[i] + 13);
		else if (!name && argv[i][0] != '-')
			name = argv[i];
		else
			name = NULL, i = argc;
	}
	if (dev && !ndev && name)
		devnames[ndev++] = (char *)name;
	if (pending && !name && !root && !dev) {
		free(devnames);
		return install_pending(c, dry, mode, deps);
	}
	if (!name || !pkg_name_ok(name) || (dev && (mode >= 0 || pending))) {
		fprintf(stderr, "usage: kdos update install [--dry-run] "
				"[--prebuilt | --source] "
				"[--build-deps=prebuilt|source] [--root DIR] "
				"<port|group>\n"
				"       kdos update install [--dry-run] "
				"[--prebuilt | --source] --pending\n"
				"       kdos update install [--dry-run] --dev "
				"[--root DIR] <port|group>...\n");
		free(devnames);
		return 2;
	}
	/* ANOTHER ROOT TAKES PACKAGES, NEVER BUILDS: a build compiles against
	 * the root it runs in, which is not DIR. Its package database, world
	 * set and origin file are DIR's. */
	KpConf rconf;
	if (root) {
		if (mode == ROUTE_SOURCE) {
			fprintf(stderr, "kdos update: --root installs prebuilt "
					"packages only; build from the system "
					"that runs in %s\n", root);
			return 2;
		}
		mode = ROUTE_PREBUILT;
		setenv("KPKG_ROOT", root, 1);
		setenv("KDOS_UPDATE_SYSROOT", root, 1);
		kp_conf_load(&rconf);
		c = &rconf;
	}
	if (dev) {
		int rc = install_dev(c, devnames, ndev, dry, root);
		free(devnames);
		return rc;
	}
	free(devnames);
	if (mode < 0 && (mode = conf_route("install")) < 0)
		return 2;
	if (deps < 0 && (deps = conf_route("build-deps")) < 0)
		return 2;
	if (mode == ROUTE_PREBUILT)
		deps = ROUTE_PREBUILT;
	if (!dry && geteuid() != 0) {
		fprintf(stderr, "kdos update install writes the running root "
				"and needs root\n");
		return 1;
	}
	char *pd = kp_port_dir(c, name);
	if (!pd) {
		/* A group: each member installed as the port it is, by the
		 * same flags, one after another. */
		Group *g = NULL;
		int ng = groups_load(c, &g);
		const Group *gr = group_find(g, ng, name);
		if (!gr) {
			groups_free(g, ng);
			fprintf(stderr, "kdos update: no port or group named %s "
					"in the tree\n", name);
			return 1;
		}
		int worst = 0;
		for (int k = 0; k < gr->n; k++) {
			char *av[8], dbuf[32];
			int an = 0;
			if (dry)
				av[an++] = "--dry-run";
			av[an++] = mode == ROUTE_SOURCE ? "--source" : "--prebuilt";
			snprintf(dbuf, sizeof(dbuf), "--build-deps=%s",
				 route_name(deps));
			av[an++] = dbuf;
			if (root) {
				av[an++] = "--root";
				av[an++] = (char *)root;
			}
			av[an++] = gr->ports[k];
			printf("==> %s: %s (%d of %d)\n", gr->id, gr->ports[k],
			       k + 1, gr->n);
			fflush(stdout);
			int r = cmd_install(c, an, av);
			if (r > worst)
				worst = r;
		}
		groups_free(g, ng);
		return worst;
	}
	free(pd);
	int lock = dry ? -1 : update_lock();
	if (!dry && lock < 0)
		return 1;

	int rc = 1;
	BhIndex ix;
	char why[512];
	Behind *v = NULL;
	KpOrder *ord = NULL;
	DevPlan dp;
	memset(&dp, 0, sizeof(dp));
	char (*seen)[128] = NULL;
	int *st = NULL, *route = NULL;
	int have_ix = bh_load(&ix, why, sizeof(why)) == 0;
	if (!have_ix && deps == ROUTE_PREBUILT) {
		fprintf(stderr, "kdos update: %s; `kdos update fetch` fetches "
				"the release's signed index\n", why);
		if (lock >= 0)
			close(lock);
		return 1;
	}
	if (build_env_for(NULL) != 0)
		fprintf(stderr, "kdos update: no %s records the build's "
				"environment; matching under this one\n",
			BUILD_ENV);

	int orphans = 0;
	int nv = survey(c, &v, &orphans);
	if (nv < 0) {
		fprintf(stderr, "kdos update: no package database to install "
				"into\n");
		goto out;
	}
	char stale[128] = "";
	int nseen = 0;
	seen = kb_calloc(KP_MAX_ORDER, sizeof(*seen));
	if (stale_dep(c, name, v, nv, seen, &nseen, KP_MAX_ORDER, stale,
		      sizeof(stale))) {
		fprintf(stderr, "kdos update: %s is older than its recipe; apply "
				"the system update first\n", stale);
		goto out;
	}

	ord = kb_calloc(1, sizeof(*ord));
	char *one[1] = { (char *)name };
	kp_resolve(c, one, 1, ord);
	/* kp_resolve passes over a root that is installed and current; one
	 * installed by the other route goes in again, alone. */
	if (kp_installed(c, name) && origin_of(name) != mode &&
	    ord->n < KP_MAX_ORDER) {
		int j;
		for (j = 0; j < ord->n && strcmp(ord->order[j], name); j++)
			;
		if (j == ord->n)
			kb_strlcpy(ord->order[ord->n++], name,
				   sizeof(ord->order[0]));
	}
	st = kb_calloc((size_t)ord->n + 1, sizeof(*st));
	route = kb_calloc((size_t)ord->n + 1, sizeof(*route));
	int need = 0, nbad = 0, npre = 0, nsrc = 0;
	long long bytes = 0;
	const char *bad[KP_MAX_ORDER] = { 0 };
	for (int i = 0; i < ord->n; i++) {
		st[i] = -2;
		route[i] = -1;
		int target = !strcmp(ord->order[i], name);
		/* The port itself again by the other route; any other
		 * package installed stays as it is. */
		if (kp_installed(c, ord->order[i]) &&
		    !(target && origin_of(name) != mode))
			continue;
		char *opd = kp_port_dir(c, ord->order[i]);
		if (!opd)
			continue;	/* not a port: nothing to install */
		free(opd);
		int want = target ? mode : deps;
		char w[96] = "";
		st[i] = have_ix ? bh_match(c, &ix, ord->order[i], w, sizeof(w))
				: -1;
		if (want == ROUTE_PREBUILT && st[i] < 0) {
			if (target || mode == ROUTE_PREBUILT) {
				bad[nbad++] = ord->order[i];
				continue;
			}
			want = ROUTE_SOURCE;	/* not in the binhost: built */
		}
		route[i] = want;
		if (want == ROUTE_SOURCE) {
			nsrc++;
		} else {
			npre++;
			if (!bh_present(&ix.st[st[i]]))
				bytes += ix.st[st[i]].size;
		}
		need++;
	}
	if (nbad) {
		char list[200];
		name_list(list, sizeof(list), bad, nbad);
		fprintf(stderr, "kdos update: %d package%s need%s a source build "
				"(%s); the binhost does not hold %s. `kdos update "
				"install --source %s` builds it here\n", nbad,
			nbad == 1 ? "" : "s", nbad == 1 ? "s" : "", list,
			nbad == 1 ? "it" : "them", name);
		goto out;
	}
	/* A BUILD HERE FIRST GETS WHAT IT COMPILES WITH: the build tools when
	 * gcc is missing, and the development files of what it builds against
	 * (dev_prepare). A tool the order also names comes from the plan. */
	if (nsrc) {
		char **src = kb_calloc((size_t)ord->n + 1, sizeof(*src));
		int ns = 0;
		for (int i = 0; i < ord->n; i++)
			if (route[i] == ROUTE_SOURCE)
				src[ns++] = ord->order[i];
		int pr = dev_prepare(c, &ix, have_ix, src, ns, &dp);
		free(src);
		if (pr != 0) {
			fprintf(stderr, "kdos update: %s\n", dp.bad);
			goto out;
		}
		for (int i = 0; i < ord->n; i++) {
			if (route[i] < 0 || !dev_has(&dp, ord->order[i]))
				continue;
			if (route[i] == ROUTE_SOURCE)
				nsrc--;
			else {
				npre--;
				if (!bh_present(&ix.st[st[i]]))
					bytes -= ix.st[st[i]].size;
			}
			route[i] = -1;
			need--;
		}
		need += dp.n;
	}
	if (dry) {
		dev_print(c, &ix, &dp);
		npre += dp.npre;
		bytes += dp.bytes;
		nsrc += dp.nsrc;
		for (int i = 0; i < ord->n; i++) {
			if (route[i] == ROUTE_PREBUILT)
				printf("%s prebuilt %s %lld\n", ord->order[i],
				       ix.st[st[i]].file, ix.st[st[i]].size);
			else if (route[i] == ROUTE_SOURCE) {
				char ver[64] = "", rel[16] = "";
				char *vpd = kp_port_dir(c, ord->order[i]);
				if (vpd) {
					kp_recipe_key(vpd, "version", ver,
						      sizeof(ver));
					kp_recipe_key(vpd, "release", rel,
						      sizeof(rel));
					free(vpd);
				}
				printf("%s source %s-%s\n", ord->order[i], ver,
				       rel[0] ? rel : "1");
			}
		}
		printf("total %d %lld %d\n", npre, bytes, nsrc);
		if (!need)
			printf("%s is already installed\n", name);
		rc = 0;
		goto out;
	}
	if (!need) {
		if (world_add(name) != 0) {
			fprintf(stderr, "kdos update: %s is installed and could "
					"not be added to %s\n", name,
				WORLD_FILE);
			goto out;
		}
		printf("%s is already installed\n", name);
		rc = 0;
		goto out;
	}
	if (dp.n) {
		int fr = dev_fetch(&ix, &dp, why, sizeof(why));
		if (fr != 0) {
			fprintf(stderr, "kdos update: %s\n",
				fr == -2 ? "the network could not be reached"
					 : why);
			goto out;
		}
	}
	for (int i = 0; i < ord->n; i++) {
		int fr = 0;
		if (route[i] == ROUTE_PREBUILT)
			fr = fetch_package(&ix, &ix.st[st[i]], why, sizeof(why));
		else if (route[i] == ROUTE_SOURCE) {
			int kr = run_kpkg("fetch", ord->order[i]);
			fr = kr == 3 ? -2 : kr ? -1 : 0;
			snprintf(why, sizeof(why), "the sources of %s could not "
				 "be fetched", ord->order[i]);
		}
		if (fr != 0) {
			fprintf(stderr, "kdos update: %s\n",
				fr == -2 ? "the network could not be reached"
					 : why);
			goto out;
		}
	}
	if (npre && keyring_into_cache() != 0) {
		fprintf(stderr, "kdos update: the keyring could not be copied "
				"into %s\n", cache_dir());
		goto out;
	}
	char keys[1024];
	snprintf(keys, sizeof(keys), "%s/keys", cache_dir());
	if (nsrc || dp.nsrc)
		setenv("KPKG_FETCH", "1", 1);
	rc = 0;
	if (dp.n || nsrc)
		setenv("KPKG_FACETS", "all", 1);
	if (dp.n && dev_install(&dp, root, why, sizeof(why)) != 0) {
		fprintf(stderr, "kdos update: %s\n", why);
		rc = 1;
	}
	for (int i = 0; i < ord->n && rc == 0; i++) {
		if (route[i] < 0)
			continue;
		printf("==> installing %s (%s)\n", ord->order[i],
		       route[i] == ROUTE_SOURCE ? "built here" : "prebuilt");
		fflush(stdout);
		int ir;
		if (route[i] == ROUTE_SOURCE) {
			/* A package built here carries no signature, and
			 * KPKG_REQUIRE_SIG would refuse it. */
			unsetenv("KPKG_REQUIRE_SIG");
			unsetenv("KPKG_KEYRING");
			ir = run_kpkg("install", ord->order[i]);
		} else {
			setenv("KPKG_KEYRING", keys, 1);
			setenv("KPKG_REQUIRE_SIG", "1", 1);
			ir = run_binhost(root, ord->order[i]);
		}
		if (ir != 0) {
			fprintf(stderr, "kdos update: %s did not install\n",
				ord->order[i]);
			rc = 1;
		} else if (origin_set(ord->order[i], route[i]) != 0) {
			fprintf(stderr, "kdos update: %s is installed and its "
					"route could not be recorded in %s\n",
				ord->order[i], ORIGIN_FILE);
		}
	}
	unsetenv("KPKG_REQUIRE_SIG");
	unsetenv("KPKG_KEYRING");
	unsetenv("KPKG_FETCH");
	unsetenv("KPKG_FACETS");
	if (rc == 0 && world_add(name) != 0) {
		fprintf(stderr, "kdos update: %s is installed and could not be "
				"added to %s; the next update removes it\n",
			name, WORLD_FILE);
		rc = 1;
	}
out:
	dev_free(&dp);
	free(st);
	free(route);
	free(ord);
	free(seen);
	free(v);
	if (have_ix)
		bh_free(&ix);
	if (lock >= 0)
		close(lock);
	return rc;
}

/* ── theme ─────────────────────────────────────────────────────────────── */

/*
 * The gap this closes is small and real: the colour schemes, the Konsole
 * schemes and the qt5ct palettes are generated when the image is built, and
 * what `kdos theme` wrote into $HOME — the colour block of kdeglobals, the
 * qt5ct colours a Qt 5 application paints with — was copied from the release
 * before. Nothing rewrites those copies after an update, so a home keeps the
 * old accent's colours in Qt 5 applications forever. This is `kdos theme
 * <current>` and deliberately nothing more — a post-update hook that did
 * something cleverer would be a second theming path to keep in step with the
 * first.
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
		/* --fetch first, so the survey reads the tree and the index it
		 * fetched. Its failure is recorded and reported as the problem;
		 * the check still answers against what is already here. It is
		 * the networked half of a check, run when somebody asks —
		 * Discover's "Check for updates" through kdos-sysupdated —
		 * and never by the nightly job, which reads only what is in
		 * place. */
		if (fetch && geteuid() != 0) {
			fprintf(stderr, "kdos update check --fetch replaces the "
					"ports tree and needs root; checking "
					"against the tree in place\n");
		} else if (fetch) {
			int lock = update_lock();
			if (lock >= 0) {
				if (tree_fetch(&c, NULL, json || out) != 2) {
					kp_conf_load(&c);
					binhost_fetch(&c, json || out);
				}
				image_fetch(NULL, json || out);
				close(lock);
			}
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
	if (!strcmp(what, "install"))
		return cmd_install(&c, rest, restv);
	if (!strcmp(what, "groups"))
		return cmd_groups(&c, rest, restv);
	if (!strcmp(what, "notes"))
		return cmd_notes(&c, rest, restv);

	fprintf(stderr, "usage: kdos update {check|fetch|apply|install|groups|"
			"notes|theme}\n"
			"  check   installed packages older than their "
			"recipe, which of them the binhost\n"
			"          holds, and what is offered; --json for a "
			"program, --out PATH to write\n"
			"          that document atomically, --fetch to fetch "
			"the newest tree and index first\n"
			"  fetch   replace the ports tree with the newest "
			"signed one and fetch its binhost\n"
			"          index and slot image manifest; --from "
			"URL|DIR; --binhost-only takes\n"
			"          just the binhost index of the tree in "
			"place\n"
			"  apply   write the newer slot image into the inactive "
			"slot, or install what\n"
			"          is behind there from the binhost; --image, "
			"--binhost, --source\n"
			"          (built here) pick one, --acquire/--arm run "
			"one half, --mount\n"
			"          mounts the slot\n"
			"  install a port or a group and what it lacks into "
			"this system: --prebuilt\n"
			"          from the binhost, --source built here "
			"(--build-deps=prebuilt|source);\n"
			"          --root DIR into another root, prebuilt; "
			"--dry-run installs nothing;\n"
			"          --dev installs installed ports again with "
			"their development files\n"
			"  groups  every group of ports, installed or not, "
			"and its download; --json\n"
			"  notes   the release notes of the slot this boot "
			"runs; --json for a program\n"
			"  theme   re-apply the accent to $HOME after an "
			"update\n");
	return 2;
}
