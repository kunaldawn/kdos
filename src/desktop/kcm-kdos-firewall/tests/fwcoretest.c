/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   fwcoretest — kdos-fwask's decisions, on a bare host
 *
 *   fwcoretest <scratch directory>
 *
 * Run by testing/selftest.sh. No bus: the counter arithmetic, the process
 * scan under a fake /proc, the answers file and the escaping, each held to
 * the rule fwcore.h states for it.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "fwcore.h"
#include "kbase.h"

static int failures;

#define CHECK(cond, what)                                                     \
	do {                                                                  \
		if (!(cond)) {                                                \
			fprintf(stderr, "  FAIL %s (%s:%d)\n", what, __FILE__, \
				__LINE__);                                    \
			failures++;                                           \
		}                                                             \
	} while (0)

static void knock(FaKnocks *k, const char *name, unsigned long long packets)
{
	kb_strlcpy(k->k[k->n].name, name, FA_NAME);
	k->k[k->n].packets = packets;
	k->n++;
}

static int has(char rose[][FA_NAME], int n, const char *name)
{
	for (int i = 0; i < n; i++)
		if (!strcmp(rose[i], name))
			return 1;
	return 0;
}

static void test_tokens(void)
{
	CHECK(fa_token_ok("kdeconnect", FA_NAME), "a service name is a token");
	CHECK(fa_token_ok("3f1c2d0e-9a8b-4c7d-8e6f-5a4b3c2d1e0f", FA_NET),
	      "a NetworkManager UUID is a token");
	CHECK(fa_token_ok("-", FA_NET), "the unnamed network is a token");
	CHECK(!fa_token_ok("", FA_NAME), "empty is not a token");
	CHECK(!fa_token_ok(NULL, FA_NAME), "NULL is not a token");
	CHECK(!fa_token_ok("a b", FA_NAME), "a space would split a record");
	CHECK(!fa_token_ok("a\nb", FA_NAME), "a newline would forge a record");
	CHECK(!fa_token_ok("SSH", FA_NAME), "uppercase is not a service name");
	CHECK(fa_token_ok("0123456789012345678901234567890", FA_NAME),
	      "31 bytes fits a 32-byte field");
	CHECK(!fa_token_ok("01234567890123456789012345678901", FA_NAME),
	      "32 bytes does not fit a 32-byte field");
}

static void test_knocks(void)
{
	FaKnocks prev = { .n = 0 }, now = { .n = 0 };
	char rose[FA_MAX_KNOCKS][FA_NAME];
	int n;

	knock(&now, "kdeconnect", 5);
	knock(&now, "vnc", 0);
	n = fa_knocks_rose(&prev, &now, rose, FA_MAX_KNOCKS);
	CHECK(n == 1 && has(rose, n, "kdeconnect"),
	      "a first sight with packets counts them all as new");
	CHECK(!has(rose, n, "vnc"), "a first sight with no packets is quiet");

	n = fa_knocks_rose(&prev, &now, rose, FA_MAX_KNOCKS);
	CHECK(n == 0, "an unchanged count is quiet");

	now.n = 0;
	knock(&now, "kdeconnect", 9);
	knock(&now, "vnc", 0);
	n = fa_knocks_rose(&prev, &now, rose, FA_MAX_KNOCKS);
	CHECK(n == 1 && has(rose, n, "kdeconnect"), "a rising count rose");

	now.n = 0;
	knock(&now, "kdeconnect", 2);
	knock(&now, "vnc", 0);
	n = fa_knocks_rose(&prev, &now, rose, FA_MAX_KNOCKS);
	CHECK(n == 1 && has(rose, n, "kdeconnect"),
	      "a count that fell is a reload, and what it holds is new");

	now.n = 0;
	knock(&now, "vnc", 0);
	n = fa_knocks_rose(&prev, &now, rose, FA_MAX_KNOCKS);
	CHECK(n == 0, "a service that left the list (opened) is quiet");
	CHECK(prev.n == 1, "the previous counts become the current ones");
}

static void test_running(const char *dir)
{
	char root[512], p[640];

	snprintf(root, sizeof(root), "%s/proc", dir);
	snprintf(p, sizeof(p), "%s/1234", root);
	kb_mkdir_p(p);
	snprintf(p, sizeof(p), "%s/1234/comm", root);
	kb_write_file(p, "kdeconnectd\n");
	snprintf(p, sizeof(p), "%s/self", root);
	kb_mkdir_p(p);
	snprintf(p, sizeof(p), "%s/self/comm", root);
	kb_write_file(p, "krfb\n");
	snprintf(p, sizeof(p), "%s/77", root);
	kb_mkdir_p(p);
	snprintf(p, sizeof(p), "%s/77/comm", root);
	kb_write_file(p, "kiwix-serve-old\n");

	CHECK(fa_running(root, "kdeconnectd", getuid()),
	      "a program of this user's is found by its comm");
	CHECK(!fa_running(root, "kdeconnectd", getuid() + 1),
	      "another user's program is not this user's question");
	CHECK(!fa_running(root, "krfb", getuid()),
	      "a directory that is not a pid is not a process");
	CHECK(!fa_running(root, "kiwix-serve", getuid()),
	      "a comm is matched whole, never as a prefix");
	CHECK(!fa_running("/nonexistent-proc", "kdeconnectd", getuid()),
	      "no /proc finds nothing");
}

static void test_memory(const char *dir)
{
	char path[512];
	FaMemory m = { 0 };
	char *data;

	snprintf(path, sizeof(path), "%s/state/kdos/fwask", dir);
	CHECK(fa_memory_load(&m, path) == 0, "no file is no answers");
	CHECK(fa_memory_record(&m, path, "net-a", "kdeconnect", "declined") == 0,
	      "an answer is written");
	CHECK(fa_memory_asked(&m, "net-a", "kdeconnect"),
	      "an answer is remembered");
	CHECK(!fa_memory_asked(&m, "net-b", "kdeconnect"),
	      "an answer on one network says nothing of another");
	CHECK(!fa_memory_asked(&m, "net-a", "vnc"),
	      "an answer for one service says nothing of another");
	CHECK(fa_memory_record(&m, path, "net a", "kdeconnect", "declined") != 0,
	      "a network id that would split the record is refused");

	fa_memory_free(&m);
	CHECK(fa_memory_load(&m, path) == 1 && fa_memory_asked(&m, "net-a", "kdeconnect"),
	      "the answer survives a restart");
	fa_memory_free(&m);

	kb_write_file(path, "net-a kdeconnect declined\n"
			    "garbage\n"
			    "net-b vnc allowed extra-field\n"
			    "NET-C rdp declined\n"
			    "net-d rdp dismissed\n");
	CHECK(fa_memory_load(&m, path) == 2, "malformed lines are skipped, not fatal");
	CHECK(fa_memory_asked(&m, "net-d", "rdp"), "the valid lines around them are kept");
	CHECK(!fa_memory_asked(&m, "net-b", "vnc"), "a line with an extra field is dropped");
	fa_memory_free(&m);

	unlink(path);
	for (int i = 0; i < FA_MEMORY_KEEP + 10; i++) {
		char net[32];

		snprintf(net, sizeof(net), "net-%d", i);
		CHECK(fa_memory_record(&m, path, net, "kdeconnect", "declined") == 0,
		      "a long run of answers is written");
	}
	CHECK(m.n == FA_MEMORY_KEEP, "the memory keeps the most recent answers");
	CHECK(!fa_memory_asked(&m, "net-0", "kdeconnect"), "the oldest answers go first");
	CHECK(fa_memory_asked(&m, "net-265", "kdeconnect"), "the newest answer stays");
	fa_memory_free(&m);
	CHECK(fa_memory_load(&m, path) == FA_MEMORY_KEEP, "the rewritten file reads back whole");
	fa_memory_free(&m);
	data = kb_read_all(path, NULL);
	CHECK(data && strstr(data, "net-265 kdeconnect declined\n") &&
		      !strstr(data, "net-0 kdeconnect"),
	      "the file on disk holds the newest answers, newest last");
	free(data);
}

static void test_words(void)
{
	char out[512];
	const FaAsk *a = fa_find("kdeconnect");
	char name[96];

	CHECK(a && !strcmp(a->program, "kdeconnectd"), "KDE Connect is known by its daemon");
	CHECK(!fa_find("http"), "a service no session program serves is never asked about");

	fa_escape(out, sizeof(out), "<b>Cafe & Bar</b>", 64);
	CHECK(!strcmp(out, "&lt;b&gt;Cafe &amp; Bar&lt;/b&gt;"), "markup in a name is escaped");
	fa_escape(out, sizeof(out), "a\nb\tc", 0);
	CHECK(!strcmp(out, "a b c"), "control characters become spaces");

	/* "a" then 40 two-byte characters: a cut at 64 falls inside one. */
	strcpy(name, "a");
	for (int i = 0; i < 40; i++)
		strcat(name, "\xc3\xa9");
	fa_escape(out, sizeof(out), name, 64);
	CHECK(strlen(out) == 63, "a cut never splits a character");
	fa_escape(out, 8, "&&&&&&", 0);
	CHECK(strlen(out) < 8 && !strcmp(out, "&amp;"), "the output is truncated, never overrun");

	fa_body(out, sizeof(out), a, "Home <5G>", 1);
	CHECK(strstr(out, "\xe2\x80\x9cHome &lt;5G&gt;\xe2\x80\x9d") != NULL,
	      "the body names the network, escaped");
	CHECK(strstr(out, "Allow opens KDE Connect to every network") != NULL,
	      "the body says what Allow does");
	fa_body(out, sizeof(out), a, "", 0);
	CHECK(strstr(out, "on this network") && strstr(out, "only an administrator"),
	      "without permission the body says who can");
}

int main(int argc, char **argv)
{
	if (argc != 2) {
		fprintf(stderr, "usage: fwcoretest <scratch>\n");
		return 2;
	}
	test_tokens();
	test_knocks();
	test_running(argv[1]);
	test_memory(argv[1]);
	test_words();
	if (failures) {
		fprintf(stderr, "  %d check(s) failed\n", failures);
		return 1;
	}
	return 0;
}
