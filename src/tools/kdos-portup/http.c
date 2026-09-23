/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   kdos-portup — HTTP and git layer
 *
 * curl and git are exec'd through KbArgv, never through a shell: the URL is a
 * recipe's `source` after candidate substitution, or an href scraped out of a
 * listing, so it is untrusted input by the time it reaches here. -f is
 * deliberately absent from every curl request — with it curl exits non-zero
 * on a 404 too, collapsing "no such version" and "could not complete the
 * request" into the same return. The status is read with -w instead.
 * ---------------------------------
 */

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "kbase.h"
#include "portup.h"

#define PU_UA "kdos-portup/1 (+https://github.com/kunaldawn/kdos)"

/* Three attempts, pausing 2 s and then 5 s. Several checks run at once
 * (main.c's worker pool), and a host that answers one of them 429 or 503
 * answers the retry a moment later; a longer ladder only delays the
 * `unknown` a genuinely dead host earns. */
#define PU_TRIES 3
static const int retry_pause_s[PU_TRIES - 1] = { 2, 5 };

/* Written after the body by -w, and cut off again before any caller sees the
 * body. Starts with a newline and carries bytes no listing or JSON document
 * contains, so the LAST occurrence is always curl's and never the body's. */
static const char STATUS_MARK[] = "\n\x01kdos-portup-status:";

static const char *fixture_dir;

void pu_http_set_fixture_dir(const char *dir)
{
	fixture_dir = dir;
}

const char *pu_http_fixture_dir(void)
{
	return fixture_dir;
}

/* A URL is not a filename; the slug keeps fixtures readable without inventing
 * a hash. Every character that is not alphanumeric, '.' or '-' maps to '_',
 * so two URLs that differ only in WHICH such character sits at a position
 * (a '_' against a '/', a '?' against a '&') share one fixture file. Changing
 * the mapping renames every recorded fixture. */
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

/* 200 and the recorded body when a fixture exists for `key`, 404 when not. */
static int fixture_read(const char *key, KbBuf *out)
{
	char slug[512], path[1100];
	url_slug(key, slug, sizeof(slug));
	snprintf(path, sizeof(path), "%s/%s", fixture_dir, slug);
	size_t n = 0;
	char *t = kb_read_all(path, &n);
	if (!t)
		return 404;
	if (out)
		kb_buf_add(out, t, n);
	free(t);
	return 200;
}

static int transient(int code)
{
	return code == 0 || code == 429 || code >= 500;
}

static void pause_s(int s)
{
	struct timespec ts = { s, 0 };
	while (nanosleep(&ts, &ts) != 0 && errno == EINTR)
		;
}

static void add_common(KbArgv *a)
{
	kb_argv_add(a, "--max-time");
	kb_argv_add(a, "30");
	kb_argv_add(a, "--connect-timeout");
	kb_argv_add(a, "15");
	/* Repology's terms require an identifying agent, crates.io refuses a
	 * request without one, and several forges answer a default curl
	 * agent with a 403. */
	kb_argv_add(a, "-A");
	kb_argv_add(a, PU_UA);
}

/* Everything after `--` is positional, so a URL that begins with a dash
 * cannot be read as a curl option. The URL is already its own argv element
 * and no shell ever sees it, but argv safety and option parsing are
 * different problems — the directory adapter builds URLs from hrefs scraped
 * out of a listing, where nothing constrains the first character. */
static void add_url(KbArgv *a, const char *url)
{
	kb_argv_add(a, "--");
	kb_argv_add(a, url);
	kb_argv_end(a);
}

int pu_http_verdict(int curl_exit, int code)
{
	/* A 2xx whose transfer stopped part-way (curl 18, 28, 56...) carries
	 * the head of a listing, and listings sort oldest first: what is
	 * missing is exactly the newest entries. Read as a complete body it
	 * would report `current`. A redirect is the final status only when
	 * its next hop failed, which is no answer either. Any other status is
	 * an answer on its own, whatever became of the error page after it. */
	if (curl_exit != 0 && code >= 200 && code < 400)
		return 0;
	return code;
}

/* One GET; the body lands in `out` with the status trailer removed. */
static int get_once(const char *url, KbBuf *out)
{
	KbArgv a = {0};
	kb_argv_add(&a, "curl");
	kb_argv_add(&a, "-sSL");
	kb_argv_add(&a, "-w");
	kb_argv_add(&a, "\n\x01kdos-portup-status:%{http_code}");
	add_common(&a);
	add_url(&a, url);

	KbBuf b = {0};
	int rc = kb_run_capture_buf(&a, &b);

	/* The trailer says what the server answered, and 000 for a request
	 * that never connected; curl's exit status says whether the body
	 * after it arrived whole. pu_http_verdict weighs the two. */
	int code = 0;
	size_t ml = sizeof(STATUS_MARK) - 1;
	if (b.p && b.n >= ml) {
		for (size_t i = b.n - ml + 1; i-- > 0;) {
			if (!memcmp(b.p + i, STATUS_MARK, ml)) {
				code = pu_http_verdict(rc, atoi(b.p + i + ml));
				if (code)
					kb_buf_add(out, b.p, i);
				break;
			}
		}
	}
	kb_buf_free(&b);
	return code;
}

int pu_http_get_once(const char *url, KbBuf *out)
{
	if (fixture_dir)
		return fixture_read(url, out);
	out->n = 0;
	if (out->p)
		out->p[0] = 0;
	return get_once(url, out);
}

int pu_http_get(const char *url, KbBuf *out)
{
	if (fixture_dir)
		return fixture_read(url, out);

	int code = 0;
	for (int t = 0; t < PU_TRIES; t++) {
		if (t)
			pause_s(retry_pause_s[t - 1]);
		/* A retried body replaces the failed one rather than following
		 * it: out holds exactly one response. */
		out->n = 0;
		if (out->p)
			out->p[0] = 0;
		code = get_once(url, out);
		if (!transient(code))
			break;
	}
	return code;
}

static int status_request(const char *url, int ranged)
{
	KbArgv a = {0};
	kb_argv_add(&a, "curl");
	if (ranged) {
		/* One byte of the body. A server that ignores ranges answers
		 * 200 and streams the whole file into /dev/null — slower, but
		 * the status is the same answer. */
		kb_argv_add(&a, "-sL");
		kb_argv_add(&a, "-r");
		kb_argv_add(&a, "0-0");
	} else {
		kb_argv_add(&a, "-sIL");	/* HEAD, following redirects */
	}
	kb_argv_add(&a, "-o");
	kb_argv_add(&a, "/dev/null");
	kb_argv_add(&a, "-w");
	kb_argv_add(&a, "%{http_code}");
	add_common(&a);
	add_url(&a, url);

	KbBuf out = {0};
	int rc = kb_run_capture_buf(&a, &out);
	int code = out.p ? atoi(out.p) : 0;
	kb_buf_free(&out);
	/* A redirect is only ever the final status when the next hop failed
	 * (mesa.freedesktop.org's archive goes through three hosts): the
	 * request never got an answer, and read as one it ends the proof. A
	 * 2xx stands whatever followed it — the status line is the proof, and
	 * a ranged GET a server answers with the whole file can run out of
	 * time after it. */
	if (rc != 0 && code >= 300 && code < 400)
		code = 0;
	return code;
}

int pu_http_head(const char *url)
{
	if (fixture_dir) {
		/* In fixture mode a URL "exists" when a fixture was recorded for
		 * it — that is not a real HEAD, but it is exactly the property
		 * the proof step wants to exercise offline: "did we record a
		 * response for this exact URL". A reader expecting a real
		 * network check here will find this surprising by design. */
		return fixture_read(url, NULL);
	}

	int code = 0;
	for (int t = 0; t < PU_TRIES; t++) {
		if (t)
			pause_s(retry_pause_s[t - 1]);
		code = status_request(url, 0);
		/* gitlab.freedesktop.org answers HEAD for a release file 403
		 * and a ranged GET of the same file 206. */
		if (code == 403 || code == 405 || code == 501) {
			int g = status_request(url, 1);
			code = (g == 206) ? 200 : g;
		}
		if (!transient(code))
			break;
	}
	return code;
}

static int ls_remote(const char *repo, int heads, KbBuf *out)
{
	if (fixture_dir) {
		char key[1100];
		snprintf(key, sizeof(key), "%s+%s", heads ? "git-heads" : "git",
			 repo);
		return fixture_read(key, out) == 200 ? 0 : -1;
	}
	/* Only an https URL this tool built: git reads a leading dash as an
	 * option and has no `--` before the repository. */
	if (strncmp(repo, "https://", 8))
		return -1;

	for (int t = 0; t < 2; t++) {
		if (t)
			pause_s(retry_pause_s[0]);
		KbArgv a = {0};
		/* Without the invoking user's configuration: a url.*.insteadOf
		 * there turns the https URL into ssh, which asks for a key's
		 * passphrase or a host key on the terminal from every worker
		 * at once. The protocol allow-list below refuses such a
		 * rewrite should one still arrive. */
		kb_argv_add(&a, "env");
		kb_argv_add(&a, "GIT_CONFIG_GLOBAL=/dev/null");
		kb_argv_add(&a, "GIT_CONFIG_NOSYSTEM=1");
		/* git has no overall timeout of its own; a connection that
		 * never completes would hold a worker forever. */
		kb_argv_add(&a, "timeout");
		kb_argv_add(&a, "60");
		kb_argv_add(&a, "git");
		/* No credential helper and (main.c's environment) no prompt:
		 * a repository that has moved or gone private answers "auth
		 * required", and asking a human for a password in the middle
		 * of a batch check is the wrong response to that. */
		kb_argv_add(&a, "-c");
		kb_argv_add(&a, "credential.helper=");
		kb_argv_add(&a, "-c");
		kb_argv_add(&a, "http.lowSpeedLimit=1");
		kb_argv_add(&a, "-c");
		kb_argv_add(&a, "http.lowSpeedTime=30");
		kb_argv_add(&a, "-c");
		kb_argv_add(&a, "protocol.allow=never");
		kb_argv_add(&a, "-c");
		kb_argv_add(&a, "protocol.https.allow=always");
		/* Without --refs: an annotated tag's peeled line (<tag>^{})
		 * names the commit it points at, and two tags on one commit
		 * are one release under two names (probe.c). */
		kb_argv_add(&a, "ls-remote");
		if (!heads)
			kb_argv_add(&a, "--tags");
		kb_argv_add(&a, repo);
		if (heads) {
			kb_argv_add(&a, "HEAD");
			kb_argv_add(&a, "refs/heads/*");
		}
		kb_argv_end(&a);

		out->n = 0;
		if (out->p)
			out->p[0] = 0;
		if (kb_run_capture_buf(&a, out) == 0)
			return 0;
	}
	return -1;
}

int pu_git_tags(const char *repo, KbBuf *out)
{
	return ls_remote(repo, 0, out);
}

int pu_git_heads(const char *repo, KbBuf *out)
{
	return ls_remote(repo, 1, out);
}
