/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   fetch — a port's sources, onto a machine that has only its recipe
 *
 *   kpkg fetch <pkg>...
 *   KPKG_FETCH=1 kpkg install <pkg>...
 *
 * A shipped machine carries the recipe-only ports tree: every `kpkgbuild`,
 * `build.sh` and patch, and no source tarball. Building a port there needs its
 * sources first, and this is where they come from. Every file a `sha256 =` line
 * names, and that is in neither the port directory nor $SOURCE_DIR, is resolved
 * in the order `ports/fetch` resolves it on a build host:
 *
 *   1. the archive    $KDOS_SOURCES_BASE/<tag>/<asset>, both read from the
 *                     tree's own `sources.idx`; a hash it does not name skips
 *                     this, and an empty KDOS_SOURCES_BASE skips it always
 *   2. upstream       the `source =` URL that names this filename
 *
 * The hash is the identity: whichever copy arrives is used only once it hashes
 * to the recipe's `sha256 =`, and a copy that does not is deleted. A verified
 * file lands in $SOURCE_DIR, where the build's own verify_declared() and
 * extract_sources() already look; a file no `source =` entry names (a vendor
 * bundle, which build.sh opens as $PORT_SRC/<file>) is also symlinked into the
 * port directory.
 *
 * THE BUILD FETCHES ONLY WHEN ASKED. `make build` runs with no network and
 * must never reach for one — a build that quietly downloaded would stop being
 * a build of the fetched tree. KPKG_FETCH=1 is what `kdos update apply` sets;
 * without it a missing source is an error.
 *
 * Exit status: 0 every source is present, 1 one could not be had (no copy
 * verified, or nowhere names it), 3 the NETWORK could not be reached — so a
 * caller can tell a machine that is offline from a source that has gone.
 * ---------------------------------
 */

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "kdos-kpkg.h"

#define IDX_MARKER "# kdos-sources-index 2"
#define IDX_DEFAULT_BASE "https://github.com/kunaldawn/kdos/releases/download"

/* What one download attempt came to. */
enum { GOT_OK, GOT_FAIL, GOT_OFFLINE };

/*
 * curl's own exit codes for "never reached a server": 5 and 6 a name that did
 * not resolve, 7 a connection refused or unroutable, 28 a timeout. Everything
 * else — a 404 (22), a transfer cut short — reached somebody and is a failure
 * of THAT copy, not of the machine's network.
 */
static int curl_offline(int rc)
{
	return rc == 5 || rc == 6 || rc == 7 || rc == 28;
}

static int download(const char *url, const char *out)
{
	KbArgv a = {0};
	kb_argv_add(&a, "curl");
	kb_argv_add(&a, "-fL");
	kb_argv_add(&a, isatty(STDOUT_FILENO) ? "-#" : "-sS");
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
	return curl_offline(rc) ? GOT_OFFLINE : GOT_FAIL;
}

/* A downloaded file that does not hash to `want` is removed: it is not the
 * file, whatever its name says. */
static int hash_ok(const char *path, const char *want)
{
	char got[65];
	if (kb_sha256_file(path, got) == 0 && kb_str_ieq(got, want))
		return 1;
	unlink(path);
	return 0;
}

/* ── the archive ───────────────────────────────────────────────────────── */

/*
 * The tree's `sources.idx`, found by walking up from the port directory: the
 * recipe tree keeps it at its root, beside `core/`, so a port at
 * <tree>/core/<shelf>/<name> finds it three levels up. A tree with no index —
 * our own programs' `src/` ports, which name no source — finds none, and
 * nothing is fetched from the archive.
 */
static char *find_index(const char *portdir)
{
	char dir[1024];
	kb_strlcpy(dir, portdir, sizeof(dir));
	for (int up = 0; up < 4; up++) {
		char *p = kb_path_join(dir, "sources.idx");
		if (kb_path_exists(p))
			return p;
		free(p);
		char *slash = strrchr(dir, '/');
		if (!slash || slash == dir)
			break;
		*slash = 0;
	}
	return NULL;
}

/*
 * The archive's base URL. KDOS_SOURCES_BASE is the variable `ports/fetch`
 * reads, with the same meaning: unset is the published archive, empty is no
 * archive at all, and a value with no scheme is a directory laid out as
 * <dir>/<tag>/<asset> — an archive disk — read through curl as a file:// URL.
 * Returns 0 and leaves `out` empty when the archive is skipped.
 */
static void archive_base(char *out, size_t cap)
{
	const char *e = getenv("KDOS_SOURCES_BASE");
	out[0] = 0;
	if (!e) {
		kb_strlcpy(out, IDX_DEFAULT_BASE, cap);
		return;
	}
	if (!*e)
		return;
	if (strstr(e, "://"))
		kb_strlcpy(out, e, cap);
	else if (e[0] == '/')
		snprintf(out, cap, "file://%s", e);
}

/* The index line for `hash`, split into its fields. 0 found, -1 not. */
static int index_lookup(const char *idx, const char *hash, char *tag,
			size_t tcap, char *asset, size_t acap, char *parts,
			size_t pcap)
{
	char *data = kb_read_all(idx, NULL);
	if (!data)
		return -1;
	int found = -1;
	char *save = NULL;
	char *line = strtok_r(data, "\n", &save);
	/* A file whose first line is not the format-2 marker is not read at
	 * all: a line of another format must never be taken for an address. */
	if (!line || strcmp(line, IDX_MARKER)) {
		free(data);
		return -1;
	}
	size_t hl = strlen(hash);
	while ((line = strtok_r(NULL, "\n", &save))) {
		if (strncmp(line, hash, hl) || line[hl] != ' ')
			continue;
		char f_tag[128], f_asset[512], f_port[512], f_parts[4096];
		f_parts[0] = 0;
		int n = sscanf(line + hl, " %127s %511s %511s %4095s", f_tag,
			       f_asset, f_port, f_parts);
		if (n < 3)
			break;
		kb_strlcpy(tag, f_tag, tcap);
		kb_strlcpy(asset, f_asset, acap);
		kb_strlcpy(parts, n == 4 && !strncmp(f_parts, "parts=", 6)
				  ? f_parts + 6 : "", pcap);
		found = 0;
		break;
	}
	free(data);
	return found;
}

static int append_file(const char *from, FILE *to)
{
	FILE *f = fopen(from, "rb");
	if (!f)
		return -1;
	char buf[65536];
	size_t n;
	int rc = 0;
	while ((n = fread(buf, 1, sizeof(buf), f)) > 0)
		if (fwrite(buf, 1, n, to) != n) {
			rc = -1;
			break;
		}
	if (ferror(f))
		rc = -1;
	fclose(f);
	return rc;
}

/*
 * A file in parts: `<N>:<h1>,<h2>,…` names each part's own hash, and the parts
 * are `<asset>.part01` … in order. Each part is checked as it arrives, then
 * the joined file is checked against the recipe's hash as a whole.
 */
static int archive_parts(const char *url, const char *parts, const char *tmp)
{
	int n = atoi(parts);
	const char *h = strchr(parts, ':');
	if (n < 1 || !h)
		return GOT_FAIL;
	h++;

	FILE *out = fopen(tmp, "wb");
	if (!out)
		return GOT_FAIL;
	char ptmp[1200];
	if ((size_t)snprintf(ptmp, sizeof(ptmp), "%s.p", tmp) >= sizeof(ptmp)) {
		fclose(out);
		unlink(tmp);
		return GOT_FAIL;
	}
	int got = GOT_OK;
	for (int i = 1; i <= n && got == GOT_OK; i++) {
		char want[65];
		size_t len = strcspn(h, ",");
		if (len != 64) {
			got = GOT_FAIL;
			break;
		}
		memcpy(want, h, 64);
		want[64] = 0;
		h += len + (h[len] == ',');

		char purl[2200];
		if ((size_t)snprintf(purl, sizeof(purl), "%s.part%02d", url,
				     i) >= sizeof(purl)) {
			got = GOT_FAIL;
			break;
		}
		got = download(purl, ptmp);
		if (got != GOT_OK)
			break;
		if (!hash_ok(ptmp, want) || append_file(ptmp, out) != 0)
			got = GOT_FAIL;
		unlink(ptmp);
	}
	if (fclose(out) != 0 && got == GOT_OK)
		got = GOT_FAIL;
	if (got != GOT_OK)
		unlink(tmp);
	return got;
}

/* ── one file ──────────────────────────────────────────────────────────── */

/*
 * The URL among the recipe's `source =` entries whose cached name is `file`,
 * or NULL. A vendor bundle is named by no entry and has none: on a machine it
 * comes from the archive or not at all.
 */
static const char *upstream_url(const KpDecl *d, const char *file, char *buf,
				size_t cap)
{
	char list[4096];
	kb_strlcpy(list, kp_decl_source(d), sizeof(list));
	char *save = NULL;
	int idx = 0;
	for (char *t = strtok_r(list, " \t\n", &save); t;
	     t = strtok_r(NULL, " \t\n", &save), idx++) {
		char name[512];
		kp_source_file(kp_decl_name(d), kp_decl_version(d), t, idx == 0,
			       name, sizeof(name));
		if (strcmp(name, file))
			continue;
		const char *sep = strstr(t, "::");
		const char *url = sep ? sep + 2 : t;
		if (strncmp(url, "http://", 7) && strncmp(url, "https://", 8) &&
		    strncmp(url, "ftp://", 6))
			return NULL;
		kb_strlcpy(buf, url, cap);
		return buf;
	}
	return NULL;
}

/* 1 when one of the recipe's `source =` entries is cached under `file`. */
static int source_names(const KpDecl *d, const char *file)
{
	char list[4096];
	kb_strlcpy(list, kp_decl_source(d), sizeof(list));
	char *save = NULL;
	int idx = 0;
	for (char *t = strtok_r(list, " \t\n", &save); t;
	     t = strtok_r(NULL, " \t\n", &save), idx++) {
		char name[512];
		kp_source_file(kp_decl_name(d), kp_decl_version(d), t, idx == 0,
			       name, sizeof(name));
		if (!strcmp(name, file))
			return 1;
	}
	return 0;
}

static int fetch_one(const KpConf *c, const KpDecl *d, const char *portdir,
		     const char *hash, const char *file)
{
	char *dst = kb_path_join(c->source_dir, file);
	char tmp[1100];
	snprintf(tmp, sizeof(tmp), "%s/.%s.part", c->source_dir, file);

	int tried = 0, reached = 0;
	char base[1024];
	archive_base(base, sizeof(base));
	char *idx = base[0] ? find_index(portdir) : NULL;
	char tag[128], asset[512], parts[4096];

	if (idx && index_lookup(idx, hash, tag, sizeof(tag), asset,
				sizeof(asset), parts, sizeof(parts)) == 0) {
		char url[2048];
		snprintf(url, sizeof(url), "%s/%s/%s", base, tag, asset);
		kp_msg("Fetching %s from the archive (%s)", file, tag);
		tried = 1;
		int got = parts[0] ? archive_parts(url, parts, tmp)
				   : download(url, tmp);
		if (got == GOT_OK && hash_ok(tmp, hash) && rename(tmp, dst) == 0)
			goto done;
		reached |= got != GOT_OFFLINE;
	}
	free(idx);
	idx = NULL;

	char ubuf[2048];
	const char *url = upstream_url(d, file, ubuf, sizeof(ubuf));
	if (url) {
		kp_msg("Fetching %s from upstream", file);
		tried = 1;
		int got = download(url, tmp);
		if (got == GOT_OK && hash_ok(tmp, hash) && rename(tmp, dst) == 0)
			goto done;
		reached |= got != GOT_OFFLINE;
	}

	unlink(tmp);
	free(dst);
	if (!tried) {
		kp_err("%s: neither the archive nor a `source =` URL names it",
		       file);
		return GOT_FAIL;
	}
	/* Offline only when NO copy reached a server: an archive that answered
	 * 404 and an upstream host that did not resolve is a source that has
	 * gone, not a machine with no network. */
	if (!reached) {
		kp_err("%s: the network could not be reached", file);
		return GOT_OFFLINE;
	}
	kp_err("%s: no copy hashed to the recipe's sha256 %s", file, hash);
	return GOT_FAIL;

done:
	free(idx);
	free(dst);
	return GOT_OK;
}

/* ── one port ──────────────────────────────────────────────────────────── */

int kp_fetch_port(const KpConf *c, const char *portdir)
{
	char *recipe = kb_path_join(portdir, "kpkgbuild");
	KpDecl *d = kp_decl_parse(recipe);
	free(recipe);
	if (!d) {
		kp_err("%s: not a recipe", portdir);
		return 1;
	}

	char list[4096];
	kb_strlcpy(list, kp_decl_sha256(d), sizeof(list));
	int worst = GOT_OK, made_dir = 0;
	char *hex = NULL, *save = NULL;
	int k = 0;
	for (char *t = strtok_r(list, " \t\n", &save); t;
	     t = strtok_r(NULL, " \t\n", &save), k++) {
		if (k % 2 == 0) {
			hex = t;
			continue;
		}
		if (!hex || strchr(t, '/'))
			continue;
		char *here = kb_path_join(portdir, t);
		char *cached = kb_path_join(c->source_dir, t);
		int in_port = kb_path_exists(here);
		int got = GOT_OK;
		if (!in_port && !kb_path_exists(cached)) {
			if (!made_dir) {
				kb_mkdir_p(c->source_dir);
				made_dir = 1;
			}
			got = fetch_one(c, d, portdir, hex, t);
		}
		/* A file no `source =` entry names — a vendor bundle — is
		 * opened by build.sh itself as $PORT_SRC/<file>, and only
		 * extract_sources() looks in $SOURCE_DIR. It is linked into
		 * the port directory, or the build cannot find it. */
		if (got == GOT_OK && !in_port && !source_names(d, t)) {
			unlink(here);
			if (symlink(cached, here) != 0) {
				kp_err("%s: cannot link it into %s: %s", t,
				       portdir, strerror(errno));
				got = GOT_FAIL;
			}
		}
		free(here);
		free(cached);
		if (got == GOT_OFFLINE)
			worst = GOT_OFFLINE;
		else if (got == GOT_FAIL && worst == GOT_OK)
			worst = GOT_FAIL;
		/* Offline is offline for every later file too: asking again
		 * costs a timeout per file and answers nothing new. */
		if (worst == GOT_OFFLINE)
			break;
	}
	kp_decl_free(d);
	return worst == GOT_OK ? 0 : worst == GOT_OFFLINE ? 3 : 1;
}

int kp_fetch_wanted(void)
{
	const char *v = getenv("KPKG_FETCH");
	return v && *v == '1';
}

int kp_cmd_fetch(const KpConf *c, int argc, char **argv)
{
	if (argc < 1) {
		printf("Usage: kpkg fetch <package>...\n");
		return 1;
	}
	int rc = 0;
	for (int i = 0; i < argc; i++) {
		char err[1600], *dir = NULL;
		int f = kp_port_find(c, argv[i], &dir, err, sizeof(err));
		if (f <= 0) {
			if (f < 0)
				kp_err("%s", err);
			else
				kp_err("%s: no such port", argv[i]);
			if (!rc)
				rc = 1;
			continue;
		}
		int r = kp_fetch_port(c, dir);
		free(dir);
		if (r == 3)
			return 3;
		if (r && !rc)
			rc = r;
	}
	return rc;
}
