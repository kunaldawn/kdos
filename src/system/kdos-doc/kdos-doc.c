/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   kdos-doc — the KDOS book, opened at a page
 *
 *   kdos-doc                                 the title page
 *   kdos-doc 04-programs/kdos-command        a chapter
 *   kdos-doc 04-programs/kdos-command#kdos-theme
 *
 * THE BOOK IS THE HTML mkbook.py INSTALLED under /usr/share/doc/kdos, read in
 * the Help Center, where it is KDOS's first entry; a machine without
 * KHelpCenter gets the desktop's browser through xdg-open, and a console with
 * neither gets the file's path, which `lynx` or `w3m` can open. A page is
 * named as the book's own links name it, with or without `.md` or `.html`.
 *
 * THE NAME IS CHECKED BEFORE IT IS USED: letters, digits and `._-/` for the
 * page, the same for the anchor, and no `..`, so the argument can only ever
 * name a page under the book's directory. It reaches the viewer as one
 * argument, through exec and never a shell.
 *
 * Exit 1 when the page is not in the book, 2 usage, otherwise the viewer's.
 * ---------------------------------
 */

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define DOCROOT "/usr/share/doc/kdos"

static int usage(void)
{
	fprintf(stderr, "usage: kdos-doc [chapter/page[#anchor]]\n");
	return 2;
}

static int clean(const char *s)
{
	return strspn(s, "abcdefghijklmnopqrstuvwxyz"
			 "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789._-/") ==
		       strlen(s) &&
	       !strstr(s, "..");
}

/* A program on PATH, by the same search exec would make. */
static int have(const char *prog)
{
	const char *path = getenv("PATH");
	char *copy, *save = NULL, buf[4096];
	int found = 0;

	copy = strdup(path && *path ? path : "/usr/local/bin:/usr/bin:/bin");
	if (!copy)
		return 0;
	for (char *d = strtok_r(copy, ":", &save); d && !found;
	     d = strtok_r(NULL, ":", &save)) {
		snprintf(buf, sizeof(buf), "%s/%s", d, prog);
		found = access(buf, X_OK) == 0;
	}
	free(copy);
	return found;
}

int main(int argc, char **argv)
{
	char page[1024] = "index", anchor[256] = "", file[2048], url[2400];
	char *hash;

	if (argc > 2 || (argc == 2 && (!strcmp(argv[1], "-h") ||
				       !strcmp(argv[1], "--help"))))
		return usage();
	if (argc == 2 && *argv[1]) {
		snprintf(page, sizeof(page), "%s", argv[1]);
		if ((hash = strchr(page, '#'))) {
			*hash = '\0';
			snprintf(anchor, sizeof(anchor), "%s", hash + 1);
		}
		while (page[0] == '/')
			memmove(page, page + 1, strlen(page));
		{
			size_t n = strlen(page);

			if (n > 3 && !strcmp(page + n - 3, ".md"))
				page[n - 3] = '\0';
			else if (n > 5 && !strcmp(page + n - 5, ".html"))
				page[n - 5] = '\0';
		}
		if (!*page)
			snprintf(page, sizeof(page), "index");
		if (!clean(page) || !clean(anchor) || strchr(anchor, '/'))
			return usage();
	}
	snprintf(file, sizeof(file), DOCROOT "/%s.html", page);
	if (access(file, R_OK) != 0) {
		/* A chapter named by its directory is its README. */
		snprintf(file, sizeof(file), DOCROOT "/%s/README.html", page);
		if (access(file, R_OK) != 0) {
			fprintf(stderr, "kdos-doc: %s is not a page of the book "
					"in " DOCROOT "\n", page);
			return 1;
		}
	}
	snprintf(url, sizeof(url), "file://%s%s%s", file, *anchor ? "#" : "",
		 anchor);
	if (have("khelpcenter"))
		execlp("khelpcenter", "khelpcenter", url, (char *)NULL);
	if (getenv("WAYLAND_DISPLAY") && have("xdg-open"))
		execlp("xdg-open", "xdg-open", url, (char *)NULL);
	printf("%s\n", file);
	return 0;
}
