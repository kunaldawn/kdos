/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   kdos-portup — HTTP layer
 *
 * curl is exec'd through KbArgv, never through a shell: the URL is a recipe's
 * `source` after candidate substitution, so it is untrusted input by the time
 * it reaches here. -f is deliberately absent from the HEAD request — with it
 * curl exits non-zero on a 404 too, collapsing "no such version" and "could
 * not complete the request" into the same return.
 * ---------------------------------
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "kbase.h"
#include "portup.h"

static const char *fixture_dir;

void pu_http_set_fixture_dir(const char *dir)
{
	fixture_dir = dir;
}

/* A URL is not a filename; the slug keeps fixtures readable and collision-free
 * without inventing a hash. Every character that is not alphanumeric, '.' or
 * '-' maps to '_', so the slug is a pure function of the whole URL with no
 * aliasing between separator classes — mapping only "/ : ? &" let two
 * different URLs collide on the same fixture file whenever one substituted a
 * literal '_' for the other's '/' or ':'. */
static void url_slug(const char *url, char *out, size_t cap)
{
	size_t o = 0;
	for (const char *p = url; *p && o + 1 < cap; p++) {
		unsigned char c = (unsigned char)*p;
		int keep = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
			   (c >= '0' && c <= '9') || c == '.' || c == '-';
		out[o++] = keep ? (char)c : '_';
	}
	out[o] = 0;
}

int pu_http_get(const char *url, KbBuf *out)
{
	if (fixture_dir) {
		char slug[512], path[1100];
		url_slug(url, slug, sizeof(slug));
		snprintf(path, sizeof(path), "%s/%s", fixture_dir, slug);
		size_t n = 0;
		char *t = kb_read_all(path, &n);
		if (!t)
			return -1;
		kb_buf_add(out, t, n);
		free(t);
		return 0;
	}

	KbArgv a = {0};
	kb_argv_add(&a, "curl");
	kb_argv_add(&a, "-sSL");
	kb_argv_add(&a, "--max-time");
	kb_argv_add(&a, "20");
	/* Repology's terms require a identifying agent, and several forges
	 * answer a default curl agent with a 403. */
	kb_argv_add(&a, "-A");
	kb_argv_add(&a, "kdos-portup/1 (+https://github.com/kunaldawn/kdos)");
	/* Everything after `--` is positional, so a URL that begins with a
	 * dash cannot be read as a curl option. The URL is already its own
	 * argv element and no shell ever sees it, but argv safety and option
	 * parsing are different problems — the directory adapter builds URLs
	 * from hrefs scraped out of a listing, where nothing constrains the
	 * first character. */
	kb_argv_add(&a, "--");
	kb_argv_add(&a, url);
	kb_argv_end(&a);
	return kb_run_capture_buf(&a, out);
}

int pu_http_head(const char *url)
{
	if (fixture_dir) {
		/* In fixture mode a URL "exists" when a fixture was recorded for
		 * it — that is not a real HEAD, but it is exactly the property
		 * the proof step wants to exercise offline: "did we record a
		 * response for this exact URL". A reader expecting a real
		 * network check here will find this surprising by design. */
		KbBuf b = {0};
		int ok = pu_http_get(url, &b) == 0;
		kb_buf_free(&b);
		return ok ? 200 : 404;
	}

	KbArgv a = {0};
	kb_argv_add(&a, "curl");
	kb_argv_add(&a, "-sIL");	/* HEAD, following redirects */
	kb_argv_add(&a, "-o");
	kb_argv_add(&a, "/dev/null");
	kb_argv_add(&a, "-w");
	kb_argv_add(&a, "%{http_code}");
	kb_argv_add(&a, "--max-time");
	kb_argv_add(&a, "20");
	kb_argv_add(&a, "-A");
	kb_argv_add(&a, "kdos-portup/1 (+https://github.com/kunaldawn/kdos)");
	/* Everything after `--` is positional, so a URL that begins with a
	 * dash cannot be read as a curl option. The URL is already its own
	 * argv element and no shell ever sees it, but argv safety and option
	 * parsing are different problems — the directory adapter builds URLs
	 * from hrefs scraped out of a listing, where nothing constrains the
	 * first character. */
	kb_argv_add(&a, "--");
	kb_argv_add(&a, url);
	kb_argv_end(&a);

	KbBuf out = {0};
	int rc = kb_run_capture_buf(&a, &out);
	int code = (rc == 0 && out.p) ? atoi(out.p) : 0;
	kb_buf_free(&out);
	return code;
}
