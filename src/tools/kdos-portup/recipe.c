/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   kdos-portup — recipe read, candidate render, transactional rewrite
 *
 * The tool never reimplements a recipe's URL template. `kpkg meta` already
 * prints a recipe's fully expanded fields with every helper variable
 * resolved, and it accepts a directory, so a candidate version is probed by
 * copying the recipe into a temp dir with the version substituted and asking
 * `kpkg meta` to expand THAT. ca-certificates' two-helper date rewrite
 * (20260115 -> 2026-01-15) falls out with no special case, and a probe never
 * touches the real ports tree.
 * ---------------------------------
 */

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "kbase.h"
#include "portup.h"

/* `kpkg meta` prints shell assignments, one per line: name='curl'
 * version='8.17.0' ... Scanned line by line rather than by substring search:
 * a substring search for "\nversion='" also matches inside a value that
 * happens to contain that text, and needs a special case for the very first
 * line of the buffer. Matching the key against the START of each line needs
 * neither. */
static void meta_field(const char *out, const char *key, char *dst, size_t cap)
{
	size_t klen = strlen(key);
	const char *p = out;

	while (p && *p) {
		const char *nl = strchr(p, '\n');
		size_t linelen = nl ? (size_t)(nl - p) : strlen(p);

		if (linelen >= klen + 3 && !strncmp(p, key, klen) &&
		    p[klen] == '=' && p[klen + 1] == '\'' &&
		    p[linelen - 1] == '\'') {
			size_t n = linelen - klen - 3;
			if (n >= cap)
				n = cap - 1;
			memcpy(dst, p + klen + 2, n);
			dst[n] = 0;
			return;
		}
		p = nl ? nl + 1 : NULL;
	}
}

/* The key must END here. A bare prefix match would also accept a helper
 * named `version_extra`, and this function does not merely misread such a
 * line — pu_rewrite_version WRITES it, which corrupts the recipe. meta_field
 * already checks this boundary; the two must agree about what "the version
 * line" means, or a probe and an accepted bump can disagree about which
 * line they are talking about.
 *
 * Exported (not static) because main.c's read-only recipe-diff preview used
 * to carry a byte-for-byte copy of this same check under a different name —
 * one definition of "this is the version line" beats two that happen to
 * agree today and silently drift tomorrow. */
int pu_is_version_line(const char *line)
{
	if (strncmp(line, "version", 7))
		return 0;
	char c = line[7];
	return (c == ' ' || c == '\t' || c == '=') && strchr(line, '=') != NULL;
}

/* Atomic replace: write to a temp file in the SAME directory as `path` (so
 * the rename can never cross a filesystem boundary), fsync it, then
 * rename(2) over the original. kb_write_all O_TRUNCs before it writes, so an
 * interruption mid-write — ENOSPC, a kill, a full disk — would otherwise
 * leave a TRUNCATED kpkgbuild rather than the original. That matters twice
 * over here: this same function is also how a failed fetch's REVERT puts the
 * old version back, and a recovery path that can itself destroy the file it
 * is recovering defeats the whole point of the transaction. rename(2) is
 * atomic, so the recipe is always either the old bytes or the new bytes and
 * never a prefix of either.
 *
 * This is deliberately local to kdos-portup rather than a change to
 * kb_write_all: libkbase is shared with kinstall, which ships on the
 * install media, and this is not the moment to change its semantics for
 * every consumer. */
static int write_file_atomic(const char *path, const char *data, size_t len)
{
	char tmp[650];
	int n = snprintf(tmp, sizeof(tmp), "%s.XXXXXX", path);
	if (n < 0 || (size_t)n >= sizeof(tmp))
		return -1;

	int fd = mkstemp(tmp);
	if (fd < 0)
		return -1;
	fchmod(fd, 0644);

	int ok = 1;
	size_t off = 0;
	while (off < len) {
		ssize_t w = write(fd, data + off, len - off);
		if (w <= 0) {
			if (w < 0 && errno == EINTR)
				continue;
			ok = 0;
			break;
		}
		off += (size_t)w;
	}
	if (ok && fsync(fd) != 0)
		ok = 0;
	if (close(fd) != 0)
		ok = 0;
	if (!ok) {
		unlink(tmp);
		return -1;
	}
	if (rename(tmp, path) != 0) {
		unlink(tmp);
		return -1;
	}
	return 0;
}

static int run_meta(const char *kpkg_bin, const char *dir, KbBuf *out)
{
	KbArgv a = {0};
	kb_argv_add(&a, kpkg_bin);
	kb_argv_add(&a, "meta");
	kb_argv_add(&a, dir);
	kb_argv_end(&a);
	return kb_run_capture_buf(&a, out);
}

int pu_recipe_read(const char *kpkg_bin, const char *portdir, PuRecipe *r)
{
	memset(r, 0, sizeof(*r));
	kb_strlcpy(r->portdir, portdir, sizeof(r->portdir));

	KbBuf out = {0};
	if (run_meta(kpkg_bin, portdir, &out) != 0 || !out.p) {
		kb_buf_free(&out);
		return -1;
	}
	meta_field(out.p, "name", r->name, sizeof(r->name));
	meta_field(out.p, "version", r->version, sizeof(r->version));
	meta_field(out.p, "source", r->source, sizeof(r->source));
	meta_field(out.p, "vendoring", r->vendoring, sizeof(r->vendoring));
	meta_field(out.p, "group", r->group, sizeof(r->group));
	kb_buf_free(&out);

	/* `source` is every tarball on one line, port's own first, vendor
	 * bundles after (mesa has 7 total). The updater only ever probes the
	 * first, so split it out here rather than making every caller redo
	 * the same strchr. */
	const char *sp = strchr(r->source, ' ');
	size_t flen = sp ? (size_t)(sp - r->source) : strlen(r->source);
	if (flen >= sizeof(r->first_source))
		flen = sizeof(r->first_source) - 1;
	memcpy(r->first_source, r->source, flen);
	r->first_source[flen] = 0;

	/* Patch count is a risk flag, not metadata: a bump can leave a patch
	 * no longer applying, and the maintainer wants to know before they say
	 * yes rather than during the build. */
	char **names = kb_listdir(portdir, NULL);
	for (char **p = names; p && *p; p++) {
		size_t l = strlen(*p);
		if (l > 6 && !strcmp(*p + l - 6, ".patch"))
			r->npatches++;
	}
	kb_strv_free(names);

	return r->name[0] && r->version[0] ? 0 : -1;
}

int pu_render_candidate(const char *kpkg_bin, const PuRecipe *r,
			const char *candidate, char *url, size_t cap)
{
	char tmpl[] = "/tmp/portup.XXXXXX";
	if (!mkdtemp(tmpl))
		return -1;

	char src[600], dst[600];
	snprintf(src, sizeof(src), "%s/kpkgbuild", r->portdir);
	snprintf(dst, sizeof(dst), "%s/kpkgbuild", tmpl);

	size_t n = 0;
	char *text = kb_read_all(src, &n);
	int rc = -1;
	if (text) {
		KbBuf outb = {0};
		int done = 0;
		for (char *line = text, *next; line && *line; line = next) {
			char *nl = strchr(line, '\n');
			next = nl ? nl + 1 : NULL;
			if (nl)
				*nl = 0;
			/* First match only, matching pu_rewrite_version: a recipe
			 * declares `version` once, so substituting every line that
			 * happens to look like one would let this probe prove a
			 * candidate URL that an accepted bump — which stops at the
			 * first match — would never actually produce. */
			if (!done && pu_is_version_line(line)) {
				char *eq = strchr(line, '=');
				*eq = 0;
				kb_buf_printf(&outb, "%s= %s\n", line, candidate);
				done = 1;
			} else {
				kb_buf_printf(&outb, "%s\n", line);
			}
		}
		if (kb_write_all(dst, outb.p, outb.n) == 0) {
			KbBuf meta = {0};
			if (run_meta(kpkg_bin, tmpl, &meta) == 0 && meta.p) {
				char got[1024] = "";
				meta_field(meta.p, "source", got, sizeof(got));
				if (got[0]) {
					/* Only the port's own tarball — the
					 * first token — is the candidate URL;
					 * vendor bundles after it are not what
					 * an updater probes. */
					char *sp = strchr(got, ' ');
					if (sp)
						*sp = 0;
					kb_strlcpy(url, got, cap);
					rc = 0;
				}
			}
			kb_buf_free(&meta);
		}
		kb_buf_free(&outb);
		free(text);
	}
	kb_rmtree(tmpl);
	return rc;
}

int pu_rewrite_version(const char *portdir, const char *newv)
{
	char path[600];
	snprintf(path, sizeof(path), "%s/kpkgbuild", portdir);

	size_t n = 0;
	char *text = kb_read_all(path, &n);
	if (!text)
		return -1;

	KbBuf out = {0};
	int done = 0;
	for (char *line = text, *next; line && *line; line = next) {
		char *nl = strchr(line, '\n');
		next = nl ? nl + 1 : NULL;
		if (nl)
			*nl = 0;
		if (!done && pu_is_version_line(line)) {
			/* Everything up to and including "= " is copied
			 * verbatim, so the recipe's column alignment survives.
			 * A tool that reflows the file it edits makes every
			 * bump look like a rewrite in `git diff`. */
			char *eq = strchr(line, '=');
			size_t pre = (size_t)(eq - line) + 1;
			kb_buf_add(&out, line, pre);
			const char *v = eq + 1;
			while (*v == ' ')
				kb_buf_add(&out, " ", 1), v++;
			kb_buf_printf(&out, "%s\n", newv);
			done = 1;
		} else {
			kb_buf_printf(&out, "%s\n", line);
		}
	}
	free(text);

	int rc = done ? write_file_atomic(path, out.p, out.n) : -1;
	kb_buf_free(&out);
	return rc;
}
