/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   kdos places — Dolphin's Places, from a prompt
 *
 *   kdos places                 the places, one per line
 *   kdos places add DIR [NAME]  keep one
 *
 * THE SAME FILE KDE READS: $XDG_DATA_HOME/user-places.xbel, which Dolphin's
 * Places panel, every KDE file dialog and Kickoff's Places tab show. A list of
 * our own would be a second answer to where a person's places are.
 *
 * `add` IS WHY THIS EXISTS. `mc` cannot drag a folder into Dolphin's panel,
 * so `F2` in the console file manager had no way to say "keep this one" — and
 * the whole point of the list is that it is the places somebody chose.
 *
 * THE FILE IS KIO'S AND IS ONLY EVER ADDED TO. A bookmark is inserted before
 * the closing tag, in the shape KFilePlacesModel writes its own, and nothing
 * else in the file is touched. A home with no file yet is refused rather than
 * given one: KIO lays out the default places (Home, Downloads, Trash…) only
 * into a file that does not exist, so a file created here holding one folder
 * would be a Places panel without them for good.
 * ---------------------------------
 */

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "kbase.h"
#include "kdos-tools.h"

static int usage(void)
{
	fprintf(stderr,
		"usage: kdos places                 the places, one per line\n"
		"       kdos places add DIR [NAME]  keep one\n"
		"\nThe same list Dolphin and every KDE file dialog show.\n");
	return 2;
}

static char *xbel_path(void)
{
	return kdt_data_home("user-places.xbel");
}

/* The text between `open` and `close` at or after *p, unescaped of the five
 * XML entities. NULL when there is none before `limit`. */
static char *between(const char **p, const char *limit, const char *open,
		     const char *close)
{
	const char *a = strstr(*p, open), *b;
	KbBuf o = { 0 };

	if (!a || (limit && a > limit))
		return NULL;
	a += strlen(open);
	b = strstr(a, close);
	if (!b || (limit && b > limit))
		return NULL;
	for (const char *s = a; s < b; s++) {
		static const struct { const char *e; char c; } ENT[] = {
			{ "&amp;", '&' }, { "&lt;", '<' }, { "&gt;", '>' },
			{ "&quot;", '"' }, { "&apos;", '\'' },
		};
		int hit = 0;

		for (size_t i = 0; i < sizeof(ENT) / sizeof(ENT[0]); i++)
			if (!strncmp(s, ENT[i].e, strlen(ENT[i].e))) {
				kb_buf_add(&o, &ENT[i].c, 1);
				s += strlen(ENT[i].e) - 1;
				hit = 1;
				break;
			}
		if (!hit)
			kb_buf_add(&o, s, 1);
	}
	kb_buf_add(&o, "", 1);
	*p = b + strlen(close);
	return o.p;
}

static void xml_escape(KbBuf *b, const char *s)
{
	for (; *s; s++) {
		switch (*s) {
		case '&': kb_buf_str(b, "&amp;"); break;
		case '<': kb_buf_str(b, "&lt;"); break;
		case '>': kb_buf_str(b, "&gt;"); break;
		case '"': kb_buf_str(b, "&quot;"); break;
		default: kb_buf_add(b, s, 1);
		}
	}
}

/* Each bookmark's href and title, hidden ones left out. Returns the count
 * printed, or calls `match` with each href and stops at the first that
 * answers 1. */
static int walk(const char *xml, const char *want_href)
{
	const char *p = xml;
	int n = 0;

	while ((p = strstr(p, "<bookmark href=\""))) {
		const char *end = strstr(p, "</bookmark>");
		const char *q = p;
		char *href = between(&q, end, "<bookmark href=\"", "\"");
		char *title = href ? between(&q, end, "<title>", "</title>")
				   : NULL;
		const char *hid = strstr(p, "<IsHidden>true</IsHidden>");
		int hidden = hid && end && hid < end;

		if (href && want_href && !strcmp(href, want_href)) {
			free(href);
			free(title);
			return 1;
		}
		if (href && !want_href && !hidden) {
			char path[PATH_MAX];

			if (kb_uri_path(href, path, sizeof(path)))
				printf("%-20s %s\n", title ? title : "", path);
			else
				printf("%-20s %s\n", title ? title : "", href);
			n++;
		}
		free(href);
		free(title);
		p = end ? end : p + 1;
	}
	return want_href ? 0 : n;
}

static int add(const char *dir, const char *name)
{
	char *path = xbel_path(), *xml, uri[PATH_MAX * 3 + 8];
	char *close;
	KbBuf out = { 0 };
	int rc = 1;

	xml = kb_read_all(path, NULL);
	if (!xml) {
		fprintf(stderr, "kdos places: %s does not exist yet — open "
				"Dolphin once and KDE lays out the default "
				"places\n", path);
		free(path);
		return 1;
	}
	kb_uri_file(dir, uri, sizeof(uri));
	if (walk(xml, uri)) {
		printf("%s is already a place\n", dir);
		rc = 0;
		goto out;
	}
	close = strstr(xml, "</xbel>");
	if (!close) {
		fprintf(stderr, "kdos places: %s is not an XBEL file\n", path);
		goto out;
	}
	kb_buf_add(&out, xml, (size_t)(close - xml));
	kb_buf_str(&out, " <bookmark href=\"");
	xml_escape(&out, uri);
	kb_buf_str(&out, "\">\n  <title>");
	xml_escape(&out, name);
	kb_buf_printf(&out,
		"</title>\n"
		"  <info>\n"
		"   <metadata owner=\"http://freedesktop.org\">\n"
		"    <bookmark:icon name=\"folder\"/>\n"
		"   </metadata>\n"
		"   <metadata owner=\"http://www.kde.org\">\n"
		"    <ID>%lld/%d</ID>\n"
		"   </metadata>\n"
		"  </info>\n"
		" </bookmark>\n", (long long)time(NULL), (int)getpid() % 1000);
	kb_buf_str(&out, close);
	if (kb_write_file_atomic(path, out.p) != 0) {
		fprintf(stderr, "kdos places: cannot write %s\n", path);
		goto out;
	}
	printf("%s is now a place\n", name);
	rc = 0;
out:
	kb_buf_free(&out);
	free(xml);
	free(path);
	return rc;
}

int kdt_places(int argc, char **argv)
{
	if (argc && !strcmp(argv[0], "add")) {
		/* PATH_MAX and not a smaller round number: realpath writes up
		 * to that whatever the argument's length, and glibc's fortified
		 * form aborts on a buffer that cannot hold it. */
		char real[PATH_MAX];

		if (argc < 2 || argc > 3)
			return usage();
		/*
		 * ABSOLUTE, because the bookmark is read back by programs with
		 * a different working directory. `mc` hands `%d`, which already
		 * is one, but a person typing `kdos places add .` means the
		 * directory they are standing in.
		 */
		if (!realpath(argv[1], real) || !kb_is_dir(real)) {
			fprintf(stderr, "kdos places: %s: no such directory\n",
				argv[1]);
			return 1;
		}
		return add(real, argc > 2 ? argv[2] : kb_basename(real));
	}
	if (argc)
		return usage();
	{
		char *path = xbel_path(), *xml = kb_read_all(path, NULL);

		if (xml)
			walk(xml, NULL);
		free(xml);
		free(path);
	}
	return 0;
}
