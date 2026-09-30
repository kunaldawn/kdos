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
 * The store: an application is built here, on this machine, out of the
 * catalogue.
 *
 * A CHAIN IS BUILT BOTTOM-UP AS A STACK OF IMAGES, one per catalogue row,
 * each `FROM` the one below. That is what makes a second GTK application one
 * apt pass instead of three: the base and the runtime are already images and
 * podman skips them. Flattening a chain into one Containerfile per application
 * would rebuild and restore every shared layer per application.
 *
 * AN IMAGE ALREADY PRESENT IS NEVER REBUILT. `podman image exists` is the
 * whole check — the catalogue has no version per row, so the image is current
 * by definition until somebody removes it.
 *
 * THE CONTAINERFILE IS GENERATED, NEVER TEMPLATED FROM A FILE. It is the one
 * thing here that is pure, which is why it is the seam --selftest drives:
 * podman is not available where the tests run and never will be.
 *
 * PROGRESS IS ONE FLUSHED LINE PER STEP, so a surface reading this process's
 * stdout shows it without parsing podman. The child is a DIRECT child of the
 * surface and never a supervised one: a pipeline reading a supervised
 * service's output never returns, because the supervisor holds the pipe open.
 *
 * NOTHING ROLLS BACK. Six applications where the fourth fails leaves five
 * installed and names the fourth: an installed application is not damaged by a
 * later one failing, and unwinding throws away twenty minutes of apt.
 */

#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "kdos-appbox.h"

/* ── the snapshot ──────────────────────────────────────────────────────── */

/*
 * `snapshot = auto` TAKES THE DATE FROM THE BASE IMAGE ITSELF. Debian's
 * official images record the snapshot they were built from as a comment in
 * their own sources file, so the packages installed on top cannot disagree
 * with the rootfs under them.
 *
 * A RESOLUTION THAT FAILS IS `off` AND SAYS SO. An unpinned build still
 * produces a working application, so refusing to install because a comment
 * moved would be worse — but it stops being reproducible, and an unpinned
 * Containerfile is indistinguishable from a pinned one once it is written.
 * The warning is the only thing that tells the two apart.
 */
const char *store_snapshot(void)
{
	static char snap[64];
	const char *conf = cat_snapshot();
	KbArgv a = {0};
	char buf[4096] = "";

	if (snap[0])
		return snap;
	if (strcmp(conf, "auto")) {
		kb_strlcpy(snap, conf, sizeof(snap));
		return snap;
	}
	kb_argv_add(&a, "podman");
	kb_argv_add(&a, "run");
	kb_argv_add(&a, "--rm");
	kb_argv_add(&a, STORE_BASE_IMAGE);
	kb_argv_add(&a, "sed");
	kb_argv_add(&a, "-n");
	kb_argv_add(&a, "s|^# *https\\?://snapshot.debian.org/archive/debian/"
			"\\([0-9TZ]*\\)$|\\1|p");
	kb_argv_add(&a, "/etc/apt/sources.list.d/debian.sources");
	kb_argv_end(&a);
	if (kb_run_capture(&a, buf, sizeof(buf)) == 0) {
		char *nl = strchr(buf, '\n');
		if (nl)
			*nl = 0;
		if (buf[0])
			kb_strlcpy(snap, buf, sizeof(snap));
	}
	if (!snap[0]) {
		kb_warn("could not read the snapshot date out of %s — building "
			"against the live archive, which is not reproducible",
			STORE_BASE_IMAGE);
		kb_strlcpy(snap, "off", sizeof(snap));
	}
	return snap;
}

/* ── the Containerfile ─────────────────────────────────────────────────── */

/* Append to `out` and say whether it still fits. Truncation here is a failure,
 * never a cosmetic one: half a Containerfile builds an image missing whatever
 * the tail would have installed. */
static int addf(char *out, size_t n, const char *fmt, ...)
	__attribute__((format(printf, 3, 4)));

static int addf(char *out, size_t n, const char *fmt, ...)
{
	size_t used = strlen(out);
	va_list ap;
	int w;

	va_start(ap, fmt);
	w = vsnprintf(out + used, n - used, fmt, ap);
	va_end(ap);
	return w >= 0 && (size_t)w < n - used;
}

int store_containerfile(const CatPack *p, const CatPack *parent,
			const char *snapshot, char *out, size_t n)
{
	char url[256], pat[64];
	int have_deb;

	if (!out || n == 0)
		return -1;
	out[0] = 0;
	if (!p)
		return -1;

	/*
	 * A BASE MAY NAME ITS OWN IMAGE. `debian:trixie-slim` is the one every
	 * application row is a diff over, but it is not the only userland worth
	 * carrying: an `image` row points a base at something else, which is
	 * how a 3 MB alpine rootfs costs one line.
	 */
	if (parent) {
		if (!addf(out, n, "FROM %s%s\n", STORE_IMG_PREFIX, parent->id))
			return -1;
	} else if (p->image[0]) {
		if (!addf(out, n, "FROM %s\n", p->image))
			return -1;
	} else if (!addf(out, n, "FROM %s\n", STORE_BASE_IMAGE)) {
		return -1;
	}

	/*
	 * A ROW WITH NO PACKAGES GETS NO `RUN`, AND THAT IS WHAT MAKES A
	 * NON-DEBIAN BASE POSSIBLE. Everything below the FROM line is apt, so
	 * emitting it against an alpine rootfs fails on `apt-get: not found`
	 * rather than on anything a reader could act on.
	 */
	if (!p->packages || !p->packages[0])
		return 0;

	if (!addf(out, n, "ENV DEBIAN_FRONTEND=noninteractive\nRUN "))
		return -1;

	/*
	 * THE SOURCES FILE IS WRITTEN, NEVER SED'D: a rewrite that depends on
	 * the base image's exact formatting breaks the day Debian reflows it,
	 * and silently, by leaving the live URI in place.
	 *
	 * HTTP AND NOT HTTPS, and it is not laziness. The base row is the one
	 * that INSTALLS ca-certificates, so pinning it over TLS is a
	 * chicken-and-egg: apt cannot verify the certificate and the row dies
	 * on "certificate verify failed". An apt repository's integrity is the
	 * GPG signature named by Signed-By, not the transport.
	 *
	 * Check-Valid-Until is off because a snapshot's Release file is stale
	 * by definition.
	 */
	if (snapshot && strcmp(snapshot, "off")) {
		if (!addf(out, n,
			  "printf '%%s\\n' \\\n"
			  "'Types: deb' \\\n"
			  "'URIs: http://snapshot.debian.org/archive/debian/%s/' \\\n"
			  "'Suites: trixie trixie-updates' \\\n"
			  "'Components: main' \\\n"
			  "'Signed-By: /usr/share/keyrings/debian-archive-keyring.pgp' \\\n"
			  "'' \\\n"
			  "'Types: deb' \\\n"
			  "'URIs: http://snapshot.debian.org/archive/debian-security/%s/' \\\n"
			  "'Suites: trixie-security' \\\n"
			  "'Components: main' \\\n"
			  "'Signed-By: /usr/share/keyrings/debian-archive-keyring.pgp' \\\n"
			  " > /etc/apt/sources.list.d/debian.sources && \\\n"
			  "    echo 'Acquire::Check-Valid-Until \"false\";' "
			  "> /etc/apt/apt.conf.d/99snapshot && ",
			  snapshot, snapshot))
			return -1;
	}

	/* A `:i386` package needs the architecture enabling first — the image
	 * is amd64 and apt answers "wine32 has no installation candidate",
	 * naming the package rather than the architecture it is not looking
	 * in. */
	if (strstr(p->packages, ":i386") &&
	    !addf(out, n, "dpkg --add-architecture i386 && "))
		return -1;

	/* NOT EVERY APPLICATION IS IN DEBIAN. A `deb` row fetches the newest
	 * asset matching its pattern and lets apt resolve its dependencies,
	 * which is why the fetch is part of the same RUN. */
	have_deb = cat_deb(p->id, url, sizeof(url), pat, sizeof(pat)) == 0;
	if (have_deb &&
	    !addf(out, n,
		  "curl -fsSL '%s' \\\n"
		  "      | grep browser_download_url | grep '%s' | head -1 \\\n"
		  "      | cut -d'\"' -f4 | xargs -r curl -fsSLo /tmp/pack.deb && ",
		  url, pat))
		return -1;

	/* ONE RUN, so the image has exactly two layers over its parent — the
	 * ENV is free — and the top one is this row's own diff. */
	if (!addf(out, n,
		  "apt-get -o APT::Update::Error-Mode=any update \\\n"
		  "    && apt-get install -y --no-install-recommends %s%s \\\n"
		  "    && rm -f /tmp/pack.deb && rm -rf /var/lib/apt/lists/*\n",
		  p->packages, have_deb ? " /tmp/pack.deb" : ""))
		return -1;
	return 0;
}

/* ── building ──────────────────────────────────────────────────────────── */

/*
 * box.c's image_exists() takes a full ref; a row is asked about by id.
 *
 * kb_strlcpy AND NOT snprintf: an id reaches here out of a row of a 2-D array,
 * which a compiler must assume runs to the end of it. The bounded copy says
 * what is true — an id is at most CAT_ID_MAX — instead of sizing the buffer
 * for a string that cannot occur.
 */
static int row_built(const char *id)
{
	char tag[sizeof(STORE_IMG_PREFIX) + CAT_ID_MAX];

	kb_strlcpy(tag, STORE_IMG_PREFIX, sizeof(tag));
	kb_strlcpy(tag + sizeof(STORE_IMG_PREFIX) - 1, id,
		   sizeof(tag) - (sizeof(STORE_IMG_PREFIX) - 1));
	return image_exists(tag);
}

/*
 * WHAT THIS RUN HAS ALREADY BUILT. A real install asks podman, and the second
 * GTK application finds the runtime the first one left behind. A DRY RUN has
 * nothing to ask, so without this it prints every chain in full and shows four
 * builds where a real install does two — a preview that disagrees with the
 * thing it is previewing is worse than none.
 */
static char g_done[CAT_MAX_PACKS][CAT_ID_MAX];
static int  g_ndone;

static int done_this_run(const char *id)
{
	for (int i = 0; i < g_ndone; i++)
		if (!strcmp(g_done[i], id))
			return 1;
	if (g_ndone < CAT_MAX_PACKS)
		kb_strlcpy(g_done[g_ndone++], id, CAT_ID_MAX);
	return 0;
}

static int build_one(const CatPack *p, const CatPack *parent, int dry)
{
	char cf[16384];
	char tag[MAX_LINE];
	KbArgv a = {0};

	if (store_containerfile(p, parent, store_snapshot(), cf, sizeof(cf))
	    != 0) {
		fprintf(stderr, "  %s: the Containerfile does not fit\n", p->id);
		return -1;
	}
	snprintf(tag, sizeof(tag), "%s%s", STORE_IMG_PREFIX, p->id);

	if (dry) {
		printf("--- %s\n%s", tag, cf);
		return 0;
	}
	printf("==> building %s\n", tag);
	fflush(stdout);

	/*
	 * THE BUILD CONTEXT IS AN EMPTY DIRECTORY. Nothing is COPY'd in, and
	 * handing podman a real directory makes it read and hash whatever is
	 * there for nothing.
	 */
	kb_argv_add(&a, "podman");
	kb_argv_add(&a, "build");
	kb_argv_add(&a, "-t");
	kb_argv_add(&a, tag);
	kb_argv_add(&a, "-f");
	kb_argv_add(&a, "-");
	kb_argv_add(&a, STORE_EMPTY_CTX);
	kb_argv_end(&a);
	return kb_run_feed(&a, cf, strlen(cf));
}

/* ── launchers ─────────────────────────────────────────────────────────── */

/*
 * THE PACK WALK CANNOT SEE A STORE BOX. `genlaunchers` with no source asks
 * kdos-packd which packs are installed, and a store-built box is an IMAGE —
 * the daemon has never heard of it, so an application installed here would get
 * no launcher, no shim and no mime entry, and nothing on the desktop could
 * reach it.
 *
 * `--packs-dir` is the shape that fits: one directory per id, each holding
 * `usr/share/applications`, which is exactly what `podman cp` gives from a
 * built image. The directory NAME is the id the launcher table's third field
 * carries, so a launcher clicked here dispatches to the box that was created.
 *
 * EVERY INSTALLED APPLICATION IS RE-READ, not just the new one. The table,
 * the mime cache and the shims are written whole each time, so generating from
 * one application would delete the launchers of every other.
 */
static int extract_entries(const char *id, const char *root)
{
	/* root is a runtime path and id a catalogue id; the destination holds
	 * both plus a suffix, so it is sized for the sum rather than for one. */
	char dir[MAX_LINE + CAT_ID_MAX + 32], src[MAX_LINE], cid[256] = "";
	KbArgv a = {0};
	int rc;

	snprintf(dir, sizeof(dir), "%s/%s/usr/share", root, id);
	if (kb_mkdir_p(dir) != 0)
		return -1;

	kb_argv_add(&a, "podman");
	kb_argv_add(&a, "create");
	kb_argv_addf(&a, "%s%s", STORE_IMG_PREFIX, id);
	kb_argv_add(&a, "/bin/true");
	kb_argv_end(&a);
	if (kb_run_capture(&a, cid, sizeof(cid)) != 0 || !cid[0])
		return -1;
	cid[strcspn(cid, "\r\n")] = 0;

	snprintf(src, sizeof(src), "%s:/usr/share/applications", cid);
	{
		KbArgv c = {0};
		kb_argv_add(&c, "podman");
		kb_argv_add(&c, "cp");
		kb_argv_add(&c, src);
		kb_argv_add(&c, dir);
		kb_argv_end(&c);
		rc = kb_run(&c);
	}
	{
		KbArgv r = {0};
		kb_argv_add(&r, "podman");
		kb_argv_add(&r, "rm");
		kb_argv_add(&r, cid);
		kb_argv_end(&r);
		kb_run(&r);
	}
	return rc;
}

/*
 * Regenerate the user's launchers from every store box that exists. Called
 * after an install and after an uninstall, because both change the set.
 */
int store_launchers(void)
{
	char boxes[1 << 16] = "";
	char root[MAX_LINE];
	KbArgv a = {0};
	char *line, *save;
	char err[256];
	int n = 0;

	if (!cat_count() && cat_load(NULL, err, sizeof(err)) != 0)
		return -1;

	kb_argv_add(&a, "podman");
	kb_argv_add(&a, "ps");
	kb_argv_add(&a, "--all");
	kb_argv_add(&a, "--format");
	kb_argv_add(&a, "{{.Names}}");
	kb_argv_end(&a);
	if (kb_run_capture(&a, boxes, sizeof(boxes)) != 0)
		return -1;

	snprintf(root, sizeof(root), "%s/kdos-launchers-%u",
		 kb_runtime_dir(), (unsigned)getpid());
	kb_mkdir_p(root);

	for (line = strtok_r(boxes, "\n", &save); line;
	     line = strtok_r(NULL, "\n", &save)) {
		const CatPack *p = cat_find(line);

		/* An app row only. A runtime's applications directory is
		 * whatever Debian's libraries dropped there and is not this
		 * desktop's business. */
		if (!p || strcmp(p->kind, "app"))
			continue;
		if (extract_entries(line, root) == 0)
			n++;
	}
	if (n)
		cmd_genlaunchers(root, NULL, 1, 1);
	else
		cmd_genlaunchers(NULL, NULL, 1, 0);

	{
		KbArgv rm = {0};
		kb_argv_add(&rm, "rm");
		kb_argv_add(&rm, "-rf");
		kb_argv_add(&rm, root);
		kb_argv_end(&rm);
		kb_run(&rm);
	}
	return n;
}

/* ── install ───────────────────────────────────────────────────────────── */

int store_install(const char *id, int dry)
{
	const CatPack *chain[CAT_CHAIN_MAX];
	char nd[8][CAT_ID_MAX];
	KbArgv a = {0};
	int n, nneed;

	n = cat_chain(id, chain);
	if (n < 0) {
		fprintf(stderr, "  %s: no chain — a parent is missing from the "
				"catalogue\n", id);
		return -1;
	}

	/* A data row an application is useless without arrives with it, or the
	 * Start menu offers a launcher onto an empty database. */
	nneed = cat_needs(id, nd, 8);
	for (int i = 0; i < nneed; i++)
		if (!row_built(nd[i]) && store_install(nd[i], dry) != 0)
			fprintf(stderr, "  %s: its data row %s did not build\n",
				id, nd[i]);

	for (int i = 0; i < n; i++) {
		if (done_this_run(chain[i]->id))
			continue;
		if (!dry && row_built(chain[i]->id)) {
			printf("    %s%s is already here\n", STORE_IMG_PREFIX,
			       chain[i]->id);
			fflush(stdout);
			continue;
		}
		if (build_one(chain[i], i ? chain[i - 1] : NULL, dry) != 0) {
			fprintf(stderr, "  %s: %s did not build\n", id,
				chain[i]->id);
			return -1;
		}
	}
	if (dry)
		return 0;

	/*
	 * The box, over the top image.
	 *
	 * THE ENVIRONMENT IS NOT A PROFILE KEY AND IS NOT BAKED INTO THE
	 * IMAGE. The launch path reads the catalogue for it, the same way the
	 * pack lane reads pack metadata — see the env block in main.c. An
	 * image `ENV` could not carry the one value that names `$HOME`, and a
	 * profile key would be a second place a variable can be set.
	 */
	kb_argv_add(&a, "kdos-box");
	kb_argv_add(&a, "create");
	kb_argv_add(&a, id);
	kb_argv_addf(&a, "base=image:%s%s", STORE_IMG_PREFIX, id);
	kb_argv_end(&a);
	if (kb_run(&a) != 0) {
		fprintf(stderr, "  %s: the image built and the box did not\n", id);
		return -1;
	}

	store_launchers();
	printf("==> %s installed\n", id);
	fflush(stdout);
	return 0;
}

int store_install_many(const char *const *ids, int n, int dry)
{
	char want[CAT_MAX_PACKS][CAT_ID_MAX];
	char err[256];
	int nw, failed = 0;

	/* Line-buffered, so a surface reading this pipe sees each step as it
	 * happens rather than in one burst at exit. */
	setvbuf(stdout, NULL, _IOLBF, 0);
	g_ndone = 0;

	nw = cat_expand(ids, n, want, CAT_MAX_PACKS, err, sizeof(err));
	if (nw < 0) {
		fprintf(stderr, "kdos-appbox: %s\n", err);
		return 1;
	}
	for (int i = 0; i < nw; i++)
		if (store_install(want[i], dry) != 0)
			failed++;
	if (failed)
		fprintf(stderr, "%d of %d did not install\n", failed, nw);
	return failed;
}

/* ── uninstall ─────────────────────────────────────────────────────────── */

/*
 * A RUNTIME IMAGE IS KEPT WHILE ANY REMAINING BOX'S CHAIN NAMES IT, and that
 * question is asked of the CATALOGUE rather than of podman: a dangling-image
 * sweep cannot tell a runtime nothing uses yet from one whose only application
 * is mid-install.
 */
static int still_wanted(const char *img_id, const char *dropped)
{
	char boxes[1 << 16] = "";
	KbArgv a = {0};
	char *line, *save;

	kb_argv_add(&a, "podman");
	kb_argv_add(&a, "ps");
	kb_argv_add(&a, "--all");
	kb_argv_add(&a, "--format");
	kb_argv_add(&a, "{{.Names}}");
	kb_argv_end(&a);
	if (kb_run_capture(&a, boxes, sizeof(boxes)) != 0)
		return 1;	/* cannot tell: keep it */

	for (line = strtok_r(boxes, "\n", &save); line;
	     line = strtok_r(NULL, "\n", &save)) {
		const CatPack *chain[CAT_CHAIN_MAX];
		int n;

		if (!strcmp(line, dropped))
			continue;
		n = cat_chain(line, chain);
		for (int i = 0; i < n; i++)
			if (!strcmp(chain[i]->id, img_id))
				return 1;
	}
	return 0;
}

int store_uninstall(const char *id)
{
	const CatPack *chain[CAT_CHAIN_MAX];
	KbArgv a = {0};
	int n;

	kb_argv_add(&a, "kdos-box");
	kb_argv_add(&a, "remove");
	kb_argv_add(&a, id);
	kb_argv_end(&a);
	kb_run(&a);

	n = cat_chain(id, chain);
	/* The application's own image first, then any runtime above the base
	 * that nothing else needs. The base itself is never removed: it is
	 * every chain's floor and 80 MB. */
	for (int i = n - 1; i >= 1; i--) {
		KbArgv r = {0};

		if (i < n - 1 && still_wanted(chain[i]->id, id))
			break;
		kb_argv_add(&r, "podman");
		kb_argv_add(&r, "rmi");
		kb_argv_addf(&r, "%s%s", STORE_IMG_PREFIX, chain[i]->id);
		kb_argv_end(&r);
		if (kb_run(&r) != 0)
			break;
		printf("    removed %s%s\n", STORE_IMG_PREFIX, chain[i]->id);
	}

	/* The table, the mime cache and the shims are written whole, so this
	 * regenerates from what REMAINS rather than removing one entry. */
	store_launchers();
	printf("==> %s removed\n", id);
	return 0;
}

int store_uninstall_many(const char *const *ids, int n)
{
	char want[CAT_MAX_PACKS][CAT_ID_MAX];
	char err[256];
	int nw, failed = 0;

	setvbuf(stdout, NULL, _IOLBF, 0);
	nw = cat_expand(ids, n, want, CAT_MAX_PACKS, err, sizeof(err));
	if (nw < 0) {
		fprintf(stderr, "kdos-appbox: %s\n", err);
		return 1;
	}
	for (int i = 0; i < nw; i++)
		if (store_uninstall(want[i]) != 0)
			failed++;
	return failed;
}

/* ── export ────────────────────────────────────────────────────────────── */

/*
 * A SET, AS SIGNED PACKS IN ONE FILE.
 *
 * WHY PACKS AND NOT `podman save`. A store install is unsigned content from
 * somebody else's registry; a pack is hashed and signature-checked by
 * kdos-packd WHERE IT MOUNTS IT. Exporting as packs is what makes an imported
 * application verified where a store-installed one is not, and it is the only
 * route to software on a machine with no network.
 *
 * `podman export` AND NOT THE OVERLAY STORE. An image's own content is spread
 * across its layers and only the export flattens them — the same reason the
 * base row is exported rather than read as a diff. It also means no overlay
 * whiteout and no `trusted.overlay.*` xattr is in play, which is the one thing
 * that would force this to run as root.
 *
 * `kdos-pack build` AND NOT mkfs.erofs FOLLOWED BY `assemble`. The build verb
 * already runs mkfs with the reproducible flag set, and a second copy of that
 * list is a second answer to how a pack is made. Its --force-uid=1000 is what
 * lets this run unprivileged: a box runs `--userns keep-id`, so the process
 * inside it is uid 1000 and a tree owned by real root is one it can create
 * nothing in.
 */

/*
 * The SELECTION manifest. Flat and commentable because the set is meant to be
 * diffed and hand-edited; a format only this program can write is one nobody
 * can correct when an id is wrong.
 */
int store_selection(const char *const *ids, int n, const char *catver,
		    char *out, size_t outn)
{
	char date[32];
	time_t now = time(NULL);
	struct tm tm;

	if (!out || outn == 0)
		return -1;
	out[0] = 0;
	gmtime_r(&now, &tm);
	strftime(date, sizeof(date), "%Y-%m-%d", &tm);

	if (!addf(out, outn,
		  "# kdos app set \xc2\xb7 exported %s \xc2\xb7 catalogue %s\n"
		  "#\n"
		  "# One id per line. A `group` line records what was PICKED and the\n"
		  "# ids under it are what that group held on the day, so a group\n"
		  "# whose membership changed later still imports the software this\n"
		  "# archive actually carries.\n", date, catver ? catver : "unknown"))
		return -1;
	for (int i = 0; i < n; i++) {
		if (cat_group_find(ids[i])) {
			if (!addf(out, outn, "group %s\n", ids[i]))
				return -1;
			continue;
		}
	}
	for (int i = 0; i < n; i++)
		if (!cat_group_find(ids[i]) && !addf(out, outn, "%s\n", ids[i]))
			return -1;
	return 0;
}

/* Skips blanks, `#` comments and `group` lines — a group line records what was
 * picked; the id lines are what is installed. */
int store_selection_parse(const char *text, char out[][CAT_ID_MAX], int max)
{
	char *copy, *line, *save;
	int n = 0;

	if (!text)
		return -1;
	copy = kb_strdup(text);
	for (line = strtok_r(copy, "\n", &save); line;
	     line = strtok_r(NULL, "\n", &save)) {
		char *p = line;

		while (*p == ' ' || *p == '\t')
			p++;
		if (!*p || *p == '#' || !strncmp(p, "group ", 6))
			continue;
		p[strcspn(p, " \t\r")] = 0;
		if (n < max)
			kb_strlcpy(out[n++], p, CAT_ID_MAX);
	}
	free(copy);
	return n;
}

/* Flatten one built image into a directory. */
static int flatten(const char *id, const char *dir)
{
	char cid[256] = "";
	KbArgv a = {0};
	int rc;

	if (kb_mkdir_p(dir) != 0)
		return -1;
	kb_argv_add(&a, "podman");
	kb_argv_add(&a, "create");
	kb_argv_addf(&a, "%s%s", STORE_IMG_PREFIX, id);
	kb_argv_add(&a, "/bin/true");
	kb_argv_end(&a);
	if (kb_run_capture(&a, cid, sizeof(cid)) != 0 || !cid[0])
		return -1;
	cid[strcspn(cid, "\r\n")] = 0;
	{
		KbArgv e = {0};
		/* `dir` is already a runtime path plus an id; the tarball path
		 * is that plus a suffix. */
		char sh[MAX_LINE + CAT_ID_MAX + 64];

		/* `podman export | tar -x` is the flatten. kb_run takes no
		 * pipeline, so the export goes to a file and tar reads it —
		 * one more temporary and no shell. */
		snprintf(sh, sizeof(sh), "%s/.export.tar", dir);
		kb_argv_add(&e, "podman");
		kb_argv_add(&e, "export");
		kb_argv_add(&e, "-o");
		kb_argv_add(&e, sh);
		kb_argv_add(&e, cid);
		kb_argv_end(&e);
		rc = kb_run(&e);
		if (rc == 0) {
			KbArgv t = {0};
			kb_argv_add(&t, "tar");
			kb_argv_add(&t, "-xf");
			kb_argv_add(&t, sh);
			kb_argv_add(&t, "-C");
			kb_argv_add(&t, dir);
			kb_argv_end(&t);
			rc = kb_run(&t);
		}
		unlink(sh);
	}
	{
		KbArgv r = {0};
		kb_argv_add(&r, "podman");
		kb_argv_add(&r, "rm");
		kb_argv_add(&r, cid);
		kb_argv_end(&r);
		kb_run(&r);
	}
	return rc;
}

static int write_meta(const CatPack *p, const char *path)
{
	KbBuf m = {0};
	int rc;

	kb_buf_printf(&m, "id          = %s\n", p->id);
	kb_buf_printf(&m, "kind        = %s\n", p->kind);
	kb_buf_printf(&m, "version     = %ld\n", (long)time(NULL));
	kb_buf_printf(&m, "release     = 1\n");
	/* `name` must not be the id — kpk_meta_valid refuses that. */
	if (strcmp(p->name, p->id))
		kb_buf_printf(&m, "name        = %s\n", p->name);
	if (p->tagline[0])
		kb_buf_printf(&m, "summary     = %s\n", p->tagline);
	rc = kb_write_all(path, m.p, m.n);
	kb_buf_free(&m);
	return rc;
}

int store_export(const char *out, const char *const *ids, int n)
{
	char want[CAT_MAX_PACKS][CAT_ID_MAX];
	char root[MAX_LINE], err[256], sel[8192];
	KbArgv a = {0};
	int nw, made = 0;

	setvbuf(stdout, NULL, _IOLBF, 0);
	nw = cat_expand(ids, n, want, CAT_MAX_PACKS, err, sizeof(err));
	if (nw < 0) {
		fprintf(stderr, "kdos-appbox: %s\n", err);
		return 1;
	}
	snprintf(root, sizeof(root), "%s/kdos-export-%u", kb_runtime_dir(),
		 (unsigned)getpid());
	kb_mkdir_p(root);

	for (int i = 0; i < nw; i++) {
		const CatPack *p = cat_find(want[i]);
		/* BOUND THE ID TO WHAT AN ID IS. `want` is a 2-D array, so a
		 * row of it is a string a compiler must assume runs to the end
		 * of the whole thing; copying it out once says the truth and
		 * sizes every path below for a real id. */
		char id[CAT_ID_MAX];
		char dir[MAX_LINE + CAT_ID_MAX + 32];
		char meta[MAX_LINE + CAT_ID_MAX + 32];
		char kpack[MAX_LINE + CAT_ID_MAX + 32];
		KbArgv b = {0};

		if (!p)
			continue;
		kb_strlcpy(id, want[i], sizeof(id));
		if (!row_built(id)) {
			fprintf(stderr, "  %s: not installed here — skipped\n",
				id);
			continue;
		}
		printf("==> flattening %s\n", id);
		snprintf(dir, sizeof(dir), "%s/%s.root", root, id);
		if (flatten(id, dir) != 0) {
			fprintf(stderr, "  %s: could not export its image\n",
				id);
			continue;
		}
		snprintf(meta, sizeof(meta), "%s/%s.meta", root, id);
		snprintf(kpack, sizeof(kpack), "%s/%s.kpack", root, id);
		if (write_meta(p, meta) != 0)
			continue;

		printf("==> packing %s\n", id);
		kb_argv_add(&b, "kdos-pack");
		kb_argv_add(&b, "build");
		kb_argv_add(&b, dir);
		kb_argv_add(&b, meta);
		kb_argv_add(&b, kpack);
		kb_argv_end(&b);
		if (kb_run(&b) != 0) {
			fprintf(stderr, "  %s: kdos-pack could not build it\n",
				id);
			continue;
		}
		unlink(meta);
		{
			KbArgv rm = {0};
			kb_argv_add(&rm, "rm");
			kb_argv_add(&rm, "-rf");
			kb_argv_add(&rm, dir);
			kb_argv_end(&rm);
			kb_run(&rm);
		}
		made++;
	}
	if (!made) {
		fprintf(stderr, "kdos-appbox: nothing to export\n");
		return 1;
	}

	/* The manifest, then the index, then the tar. */
	store_selection(ids, n, cat_version(), sel, sizeof(sel));
	{
		char selp[MAX_LINE + 32];
		snprintf(selp, sizeof(selp), "%s/SELECTION", root);
		kb_write_all(selp, sel, strlen(sel));
	}

	/*
	 * `kdos-pack index --sign` when a key is readable, and WITHOUT one
	 * otherwise — plainly said. An unsigned archive still imports and
	 * kdos-packd still checks every hash against the index, but nobody may
	 * be left thinking it was signed when it was not.
	 */
	kb_argv_add(&a, "kdos-pack");
	kb_argv_add(&a, "index");
	kb_argv_add(&a, root);
	{
		const char *key = getenv("KDOS_PACK_KEY");
		if (key && *key && kb_path_exists(key)) {
			kb_argv_add(&a, "--sign");
			kb_argv_add(&a, key);
		} else {
			printf("==> no signing key (KDOS_PACK_KEY) — the index "
			       "is UNSIGNED; every hash is still checked at the "
			       "mount\n");
		}
	}
	kb_argv_end(&a);
	if (kb_run(&a) != 0) {
		fprintf(stderr, "kdos-appbox: could not index the set\n");
		return 1;
	}

	{
		KbArgv t = {0};
		kb_argv_add(&t, "tar");
		kb_argv_add(&t, "-cf");
		kb_argv_add(&t, out);
		kb_argv_add(&t, "-C");
		kb_argv_add(&t, root);
		kb_argv_add(&t, ".");
		kb_argv_end(&t);
		if (kb_run(&t) != 0) {
			fprintf(stderr, "kdos-appbox: could not write %s\n", out);
			return 1;
		}
	}
	{
		KbArgv rm = {0};
		kb_argv_add(&rm, "rm");
		kb_argv_add(&rm, "-rf");
		kb_argv_add(&rm, root);
		kb_argv_end(&rm);
		kb_run(&rm);
	}
	printf("==> %s: %d application(s)\n", out, made);
	return 0;
}

/* ── import ────────────────────────────────────────────────────────────── */

/*
 * AN IMPORTED APPLICATION IS VERIFIED WHERE A STORE-INSTALLED ONE IS NOT.
 * Every pack goes through kdos-packd, which hashes it against its own footer
 * and checks the signature AT THE MOUNT — so a tampered pack produces a mount
 * that fails rather than an application that runs.
 *
 * THE DAEMON IS HANDED A FILENAME IN ITS OWN STAGING DIRECTORY AND NEVER A
 * PATH. That is the rule the whole daemon is built on and it is what keeps
 * something reachable from `wheel` from being `mount /dev/sda2 /etc`.
 *
 * A PACK THE DAEMON REFUSES IS NAMED AND SKIPPED. The rest of the archive
 * still imports: one bad member is not a reason to refuse software that
 * verified perfectly.
 */
static int stage_and_install(const char *file, char *idout, size_t idn)
{
	char *staging, *dst;
	char req[512], msg[512] = "";
	const char *base;
	int rc;

	staging = kb_path_join(pack_store(), "staging");
	base = kb_basename(file);
	dst = kb_path_join(staging, base);
	free(staging);

	if (kb_copy_file(file, dst) != 0) {
		fprintf(stderr, "  %s: cannot stage it (is kdos-packd "
				"running?)\n", base);
		free(dst);
		return -1;
	}
	snprintf(req, sizeof(req), "install %s", base);
	rc = packd_ask(req, msg, sizeof(msg));
	if (rc != 0) {
		unlink(dst);
		free(dst);
		fprintf(stderr, "  %s: %s\n", base,
			msg[0] ? msg : "kdos-packd is not running");
		return -1;
	}
	free(dst);
	/* `install` answered `<id> <version>`; the id is what a box's base
	 * names. */
	kb_strlcpy(idout, strtok(msg, " \t\n"), idn);
	return 0;
}

int store_import(const char *archive, const char *const *ids, int n)
{
	char root[MAX_LINE], selp[MAX_LINE + 32];
	char want[CAT_MAX_PACKS][CAT_ID_MAX];
	char *sel;
	size_t sn = 0;
	KbArgv t = {0};
	int nw = 0, done = 0, failed = 0;

	setvbuf(stdout, NULL, _IOLBF, 0);
	if (!kb_path_exists(archive)) {
		fprintf(stderr, "kdos-appbox: %s is not here\n", archive);
		return 1;
	}
	snprintf(root, sizeof(root), "%s/kdos-import-%u", kb_runtime_dir(),
		 (unsigned)getpid());
	kb_mkdir_p(root);

	kb_argv_add(&t, "tar");
	kb_argv_add(&t, "-xf");
	kb_argv_add(&t, archive);
	kb_argv_add(&t, "-C");
	kb_argv_add(&t, root);
	kb_argv_end(&t);
	if (kb_run(&t) != 0) {
		fprintf(stderr, "kdos-appbox: %s does not unpack\n", archive);
		return 1;
	}

	/* The caller's narrowing, or the whole manifest. */
	if (n > 0) {
		for (int i = 0; i < n && nw < CAT_MAX_PACKS; i++)
			kb_strlcpy(want[nw++], ids[i], CAT_ID_MAX);
	} else {
		snprintf(selp, sizeof(selp), "%s/SELECTION", root);
		sel = kb_read_all(selp, &sn);
		if (!sel) {
			fprintf(stderr, "kdos-appbox: %s has no SELECTION\n",
				archive);
			return 1;
		}
		nw = store_selection_parse(sel, want, CAT_MAX_PACKS);
		free(sel);
	}

	for (int i = 0; i < nw; i++) {
		char id[CAT_ID_MAX], kpack[MAX_LINE + CAT_ID_MAX + 32];
		char got[CAT_ID_MAX] = "";
		KbArgv c = {0};

		kb_strlcpy(id, want[i], sizeof(id));
		snprintf(kpack, sizeof(kpack), "%s/%s.kpack", root, id);
		if (!kb_path_exists(kpack)) {
			fprintf(stderr, "  %s: not in this archive\n", id);
			failed++;
			continue;
		}
		printf("==> %s\n", id);
		if (stage_and_install(kpack, got, sizeof(got)) != 0) {
			failed++;
			continue;
		}
		kb_argv_add(&c, "kdos-box");
		kb_argv_add(&c, "create");
		kb_argv_add(&c, id);
		kb_argv_addf(&c, "base=pack:%s", got[0] ? got : id);
		kb_argv_end(&c);
		if (kb_run(&c) != 0) {
			fprintf(stderr, "  %s: the pack installed and the box "
					"did not\n", id);
			failed++;
			continue;
		}
		done++;
	}

	if (done)
		store_launchers();
	{
		KbArgv rm = {0};
		kb_argv_add(&rm, "rm");
		kb_argv_add(&rm, "-rf");
		kb_argv_add(&rm, root);
		kb_argv_end(&rm);
		kb_run(&rm);
	}
	printf("==> %d imported, %d failed\n", done, failed);
	return failed ? 1 : 0;
}

/* ── --selftest ────────────────────────────────────────────────────────── */

/*
 * The Containerfile generator, offline and pure. What is pinned here is what a
 * wrong Containerfile costs: a missing snapshot pin is an unreproducible
 * image, a missing `dpkg --add-architecture i386` is `wine32 has no
 * installation candidate` naming the package rather than the architecture, and
 * an apt RUN emitted against a row with no packages is `apt-get: not found`
 * against an alpine rootfs.
 */
int store_selftest(void)
{
	char buf[16384], err[256];
	int runs;

	if (cat_load(NULL, err, sizeof(err)) != 0) {
		fprintf(stderr, "cat_load: %s\n", err);
		return 1;
	}

	/* A base with its own image row builds FROM that ref and emits no apt
	 * line, because it declares no packages. */
	store_containerfile(cat_find("alpine"), NULL, cat_snapshot(),
			    buf, sizeof(buf));
	cat_chk(strstr(buf, "FROM alpine:3.24.1") != NULL, "alpine FROM its ref");
	cat_chk(strstr(buf, "apt-get") == NULL, "alpine emits no apt RUN");
	cat_chk(strstr(buf, "RUN") == NULL, "a row with no packages has no RUN");

	/* A base with no image row builds FROM the Debian default. */
	store_containerfile(cat_find("base"), NULL, cat_snapshot(),
			    buf, sizeof(buf));
	cat_chk(strstr(buf, "FROM debian:trixie-slim") != NULL, "base FROM debian");
	cat_chk(strstr(buf, "apt-get install -y --no-install-recommends "
			    "bash curl ca-certificates") != NULL,
		"base installs its packages");
	cat_chk(strstr(buf, "snapshot.debian.org/archive/debian/20260824T000000Z/")
		!= NULL, "the snapshot is pinned");
	cat_chk(strstr(buf, "URIs: http://snapshot") != NULL,
		"the pin is http, not https");
	cat_chk(strstr(buf, "Acquire::Check-Valid-Until \"false\";") != NULL,
		"valid-until is off");
	cat_chk(strstr(buf, "sed ") == NULL, "the sources file is written, not sed'd");

	/* A child builds FROM its parent's IMAGE, not from Debian. */
	store_containerfile(cat_find("rt-gtk"), cat_find("base"),
			    cat_snapshot(), buf, sizeof(buf));
	cat_chk(strstr(buf, "FROM kdos/base") != NULL, "rt-gtk FROM kdos/base");
	cat_chk(strstr(buf, "debian:trixie-slim") == NULL,
		"a child never names the Debian image");

	/* The deb row's fetch runs before apt, so apt resolves its deps. */
	store_containerfile(cat_find("app.two"), cat_find("rt-extra"),
			    cat_snapshot(), buf, sizeof(buf));
	cat_chk(strstr(buf, "amd64.deb") != NULL, "the deb pattern is used");
	cat_chk(strstr(buf, "/tmp/pack.deb") != NULL, "the deb is handed to apt");
	{
		const char *fetch = strstr(buf, "curl -fsSL");
		const char *apt = strstr(buf, "apt-get -o");
		cat_chk(fetch && apt && fetch < apt, "the deb is fetched before apt");
	}

	/* ONE RUN, so the image has exactly two layers over its parent. */
	runs = 0;
	for (const char *q = buf; (q = strstr(q, "\nRUN ")); q++)
		runs++;
	cat_chk(runs == 1, "exactly one RUN");

	/* `snapshot = off` emits no pin at all. */
	store_containerfile(cat_find("base"), NULL, "off", buf, sizeof(buf));
	cat_chk(strstr(buf, "snapshot.debian.org") == NULL,
		"snapshot = off emits no pin");
	cat_chk(strstr(buf, "apt-get install") != NULL,
		"snapshot = off still installs");

	/* Truncation is refused rather than emitted half-written. */
	{
		char small[64];
		cat_chk(store_containerfile(cat_find("base"), NULL,
					    cat_snapshot(), small,
					    sizeof(small)) == -1,
			"a buffer too small is refused, not truncated");
	}

	/* The SELECTION manifest round-trips: what export writes, import reads
	 * back. A format whose writer and reader disagree is an archive that
	 * restores nothing and reports success. */
	{
		const char *sel[] = { "app.one", "beta" };
		char m[4096], back[16][CAT_ID_MAX];
		int k;

		store_selection(sel, 2, "test", m, sizeof(m));
		cat_chk(m[0] == '#', "the manifest opens with a comment");
		cat_chk(strstr(m, "\ngroup beta\n") != NULL,
			"a picked group is recorded as a group");
		cat_chk(strstr(m, "\napp.one\n") != NULL, "an id is recorded");

		k = store_selection_parse(m, back, 16);
		cat_chk(k == 1, "the group line is not read back as an id");
		cat_chk(k == 1 && !strcmp(back[0], "app.one"), "the id survives");

		k = store_selection_parse("# only a comment\n\n", back, 16);
		cat_chk(k == 0, "a manifest with no ids is empty, not an error");
	}

	cat_free();
	printf("store: %s\n", cat_fail ? "FAILED" : "ok");
	return cat_fail ? 1 : 0;
}
