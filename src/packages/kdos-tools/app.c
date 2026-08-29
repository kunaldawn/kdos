/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   kdos app — the applications on this machine, and the ones on the medium
 * ---------------------------------
 *
 *     $ kdos app search image
 *     app.gimp    3.0.4  installed  92.0M  Create images and edit photographs
 *     app.krita   5.2.6  available  180.0M Digital painting
 *     $ kdos app install app.krita
 *     app.krita mounted from the medium
 *
 * THERE IS NO APP STORE HERE AND THAT IS DELIBERATE. On a distro whose medium
 * IS the software library the question is never "where do I get this" — every
 * application is already on the stick. What remains is disposal: what does this
 * cost me, and should I keep it. So this is a handful of verbs and the readings
 * live where the readings already are, in kdos-res.
 *
 * IT NEVER NAMES A PATH TO THE DAEMON. Every verb hands kdos-packd an id out
 * of the list the daemon itself published; the one exception is `install` of a
 * file, which copies into the daemon's own staging directory and then names
 * the FILENAME there. That is rule 3 and it is what keeps a daemon reachable
 * from `wheel` from being `mount /dev/sda2 /etc`.
 */

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <unistd.h>

#include "kbase.h"
#include "kpack.h"
#include "kpkg.h"
#include "kdos-tools.h"

#define APP_MAX 512

typedef struct {
	char id[64];
	char version[64];
	char kind[16];
	char state[16];
	char origin[16];
	unsigned long long size;
} AppRow;

static AppRow rows[APP_MAX];
static int nrows;

static const char *packd_socket(void)
{
	const char *p = getenv("KDOS_PACKD_SOCKET");
	return p && *p ? p : "/run/kdos-packd.sock";
}

/* 0 the daemon said ok, 1 it said err, -1 nothing is listening. The caller
 * must tell the last two apart: "no pack lane on this machine" and "that pack
 * cannot be installed" send a person to different places. */
static int ask(const char *req, char *out, size_t n)
{
	int fd;
	struct sockaddr_un addr = { .sun_family = AF_UNIX };
	KbBuf b = {0};
	char buf[4096];
	ssize_t got;
	int rc = -1;

	if (out && n)
		out[0] = 0;
	fd = socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0);
	if (fd < 0)
		return -1;
	snprintf(addr.sun_path, sizeof(addr.sun_path), "%s", packd_socket());
	if (connect(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
		close(fd);
		return -1;
	}
	dprintf(fd, "%s\n", req);
	while ((got = read(fd, buf, sizeof(buf))) > 0)
		kb_buf_add(&b, buf, (size_t)got);
	close(fd);
	if (!b.p)
		return -1;

	{
		char *last = b.p;
		for (size_t i = 0; i + 1 < b.n; i++)
			if (b.p[i] == '\n')
				last = b.p + i + 1;
		if (!strncmp(last, "ok", 2)) {
			rc = 0;
			if (out && n) {
				size_t pre = (size_t)(last - b.p);
				if (pre) {		/* a listing */
					if (pre >= n)
						pre = n - 1;
					memcpy(out, b.p, pre);
					out[pre] = 0;
				} else {
					kb_strlcpy(out, last[2] == ' ' ? last + 3
								       : "", n);
					out[strcspn(out, "\r\n")] = 0;
				}
			}
		} else if (!strncmp(last, "err", 3)) {
			rc = 1;
			if (out && n) {
				kb_strlcpy(out, last[3] == ' ' ? last + 4 : last, n);
				out[strcspn(out, "\r\n")] = 0;
			}
		}
	}
	kb_buf_free(&b);
	return rc;
}

static int no_daemon(void)
{
	fprintf(stderr,
		"kdos app: kdos-packd is not answering on %s.\n"
		"          `service packd status` says whether it started; it\n"
		"          skips itself when the kernel has no erofs.\n",
		packd_socket());
	return 2;
}

static int load_msg(char *why, size_t wn)
{
	char *buf = kb_calloc(1, 1 << 16);
	char *line, *save;
	int rc = ask("list", buf, 1 << 16);

	nrows = 0;
	if (rc != 0) {
		if (why && wn)
			kb_strlcpy(why, buf, wn);
		free(buf);
		return rc;
	}
	for (line = strtok_r(buf, "\n", &save); line && nrows < APP_MAX;
	     line = strtok_r(NULL, "\n", &save)) {
		char *f[6] = {0};
		int nf = 0;
		char *tok, *s2 = NULL;
		AppRow *r;

		for (tok = strtok_r(line, "\t", &s2); tok && nf < 6;
		     tok = strtok_r(NULL, "\t", &s2))
			f[nf++] = tok;
		if (nf < 6)
			continue;
		r = &rows[nrows++];
		kb_strlcpy(r->id, f[0], sizeof(r->id));
		kb_strlcpy(r->version, f[1], sizeof(r->version));
		kb_strlcpy(r->kind, f[2], sizeof(r->kind));
		kb_strlcpy(r->state, f[3], sizeof(r->state));
		r->size = strtoull(f[4], NULL, 10);
		kb_strlcpy(r->origin, f[5], sizeof(r->origin));
	}
	free(buf);
	return 0;
}

static const AppRow *find(const char *id)
{
	for (int i = 0; i < nrows; i++)
		if (!strcmp(rows[i].id, id))
			return &rows[i];
	return NULL;
}

/* One field out of `info`, which is the metadata blob the pack carries. */
static int info_key(const char *id, const char *key, char *out, size_t n)
{
	char req[256];
	char *buf = kb_calloc(1, 1 << 15);
	char *line, *save;
	int found = 0;

	out[0] = 0;
	snprintf(req, sizeof(req), "info %s", id);
	if (ask(req, buf, 1 << 15) != 0) {
		free(buf);
		return -1;
	}
	for (line = strtok_r(buf, "\n", &save); line && !found;
	     line = strtok_r(NULL, "\n", &save)) {
		char *eq = strchr(line, '=');
		char k[64];
		size_t kl;
		if (!eq)
			continue;
		kl = (size_t)(eq - line);
		while (kl && (line[kl - 1] == ' ' || line[kl - 1] == '\t'))
			kl--;
		if (kl >= sizeof(k))
			continue;
		memcpy(k, line, kl);
		k[kl] = 0;
		if (strcmp(k, key))
			continue;
		eq++;
		while (*eq == ' ')
			eq++;
		kb_strlcpy(out, eq, n);
		found = 1;
	}
	free(buf);
	return found ? 0 : -1;
}

/* ── the verbs ─────────────────────────────────────────────────────────── */

static void row_print(const AppRow *r, const char *summary)
{
	printf("  %-22s %-9s %-10s %8s  %s\n", r->id, r->version, r->state,
	       kb_human_size(r->size), summary ? summary : "");
}

static int cmd_list(int all)
{
	int n = 0;

	for (int i = 0; i < nrows; i++) {
		char sum[256] = "";
		if (!all && strcmp(rows[i].kind, "app"))
			continue;
		info_key(rows[i].id, "summary", sum, sizeof(sum));
		row_print(&rows[i], sum);
		n++;
	}
	if (!n)
		printf("  nothing%s\n", all ? "" : " — no application packs");
	return 0;
}

static int cmd_search(const char *q)
{
	int n = 0;

	for (int i = 0; i < nrows; i++) {
		char sum[256] = "", name[256] = "", cat[128] = "";
		info_key(rows[i].id, "summary", sum, sizeof(sum));
		info_key(rows[i].id, "name", name, sizeof(name));
		info_key(rows[i].id, "category", cat, sizeof(cat));
		if (!strcasestr(rows[i].id, q) && !strcasestr(sum, q) &&
		    !strcasestr(name, q) && !strcasestr(cat, q))
			continue;
		row_print(&rows[i], sum[0] ? sum : name);
		n++;
	}
	if (!n) {
		printf("  nothing matches '%s'\n", q);
		return 1;
	}
	return 0;
}

static int cmd_show(const char *id)
{
	char req[256];
	char *buf = kb_calloc(1, 1 << 15);
	char needs[256] = "";
	const AppRow *r = find(id);

	if (!r) {
		fprintf(stderr, "kdos app: no pack '%s'\n", id);
		free(buf);
		return 1;
	}
	snprintf(req, sizeof(req), "info %s", id);
	if (ask(req, buf, 1 << 15) != 0) {
		free(buf);
		return 1;
	}
	fputs(buf, stdout);
	free(buf);

	/*
	 * A PROGRAM WHOSE DATABASE IS EMPTY IS NOT SHIPPED SOFTWARE, IT IS A
	 * BROKEN MENU ENTRY. A pack that needs a dataset says so in its
	 * metadata and this prints whether that dataset is here — installing
	 * the application and discovering the emptiness later is the failure
	 * this line exists to prevent.
	 */
	if (info_key(id, "needs", needs, sizeof(needs)) == 0 && needs[0]) {
		const AppRow *d = find(needs);
		printf("\nneeds %s — %s\n", needs,
		       !d			 ? "NOT on this machine; the "
						   "application will open empty"
		       : !strcmp(d->state, "available") ? "on the medium, not yet "
							  "installed"
							: "installed");
	}
	return 0;
}

/*
 * On a live medium a pack is mounted where it lies: ISO9660 is readable, the
 * pack costs nothing until it is mounted, and copying 180 MB into a tmpfs
 * $HOME to install something that is already on the stick is the opposite of
 * what the medium is for. On an installed system the pack is COPIED into the
 * daemon's staging directory and installed from there, because the medium may
 * not be in the machine tomorrow.
 */
/*
 * A PROGRAM WHOSE DATABASE IS EMPTY IS NOT SHIPPED SOFTWARE, it is a broken
 * menu entry. So a pack that is useless without a dataset names it in
 * `needs =`, and installing it SAYS what is missing and where it is rather
 * than launching into an empty window. `kdos app show` prints the same thing
 * before the fact; this is the half that reaches somebody who never asked.
 */
static void warn_needs(const char *id)
{
	char needs[256] = "";

	if (info_key(id, "needs", needs, sizeof(needs)) != 0 || !needs[0])
		return;
	for (char *tok = strtok(needs, " ,"); tok; tok = strtok(NULL, " ,")) {
		const AppRow *d = find(tok);

		if (d && (!strcmp(d->state, "installed") ||
			  !strcmp(d->state, "mounted")))
			continue;
		if (d)
			printf("  it needs %s, which is on the medium — "
			       "`kdos app install %s`\n", tok, tok);
		else
			printf("  it needs %s, which is NOT on this medium — "
			       "the application will open with no data in it\n",
			       tok);
	}
}

/*
 * AN INSTALLED APPLICATION IS IN THE MENU BEFORE THIS COMMAND RETURNS. The
 * launchers, the shim and the mime rows for installed packs are written into
 * the USER's tree by `genlaunchers --packs --user`, as the calling user, so
 * nothing here needs root and nothing has to be remembered afterwards. A
 * failure is reported and does not fail the install: the pack IS installed,
 * and "installed but not in the menu" with a sentence saying so is better than
 * "install failed" over a launcher.
 */
static void relaunchers(void)
{
	KbArgv a = {0};
	/* Its stderr is the diagnosis when it fails — `cannot write X` names
	 * the path — and kb_run drops a child's stderr unless told not to. */
	kb_proc_verbose = 1;
	kb_argv_add(&a, "kdos-appbox");
	kb_argv_add(&a, "genlaunchers");
	kb_argv_add(&a, "--packs");
	kb_argv_add(&a, "--user");
	kb_argv_end(&a);
	if (kb_run(&a) != 0)
		fprintf(stderr, "kdos app: launchers not regenerated — run "
				"`kdos-appbox genlaunchers --packs --user`\n");
}

/*
 * A RUNNING BOX KEEPS THE OLD BYTES. Mounts are reference-counted, so a pack
 * that is replaced stays mounted under every box composed on it — correct, or
 * the running application's paths would go ENOENT — and the box runs the
 * previous version until it is restarted, with nothing saying so. This says
 * so, by name, so an update the user cannot see take effect is at least an
 * update the user was told about.
 */
static void warn_running(const char *id)
{
	char buf[8192];
	KbArgv a = {0};
	char *line, *save;
	int n = 0;

	kb_argv_add(&a, "kdos-box");
	kb_argv_add(&a, "list");
	kb_argv_end(&a);
	if (kb_run_capture(&a, buf, sizeof(buf)) != 0)
		return;
	for (line = strtok_r(buf, "\n", &save); line;
	     line = strtok_r(NULL, "\n", &save)) {
		char name[64], base[64], state[32];
		if (sscanf(line, "%63s %63s %31s", name, base, state) != 3)
			continue;
		if (strcmp(state, "running"))
			continue;
		/* the box IS the app pack, or it composes the runtime */
		if (!strcmp(base + (strncmp(base, "pack:", 5) ? 0 : 5), id) ||
		    !strcmp(name, id)) {
			if (!n)
				printf("  running on the previous version — restart "
				       "to pick this up:");
			printf(" %s", name);
			n++;
		}
	}
	if (n)
		printf("\n");
}

static int cmd_install(const char *id)
{
	const AppRow *r = find(id);
	char msg[512], req[256];
	int rc;

	if (!r) {
		fprintf(stderr, "kdos app: no pack '%s' — `kdos app search` "
				"lists what is here\n", id);
		return 1;
	}
	if (!strcmp(r->state, "installed") || !strcmp(r->state, "mounted")) {
		printf("%s %s is already installed\n", r->id, r->version);
		return 0;
	}

	/* A live session: $HOME is on the boot overlay, so a copy would go into
	 * RAM. Mount it where it is and say so. */
	if (kb_path_exists("/mnt/iso/packs")) {
		snprintf(req, sizeof(req), "mount %s", id);
		rc = ask(req, msg, sizeof(msg));
		if (rc == 0) {
			printf("%s mounted from the medium — not copied, so it "
			       "is gone when this session ends\n", id);
			/* A data pack is mounted noexec and never composed: what
			 * it is FOR is its grafts, and a mount with none made is
			 * a dataset nothing can find. */
			if (!strcmp(r->kind, "data")) {
				snprintf(req, sizeof(req), "graft %s", id);
				if (ask(req, msg, sizeof(msg)) == 0)
					printf("%s\n", msg);
				else
					fprintf(stderr, "kdos app: %s\n", msg);
			}
			warn_needs(id);
			relaunchers();
			return 0;
		}
		if (rc < 0)
			return no_daemon();
		fprintf(stderr, "kdos app: %s\n", msg);
		return 1;
	}
	fprintf(stderr, "kdos app: %s is on the medium and this is not a live "
			"session — the medium is not mounted\n", id);
	return 1;
}

static int cmd_simple(const char *verb, const char *id)
{
	char req[256], msg[512];
	int rc;

	snprintf(req, sizeof(req), "%s %s", verb, id);
	rc = ask(req, msg, sizeof(msg));
	if (rc < 0)
		return no_daemon();
	if (rc) {
		fprintf(stderr, "kdos app: %s\n", msg);
		return 1;
	}
	printf("%s\n", msg[0] ? msg : "ok");
	return 0;
}

/* ── update ────────────────────────────────────────────────────────────── */

/*
 * WHERE AN UPDATE COMES FROM. A source is a directory with a `PACKAGES` index
 * in it: the medium under `/mnt/iso/packs`, an installed store's own copy, and
 * whatever `/etc/kdos/pack-sources` lists — one path per line. On a distro
 * whose medium IS the software library the interesting case is "the stick I
 * just wrote is newer than the disk".
 *
 * A URL IS NEVER ONE OF THEM AND IS NEVER CONFIGURED. Reaching the network is
 * `--online <url>` on the command line and nothing else: an argument is
 * visible at the moment it is used, where a line in a config file is invisible
 * until the day it surprises somebody. Nothing this machine does by itself
 * ever leaves it.
 */
static int sources(char out[8][KPK_PATH])
{
	char st[4096], buf[4096];
	int n = 0;

	if (ask("status", st, sizeof(st)) == 0) {
		char *line, *save;
		for (line = strtok_r(st, "\n", &save); line && n < 8;
		     line = strtok_r(NULL, "\n", &save)) {
			char *tab = strchr(line, '\t');
			if (!tab || strncmp(line, "medium", 6))
				continue;
			kb_strlcpy(out[n++], tab + 1, KPK_PATH);
		}
	}
	/* `> 0`, not `== 0`: kb_read_file answers the BYTE COUNT, so `== 0` is
	 * true only for an EMPTY file — every path somebody listed here was
	 * skipped, and `kdos app update` saw the medium and nothing else. */
	if (kb_read_file("/etc/kdos/pack-sources", buf, sizeof(buf)) > 0) {
		char *line, *save;
		for (line = strtok_r(buf, "\n", &save); line && n < 8;
		     line = strtok_r(NULL, "\n", &save)) {
			while (*line == ' ' || *line == '\t')
				line++;
			if (!*line || *line == '#')
				continue;
			line[strcspn(line, " \t\r")] = 0;
			kb_strlcpy(out[n++], line, KPK_PATH);
		}
	}
	return n;
}

/* The staging directory the DAEMON published. Reconstructing it here would be
 * a second definition of where an unprivileged write is allowed. */
static int staging(char *out, size_t n)
{
	char st[4096], *line, *save;

	if (ask("status", st, sizeof(st)) != 0)
		return -1;
	for (line = strtok_r(st, "\n", &save); line;
	     line = strtok_r(NULL, "\n", &save)) {
		char *tab = strchr(line, '\t');
		if (tab && !strncmp(line, "staging", 7)) {
			kb_strlcpy(out, tab + 1, n);
			return 0;
		}
	}
	return -1;
}

/*
 * One update. The delta is taken when the pack it patches is STILL IN THE
 * STORE, which is the ordinary case for a machine that has been updating
 * rather than installing fresh; when it is gone the full pack is the answer
 * and nothing is lost.
 *
 * NOTHING HERE VERIFIES ANYTHING, deliberately. The reconstruction lands in
 * the staging directory and kdos-packd hashes it against the pack's own
 * footer and checks the signature where the mount happens. A tampered delta
 * produces a pack that fails there; it cannot make this machine install
 * something the index did not already name.
 */
/*
 * A SOURCE MAY BE A URL, and exactly two places have to know: where the index
 * is read from and where a file is staged from. Everything between them —
 * the version compare, the delta choice, the staging directory — is identical,
 * because a release over HTTP is the same shape as a stick: a directory with a
 * `PACKAGES` index and the files it names.
 */
static int is_url(const char *s)
{
	return !strncmp(s, "https://", 8) || !strncmp(s, "http://", 7);
}

/*
 * Put `<dir>/<name>` at `dst`, whether dir is a path or a URL.
 *
 * NOTHING HERE VERIFIES ANYTHING, and it does not need to: the file lands in
 * the staging directory and kdos-packd hashes it against the `C:` in the
 * signed index where it MOUNTS it. A tampered download produces a pack that
 * fails there and cannot make the machine install something the index did not
 * already name — the rule the whole format is built on.
 */
static int stage_from(const char *dir, const char *name, const char *dst)
{
	char src[KPK_PATH * 2];

	if (!is_url(dir)) {
		snprintf(src, sizeof(src), "%s/%s", dir, name);
		return kb_copy_file(src, dst);
	}
	KbArgv a = {0};
	kb_argv_add(&a, "curl");
	kb_argv_add(&a, "-fsSL");
	kb_argv_add(&a, "--retry");
	kb_argv_add(&a, "3");
	kb_argv_add(&a, "-o");
	kb_argv_add(&a, dst);
	kb_argv_addf(&a, "%s/%s", dir, name);
	kb_argv_end(&a);
	return kb_run(&a);
}

static int fetch_one(const char *dir, const KpkIndex *ix,
		     const KpkIndexEnt *want, const char *have_version,
		     const char *stage, char *file, size_t fn)
{
	char src[KPK_PATH * 2], dst[KPK_PATH * 2], old[KPK_PATH * 2];
	const KpkIndexEnt *d = NULL;
	char store[KPK_PATH] = "/var/lib/kdos/packs";
	char oldname[KPK_ID_MAX + 96];

	snprintf(oldname, sizeof(oldname), "%s-%s.kpack", want->id, have_version);
	snprintf(old, sizeof(old), "%s/%s", store, oldname);
	/* The version being replaced is `<id>.kpack` in the store until the
	 * install renames it; that is the file a delta patches. */
	if (kb_path_exists(old))
		d = kpk_index_delta(ix, want->id, want->version, oldname);
	if (!d) {
		snprintf(old, sizeof(old), "%s/%s.kpack", store, want->id);
		if (kb_path_exists(old)) {
			char cur[KPK_ID_MAX + 96];
			snprintf(cur, sizeof(cur), "%s-%s.kpack", want->id,
				 have_version);
			d = kpk_index_delta(ix, want->id, want->version, cur);
		}
	}

	snprintf(file, fn, "%s.kpack", want->id);
	snprintf(dst, sizeof(dst), "%s/%s", stage, file);

	if (d) {
		KbArgv a = {0};
		char dl[KPK_PATH * 2];

		/* A delta is fetched like anything else, then applied against
		 * the pack already on disk. */
		snprintf(dl, sizeof(dl), "%s/%s", stage, d->file);
		if (stage_from(dir, d->file, dl) != 0) {
			fprintf(stderr, "  %s: cannot fetch the delta\n", want->id);
			d = NULL;
			goto whole;
		}
		kb_strlcpy(src, dl, sizeof(src));
		kb_argv_add(&a, "kdos-pack");
		kb_argv_add(&a, "apply");
		kb_argv_add(&a, old);
		kb_argv_add(&a, src);
		kb_argv_add(&a, dst);
		kb_argv_end(&a);
		if (kb_run(&a) == 0) {
			printf("  %-22s %s -> %s  via a %s delta\n", want->id,
			       have_version, want->version,
			       kb_human_size(d->size));
			return 0;
		}
		fprintf(stderr, "  %s: the delta would not apply — taking the "
				"whole pack\n", want->id);
	}

whole:
	if (stage_from(dir, want->file, dst) != 0) {
		fprintf(stderr, "  %s: cannot stage %s from %s\n", want->id,
			want->file, dir);
		return -1;
	}
	printf("  %-22s %s -> %s  %s\n", want->id, have_version, want->version,
	       kb_human_size(want->size));
	return 0;
}

/* `--check`: dry, one line, exit 1 when something is newer. The seam a panel
 * or a device manager asks — a stick that IS newer than the disk had no
 * surface at all before this, only a verb somebody had to know to type. */
static int check_mode;

static int cmd_update(const char *only, int dry, const char *url)
{
	char dirs[8][KPK_PATH];
	char stage[KPK_PATH];
	int ndir = sources(dirs), done = 0, seen = 0, failed = 0;

	/*
	 * `--online` IS THE ONLY WAY TO THE NETWORK, and it is an argument
	 * rather than a setting: nothing this machine does on its own ever
	 * leaves it. The URL is a directory holding a `PACKAGES` index and the
	 * files it names — a release, a mirror, a web server, the same shape
	 * as a stick.
	 */
	if (url) {
		if (!is_url(url)) {
			fprintf(stderr, "--online wants an http(s) URL\n");
			return 1;
		}
		if (ndir >= 8) {
			fprintf(stderr, "too many sources to add one more\n");
			return 1;
		}
		printf("online: %s\n", url);
		kb_strlcpy(dirs[ndir++], url, KPK_PATH);
	}

	if (!ndir) {
		printf("no sources — nothing to update from\n");
		printf("  a stick with a PACKAGES index on it, or --online <url>\n");
		return 0;
	}
	if (!dry && staging(stage, sizeof(stage)) != 0)
		return no_daemon();

	for (int d = 0; d < ndir; d++) {
		char path[KPK_PATH * 2];
		KpkIndex *ix;

		if (is_url(dirs[d])) {
			/* The index comes down first; everything after it is
			 * decided from what it says. */
			snprintf(path, sizeof(path), "%s/PACKAGES.online", stage);
			if (stage_from(dirs[d], "PACKAGES", path) != 0) {
				fprintf(stderr, "%s: no PACKAGES there\n", dirs[d]);
				failed++;
				continue;
			}
		} else {
			snprintf(path, sizeof(path), "%s/PACKAGES", dirs[d]);
			if (!kb_path_exists(path))
				continue;
		}
		ix = kb_calloc(1, sizeof(*ix));
		if (kpk_index_load(ix, path) <= 0) {
			free(ix);
			continue;
		}
		/*
		 * A TRUNCATED INDEX IS SAID OUT LOUD. The cap is
		 * KPK_INDEX_MAX and an update that silently skipped the tail
		 * would report a machine as current that is not.
		 */
		if (ix->truncated)
			fprintf(stderr, "  %s holds more than %d packs — the "
					"tail was not read\n", path,
				KPK_INDEX_MAX);

		for (int i = 0; i < nrows; i++) {
			const KpkIndexEnt *e;
			char file[KPK_ID_MAX + 16], req[512], msg[512];

			if (strcmp(rows[i].origin, "store"))
				continue;	/* not installed here */
			if (only && strcmp(rows[i].id, only))
				continue;
			seen++;
			e = kpk_index_find(ix, rows[i].id);
			if (!e || kp_vercmp(e->version, rows[i].version) <= 0)
				continue;
			if (check_mode) {
				done++;
				continue;
			}
			if (dry) {
				printf("  %-22s %s -> %s\n", rows[i].id,
				       rows[i].version, e->version);
				done++;
				continue;
			}
			if (fetch_one(dirs[d], ix, e, rows[i].version, stage,
				      file, sizeof(file)) != 0) {
				failed++;
				continue;
			}
			snprintf(req, sizeof(req), "install %s", file);
			if (ask(req, msg, sizeof(msg)) != 0) {
				fprintf(stderr, "  %s: kdos-packd refused: %s\n",
					rows[i].id, msg);
				failed++;
				continue;
			}
			done++;
			warn_running(rows[i].id);
		}
		free(ix);
	}

	if (check_mode) {
		if (done)
			printf("%d update%s on the medium\n", done, done == 1 ? "" : "s");
		else
			printf("up to date\n");
		return done ? 1 : 0;
	}
	if (only && !seen) {
		fprintf(stderr, "kdos app: %s is not installed here\n", only);
		return 1;
	}
	if (!done)
		printf("everything is current\n");
	else
		printf("\n%d %s%s. `kdos app rollback <id>` undoes one.\n", done,
		       dry ? "update available" : "updated",
		       done == 1 ? "" : "s");
	return failed ? 1 : 0;
}

static int cmd_sources(void)
{
	char st[4096];

	if (ask("status", st, sizeof(st)) != 0)
		return no_daemon();
	fputs(st, stdout);
	printf("\nA pack is verified where it is mounted, never by the client.\n"
	       "The store was checked when root wrote it; a pack on the medium\n"
	       "is hashed the first time it is mounted, because that is the only\n"
	       "time anything looks at it.\n");
	return 0;
}

static int usage(void)
{
	fprintf(stderr,
		"usage: kdos app list [--all]\n"
		"       kdos app search <text>\n"
		"       kdos app show <id>\n"
		"       kdos app install <id>\n"
		"       kdos app remove <id>\n"
		"       kdos app rollback <id>\n"
		"       kdos app update [<id>] [--dry-run] [--online <url>]\n"
		"       kdos app sources\n"
		"\nAn application is one signed file. Installing it is a mount.\n"
		"To install something that is not packed at all, from a network:\n"
		"kdos-fetch-app <name>.\n");
	return 2;
}

int kdt_app(int argc, char **argv)
{
	const char *cmd;

	if (argc < 1)
		return usage();
	cmd = argv[0];

	if (!strcmp(cmd, "-h") || !strcmp(cmd, "--help"))
		return usage();

	{
		/*
		 * THREE ANSWERS, NOT TWO. `err` here is almost always the
		 * SO_PEERCRED gate — root and `wheel` — and reporting it as an
		 * empty catalogue sends somebody looking for missing packs
		 * instead of for their group membership.
		 */
		char why[256];
		int rc = load_msg(why, sizeof(why));
		if (rc < 0)
			return no_daemon();
		if (rc > 0) {
			fprintf(stderr, "kdos app: kdos-packd refused: %s\n", why);
			return 2;
		}
	}

	if (!strcmp(cmd, "list"))
		return cmd_list(argc > 1 && !strcmp(argv[1], "--all"));
	if (!strcmp(cmd, "search") && argc > 1)
		return cmd_search(argv[1]);
	if (!strcmp(cmd, "show") && argc > 1)
		return cmd_show(argv[1]);
	if (!strcmp(cmd, "install") && argc > 1)
		return cmd_install(argv[1]);
	if (!strcmp(cmd, "remove") && argc > 1) {
		/* A PACK MOUNTED OFF THE MEDIUM IS NOT IN THE STORE, so the
		 * daemon's `remove` refuses it — "on the medium, not
		 * installed" — and the launchers it got at install time stayed
		 * behind. What "remove" means for a live mount is unmount. */
		const AppRow *r = find(argv[1]);
		int rc;
		if (r && !strcmp(r->kind, "data"))
			cmd_simple("ungraft", argv[1]);
		rc = cmd_simple(r && !strcmp(r->state, "mounted") ? "unmount"
								  : "remove", argv[1]);
		if (rc == 0)
			relaunchers();
		return rc;
	}
	if (!strcmp(cmd, "rollback") && argc > 1)
		return cmd_simple("rollback", argv[1]);
	if (!strcmp(cmd, "graft") && argc > 1)
		return cmd_simple("graft", argv[1]);
	if (!strcmp(cmd, "ungraft") && argc > 1)
		return cmd_simple("ungraft", argv[1]);
	if (!strcmp(cmd, "update")) {
		const char *id = NULL, *url = NULL;
		int dry = 0;
		for (int i = 1; i < argc; i++) {
			if (!strcmp(argv[i], "--dry-run"))
				dry = 1;
			else if (!strcmp(argv[i], "--check"))
				dry = check_mode = 1;
			else if (!strcmp(argv[i], "--online")) {
				if (++i >= argc)
					return usage();
				url = argv[i];
			} else if (argv[i][0] != '-')
				id = argv[i];
			else
				return usage();
		}
		return cmd_update(id, dry, url);
	}
	if (!strcmp(cmd, "sources"))
		return cmd_sources();
	return usage();
}
