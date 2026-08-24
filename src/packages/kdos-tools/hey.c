/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   kdos hey — the compositor, from a shell prompt
 *
 *   kdos hey list [--json]              every toplevel: id, workspace, state
 *   kdos hey run <action> <id>          any labwc action, aimed at one view
 *   kdos hey outputs [--json]           the screens
 *
 * Haiku's `hey` is the ancestor and the shape is deliberately the same one:
 * the window manager answers questions from the command line, so a window is
 * something a script can find and act on. What is KDOS-shaped about it is the
 * transport — $XDG_RUNTIME_DIR/kdos-cmd.sock, one NDJSON request line in, one
 * NDJSON response line out, the socket closes — cloned from kdos-frames rather
 * than invented, and deliberately NOT a Wayland protocol: it is one distro's
 * channel between two of its own programs.
 *
 * The JSON reader here is a scanner over one response line and nothing more.
 * It is not a JSON parser and must not become one: the only document it will
 * ever see is the one kdos-comp's kdos-cmd graft writes, and libkbuild's
 * kj_parse — the real one — lives in a library this package does not link.
 * --json prints the response verbatim, which is the escape hatch for anything
 * this scanner cannot see.
 * ---------------------------------
 */

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>

#include "kdos-tools.h"

#define CMD_SOCKET "kdos-cmd.sock"

/* ── the one-line JSON scanner ─────────────────────────────────────────── */

/*
 * Walk to the next `{...}` at or after *p, string-aware. Window titles carry
 * braces and quotes routinely — "foo}bar" in a filename is not exotic — so a
 * splitter that counted braces without knowing where strings are would cut an
 * object in half and silently report the wrong window.
 */
static int j_object(const char **p, const char **beg, const char **end)
{
	const char *s = *p;
	while (*s && *s != '{')
		s++;
	if (!*s)
		return 0;
	*beg = s;

	int depth = 0, instr = 0;
	for (; *s; s++) {
		if (instr) {
			if (*s == '\\' && s[1])
				s++;
			else if (*s == '"')
				instr = 0;
			continue;
		}
		if (*s == '"')
			instr = 1;
		else if (*s == '{')
			depth++;
		else if (*s == '}' && --depth == 0) {
			*end = s + 1;
			*p = s + 1;
			return 1;
		}
	}
	return 0;
}

/* The value after `"key":` inside [beg,end), or NULL. Only top-level keys of
 * the object are wanted, and every object kdos-comp emits is flat, so nesting
 * is not tracked. */
static const char *j_value(const char *beg, const char *end, const char *key)
{
	size_t klen = strlen(key);
	for (const char *s = beg; s + klen + 2 < end; s++) {
		if (*s != '"' || strncmp(s + 1, key, klen) || s[1 + klen] != '"')
			continue;
		const char *v = s + 2 + klen;
		while (v < end && (*v == ' ' || *v == ':'))
			v++;
		return v < end ? v : NULL;
	}
	return NULL;
}

/* Decodes the two escapes JSON requires plus the shorthand controls; \uXXXX is
 * copied as-is rather than decoded, because this prints into a cell grid and a
 * half-decoded codepoint is worse than a visible escape. */
static void j_str(const char *beg, const char *end, const char *key, char *out,
		  size_t cap)
{
	out[0] = 0;
	const char *v = j_value(beg, end, key);
	if (!v || *v != '"')
		return;
	size_t o = 0;
	for (const char *s = v + 1; s < end && *s != '"' && o + 1 < cap; s++) {
		if (*s == '\\' && s + 1 < end) {
			s++;
			switch (*s) {
			case 'n': out[o++] = ' '; break;
			case 't': out[o++] = ' '; break;
			case 'r': case 'b': case 'f': break;
			default:  out[o++] = *s; break;
			}
			continue;
		}
		out[o++] = *s;
	}
	out[o] = 0;
}

static long j_int(const char *beg, const char *end, const char *key, long dflt)
{
	const char *v = j_value(beg, end, key);
	if (!v || (*v != '-' && (*v < '0' || *v > '9')))
		return dflt;
	return strtol(v, NULL, 10);
}

static double j_num(const char *beg, const char *end, const char *key,
		    double dflt)
{
	const char *v = j_value(beg, end, key);
	if (!v || (*v != '-' && *v != '.' && (*v < '0' || *v > '9')))
		return dflt;
	return strtod(v, NULL);
}

static int j_bool(const char *beg, const char *end, const char *key)
{
	const char *v = j_value(beg, end, key);
	return v && *v == 't';
}

/* ── the socket ────────────────────────────────────────────────────────── */

/*
 * The message this prints is the whole of what a missing socket means, and it
 * has to name both possibilities: the compositor may not be running at all, or
 * it may be an older kdos-comp with no kdos-cmd graft in it. Reporting one and
 * not the other sends half the people who see it to the wrong place.
 */
static void no_socket(void)
{
	fprintf(stderr, "kdos hey: the compositor does not expose the command "
			"socket (kdos-comp too old, or not running)\n");
}

/*
 * `quiet` is for a caller that is ASKING WHETHER the compositor is there
 * rather than being told to talk to it — `kdos appid`'s fallback runs on a
 * machine that may have no session at all, and a diagnostic about a socket
 * nobody mentioned would be noise in the middle of its report.
 */
static int cmd_socket(int quiet)
{
	const char *rt = getenv("XDG_RUNTIME_DIR");
	if (!rt || !*rt) {
		if (!quiet)
			no_socket();
		return -1;
	}
	struct sockaddr_un addr = { .sun_family = AF_UNIX };
	snprintf(addr.sun_path, sizeof(addr.sun_path), "%s/%s", rt, CMD_SOCKET);

	int fd = socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0);
	if (fd < 0) {
		if (!quiet)
			no_socket();
		return -1;
	}
	if (connect(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
		close(fd);
		if (!quiet)
			no_socket();
		return -1;
	}
	return fd;
}

/*
 * One request, one response, close. The reply is read to EOF rather than to
 * the first newline: the protocol promises one line, and reading to EOF is
 * what notices when it did not keep that promise instead of leaving the rest
 * in the socket buffer for nobody.
 */
static char *ask_q(const char *req, int quiet)
{
	int fd = cmd_socket(quiet);
	if (fd < 0)
		return NULL;

	size_t len = strlen(req);
	if (write(fd, req, len) != (ssize_t)len) {
		if (!quiet)
			fprintf(stderr, "kdos hey: %s\n", strerror(errno));
		close(fd);
		return NULL;
	}
	shutdown(fd, SHUT_WR);

	KbBuf b = {0};
	char chunk[4096];
	ssize_t n;
	while ((n = read(fd, chunk, sizeof(chunk))) > 0)
		kb_buf_add(&b, chunk, (size_t)n);
	close(fd);

	if (!b.n) {
		if (!quiet)
			fprintf(stderr, "kdos hey: the compositor closed "
					"without answering\n");
		kb_buf_free(&b);
		return NULL;
	}
	kb_buf_add(&b, "", 1);		/* NUL, not counted in b.n */
	return b.p;
}

static char *ask(const char *req)
{
	return ask_q(req, 0);
}

/*
 * `"ok":false` carries an `err` that is the compositor's own words; printing
 * anything else here would be this program guessing at what went wrong.
 *
 * `end` bounds the ENVELOPE rather than the document: the array that follows
 * carries window titles, and a title containing the text `"ok"` would
 * otherwise be read as the response's own verdict.
 */
static int reply_ok(const char *reply, const char *end)
{
	if (j_bool(reply, end, "ok"))
		return 1;
	char err[256];
	j_str(reply, end, "err", err, sizeof(err));
	fprintf(stderr, "kdos hey: %s\n", err[0] ? err : "refused");
	return 0;
}

/* ── the tables ────────────────────────────────────────────────────────── */

static void state_letters(const char *beg, const char *end, char out[8])
{
	int o = 0;
	if (j_bool(beg, end, "focused"))
		out[o++] = 'F';
	if (j_bool(beg, end, "minimized"))
		out[o++] = 'm';
	if (j_bool(beg, end, "maximized"))
		out[o++] = 'M';
	if (j_bool(beg, end, "fullscreen"))
		out[o++] = 'X';
	if (j_bool(beg, end, "shaded"))
		out[o++] = 's';
	out[o] = 0;
}

static int list_table(const char *reply)
{
	const char *beg, *end;
	const char *views = strstr(reply, "\"views\"");
	const char *p = views ? views : reply + strlen(reply);

	if (!reply_ok(reply, p))
		return 1;

	printf("  %3s %3s %-6s %-22s %s\n", "id", "ws", "state", "app_id",
	       "title");
	int n = 0;
	while (j_object(&p, &beg, &end)) {
		char app[64], title[160], st[8];
		j_str(beg, end, "app_id", app, sizeof(app));
		j_str(beg, end, "title", title, sizeof(title));
		state_letters(beg, end, st);
		printf("  %3ld %3ld %-6s %-22.22s %.60s\n",
		       j_int(beg, end, "id", -1), j_int(beg, end, "workspace", 0),
		       st, app[0] ? app : "-", title);
		n++;
	}
	if (!n)
		printf("  (no windows)\n");
	return 0;
}

static int outputs_table(const char *reply)
{
	const char *beg, *end;
	const char *outs = strstr(reply, "\"outputs\"");
	const char *p = outs ? outs : reply + strlen(reply);

	if (!reply_ok(reply, p))
		return 1;

	printf("  %-14s %6s %6s %6s %6s %6s\n", "name", "w", "h", "scale", "x",
	       "y");
	int n = 0;
	while (j_object(&p, &beg, &end)) {
		char name[64];
		j_str(beg, end, "name", name, sizeof(name));
		printf("  %-14.14s %6ld %6ld %6.2f %6ld %6ld\n",
		       name[0] ? name : "-", j_int(beg, end, "w", 0),
		       j_int(beg, end, "h", 0), j_num(beg, end, "scale", 1.0),
		       j_int(beg, end, "x", 0), j_int(beg, end, "y", 0));
		n++;
	}
	if (!n)
		printf("  (no outputs)\n");
	return 0;
}

/* ── the front end ─────────────────────────────────────────────────────── */

static int usage(void)
{
	fprintf(stderr,
		"usage: kdos hey list [--json]\n"
		"       kdos hey run <action> <id>\n"
		"       kdos hey outputs [--json]\n"
		"\n"
		"<action> is a labwc action name: Close, Focus, Iconify,\n"
		"ToggleMaximize, ToggleShade, ToggleFullscreen, Raise, ...\n");
	return 2;
}

/*
 * THE app_ids OF THE WINDOWS OPEN RIGHT NOW, which is `kdos appid`'s fallback
 * when the recorded ledger is not there — a fresh boot, or a home that has
 * never run this compositor.
 *
 * It lives HERE rather than in appid.c because this file already owns the
 * string-aware object walk, and a window title containing the literal text
 * `"app_id":"` is not exotic: a scan for that substring would report a window
 * that does not exist. Two parsers would be two answers.
 *
 * Answers -1 when the compositor cannot be reached, which the caller must tell
 * apart from 0 — "nothing is running" and "nobody asked" are different
 * verdicts, and folding them is the confident wrong answer that tool exists
 * not to give.
 */
int hey_app_ids(char ***out)
{
	char *reply = ask_q("{\"cmd\":\"list\"}\n", 1);
	const char *p, *beg, *end;
	char **v = NULL;
	int n = 0;

	*out = NULL;
	if (!reply)
		return -1;
	/* The envelope is the FIRST object; every one after it is a view. */
	p = reply;
	if (!j_object(&p, &beg, &end) || !reply_ok(reply, end)) {
		free(reply);
		return -1;
	}
	/*
	 * Counted first, then filled. There is no realloc in libkbase and a
	 * fixed cap would silently drop a window, which is the one thing a
	 * checker must not do; two walks of a reply that is a few kilobytes
	 * cost nothing worth measuring.
	 */
	{
		const char *q = p;
		int cap = 0;

		while (j_object(&q, &beg, &end))
			cap++;
		if (!cap) {
			free(reply);
			return 0;
		}
		v = kb_calloc((size_t)cap, sizeof(*v));
	}
	while (j_object(&p, &beg, &end)) {
		char id[256];
		int dup = 0;

		j_str(beg, end, "app_id", id, sizeof(id));
		if (!id[0])
			continue;
		for (int i = 0; i < n; i++)
			if (!strcmp(v[i], id))
				dup = 1;
		if (!dup)
			v[n++] = kb_strdup(id);
	}
	free(reply);
	*out = v;
	return n;
}

int hey_main(int argc, char **argv)
{
	if (argc < 2)
		return usage();

	int json = 0;
	for (int i = 2; i < argc; i++)
		if (!strcmp(argv[i], "--json"))
			json = 1;

	const char *what = argv[1];

	if (!strcmp(what, "list") || !strcmp(what, "outputs")) {
		char req[64];
		snprintf(req, sizeof(req), "{\"cmd\":\"%s\"}\n", what);
		char *reply = ask(req);
		if (!reply)
			return 1;
		int rc;
		if (json) {
			fputs(reply, stdout);
			if (reply[0] && reply[strlen(reply) - 1] != '\n')
				fputc('\n', stdout);
			rc = 0;
		} else {
			rc = what[0] == 'l' ? list_table(reply)
					    : outputs_table(reply);
		}
		free(reply);
		return rc;
	}

	if (!strcmp(what, "run")) {
		if (argc < 4)
			return usage();
		const char *action = argv[2];
		char *tail = NULL;
		long id = strtol(argv[3], &tail, 10);
		if (!tail || *tail || id < 0) {
			fprintf(stderr, "kdos hey: '%s' is not a view id — "
					"`kdos hey list` prints them\n", argv[3]);
			return 2;
		}
		/* The action is quoted into the request rather than
		 * concatenated raw: it comes from argv, and a name with a quote
		 * in it would otherwise produce a request the compositor cannot
		 * parse instead of one it refuses. */
		KbBuf b = {0};
		kb_buf_str(&b, "{\"cmd\":\"run\",\"action\":");
		kb_json_str(&b, action);
		kb_buf_printf(&b, ",\"id\":%ld}\n", id);
		kb_buf_add(&b, "", 1);

		char *reply = ask(b.p);
		kb_buf_free(&b);
		if (!reply)
			return 1;
		if (json) {
			fputs(reply, stdout);
			if (reply[0] && reply[strlen(reply) - 1] != '\n')
				fputc('\n', stdout);
			free(reply);
			return 0;
		}
		int ok = reply_ok(reply, reply + strlen(reply));
		free(reply);
		return ok ? 0 : 1;
	}

	return usage();
}
