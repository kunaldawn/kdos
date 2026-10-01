/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   the two hashes a binary package is keyed by
 *
 * Gentoo needs USE-flag matching to decide whether a prebuilt binary is the one
 * you would have built. **KDOS has no USE flags**, so the same question is two
 * equality tests over two hashes:
 *
 *   E:  the RECIPE — kpkgbuild, build.sh, postinstall.sh and every .patch,
 *       sorted by name, each contributing its name AND its bytes, then every
 *       other file beside them that no `sha256 =` line names. A recipe hash
 *       is an exact statement of what a package was built FROM.
 *   B:  the BUILD CONFIG — the flags, the target, the libc and the compiler.
 *       Two machines with the same B produce comparable binaries; two with
 *       different B do not, whatever the recipe says.
 *
 * Both are canonical text hashed with SHA-256, and both include their field
 * NAMES: a hash over values alone collides the moment two fields swap.
 * ---------------------------------
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/utsname.h>

#include "kpkg.h"

/* The files that DEFINE a build, hashed first and by name. Every other file
 * beside them is hashed after them by hash_tree(), unless a `sha256 =` line
 * names it: a tarball or vendor bundle is covered by that line, which
 * `kpkgbuild` checks for EVERY declared entry present beside the recipe, and
 * hashing it here too would only make the key change for no reason. */
static int recipe_file(const char *name)
{
	size_t n = strlen(name);
	return !strcmp(name, "kpkgbuild") || !strcmp(name, "build.sh") ||
	       !strcmp(name, "postinstall.sh") ||
	       (n > 6 && !strcmp(name + n - 6, ".patch"));
}

/* Does a `sha256 = <hash> <file>` line of this kpkgbuild text name `name`?
 * The value is hash/file pairs, so the file names are its odd tokens. The key
 * must start its line, as kp_recipe_key() reads it. */
static int sha256_names(const char *recipe, const char *name)
{
	size_t nl = strlen(name);

	for (const char *line = recipe; line && *line;) {
		const char *end = strchr(line, '\n');
		const char *stop = end ? end : line + strlen(line);
		const char *p = line + 6;

		if (stop - line > 6 && !strncmp(line, "sha256", 6)) {
			while (p < stop && (*p == ' ' || *p == '\t'))
				p++;
			if (p < stop && *p == '=') {
				int tok = 0;

				p++;
				while (p < stop) {
					const char *t;

					while (p < stop && (*p == ' ' || *p == '\t' || *p == '\r'))
						p++;
					t = p;
					while (p < stop && *p != ' ' && *p != '\t' && *p != '\r')
						p++;
					if (p > t && (tok++ & 1) && (size_t)(p - t) == nl &&
					    !strncmp(t, name, nl))
						return 1;
				}
			}
		}
		line = end ? end + 1 : NULL;
	}
	return 0;
}

/*
 * EVERY FILE A BUILD CAN READ IS PART OF ITS RECIPE, unless something else
 * already vouches for its bytes. A port with no `source =` builds out of
 * `$PORT_SRC`, and a sourced port reads its own directory for a kernel config,
 * a desktop entry or a freeze file: nothing names those files, so with only
 * the four recipe files hashed, editing one changes nothing the build can
 * see — the port is reported installed and current, and the tree keeps the
 * binary it already had. The symptom is never a build error; it is a shipped
 * program that behaves like an older one. The price is that any stray file
 * beside a recipe — an editor backup, a `kpkgbuild.new` — changes the hash
 * and rebuilds the port.
 *
 * `recipe`, when not NULL, is the kpkgbuild text, and a top-level file one of
 * its `sha256 =` lines names is skipped. A port with nothing beyond its recipe
 * and its named sources therefore hashes exactly as its recipe files alone.
 *
 * Sorted at EVERY level, because readdir order is the filesystem's and two
 * machines would otherwise disagree about the same tree. Directories are
 * descended into, since a fork keeps its sources under `src/`.
 */
static void hash_tree(KbSha256 *s, const char *root, const char *rel, int depth,
		      const char *recipe)
{
	char here[1024];
	char **names;
	int n = 0;

	/* A port is a directory somebody wrote, not an arbitrary filesystem:
	 * a bound depth means a symlink that points upward cannot spin. */
	if (depth > 8)
		return;
	if (*rel)
		snprintf(here, sizeof(here), "%s/%s", root, rel);
	else
		snprintf(here, sizeof(here), "%s", root);

	names = kb_listdir(here, NULL);
	if (!names)
		return;
	for (char **p = names; *p; p++)
		n++;
	for (int i = 0; i < n; i++)
		for (int j = i + 1; j < n; j++)
			if (strcmp(names[i], names[j]) > 0) {
				char *t = names[i];
				names[i] = names[j];
				names[j] = t;
			}

	for (int i = 0; i < n; i++) {
		char sub[1024], *path;
		size_t len = 0;
		char *data;

		/* Hashed already by the caller, and hashing them twice would
		 * make the two loops' order load-bearing; or vouched for by
		 * their own `sha256 =` line. */
		if (!*rel && (recipe_file(names[i]) ||
			      (recipe && sha256_names(recipe, names[i]))))
			continue;
		if (*rel)
			snprintf(sub, sizeof(sub), "%s/%s", rel, names[i]);
		else
			snprintf(sub, sizeof(sub), "%s", names[i]);
		path = kb_path_join(root, sub);
		if (kb_is_dir(path)) {
			free(path);
			hash_tree(s, root, sub, depth + 1, recipe);
			continue;
		}
		data = kb_read_all(path, &len);
		free(path);
		if (!data)
			continue;
		char hdr[1152];
		int hn = snprintf(hdr, sizeof(hdr), "%s %zu\n", sub, len);
		kb_sha256_update(s, hdr, (size_t)hn);
		kb_sha256_update(s, data, len);
		free(data);
	}
	kb_strv_free(names);
}

/* Does this recipe name any upstream source at all? */
static int has_source(const char *portdir)
{
	char v[4096] = {0};

	kp_recipe_key(portdir, "source", v, sizeof(v));
	return v[0] != 0;
}

int kp_recipe_hash(const char *portdir, char out[65])
{
	char **names = kb_listdir(portdir, NULL);
	int n = 0;
	out[0] = 0;

	if (!names)
		return -1;
	for (char **p = names; *p; p++)
		n++;
	/* Sorted, because readdir order is the filesystem's and would make the
	 * same recipe hash differently on two machines. */
	for (int i = 0; i < n; i++)
		for (int j = i + 1; j < n; j++)
			if (strcmp(names[i], names[j]) > 0) {
				char *t = names[i];
				names[i] = names[j];
				names[j] = t;
			}

	KbSha256 s;
	kb_sha256_init(&s);
	int any = 0;
	for (int i = 0; i < n; i++) {
		if (!recipe_file(names[i]))
			continue;
		char *path = kb_path_join(portdir, names[i]);
		size_t len = 0;
		char *data = kb_read_all(path, &len);
		free(path);
		if (!data)
			continue;
		/* Name, length, then bytes: without the length two files could
		 * be concatenated into the same stream by moving a boundary. */
		char hdr[320];
		int hn = snprintf(hdr, sizeof(hdr), "%s %zu\n", names[i], len);
		kb_sha256_update(&s, hdr, (size_t)hn);
		kb_sha256_update(&s, data, len);
		free(data);
		any = 1;
	}
	kb_strv_free(names);
	if (!any)
		return -1;
	if (has_source(portdir)) {
		char *path = kb_path_join(portdir, "kpkgbuild");
		size_t len = 0;
		char *recipe = kb_read_all(path, &len);

		free(path);
		if (recipe) {
			hash_tree(&s, portdir, "", 0, recipe);
			free(recipe);
		}
	} else {
		char libs[1024];

		hash_tree(&s, portdir, "", 0, NULL);
		/*
		 * AND THE SHARED LIBRARIES, because a source-less port
		 * compiles them into itself. `build.sh` names which — a glob
		 * under $LIBS naming one library's sources — and parsing that
		 * would be a shell parser in the package manager, so all of
		 * `src/libs` is hashed instead. The cost is that editing one
		 * library rebuilds every port of ours rather than only its
		 * consumers; every one of them takes seconds, and the
		 * alternative is shipping a binary compiled against a library
		 * that differs from the one in this tree. The walk needs a `../../libs`
		 * beside the port, which every port under `src/` has; a
		 * source-less port under `ports/core` has none and hashes
		 * its own directory alone.
		 *
		 * A core port sits at `ports/core/<shelf>/<name>`, so its
		 * `../../libs` is `ports/core/libs`: A SHELF MUST NEVER BE NAMED
		 * `libs`. One that was would be hashed whole into every
		 * source-less core port, and any edit on that shelf would
		 * change their recipe hashes and rebuild them.
		 */
		snprintf(libs, sizeof(libs), "%s/../../libs", portdir);
		if (kb_is_dir(libs))
			hash_tree(&s, libs, "", 0, NULL);
	}
	kb_sha256_final(&s, out);
	return 0;
}

/* The compiler's own version string, which is what actually differs between two
 * machines that both say "gcc". Asked once. */
static void compiler_id(char *out, size_t cap)
{
	const char *cc = getenv("CC");
	char buf[256] = {0};
	KbArgv a = {0};

	out[0] = 0;
	kb_argv_add(&a, cc && *cc ? cc : "cc");
	kb_argv_add(&a, "-dumpversion");
	kb_argv_end(&a);
	if (kb_run_capture(&a, buf, sizeof(buf)) == 0) {
		buf[strcspn(buf, "\r\n")] = 0;
		snprintf(out, cap, "%.100s %.100s", cc && *cc ? cc : "cc", buf);
	} else {
		snprintf(out, cap, "unknown");
	}
}

int kp_buildconfig_hash(char out[65], char *human, size_t hcap)
{
	struct utsname u;
	char cc[256];
	const char *cflags = getenv("CFLAGS");
	const char *cxxflags = getenv("CXXFLAGS");
	const char *ldflags = getenv("LDFLAGS");
	const char *target = getenv("KDOS_TARGET");

	uname(&u);
	compiler_id(cc, sizeof(cc));

	/*
	 * One field per line, in a fixed order, each `key=value`. The libc is
	 * stated rather than detected: every KDOS build is musl by construction,
	 * and a field that is always the same is still worth hashing — it is
	 * what makes this key mean "a KDOS build" rather than "some build".
	 */
	KbBuf b = {0};
	kb_buf_printf(&b,
		      "arch=%s\nlibc=musl\ntarget=%s\ncc=%s\n"
		      "cflags=%s\ncxxflags=%s\nldflags=%s\n",
		      u.machine, target && *target ? target : "native", cc,
		      cflags ? cflags : "", cxxflags ? cxxflags : "",
		      ldflags ? ldflags : "");

	KbSha256 s;
	kb_sha256_init(&s);
	kb_sha256_update(&s, b.p, b.n);
	kb_sha256_final(&s, out);
	if (human && hcap)
		kb_strlcpy(human, b.p ? b.p : "", hcap);
	kb_buf_free(&b);
	return 0;
}

void kp_arch(char *out, size_t cap)
{
	struct utsname u;
	uname(&u);
	kb_strlcpy(out, u.machine, cap);
}
