/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   matchtest — kdos-runner's matcher with no bus
 *
 *   matchtest query TEXT            the palette's rows
 *   matchtest library TEXT [MS]     the library's rows, servers asked
 *   matchtest ask TEXT [MS]         the palette's rows and the local
 *                                   model's, waited for up to MS
 *   matchtest run TEXT N [term]     execute row N as Run would
 *   matchtest time TEXT N           the slowest of N palette matches, in
 *                                   microseconds, caches warm after the
 *                                   first
 *   matchtest zeal                  "yes" when this build searches docsets
 *   matchtest fuzz FILE             every line of FILE as a query to both
 *                                   sides, and the JSON reader over FILE
 *
 * The sources it links are the runner's own, every one but main.c; the
 * tests in run.sh drive this with tree.sh's fixture home and root.
 * ---------------------------------
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "runner.h"

static long long usec(void)
{
	struct timespec ts;

	clock_gettime(CLOCK_MONOTONIC, &ts);
	return (long long)ts.tv_sec * 1000000 + ts.tv_nsec / 1000;
}

static void print(RnList *l)
{
	KbBuf b = { 0 };

	rn_list_print(l, &b);
	if (b.p)
		fputs(b.p, stdout);
	kb_buf_free(&b);
}

int main(int argc, char **argv)
{
	RnList l = { 0 };

	kb_set_progname("matchtest");
	if (argc == 2 && !strcmp(argv[1], "zeal")) {
		puts(rn_zeal_built() ? "yes" : "no");
	} else if (argc >= 3 && !strcmp(argv[1], "query")) {
		rn_palette_match(argv[2], &l);
		print(&l);
	} else if (argc >= 3 && !strcmp(argv[1], "library")) {
		rn_library_collect(argv[2], argc > 3 ? atoi(argv[3]) : 250, &l);
		print(&l);
	} else if (argc >= 3 && !strcmp(argv[1], "ask")) {
		rn_palette_match(argv[2], &l);
		rn_ask_collect(argv[2], argc > 3 ? atoi(argv[3]) : 2000, &l);
		rn_list_sort(&l);
		print(&l);
	} else if (argc >= 4 && !strcmp(argv[1], "run")) {
		int n = atoi(argv[3]);

		rn_palette_match(argv[2], &l);
		if (n < 0 || n >= l.n) {
			fprintf(stderr, "matchtest: no row %d\n", n);
			return 1;
		}
		rn_run_token("test-token-1");
		if (rn_run(&l.v[n], argc > 4 && !strcmp(argv[4], "term")) != 0)
			return 1;
	} else if (argc >= 4 && !strcmp(argv[1], "time")) {
		long long worst = 0;

		for (int i = 0; i < atoi(argv[3]); i++) {
			long long t0 = usec(), t;

			rn_palette_match(argv[2], &l);
			t = usec() - t0;
			if (i && t > worst)
				worst = t;
			rn_list_free(&l);
		}
		printf("%lld\n", worst);
	} else if (argc >= 3 && !strcmp(argv[1], "fuzz")) {
		char *text = kb_read_whole(argv[2], NULL), *save = NULL;
		char s[256];

		if (!text)
			return 1;
		for (const char *e = json_first(text); e; e = json_next(e))
			json_str(json_get(e, "value"), s, sizeof(s));
		json_str(text, s, sizeof(s));
		for (char *line = strtok_r(text, "\n", &save); line;
		     line = strtok_r(NULL, "\n", &save)) {
			rn_palette_match(line, &l);
			rn_list_free(&l);
			rn_library_collect(line, 1, &l);
			rn_list_free(&l);
			kb_strlcpy(s, line, sizeof(s));
			rn_html_text(s);
		}
		free(text);
	} else {
		fprintf(stderr, "usage: matchtest query|library|ask|run|time|fuzz ...\n");
		return 2;
	}
	rn_list_free(&l);
	return 0;
}
