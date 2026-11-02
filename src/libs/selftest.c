/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   src/libs/selftest.c — the regression net for libk*
 *
 * Host-only; nothing links this into the ISO. Run it with
 * `testing/selftest.sh`, which compiles it against the libraries and
 * executes it. Many
 * programs share these libraries, and a change to one of them can quietly
 * break a distro artefact that is only visible after a full build.
 *
 * The assertions here are INVARIANTS, each a claim measured against a
 * reference implementation such as python's colorsys. This file is what
 * notices when one stops being true.
 * ---------------------------------
 */

#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/file.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

#include "kbase.h"
#include "kcolor.h"
#include "kbuild.h"
#include "kpkg.h"
#include "ktui.h"
#include "portup.h"
#include "kproc.h"
#include "kxdg.h"
#include "kpack.h"

static int failures;
static int checks;

static void ok(int cond, const char *what)
{
	checks++;
	if (cond)
		return;
	failures++;
	printf("  FAIL  %s\n", what);
}

static void eq_str(const char *got, const char *want, const char *what)
{
	checks++;
	if (got && want && !strcmp(got, want))
		return;
	failures++;
	printf("  FAIL  %s\n        got  '%s'\n        want '%s'\n", what,
	       got ? got : "(null)", want ? want : "(null)");
}

static void eq_int(long long got, long long want, const char *what)
{
	checks++;
	if (got == want)
		return;
	failures++;
	printf("  FAIL  %s\n        got  %lld\n        want %lld\n", what,
	       got, want);
}

/* ──────────────────────────────────────────────────────────────────────── */


static void test_colour(void)
{
	printf("libkcolor\n");

	uint32_t v;
	ok(kcol_parse("#39ff14", &v) == 0 && v == 0x39ff14, "parse #rrggbb");
	ok(kcol_parse("39ff14", &v) == 0 && v == 0x39ff14, "parse rrggbb");
	/* CSS shorthand doubles each digit — #39f is #3399ff, not #0039f0. */
	ok(kcol_parse("#39f", &v) == 0 && v == 0x3399ff, "parse #rgb doubles");
	ok(kcol_parse("#zz", &v) < 0, "reject non-hex");

	char hex[8];
	kcol_format(0x0a0b0c, hex);
	eq_str(hex, "0a0b0c", "format keeps leading zeroes");

	/* Round-tripping every scheme colour through HLS must be exact — the
	 * generators depend on it. */
	int rt = 1;
	for (int i = 0; i < kcol_nscheme; i++) {
		const uint32_t f[] = { kcol_schemes[i].primary,
				       kcol_schemes[i].secondary,
				       kcol_schemes[i].urgent,
				       kcol_schemes[i].text };
		for (size_t k = 0; k < sizeof(f) / sizeof(f[0]); k++) {
			double h, l, s;
			kcol_to_hls(f[k], &h, &l, &s);
			if (kcol_from_hls(h, l, s) != f[k])
				rt = 0;
		}
	}
	ok(rt, "HLS round-trip is exact for every scheme colour");

	/* The terminal colours: phosphor's bright slots are the anchors the
	 * derivation was tuned on, byte for byte, and every scheme's ANSI 1-3
	 * and 7 are its own slots, so the Konsole schemes and the terminal
	 * programs' palettes name the accent itself. */
	{
		KcolAnsi a;
		const KcolScheme *ph = kcol_find("phosphor");
		int slots = 1;

		kcol_ansi(ph, &a);
		ok(a.bright[1] == 0xff6b6b && a.bright[2] == 0x7dff5c &&
		   a.bright[3] == 0xffd166 && a.bright[7] == 0xe8ffee,
		   "phosphor's bright ANSI colours are the anchors");
		for (int i = 0; i < kcol_nscheme; i++) {
			const KcolScheme *sc = &kcol_schemes[i];
			kcol_ansi(sc, &a);
			if (a.normal[1] != sc->urgent || a.normal[2] != sc->primary ||
			    a.normal[3] != sc->secondary || a.normal[7] != sc->text ||
			    a.bright[0] != kcol_muted(sc))
				slots = 0;
		}
		ok(slots, "every scheme's ANSI 1-3, 7 and bright black are its "
			  "own slots");
	}

	/*
	 * EVERY SCHEME CLEARS THE SAME TWO FLOORS. A palette is judged by eye
	 * at the bright end and the dark end of this one is compressed enough
	 * that eyes are wrong about it — which is why a scheme added without
	 * this check is a scheme whose labels are a shade of its background.
	 */
	{
		int text_bad = -1, accent_bad = -1;

		for (int i = 0; i < kcol_nscheme; i++) {
			const KcolScheme *sc = &kcol_schemes[i];

			if (text_bad < 0 &&
			    kcol_contrast(sc->text, sc->deep) < 700)
				text_bad = i;
			if (accent_bad < 0 &&
			    kcol_contrast(sc->primary, sc->deep) < 450)
				accent_bad = i;
		}
		ok(text_bad < 0, text_bad < 0
		   ? "every scheme reads 7:1 or better, text on its ground"
		   : kcol_schemes[text_bad < 0 ? 0 : text_bad].name);
		ok(accent_bad < 0, accent_bad < 0
		   ? "and 4.5:1 or better, accent on the same ground"
		   : kcol_schemes[accent_bad < 0 ? 0 : accent_bad].name);

		/*
		 * AND EVERY SCHEME'S FOCUSED PLATE DOES BOTH OF ITS JOBS.
		 *
		 * The plates are measured themselves, not judged from the
		 * GROUND: a test that white never beats black against `deep`
		 * is a proxy and wrong in both directions — it refuses a light
		 * palette whose plates are fine, and it passes a dark one
		 * whose plates are not. The two floors above measure text
		 * against the ground and say nothing about the plates at
		 * all.
		 *
		 * The plate has to STAND OFF THE BAR and CARRY ITS OWN LABEL,
		 * and mixing toward `pdark` buys the first at the second's
		 * expense. Both are measured as composited, because that is
		 * what reaches a screen: a raw plate reads about 0.8 lower and
		 * judging by it is how a working ladder looks broken.
		 *
		 * THE LABEL FLOOR IS 4.5:1 AND NOT 7:1. The solver aims at 7
		 * and stops early when it gets there, but `bone` cannot reach
		 * it — its separation floor binds first and the best any mix
		 * reaches is 6.69:1. Asserting 7 here would assert a fiction;
		 * 4.5 is WCAG AA for normal text and is what every scheme
		 * actually holds.
		 */
		int plate_bad = -1, sep_bad = -1;

		for (int i = 0; i < kcol_nscheme; i++) {
			const KcolScheme *sc = &kcol_schemes[i];
			uint32_t body = kcol_over(
				kcol_mix(sc->deep, sc->variant, 60),
				sc->backdrop, 0xCC);
			uint32_t hov = kcol_over(
				kcol_mix(sc->dim, sc->pdark, 40), body, 0xCC);
			int floor = kcol_contrast(hov, body);
			int best_lab = 0, best_sep = 0;

			if (floor < 200)
				floor = 200;
			for (int x = 68; x >= 10; x--) {
				uint32_t seen = kcol_over(
					kcol_mix(sc->dim, sc->pdark, x), body,
					0xEB);
				int sep = kcol_contrast(seen, body);

				if (sep < floor)
					continue;
				best_lab = kcol_contrast(sc->text, seen);
				best_sep = sep;
				if (best_lab >= 700)
					break;
			}
			if (plate_bad < 0 && best_lab < 450)
				plate_bad = i;
			if (sep_bad < 0 && best_sep < floor)
				sep_bad = i;
		}
		ok(plate_bad < 0, plate_bad < 0
		   ? "and every focused plate carries its label at 4.5:1 or better"
		   : kcol_schemes[plate_bad < 0 ? 0 : plate_bad].name);
		ok(sep_bad < 0, sep_bad < 0
		   ? "and every focused plate stands off the bar it sits on"
		   : kcol_schemes[sep_bad < 0 ? 0 : sep_bad].name);
	}
	eq_int(kcol_nscheme, 8, "and there are eight of them");

	/* Measured against CPython over 8476 colours: black and white are
	 * STRUCTURE and must come back untouched, or a recoloured icon set
	 * turns to mush. */
	const KcolScheme *ph = kcol_find("phosphor");
	ok(ph != NULL, "kcol_find(phosphor)");
	ok(kcol_remap(ph, 0x000000) == 0x000000, "remap leaves pure black");
	ok(kcol_remap(ph, 0xffffff) == 0xffffff, "remap leaves pure white");
	ok(kcol_family(0xff0000) == KCOL_FAM_URGENT, "red is the urgent family");
	ok(kcol_family(0x808080) == KCOL_FAM_NEUTRAL, "grey is neutral");
	ok(kcol_family(0x2277dd) == KCOL_FAM_ACCENT, "blue maps to the accent");

	/* kcol_mix and kcol_mixf are NOT interchangeable, and this is the proof:
	 * halfway between black and white is 0x7f by the integer path (a
	 * truncating divide) and 0x80 by the float one (python's round(), which
	 * sends a half to EVEN). Each generated file was written against exactly
	 * one of them, so swapping them shifts every mixed colour by a unit. */
	ok(kcol_mix(0x000000, 0xffffff, 50) == 0x7f7f7f, "integer mix at 50%");

	/*
	 * kcol_contrast is integer and table-driven because libkcolor may not
	 * link libm. The claim is that it agrees with the real WCAG formula to
	 * the two decimals every threshold in this tree is expressed in, and
	 * the value that matters most is the one nobody would guess: `variant`
	 * against `backdrop` is 1.00:1 — a surface painted in its own background
	 * is the same colour as the ground behind it, so a raised plate needs a
	 * tone of its own.
	 */
	ok(kcol_contrast(0x04120a, 0x02120a) == 100,
	   "phosphor SURFACE vs BG is 1.00:1 — the bar has no body of its own");
	ok(kcol_contrast(0x39ff14, 0x04120a) == 1413, "accent on surface 14.13:1");
	ok(kcol_contrast(0x12401f, 0x04120a) == 162, "dim is a FILL at 1.62:1");
	ok(kcol_contrast(0x1f8f0c, 0x04120a) == 455, "mid is a LABEL at 4.55:1");
	ok(kcol_contrast(0xffffff, 0xffffff) == 100, "a colour against itself");

	/*
	 * kxdg_recent is a jump list's data, read out of freedesktop's
	 * recently-used.xbel by a SCANNER rather than an XML parser — this
	 * tree ships no XML library and one bookmark file is not the reason to
	 * start. The fixture carries the four things that scanner has to get
	 * right, and each is a way a jump list misleads rather than fails:
	 * newest first, a percent-escaped path decoded, a file that has since
	 * been deleted left out, and another application's entries not
	 * offered under this one's name.
	 */
	{
		char got[8][512];
		int n;

		setenv("XDG_DATA_HOME", "testing/fixtures/recent", 1);
		n = kxdg_recent("nvim", got, 8);
		ok(n == 2, "recent: the deleted destination is not offered");
		ok(n == 2 && !strcmp(got[0], "/etc/os-release"),
		   "recent: newest first, and %2D is decoded");
		ok(n == 2 && !strcmp(got[1], "/etc/hostname"),
		   "recent: the older one follows");
		n = kxdg_recent("gimp", got, 8);
		ok(n == 1 && !strcmp(got[0], "/etc/passwd"),
		   "recent: another application's files are its own");
		n = kxdg_recent("no-such-app", got, 8);
		ok(n == 0, "recent: an application with no history has none");
		n = kxdg_recent_all(got, 8);
		ok(n == 3, "recent: every application's, for a Recent list");
		unsetenv("XDG_DATA_HOME");
	}

	/*
	 * The write half, into a scratch store. Every open on this desktop
	 * passes through `kdos-appbox open`, which calls this — so the two
	 * things asserted are the two that make a recent list wrong rather
	 * than empty: a file opened twice is ONE entry, and it is the newest.
	 */
	{
		char got[8][512], dir[512], store[600];
		char *cwd = getcwd(NULL, 0);
		int n;

		snprintf(dir, sizeof(dir), "%s/recent-write",
			 getenv("KDOS_SELFTEST_OUT") ? getenv("KDOS_SELFTEST_OUT")
						    : "/tmp");
		kb_mkdir_p(dir);
		snprintf(store, sizeof(store), "%s/recently-used.xbel", dir);
		unlink(store);
		setenv("XDG_DATA_HOME", dir, 1);

		eq_int(kxdg_recent_add("ed", "/etc/hostname", NULL), 0,
		       "recent: a first open creates the store");
		eq_int(kxdg_recent_add("ed", "/etc/os-release", NULL), 0,
		       "recent: and a second appends to it");
		n = kxdg_recent("ed", got, 8);
		ok(n == 2 && !strcmp(got[0], "/etc/os-release"),
		   "recent: newest first, read back through the same scanner");

		eq_int(kxdg_recent_add("ed", "/etc/hostname", NULL), 0,
		       "recent: opening one of them again");
		n = kxdg_recent("ed", got, 8);
		ok(n == 2, "recent: is still two entries, not three");
		ok(n == 2 && !strcmp(got[0], "/etc/hostname"),
		   "recent: and the one just opened leads");

		eq_int(kxdg_recent_add("ed", "etc/hostname", NULL), -1,
		       "recent: a relative path is refused — it means nothing "
		       "to whatever reads the store next");

		unlink(store);
		unsetenv("XDG_DATA_HOME");
		free(cwd);
	}

	/*
	 * WHAT AN ARGUMENT IS. Two openers ask this one function, so
	 * what it answers for a URL is what the whole desktop does
	 * with a link. The glob half is the host's own table and is
	 * not asserted here; the scheme half depends on nothing.
	 */
	{
		char m[128];
		const char *p;

		p = kxdg_mime_for_arg("mailto:ada@example.com", m,
				      sizeof(m));
		eq_str(m, "x-scheme-handler/mailto",
		       "arg: a mail address is a scheme, not a file");
		ok(!strcmp(p, "mailto:ada@example.com"),
		   "arg: and it is passed on unchanged");

		/* The case this function exists for: the basename
		 * `a@b.c` matches the `*.c` glob, so a mail address
		 * looked up as a file name resolves to C++ source. */
		kxdg_mime_for_arg("mailto:a@b.c", m, sizeof(m));
		eq_str(m, "x-scheme-handler/mailto",
		       "arg: even when the basename matches a glob");

		kxdg_mime_for_arg("https://example.com", m, sizeof(m));
		eq_str(m, "x-scheme-handler/https",
		       "arg: a bare host is still a scheme");
		kxdg_mime_for_arg("HTTPS://EXAMPLE.COM", m, sizeof(m));
		eq_str(m, "x-scheme-handler/https",
		       "arg: a scheme is case-insensitive");
		kxdg_mime_for_arg("webcal://x/y.ics", m, sizeof(m));
		eq_str(m, "x-scheme-handler/webcal",
		       "arg: and the suffix does not win over it");

		/* file: names a path, and the caller must be handed
		 * the path rather than the URL. */
		p = kxdg_mime_for_arg("file:///etc/hostname", m,
				      sizeof(m));
		eq_str(p, "/etc/hostname",
		       "arg: file:// is unwrapped to its path");
		p = kxdg_mime_for_arg("file://localhost/etc/hostname",
				      m, sizeof(m));
		eq_str(p, "/etc/hostname",
		       "arg: and so is one carrying an authority");
		p = kxdg_mime_for_arg("file:/etc/hostname", m,
				      sizeof(m));
		eq_str(p, "/etc/hostname",
		       "arg: and one carrying none");

		/* A NAME THAT EXISTS IS A PATH. /etc/hostname has no
		 * colon, so the case that matters is that a plain
		 * path is never read as a scheme. */
		p = kxdg_mime_for_arg("/etc/hostname", m, sizeof(m));
		ok(!strcmp(p, "/etc/hostname") &&
		   strncmp(m, "x-scheme-handler/", 17) != 0,
		   "arg: a path is a path");
		kxdg_mime_for_arg("notes.txt", m, sizeof(m));
		ok(strncmp(m, "x-scheme-handler/", 17) != 0,
		   "arg: a relative name with a dot is not a scheme");
		kxdg_mime_for_arg("", m, sizeof(m));
		eq_str(m, "application/octet-stream",
		       "arg: nothing is not a scheme either");
	}

	/*
	 * kxdg_handlers is the opener's resolver AND the Settings Apps page's,
	 * so the layer it reports is what that page prints. A private type no
	 * host table can claim keeps /etc/xdg out of the answer; what is left
	 * is the user's list, an uninstalled id falling through, the cache, and
	 * the user's default ending the search.
	 */
	{
		char dir[] = "/tmp/kdos-handlers.XXXXXX";
		const char *mt = "application/x-kdos-selftest";
		KxdgHandler h[8];
		char p[700];
		int n, dflt = -1;

		ok(mkdtemp(dir) != NULL, "handlers: scratch directory");
		snprintf(p, sizeof(p), "%s/data/applications", dir);
		kb_mkdir_p(p);
		snprintf(p, sizeof(p), "%s/cfg", dir);
		kb_mkdir_p(p);
		snprintf(p, sizeof(p), "%s/data/applications/a.desktop", dir);
		kb_write_file(p, "[Desktop Entry]\nType=Application\nExec=a\n");
		snprintf(p, sizeof(p), "%s/data/applications/b.desktop", dir);
		kb_write_file(p, "[Desktop Entry]\nType=Application\nExec=b\n");
		snprintf(p, sizeof(p), "%s/data/applications/mimeinfo.cache",
			 dir);
		kb_write_file(p, "[MIME Cache]\n"
			      "application/x-kdos-selftest=a.desktop;b.desktop;\n");
		snprintf(p, sizeof(p), "%s/data", dir);
		setenv("XDG_DATA_HOME", p, 1);
		setenv("XDG_DATA_DIRS", p, 1);
		snprintf(p, sizeof(p), "%s/cfg", dir);
		setenv("XDG_CONFIG_HOME", p, 1);
		unsetenv("XDG_CURRENT_DESKTOP");

		n = kxdg_handlers(mt, h, 8, &dflt);
		eq_int(n, 2, "handlers: the cache names two");
		ok(n == 2 && !strcmp(h[0].id, "a.desktop") &&
		   h[0].layer == KXDG_LAYER_CACHE && dflt == 0,
		   "handlers: an installed claim is not a default");

		snprintf(p, sizeof(p), "%s/cfg/mimeapps.list", dir);
		kb_write_file(p, "[Default Applications]\n"
			      "application/x-kdos-selftest=gone.desktop;"
			      "b.desktop\n");
		n = kxdg_handlers(mt, h, 8, &dflt);
		ok(n == 1 && !strcmp(h[0].id, "b.desktop") &&
		   h[0].layer == KXDG_LAYER_USER && dflt == 1,
		   "handlers: the user's default, past an uninstalled id, "
		   "ends the search");
		eq_str(kxdg_layer_name(h[0].layer), "your default",
		       "handlers: the layer as the page prints it");
		eq_int(kxdg_handlers("application/x-kdos-none", h, 8, NULL), 0,
		       "handlers: an unclaimed type has none");

		/* The shipped layer is every $XDG_CONFIG_DIRS directory in
		 * order, each one's desktop-named list ahead of its plain one:
		 * the administrator's /etc/xdg row beats KDOS's
		 * /usr/share/kdos/xdg row, and KDOS's still answers when
		 * /etc/xdg is silent. */
		snprintf(p, sizeof(p), "%s/cfg/mimeapps.list", dir);
		unlink(p);
		snprintf(p, sizeof(p), "%s/etc", dir);
		kb_mkdir_p(p);
		snprintf(p, sizeof(p), "%s/kdos", dir);
		kb_mkdir_p(p);
		snprintf(p, sizeof(p), "%s/kdos/kde-mimeapps.list", dir);
		kb_write_file(p, "[Default Applications]\n"
			      "application/x-kdos-selftest=a.desktop\n");
		snprintf(p, sizeof(p), "%s/etc:%s/kdos", dir, dir);
		setenv("XDG_CONFIG_DIRS", p, 1);
		setenv("XDG_CURRENT_DESKTOP", "KDE", 1);
		n = kxdg_handlers(mt, h, 8, &dflt);
		ok(n >= 1 && !strcmp(h[0].id, "a.desktop") &&
		   h[0].layer == KXDG_LAYER_SYSTEM && dflt == 1,
		   "handlers: a later config dir's desktop list is a default");
		snprintf(p, sizeof(p), "%s/etc/mimeapps.list", dir);
		kb_write_file(p, "[Default Applications]\n"
			      "application/x-kdos-selftest=b.desktop\n");
		n = kxdg_handlers(mt, h, 8, &dflt);
		ok(n == 2 && !strcmp(h[0].id, "b.desktop") &&
		   !strcmp(h[1].id, "a.desktop") && dflt == 1,
		   "handlers: an earlier config dir wins, desktop list or not");
		unsetenv("XDG_CURRENT_DESKTOP");
		n = kxdg_handlers(mt, h, 8, &dflt);
		ok(n == 2 && !strcmp(h[0].id, "b.desktop") &&
		   h[1].layer == KXDG_LAYER_CACHE,
		   "handlers: with no desktop named, its list is not read");
		unsetenv("XDG_CONFIG_DIRS");

		unsetenv("XDG_DATA_HOME");
		unsetenv("XDG_DATA_DIRS");
		unsetenv("XDG_CONFIG_HOME");
		kb_rmtree(dir);
	}

	/*
	 * MD5 AND THE URI, together, because the pair is the thumbnail cache's
	 * FILE NAME and both halves have to match what every other program on
	 * the machine computes. The vectors are RFC 1321's; the URI cases are
	 * the two that a hand-rolled escaper gets wrong — a space, and a
	 * character glib deliberately leaves alone.
	 */
	{
		char h[33], uri[512];

		kb_md5_str("", h);
		eq_str(h, "d41d8cd98f00b204e9800998ecf8427e", "md5 of nothing");
		kb_md5_str("abc", h);
		eq_str(h, "900150983cd24fb0d6963f7d28e17f72", "md5 of abc");
		kb_md5_str("The quick brown fox jumps over the lazy dog", h);
		eq_str(h, "9e107d9d372bb6826bd81d3542a419d6",
		       "md5 of a sentence — the block boundary at 56 bytes");
		kb_md5_str("12345678901234567890123456789012345678901234567890"
			   "123456789012345678901234567890", h);
		eq_str(h, "57edf4a22be3c955ac49da2e2107b67a",
		       "md5 of eighty digits — two blocks and a pad");

		kb_uri_file("/home/kdos/a b.txt", uri, sizeof(uri));
		eq_str(uri, "file:///home/kdos/a%20b.txt",
		       "uri: a space is escaped, and the slashes are not");
		/* The sub-delimiters glib deliberately leaves ALONE in a path.
		 * Escaping any of them would be a name nothing else computes. */
		kb_uri_file("/tmp/it's (1)+2,3;4=5@6.png", uri, sizeof(uri));
		eq_str(uri, "file:///tmp/it's%20(1)+2,3;4=5@6.png",
		       "uri: the sub-delimiters a path may carry are left alone");
		/*
		 * THE PAIR, AS A CACHE NAME. This is the whole interop
		 * claim — the same URI hashed the same way as every other
		 * program that writes into the shared cache. The value is
		 * `md5sum` of the exact string, checked against it.
		 */
		kb_uri_file("/home/jens/photo/me.png", uri, sizeof(uri));
		kb_md5_str(uri, h);
		eq_str(h, "d40775e596682f2a16d1b834c221c0a2",
		       "the thumbnail cache name is the md5 of the escaped URI");

		kb_uri_file("/tmp/a#b?c%d[e]", uri, sizeof(uri));
		eq_str(uri, "file:///tmp/a%23b%3Fc%25d%5Be%5D",
		       "uri: and the ones that would end it are not — in "
		       "UPPERCASE hex, because the hash is over the string");
	}

	ok(kcol_mixf(0x000000, 0xffffff, 0.5) == 0x808080, "float mix at 0.5");
	ok(kcol_mix(0x000000, 0xffffff, 50) != kcol_mixf(0x000000, 0xffffff, 0.5),
	   "the two mixes genuinely disagree");

	/* The hex-token scanner: python's #([0-9a-f]{6}|[0-9a-f]{3})\b, whose
	 * trailing word boundary is what keeps an 8-digit RGBA intact. */
	size_t n = 0;
	char *out = kcol_retint_text("x #39ff14ff y", 13, ph, &n);
	eq_str(out, "x #39ff14ff y", "8 hex digits are left alone");
	free(out);
	out = kcol_retint_text("#000000", 7, ph, &n);
	eq_str(out, "#000000", "declined colours keep their spelling");
	free(out);

	/*
	 * kcol_muted is the text colour for secondary labels: `dim` is a fill
	 * and lands around 1.6:1 against `deep`, which is below any legibility
	 * floor. The claim is not "muted is a colour" — it is that muted sits
	 * strictly between the background and the text, and is farther from the
	 * background than dim is by a wide enough margin to be a different
	 * decision rather than a nudge. Checked for every scheme, because a
	 * muted that reads only in phosphor is illegible in every other scheme.
	 *
	 * STATED AS DISTANCE FROM THE GROUND, not as a luma ORDER. A light
	 * scheme runs the other way — its text is darker than its background —
	 * and an ordering test passes or fails on which direction the palette
	 * happens to go rather than on whether the tones separate.
	 */
	int muted_ok = 1, muted_far = 1, muted_reads = 1;
	for (int i = 0; i < kcol_nscheme; i++) {
		const KcolScheme *sc = &kcol_schemes[i];
		unsigned m = kcol_muted(sc);
		if (kcol_contrast(m, sc->deep) < 450)
			muted_reads = 0;
		double ld = kcol_luma(sc->deep);
		double dm = kcol_luma(m) - ld;
		double dd = kcol_luma(sc->dim) - ld;
		double dt = kcol_luma(sc->text) - ld;

		if (dm < 0) {		/* a light ground: measure the other way */
			dm = -dm;
			dd = -dd;
			dt = -dt;
		}
		if (m == sc->dim || m == sc->text || m == sc->deep)
			muted_ok = 0;
		if (!(dd > 0 && dd < dm && dm < dt))
			muted_ok = 0;
		/* Measured: 2.2x in bone, 2.7x in phosphor. */
		if (dm <= 2.0 * dd)
			muted_far = 0;
	}
	ok(muted_ok, "muted is its own colour, between deep and text, in every scheme");
	ok(muted_far, "muted is more than twice as far from the background as dim");
	ok(muted_reads, "muted reads 4.5:1 or better against deep, in every scheme");
}

/* ──────────────────────────────────────────────────────────────────────── */

static void test_base(void)
{
	printf("libkbase\n");

	char buf[8];
	kb_strlcpy(buf, "abcdefghij", sizeof(buf));
	eq_str(buf, "abcdefg", "strlcpy truncates and terminates");
	kb_strlcpy(buf, "", sizeof(buf));
	eq_str(buf, "", "strlcpy handles empty");

	ok(kb_str_ieq("KDOS", "kdos"), "case-insensitive compare");
	ok(!kb_str_ieq("kdos", "kdosx"), "compare rejects a prefix");
	eq_str(kb_basename("/a/b/c"), "c", "basename");
	eq_str(kb_basename("bare"), "bare", "basename of a bare name");

	eq_str(kb_human_size(0), "0B", "human_size zero");
	eq_str(kb_human_size(1024), "1.0K", "human_size exact K");
	eq_str(kb_human_size(1536), "1.5K", "human_size fractional");

	/*
	 * ONE MATCHER FOR THREE SURFACES, so the palette, the launcher and the
	 * Start menu cannot rank one query three ways. What is asserted is the
	 * LADDER rather than any particular number: the constants may be tuned,
	 * and these orderings may not change without somebody meaning it.
	 */
	ok(kb_fuzzy("System Monitor", "xyz") == 0, "fuzzy: no match is zero");
	ok(kb_fuzzy("System Monitor", "") > 0,
	   "fuzzy: an empty query matches, or a list empties before the first key");
	/* The whole reason this is a subsequence and not a substring. */
	ok(kb_fuzzy("System Monitor", "sm") > 0, "fuzzy: an acronym matches at all");
	ok(kb_fuzzy("System Monitor", "sm") > kb_fuzzy("Assembler", "sm"),
	   "fuzzy: an acronym beats the same letters scattered mid-word");
	ok(kb_fuzzy("Terminal", "term") > kb_fuzzy("KDOS Terminal", "term"),
	   "fuzzy: a prefix beats the same word further in");
	ok(kb_fuzzy("abcdef", "abc") > kb_fuzzy("axbxcx", "abc"),
	   "fuzzy: a run beats a scatter");
	ok(kb_fuzzy("System Monitor", "SM") == kb_fuzzy("System Monitor", "sm"),
	   "fuzzy: case-insensitive");
	/* camelCase is a word boundary, or every application id is one word. */
	ok(kb_fuzzy("SystemMonitor", "sm") > kb_fuzzy("Assembler", "sm"),
	   "fuzzy: a capital inside a word starts one");

	/* kb_buf_printf must never truncate: a fixed stack buffer here once cut
	 * a generated btop theme in half, and the half-file looked plausible. */
	KbBuf b = {0};
	for (int i = 0; i < 200; i++)
		kb_buf_printf(&b, "%s", "0123456789");
	ok(b.n == 2000, "buf_printf grows past its scratch buffer");
	kb_buf_free(&b);

	KbBuf big = {0};
	char huge[4096];
	memset(huge, 'x', sizeof(huge) - 1);
	huge[sizeof(huge) - 1] = 0;
	kb_buf_printf(&big, "%s", huge);
	ok(big.n == 4095, "buf_printf handles an over-long single write");
	kb_buf_free(&big);

	/* JSON escaping. Every --json output funnels through this, and the
	 * inputs are package descriptions, file paths and window titles — all
	 * of which can carry a quote or a control character. Getting this wrong
	 * emits something that looks like JSON and does not parse. */
	KbBuf j = {0};
	kb_json_str(&j, "plain");
	eq_str(j.p, "\"plain\"", "json plain");
	j.n = 0;
	kb_json_str(&j, "say \"hi\"");
	eq_str(j.p, "\"say \\\"hi\\\"\"", "json escapes quotes");
	j.n = 0;
	kb_json_str(&j, "back\\slash");
	eq_str(j.p, "\"back\\\\slash\"", "json escapes backslash");
	j.n = 0;
	kb_json_str(&j, "a\nb\tc");
	eq_str(j.p, "\"a\\nb\\tc\"", "json escapes newline and tab");
	j.n = 0;
	kb_json_str(&j, "bell\x07");
	eq_str(j.p, "\"bell\\u0007\"", "json escapes other controls as \\u");
	j.n = 0;
	/* UTF-8 passes through: JSON strings are Unicode and the input is
	 * already UTF-8, so escaping high bytes would double-encode them. */
	kb_json_str(&j, "caf\xc3\xa9");
	eq_str(j.p, "\"caf\xc3\xa9\"", "json passes UTF-8 through");
	j.n = 0;
	kb_json_str(&j, NULL);
	eq_str(j.p, "\"\"", "json NULL is an empty string");
	kb_buf_free(&j);

	/* A scratch tree for the filesystem helpers. Every component is made,
	 * because a caller handed a path two levels deep gets one mkdir and a
	 * missing parent otherwise. */
	char dir[] = "/tmp/kdos-selftest.XXXXXX";
	ok(mkdtemp(dir) != NULL, "scratch directory");

	char *nested = kb_path_join(dir, "a/b");
	ok(kb_mkdir_p(nested) == 0, "mkdir_p makes every component");
	ok(kb_is_dir(nested), "and the deepest one is there");
	char *leaf = kb_path_join(nested, "f");
	ok(kb_write_file(leaf, "x") == 0, "write a file under it");
	ok(kb_path_exists(leaf), "and it is on disk");
	free(leaf);
	free(nested);

	/* Output bigger than the buffer must TRUNCATE, not hang. A child that
	 * inherits the pipe's read end keeps the pipe writable after ours is
	 * closed, and then blocks in write() while we block in waitpid() — a
	 * 1.1 MB listing such as `kpkg install zig` prints is enough. */
	KbArgv gen = {0};
	kb_argv_add(&gen, "seq");
	kb_argv_add(&gen, "100000");
	kb_argv_end(&gen);
	char small[64];
	kb_run_capture(&gen, small, sizeof(small));
	ok(strlen(small) >= sizeof(small) - 2, "capture truncates at the ceiling");

	KbBuf all = {0};
	ok(kb_run_capture_buf(&gen, &all) == 0, "unbounded capture succeeds");
	ok(all.n > (1 << 19), "unbounded capture keeps every byte");
	eq_str(all.p + all.n - 7, "100000\n", "and the last line is intact");
	kb_buf_free(&all);

	kb_rmtree(dir);
	ok(!kb_path_exists(dir), "rmtree removes the tree");

	/* Landlock. A ruleset is only BUILT here, never enforced — enforcing is
	 * irreversible and would sandbox the rest of this process. */
	int abi = kb_landlock_abi();
	ok(abi > 0 || abi == -ENOSYS || abi == -EOPNOTSUPP,
	   "landlock abi probe answers a version or a known errno");

	if (abi > 0) {
		KbLandlock ll;
		ok(kb_landlock_new(&ll, 0) == 0, "ruleset builds for this ABI");

		/* A path_beneath rule on a NON-DIRECTORY is
		 * EINVAL if the mask names directory-only accesses. /dev/null
		 * has to take a write rule, or every sandboxed program dies on
		 * its first `>/dev/null` and looks broken rather than confined. */
		ok(kb_landlock_allow(&ll, "/dev/null", 1) == 0,
		   "a write rule on a non-directory is accepted");
		ok(kb_landlock_allow(&ll, "/usr", 0) == 0,
		   "a read rule on a directory is accepted");
		ok(kb_landlock_allow(&ll, "/nonexistent-kdos-selftest", 0) == -ENOENT,
		   "a missing path reports ENOENT rather than being ignored");
		/* Never enforced: enforcement is irreversible and would sandbox
		 * the rest of this process. The ruleset fd is ours to close. */
		close(ll.fd);
	}
}

/* ──────────────────────────────────────────────────────────────────────── */

/*
 * The freedesktop trash. It is libkbase's, and it is the spec's, because
 * `kdos trash` at a prompt and Dolphin's Delete key have to mean the same
 * thing, and every assertion below is a way the two could quietly come apart.
 */
static void test_trash(void)
{
	char dir[] = "/tmp/kdos-selftest-trash.XXXXXX";
	char oldhome[512] = "", oldcwd[512] = "";
	char work[200], a[240], b[240], p[400];
	const char *home = getenv("HOME");
	KbTrashItem *v;
	char to[KB_TRASH_PATH];
	int n;

	printf("libkbase trash\n");
	if (home)
		kb_strlcpy(oldhome, home, sizeof(oldhome));
	if (!getcwd(oldcwd, sizeof(oldcwd)))
		oldcwd[0] = '\0';
	ok(mkdtemp(dir) != NULL, "scratch directory");
	setenv("HOME", dir, 1);

	snprintf(work, sizeof(work), "%s/w", dir);
	snprintf(a, sizeof(a), "%s/a", work);
	snprintf(b, sizeof(b), "%s/b", work);
	kb_mkdir_p(a);
	kb_mkdir_p(b);

	/*
	 * A STATE FILE IS REPLACED, NEVER TRUNCATED. `kb_write_file` opens
	 * O_TRUNC and then writes, so the file is zero bytes in between and any
	 * failure there leaves it that way — measured as a box profile found
	 * empty, which loses `base` and makes the box unstartable. The atomic
	 * writer must land the whole content, keep 0644, shrink cleanly when
	 * the new content is shorter, and leave no temp file behind.
	 */
	{
		struct stat st;
		char rd[64];
		snprintf(p, sizeof(p), "%s/state.conf", work);
		ok(kb_write_file_atomic(p, "base=pack:kdos\nimage=x\n") == 0,
		   "atomic write lands");
		ok(stat(p, &st) == 0 && st.st_size == 23,
		   "the whole content is there");
		ok((st.st_mode & 0777) == 0644, "and it is 0644");
		ok(kb_write_file_atomic(p, "x\n") == 0, "replace with less");
		ok(stat(p, &st) == 0 && st.st_size == 2,
		   "no tail of the old content survives");
		ok(kb_read_file(p, rd, sizeof(rd)) == 2, "and it reads back");
		{
			/* the temp file is named <path>.tmpXXXXXX */
			char g[512];
			snprintf(g, sizeof(g), "%s.tmp", p);
			ok(!kb_path_exists(g), "no temp file left behind");
		}
		unlink(p);
	}

	/*
	 * NIGHT LIGHT IS A TRANSFORM, and the scheme has to survive it. Eight
	 * bits do not divide back: a blue scaled down and up again is not the
	 * blue it started as, so turning the toggle off must return to the
	 * table rather than undo the arithmetic — which is the whole reason
	 * the chosen scheme is kept beside the warmed copy.
	 */
	{
		KtuiTheme cold;

		ktui_theme_set(ktui_themes[KCOL_DEFAULT_INDEX].name);
		cold = *ktui_theme;

		ok(ktui_theme_night(1) == 1, "night light says it changed");
		ok(ktui_theme_night(1) == 0, "and setting it again does not");

		int warmer = 0, cooler = 0;

		for (int i = 0; i < KT_NCOLOR; i++) {
			if (ktui_theme->slot[i].r != cold.slot[i].r)
				cooler = 1;	/* red must not move at all */
			if (ktui_theme->slot[i].b < cold.slot[i].b)
				warmer = 1;
		}
		ok(!cooler, "red is untouched, so the accent still reads");
		ok(warmer, "and blue comes down");

		ok(ktui_theme_night(0) == 1, "turning it off changes it back");
		ok(!memcmp(ktui_theme->slot, cold.slot, sizeof(cold.slot)),
		   "back to the scheme's own bytes, not the arithmetic undone");
	}

	/*
	 * ONE LOADER FOR THE ACCENT. A TUI loads its palette through
	 * ktui_theme_load(), so the accent `kdos theme` recorded in
	 * $XDG_CONFIG_HOME/kdos/accent is the one it draws, and a missing file
	 * is the default scheme rather than whatever the last caller set.
	 */
	{
		char cdir[512], tf[600];
		const KtuiTheme *other =
			&ktui_themes[(KCOL_DEFAULT_INDEX + 1) % ktui_ntheme];
		const KtuiTheme *dflt = &ktui_themes[KCOL_DEFAULT_INDEX];

		snprintf(cdir, sizeof(cdir), "%s/load-config", work);
		setenv("XDG_CONFIG_HOME", cdir, 1);
		snprintf(tf, sizeof(tf), "%s/kdos", cdir);
		kb_mkdir_p(tf);
		snprintf(tf, sizeof(tf), "%s/kdos/accent", cdir);
		kb_write_file(tf, other->name);

		ktui_theme_load();
		ok(ktui_theme == other, "the recorded accent is the one loaded");

		unlink(tf);
		ktui_theme_load();
		ok(ktui_theme == dflt, "a missing accent file is the default scheme");

		unsetenv("XDG_CONFIG_HOME");
	}

	/*
	 * WHICH DESKTOP, for the `<desktop>-mimeapps.list` searched ahead of the
	 * plain one. The FIRST name only and lowercased: the variable is a
	 * preference ORDER, and the file the spec asks for is spelled in lower
	 * case.
	 */
	{
		char pre[80];

		setenv("XDG_CURRENT_DESKTOP", "KDE:Plasma", 1);
		ok(kb_desktop_prefix(pre, sizeof(pre)) == 1 &&
		   !strcmp(pre, "kde"),
		   "the desktop prefix is the first name, lowercased");
		setenv("XDG_CURRENT_DESKTOP", "KDE", 1);
		ok(kb_desktop_prefix(pre, sizeof(pre)) == 1 &&
		   !strcmp(pre, "kde"),
		   "and a single name is taken whole");
		setenv("XDG_CURRENT_DESKTOP", "", 1);
		ok(kb_desktop_prefix(pre, sizeof(pre)) == 0 && pre[0] == '\0',
		   "an empty variable names no desktop rather than an empty one");
		unsetenv("XDG_CURRENT_DESKTOP");
		ok(kb_desktop_prefix(pre, sizeof(pre)) == 0,
		   "and neither does an unset one");
	}

	/*
	 * AND WHICH TERMINAL A `Terminal=true` ENTRY RUNS IN. `konsole` needs
	 * a Plasma session, so a login with none that wrapped an entry in it
	 * would resolve the right program and then fail to open a window for
	 * it — which reads as the handler being wrong.
	 */
	{
		setenv("WAYLAND_DISPLAY", "wayland-0", 1);
		eq_str(kb_terminal(), "konsole",
		       "a Plasma session runs a terminal entry in konsole");
		/*
		 * AND A BARE VIRTUAL TERMINAL HAS NONE. `Ctrl+Alt+F2` is a
		 * login with no session, so there is no emulator to open — and
		 * a name answered here would wrap a terminal program in a
		 * window that cannot start.
		 */
		unsetenv("WAYLAND_DISPLAY");
		ok(kb_terminal() == NULL,
		   "and a login no session started has no terminal to open");
	}

	/*
	 * TWO FILES OF THE SAME NAME FROM DIFFERENT DIRECTORIES is the ordinary
	 * case, and the second silently replacing the first is data loss.
	 */
	snprintf(p, sizeof(p), "%s/notes.txt", a);
	kb_write_file(p, "one\n");
	ok(kb_trash_put(p) == 0, "trash the first notes.txt");
	snprintf(p, sizeof(p), "%s/notes.txt", b);
	kb_write_file(p, "two\n");
	ok(kb_trash_put(p) == 0, "trash the second notes.txt");

	n = kb_trash_list(&v);
	ok(n == 2, "both are in the trash");
	free(v);

	/* And each goes back where IT came from, which is the whole reason the
	 * record exists. */
	ok(kb_trash_restore("notes.txt", to, sizeof(to)) == 0, "restore the first");
	snprintf(p, sizeof(p), "%s/notes.txt", a);
	eq_str(to, p, "the first went back to a/");
	ok(kb_trash_restore("notes.txt.1", to, sizeof(to)) == 0, "restore the second");
	snprintf(p, sizeof(p), "%s/notes.txt", b);
	eq_str(to, p, "the second went back to b/");

	/* A name with a space and a percent in it. The Path= value is a URI, so
	 * it is escaped on the way in and unescaped on the way out; getting one
	 * half wrong restores to a path with `%20` in its name. */
	snprintf(p, sizeof(p), "%s/od%%d file.txt", work);
	kb_write_file(p, "x\n");
	ok(kb_trash_put(p) == 0, "trash an awkward name");
	ok(kb_trash_restore("od%d file.txt", to, sizeof(to)) == 0,
	   "restore an awkward name");
	eq_str(to, p, "the escape round-trips");

	/* Restoring onto something that is there NOW would destroy it. */
	ok(kb_trash_put(p) == 0, "trash it again");
	kb_write_file(p, "in the way\n");
	ok(kb_trash_restore("od%d file.txt", to, sizeof(to)) == -1 &&
	   errno == EEXIST, "restore refuses to overwrite");

	/* Trashing the trash empties it in the worst possible order. */
	{
		char files[KB_TRASH_PATH], info[KB_TRASH_PATH];
		kb_trash_dirs(files, sizeof(files), info, sizeof(info));
		ok(kb_trash_put(files) == -1, "the trash cannot be trashed");
	}

	/* A DIRECTORY comes back too, and empty takes both halves of every
	 * record: an info/ entry outliving its file is the state every other
	 * implementation ignores. */
	snprintf(p, sizeof(p), "%s/adir", work);
	kb_mkdir_p(p);
	snprintf(a, sizeof(a), "%s/adir/f", work);
	kb_write_file(a, "y\n");
	ok(kb_trash_put(p) == 0, "trash a directory");
	ok(kb_trash_empty() >= 1, "empty removes it");
	n = kb_trash_list(&v);
	ok(n == 0, "the trash is empty");
	free(v);
	{
		char files[KB_TRASH_PATH], info[KB_TRASH_PATH];
		int left = 0;
		char **nm;
		kb_trash_dirs(files, sizeof(files), info, sizeof(info));
		nm = kb_listdir(info, &left);
		ok(left == 0, "and so is info/");
		kb_strv_free(nm);
	}

	if (oldhome[0])
		setenv("HOME", oldhome, 1);
	else
		unsetenv("HOME");
	/* The chdir back is best effort: the scratch directory is going next
	 * line either way, and a failure here must not fail the suite. */
	if (oldcwd[0]) {
		int rc = chdir(oldcwd);
		(void)rc;
	}
	kb_rmtree(dir);
}

/* ──────────────────────────────────────────────────────────────────────── */

static int dies(void (*fn)(const KpConf *), const KpConf *c);

/* What every caller of kp_depends does with a port: the whole closure, or
 * the process ends naming the recipe. */
static void depends_wide(const KpConf *c)
{
	char *d = kp_port_dir(c, "wide");
	char (*deps)[128] = malloc(sizeof(char[KP_MAX_DEPS][128]));
	if (d && deps)
		kp_depends(d, deps, KP_MAX_DEPS);
	free(deps);
	free(d);
}

static void test_pkg(void)
{
	printf("libkpkg\n");

	char dir[] = "/tmp/kdos-selftest-pkg.XXXXXX";
	ok(mkdtemp(dir) != NULL, "scratch directory");

	char *repo = kb_path_join(dir, "repo");
	const char *ports[][2] = {
		{ "a", "depends = b c\n\nname = a\nversion = 1\nrelease = 1\n" },
		{ "b", "depends = c\n\nname = b\nversion = 1\nrelease = 1\n" },
		{ "c", "depends =\n\nname = c\nversion = 1\nrelease = 1\n" },
		/* A cycle. The shell solver terminated on these silently and so
		 * must this one — marking a name visited BEFORE walking it is
		 * the reason. */
		{ "x", "depends = y\n\nname = x\nversion = 1\nrelease = 1\n" },
		{ "y", "depends = x\n\nname = y\nversion = 1\nrelease = 1\n" },
		{ NULL, NULL }
	};
	for (int i = 0; ports[i][0]; i++) {
		char *pd = kb_path_join(repo, ports[i][0]);
		kb_mkdir_p(pd);
		char *rp = kb_path_join(pd, "kpkgbuild");
		kb_write_file(rp, ports[i][1]);
		free(rp);
		free(pd);
	}

	setenv("PORT_REPO", repo, 1);
	setenv("KPKG_CONF", "/nonexistent", 1);
	setenv("PKGDB_DIR", "/dev/null", 1);

	KpConf c;
	kp_conf_load(&c);
	ok(c.nrepos == 1, "PORT_REPO from the environment");

	/* The /dev/null idiom: three callers ask for an empty database this
	 * way, and it works only because /dev/null/<name> cannot be a file. */
	ok(!kp_installed(&c, "a"), "PKGDB_DIR=/dev/null reads as empty");

	char *names[] = { (char *)"a" };
	KpOrder o;
	kp_resolve(&c, names, 1, &o);
	ok(o.n == 3, "a pulls in three packages");
	eq_str(o.order[0], "c", "deepest dependency first");
	eq_str(o.order[o.n - 1], "a", "the named package last");

	char *cyc[] = { (char *)"x" };
	kp_resolve(&c, cyc, 1, &o);
	ok(o.n == 2, "a dependency cycle terminates");

	char *pd = kp_port_dir(&c, "a");
	ok(pd != NULL, "port lookup");
	char deps[KP_MAX_DEPS][128];
	int nd = kp_depends(pd, deps, KP_MAX_DEPS);
	ok(nd == 2, "two dependencies parsed");
	eq_str(deps[0], "b", "first dependency");
	free(pd);

	/* THE LIMIT IS A REFUSAL, NOT A CUT. A recipe naming exactly
	 * KP_MAX_DEPS reads whole; one more is an error naming the port and
	 * the count, and nothing a caller could take for the closure. The
	 * port is reached through a trailing slash, the spelling a caller
	 * building in a recipe's own directory hands over. */
	for (int extra = 0; extra <= 1; extra++) {
		KbBuf r = {0};
		kb_buf_str(&r, "depends =");
		for (int i = 0; i < KP_MAX_DEPS + extra; i++)
			kb_buf_printf(&r, " d%03d", i);
		kb_buf_str(&r, "\n\nname = wide\nversion = 1\nrelease = 1\n");
		char *wd = kb_path_join(repo, extra ? "wide" : "full");
		kb_mkdir_p(wd);
		char *wr = kb_path_join(wd, "kpkgbuild");
		kb_write_file(wr, r.p);
		free(wr);
		kb_buf_free(&r);

		char slash[600], werr[512] = "";
		snprintf(slash, sizeof(slash), "%s/", wd);
		int wn = kp_depends_scan(slash, deps, KP_MAX_DEPS, werr,
					 sizeof(werr));
		if (!extra) {
			ok(wn == KP_MAX_DEPS, "a recipe at the limit reads whole");
			eq_str(deps[KP_MAX_DEPS - 1], "d127",
			       "and its last name is kept");
		} else {
			char want[128];
			snprintf(want, sizeof(want),
				 "wide: depends names %d ports; the limit is %d",
				 KP_MAX_DEPS + 1, KP_MAX_DEPS);
			ok(wn == -1, "a recipe past the limit is refused");
			eq_str(werr, want, "and the refusal names the port and count");
		}
		free(wd);
	}
	ok(dies(depends_wide, &c), "kp_depends ends the process on that recipe");

	pd = kp_port_dir(&c, "nope");
	ok(pd == NULL, "a missing port is not found");

	/* Ownership, which is what a file conflict is actually about.
	 *
	 * 00_cross and 10_bootstrap install tar, musl, binutils and gcc by
	 * hand with `make DESTDIR=$SYSROOT install`, so those files exist and
	 * NO database entry owns them. 20_selfhost — the self-hosting
	 * bootstrap — then rebuilds exactly those packages with kpkg. If an unowned file
	 * counts as a conflict the bootstrap cannot run at all, which is what
	 * `tar` failed on. A file another PACKAGE owns is still a conflict. */
	char *dbdir = kb_path_join(dir, "db");
	kb_mkdir_p(dbdir);
	char *dbfile = kb_path_join(dbdir, "owner");
	kb_write_file(dbfile, "1 1\n./usr/bin/owned\n./usr/share/\n");
	free(dbfile);
	/* The rest of this function relies on PKGDB_DIR=/dev/null meaning
	 * "empty database", so the real one is only pointed at here. */
	setenv("PKGDB_DIR", dbdir, 1);
	KpConf owned_conf;
	kp_conf_load(&owned_conf);
	free(dbdir);

	KpOwned *ow = kp_owned_load(&owned_conf);
	/* An overwrite has to know WHO to take the path from. */
	eq_str(kp_owned_owner(ow, "usr/bin/owned"), "owner",
	       "a file an installed package owns names its package");
	ok(kp_owned_owner(ow, "usr/bin/stray") == NULL,
	   "a file left by a hand install is owned by nobody");
	ok(kp_owned_owner(ow, "usr/share") == NULL,
	   "directories are shared and never owned");
	kp_owned_free(ow);

	/* A declared takeover (`replaces =`) moves the path out of the old
	 * owner's manifest. Left
	 * behind, it is claimed twice and `kpkgdel <old>` deletes a file the
	 * new owner installed. */
	char *drop[] = { (char *)"usr/bin/owned" };
	ok(kp_db_drop_paths(&owned_conf, "owner", drop, 1) == 1,
	   "the path leaves the old owner");
	ok(kp_db_drop_paths(&owned_conf, "owner", drop, 1) == 0,
	   "and dropping it twice is a no-op");
	ow = kp_owned_load(&owned_conf);
	ok(kp_owned_owner(ow, "usr/bin/owned") == NULL, "nothing claims it now");
	ok(kp_owned_owner(ow, "usr/share") == NULL,
	   "the rest of the manifest survived the rewrite");
	kp_owned_free(ow);
	char *dbcheck = kb_path_join(dir, "db/owner");
	size_t dblen = 0;
	char *dbtext = kb_read_all(dbcheck, &dblen);
	eq_str(dbtext, "1 1\n./usr/share/\n", "version line and all");
	free(dbtext);
	free(dbcheck);

	/* Merged /usr: `./bin/free` and `./usr/bin/free` are one file when the
	 * root's `bin` links to `usr/bin`. Compared as strings they never
	 * collide, and the file is claimed twice. */
	char *mroot = kb_path_join(dir, "mroot");
	char *mbin = kb_path_join(mroot, "usr/bin");
	kb_mkdir_p(mbin);
	free(mbin);
	char *mlink = kb_path_join(mroot, "bin");
	ok(symlink("usr/bin", mlink) == 0, "a merged-/usr root");
	free(mlink);
	char *mlib = kb_path_join(mroot, "lib");
	kb_mkdir_p(mlib);	/* a real directory is not an alias */
	free(mlib);
	char *mdb = kb_path_join(mroot, "db");
	kb_mkdir_p(mdb);
	char *mf = kb_path_join(mdb, "usrfree");
	kb_write_file(mf, "1 1\n./usr/bin/free\n./lib/x\n");
	free(mf);
	mf = kb_path_join(mdb, "procps");
	kb_write_file(mf, "1 1\n./bin/free\n");
	free(mf);
	free(mdb);
	setenv("KPKG_ROOT", mroot, 1);
	setenv("PKGDB_DIR", "db", 1);
	KpConf mconf;
	kp_conf_load(&mconf);

	KpCanon mk;
	kp_canon_load(&mconf, &mk);
	char *cp = kp_canon_path(&mk, "./bin/free");
	eq_str(cp, "./usr/bin/free", "an aliased path takes its canonical name");
	free(cp);
	cp = kp_canon_path(&mk, "binutils/x");
	eq_str(cp, "binutils/x", "a name that only starts like an alias is not one");
	free(cp);
	cp = kp_canon_path(&mk, "lib/x");
	eq_str(cp, "lib/x", "a real top-level directory is not an alias");
	free(cp);

	ow = kp_owned_load(&mconf);
	eq_str(kp_owned_owner(ow, "bin/free"), "procps",
	       "either spelling finds the claim, and of two claimants the "
	       "owner is the first by package name");
	eq_str(kp_owned_owner(ow, "usr/bin/free"), "procps",
	       "under either spelling");
	ok(kp_owned_owner(ow, "usr/lib/x") == NULL,
	   "a path under a real directory keeps its own name");
	eq_str(kp_owned_other(ow, "usr/bin/free", "usrfree"), "procps",
	       "the other claimant is found under the other spelling");
	eq_str(kp_owned_other(ow, "bin/free", "procps"), "usrfree",
	       "from either side");
	ok(kp_owned_other(ow, "lib/x", "usrfree") == NULL,
	   "a sole claimant has no other");
	kp_owned_free(ow);

	/* The partial table answers exactly as the full one for the keys it
	 * was asked for, under either spelling, and knows nothing else. The
	 * lock and the sidecar directory sit in the database and are never
	 * read as packages. */
	mdb = kb_path_join(mroot, "db");
	mf = kb_path_join(mdb, "extra");
	kb_write_file(mf, "1 1\n./usr/bin/extra\n");
	free(mf);
	mf = kb_path_join(mdb, ".recipe");
	kb_mkdir_p(mf);
	free(mf);
	free(mdb);
	int lk = kp_db_lock(&mconf);
	ok(lk >= 0, "the database takes its writer lock");
	char *lpath = kb_path_join(mroot, "db/.lock");
	int lk2 = open(lpath, O_RDWR | O_CLOEXEC);
	ok(lk2 >= 0 && flock(lk2, LOCK_EX | LOCK_NB) != 0,
	   "a second writer finds it held");
	ow = kp_owned_load(&mconf);
	ok(ow->nowner == 3, "the lock and .recipe/ are not packages");
	char *want[] = { (char *)"bin/free", (char *)"./lib/x",
			 (char *)"usr/bin/none" };
	KpOwned *some = kp_owned_load_some(&mconf, want, 3);
	ok(some->n == 3, "only the claims on the asked keys are kept");
	eq_str(kp_owned_owner(some, "usr/bin/free"),
	       kp_owned_owner(ow, "usr/bin/free"),
	       "a partial table names the owner the full one names");
	eq_str(kp_owned_other(some, "usr/bin/free", "usrfree"), "procps",
	       "and finds the other claimant under the other spelling");
	eq_str(kp_owned_other(some, "bin/free", "procps"), "usrfree",
	       "from either side");
	eq_str(kp_owned_owner(some, "lib/x"), "usrfree",
	       "a key asked with its ./ is found without it");
	ok(kp_owned_other(some, "lib/x", "usrfree") == NULL,
	   "a sole claimant has no other in either table");
	ok(kp_owned_owner(some, "usr/bin/none") == NULL &&
	   kp_owned_owner(ow, "usr/bin/none") == NULL,
	   "an unclaimed key is unclaimed in both");
	ok(kp_owned_owner(ow, "usr/bin/extra") != NULL &&
	   kp_owned_owner(some, "usr/bin/extra") == NULL,
	   "a key not asked for is not in the partial table");
	kp_owned_free(some);
	some = kp_owned_load_some(&mconf, NULL, 0);
	ok(some->n == 0 && kp_owned_owner(some, "bin/free") == NULL,
	   "asking about nothing reads nothing");
	kp_owned_free(some);
	kp_owned_free(ow);
	if (lk2 >= 0)
		close(lk2);
	close(lk);
	lk2 = open(lpath, O_RDWR | O_CLOEXEC);
	ok(lk2 >= 0 && flock(lk2, LOCK_EX | LOCK_NB) == 0,
	   "and the lock is free once its descriptor closes");
	if (lk2 >= 0)
		close(lk2);
	free(lpath);

	char *mdrop[] = { (char *)"bin/free" };
	ok(kp_db_drop_paths(&mconf, "usrfree", mdrop, 1) == 1,
	   "an overwrite takes the path under the other spelling");
	ow = kp_owned_load(&mconf);
	eq_str(kp_owned_owner(ow, "usr/bin/free"), "procps",
	       "and the file has one owner");
	kp_owned_free(ow);
	unsetenv("KPKG_ROOT");
	kb_rmtree(mroot);
	free(mroot);
	setenv("PKGDB_DIR", "/dev/null", 1);

	kb_rmtree(dir);
	free(repo);
}


/* ──────────────────────────────────────────────────────────────────────── */

static void put_recipe(const char *root, const char *rel)
{
	char *d = kb_path_join(root, rel);
	kb_mkdir_p(d);
	char *r = kb_path_join(d, "kpkgbuild");
	kb_write_file(r, "name = x\nversion = 1\nrelease = 1\n");
	free(r);
	free(d);
}

/* Whether `fn` makes the process exit 1, run in a child. kb_die is exit(1)
 * and a leak check at exit would turn that into its own status, so the child
 * leaves through _exit from an exit handler registered after the sanitiser's
 * — handlers run last-registered first. */
static void die_exit(void)
{
	_exit(1);
}

static int dies(void (*fn)(const KpConf *), const KpConf *c)
{
	fflush(stdout);
	fflush(stderr);
	pid_t pid = fork();
	if (pid == 0) {
		int nul = open("/dev/null", O_WRONLY);
		if (nul >= 0)
			dup2(nul, 2);
		atexit(die_exit);
		fn(c);
		_exit(0);
	}
	int st = 0;
	if (pid < 0 || waitpid(pid, &st, 0) < 0)
		return 0;
	return WIFEXITED(st) && WEXITSTATUS(st) == 1;
}

static void lookup_dup(const KpConf *c)
{
	free(kp_port_dir(c, "dup"));
}

static void walk_all(const KpConf *c)
{
	kb_strv_free(kp_all_ports(c, NULL));
}

/*
 * The shelved ports tree. `ports/core/<shelf>/<name>/` holds the upstream
 * ports and every `src/<area>/<name>/` holds its ports flat, and one resolver
 * reads both. Each fixture here is a failure that is SILENT without the rule
 * it pins: a walker that does not descend sees zero ports and reports success,
 * a duplicate picks one of two recipes, a nested port is never found.
 */
static void test_shelves(void)
{
	printf("libkpkg shelves\n");

	char dir[] = "/tmp/kdos-selftest-shelf.XXXXXX";
	ok(mkdtemp(dir) != NULL, "scratch directory");

	char *core = kb_path_join(dir, "core");
	put_recipe(core, "archiver/zlib");
	put_recipe(core, "base/musl");
	put_recipe(core, "net/curl");
	put_recipe(core, "loose");		/* flat beside the shelves */
	put_recipe(core, ".cache/hidden");	/* never a shelf */
	char *emp = kb_path_join(core, "empty/notes");
	kb_mkdir_p(emp);		/* a shelf with no ports, only a stray */
	free(emp);
	emp = kb_path_join(core, "empty/README");
	kb_write_file(emp, "nothing filed here yet\n");
	free(emp);
	char *flat = kb_path_join(dir, "flat");
	put_recipe(flat, "kdos-tools");
	put_recipe(flat, "zlib");		/* shadows core's: first repo wins */

	KpConf *c = kb_calloc(1, sizeof(*c));
	char list[1200];
	snprintf(list, sizeof(list), "%s %s", core, flat);
	ok(kp_conf_set_repos(c, list) == 2, "two repositories");
	ok(c->shelf_cached[0], "the shelf list is cached");

	char err[1600], *pd = NULL, *want = NULL;
	ok(kp_port_find(c, "curl", &pd, err, sizeof(err)) == 1,
	   "a shelved port resolves");
	want = kb_path_join(core, "net/curl");
	eq_str(pd, want, "at <repo>/<shelf>/<name>");
	free(want);
	free(pd);
	pd = kp_port_dir(c, "loose");
	want = kb_path_join(core, "loose");
	eq_str(pd, want, "a flat port beside the shelves resolves");
	free(want);
	free(pd);
	pd = kp_port_dir(c, "kdos-tools");
	want = kb_path_join(flat, "kdos-tools");
	eq_str(pd, want, "a flat repository resolves");
	free(want);
	free(pd);
	pd = kp_port_dir(c, "zlib");
	want = kb_path_join(core, "archiver/zlib");
	eq_str(pd, want, "the first repository holding a name wins it");
	free(want);
	free(pd);
	ok(kp_port_find(c, "archiver", &pd, err, sizeof(err)) == 0,
	   "a shelf is not a port");
	ok(kp_port_find(c, "empty", &pd, err, sizeof(err)) == 0,
	   "nor is a shelf with no ports");
	ok(kp_port_find(c, "hidden", &pd, err, sizeof(err)) == 0,
	   "a dot-directory is never a shelf");
	ok(kp_port_find(c, "nope", &pd, err, sizeof(err)) == 0 && !pd,
	   "a missing port is not found");

	int n = 0;
	char **all = kp_ports_scan(c, &n, err, sizeof(err));
	ok(all && n == 5, "the walker reads both depths of every repository");
	if (all && n == 5) {
		eq_str(all[0], "curl", "sorted across shelves");
		eq_str(all[4], "zlib", "and a shadowed name counted once");
	}
	kb_strv_free(all);

	/* The cache is only a speed-up: a KpConf whose repos were written by
	 * hand lists the shelves live and answers the same. */
	KpConf *live = kb_calloc(1, sizeof(*live));
	live->nrepos = 1;
	kb_strlcpy(live->repos[0], core, sizeof(live->repos[0]));
	pd = kp_port_dir(live, "musl");
	want = kb_path_join(core, "base/musl");
	eq_str(pd, want, "an uncached repository resolves the same");
	free(want);
	free(pd);

	/* A NAME FILED TWICE IS AN ERROR, never a silent first-wins. */
	char *twice = kb_path_join(dir, "twice");
	put_recipe(twice, "base/dup");
	put_recipe(twice, "net/dup");
	KpConf *d2 = kb_calloc(1, sizeof(*d2));
	kp_conf_set_repos(d2, twice);
	ok(kp_port_find(d2, "dup", &pd, err, sizeof(err)) == -1 && !pd,
	   "a name on two shelves is refused");
	ok(strstr(err, "base/dup") && strstr(err, "net/dup"),
	   "naming both paths");
	ok(!kp_ports_scan(d2, &n, err, sizeof(err)) && strstr(err, "dup"),
	   "and the walker refuses it too");
	ok(dies(lookup_dup, d2), "kp_port_dir dies rather than pick one");
	ok(dies(walk_all, d2), "kp_all_ports dies rather than skip one");

	char *mixed = kb_path_join(dir, "mixed");
	put_recipe(mixed, "dup");
	put_recipe(mixed, "base/dup");
	kp_conf_set_repos(d2, mixed);
	ok(kp_port_find(d2, "dup", &pd, err, sizeof(err)) == -1,
	   "a name flat and shelved is refused");
	ok(!kp_ports_scan(d2, &n, err, sizeof(err)),
	   "by the walker as well");

	/* One shelf level and no more: a port below it is found by nothing. */
	char *deep = kb_path_join(dir, "deep");
	put_recipe(deep, "base/group/lost");
	put_recipe(deep, "base/ok");
	kp_conf_set_repos(d2, deep);
	ok(!kp_ports_scan(d2, &n, err, sizeof(err)) && strstr(err, "nested"),
	   "a port nested below its shelf is an error");

	/* KP_MAX_REPOS truncates, and says so on stderr. */
	char many[4096] = "";
	for (int i = 0; i < KP_MAX_REPOS + 2; i++)
		snprintf(many + strlen(many), sizeof(many) - strlen(many),
			 "%s%s/r%d", i ? " " : "", dir, i);
	fflush(stderr);
	int saved = dup(2), nul = open("/dev/null", O_WRONLY);
	if (nul >= 0)
		dup2(nul, 2);
	int kept = kp_conf_set_repos(d2, many);
	if (saved >= 0) {
		dup2(saved, 2);
		close(saved);
	}
	if (nul >= 0)
		close(nul);
	ok(kept == KP_MAX_REPOS, "PORT_REPO stops at KP_MAX_REPOS");

	free(d2);
	free(live);
	free(c);
	free(deep);
	free(mixed);
	free(twice);
	free(flat);
	free(core);
	kb_rmtree(dir);
}


/* ──────────────────────────────────────────────────────────────────────── */

/*
 * libkproc, against testing/fixtures/res.
 *
 * Every assertion here is a rule that fails SILENTLY when it is wrong: a
 * doubled disk rate, a battery at 100% health, a wrapped counter as a spike,
 * an unreadable io column of zeroes. None of them crashes, and none of them is
 * visible to the compiler.
 */
static void test_proc(void)
{
	printf("libkproc\n");

	const char *base = getenv("KDOS_RES_FIXTURE");
	if (!base)
		base = "testing/fixtures/res";

	char p0[512], s0[512], p1[512], s1[512];
	snprintf(p0, sizeof(p0), "%s/proc", base);
	snprintf(s0, sizeof(s0), "%s/sys", base);
	snprintf(p1, sizeof(p1), "%s/next/proc", base);
	snprintf(s1, sizeof(s1), "%s/next/sys", base);

	if (!kb_path_exists(p0)) {
		printf("  (fixture absent — skipped)\n");
		return;
	}

	kpr_root_set(p0, s0);

	/* ── CPU: topology is read, never counted ───────────────────────── */
	KprCpu c0;
	ok(kpr_cpu_read(&c0) == 0, "cpu snapshot reads");
	eq_int(c0.ncpu, 2, "logical cpus from /proc/stat");
	/* cpuinfo has two blocks and the machine has two CORES; a tree where
	 * these differ is what catches counting cpuinfo and calling it cores. */
	eq_int(c0.ncore, 2, "cores from topology/, not from cpuinfo blocks");
	eq_int(c0.npkg, 1, "packages from topology/");
	eq_int(c0.virt, KPR_VIRT_QEMU, "virtualisation from DMI sys_vendor");
	ok(c0.temp_c > 51.9 && c0.temp_c < 52.1, "cpu temperature from hwmon");
	eq_str(c0.governor, "schedutil", "cpufreq governor");

	kpr_root_set(p1, s1);
	KprCpu c1;
	kpr_cpu_read(&c1);
	double busy = kpr_cpu_busy(&c0.total, &c1.total);
	/* delta: user+sys 500, idle+iowait 500, total 1000 -> exactly half.
	 * Computed over the DELTA; over the absolute it would be ~13%. */
	ok(busy > 0.49 && busy < 0.51, "cpu busy is over the delta, not the total");
	kpr_cpu_free(&c0);
	kpr_cpu_free(&c1);

	kpr_root_set(p0, s0);

	/* ── memory: MemAvailable, never total - free ───────────────────── */
	KprMem m;
	ok(kpr_mem_read(&m) == 0, "meminfo reads");
	ok(m.available == 5242880ULL * 1024ULL, "available is MemAvailable");
	/*
	 * The fixture's available is deliberately NOT half of total: with
	 * total/2 the used figure and the available figure come out equal and
	 * a swapped subtraction renders identically to a correct one.
	 */
	ok(m.total - m.available != m.available, "used and available differ");
	/* total - free would be 15.5 GB of 16 GB — a healthy machine at 97%. */
	ok(m.total - m.available < m.total - m.free, "used is not total - free");

	/* ── pressure: absent is not zero ───────────────────────────────── */
	KprPsi psi;
	ok(kpr_psi_read("io", &psi) == 0 && psi.present, "psi io present");
	ok(psi.some10 > 11.9 && psi.some10 < 12.1, "psi some avg10");
	ok(psi.full10 > 7.9 && psi.full10 < 8.1, "psi full avg10");
	KprPsi none;
	ok(kpr_psi_read("nosuchfile", &none) != 0 || !none.present,
	   "a missing pressure file is absent, not a stall of zero");

	/* ── processes ──────────────────────────────────────────────────── */
	KprSample sm;
	ok(kpr_sample_take(&sm, KPR_WANT_IO | KPR_WANT_CMDLINE |
			       KPR_WANT_STATUS) == 0, "sample taken");
	eq_int(sm.n, 9, "every pid in the fixture");
	eq_int(sm.nkthread, 2, "kernel threads counted, not dropped");

	const KprProc *fx = kpr_find_pid(&sm, 950);
	ok(fx != NULL, "the boxed app is in the sample");
	eq_str(fx ? fx->comm : "", "firefox-esr", "comm parsed");
	const KprProc *wc = kpr_find_pid(&sm, 951);
	/* comm carries a space; the split must be on the LAST ')' or every
	 * field after it is read from the wrong place. */
	eq_str(wc ? wc->comm : "", "Web Content", "a comm with a space in it");
	ok(wc && wc->ppid == 950, "ppid survives a comm with a space");

	const KprProc *bk = kpr_find_pid(&sm, 800);
	ok(bk && bk->state == 'D', "the blocked process is in D");
	ok(bk && bk->rd_bytes != KPR_UNREADABLE && bk->rd_bytes > 0,
	   "a readable io counter is a number");

	/* The trap this sentinel exists for: an unreadable counter must never
	 * reach a column as 0, which would claim root's sshd did no disk io. */
	const KprProc *sd = kpr_find_pid(&sm, 300);
	ok(sd && sd->rd_bytes == KPR_UNREADABLE,
	   "an unreadable io is KPR_UNREADABLE, never 0");
	ok(sd && sd->wr_bytes == KPR_UNREADABLE, "both io counters, not just one");

	/* ── identity: the box, and its boundary ────────────────────────── */
	char box[64];
	ok(kpr_box_of(&sm, 950, box, sizeof(box)) && !strcmp(box, "kdos-apps"),
	   "a process under conmon is in its box");
	ok(kpr_box_of(&sm, 951, box, sizeof(box)) && !strcmp(box, "kdos-apps"),
	   "and so is its child, two hops up");
	ok(!kpr_box_of(&sm, 700, box, sizeof(box)),
	   "a process whose parent chain leaves the box is in none");
	/* conmon runs on the HOST and supervises from outside; reporting it as
	 * a member would put the supervisor in the app's own rollup. */
	ok(!kpr_box_of(&sm, 900, box, sizeof(box)),
	   "conmon itself is not in the box it supervises");

	/* ── cpu% of one core, and the new-process case ─────────────────── */
	kpr_root_set(p1, s1);
	KprSample sm1;
	kpr_sample_take(&sm1, 0);
	const KprProc *a = kpr_find_pid(&sm, 950), *b = kpr_find_pid(&sm1, 950);
	double pc = kpr_proc_cpu(a, b, 1000);
	ok(pc > 0.0, "a process that used cpu reports some");
	/* No previous sample means no interval to divide by; reporting the
	 * whole lifetime as this second's usage is how a fresh process shows
	 * thousands of percent. */
	ok(kpr_proc_cpu(NULL, b, 1000) == 0.0, "a process first seen reports 0");
	kpr_sample_free(&sm1);
	kpr_sample_free(&sm);
	kpr_root_set(p0, s0);

	/* ── block: whole disks only ────────────────────────────────────── */
	KprDisk *d = NULL;
	int nd = kpr_block_list(&d);
	/* sda1 is in diskstats beside sda. Summing both counts every byte
	 * twice, which is the defect this count catches. */
	eq_int(nd, 3, "sda, nvme0n1 and loop0 — sda1 is a partition");
	int seen_sda = 0, seen_part = 0, seen_loop_virt = 0;
	for (int i = 0; i < nd; i++) {
		if (!strcmp(d[i].name, "sda"))
			seen_sda = 1;
		if (!strcmp(d[i].name, "sda1"))
			seen_part = 1;
		if (!strcmp(d[i].name, "loop0") && d[i].virt)
			seen_loop_virt = 1;
	}
	ok(seen_sda, "the whole disk is kept");
	ok(!seen_part, "the partition is not");
	ok(seen_loop_virt, "loop0 is flagged virtual, not dropped");
	for (int i = 0; i < nd; i++)
		if (!strcmp(d[i].name, "sda")) {
			/* A diskstats sector is 512 by definition of that
			 * interface, whatever the drive's own sector size. */
			ok(d[i].size == 976773168ULL * 512ULL,
			   "size is sysfs 512-byte units");
			ok(d[i].rotational == 1, "rotational read from queue/");
		}
	kpr_block_free(d);

	/* ── net: a wrap is a gap, not a negative ───────────────────────── */
	KprIface *n0 = NULL;
	int nn = kpr_net_list(&n0);
	ok(nn == 3, "three interfaces");
	unsigned long long eth_rx0 = 0;
	for (int i = 0; i < nn; i++) {
		if (!strcmp(n0[i].name, "lo")) {
			/* operstate is "unknown" for loopback for ever; IFF_UP
			 * out of the flags word is the real answer. */
			ok(n0[i].up, "lo is up from flags, not from operstate");
			ok(!n0[i].carrier, "and its operstate is not up");
			ok(n0[i].loopback && n0[i].virt, "lo is flagged");
		}
		if (!strcmp(n0[i].name, "eth0")) {
			eth_rx0 = n0[i].rx_bytes;
			ok(n0[i].speed_mbit == 1000, "speed where the driver has one");
			eq_str(n0[i].driver, "", "no driver link in the fixture");
		}
		if (!strcmp(n0[i].name, "wlan0"))
			ok(n0[i].speed_mbit == -1,
			   "no speed is -1, never 0 Mbit");
	}
	kpr_net_free(n0);

	kpr_root_set(p1, s1);
	KprIface *n1 = NULL;
	kpr_net_list(&n1);
	unsigned long long eth_rx1 = 0;
	for (int i = 0; i < 3; i++)
		if (!strcmp(n1[i].name, "eth0"))
			eth_rx1 = n1[i].rx_bytes;
	kpr_net_free(n1);
	/* 4294967000 -> 200 across the interval. A subtraction produces an
	 * enormous negative; the caller must see the decrease and report a
	 * gap. The library's job is to hand back both readings faithfully. */
	ok(eth_rx1 < eth_rx0, "the wrapped counter decreased");
	kpr_root_set(p0, s0);

	/* ── power: health is wear, capacity is charge ──────────────────── */
	KprBattery *bt = NULL;
	int nb = kpr_power_list(&bt);
	ok(nb == 2, "one battery and one adapter");
	for (int i = 0; i < nb; i++) {
		if (!bt[i].is_battery) {
			ok(bt[i].online == 0, "the adapter is unplugged");
			continue;
		}
		ok(bt[i].capacity == 90, "capacity is the charge");
		/* 50 Wh of a design 62 Wh. A monitor printing only capacity
		 * would say this battery is fine. */
		ok(bt[i].health > 0.80 && bt[i].health < 0.81,
		   "health is full / full_design, not capacity");
		ok(bt[i].cycles == 412, "cycle count");
		ok(bt[i].power_uw == 8500000, "draw from power_now");
	}
	kpr_power_free(bt);

	/* ── the ring, the axis and the smoothing ───────────────────────── */
	KprHist h;
	kpr_hist_init(&h, 0);
	for (int i = 0; i < 5; i++)
		kpr_hist_push(&h, 1000.0);
	double sc = kpr_hist_scale(&h);
	ok(sc == 16e3, "a small series sits on the first rung");
	kpr_hist_push(&h, 100000.0);
	ok(kpr_hist_scale(&h) == 256e3, "the axis grows the moment a sample does not fit");
	/*
	 * Hysteresis. The rung below 256e3 is 64e3, so a series peaking at
	 * 100e3 has no rung to step down to that it would still fit inside and
	 * must hold the axis still; one peaking at 1e3 fits two rungs lower and
	 * must let the axis down.
	 */
	for (int i = 0; i < KPR_HIST; i++)
		kpr_hist_push(&h, 100e3);
	ok(kpr_hist_scale(&h) == 256e3,
	   "the axis holds where no lower rung would fit the peak");
	for (int i = 0; i < KPR_HIST; i++)
		kpr_hist_push(&h, 1000.0);
	ok(kpr_hist_scale(&h) < 256e3, "and comes down when one would");

	/*
	 * AND IT SETTLES. A peak between the rung below and the scale is the
	 * case that flipped between full scale and the bottom rung on
	 * alternate ticks: the shrink found no rung between peak and scale and
	 * fell through to the smallest one, which is under the peak, and the
	 * next call grew straight back. Every rung has such a band, and a
	 * fifth of all steady rates land in one.
	 */
	for (int i = 0; i < (int)(sizeof((double[]){ 16e3, 64e3, 256e3, 1e6,
						     4e6, 16e6, 64e6, 256e6,
						     1e9, 4e9 }) /
				 sizeof(double));
	     i++) {
		const double rungs[] = { 16e3, 64e3, 256e3, 1e6, 4e6, 16e6,
					 64e6, 256e6, 1e9, 4e9 };
		double pk = rungs[i] * 0.3;
		double a = kpr_scale_step(pk, rungs[i]);
		double b = kpr_scale_step(pk, a);

		ok(a >= pk && b == a,
		   "the axis never drops below its own peak and settles");
	}

	KprHist pin;
	kpr_hist_init(&pin, 1);
	kpr_hist_push(&pin, 3.0);
	ok(kpr_hist_scale(&pin) == 100.0,
	   "a percentage ring is pinned 0..100 and never rescales");

	KprHist sm3;
	kpr_hist_init(&sm3, 0);
	kpr_hist_push(&sm3, 0.0);
	kpr_hist_push(&sm3, 30.0);
	kpr_hist_push(&sm3, 0.0);
	ok(kpr_hist_smooth(&sm3, 1) == 10.0, "the plotted series is a three-point mean");
	ok(kpr_hist_at(&sm3, 1) == 30.0, "the stored sample is never smoothed");
	ok(kpr_hist_peak(&sm3) == 30.0, "peak is of the raw series");
}

/* ──────────────────────────────────────────────────────────────────────── */
/* libkbuild's snapshot layers: the diff that decides what a layer holds and
 * the chains that decide what a restore reads. Both go wrong silently — a
 * missed change restores a stale file, a wrong chain restores a tree that
 * never existed — so each rule is pinned by a case that breaks it. */

static void ix_put(KbuildSnapIndex *ix, unsigned mode, unsigned long long ino,
		   long long ct, const char *path)
{
	kbuild_snap_idx_add(ix, mode, ino, ct, mode == S_IFREG ? 4096 : 0, path);
}

/* The NUL-separated list as "a|b|c". */
static void nul_join(const KbBuf *b, char *out, size_t cap)
{
	size_t o = 0;
	out[0] = 0;
	for (size_t i = 0; b->p && i < b->n && o + 1 < cap; i++)
		out[o++] = b->p[i] ? b->p[i] : '|';
	if (o && out[o - 1] == '|')
		o--;
	out[o] = 0;
}

static void snap_manifest(const char *root, const char *dir, const char *text,
			  const char *const *members)
{
	char *d = kb_path_join(root, dir);
	kb_mkdir_p(d);
	char *mf = kb_path_join(d, KBUILD_MANIFEST);
	kb_write_file(mf, text);
	free(mf);
	for (int i = 0; members && members[i]; i++) {
		char *p = kb_path_join(d, members[i]);
		kb_write_file(p, "x");
		free(p);
	}
	free(d);
}

#define SNAP4(id, kind, base, removed) \
	"{\"schema\": 4, \"id\": \"" id "\", \"codec\": \"zstd\", " \
	"\"paths\": [{\"path\": \"fs\", \"archive\": \"fs.tar.zst\", " \
	"\"kind\": \"" kind "\", \"base\": " base ", \"removed\": " removed \
	", \"bytes_compressed\": 10}]}"

static void test_snap_layers(void)
{
	printf("libkbuild snapshot layers\n");

	ok(kbuild_snap_path_cmp("fs/a", "fs/a/b") < 0, "a directory sorts before its contents");
	ok(kbuild_snap_path_cmp("fs/a/z", "fs/a-b") < 0,
	   "'/' sorts below every other byte: a/z before a-b");
	ok(kbuild_snap_path_cmp("fs/a-b", "fs/a/z") > 0, "and the reverse");
	ok(kbuild_snap_path_cmp("fs", "fs") == 0, "equal paths compare equal");

	KbuildPhase ex;
	memset(&ex, 0, sizeof(ex));
	kb_strlcpy(ex.snap_exclude[0], "fs/tmp/*", sizeof(ex.snap_exclude[0]));
	kb_strlcpy(ex.snap_exclude[1], "fs/root/.cache/*",
		   sizeof(ex.snap_exclude[1]));
	ex.nexclude = 2;
	ok(kbuild_snap_exclude_match(&ex, "fs/tmp/x"), "exclude: a child matches");
	ok(kbuild_snap_exclude_match(&ex, "fs/tmp/x/y"), "exclude: '*' crosses '/'");
	ok(!kbuild_snap_exclude_match(&ex, "fs/tmp"), "exclude: the directory itself stays");
	ok(!kbuild_snap_exclude_match(&ex, "fs/var/tmp/x"),
	   "exclude: matched against the whole path, not a tail of it");
	ok(kbuild_snap_exclude_match(&ex, "fs/root/.cache/pip"),
	   "exclude: '*' matches a leading dot");

	ok(kbuild_snap_removal_ok("fs", "fs/a/b"), "removal: under the path");
	ok(kbuild_snap_removal_ok("fs", "fs"), "removal: the path itself");
	ok(!kbuild_snap_removal_ok("fs", "fsx/a"), "removal: a sibling is refused");
	ok(!kbuild_snap_removal_ok("fs", "cross/a"), "removal: another path is refused");
	ok(!kbuild_snap_removal_ok("fs", "fs/../etc"), "removal: '..' is refused");
	ok(!kbuild_snap_removal_ok("fs", "fs//a"), "removal: an empty component is refused");
	ok(!kbuild_snap_removal_ok("fs", "fs/./a"), "removal: '.' is refused");
	ok(!kbuild_snap_removal_ok("fs", "/fs/a"), "removal: absolute is refused");

	/* ----- the diff ------------------------------------------------- */

	KbuildSnapIndex base = {0}, cur = {0};
	ix_put(&base, S_IFDIR, 1, 10, "fs");
	ix_put(&base, S_IFREG, 2, 10, "fs/a");
	ix_put(&base, S_IFDIR, 3, 10, "fs/d");
	ix_put(&base, S_IFREG, 4, 10, "fs/d/x");
	ix_put(&base, S_IFREG, 5, 10, "fs/d/y");
	ix_put(&base, S_IFREG, 6, 10, "fs/e");
	ix_put(&base, S_IFLNK, 7, 10, "fs/s");
	ix_put(&base, S_IFDIR, 12, 10, "fs/t");
	ix_put(&base, S_IFDIR, 13, 10, "fs/t/u");
	ix_put(&base, S_IFREG, 14, 10, "fs/t/u/v");
	ix_put(&base, S_IFREG, 15, 10, "fs/u");

	ix_put(&cur, S_IFDIR, 1, 10, "fs");
	ix_put(&cur, S_IFREG, 2, 20, "fs/a");	/* ctime moved      */
	ix_put(&cur, S_IFREG, 8, 30, "fs/b");	/* new              */
	ix_put(&cur, S_IFDIR, 3, 10, "fs/d");	/* lost fs/d/y      */
	ix_put(&cur, S_IFREG, 4, 10, "fs/d/x");	/* untouched        */
	ix_put(&cur, S_IFDIR, 9, 30, "fs/e");	/* file -> dir      */
	ix_put(&cur, S_IFREG, 10, 30, "fs/e/z");
	ix_put(&cur, S_IFLNK, 11, 10, "fs/s");	/* replaced, same ctime */
	ix_put(&cur, S_IFREG, 15, 10, "fs/u");	/* untouched        */

	KbBuf add = {0}, gone = {0};
	KbuildSnapDiff st;
	kbuild_snap_diff(&base, &cur, &add, &gone, &st);
	char got[512];
	nul_join(&add, got, sizeof(got));
	eq_str(got, "fs|fs/a|fs/b|fs/d|fs/e|fs/e/z|fs/s",
	       "diff: new, changed, type-changed, re-inoded, and parents");
	nul_join(&gone, got, sizeof(got));
	eq_str(got, "fs/d/y|fs/e|fs/t",
	       "diff: removals at their top, and every type change");
	ok(st.listed == 7 && st.removed == 3, "diff: the counts agree");
	ok(st.listed_alloc == 3 * 4096, "diff: allocated bytes of the listed files");
	kb_buf_free(&add);
	kb_buf_free(&gone);

	/* A dir -> symlink under a dir that also lost children: the children
	 * are not listed again below the type change. */
	KbuildSnapIndex b2 = {0}, c2 = {0};
	ix_put(&b2, S_IFDIR, 1, 10, "fs");
	ix_put(&b2, S_IFDIR, 2, 10, "fs/a");
	ix_put(&b2, S_IFREG, 3, 10, "fs/a/k");
	ix_put(&b2, S_IFREG, 4, 10, "fs/a-b");
	ix_put(&c2, S_IFDIR, 1, 10, "fs");
	ix_put(&c2, S_IFLNK, 5, 40, "fs/a");
	ix_put(&c2, S_IFREG, 4, 10, "fs/a-b");
	kbuild_snap_diff(&b2, &c2, &add, &gone, &st);
	nul_join(&add, got, sizeof(got));
	eq_str(got, "fs|fs/a", "diff: a dir that became a symlink is listed once");
	nul_join(&gone, got, sizeof(got));
	eq_str(got, "fs/a", "diff: and removed once, its old contents with it");
	kb_buf_free(&add);
	kb_buf_free(&gone);

	/* No change at all still lists the root: a layer is never an empty
	 * archive. */
	kbuild_snap_diff(&cur, &cur, &add, &gone, &st);
	nul_join(&add, got, sizeof(got));
	eq_str(got, "fs", "diff: nothing changed lists the root alone");
	ok(gone.n == 0, "diff: and removes nothing");
	kb_buf_free(&add);
	kb_buf_free(&gone);

	/* ----- the index file ------------------------------------------- */

	char dir[] = "/tmp/kdos-selftest-snp.XXXXXX";
	ok(mkdtemp(dir) != NULL, "scratch directory");
	char *idxf = kb_path_join(dir, "fs.idx");
	kb_strlcpy(cur.head, "41_system-1-00000000", sizeof(cur.head));
	cur.phase_index = 6;
	kb_strlcpy(cur.phase_dir, "41_system", sizeof(cur.phase_dir));
	cur.partial = 1;
	cur.root_dev = 2049;
	cur.root_ino = 77;
	ix_put(&cur, S_IFREG, 16, 10, "fs/v\twith tab\nand newline");
	ok(kbuild_snap_idx_write(idxf, &cur) == 0, "index: writes");
	KbuildSnapIndex back;
	ok(kbuild_snap_idx_read(idxf, &back) == 0, "index: reads back");
	ok(back.n == cur.n && !strcmp(back.head, cur.head) &&
	   back.phase_index == 6 && back.partial == 1 &&
	   back.root_dev == 2049 && back.root_ino == 77,
	   "index: the header round-trips");
	ok(back.n == cur.n &&
	   !strcmp(kbuild_idx_path(&back, back.n - 1),
		   "fs/v\twith tab\nand newline") &&
	   back.rec[1].ctime_ns == 20 && back.rec[1].alloc == 4096,
	   "index: a name with a tab and a newline round-trips");
	kbuild_snap_idx_free(&back);
	KbuildSnapIndex bad = {0};
	ix_put(&bad, S_IFDIR, 1, 10, "fs/b");
	ix_put(&bad, S_IFDIR, 1, 10, "fs/a");
	kb_strlcpy(bad.head, "x", sizeof(bad.head));
	kbuild_snap_idx_write(idxf, &bad);
	ok(kbuild_snap_idx_read(idxf, &back) < 0,
	   "index: records out of walk order read as no index");
	kb_write_file(idxf, "kdos-snap-index 1\nhead x\n");
	ok(kbuild_snap_idx_read(idxf, &back) < 0,
	   "index: a short header reads as no index");
	kbuild_snap_idx_free(&bad);
	kbuild_snap_idx_free(&base);
	kbuild_snap_idx_free(&cur);
	kbuild_snap_idx_free(&b2);
	kbuild_snap_idx_free(&c2);
	free(idxf);

	/* ----- chains --------------------------------------------------- */

	static const char *const ARCH[] = { "fs.tar.zst", NULL };
	static const char *const ARCHG[] = { "fs.tar.zst", "fs.gone", NULL };
	snap_manifest(dir, "00_a",
		      "{\"schema\": 3, \"phase_dir\": \"00_a\", \"created\": 100.0,"
		      " \"entries\": [{\"path\": \"fs\", \"archive\": "
		      "\"fs.tar.zst\", \"bytes_compressed\": 100}]}", ARCH);
	snap_manifest(dir, "01_b", SNAP4("01_b-1-aaaaaaaa", "layer",
					 "\"legacy-00_a-1000\"", "\"fs.gone\""),
		      ARCHG);
	snap_manifest(dir, ".held/02_c@02_c-1-bbbbbbbb",
		      SNAP4("02_c-1-bbbbbbbb", "layer", "\"01_b-1-aaaaaaaa\"",
			    "null"), ARCH);
	snap_manifest(dir, "03_d", SNAP4("03_d-1-cccccccc", "layer",
					 "\"02_c-1-bbbbbbbb\"", "null"), ARCH);
	snap_manifest(dir, "04_e", SNAP4("04_e-1-dddddddd", "layer",
					 "\"no-such-id\"", "null"), ARCH);
	snap_manifest(dir, ".held/05_x@x1", SNAP4("x1", "layer", "\"x2\"",
						  "null"), ARCH);
	snap_manifest(dir, ".held/05_y@x2", SNAP4("x2", "layer", "\"x1\"",
						  "null"), ARCH);
	snap_manifest(dir, "06_f", SNAP4("06_f-1-eeeeeeee", "layer", "\"x1\"",
					 "null"), ARCH);
	snap_manifest(dir, ".held/07_z@z1", SNAP4("z1", "full", "null", "null"),
		      ARCH);
	/* Named but absent: its .gone list is gone, so the whole snapshot is. */
	snap_manifest(dir, "08_g", SNAP4("08_g-1-ffffffff", "layer",
					 "\"03_d-1-cccccccc\"", "\"fs.gone\""),
		      ARCH);
	/* Schema 4 carrying only `entries` is not a manifest this reads. */
	snap_manifest(dir, "09_h",
		      "{\"schema\": 4, \"id\": \"h\", \"entries\": [{\"path\": "
		      "\"fs\", \"archive\": \"fs.tar.zst\"}]}", ARCH);

	KbuildSnapshot *all = kb_calloc(KBUILD_MAX_SNAPS, sizeof(*all));
	int nall = kbuild_snap_list_all(dir, all, KBUILD_MAX_SNAPS);
	eq_int(nall, 9, "chains: every loadable snapshot, held ones included");
	const KbuildSnapshot *a0 = kbuild_snap_by_id(all, nall, "legacy-00_a-1000");
	ok(a0 && !strcmp(a0->dir_name, "00_a") && !a0->entry[0].layer,
	   "chains: a schema-3 snapshot is full, with its legacy id");
	const KbuildSnapshot *held = kbuild_snap_by_id(all, nall,
						       "02_c-1-bbbbbbbb");
	ok(held && held->held, "chains: a held snapshot is found by its id");

	const KbuildSnapshot *d = kbuild_snap_find(all, nall, "03_d");
	const KbuildSnapshot *chain[KBUILD_MAX_CHAIN];
	int len = d ? kbuild_snap_chain(all, nall, d, "fs", chain,
					KBUILD_MAX_CHAIN) : -1;
	ok(len == 4 && chain[0] == a0 && !strcmp(chain[1]->dir_name, "01_b") &&
	   chain[2] == held && chain[3] == d,
	   "chains: base first, through a held snapshot, to the top");
	ok(len > 0 && kbuild_snap_chain(all, nall, d, "fs", chain, 3) < 0,
	   "chains: longer than the room given is refused");
	const KbuildSnapshot *e = kbuild_snap_find(all, nall, "04_e");
	ok(e && !kbuild_snap_usable(all, nall, e),
	   "chains: a missing base makes a snapshot unusable");
	const KbuildSnapshot *f = kbuild_snap_find(all, nall, "06_f");
	ok(f && !kbuild_snap_usable(all, nall, f),
	   "chains: a loop of bases is refused");
	ok(!kbuild_snap_find(all, nall, "08_g"),
	   "chains: a layer whose .gone list is missing reads as absent");
	ok(!kbuild_snap_find(all, nall, "09_h"),
	   "chains: schema 4 without `paths` reads as absent");

	KbuildSnapshot *use = kb_calloc(KBUILD_MAX_PHASES, sizeof(*use));
	int nuse = kbuild_snap_list(dir, use, KBUILD_MAX_PHASES);
	ok(nuse == 3 && !strcmp(use[0].dir_name, "00_a") &&
	   !strcmp(use[1].dir_name, "01_b") && !strcmp(use[2].dir_name, "03_d"),
	   "chains: the inventory is the phase snapshots whose chains resolve");
	free(use);

	int idx[KBUILD_MAX_SNAPS];
	int nd = kbuild_snap_dependants(all, nall, "01_b-1-aaaaaaaa", idx,
					KBUILD_MAX_SNAPS);
	ok(nd == 2, "chains: the snapshots layered on one, directly or not");
	int ng = kbuild_snap_gc_set(all, nall, idx, KBUILD_MAX_SNAPS);
	ok(ng == 1 && !strcmp(all[idx[0]].id, "z1"),
	   "chains: only a held snapshot no phase reaches is collectable");

	KbuildPhase ph[4];
	memset(ph, 0, sizeof(ph));
	static const char *const PD[] = { "00_a", "01_b", "02_c", "03_d" };
	for (int i = 0; i < 4; i++) {
		kb_strlcpy(ph[i].dir_name, PD[i], sizeof(ph[i].dir_name));
		ph[i].index = i;
	}
	KbuildRestoreItem *plan = kb_calloc(KBUILD_MAX_RESTORE, sizeof(*plan));
	int np = kbuild_snap_plan_restore(dir, ph, 4, 3, plan,
					  KBUILD_MAX_RESTORE);
	ok(np == 4 && plan[0].seq == 0 && !plan[0].layer &&
	   plan[3].seq == 3 && plan[3].nseq == 4 &&
	   !strcmp(plan[3].id, "03_d-1-cccccccc"),
	   "plan: a restore reads the whole chain, base first");
	ok(np == 4 && plan[1].removed[0] && !plan[2].removed[0] &&
	   strstr(plan[2].archive, "/.held/02_c@02_c-1-bbbbbbbb/"),
	   "plan: removal lists and held archives are carried");
	np = kbuild_snap_plan_restore(dir, ph, 4, 1, plan, KBUILD_MAX_RESTORE);
	ok(np == 2 && !strcmp(plan[1].source, "01_b"),
	   "plan: a middle phase stops at its own layer");
	free(plan);
	free(all);

	kb_rmtree(dir);
}

/* ──────────────────────────────────────────────────────────────────────── */

static void test_build(void)
{
	printf("libkbuild\n");

	char v[128];

	kbuild_unquote("\"Cross Toolchain\"", v, sizeof(v));
	eq_str(v, "Cross Toolchain", "double-quoted literal");
	kbuild_unquote("'single'", v, sizeof(v));
	eq_str(v, "single", "single-quoted literal");
	kbuild_unquote("  bare   # trailing", v, sizeof(v));
	eq_str(v, "bare", "unquoted stops at whitespace and comment");
	/* An unterminated quote is not a literal, and metadata values are
	 * required to be literals — no expansion is ever performed. */
	kbuild_unquote("\"never closed", v, sizeof(v));
	eq_str(v, "", "unterminated quote reads as empty");
	kbuild_unquote("", v, sizeof(v));
	eq_str(v, "", "empty value");

	/* A snapshot path is deleted and re-extracted AS ROOT. Everything that
	 * could escape $BUILD_DIR is refused rather than sanitised. */
	ok(kbuild_safe_relpath("fs"), "a plain relative path is safe");
	ok(kbuild_safe_relpath("fs/usr/bin"), "a nested relative path is safe");
	ok(!kbuild_safe_relpath("/fs"), "absolute is refused");
	ok(!kbuild_safe_relpath("~/fs"), "a home path is refused");
	ok(!kbuild_safe_relpath(""), "empty is refused");
	ok(!kbuild_safe_relpath("."), "'.' is refused");
	ok(!kbuild_safe_relpath("/"), "'/' is refused");
	ok(!kbuild_safe_relpath("../etc"), "a leading .. is refused");
	ok(!kbuild_safe_relpath("fs/../.."), "an embedded .. is refused");
	/* ".." only counts as a whole component: "..foo" is a real name. */
	ok(kbuild_safe_relpath("fs/..foo"), "a name beginning .. is allowed");

	/* The reject path itself — no phase in the tree has a bad entry, so it
	 * is only reachable with a synthetic one. */
	char dir[] = "/tmp/kdos-selftest-bld.XXXXXX";
	ok(mkdtemp(dir) != NULL, "scratch directory");
	char *pd = kb_path_join(dir, "phases/07_evil");
	kb_mkdir_p(pd);
	char *step = kb_path_join(pd, "00_run.sh");
	kb_write_file(step, "#!/bin/bash\n");
	free(step);
	char *env = kb_path_join(pd, "phase.env");
	kb_write_file(env,
		"export KDOS_PHASE_TITLE=\"Evil\"\n"
		"export CHROOT=1\n"
		"export KDOS_SNAPSHOT_PATHS=\"fs ../escape /abs\"\n"
		"export KDOS_SNAPSHOT_EXCLUDE=\"fs/tmp/*\"\n"
		"rm -rf /var/cache/kpkg/work\n");

	KbuildPhase ph[KBUILD_MAX_PHASES];
	int n = kbuild_discover(dir, ph, KBUILD_MAX_PHASES);
	ok(n == 1, "one phase discovered");
	if (n == 1) {
		eq_str(ph[0].name, "evil", "phase name drops the numeric prefix");
		eq_str(ph[0].title, "Evil", "declared title wins");
		ok(ph[0].chroot == 1, "CHROOT=1 is read");
		ok(ph[0].nsnap == 1, "only the safe path is kept");
		eq_str(ph[0].snap_path[0], "fs", "the safe path");
		ok(ph[0].nrejected == 2, "both unsafe paths are REPORTED");
		ok(kbuild_snapshottable(&ph[0]), "phase is snapshottable");
		char label[128];
		kbuild_label(&ph[0], label, sizeof(label));
		eq_str(label, "evil (Chroot)", "label marks a chroot phase");
	}

	/* A phase with no declared paths is never snapshotted. */
	char *pd2 = kb_path_join(dir, "phases/08_bare");
	kb_mkdir_p(pd2);
	n = kbuild_discover(dir, ph, KBUILD_MAX_PHASES);
	ok(n == 2, "a phase without an env file still discovers");
	ok(!kbuild_snapshottable(&ph[1]), "no paths means never snapshotted");
	eq_str(ph[1].title, "bare", "title falls back to the tidied name");
	/* ...but one with nothing to run is refused, or it reports success. */
	ok(ph[1].error[0] != 0, "a phase with no list and no step is refused");
	ok(!ph[0].error[0], "a phase with a step is not");

	/* The repositories a phase's kpkg searches, mapped onto the host. */
	char repos[512];
	ok(kbuild_phase_repos(&ph[0], "/r", repos, sizeof(repos)) == 1,
	   "no PORT_REPO is one repository");
	eq_str(repos, "/r/ports/core", "no PORT_REPO is /ports/core");
	char *env2 = kb_path_join(pd2, "phase.env");
	kb_write_file(env2,
		"export PORT_REPO=\"/ports/old\"\n"
		"  export PORT_REPO=\"/ports/core /kdos/src/system\" # last wins\n");
	free(env2);
	n = kbuild_discover(dir, ph, KBUILD_MAX_PHASES);
	ok(n == 2 && kbuild_phase_repos(&ph[1], "/r", repos, sizeof(repos)) == 2,
	   "the last PORT_REPO line is the one read");
	eq_str(repos, "/r/ports/core /r/src/system",
	       "/ports maps to <repo>/ports and /kdos to <repo>");
	ok(kbuild_phase_repos(&ph[1], "/r", repos, 8) == -1 && !repos[0],
	   "repositories that do not fit are refused, not cut short");

	/* ----- the build plan ------------------------------------------- */

	KbuildPlan pl;
	char err[256];

	ok(kbuild_plan_from_cli(&pl, NULL, NULL, NULL, ph, n, err,
				sizeof(err)) == 0, "the empty plan parses");
	ok(!kbuild_plan_custom(&pl), "no arguments is not a custom plan");
	ok(kbuild_plan_phase_selected(&pl, "07_evil"), "everything is selected");
	ok(kbuild_plan_step_selected(&pl, "07_evil", "x.sh"), "every step runs");

	/* Rebuilding a port is custom but does NOT narrow execution — the
	 * distinction is what decides whether snapshots are suppressed. */
	ok(kbuild_plan_from_cli(&pl, NULL, NULL, "zlib, zlib musl", ph, n, err,
				sizeof(err)) == 0, "a rebuild-only plan parses");
	ok(kbuild_plan_custom(&pl), "rebuild makes a plan custom");
	ok(!kbuild_plan_narrows(&pl), "rebuild alone does not narrow execution");
	ok(pl.nrebuild == 2, "a repeated port is forced once");
	ok(kbuild_plan_forced(&pl, "musl") && !kbuild_plan_forced(&pl, "bash"),
	   "only the named ports are forced");

	ok(kbuild_plan_from_cli(&pl, "evil", NULL, NULL, ph, n, err,
				sizeof(err)) == 0, "a phase resolves by short name");
	eq_str(pl.phase[0], "07_evil", "and is stored by directory name");
	ok(kbuild_plan_narrows(&pl), "selecting phases narrows execution");
	ok(!kbuild_plan_phase_selected(&pl, "08_bare"), "the others are skipped");

	ok(kbuild_plan_from_cli(&pl, "nosuch", NULL, NULL, ph, n, err,
				sizeof(err)) < 0, "an unknown phase is refused");
	eq_str(err, "unknown phase: nosuch", "with the phase named");
	ok(kbuild_plan_from_cli(&pl, NULL, "evil", NULL, ph, n, err,
				sizeof(err)) < 0, "--steps without a colon is refused");

	/* A plan file that is missing or not an object reads as NO plan.
	 * Reading it as "run everything" is the dangerous failure. */
	ok(kbuild_plan_load(&pl, dir) < 0, "a missing plan file is not a plan");
	char *pf = kb_path_join(dir, KBUILD_PLAN_FILE);
	kb_write_file(pf, "not json at all\n");
	ok(kbuild_plan_load(&pl, dir) < 0, "a malformed plan file is not a plan");

	KbuildPlan saved;
	kbuild_plan_from_cli(&saved, "evil", NULL, "zlib", ph, n, err, sizeof(err));
	ok(kbuild_plan_save(&saved, dir) == 0, "a plan writes");
	ok(kbuild_plan_load(&pl, dir) == 0, "and reads back");
	ok(pl.nphase == 1 && !strcmp(pl.phase[0], "07_evil"), "phases round-trip");
	ok(pl.nrebuild == 1 && kbuild_plan_forced(&pl, "zlib"),
	   "rebuilds round-trip");
	free(pf);

	/* ----- the phase layout ----------------------------------------- */

	char ldir[] = "/tmp/kdos-selftest-lst.XXXXXX";
	ok(mkdtemp(ldir) != NULL, "scratch directory");
	/* The script directory is the parent of phases/: a phase directory
	 * directly under it is not a phase. */
	char *stray = kb_path_join(ldir, "05_stray");
	kb_mkdir_p(stray);
	free(stray);
	char *f;
#define LPUT(rel, text) do { f = kb_path_join(ldir, rel); \
	char *slash = strrchr(f, '/'); *slash = 0; kb_mkdir_p(f); *slash = '/'; \
	kb_write_file(f, text); free(f); } while (0)
	/* KDOS_* keys come from phase.env's own text; the file it sources is
	 * NOT read, so a key written only there is not seen. */
	LPUT("env/common.env", "export KDOS_PHASE_TITLE=\"Shared\"\n");
	LPUT("phases/10_one/phase.env",
	     "source script/env/common.env\n"
	     "export KDOS_PHASE_DESC=\"own\"\n");
	/* Out of order on disk, unterminated, commented, and beside files
	 * that are not part of the list. */
	LPUT("phases/10_one/packages.d/b.txt", "three\n# four\n  five  ");
	LPUT("phases/10_one/packages.d/a.txt", "two");
	LPUT("phases/10_one/packages.d/00-order.txt", "# pinned\none\n");
	LPUT("phases/10_one/packages.d/README", "not-a-port\n");
	LPUT("phases/10_one/packages.d/.x.txt", "hidden\n");
	LPUT("phases/10_one/packages.d/c.sh", "not-a-step\n");
	LPUT("phases/20_both/packages.txt", "a\n");
	LPUT("phases/20_both/packages.d/x.txt", "b\n");
	LPUT("phases/30_nolist/packages.d/README", "x\n");
	LPUT("phases/40_flat/packages.txt",
	     "solo\n# base \xe2\x80\x94 the base shelf\nlater\n");
#undef LPUT

	KbuildPhase lp[KBUILD_MAX_PHASES];
	int ln = kbuild_discover(ldir, lp, KBUILD_MAX_PHASES);
	ok(ln == 4, "phases are read from phases/ and nowhere else");
	if (ln == 4) {
		ok(strstr(lp[0].env_file, "/phases/10_one/phase.env") != NULL,
		   "the environment is the phase's own phase.env");
		eq_str(lp[0].desc, "own", "its keys are read");
		eq_str(lp[0].title, "one", "a sourced file's keys are not");
		ok(kbuild_is_package_phase(&lp[0]) == 1 && !lp[0].error[0],
		   "packages.d/ makes a package phase");
		int nst = 0;
		kb_strv_free(kbuild_steps(&lp[0], &nst));
		ok(nst == 0, "and a package phase has no steps");
		int np = 0;
		char **pk = kbuild_packages(&lp[0], &np);
		ok(np == 4, "packages.d/ reads as one list");
		if (np == 4) {
			eq_str(pk[0], "one", "files in byte order: 00-order first");
			eq_str(pk[1], "two", "an unterminated file keeps its last name");
			eq_str(pk[2], "three", "and the next file its first");
			eq_str(pk[3], "five", "names are trimmed");
		}
		kb_strv_free(pk);
		int nf = 0;
		kb_strv_free(kbuild_list_files(&lp[0], &nf));
		ok(nf == 3, "only packages.d/*.txt is part of the list");

		ok(kbuild_is_package_phase(&lp[1]) == -1,
		   "packages.txt and packages.d/ together are refused");
		ok(strstr(lp[1].error, "both") != NULL, "at discovery");
		ok(strstr(lp[2].error, "no *.txt") != NULL,
		   "a packages.d/ with no list is refused");
		pk = kbuild_packages(&lp[3], &np);
		ok(np == 2 && !strcmp(pk[0], "solo") && !strcmp(pk[1], "later") &&
		   !lp[3].error[0], "a single packages.txt reads");
		kb_strv_free(pk);

		/* The order run: 00-order.txt of a packages.d/, the names
		 * ahead of a packages.txt's first shelf banner. */
		char **run = NULL;
		int nrun = kbuild_packages_order_run(&lp[0], &run);
		ok(nrun == 1 && !strcmp(run[0], "one"),
		   "packages.d's order run is 00-order.txt");
		kb_strv_free(run);
		nrun = kbuild_packages_order_run(&lp[3], &run);
		ok(nrun == 1 && !strcmp(run[0], "solo"),
		   "a packages.txt's order run ends at its first shelf banner");
		kb_strv_free(run);
	}

	/* The port index walks ports/core at both depths through libkpkg, and
	 * src/system and src/art flat. */
	put_recipe(ldir, "ports/core/base/musl");
	put_recipe(ldir, "ports/core/net/curl");
	put_recipe(ldir, "src/system/kdos-tools");
	put_recipe(ldir, "src/art/kdos-theme");
	put_recipe(ldir, "src/desktop/kdos-runner");
	char perr[512] = "";
	int npo = 0;
	char **po = kbuild_ports(ldir, &npo, perr, sizeof(perr));
	ok(po && npo == 4, "the index sees ports/core, src/system and src/art");
	kb_strv_free(po);
	static KbuildPkgRef ref[64];
	int nref = kbuild_package_index(lp, ln, ldir, ref, 64, perr,
					sizeof(perr));
	int solo_seen = 0, shell_seen = 0;
	for (int i = 0; i < nref; i++) {
		solo_seen |= !strcmp(ref[i].name, "solo") &&
			     !strcmp(ref[i].phase, "40_flat");
		shell_seen |= !strcmp(ref[i].name, "kdos-runner");
	}
	ok(solo_seen, "a listed name joins the index with its phase");
	ok(!shell_seen, "src/desktop reaches it only through a list");
	char *pc = kb_path_join(ldir, "ports/core");
	kb_rmtree(pc);
	kb_mkdir_p(pc);
	free(pc);
	perr[0] = 0;
	fflush(stderr);
	int esaved = dup(2), enul = open("/dev/null", O_WRONLY);
	if (enul >= 0)
		dup2(enul, 2);
	po = kbuild_ports(ldir, &npo, perr, sizeof(perr));
	if (esaved >= 0) {
		dup2(esaved, 2);
		close(esaved);
	}
	if (enul >= 0)
		close(enul);
	ok(!po && perr[0], "an empty ports/core is an error, not an empty list");
	kb_strv_free(po);
	kb_rmtree(ldir);

	/* ----- the JSON manifests carry -------------------------------- */

	KjNode *j = kj_parse("{\"a\": \"x\\u2014y\", \"n\": -1.5e2, "
			     "\"t\": true, \"f\": false, \"z\": null, "
			     "\"arr\": [1, {\"k\": \"v\"}]}");
	ok(j != NULL, "a manifest-shaped document parses");
	eq_str(kj_str(j, "a", ""), "x\xe2\x80\x94y", "\\uXXXX becomes UTF-8");
	ok(kj_num(j, "n", 0) == -150.0, "exponents and signs are read");
	ok(kj_bool(j, "t", 0) && !kj_bool(j, "f", 1), "true and false");
	ok(kj_bool(j, "z", 1) == 1, "null falls back to the default");
	eq_str(kj_str(kj_get(j, "arr")->child->next, "k", ""), "v",
	       "an object inside an array");
	ok(kj_str(j, "missing", "def") != NULL &&
	   !strcmp(kj_str(j, "missing", "def"), "def"), "a missing key defaults");
	kj_free(j);

	/* Anything that does not parse WHOLE is not a document. A manifest
	 * read as half a manifest would restore half a tree. */
	ok(kj_parse("not json") == NULL, "bare text is refused");
	ok(kj_parse("{\"a\": 1") == NULL, "a truncated object is refused");
	ok(kj_parse("{\"a\": 1} trailing") == NULL, "trailing junk is refused");
	ok(kj_parse("{\"a\": }") == NULL, "a missing value is refused");
	ok(kj_parse("[1, 2,]") == NULL, "a trailing comma is refused");
	/* Freed rather than dropped so the whole suite runs clean under
	 * `CC="cc -fsanitize=address,undefined" testing/selftest.sh` — one
	 * leaked node is enough to make LeakSanitizer's verdict useless. */
	KjNode *empty = kj_parse("{}");
	ok(empty != NULL, "an empty object is still a document");
	kj_free(empty);

	/* /proc reports st_size 0. A whole-file read that trusts it returns an
	 * empty string, and "no mounts" is the wrong answer to act on. */
	size_t plen = 0;
	char *mounts = kb_read_all("/proc/self/mounts", &plen);
	ok(mounts && plen > 0, "a whole-file read gets past a stat'ed size of 0");
	free(mounts);

	free(pd);
	free(pd2);
	free(env);
	kb_rmtree(dir);
}

/* ──────────────────────────────────────────────────────────────────────── */

static void test_ramps(int caps, int want_levels, const char *tier)
{
	ktui_caps = caps;
	ktui_ramp_init();
	checks++;
	if (ktui_ramp_levels() != want_levels) {
		failures++;
		printf("  FAIL  %s ramp has %d levels, want %d\n", tier,
		       ktui_ramp_levels(), want_levels);
	}

	/* Endpoints are exact: an empty bar must not show a sliver and a full
	 * one must not stop one shade short. */
	eq_str(ktui_ramp_h(0.0), " ", "empty cell is a space");
	eq_str(ktui_ramp_v(0.0), " ", "empty vertical cell is a space");

	/* Monotonic, and never out of range at the edges. */
	int mono = 1;
	const char *prev = ktui_ramp_h(0.0);
	for (int i = 0; i <= 100; i++) {
		const char *now = ktui_ramp_h(i / 100.0);
		if (!now)
			mono = 0;
		prev = now;
	}
	(void)prev;
	ok(mono, "the horizontal ramp answers every input");
	ok(ktui_ramp_h(-5.0) != NULL && ktui_ramp_h(9.0) != NULL,
	   "out-of-range input is clamped, not indexed");
	ok(!strcmp(ktui_ramp_h(9.0), ktui_ramp_h(1.0)),
	   "above 1 is the full cell");

	/* Small non-zero f must show a sliver, not empty: the bar's tip tells
	 * the story of how full it is, and a rounded-down tip lies about that. */
	ok(strcmp(ktui_ramp_h(0.01), ktui_ramp_h(0.0)) != 0,
	   "a small non-zero fraction is never the empty cell");
	ok(strcmp(ktui_ramp_v(0.01), ktui_ramp_v(0.0)) != 0,
	   "and the same vertically");
}

static void test_chart(void)
{
	printf("libktui charts\n");
	int saved = ktui_caps;
	test_ramps(KT_CAP_UTF8, 8, "rich");
	test_ramps(KT_CAP_UTF8 | KT_CAP_LINUXVT, 3, "vt");
	test_ramps(0, 3, "ascii");

	/* The vt tier may only use glyphs the shipped VT font carries. */
	ktui_caps = KT_CAP_UTF8 | KT_CAP_LINUXVT;
	ktui_ramp_init();
	eq_str(ktui_ramp_h(1.0), "█", "vt full cell is FULL BLOCK");
	eq_str(ktui_ramp_h(0.5), "▒", "vt mid cell is MEDIUM SHADE");
	eq_str(ktui_ramp_h(0.2), "░", "vt low cell is LIGHT SHADE");

	ktui_caps = KT_CAP_UTF8;
	ktui_ramp_init();
	eq_str(ktui_ramp_h(1.0), "█", "rich full cell is FULL BLOCK");
	eq_str(ktui_ramp_v(0.125), "▁", "rich vertical starts at LOWER ONE EIGHTH");

	ktui_caps = saved;
	ktui_ramp_init();

	/* The integer/tip split. A 40-cell bar at exactly half is 20 solid
	 * cells and NO tip — a tip there would read as 51%. */
	double tip = -1;
	ok(ktui_bar_fill(40, 0.5, &tip) == 20 && tip == 0.0,
	   "half of 40 cells is 20 solid and no tip");
	ok(ktui_bar_fill(40, 0.5125, &tip) == 20 && tip > 0.49 && tip < 0.51,
	   "half a cell past half is 20 solid plus a half tip");
	ok(ktui_bar_fill(40, 0.0, &tip) == 0 && tip == 0.0, "zero is empty");
	ok(ktui_bar_fill(40, 1.0, &tip) == 40 && tip == 0.0, "one is full");
	ok(ktui_bar_fill(40, 1.5, &tip) == 40 && tip == 0.0, "over one clamps");
	ok(ktui_bar_fill(40, -1.0, &tip) == 0 && tip == 0.0, "under zero clamps");

	/* Never w+1 cells: solid + (tip ? 1 : 0) has to fit. */
	int overflow = 0;
	for (int w = 1; w <= 200; w++)
		for (int i = 0; i <= 1000; i++) {
			int solid = ktui_bar_fill(w, i / 1000.0, &tip);
			if (solid + (tip > 0 ? 1 : 0) > w)
				overflow = 1;
		}
	ok(!overflow, "solid plus tip never exceeds the bar width");
}

/* ──────────────────────────────────────────────────────────────────────── */

/*
 * The cell grid's own arithmetic: widths, the paste queue, the caret, and one
 * modal rendered offscreen.
 *
 * S3 (no wcwidth anywhere) and S2 (nothing above 0x7f could be typed) were
 * both invisible to the compiler and to a suite with no terminal, and the
 * damage they did — a CJK filename shearing every column after it in pick, a
 * non-ASCII password that could not unlock the session — is arithmetic, so it
 * is checkable here rather than by looking at a screen.
 */

static void feed(KtuiEvent *ev, int key)
{
	memset(ev, 0, sizeof(*ev));
	ev->type = KT_EVT_KEY;
	ev->key = key;
}

/* ktui_draw_dump() writes the cell buffer to stdout; there is no reader for
 * it, and a golden-frame check needs one. Redirect, render, read back. */
static char *capture_dump(void)
{
	char tmpl[] = "/tmp/kdos-selftest-dump.XXXXXX";
	int fd = mkstemp(tmpl);
	if (fd < 0)
		return NULL;
	fflush(stdout);
	int saved = dup(1);
	if (saved < 0 || dup2(fd, 1) < 0) {
		close(fd);
		unlink(tmpl);
		return NULL;
	}
	ktui_draw_dump();
	fflush(stdout);
	dup2(saved, 1);
	close(saved);
	close(fd);
	size_t n = 0;
	char *text = kb_read_all(tmpl, &n);
	unlink(tmpl);
	return text;
}

/*
 * A modal, rendered at a narrow size, with nothing outside its own rect.
 *
 * Buttons placed at `x + w - 40` with no floor start to the LEFT of the dialog
 * below 46 columns and draw over the page behind it. The rect is recomputed
 * here from the same three rules ktui_modal_draw uses (56 wide, ktui_w - 6,
 * floor 20) because the assertion IS "the drawing stayed inside the box the layout claims", and a check that
 * asked the drawing where the box was would agree with itself.
 */
static void check_modal(int sw, int sh)
{
	KtuiEvent ev;
	char what[64];

	ktui_offscreen_init(sw, sh);
	ktui_modal_confirm("Log Out", "End this session?", "Log Out",
			   "Cancel", NULL);
	feed(&ev, 0);
	ev.type = KT_EVT_NONE;
	ktui_frame_begin(&ev);
	ktui_draw_clear();
	ktui_modal_draw();
	ktui_frame_end();

	int w = 56;
	if (w > sw - 6)
		w = sw - 6;
	if (w < 20)
		w = 20;
	int h = 1 + 6;
	int x = (sw - w) / 2, y = (sh - h) / 2;
	if (x < 0)
		x = 0;
	if (y < 0)
		y = 0;

	char *text = capture_dump();
	if (!text) {
		ok(0, "the modal dump could not be captured");
		return;
	}
	int row = 0, col = 0, outside = 0;
	for (const char *p = text; *p; p++) {
		if (*p == '\n') {
			row++;
			col = 0;
			continue;
		}
		/* The dump is ASCII here: ktui_caps is 0 offscreen, so the box
		 * is drawn in the ascii tier and one byte is one cell. */
		if (*p != ' ' &&
		    (col < x || col >= x + w || row < y || row >= y + h))
			outside = 1;
		col++;
	}
	snprintf(what, sizeof(what),
		 "a modal at %dx%d draws nothing outside its own rect", sw, sh);
	ok(!outside, what);
	snprintf(what, sizeof(what), "a modal at %dx%d keeps both buttons", sw, sh);
	ok(strstr(text, "Log Out") && strstr(text, "Cancel"), what);
	free(text);
}

/*
 * THE DROP SHADOW DARKENS AND DOES NOT ERASE, and the translucency pass mixes
 * towards what was under it.
 *
 * NEITHER IS VISIBLE IN A GOLDEN. Both write the cell's LITERAL colour and
 * leave the slot alone — which is what makes a terminal and a braille reader
 * show an opaque window — and a golden is that same slot dump, so a
 * frame at 70 per cent and a frame at 100 are byte-identical there. This is
 * the only gate either has.
 */
static void test_shade(void)
{
	printf("libktui shadow and blend\n");

	int w = 0, h = 0;
	const KtuiCell *cells;

	ktui_offscreen_init(24, 8);
	ktui_draw_init();
	ktui_draw_fill(krect(0, 0, 24, 8), KT_BG);
	ktui_draw_fill(krect(2, 3, 12, 3), KT_SURFACE);
	ktui_draw_text(2, 3, 12, "HELLO", KT_TEXT, KT_SURFACE, KT_A_NONE);

	/* A window above and left of it, so the strip lands on the first
	 * window's top row rather than on bare desktop. */
	ktui_draw_shadow(krect(1, 2, 12, 1));

	cells = ktui_draw_cells(&w, &h);

	const KtuiCell *lit = &cells[3 * w + 2];
	const KtuiCell *off = &cells[3 * w + 20];

	eq_int((int)lit->ch, 'H',
	       "the shadow keeps the glyph of the window it falls on");
	ok((lit->attr & KT_A_BGRGB) && (lit->attr & KT_A_FGRGB),
	   "and writes both halves as literals, which is what a mix is");
	ok(((lit->bgc >> 16) & 0xff) <= ktui_theme->slot[KT_SURFACE].r &&
	   (lit->bgc & 0xff) <= ktui_theme->slot[KT_SURFACE].b,
	   "darker than the surface underneath, in every channel");
	eq_int(lit->bg, KT_BG,
	       "and the SLOT is the backdrop, which is the shadow a --tty shows");
	ok(!(off->attr & KT_A_BGRGB),
	   "a cell outside the one-cell strip is untouched");

	/*
	 * AND IT DARKENS IN EVERY SCHEME, not only in the one that happens to
	 * be the default. The strip is mixed towards KT_BG, and KT_BG is NOT
	 * the darker of the two in every accent — `bone`'s backdrop is lighter
	 * than its surface in red and green, `ice`'s in blue — so an
	 * unclamped mix makes those two glow along the shadowed edges instead.
	 * Asserted per scheme and per channel, because it is a property of the
	 * palette and the default moves.
	 */
	for (int t = 0; t < ktui_ntheme; t++) {
		ktui_theme_set(ktui_themes[t].name);
		ktui_offscreen_init(24, 8);
		ktui_draw_init();
		ktui_draw_fill(krect(0, 0, 24, 8), KT_BG);
		ktui_draw_fill(krect(2, 3, 12, 3), KT_SURFACE);
		ktui_draw_text(2, 3, 12, "HELLO", KT_TEXT, KT_SURFACE,
			       KT_A_NONE);
		ktui_draw_shadow(krect(1, 2, 12, 1));

		cells = ktui_draw_cells(&w, &h);
		const KtuiCell *sh = &cells[3 * w + 2];
		KRgb surf = ktui_theme->slot[KT_SURFACE];
		char why[96];

		snprintf(why, sizeof(why),
			 "%s: the shadow is darker than the surface, in all "
			 "three channels", ktui_themes[t].name);
		ok(((sh->bgc >> 16) & 0xff) <= surf.r &&
		   ((sh->bgc >> 8) & 0xff) <= surf.g &&
		   (sh->bgc & 0xff) <= surf.b, why);
	}
	ktui_theme_set(ktui_themes[KCOL_DEFAULT_INDEX].name);

	/* ── the blend: a rectangle mixed back towards what it covered ── */
	uint32_t under[12 * 3];

	ktui_offscreen_init(24, 8);
	ktui_draw_init();
	ktui_draw_fill(krect(0, 0, 24, 8), KT_BG);
	ktui_draw_bg_take(krect(2, 3, 12, 3), under);
	ktui_draw_fill(krect(2, 3, 12, 3), KT_SURFACE);
	ktui_draw_blend(krect(2, 3, 12, 3), under, 128);

	cells = ktui_draw_cells(&w, &h);
	lit = &cells[3 * w + 3];

	ok(lit->attr & KT_A_BGRGB, "a blended cell carries a literal background");
	eq_int(lit->bg, KT_SURFACE,
	       "and the slot is still what the window was drawn in");
	{
		unsigned got = (lit->bgc >> 16) & 0xff;
		unsigned want = ((unsigned)ktui_theme->slot[KT_SURFACE].r +
				 (unsigned)ktui_theme->slot[KT_BG].r) / 2;

		/* Half of each, within the rounding one integer divide costs. */
		ok(got + 2 >= want && want + 2 >= got,
		   "half of the window's colour and half of the desktop's");
	}

	/* AND NOT A PICTURE. A sprite cell is somebody else's pixels edge to
	 * edge; there is no background of ours behind them to mix. */
	ktui_draw_cell(4, 4, KTUI_SPRITE_BASE | (7u << 8), KT_TEXT, KT_BG, 0);
	ktui_draw_blend(krect(2, 3, 12, 3), under, 128);
	cells = ktui_draw_cells(&w, &h);
	ok(!(cells[4 * w + 4].attr & KT_A_BGRGB),
	   "a sprite cell is left out of the blend");
}

/*
 * THE SLIDER, WHICH IS THE ONE CONTROL A NUMBER DID NOT HAVE.
 *
 * Every knob in kdos-settings was an integer stepped with Left and Right and
 * with nothing else, so a mouse could not turn one at all. The value a cell
 * stands for and the cell a value lands on are the same arithmetic in opposite
 * directions; computed twice they drift, and the symptom is a thumb that does
 * not sit under the pointer that dragged it. These check the round trip.
 */
/* Defined with the a11y block below; the slider is the first caller. */
static const KtuiA11y *a11y_find(int role);

/*
 * WHERE THE THUMB IS, READ OFF THE FRAME THAT WAS DRAWN — never recomputed.
 * A test that worked the cell out for itself would be a second copy of the
 * arithmetic under test, and the two would agree with each other while
 * disagreeing with the screen.
 */
static int thumb_cell(KRect r, int val)
{
	int gw = 0, gh = 0;
	const KtuiCell *g;
	uint32_t mark = 0;
	int cell = -1;

	ktui_offscreen_init(40, 4);
	ktui_draw_init();
	ktui_slider_draw(r, val, 0, 100, 0, KT_SURFACE);
	g = ktui_draw_cells(&gw, &gh);
	ktui_utf8_next(ktui_glyph[KT_G_SQUARE], &mark);
	for (int x = r.x; x < r.x + r.w && x < gw; x++)
		if (g[(size_t)r.y * gw + x].ch == mark)
			cell = x;
	return cell;
}

static void test_slider(void)
{
	printf("libktui slider\n");

	KRect r = krect(2, 1, 20, 1);
	int v = 50;

	/* ── keys ──────────────────────────────────────────── */
	eq_int(ktui_slider_key(&v, 0, 100, 5, KT_K_RIGHT), 1, "Right steps up");
	eq_int(v, 55, "by one step");
	ktui_slider_key(&v, 0, 100, 5, KT_K_LEFT);
	eq_int(v, 50, "and Left steps back");
	ktui_slider_key(&v, 0, 100, 5, KT_K_HOME);
	eq_int(v, 0, "Home is the minimum");
	eq_int(ktui_slider_key(&v, 0, 100, 5, KT_K_LEFT), 0,
	       "and a step past an end moves nothing, so it reports nothing");
	ktui_slider_key(&v, 0, 100, 5, KT_K_END);
	eq_int(v, 100, "End is the maximum");
	eq_int(ktui_slider_key(&v, 0, 100, 5, 'x'), 0,
	       "a key the control has no use for is left for the surface");

	/* ── the pointer, and the round trip ─────────────────────── */
	v = 0;
	eq_int(ktui_slider_hit(r, &v, 0, 100, 5, r.x, r.y + 1, 1), 0,
	       "a press on another row is not this control's");

	/*
	 * GRABBING THE THUMB DOES NOT MOVE IT. A hundred values on sixteen
	 * cells cannot round-trip through the VALUES — a press snaps to the
	 * cell it landed on, which is what a track is — but it must round-trip
	 * through the CELLS, or the thumb jumps out from under the hand that
	 * took hold of it.
	 */
	{
		int bad_round = 0, bad_grab = 0;

		for (int want = 0; want <= 100; want += 4) {
			int got = want, cell = thumb_cell(r, want);
			int after;

			if (cell < 0) {
				bad_grab = 1;
				continue;
			}
			ktui_slider_hit(r, &got, 0, 100, 5, cell, r.y, 1);
			after = thumb_cell(r, got);
			if (after != cell)
				bad_grab = 1;
		}
		ok(!bad_grab,
		   "a press on the cell the thumb is drawn on leaves it there");

		/*
		 * AND THE TRACK RUNS THE RIGHT WAY. Measured from the ends,
		 * where the neighbouring cell is known to be track and not the
		 * gap beside an end cap: a slider drawn with the fill one way
		 * and read the other passes every round trip above and still
		 * runs backwards under the hand.
		 */
		int lo = 0, hi = 100;

		ktui_slider_hit(r, &lo, 0, 100, 5, thumb_cell(r, 0) + 1, r.y, 1);
		ktui_slider_hit(r, &hi, 0, 100, 5, thumb_cell(r, 100) - 1, r.y, 1);
		ok(lo > 0, "one cell right of the minimum is a larger value");
		ok(hi < 100, "and one cell left of the maximum is a smaller one");
		(void)bad_round;
	}

	/* ── the end caps step, and only on a press ──────────────── */
	v = 50;
	eq_int(ktui_slider_hit(r, &v, 0, 100, 5, r.x, r.y, 1), 1,
	       "a press on the left cap");
	eq_int(v, 45, "steps down by one");
	eq_int(ktui_slider_hit(r, &v, 0, 100, 5, r.x, r.y, 0), 0,
	       "and a DRAG across it steps nothing — it would fight the "
	       "position the same drag is setting");

	/* ── as a frame control ───────────────────────────────── */
	{
		KtuiEvent ev = { 0 };
		const KtuiA11y *a;

		ktui_offscreen_init(40, 4);
		ktui_draw_init();
		v = 40;
		ev.type = KT_EVT_KEY;
		ev.key = KT_K_RIGHT;
		ktui_frame_begin(&ev);
		eq_int(ktui_slider(r, &v, 0, 100, 5, "crt"), 1,
		       "the frame control takes the key of the focused slider");
		eq_int(v, 45, "and steps it");
		a = a11y_find(KT_A11Y_SLIDER);
		ok(a && !strcmp(a->label, "crt") && !strcmp(a->value, "45"),
		   "a focused slider says its name and its number");
		eq_int(ktui_consumed(), 1,
		       "and the key is spent, so the surface does not act on it too");
		ktui_frame_end();
	}
}

/*
 * THE TWO POINTER RULES EVERY LIST AND EVERY TABLE ON THIS DESKTOP KEEPS.
 *
 * They exist because twenty-one surfaces had written their own answer to
 * "what does a press on a row mean" or had written none at all, and the point
 * of one implementation is that it can be stated once and checked once. A
 * press MOVES the caret, a press on the row it is already on PICKS, the wheel
 * walks, and the right button is Back.
 */
/* ── sortable, right-aligned and resizable table columns ─────────────── */

static const char *tc_name[] = { "pear", "apple", "fig", "apple", "kiwi" };
static const int tc_size[] = { 30, 5, 30, 12, 7 };

static int tc_cmp(int a, int b, int col, void *user)
{
	(void)user;
	if (col == 0)
		return strcmp(tc_name[a], tc_name[b]);
	return (tc_size[a] > tc_size[b]) - (tc_size[a] < tc_size[b]);
}

static const KtuiCol tc_col[] = {
	{ "Name", 8, KT_COL_SORT | KT_COL_RESIZE },
	{ "Size", 6, KT_COL_SORT | KT_COL_RIGHT },
	{ "Note", 0, 0 },
};

static int tc_order[5];

static void tc_cell(int idx, int col, int x, int y, int w, int fg, int bg,
		    void *user)
{
	char n[16];
	int rec = tc_order[idx];

	(void)user;
	if (col == 1) {
		snprintf(n, sizeof(n), "%d", tc_size[rec]);
		ktui_table_text(&tc_col[1], x, y, w, n, fg, bg);
	} else if (col == 0) {
		ktui_table_text(&tc_col[0], x, y, w, tc_name[rec], fg, bg);
	}
}

static uint32_t tc_ch(int x, int y)
{
	int w = 0, h = 0;
	const KtuiCell *c = ktui_draw_cells(&w, &h);

	return x < w && y < h ? c[y * w + x].ch : 0;
}

static const KtuiCell *tc_cellat(int x, int y)
{
	int w = 0, h = 0;
	const KtuiCell *c = ktui_draw_cells(&w, &h);

	return &c[y * w + x];
}

static void test_table_columns(void)
{
	KRect r = krect(0, 0, 30, 8);	/* 2 header rows, 6 body rows */
	KtuiTable t = { 0 };
	KtuiEvent ev = { 0 };
	uint32_t up, down;

	ktui_utf8_next(ktui_glyph[KT_G_UP], &up);
	ktui_utf8_next(ktui_glyph[KT_G_DOWN], &down);
	ktui_offscreen_init(30, 8);
	ktui_draw_init();
	for (int i = 0; i < 5; i++)
		tc_order[i] = i;

	/* ── a zeroed table: unsorted, unresized, the accent ─────── */
	ktui_table_draw(r, &t, 5, tc_col, 3, tc_cell, NULL, NULL, -1);
	ok(tc_ch(5, 0) == ' ' && tc_ch(4, 0) != up,
	   "an unsorted table draws no sort marker");
	eq_int((int)tc_ch(14, 2), '0',
	       "a KT_COL_RIGHT cell ends at its column's edge");
	eq_int((int)tc_ch(11, 2), ' ', "and leaves the left of it empty");
	eq_int((int)tc_ch(14, 0), 'e',
	       "and so does a KT_COL_RIGHT title");
	eq_int(tc_cellat(3, 2)->bg, KT_ACCENT,
	       "a zeroed table lights its selection with the accent");

	/* ── a press on a sortable title sorts; again turns it round ── */
	ev.type = KT_EVT_MOUSE;
	ev.btn = KT_MB_LEFT;
	ev.press = KT_MP_PRESS;
	ev.mx = 2;
	ev.my = 0;
	t.sel = 0;		/* "pear" */
	eq_int(ktui_table_event(r, &t, 5, 6, 3, tc_col, &ev, NULL, NULL),
	       KTUI_TABLE_SORT, "a press on a sortable title sorts");
	ok(t.sort == 1 && !t.desc, "by that column, rising");
	ktui_table_sort(&t, tc_order, 5, tc_cmp, NULL);
	ok(tc_order[0] == 1 && tc_order[1] == 3 && tc_order[2] == 2 &&
	   tc_order[3] == 4 && tc_order[4] == 0,
	   "the order rises by name, and equal names keep their index order");
	eq_int(t.sel, 4, "the selection follows its record");
	{
		int rev[5] = { 4, 3, 2, 1, 0 };
		KtuiTable u = { 0 };

		u.sort = 1;
		ktui_table_sort(&u, rev, 5, tc_cmp, NULL);
		ok(rev[0] == 1 && rev[1] == 3,
		   "a tie is broken by index, not by where the array had it");
	}
	eq_int(ktui_table_event(r, &t, 5, 6, 3, tc_col, &ev, NULL, NULL),
	       KTUI_TABLE_SORT, "a second press on the same title");
	ok(t.sort == 1 && t.desc, "turns the order round");
	ktui_table_sort(&t, tc_order, 5, tc_cmp, NULL);
	ok(tc_order[0] == 0 && tc_order[3] == 1 && tc_order[4] == 3,
	   "falling by name, and equal names still keep their index order");
	eq_int(t.sel, 0, "and the selection is still on pear");

	ktui_table_draw(r, &t, 5, tc_col, 3, tc_cell, NULL, NULL, -1);
	ok(tc_ch(5, 0) == down, "the sorted title carries a falling arrow");
	ok(tc_cellat(0, 0)->fg == KT_TEXT && tc_cellat(5, 0)->fg == KT_ACCENT,
	   "the title lifts to KT_TEXT and the arrow wears the accent");

	ev.mx = 20;
	eq_int(ktui_table_event(r, &t, 5, 6, 3, tc_col, &ev, NULL, NULL),
	       KTUI_TABLE_NONE, "a title that does not sort ignores a press");
	ev.mx = 12;
	eq_int(ktui_table_event(r, &t, 5, 6, 3, tc_col, &ev, NULL, NULL),
	       KTUI_TABLE_SORT, "a right-aligned sortable title sorts too");
	ktui_table_sort(&t, tc_order, 5, tc_cmp, NULL);
	ok(tc_order[0] == 1 && tc_order[1] == 4 && tc_order[2] == 3 &&
	   tc_order[3] == 0 && tc_order[4] == 2,
	   "rising by size, the tie at 30 broken by index");
	ktui_table_draw(r, &t, 5, tc_col, 3, tc_cell, NULL, NULL, -1);
	ok(tc_ch(9, 0) == up && tc_ch(11, 0) == 'S',
	   "a right-aligned title's arrow stands before it");

	/* ── the keyboard reaches every sort ─────────────────────── */
	{
		KtuiTable k = { 0 };
		int seen = 0;

		ok(ktui_table_sort_next(&k, tc_col, 3) && k.sort == 1 &&
		   !k.desc, "the sort key starts on the first sortable column");
		ok(ktui_table_sort_next(&k, tc_col, 3) && k.sort == 1 && k.desc,
		   "then turns it round");
		ok(ktui_table_sort_next(&k, tc_col, 3) && k.sort == 2 &&
		   !k.desc, "then moves to the next sortable column");
		ktui_table_sort_next(&k, tc_col, 3);
		ktui_table_sort_next(&k, tc_col, 3);
		ok(k.sort == 1 && !k.desc,
		   "and skips the column that does not sort on the way round");
		for (int i = 0; i < 8; i++) {
			ktui_table_sort_next(&k, tc_col, 3);
			seen |= 1 << ((k.sort - 1) * 2 + k.desc);
		}
		eq_int(seen, 0xf, "four presses visit all four orders");
	}

	/* ── a header edge follows the pointer ───────────────────── */
	ev.mx = 8;		/* the cell right of "Name" (x 0, width 8) */
	ev.my = 1;
	ev.press = KT_MP_PRESS;
	eq_int(ktui_table_event(r, &t, 5, 6, 3, tc_col, &ev, NULL, NULL),
	       KTUI_TABLE_NONE, "a press on a resizable edge takes hold of it");
	eq_int(t.drag, 1, "and holds column 0");
	ev.press = KT_MP_DRAG;
	ev.btn = KT_MB_MOVE;
	ev.mx = 12;
	eq_int(ktui_table_event(r, &t, 5, 6, 3, tc_col, &ev, NULL, NULL),
	       KTUI_TABLE_RESIZED, "a motion while held resizes");
	eq_int(t.w[0], 12, "to where the pointer is");
	ev.mx = 1;
	ktui_table_event(r, &t, 5, 6, 3, tc_col, &ev, NULL, NULL);
	eq_int(t.w[0], 4, "never narrower than its title");
	ev.mx = 29;
	ktui_table_event(r, &t, 5, 6, 3, tc_col, &ev, NULL, NULL);
	eq_int(t.w[0], 21, "never so wide that the remainder column is gone");
	{
		int x[3], cw[3], used = 0;
		KtuiCol c[3] = { tc_col[0], tc_col[1], tc_col[2] };

		c[0].width = t.w[0];
		ktui_table_layout(c, 3, 30, x, cw);
		for (int i = 0; i < 3; i++)
			used += cw[i] + (i ? 1 : 0);
		ok(used <= 30 && cw[2] == 1,
		   "at the widest the row still fits the rect");
	}
	ev.press = KT_MP_RELEASE;
	ev.btn = KT_MB_LEFT;
	ktui_table_event(r, &t, 5, 6, 3, tc_col, &ev, NULL, NULL);
	eq_int(t.drag, 0, "the release lets go");
	ev.press = KT_MP_DRAG;
	ev.btn = KT_MB_MOVE;
	ev.mx = 10;
	eq_int(ktui_table_event(r, &t, 5, 6, 3, tc_col, &ev, NULL, NULL),
	       KTUI_TABLE_NONE, "and a motion after it moves nothing");
	ev.press = KT_MP_PRESS;
	ev.btn = KT_MB_LEFT;
	ev.mx = 15;		/* right of Size, which is not resizable */
	ev.my = 1;
	ktui_table_event(r, &t, 5, 6, 3, tc_col, &ev, NULL, NULL);
	eq_int(t.drag, 0, "an edge without KT_COL_RESIZE is not a handle");

	/* ── a widened column in a table that has since narrowed ─── */
	{
		KRect n = krect(0, 0, 18, 8);
		int past = 0, held = 0;

		for (int y = 0; y < 8; y++)
			for (int x = 0; x < 30; x++)
				ktui_draw_text(x, y, 1, "X", KT_TEXT, KT_BG,
					       KT_A_NONE);
		ktui_table_draw(n, &t, 5, tc_col, 3, tc_cell, NULL, NULL, -1);
		for (int y = 0; y < 8; y++)
			for (int x = 18; x < 30; x++)
				past += tc_ch(x, y) != 'X';
		eq_int(past, 0, "a narrowed table writes nothing right of its rect");
		eq_int((int)tc_ch(17, 0), 'N', "and keeps its remainder column");
		ev.press = KT_MP_PRESS;
		ev.btn = KT_MB_LEFT;
		ev.my = 1;
		for (int x = 0; x < 18 && !held; x++) {
			t.drag = 0;
			ev.mx = x;
			ktui_table_event(n, &t, 5, 6, 3, tc_col, &ev, NULL, NULL);
			held = t.drag ? x : 0;
		}
		eq_int(held, 9, "whose widened edge stays inside it to be held");
		t.drag = 0;
		eq_int(t.w[0], 21, "and the dragged width is kept for a wider rect");
	}

	/* ── page, selection rule and inset ──────────────────────── */
	{
		KtuiTable q = { 0 };
		int hit;

		q.page = KT_SURFACE;
		q.selrule = 1;
		q.inset = 1;
		for (int i = 0; i < 5; i++)
			tc_order[i] = i;
		ktui_table_draw(r, &q, 5, tc_col, 3, tc_cell, NULL, NULL, 3);
		ok(tc_cellat(0, 2)->bg == KT_DIM && tc_cellat(1, 2)->fg == KT_TEXT,
		   "selrule fills the selected row KT_DIM with KT_TEXT on it");
		ok(tc_cellat(0, 5)->bg == KT_SURFACE,
		   "and draws no hover plate");
		ok(tc_cellat(0, 7)->bg == KT_SURFACE &&
		   tc_cellat(29, 0)->bg == KT_SURFACE,
		   "the rows and the header stand on the page slot");
		eq_int((int)tc_ch(1, 2), 'p', "the inset moves the columns in");
		eq_int((int)tc_ch(1, 0), 'N', "the titles with them");
		ev.mx = 9;
		ev.my = 1;
		ev.press = KT_MP_PRESS;
		ktui_table_event(r, &q, 5, 6, 3, tc_col, &ev, NULL, NULL);
		eq_int(q.drag, 1, "and the edge handles with them");
		ev.mx = 0;
		ev.my = 4;
		hit = ktui_table_event(r, &q, 5, 6, 3, tc_col, &ev, NULL, NULL);
		ok(hit == KTUI_TABLE_MOVED && q.sel == 2,
		   "a press in the inset still lands on its row");
	}
}

/* ── ids that do not move ─────────────────────────────────────────────── */

static int idg_ids[8];

/* One page: a check, a group drawn only while `on`, a field, a button. */
static void idg_page(KtuiEvent *ev, int on, int scoped, char *a, char *b,
		     char *x)
{
	KRect r = krect(0, 0, 20, 1);
	int n = 0;

	ktui_frame_begin(ev);
	idg_ids[n++] = ktui_id_base();
	ktui_button(krect(0, 1, 6, 1), "Top", 1, 0);
	if (scoped)
		ktui_id_push("group");
	if (on) {
		idg_ids[n++] = ktui_id_base();
		ktui_input(r, a, 32, 0, NULL);
		idg_ids[n++] = ktui_id_base();
		ktui_input(r, b, 32, 0, NULL);
	}
	if (scoped)
		ktui_id_pop();
	idg_ids[6] = ktui_id_base();
	ktui_input(r, x, 32, 0, NULL);
	idg_ids[7] = ktui_id_base();
	ktui_button(krect(0, 3, 6, 1), "Next", 1, 1);
	ktui_frame_end();
}

static void test_ktui_ids(void)
{
	KtuiEvent ev;
	char a[32] = "", b[32] = "", x[32] = "";

	printf("libktui ids\n");
	ktui_offscreen_init(40, 6);
	ktui_draw_init();
	ev.type = KT_EVT_NONE;

	/* ── positional ids are what they were ───────────────────── */
	feed(&ev, 0);
	ev.type = KT_EVT_NONE;
	idg_page(&ev, 0, 0, a, b, x);
	ok(idg_ids[0] == 0 && idg_ids[6] == 1 && idg_ids[7] == 2,
	   "at depth 0 the n-th control is id n");
	idg_page(&ev, 1, 0, a, b, x);
	eq_int(idg_ids[6], 3,
	       "so an unscoped group renumbers the field after it");

	/* ── a scoped group leaves every id after it alone ───────── */
	idg_page(&ev, 0, 1, a, b, x);
	int x_off = idg_ids[6], next_off = idg_ids[7];
	idg_page(&ev, 1, 1, a, b, x);
	ok(idg_ids[6] == x_off && idg_ids[7] == next_off,
	   "a scoped group drawn or not, the field and button after it "
	   "keep their ids");
	ok(idg_ids[1] >= KTUI_ID_HASHED && idg_ids[2] >= KTUI_ID_HASHED &&
	   idg_ids[1] != idg_ids[2],
	   "the group's own ids are hashed, distinct, and above chrome");
	int ga = idg_ids[1];
	idg_page(&ev, 1, 1, a, b, x);
	eq_int(idg_ids[1], ga, "and the same on every frame");

	/* ── the caret stays with its field ──────────────────────── */
	kb_strlcpy(x, "abc", sizeof(x));
	ev.type = KT_EVT_NONE;
	idg_page(&ev, 0, 1, a, b, x);
	ktui_focus_set(x_off);
	feed(&ev, KT_K_END);
	idg_page(&ev, 0, 1, a, b, x);
	feed(&ev, KT_K_LEFT);
	idg_page(&ev, 0, 1, a, b, x);
	feed(&ev, KT_K_ENTER);	/* passed: nothing typed, nothing moved */
	idg_page(&ev, 1, 1, a, b, x);
	ktui_focus_set(x_off);
	feed(&ev, 'Z');
	idg_page(&ev, 1, 1, a, b, x);
	eq_str(x, "abZc",
	       "the field keeps its caret across the group appearing");
	ok(!a[0] && !b[0], "and the group's fields took nothing");

	/* ── Tab walks the ring in drawn order ───────────────────── */
	ev.type = KT_EVT_NONE;
	idg_page(&ev, 1, 1, a, b, x);	/* resolve onto this layout */
	ktui_focus_set(0);
	feed(&ev, KT_K_TAB);
	idg_page(&ev, 1, 1, a, b, x);
	eq_int(ktui_focus_get(), idg_ids[1], "Tab from the top reaches the group");
	idg_page(&ev, 1, 1, a, b, x);
	eq_int(ktui_focus_get(), idg_ids[2], "then its second field");
	idg_page(&ev, 1, 1, a, b, x);
	eq_int(ktui_focus_get(), idg_ids[6], "then the field after it");
	idg_page(&ev, 1, 1, a, b, x);
	idg_page(&ev, 1, 1, a, b, x);
	eq_int(ktui_focus_get(), 0, "and round to the top");
	feed(&ev, KT_K_BTAB);
	idg_page(&ev, 1, 1, a, b, x);
	eq_int(ktui_focus_get(), idg_ids[7], "Shift+Tab goes round backwards");

	/* ── a focused control that goes away ────────────────────── */
	ktui_focus_set(idg_ids[2]);
	ev.type = KT_EVT_NONE;
	idg_page(&ev, 1, 1, a, b, x);	/* seen at ring position 2 */
	idg_page(&ev, 0, 1, a, b, x);
	eq_int(ktui_focus_get(), idg_ids[7],
	       "focus on a vanished control goes to what stands in its place");
	ktui_focus_set(99);
	idg_page(&ev, 0, 1, a, b, x);
	eq_int(ktui_focus_get(), idg_ids[7],
	       "a positional focus past the page lands on the last control");

	/* ── named ids ───────────────────────────────────────────── */
	{
		int s1, s2, s3, s4, s5, s6, s7, s8, base;

		ktui_frame_begin(&ev);
		s1 = ktui_id_str("a");
		s2 = ktui_id_str("b");
		ktui_id_push("ab");
		s3 = ktui_id_str("c");
		base = ktui_id_base();
		s5 = ktui_id();
		ktui_id_pop();
		ktui_id_push("a");
		s4 = ktui_id_str("bc");
		ktui_id_pop();
		ktui_id_push_int(7);
		s6 = ktui_id_str("a");
		ktui_id_pop();
		/* The same four bytes, once a name and once a number: on a
		 * little-endian machine only the tag keeps the scopes apart. */
		ktui_id_push("abc");
		s7 = ktui_id_str("q");
		ktui_id_pop();
		ktui_id_push_int((int)0x00636261);
		s8 = ktui_id_str("q");
		ktui_id_pop();
		ktui_frame_end();
		ok(s1 != s2 && s1 >= KTUI_ID_HASHED,
		   "two names are two hashed ids");
		ok(s3 != s4, "a scope and a name never run into each other");
		ok(s6 != s1, "a numbered scope names its own ids");
		ok(s7 != s8, "a named scope and a numbered one never meet");
		eq_int(base, s5,
		       "inside a scope ktui_id_base is the id ktui_id takes next");
		ktui_frame_begin(&ev);
		ok(ktui_id_str("a") == s1, "a name is the same id every frame");
		ktui_frame_end();
	}

	/* ── the state store ─────────────────────────────────────── */
	{
		int *p, *q;
		int n = 0, full = 0;

		ktui_frame_begin(&ev);
		p = ktui_state(KTUI_ID_HASHED + 12345, sizeof(int));
		ok(p && *p == 0, "a first record is zeroed");
		*p = 42;
		ktui_frame_end();
		ktui_frame_begin(&ev);
		q = ktui_state(KTUI_ID_HASHED + 12345, sizeof(int));
		ok(q == p && *q == 42, "and handed back on the next frame");
		q = ktui_state(KTUI_ID_HASHED + 12345, 2 * sizeof(int));
		ok(q && q[0] == 0, "asked for at another size it starts over");
		ok(!ktui_state(1, KTUI_STATE_MAX + 1),
		   "a record over KTUI_STATE_MAX is refused");
		for (int i = 0; i < 300; i++) {
			if (!ktui_state(KTUI_ID_HASHED + 50000 + i, 4)) {
				full = 1;
				break;
			}
			n++;
		}
		ok(full && n <= 256, "a store of live records refuses, not grows");
		ktui_frame_end();
		for (int i = 0; i < 602; i++) {
			ktui_frame_begin(&ev);
			ktui_frame_end();
		}
		ktui_frame_begin(&ev);
		ok(ktui_state(KTUI_ID_HASHED + 99999, 4) != NULL,
		   "a record unseen for 600 frames gives its slot up");
		ktui_frame_end();
	}
	/* The focus is process state; the tests after this one start from
	 * the first control, as a surface's first frame does. */
	ktui_focus_set(0);
}

/* ── the layout cursor, the splitter and the fold ────────────────────── */

/* The first codepoint of a glyph-table entry, which is what a cell holds. */
static uint32_t lay_glyph(int g)
{
	uint32_t cp = 0;

	ktui_utf8_next(ktui_glyph[g], &cp);
	return cp;
}

static void lay_mouse(KtuiEvent *ev, int press, int x, int y)
{
	memset(ev, 0, sizeof(*ev));
	ev->type = KT_EVT_MOUSE;
	ev->btn = KT_MB_LEFT;
	ev->press = press;
	ev->mx = x;
	ev->my = y;
}

/* One frame holding one splitter; the panes come back in a and b. */
static int lay_split(KtuiEvent *ev, KRect r, int stack, int at, int min,
		     KRect *a, KRect *b)
{
	int moved;

	ktui_frame_begin(ev);
	moved = ktui_split(r, "sp", stack, at, min, a, b);
	ktui_frame_end();
	return moved;
}

/* A fold between a field and a button: its body is a field too. */
static int fold_after;
static int lay_fold(KtuiEvent *ev, int def, char *in, char *body)
{
	int open;

	ktui_frame_begin(ev);
	ktui_input(krect(0, 0, 20, 1), in, 32, 0, NULL);
	open = ktui_fold_begin(0, 1, 30, "More", def);
	if (open) {
		ktui_input(krect(2, 2, 20, 1), body, 32, 0, NULL);
		ktui_fold_end();
	}
	fold_after = ktui_id_base();
	ktui_button(krect(0, 4, 6, 1), "Go", 1, 1);
	ktui_frame_end();
	return open;
}

/* Two copies of one form: written out by hand as the installer's pages were,
 * and through the cursor. They must be the same cells. */
static void lay_form_hand(KRect b)
{
	int y = b.y;

	ktui_section(b.x, y, b.w, "MACHINE");
	y++;
	ktui_draw_text(b.x, y, 16, "hostname", KT_MID, KT_BG, 0);
	ktui_draw_text(b.x + 17, y, 10, "kdos", KT_TEXT, KT_BG, 0);
	y += 2;
	ktui_draw_text(b.x, y, 16, "password", KT_MID, KT_BG, 0);
	ktui_draw_text(b.x + 17, y, 10, "****", KT_TEXT, KT_BG, 0);
	y++;
	ktui_pw_meter(b.x + 17, y, b.w - 17, "hunter22");
	y++;
	ktui_note(b.x + 4, y, b.w, "an indented note");
}

static void lay_form_cursor(KRect b)
{
	KtuiLay l;
	KRect f;

	ktui_lay_begin(&l, b);
	ktui_lay_section(&l, "MACHINE");
	f = ktui_lay_field(&l, "hostname", 0);
	ktui_draw_text(f.x, f.y, 10, "kdos", KT_TEXT, KT_BG, 0);
	ktui_lay_gap(&l, 1);
	f = ktui_lay_field(&l, "password", 0);
	ktui_draw_text(f.x, f.y, 10, "****", KT_TEXT, KT_BG, 0);
	f = ktui_lay_field(&l, NULL, 0);
	ktui_pw_meter(f.x, f.y, f.w, "hunter22");
	ktui_lay_indent(&l, 0);
	f = ktui_lay_row(&l, 1);
	ktui_note(f.x, f.y, b.w, "an indented note");
}

static void test_ktui_layout(void)
{
	KtuiEvent ev;
	KtuiLay l;
	KRect r, a, b;
	int w = 0, h = 0;

	printf("libktui layout, splitter and fold\n");
	ktui_offscreen_init(40, 12);
	ktui_draw_init();
	ktui_draw_clear();

	/* ── the cursor ──────────────────────────────────────────── */
	ktui_lay_begin(&l, krect(2, 1, 36, 10));
	r = ktui_lay_row(&l, 1);
	ok(r.x == 2 && r.y == 1 && r.w == 36 && r.h == 1,
	   "a row is the page's width at the cursor");
	r = ktui_lay_row(&l, 0);
	ok(r.y == 2 && r.h == 1, "a row asked for 0 high is one row");
	ktui_lay_gap(&l, 2);
	r = ktui_lay_field(&l, "name", 0);
	ok(r.x == 2 + KTUI_LAY_LABEL + 1 && r.y == 5 && r.h == 1 &&
	   r.w == 36 - KTUI_LAY_LABEL - 1,
	   "a field's control starts one cell past the label column");
	ok(tc_ch(2, 5) == 'n' && tc_cellat(2, 5)->fg == KT_MID,
	   "and its label is drawn in KT_MID");
	r = ktui_lay_field(&l, NULL, 8);
	ok(r.x == 11 && tc_ch(2, 6) == ' ',
	   "a NULL label draws nothing and keeps the column asked for");
	ktui_lay_indent(&l, 0);
	r = ktui_lay_row(&l, 1);
	ok(r.x == 2 + KTUI_LAY_INDENT && r.w == 36 - KTUI_LAY_INDENT,
	   "an indent moves the row in and narrows it");
	ktui_lay_indent(&l, 3);
	ktui_lay_unindent(&l, 0);
	r = ktui_lay_row(&l, 1);
	eq_int(r.x, 5, "and an unindent takes back what it is asked to");
	ktui_lay_unindent(&l, 99);
	ktui_lay_indent(&l, 99);
	r = ktui_lay_left(&l);
	ok(r.x == 38 && r.w == 0, "an indent stops at the page's width");
	ktui_lay_unindent(&l, 99);
	r = ktui_lay_left(&l);
	ok(r.x == 2 && r.y == 9 && r.h == 2, "what is left is below the cursor");
	ktui_lay_gap(&l, 5);
	r = ktui_lay_left(&l);
	eq_int(r.h, 0, "and nothing once the cursor is past the bottom");
	r = ktui_lay_row(&l, 1);
	eq_int(r.y, 14, "a row past the page is still handed out where it is");
	{
		static const KtuiCol c[] = {
			{ NULL, 10, 0 }, { NULL, 0, 0 }, { NULL, 6, 0 },
		};
		KRect out[3];
		int x[3], cw[3], n;

		ktui_lay_begin(&l, krect(2, 0, 36, 4));
		ktui_lay_indent(&l, 4);
		n = ktui_lay_cols(&l, c, 3, 2, out);
		ktui_table_layout(c, 3, 32, x, cw);
		ok(n == 3 && out[0].x == 6 + x[0] && out[1].x == 6 + x[1] &&
		   out[2].x == 6 + x[2] && out[1].w == cw[1] &&
		   out[2].w == 6 && out[1].h == 2 &&
		   out[2].x + out[2].w == 38,
		   "columns are the table's columns, across the indented row");
		eq_int(ktui_lay_left(&l).y, 2, "and take the rows they asked for");
	}
	{
		const KtuiCell *c;
		KtuiCell hand[40 * 12];

		ktui_draw_clear();
		lay_form_hand(krect(1, 1, 38, 10));
		c = ktui_draw_cells(&w, &h);
		memcpy(hand, c, sizeof(hand));
		ktui_draw_clear();
		lay_form_cursor(krect(1, 1, 38, 10));
		c = ktui_draw_cells(&w, &h);
		ok(w == 40 && h == 12 && !memcmp(hand, c, sizeof(hand)),
		   "a form through the cursor draws the cells written by hand");
	}

	/* ── the splitter ────────────────────────────────────────── */
	ktui_draw_clear();
	ev.type = KT_EVT_NONE;
	r = krect(0, 0, 30, 5);
	lay_split(&ev, r, KT_SPLIT_SIDE, 10, 3, &a, &b);
	ok(a.x == 0 && a.w == 10 && b.x == 11 && b.w == 19 && a.h == 5 &&
	   b.h == 5, "side by side: the first pane, the divider, the rest");
	ok(tc_ch(10, 0) == lay_glyph(KT_G_VL) && tc_ch(10, 4) == lay_glyph(KT_G_VL) &&
	   tc_cellat(10, 2)->fg == KT_DIM, "the divider is a KT_DIM rule");
	lay_mouse(&ev, KT_MP_PRESS, 10, 2);
	lay_split(&ev, r, KT_SPLIT_SIDE, 10, 3, &a, &b);
	ok(tc_cellat(10, 2)->fg == KT_ACCENT, "held, it wears the accent");
	lay_mouse(&ev, KT_MP_DRAG, 15, 4);
	eq_int(lay_split(&ev, r, KT_SPLIT_SIDE, 10, 3, &a, &b), 1,
	       "a drag moves it");
	ok(a.w == 15 && b.x == 16, "to the pointer's column");
	lay_mouse(&ev, KT_MP_DRAG, 29, 9);
	lay_split(&ev, r, KT_SPLIT_SIDE, 10, 3, &a, &b);
	eq_int(b.w, 3, "no further than leaves the second pane its minimum");
	lay_mouse(&ev, KT_MP_DRAG, 0, 0);
	lay_split(&ev, r, KT_SPLIT_SIDE, 10, 3, &a, &b);
	eq_int(a.w, 3, "nor the first");
	lay_mouse(&ev, KT_MP_DRAG, 12, 0);
	lay_split(&ev, r, KT_SPLIT_SIDE, 10, 3, &a, &b);
	lay_mouse(&ev, KT_MP_RELEASE, 12, 0);
	lay_split(&ev, r, KT_SPLIT_SIDE, 10, 3, &a, &b);
	ev.type = KT_EVT_NONE;
	lay_split(&ev, r, KT_SPLIT_SIDE, 10, 3, &a, &b);
	eq_int(a.w, 12, "let go, it stays where it was left");
	lay_split(&ev, krect(0, 0, 10, 5), KT_SPLIT_SIDE, 10, 3, &a, &b);
	eq_int(a.w, 6, "a narrower rect holds it to the minimums");
	lay_split(&ev, r, KT_SPLIT_SIDE, 10, 3, &a, &b);
	eq_int(a.w, 12, "and the place comes back as it widens");
	lay_split(&ev, krect(0, 0, 4, 5), KT_SPLIT_SIDE, 10, 3, &a, &b);
	ok(a.w == 1 && b.w == 2, "with no room for both it halves what there is");
	{
		int id;

		ktui_frame_begin(&ev);
		id = ktui_id_str("sp");
		ktui_frame_end();
		ktui_focus_set(id);
		feed(&ev, KT_K_RIGHT);
		eq_int(lay_split(&ev, r, KT_SPLIT_SIDE, 10, 3, &a, &b), 1,
		       "Right moves the focused divider");
		ok(a.w == 13 && ktui_consumed(), "a cell, and takes the key");
		ok(ktui_announce_count() == 1 &&
		   ktui_announce_at(0)->role == KT_A11Y_SLIDER &&
		   !strcmp(ktui_announce_at(0)->value, "13"),
		   "and says where it is");
		feed(&ev, KT_K_UP);
		lay_split(&ev, r, KT_SPLIT_SIDE, 10, 3, &a, &b);
		ok(a.w == 13 && !ktui_consumed(),
		   "Up is not a side-by-side divider's key");
		feed(&ev, KT_K_HOME);
		lay_split(&ev, r, KT_SPLIT_SIDE, 10, 3, &a, &b);
		eq_int(a.w, 10, "Home puts it back at its default");
		feed(&ev, KT_K_DOWN);
		lay_split(&ev, r, KT_SPLIT_STACK, 2, 1, &a, &b);
		ok(a.h == 3 && a.w == 30 && b.y == 4 && b.h == 1,
		   "stacked: Down moves a horizontal divider");
		ok(tc_ch(5, 3) == lay_glyph(KT_G_HL), "drawn as a rule across");
		feed(&ev, KT_K_HOME);
		lay_split(&ev, r, KT_SPLIT_SIDE, -8, 3, &a, &b);
		ok(b.w == 8 && a.w == 21,
		   "a negative `at` is the second pane's size");
		feed(&ev, KT_K_LEFT);
		lay_split(&ev, r, KT_SPLIT_SIDE, -8, 3, &a, &b);
		ev.type = KT_EVT_NONE;
		lay_split(&ev, krect(0, 0, 40, 5), KT_SPLIT_SIDE, -8, 3, &a,
			  &b);
		eq_int(b.w, 9,
		       "and a moved one keeps that pane's size as the rect grows");
		lay_mouse(&ev, KT_MP_PRESS, 30, 1);
		lay_split(&ev, krect(0, 0, 40, 5), KT_SPLIT_SIDE, -8, 3, &a,
			  &b);
		lay_mouse(&ev, KT_MP_DRAG, 25, 1);
		lay_split(&ev, krect(0, 0, 40, 5), KT_SPLIT_SIDE, -8, 3, &a,
			  &b);
		lay_mouse(&ev, KT_MP_RELEASE, 25, 1);
		lay_split(&ev, krect(0, 0, 40, 5), KT_SPLIT_SIDE, -8, 3, &a,
			  &b);
		ev.type = KT_EVT_NONE;
		lay_split(&ev, krect(0, 0, 50, 5), KT_SPLIT_SIDE, -8, 3, &a,
			  &b);
		eq_int(b.w, 14, "so does a dragged one");
		ktui_focus_set(0);
	}

	/* ── the fold ────────────────────────────────────────────── */
	{
		char in[32] = "", body[32] = "";
		int after_shut, id;

		ktui_draw_clear();
		ev.type = KT_EVT_NONE;
		ok(!lay_fold(&ev, 0, in, body), "a fold starts as it is told");
		ok(tc_ch(0, 1) == lay_glyph(KT_G_ARROW_R) &&
		   tc_cellat(0, 1)->fg == KT_ACCENT && tc_ch(2, 1) == 'M',
		   "shut: the marker, then the title");
		ok(tc_ch(8, 1) == lay_glyph(KT_G_HL), "and the section rule after it");
		after_shut = fold_after;
		lay_mouse(&ev, KT_MP_PRESS, 5, 1);
		ok(lay_fold(&ev, 0, in, body), "a press opens it");
		ok(tc_ch(0, 1) == lay_glyph(KT_G_ARROW_DOWN),
		   "and the marker turns down");
		eq_int(fold_after, after_shut,
		       "its body comes and goes without moving the id after it");
		ev.type = KT_EVT_NONE;
		ok(lay_fold(&ev, 0, in, body), "it stays open");
		ktui_frame_begin(&ev);
		id = ktui_id_str("More");
		ktui_frame_end();
		ktui_focus_set(id);
		feed(&ev, KT_K_LEFT);
		ok(!lay_fold(&ev, 1, in, body) && ktui_consumed(),
		   "Left shuts the focused fold, over a default of open");
		ok(ktui_announce_count() == 1 &&
		   ktui_announce_at(0)->role == KT_A11Y_BUTTON &&
		   !strcmp(ktui_announce_at(0)->value, "collapsed"),
		   "and it says so");
		feed(&ev, KT_K_LEFT);
		lay_fold(&ev, 1, in, body);
		ok(!ktui_consumed(), "Left on a shut fold is not its key");
		feed(&ev, KT_K_RIGHT);
		ok(lay_fold(&ev, 0, in, body), "Right opens it");
		feed(&ev, KT_K_ENTER);
		ok(!lay_fold(&ev, 0, in, body), "Enter toggles it");
		feed(&ev, KT_K_TAB);
		lay_fold(&ev, 0, in, body);
		eq_int(ktui_focus_get(), fold_after, "Tab goes on past it");
		ktui_focus_set(0);
	}

	/* ── the hit list a developer overlay outlines ───────────── */
	{
		int id = -2;

		ev.type = KT_EVT_NONE;
		ktui_frame_begin(&ev);
		ktui_button(krect(3, 2, 6, 1), "One", 1, 0);
		ktui_hit_chrome(krect(0, 0, 2, 1), 4);
		ktui_frame_end();
		eq_int(ktui_hit_count(), 2, "a frame's hit rects are counted");
		ok(ktui_hit_at(0, &r, &id) && r.x == 3 && r.y == 2 && r.w == 6 &&
		   id == 0, "and each is its rect and its id");
		ok(ktui_hit_at(1, &r, &id) && id == KTUI_ID_CHROME + 4,
		   "chrome among them");
		ok(!ktui_hit_at(2, &r, &id), "nothing past the end");
		ktui_frame_begin(&ev);
		eq_int(ktui_hit_count(), 2, "the list stands through the next frame");
		ktui_frame_end();
		eq_int(ktui_hit_count(), 0, "until a frame that drew nothing ends");
		ktui_focus_set(0);
	}
}

static void test_rows(void)
{
	printf("libktui row and table pointers\n");

	KRect r = krect(2, 3, 30, 5);	/* five drawn rows, from index `top` */
	KtuiEvent ev = { 0 };
	int sel = 0, top = 0;

	ev.type = KT_EVT_MOUSE;
	ev.press = KT_MP_PRESS;
	ev.btn = KT_MB_LEFT;

	/* ── a press moves, a second press on the same row picks ────── */
	ev.mx = 5;
	ev.my = r.y + 2;
	eq_int(ktui_rows_event(r, &sel, &top, 20, &ev), KTUI_ROWS_MOVED,
	       "a press on another row moves the caret");
	eq_int(sel, 2, "to the row under the pointer");
	eq_int(ktui_rows_event(r, &sel, &top, 20, &ev), KTUI_ROWS_PICKED,
	       "and a press on the row it is already on picks");
	eq_int(sel, 2, "leaving it where it was");

	/* ── the row is measured from `top`, not from the rectangle ─── */
	top = 10;
	sel = 0;
	ktui_rows_event(r, &sel, &top, 20, &ev);
	eq_int(sel, 12, "a scrolled list names the row it is SHOWING");

	/* ── outside, and past the end ───────────────────────────── */
	ev.my = r.y + r.h + 1;
	eq_int(ktui_rows_event(r, &sel, &top, 20, &ev), KTUI_ROWS_NONE,
	       "a press below the rows is not the list's");
	top = 0;
	ev.my = r.y + 4;
	eq_int(ktui_rows_event(r, &sel, &top, 3, &ev), KTUI_ROWS_NONE,
	       "nor is one on a drawn line past the last row");

	/* ── the wheel walks and stops at the ends ───────────────── */
	sel = 0;
	ev.btn = KT_MB_WHEEL_DOWN;
	eq_int(ktui_rows_event(r, &sel, &top, 20, &ev), KTUI_ROWS_MOVED,
	       "a detent walks the caret");
	eq_int(sel, 1, "by one");
	ev.btn = KT_MB_WHEEL_UP;
	ktui_rows_event(r, &sel, &top, 20, &ev);
	eq_int(ktui_rows_event(r, &sel, &top, 20, &ev), KTUI_ROWS_NONE,
	       "and a detent at the end moves nothing, so it reports nothing");

	/* ── the right button is Back ─────────────────────────── */
	ev.btn = KT_MB_RIGHT;
	ev.my = r.y;
	eq_int(ktui_rows_event(r, &sel, &top, 20, &ev), KTUI_ROWS_CLOSE,
	       "the right button is Back, wherever it lands");

	/* ── and `follow` brings the caret back on screen ─────────── */
	top = 0;
	ktui_rows_follow(r, 12, &top, 20);
	ok(12 >= top && 12 < top + r.h,
	   "follow scrolls until the caret is one of the drawn rows");
	ktui_rows_follow(r, 0, &top, 20);
	eq_int(top, 0, "and back to the start for the first row");

	/* ── the table takes the same four answers ───────────────── */
	{
		static const KtuiCol col[] = { { "Name", 10, 0 } };
		KtuiTable tbl = { 0 };
		KRect tr = krect(2, 3, 20, 6);

		ev.type = KT_EVT_MOUSE;
		ev.press = KT_MP_PRESS;
		ev.btn = KT_MB_WHEEL_DOWN;
		eq_int(ktui_table_event(tr, &tbl, 20, 5, 1, col, &ev, NULL,
					NULL),
		       KTUI_TABLE_MOVED,
		       "a detent scrolls a table — which ten surfaces drew and "
		       "none answered");
		eq_int(tbl.sel, 1, "by one row");
		ev.btn = KT_MB_RIGHT;
		eq_int(ktui_table_event(tr, &tbl, 20, 5, 1, col, &ev, NULL,
					NULL),
		       KTUI_TABLE_CLOSE, "and its right button is Back too");
	}

	test_table_columns();
}

static void test_grid(void)
{
	printf("libktui grid\n");

	/* The three answers, and nothing else. */
	int only_three = 1;
	for (uint32_t cp = 0; cp <= 0x10ffff; cp++) {
		int w = ktui_wcwidth(cp);
		if (w < 0 || w > 2)
			only_three = 0;
	}
	ok(only_three, "wcwidth answers 0, 1 or 2 for every codepoint");

	ok(ktui_wcwidth('A') == 1 && ktui_wcwidth(' ') == 1 &&
	   ktui_wcwidth(0x7f) == 1, "ASCII is one cell");
	ok(ktui_wcwidth(0x00e9) == 1 && ktui_wcwidth(0x0410) == 1,
	   "Latin-1 and Cyrillic are one cell");
	ok(ktui_wcwidth(0x2500) == 1 && ktui_wcwidth(0x2591) == 1,
	   "the box-drawing and shade glyphs the VT font carries are one cell");
	ok(ktui_wcwidth(0x0301) == 0, "a combining acute takes no cell");
	ok(ktui_wcwidth(0x4e2d) == 2 && ktui_wcwidth(0xac00) == 2,
	   "CJK and Hangul are two cells");
	ok(ktui_wcwidth(0xff21) == 2, "a fullwidth Latin capital is two cells");
	ok(ktui_wcwidth(0x1f600) == 2, "an emoji is two cells");

	/*
	 * The bsearch precondition.
	 *
	 * cp_in() is a binary search, so both range tables must be sorted and
	 * disjoint — an out-of-order entry makes the ranges after it
	 * UNREACHABLE and the failure is silent, a CJK name that shears one
	 * row and not another. The tables are static to ktui_draw.c and cannot
	 * be walked from here, so the precondition is asserted the only way it
	 * can be from outside: one codepoint from the first, the last and
	 * several middle ranges of each table, which is what an unsorted table
	 * stops finding. A gap between two ranges is checked too, or a table
	 * that answered 2 for everything would pass.
	 */
	static const uint32_t two[] = {
		0x1100, 0x231a, 0x2648, 0x267f, 0x2b50, 0x3000, 0x3400,
		0x4e00, 0xa000, 0xac00, 0xf900, 0xfe30, 0xff01, 0xffe0,
		0x17000, 0x1f004, 0x1f191, 0x1f300, 0x1f680, 0x1f9d1,
		0x20000, 0x3fffd,
	};
	static const uint32_t zero[] = {
		0x0300, 0x0591, 0x06df, 0x0e31, 0x102d, 0x135f, 0x1712,
		0x1dc0, 0x200b, 0x20d0, 0x302a, 0xa806, 0xaab0, 0xfe00,
		0xfeff, 0x101fd, 0x11038, 0x1d167, 0xe0100,
	};
	/* Neither table claims these: the gaps a "return 2 for everything"
	 * table would swallow. */
	static const uint32_t one[] = {
		0x1160, 0x2000, 0x2318, 0x2600, 0x303f, 0xa4d0, 0xd800,
		0xfb00, 0xff61, 0x1f000, 0x1f700, 0x40000,
	};
	int t_ok = 1, z_ok = 1, o_ok = 1;
	for (size_t i = 0; i < sizeof(two) / sizeof(*two); i++)
		if (ktui_wcwidth(two[i]) != 2)
			t_ok = 0;
	for (size_t i = 0; i < sizeof(zero) / sizeof(*zero); i++)
		if (ktui_wcwidth(zero[i]) != 0)
			z_ok = 0;
	for (size_t i = 0; i < sizeof(one) / sizeof(*one); i++)
		if (ktui_wcwidth(one[i]) != 1)
			o_ok = 0;
	ok(t_ok, "every sampled wide range is still reachable by the bsearch");
	ok(z_ok, "and every sampled zero-width one");
	ok(o_ok, "while the gaps between them stay one cell");

	/* ktui_utf8_width is what every column in this toolkit measures with. */
	ok(ktui_utf8_width("abc") == 3, "utf8_width counts ASCII");
	ok(ktui_utf8_width("中文") == 4, "two CJK glyphs are four columns");
	/* e + combining acute is ONE column, not two: a name typed with a dead
	 * key must not push the column after it out of line. */
	ok(ktui_utf8_width("e\xcc\x81") == 1,
	   "a combining mark adds no column");
	ok(ktui_utf8_width("e\xcc\x81t\xc3\xa9") == 3,
	   "and a mix of composed and decomposed measures the same either way");
	ok(ktui_utf8_width("") == 0, "an empty string is no columns");

	/* ── the paste queue ─────────────────────────────────────────────── */

	/*
	 * There is no reader for the queue: the focused ktui_input takes it.
	 * So every assertion below pushes, draws one input, and reads the
	 * buffer — which is also the only path that matters.
	 */
	KtuiEvent ev;
	char buf[256];
	KRect r = krect(0, 0, 40, 1);

	ktui_offscreen_init(40, 3);
	feed(&ev, 0);
	ev.type = KT_EVT_NONE;

#define PASTE_INTO(src, len) do {                                        \
		buf[0] = 0;                                              \
		ktui_paste_push((src), (len));                           \
		ktui_frame_begin(&ev);                                   \
		ktui_focus_set(0);                                       \
		ktui_input(r, buf, sizeof(buf), 0, NULL);                \
		ktui_frame_end();                                        \
	} while (0)

	PASTE_INTO("hello", 5);
	eq_str(buf, "hello", "a plain paste reaches the focused input");

	/* A multi-line paste must not be able to fake an Enter. */
	PASTE_INTO("one\ntwo", 7);
	eq_str(buf, "one two", "a newline in a paste becomes a space");

	PASTE_INTO("a\x01\x02\tb\x7f", 6);
	eq_str(buf, "ab", "control bytes and DEL are stripped at the door");

	/* Stripping bytes below 0x20 can never split a UTF-8 sequence — a
	 * continuation byte is >= 0x80 — and this is the check that says so. */
	PASTE_INTO("caf\xc3\xa9\x01!", 7);
	eq_str(buf, "café!", "stripping a control byte leaves a sequence intact");

	/* The 4 KB cap, and the fragment it can leave behind. The queue is
	 * filled to one byte short of full with ASCII and then handed a
	 * three-byte sequence: two bytes fit, and a lead byte with no body
	 * would be a trailing fragment every later edit trips over. */
	{
		static char big[8192];
		memset(big, 'x', sizeof(big));
		PASTE_INTO(big, sizeof(big));
		size_t n = strlen(buf);
		ok(n == sizeof(buf) - 1,
		   "an oversized paste fills the input and stops");
		int clean = 1;
		for (size_t i = 0; i < n; i++)
			if (buf[i] != 'x')
				clean = 0;
		ok(clean, "and what it inserted is the paste, not the overflow");

		static char edge[4200];
		memset(edge, 'y', sizeof(edge));
		memcpy(edge + 4090, "\xe4\xb8\xad", 3);	/* 中, on the seam */
		buf[0] = 0;
		ktui_paste_push(edge, sizeof(edge));
		char sink[8192];
		sink[0] = 0;
		ktui_frame_begin(&ev);
		ktui_focus_set(0);
		ktui_input(krect(0, 0, 40, 1), sink, sizeof(sink), 0, NULL);
		ktui_frame_end();
		size_t m = strlen(sink);
		int trailing_lead = m > 0 && (unsigned char)sink[m - 1] >= 0xc0;
		ok(!trailing_lead,
		   "a sequence cut by the 4 KB cap is dropped, not half-kept");
	}
#undef PASTE_INTO

	/* ── the caret walks sequences, not bytes ────────────────────────── */

	/*
	 * S2's other half. The caret is a BYTE index into a UTF-8 buffer, so
	 * LEFT and BACKSPACE have to move a whole sequence or they leave a
	 * half-codepoint behind — which is what a lock screen refusing a
	 * password typed with a dead key looked like.
	 */
	kb_strlcpy(buf, "", sizeof(buf));
	ktui_paste_push("héllo中", strlen("héllo中"));
	ktui_frame_begin(&ev);
	ktui_focus_set(0);
	ktui_input(r, buf, sizeof(buf), 0, NULL);
	ktui_frame_end();
	eq_str(buf, "héllo中", "multibyte text reaches the buffer whole");

	/* One BACKSPACE removes the three-byte 中, not one byte of it. */
	feed(&ev, KT_K_BACKSPACE);
	ktui_frame_begin(&ev);
	ktui_focus_set(0);
	ktui_input(r, buf, sizeof(buf), 0, NULL);
	ktui_frame_end();
	eq_str(buf, "héllo", "backspace deletes a whole sequence");

	/* LEFT over the two-byte é, then BACKSPACE, must take the 'l' before
	 * it and leave the é standing. */
	for (int i = 0; i < 4; i++) {
		feed(&ev, KT_K_LEFT);
		ktui_frame_begin(&ev);
		ktui_focus_set(0);
		ktui_input(r, buf, sizeof(buf), 0, NULL);
		ktui_frame_end();
	}
	feed(&ev, KT_K_BACKSPACE);
	ktui_frame_begin(&ev);
	ktui_focus_set(0);
	ktui_input(r, buf, sizeof(buf), 0, NULL);
	ktui_frame_end();
	eq_str(buf, "éllo", "left moves by sequence, so the delete lands on one");

	/* ── the field trio, with no frame around it ────────────────────── */

	/*
	 * A surface with its own loop holds a KtuiField and calls the three.
	 * The frame control is a wrapper over them, so what is asserted here
	 * is also what ktui_input does — and the chord rule, which a frame
	 * test cannot see from the buffer alone, is asserted from both sides.
	 */
	{
		char fb[32] = "ab";
		KtuiField f = { fb, sizeof(fb), 99, 0, NULL };
		int rc;

		feed(&ev, 'c');
		rc = ktui_field_key(&f, &ev);
		eq_str(fb, "abc", "a caret past the end is clamped, then types");
		ok(rc == (KTUI_FIELD_USED | KTUI_FIELD_CHANGED),
		   "a typed key is the field's and changes it");
		feed(&ev, 's');
		ev.mods = KT_MOD_CTRL;
		ok(ktui_field_key(&f, &ev) == KTUI_FIELD_PASS &&
		   !strcmp(fb, "abc"), "a Ctrl chord is passed back, not typed");
		feed(&ev, KT_K_ENTER);
		ok(ktui_field_key(&f, &ev) == KTUI_FIELD_PASS,
		   "Enter is the surface's");
		feed(&ev, KT_K_LEFT);
		ev.mods = KT_MOD_ALT;
		ok(ktui_field_key(&f, &ev) == KTUI_FIELD_PASS && f.caret == 3,
		   "Alt+Left is the page's, not a caret move");
		kb_strlcpy(fb, "git commit", sizeof(fb));
		f.caret = 99;
		feed(&ev, 'w');
		ev.mods = KT_MOD_CTRL;
		ktui_field_key(&f, &ev);
		eq_str(fb, "git ", "Ctrl+W takes the word before the caret");
		feed(&ev, KT_K_LEFT);
		ev.mods = KT_MOD_CTRL;
		ktui_field_key(&f, &ev);
		ok(f.caret == 0, "Ctrl+Left walks back over a word");
		f.secret = 1;
		kb_strlcpy(fb, "a b", sizeof(fb));
		f.caret = 3;
		ktui_field_key(&f, &ev);
		ok(f.caret == 0, "a secret field is one word");
		f.secret = 0;
		feed(&ev, 'u');
		ev.mods = KT_MOD_CTRL;
		ktui_field_key(&f, &ev);
		eq_str(fb, "", "Ctrl+U clears the line");

		/* A paste is a queue, taken on a call with no event at all. */
		ktui_paste_push("x中", strlen("x中"));
		rc = ktui_field_key(&f, NULL);
		ok(rc == KTUI_FIELD_CHANGED && !strcmp(fb, "x中"),
		   "a paste lands on a call with no event, which it does not use");
		ok(ktui_field_col(&f) == 3, "the caret column counts a wide glyph twice");

		/* The press maps through the window the draw showed: a line
		 * wider than the field is scrolled to its caret, and a press
		 * on the first text cell lands on the first character shown. */
		kb_strlcpy(fb, "0123456789abcdef", sizeof(fb));
		f.caret = 99;
		ktui_field_draw(krect(0, 0, 8, 1), &f, 1, KT_SURFACE);
		ok(ktui_field_hit(krect(0, 0, 8, 1), &f, 2, 0) && f.caret == 11,
		   "a press lands on the character the scrolled field shows");
		ok(!ktui_field_hit(krect(0, 0, 8, 1), &f, 2, 1),
		   "and a press off its row is not the field's");
	}

	/* The frame control inherits both: Ctrl+U clears, and a chord it does
	 * not own is left for the page instead of being typed. */
	kb_strlcpy(buf, "abc", sizeof(buf));
	feed(&ev, 'u');
	ev.mods = KT_MOD_CTRL;
	ktui_frame_begin(&ev);
	ktui_focus_set(0);
	ok(ktui_input(r, buf, sizeof(buf), 0, NULL) && !buf[0],
	   "ktui_input clears on Ctrl+U");
	ktui_frame_end();
	feed(&ev, 's');
	ev.mods = KT_MOD_CTRL;
	ktui_frame_begin(&ev);
	ktui_focus_set(0);
	ktui_input(r, buf, sizeof(buf), 0, NULL);
	ok(!buf[0] && !ktui_consumed(), "and leaves Ctrl+S to the page");
	ktui_frame_end();

	/* ── modals ──────────────────────────────────────────────────────── */

	check_modal(44, 12);
	check_modal(30, 8);
}

/* ──────────────────────────────────────────────────────────────────────── */

static void vcmp(const char *a, const char *b, int want, const char *what)
{
	checks++;
	int got = kp_vercmp(a, b);
	if (got == want)
		return;
	failures++;
	printf("  FAIL  %s: vercmp(%s,%s) = %d, want %d\n", what, a, b, got, want);
}

static void vcmp_sym(const char *a, const char *b, int want, const char *what)
{
	vcmp(a, b, want, what);
	vcmp(b, a, -want, what);	/* a comparator that is not antisymmetric
					   is not an ordering */
}

/* Test helper: check if a candidate is present among the extracted ones. */
static int pu_extract_has(char cand[][PU_MAX_VER], int n, const char *want)
{
	for (int i = 0; i < n; i++)
		if (!strcmp(cand[i], want))
			return 1;
	return 0;
}

static void test_portup(void)
{
	printf("kdos-portup\n");

	/* Every one of these is a real version pair from the ports tree. A
	 * comparator that gets any of them wrong proposes a downgrade, and a
	 * downgrade costs a full rebuild to notice. */
	vcmp_sym("1.4rc5", "1.4", -1, "a release candidate precedes its release");
	vcmp_sym("1.3.0rc1", "1.3.0", -1, "rc with a dotted base");
	vcmp_sym("1.0pre4", "1.0", -1, "pre behaves like rc");
	vcmp_sym("10.2p1", "10.2", 1, "OpenSSH's p-suffix is an increment");
	vcmp_sym("1.9.17p2", "1.9.17p1", 1, "sudo's p-suffixes order numerically");
	vcmp_sym("3.6a", "3.6", 1, "tmux's letter suffix is an increment");
	vcmp_sym("20250826", "20250901", -1, "dates order as integers");
	vcmp_sym("1.10.0", "1.9.0", 1, "components are numeric, not lexical");
	vcmp_sym("r62", "r52", 1, "inih's rNN");
	vcmp_sym("8.17.0", "8.17.0", 0, "equal");
	vcmp_sym("1.4.0", "1.4", 1, "more components wins when the prefix ties");
	vcmp_sym("1.4.0", "1.4.0.0", -1, "a trailing component is significant, "
				       "as in dpkg and rpm");
	vcmp_sym("99999999999999999999999", "1.0", 1,
		 "a run too long for a long still orders as greater");
	vcmp_sym("99999999999999999999999", "99999999999999999999998", 1,
		 "two saturating runs order by their digits");
	vcmp_sym("007", "7", 0, "leading zeroes do not change the value");

	char sh[64];
	kp_vershape("1.4.0", sh, sizeof(sh));
	eq_str(sh, "N.N.N", "shape of a three-part version");
	kp_vershape("20250826", sh, sizeof(sh));
	eq_str(sh, "N8", "a lone digit run records its length");
	kp_vershape("1", sh, sizeof(sh));
	eq_str(sh, "N1", "so a date cannot match a bare 1");
	kp_vershape("r62", sh, sizeof(sh));
	eq_str(sh, "aN", "letter then digits");
	kp_vershape("10.2p1", sh, sizeof(sh));
	eq_str(sh, "N.NaN", "sudo/openssh p-suffix shape");
	kp_vershape("1.4rc5", sh, sizeof(sh));
	eq_str(sh, "N.NaN", "rc shares the p-suffix shape, which is fine: "
			    "shape only gates comparability");

	/* Real strings the adapters will hand it. */
	char cand[PU_MAX_CAND][PU_MAX_VER];
	int n;

	n = pu_extract("epoch-1.5.0", cand, PU_MAX_CAND);
	ok(n == 2, "epoch tag yields two (raw and cleaned)");
	ok(pu_extract_has(cand, n, "1.5.0"), "cleaned version is present");

	n = pu_extract("v2.1-20250901", cand, PU_MAX_CAND);
	ok(pu_extract_has(cand, n, "v2.1-20250901"), "luajit's tag yields the whole form");
	ok(pu_extract_has(cand, n, "2.1-20250901"), "the form without v prefix");
	ok(pu_extract_has(cand, n, "20250901"), "and the date component");

	n = pu_extract("cacert-2026-01-15.pem", cand, PU_MAX_CAND);
	ok(pu_extract_has(cand, n, "2026-01-15"), "a dated pem yields the whole date");

	n = pu_extract("fuse-3.19.0.tar.gz", cand, PU_MAX_CAND);
	ok(pu_extract_has(cand, n, "3.19.0"), "a tarball name yields its version");

	n = pu_extract("no-version-here", cand, PU_MAX_CAND);
	ok(n == 0, "a string with no version yields nothing");

	/* The extractor is deliberately generous and the SHAPE FILTER is what
	 * narrows it — that split is why no adapter needs to know about
	 * `epoch-` or `v2.1-` conventions. */
	n = pu_extract("Release_1_16_1", cand, PU_MAX_CAND);
	ok(n == 2, "underscores separate into digit runs");
	ok(pu_extract_has(cand, n, "1"), "including versions with repeated digits");
	ok(pu_extract_has(cand, n, "16"), "and longer ones");

	n = pu_extract("curl-8.12.1.tar.xz", cand, PU_MAX_CAND);
	ok(pu_extract_has(cand, n, "8.12.1"), ".tar.xz is trimmed — 129 ports use it");
	n = pu_extract("mesa-25.2.0.tar.bz2", cand, PU_MAX_CAND);
	ok(pu_extract_has(cand, n, "25.2.0"), ".tar.bz2 too — another 40 ports");
	n = pu_extract("zlib-1.3.1.tar.gz", cand, PU_MAX_CAND);
	ok(pu_extract_has(cand, n, "1.3.1"), "and .tar.gz still works");

	/* r62 is inih's real version; v1.2.3's v is a tag convention. The
	 * extractor emits both forms rather than guessing which letters
	 * belong to the version — the shape filter is what decides. */
	n = pu_extract("r62", cand, PU_MAX_CAND);
	ok(n >= 2, "a letter-prefixed run yields multiple forms");
	ok(pu_extract_has(cand, n, "r62"), "the run as written");
	ok(pu_extract_has(cand, n, "62"), "and without the prefix");

	/* Multi-hyphen package names were yielding prefix+rest (e.g., "conf-1.2.5.1")
	 * instead of the version alone. The new generative rules emit all useful
	 * forms and the shape filter picks the matching one. Cost a debug cycle:
	 * 15+ ports were broken; alsa-topology-conf, gst-plugins-{bad,base,good,ugly},
	 * desktop-file-utils, shared-mime-info, xcb-util-{cursor,image,renderutil}
	 * and others all reported unknown forever. */
	n = pu_extract("alsa-topology-conf-1.2.5.1.tar.bz2", cand, PU_MAX_CAND);
	ok(pu_extract_has(cand, n, "1.2.5.1"),
	   "a name with three hyphens still yields its version");
	n = pu_extract("gst-plugins-bad-1.28.0.tar.xz", cand, PU_MAX_CAND);
	ok(pu_extract_has(cand, n, "1.28.0"), "and a two-hyphen name");
	n = pu_extract("cacert-2026-01-15.pem", cand, PU_MAX_CAND);
	ok(pu_extract_has(cand, n, "2026-01-15"),
	   "while a hyphenated DATE survives intact");

	/* Rules must compose so that <name>-v<version> tarballs work. node-v22.14.0
	 * needs both "node-" stripping (rule 2) AND "v" stripping (rule 1b) to yield
	 * "22.14.0", so the transformations must iterate: applied once each, they
	 * miss the version of brotli, iso-codes, libglvnd, libslirp, libuv, lynx,
	 * nodejs, spirv-tools and upower. */
	n = pu_extract("node-v22.14.0.tar.xz", cand, PU_MAX_CAND);
	ok(pu_extract_has(cand, n, "22.14.0"),
	   "a <name>-v<version> tarball yields the bare version");
	n = pu_extract("upower-v1.90.9.tar.bz2", cand, PU_MAX_CAND);
	ok(pu_extract_has(cand, n, "1.90.9"), "and so does a .tar.bz2 one");
	n = pu_extract("alsa-topology-conf-1.2.5.1.tar.bz2", cand, PU_MAX_CAND);
	ok(pu_extract_has(cand, n, "1.2.5.1"), "without losing round 2's fix");

	/* LLVM uses .src convention; Debian uses .orig. Both must trim. */
	n = pu_extract("llvm-21.1.8.src.tar.xz", cand, PU_MAX_CAND);
	ok(pu_extract_has(cand, n, "21.1.8"),
	   "LLVM .src.tar.xz tarballs yield bare version");
	n = pu_extract("libaio_0.3.113.orig.tar.gz", cand, PU_MAX_CAND);
	ok(pu_extract_has(cand, n, "0.3.113"),
	   "Debian .orig.tar.gz tarballs yield bare version");

	/* No candidate may repeat: a duplicate means dedup ran before a
	 * transformation rather than after it. Release_1_16_1 yields repeated "1"
	 * before dedup, so this check genuinely exercises that invariant. */
	n = pu_extract("Release_1_16_1", cand, PU_MAX_CAND);
	ok(n >= 2, "the dedup check needs at least two candidates to mean anything");
	int dup = 0;
	for (int i = 0; i < n; i++)
		for (int k = i + 1; k < n; k++)
			if (!strcmp(cand[i], cand[k]))
				dup = 1;
	ok(!dup, "no candidate repeats");
}


/* ──────────────────────────────────────────────────────────────────────── *
 * libkpack — a pack is a filesystem with a footer, and the footer is the
 * only thing standing between a corrupt file and a mount as root.
 *
 * THE ONE RULE UNDER TEST IS "ABSENT, NEVER PARTIAL". Every malformed shape
 * below must read as "there is no pack here" rather than as a pack with some
 * of its fields filled in: a half-read description that looks complete is
 * what gets mounted.
 * ──────────────────────────────────────────────────────────────────────── */

/* A pack whose "filesystem" is arbitrary bytes. Nothing in libkpack looks
 * inside the image — mkfs.erofs is not a dependency of the library and is not
 * on the host running this. */
static char *fake_pack(const char *dir, const char *name, const char *meta,
		       const char *fs, const char *icon)
{
	char *img = kb_path_join(dir, "img.bin");
	char *out = kb_path_join(dir, name);
	KpkMeta m;

	kb_write_file(img, fs);
	kpk_meta_parse(meta, strlen(meta), &m);
	ok(kpk_write(out, img, &m, icon, icon ? strlen(icon) : 0) == 0,
	   "kpk_write assembles a pack");
	free(img);
	return out;
}

static void poke(const char *path, long long off, unsigned char v)
{
	FILE *f = fopen(path, "r+b");
	if (!f)
		return;
	fseeko(f, (off_t)off, off < 0 ? SEEK_END : SEEK_SET);
	fwrite(&v, 1, 1, f);
	fclose(f);
}

static void test_pack(void)
{
	printf("libkpack\n");

	char dir[] = "/tmp/kdos-selftest-pack.XXXXXX";
	ok(mkdtemp(dir) != NULL, "scratch directory");

	const char *meta =
		"id          = app.gimp\n"
		"kind        = app\n"
		"name        = GNU Image Manipulation Program\n"
		"version     = 3.0.4\n"
		"release     = 1\n"
		"summary     = Create images and edit photographs\n"
		"description = A raster editor.\n"
		"description = It has layers.\n"
		"category    = Graphics\n"
		"licence     = GPL-3.0-or-later\n"
		"requires    = rt-gtk >= 1\n"
		"requires    = base\n"
		"provides    = gimp\n"
		"desktop     = gimp.desktop\n"
		"mime        = image/png\n"
		"mime        = image/xcf\n"
		"command     = gimp\n"
		"launch_cold = 18300\n";

	/* ── round trip ─────────────────────────────────────────────── */
	char *pack = fake_pack(dir, "app.gimp.kpack", meta,
			       "not really erofs, and libkpack must not care",
			       "\x89PNG\r\n\x1a\n" "fake");
	KpkPack p;
	ok(kpk_open(pack, &p) == 0, "a written pack opens");
	eq_str(p.meta.id, "app.gimp", "id survives the round trip");
	eq_int(p.meta.kind, KPK_KIND_APP, "kind parses");
	eq_str(p.meta.version, "3.0.4", "version survives");
	eq_int(p.meta.nreq, 2, "both requirements parsed");
	eq_str(p.meta.req[0].name, "rt-gtk", "requirement name");
	eq_str(p.meta.req[0].op, ">=", "requirement operator");
	eq_str(p.meta.req[0].ver, "1", "requirement version");
	eq_str(p.meta.req[1].op, "", "a bare requirement has no operator");
	eq_int(p.meta.nmime, 2, "repeated keys accumulate");
	eq_str(p.meta.description, "A raster editor.\nIt has layers.",
	       "repeated description lines become paragraphs");
	eq_int(p.meta.launch_cold, 18300, "the measured cold launch");
	eq_int((long long)p.meta.size, (long long)p.fsize,
	       "size is what the file measures, not what it claims");

	size_t ilen = 0;
	void *icon = kpk_icon_read(&p, &ilen);
	ok(icon && ilen == 12 && !memcmp(icon, "\x89PNG", 4),
	   "the icon comes back byte for byte");
	free(icon);

	/* Rendering and re-parsing must yield the same struct, or a pack
	 * cannot survive `extract-meta` and a rebuild. */
	size_t rlen = 0;
	char *rendered = kpk_meta_render(&p.meta, &rlen);
	KpkMeta again;
	kpk_meta_parse(rendered, rlen, &again);
	eq_str(again.id, p.meta.id, "render/parse round trip: id");
	eq_int(again.nmime, p.meta.nmime, "render/parse round trip: mime list");
	eq_str(again.description, p.meta.description,
	       "render/parse round trip: description");
	free(rendered);

	/* ── the payload hash and the signature ─────────────────────── */
	KsigRing ring = {0};
	char who[KSIG_ID_HEX];
	eq_int(kpk_verify(&p, &ring, who), KPK_SIG_NONE,
	       "a pack nobody signed is unsigned, not bad");

	uint8_t seed[KSIG_SEED_LEN], pub[KSIG_PUB_LEN];
	if (ksig_keygen(seed, pub) == 0) {
		char *keydir = kb_path_join(dir, "keys");
		kb_mkdir_p(keydir);
		char *kf = kb_path_join(keydir, "b.pub");
		ksig_write_public(kf, pub, "builder");
		ok(kpk_sign(pack, seed, pub) == 0, "a pack signs");
		ok(kpk_open(pack, &p) == 0, "a signed pack still opens");
		ksig_ring_load(&ring, keydir);
		eq_int(kpk_verify(&p, &ring, who), KPK_SIG_GOOD,
		       "and verifies against the ring");

		/*
		 * THREE WAYS TO FAIL A SIGNATURE AND THEY ARE DIFFERENT
		 * QUESTIONS. An EMPTY ring is a fact about this machine — it
		 * holds no key that could check anything — and reporting it as
		 * a bad signature sends the reader to inspect the artefact
		 * instead of the drawer. A ring holding SOMEBODY ELSE'S key is
		 * a fact about the pack: a block that no trusted key verifies.
		 * Both are refused; only one of them is about the pack.
		 */
		KsigRing empty = {0};
		eq_int(kpk_verify(&p, &empty, who), KPK_SIG_NOKEY,
		       "an empty ring is 'no key here', not a bad signature");

		uint8_t oseed[KSIG_SEED_LEN], opub[KSIG_PUB_LEN];
		if (ksig_keygen(oseed, opub) == 0) {
			char *odir = kb_path_join(dir, "otherkeys");
			kb_mkdir_p(odir);
			char *of = kb_path_join(odir, "other.pub");
			ksig_write_public(of, opub, "stranger");
			KsigRing other = {0};
			ksig_ring_load(&other, odir);
			eq_int(kpk_verify(&p, &other, who), KPK_SIG_BAD,
			       "a signature by a key nobody trusts is refused");
			free(of);
			free(odir);
		}

		/* One byte of the filesystem, flipped. The hash is checked
		 * BEFORE the signature, so this is KPK_SIG_HASH — a caller
		 * that saw KPK_SIG_BAD here would go looking for the wrong
		 * problem. */
		poke(pack, 4, 'X');
		ok(kpk_open(pack, &p) == 0, "a tampered pack still opens");
		eq_int(kpk_verify(&p, &ring, who), KPK_SIG_HASH,
		       "one flipped payload byte fails the hash, not the signature");

		/*
		 * A SUBDIRECTORY IS A REAL BOUNDARY BETWEEN KEYRINGS, and two
		 * of them depend on it: /etc/kdos/keys is kpkg's binhost ring
		 * and /etc/kdos/keys/packs is kdos-packd's. A key that attests
		 * "these packs came off this medium" must not thereby become a
		 * trusted publisher of HOST packages, and the only thing
		 * keeping those apart is that the loader does not descend.
		 */
		char *subdir = kb_path_join(keydir, "packs");
		kb_mkdir_p(subdir);
		char *sub = kb_path_join(subdir, "scoped.pub");
		ksig_write_public(sub, pub, "scoped");
		KsigRing outer = {0}, inner = {0};
		char *bare = kb_path_join(dir, "keys2");
		kb_mkdir_p(bare);
		char *bsub = kb_path_join(bare, "packs");
		kb_mkdir_p(bsub);
		char *bkey = kb_path_join(bsub, "scoped.pub");
		ksig_write_public(bkey, pub, "scoped");
		eq_int(ksig_ring_load(&outer, bare), 0,
		       "a keyring does not descend into a subdirectory");
		eq_int(ksig_ring_load(&inner, bsub), 1,
		       "and the subdirectory is a ring of its own");
		free(bkey); free(bsub); free(bare); free(sub); free(subdir);
		free(kf);
		free(keydir);
	}
	free(pack);

	/* ── absent, never partial ──────────────────────────────────── */
	char *p2 = fake_pack(dir, "b.kpack", "id = base\nkind = base\nversion = 1\n",
			     "fs", NULL);
	KpkFooter f;

	ok(kpk_footer_read(p2, &f, NULL) == 0, "a good footer reads");

	/* the magic */
	poke(p2, -512, 'X');
	ok(kpk_footer_read(p2, &f, NULL) != 0, "a wrong magic is refused");
	poke(p2, -512, 'K');
	ok(kpk_footer_read(p2, &f, NULL) == 0, "and reads again once restored");

	/* a footer whose offsets point past the end of the file. Without the
	 * consistency check this is a read of the whole address space: a length
	 * taken from the file and never measured against the file. */
	{
		uint8_t buf[KPK_FOOTER_LEN];
		KpkFooter bad = f;
		bad.meta_len = (uint64_t)1 << 62;
		kpk_footer_pack(&bad, buf);
		FILE *fp = fopen(p2, "r+b");
		fseeko(fp, -(off_t)KPK_FOOTER_LEN, SEEK_END);
		fwrite(buf, 1, sizeof(buf), fp);
		fclose(fp);
		ok(kpk_footer_read(p2, &f, NULL) != 0,
		   "an offset past the end of the file is refused");
	}

	/* a truncated footer */
	{
		char *t = kb_path_join(dir, "short.kpack");
		kb_write_file(t, "KDOSPACK");
		ok(kpk_footer_read(t, &f, NULL) != 0,
		   "a file too short to hold a footer is absent, not partial");
		free(t);
	}

	/*
	 * A PACK FROM AN EARLIER FORMAT STILL VERIFIES, and `restamp` is what
	 * raises it. Widening the digest's span without moving the format
	 * number made every pack already published answer HASH — kdos-packd
	 * mounts none of them, no box composes, and each application opens a
	 * window that closes itself. The span is the pack's to declare.
	 */
	{
		char *old = kb_path_join(dir, "fmt1.kpack");
		char *img = kb_path_join(dir, "img.bin");
		uint8_t buf[KPK_FOOTER_LEN];
		KpkFooter o;
		KpkPack op;
		KpkMeta m;
		char h1[65];

		kpk_meta_parse("id = old\nkind = app\nversion = 1\n", 31, &m);
		kb_write_file(img, "payload");
		kpk_write(old, img, &m, NULL, 0);

		/* Back to format 1: the number, and the digest over the
		 * payload alone that a format 1 writer would have left. */
		kpk_footer_read(old, &o, NULL);
		o.format = 1;
		ok(kpk_payload_hash(old, &o, h1) == 0,
		   "a format 1 digest is taken over the payload alone");
		for (int i = 0; i < 32; i++) {
			unsigned v;
			sscanf(h1 + i * 2, "%2x", &v);
			o.payload_sha256[i] = (uint8_t)v;
		}
		kpk_footer_pack(&o, buf);
		FILE *fp = fopen(old, "r+b");
		fseeko(fp, -(off_t)KPK_FOOTER_LEN, SEEK_END);
		fwrite(buf, 1, sizeof(buf), fp);
		fclose(fp);

		ok(kpk_open(old, &op) == 0, "a format 1 pack still opens");
		eq_int((int)kpk_verify(&op, NULL, NULL), (int)KPK_SIG_NONE,
		       "and verifies — an older format is read, not refused");

		eq_int(kpk_restamp(old), 0, "restamp raises it to this format");
		ok(kpk_footer_read(old, &o, NULL) == 0 &&
		   o.format == KPK_FORMAT,
		   "and the footer declares the format it was hashed under");
		ok(kpk_open(old, &op) == 0, "the raised pack opens");
		eq_int((int)kpk_verify(&op, NULL, NULL), (int)KPK_SIG_NONE,
		       "and verifies under the wider span");
		eq_int(kpk_restamp(old), 1, "a second restamp leaves it alone");

		free(img);
		free(old);
	}

	/* a format from the future */
	{
		char *fut = kb_path_join(dir, "future.kpack");
		char *img = kb_path_join(dir, "img.bin");
		KpkMeta m;
		uint8_t buf[KPK_FOOTER_LEN];
		KpkFooter ff;
		kpk_meta_parse("id = x\nkind = app\nversion = 1\n", 30, &m);
		kb_write_file(img, "fs");
		kpk_write(fut, img, &m, NULL, 0);
		kpk_footer_read(fut, &ff, NULL);
		ff.format = KPK_FORMAT + 1;
		kpk_footer_pack(&ff, buf);
		FILE *fp = fopen(fut, "r+b");
		fseeko(fp, -(off_t)KPK_FOOTER_LEN, SEEK_END);
		fwrite(buf, 1, sizeof(buf), fp);
		fclose(fp);
		ok(kpk_footer_read(fut, &ff, NULL) != 0,
		   "a format this build does not know is not guessed at");
		free(img);
		free(fut);
	}
	free(p2);

	/* ── the metadata parser's own edges ────────────────────────── */
	{
		KpkMeta m;
		/* NOT NUL-terminated: the blob on disk is a span, and the
		 * parser is bounded by its length. Passing a shorter length
		 * must cut the value, never read past it. */
		const char *blob = "id = a\nversion = 9.9.9\nkind = app\n";
		kpk_meta_parse(blob, 6, &m);
		eq_str(m.id, "a", "a length-bounded parse stops where told");
		eq_str(m.version, "", "and does not see past the span");

		char big[2048];
		memset(big, 'x', sizeof(big));
		memcpy(big, "summary = ", 10);
		big[sizeof(big) - 1] = '\n';
		kpk_meta_parse(big, sizeof(big), &m);
		eq_str(m.summary, "",
		       "a line longer than the buffer is dropped, not halved");

		const char *unk = "id = a\nkind = app\nversion = 1\n"
				  "nonsense = 3\nno-equals-here\n";
		kpk_meta_parse(unk, strlen(unk), &m);
		eq_str(m.id, "a", "an unknown key is ignored, not fatal");

		/* An env line is a variable a daemon will export. */
		const char *envs = "id = d\nkind = data\nversion = 1\n"
				   "env = PROJ_DATA=/x\nenv = BAD NAME=1\n"
				   "env = =nope\n";
		kpk_meta_parse(envs, strlen(envs), &m);
		eq_int(m.nenv, 1, "only a well-formed env line is kept");
		eq_str(m.env[0], "PROJ_DATA=/x", "and it is the right one");
	}

	/* ── what kpk_meta_valid refuses ────────────────────────────── */
	{
		KpkMeta m;
		char err[256];

		kpk_meta_parse("id = ../etc\nkind = app\nversion = 1\n",
			       strlen("id = ../etc\nkind = app\nversion = 1\n"), &m);
		ok(kpk_meta_valid(&m, err, sizeof(err)) != 0,
		   "an id that is a path is refused");

		kpk_meta_parse("id = app.x\nkind = app\nversion = 1\n",
			       strlen("id = app.x\nkind = app\nversion = 1\n"), &m);
		ok(kpk_meta_valid(&m, err, sizeof(err)) == 0, "a plain app is valid");

		/* A data pack is mounted noexec, so a command in one names
		 * something that can never run. */
		const char *dcmd = "id = d.x\nkind = data\nversion = 1\n"
				   "command = thing\n";
		kpk_meta_parse(dcmd, strlen(dcmd), &m);
		ok(kpk_meta_valid(&m, err, sizeof(err)) != 0,
		   "a data pack carrying a command is refused");

		const char *dgr = "id = d.y\nkind = data\nversion = 1\n"
				  "graft = share ../../etc\n";
		kpk_meta_parse(dgr, strlen(dgr), &m);
		ok(kpk_meta_valid(&m, err, sizeof(err)) != 0,
		   "a graft that escapes its root is refused");

		/* env is NOT a data-pack key: which QT_QPA_PLATFORMTHEME works
		 * is a fact about the runtime that installed the platform
		 * theme, so the runtime is what declares it. graft and
		 * boxgraft stay data-only. */
		const char *renv = "id = rt-kde\nkind = runtime\nversion = 1\n"
				   "env = QT_QPA_PLATFORMTHEME=kde\n";
		kpk_meta_parse(renv, strlen(renv), &m);
		ok(kpk_meta_valid(&m, err, sizeof(err)) == 0,
		   "a runtime may declare env");
		eq_str(m.env[0], "QT_QPA_PLATFORMTHEME=kde",
		       "and it survives the parse");

		const char *rgr = "id = rt-x\nkind = runtime\nversion = 1\n"
				  "graft = share here\n";
		kpk_meta_parse(rgr, strlen(rgr), &m);
		ok(kpk_meta_valid(&m, err, sizeof(err)) != 0,
		   "but a runtime may not graft");
	}

	/* ── the solve ──────────────────────────────────────────────── */
	{
		KpkMeta store[5];
		const KpkMeta *av[5];
		const char *src[5] = {
			"id = base\nkind = base\nversion = 1\n",
			"id = rt-gtk\nkind = runtime\nversion = 2\nrequires = base\n",
			"id = app.gimp\nkind = app\nversion = 3\n"
				"requires = rt-gtk >= 1\nrequires = base\n",
			"id = app.old\nkind = app\nversion = 1\n"
				"requires = rt-gtk >= 9\n",
			"id = rt-alias\nkind = runtime\nversion = 1\n"
				"provides = toolkit\n",
		};
		for (int i = 0; i < 5; i++) {
			kpk_meta_parse(src[i], strlen(src[i]), &store[i]);
			av[i] = &store[i];
		}

		int order[8];
		char err[256];
		const char *want1[] = { "app.gimp" };
		int n = kpk_solve(av, 5, want1, 1, order, 8, err, sizeof(err));
		eq_int(n, 3, "the solve pulls in both runtimes");
		eq_str(av[order[0]]->id, "base", "base is mounted first");
		eq_str(av[order[2]]->id, "app.gimp", "and the app last");

		const char *want2[] = { "app.old" };
		eq_int(kpk_solve(av, 5, want2, 1, order, 8, err, sizeof(err)), -1,
		       "a requirement nothing satisfies is refused");
		ok(strstr(err, "rt-gtk") != NULL, "and the message names it");

		/* `provides` satisfies a requirement; an INSTALL REQUEST does
		 * not resolve through it, or `install gimp` could quietly
		 * install whatever claimed the name. */
		KpkReq r = { .name = "toolkit" };
		ok(kpk_req_met(&r, av[4]), "a provides name satisfies a requirement");
		const char *want3[] = { "toolkit" };
		eq_int(kpk_solve(av, 5, want3, 1, order, 8, err, sizeof(err)), -1,
		       "but a request names an id, never a provides");
	}

	/* ── the index ──────────────────────────────────────────────── */
	{
		KpkIndex ix = {0};
		char *ipath = kb_path_join(dir, "PACKAGES");

		ix.n = 2;
		kb_strlcpy(ix.ent[0].id, "rt-gtk", KPK_ID_MAX);
		kb_strlcpy(ix.ent[0].version, "2", 64);
		kb_strlcpy(ix.ent[0].release, "1", 16);
		kb_strlcpy(ix.ent[0].file, "rt-gtk.kpack", KPK_PATH);
		memset(ix.ent[0].sha256, 'a', 64);
		ix.ent[0].size = 410;
		kb_strlcpy(ix.ent[1].id, "app.gimp", KPK_ID_MAX);
		kb_strlcpy(ix.ent[1].version, "3.0.4", 64);
		kb_strlcpy(ix.ent[1].release, "1", 16);
		kb_strlcpy(ix.ent[1].file, "app.gimp.kpack", KPK_PATH);
		memset(ix.ent[1].sha256, 'b', 64);
		ix.ent[1].size = 96;

		ix.ent[1].recommended = 1;
		ok(kpk_index_write(&ix, ipath) == 0, "an index writes");
		KpkIndex back = {0};
		eq_int(kpk_index_load(&back, ipath), 2, "and reads back");
		eq_str(back.ent[0].id, "app.gimp",
		       "sorted by id, so the same set is always the same bytes");
		ok(kpk_index_find(&back, "rt-gtk") != NULL, "lookup by id");
		/* `R:` is in the index and not only in the pack, so kinstall —
		 * which links libkbase and libktui and nothing else — can
		 * answer "what does KDOS suggest" from a flat file. */
		ok(back.ent[0].recommended && !back.ent[1].recommended,
		   "the recommended flag round-trips through the index");
		ok(kpk_index_find(&back, "nope") == NULL, "and a miss is a miss");

		/* A version carrying a dash: the release is after the LAST one. */
		kb_write_file(ipath, "P:x\nV:1.2-rc1-4\nC:cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc\nF:x.kpack\n\n");
		kpk_index_load(&back, ipath);
		eq_str(back.ent[0].version, "1.2-rc1", "version keeps its dash");
		eq_str(back.ent[0].release, "4", "the release is the last field");

		/* A stanza with no hash cannot be verified, so it is dropped
		 * rather than recorded as an entry nothing can check. */
		kb_write_file(ipath, "P:x\nV:1-1\nF:x.kpack\n\nP:y\nV:1-1\n"
				     "C:dddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddd\nF:y.kpack\n\n");
		eq_int(kpk_index_load(&back, ipath), 1,
		       "a stanza with no hash is dropped, not half-recorded");
		eq_str(back.ent[0].id, "y", "and it is the one that had one");
		free(ipath);
	}
}

/* ──────────────────────────────────────────────────────────────────────── */

/* ──────────────────────────────────────────────────────────────────────── */

/*
 * The gesture recogniser.
 *
 * DRIVEN, NOT SLEPT THROUGH. Every timestamp here is supplied on the event,
 * which is the whole reason the recogniser reads no clock: a long press is
 * asserted in microseconds instead of half a second, and the suite stays a
 * pure function of its input.
 *
 * Both halves are checked on every scenario — the gesture, and the ordinary
 * mouse event synthesised beneath it — because a widget that never heard of
 * touch is supposed to keep working, and that only holds if the synthesis does.
 */

static KtuiEvent touch_ev(int phase, int slot, int x, int y, unsigned ms)
{
	KtuiEvent ev;

	memset(&ev, 0, sizeof(ev));
	ev.type = KT_EVT_TOUCH;
	ev.phase = phase;
	ev.slot = slot;
	ev.mx = x;
	ev.my = y;
	ev.ms = ms;
	return ev;
}

static void test_gesture(void)
{
	printf("\n==> touch becomes a gesture, and a mouse\n");

	KtuiEvent ev, m;
	KtuiGesture g;
	int have = 0;

	/* start_edge() reads the grid, so the scenarios below need one. */
	ktui_w = 80;
	ktui_h = 24;

	/* ── a tap ─────────────────────────────────────────────────────── */
	ktui_gesture_reset();
	ev = touch_ev(KT_TOUCH_DOWN, 0, 10, 5, 1000);
	ok(!ktui_gesture_feed(&ev, &g, &m, &have),
	   "a finger down is not yet a gesture");
	ok(have && m.type == KT_EVT_MOUSE && m.btn == KT_MB_LEFT
	   && m.press == KT_MP_PRESS,
	   "a finger down synthesises a left press");
	eq_int(m.mx, 10, "the synthesised press is where the finger is");

	ev = touch_ev(KT_TOUCH_UP, 0, 10, 5, 1100);
	ok(ktui_gesture_feed(&ev, &g, &m, &have),
	   "a short press in one cell reports");
	eq_int(g.type, KT_GEST_TAP, "and it is a tap");
	eq_int(g.fingers, 1, "a tap is one finger");
	eq_int(g.x, 10, "the tap is at the cell it went down in");
	ok(have && m.press == KT_MP_RELEASE,
	   "and a release is synthesised under it");

	/* ── held too long is not a tap ────────────────────────────────── */
	ktui_gesture_reset();
	ev = touch_ev(KT_TOUCH_DOWN, 0, 10, 5, 0);
	ktui_gesture_feed(&ev, &g, NULL, NULL);
	ev = touch_ev(KT_TOUCH_UP, 0, 10, 5, KT_TAP_MS);
	ok(!ktui_gesture_feed(&ev, &g, NULL, NULL),
	   "a press held past the tap window is not a tap");

	/* ── a move inside the cell says nothing ───────────────────────── */
	ktui_gesture_reset();
	ev = touch_ev(KT_TOUCH_DOWN, 0, 10, 5, 0);
	ktui_gesture_feed(&ev, &g, NULL, NULL);
	ev = touch_ev(KT_TOUCH_MOVE, 0, 10, 5, 10);
	ok(!ktui_gesture_feed(&ev, &g, NULL, NULL),
	   "a move that stays in the cell is not a gesture");

	ev = touch_ev(KT_TOUCH_MOVE, 0, 12, 5, 20);
	ok(ktui_gesture_feed(&ev, &g, &m, &have), "leaving the cell reports");
	eq_int(g.type, KT_GEST_DRAG, "and it is a drag");
	eq_int(g.dx, 2, "the drag carries the cells moved");
	ok(have && m.press == KT_MP_DRAG,
	   "a drag synthesises a dragging pointer");

	/* A drag disqualifies the tap that would otherwise follow. */
	ev = touch_ev(KT_TOUCH_UP, 0, 12, 5, 30);
	ok(!ktui_gesture_feed(&ev, &g, NULL, NULL),
	   "a finger that moved does not tap when it lifts");

	/* ── an edge swipe is a drag that started against an edge ──────── */
	ktui_gesture_reset();
	ev = touch_ev(KT_TOUCH_DOWN, 0, 0, 5, 0);
	ktui_gesture_feed(&ev, &g, NULL, NULL);
	ev = touch_ev(KT_TOUCH_MOVE, 0, 4, 5, 10);
	ok(ktui_gesture_feed(&ev, &g, NULL, NULL),
	   "a drag from the edge reports");
	eq_int(g.type, KT_GEST_SWIPE_EDGE, "and it is an edge swipe");
	eq_int(g.edge, KT_K_LEFT, "naming the edge it came from");

	ktui_gesture_reset();
	ev = touch_ev(KT_TOUCH_DOWN, 0, ktui_w - 1, 5, 0);
	ktui_gesture_feed(&ev, &g, NULL, NULL);
	ev = touch_ev(KT_TOUCH_MOVE, 0, ktui_w - 5, 5, 10);
	ktui_gesture_feed(&ev, &g, NULL, NULL);
	eq_int(g.edge, KT_K_RIGHT, "the far edge is the right one");

	/* ── a long press has no event of its own ──────────────────────── */
	ktui_gesture_reset();
	ev = touch_ev(KT_TOUCH_DOWN, 0, 10, 5, 0);
	ktui_gesture_feed(&ev, &g, NULL, NULL);
	ok(!ktui_gesture_tick(KT_LONG_MS - 1, &g),
	   "a press short of the long window is not a long press");
	ok(ktui_gesture_tick(KT_LONG_MS, &g), "held past it, it is");
	eq_int(g.type, KT_GEST_LONG, "and it says so");
	ok(!ktui_gesture_tick(KT_LONG_MS + 100, &g),
	   "a long press is reported once, not once per tick");
	ev = touch_ev(KT_TOUCH_UP, 0, 10, 5, KT_LONG_MS + 200);
	ok(!ktui_gesture_feed(&ev, &g, NULL, NULL),
	   "and no tap follows the long press");

	/* A finger that has moved never becomes a long press. */
	ktui_gesture_reset();
	ev = touch_ev(KT_TOUCH_DOWN, 0, 10, 5, 0);
	ktui_gesture_feed(&ev, &g, NULL, NULL);
	ev = touch_ev(KT_TOUCH_MOVE, 0, 14, 5, 10);
	ktui_gesture_feed(&ev, &g, NULL, NULL);
	ok(!ktui_gesture_tick(KT_LONG_MS, &g),
	   "a finger that moved does not become a long press");

	/* ── two fingers: nothing is decided by one of them ────────────── */
	ktui_gesture_reset();
	ev = touch_ev(KT_TOUCH_DOWN, 0, 10, 10, 0);
	ktui_gesture_feed(&ev, &g, NULL, NULL);
	ev = touch_ev(KT_TOUCH_DOWN, 1, 20, 10, 0);
	ktui_gesture_feed(&ev, &g, NULL, NULL);

	ev = touch_ev(KT_TOUCH_MOVE, 0, 10, 14, 10);
	ok(!ktui_gesture_feed(&ev, &g, NULL, NULL),
	   "one finger moving while the other has not is undecided");
	ev = touch_ev(KT_TOUCH_MOVE, 1, 20, 14, 20);
	ok(ktui_gesture_feed(&ev, &g, &m, &have),
	   "the second finger decides it");
	eq_int(g.type, KT_GEST_SCROLL, "two fingers the same way is a scroll");
	eq_int(g.fingers, 2, "and it says how many");
	ok(have && m.btn == KT_MB_WHEEL_DOWN,
	   "a downward scroll synthesises a wheel");

	/* ── two fingers apart is a pinch ──────────────────────────────── */
	ktui_gesture_reset();
	ev = touch_ev(KT_TOUCH_DOWN, 0, 10, 10, 0);
	ktui_gesture_feed(&ev, &g, NULL, NULL);
	ev = touch_ev(KT_TOUCH_DOWN, 1, 20, 10, 0);
	ktui_gesture_feed(&ev, &g, NULL, NULL);
	ev = touch_ev(KT_TOUCH_MOVE, 0, 6, 10, 10);
	ok(!ktui_gesture_feed(&ev, &g, NULL, NULL), "still undecided");
	ev = touch_ev(KT_TOUCH_MOVE, 1, 24, 10, 20);
	ok(ktui_gesture_feed(&ev, &g, NULL, NULL), "the pair disagreed");
	eq_int(g.type, KT_GEST_PINCH, "and that is a pinch");
	ok(g.dx > 0, "spreading reports a growing span");

	/* ── a second finger disqualifies the tap ──────────────────────── */
	ktui_gesture_reset();
	ev = touch_ev(KT_TOUCH_DOWN, 0, 10, 10, 0);
	ktui_gesture_feed(&ev, &g, NULL, NULL);
	ev = touch_ev(KT_TOUCH_DOWN, 1, 20, 10, 0);
	ktui_gesture_feed(&ev, &g, NULL, NULL);
	ev = touch_ev(KT_TOUCH_UP, 1, 20, 10, 10);
	ktui_gesture_feed(&ev, &g, NULL, NULL);
	ev = touch_ev(KT_TOUCH_UP, 0, 10, 10, 20);
	ok(!ktui_gesture_feed(&ev, &g, NULL, NULL),
	   "a sequence that had two fingers in it never taps");

	/* ── cancel is not up ──────────────────────────────────────────── */
	ktui_gesture_reset();
	ev = touch_ev(KT_TOUCH_DOWN, 0, 10, 5, 0);
	ktui_gesture_feed(&ev, &g, NULL, NULL);
	ev = touch_ev(KT_TOUCH_CANCEL, 0, 10, 5, 10);
	ok(!ktui_gesture_feed(&ev, &g, NULL, NULL),
	   "a cancelled sequence reports nothing");
	ok(!ktui_gesture_tick(KT_LONG_MS, &g),
	   "and leaves nothing half-recognised behind it");

	/* ── a caller that wants only gestures passes NULL ─────────────── */
	ktui_gesture_reset();
	have = 1;
	ev = touch_ev(KT_TOUCH_DOWN, 0, 3, 3, 0);
	ok(!ktui_gesture_feed(&ev, &g, NULL, NULL),
	   "NULL for the mouse pair is allowed");

	/* ── anything that is not a touch event is not ours ────────────── */
	memset(&ev, 0, sizeof(ev));
	ev.type = KT_EVT_KEY;
	have = 1;
	ok(!ktui_gesture_feed(&ev, &g, &m, &have),
	   "a key event is not a gesture");
	eq_int(have, 0, "and the mouse flag is cleared before anything else");

	ktui_gesture_reset();
	ktui_w = 0;
	ktui_h = 0;
}

/* ──────────────────────────────────────────────────────────────────────── */

/*
 * The sprite table: its cap, its byte budget, its opt-in eviction, and the
 * grid a picture larger than one slot becomes.
 *
 * EVERY PICTURE HERE IS A FAKE POINTER. The table holds an opaque `const void
 * *` and never dereferences it — that is the property that keeps libktui
 * linking nothing but the C library — so a distinct address per picture is a
 * complete test subject, and the suite needs no pixel library to run this.
 */

static uint64_t spr_freed[64];
static const void *spr_freed_pix[64];
static int spr_nfreed;

static void spr_evictor(uint64_t key, const void *pix, void *user)
{
	(void)user;
	if (spr_nfreed < (int)(sizeof(spr_freed) / sizeof(spr_freed[0]))) {
		spr_freed_pix[spr_nfreed] = pix;
		spr_freed[spr_nfreed++] = key;
	}
}

/* The tile callback hands back a distinct address per tile, and records the
 * cell rectangle it was asked for. */
static int spr_tw[64], spr_th[64], spr_tx[64], spr_ty[64];
static int spr_ntiles;
static int spr_fail_at = -1;
static char spr_pixels[64];

static const void *spr_tile(void *user, int cell_x, int cell_y, int cw, int ch)
{
	(void)user;
	if (spr_ntiles >= 64)
		return NULL;
	if (spr_fail_at >= 0 && spr_ntiles == spr_fail_at) {
		spr_ntiles++;
		return NULL;
	}
	spr_tx[spr_ntiles] = cell_x;
	spr_ty[spr_ntiles] = cell_y;
	spr_tw[spr_ntiles] = cw;
	spr_th[spr_ntiles] = ch;
	return &spr_pixels[spr_ntiles++];
}

/*
 * A BACKEND THAT PRESENTS, and the block needs one.
 *
 * The eviction rule is "is anything still drawing this", and ktui_cells()
 * answers it with the PRESENTED frame — `front`, which a backend's flush
 * updates as it presents. Offscreen mode deliberately makes ktui_draw_flush()
 * a no-op, so `front` there is always empty and nothing ever looks on-screen.
 * Presenting into it is the backend's documented job, so the test does the
 * smallest possible one rather than reaching around the seam.
 */
static void spr_flush(const KtuiCell *cur, KtuiCell *prev, int w, int h,
		      int force_full)
{
	(void)force_full;
	memcpy(prev, cur, (size_t)w * h * sizeof(*prev));
}

static void spr_size(int *w, int *h)
{
	if (w)
		*w = 80;
	if (h)
		*h = 24;
}

static int spr_caps(void) { return 0; }

static int spr_poll(KtuiEvent *ev, int timeout_ms)
{
	(void)ev;
	(void)timeout_ms;
	return 0;
}

static const KtuiBackend spr_backend = {
	.name = "selftest",
	.flush = spr_flush,
	.poll_event = spr_poll,
	.size = spr_size,
	.caps = spr_caps,
};

static void spr_reset(void)
{
	ktui_sprite_clear();
	ktui_sprite_evictor(NULL, NULL);
	ktui_sprite_budget(0, 0, 0);
	spr_nfreed = 0;
	spr_ntiles = 0;
	spr_fail_at = -1;
}

static void test_sprite(void)
{
	printf("\n==> the sprite table: the cap, the budget and the grid\n");

	char a_pix, b_pix, c_pix;

	const KtuiBackend *was = ktui_backend();

	ktui_backend_set(&spr_backend);
	ktui_w = 80;
	ktui_h = 24;
	ktui_draw_resize();
	ktui_draw_clip_none();
	ktui_draw_clear();
	ktui_draw_flush();
	spr_reset();

	/* ── the cap, and where the number comes from ──────────────────── */
	eq_int(KTUI_MAX_SPRITES, 4096, "the slot table holds four thousand");
	ok(((240 + 15) / 16) * ((67 + 15) / 16) <= KTUI_MAX_SPRITES,
	   "a full screen tiled edge to edge fits inside it");

	/* ── the round trip ────────────────────────────────────────────── */
	int sa = ktui_sprite_put(1, &a_pix, 2, 2, 'A');
	ok(sa >= 0, "a picture registers");
	eq_int(ktui_sprite_find(1), sa, "and is found again by its key");
	const KtuiSprite *got = ktui_sprite_get(sa);
	ok(got && got->pix == &a_pix, "the slot holds the pointer it was given");
	eq_int(got ? got->fallback : 0, 'A', "and the fallback beside it");

	eq_int(ktui_sprite_find(999), -1, "an unknown key is not found");
	eq_int(ktui_sprite_put(2, NULL, 2, 2, 'x'), -1,
	       "a null picture is refused");
	eq_int(ktui_sprite_put(2, &b_pix, 17, 2, 'x'), -1,
	       "wider than a slot is refused");
	eq_int(ktui_sprite_put(2, &b_pix, 2, 17, 'x'), -1,
	       "taller than a slot is refused");
	eq_int(ktui_sprite_put(2, &b_pix, 0, 2, 'x'), -1,
	       "a zero dimension is refused");

	/* ── re-registering ────────────────────────────────────────────── */
	ktui_sprite_evictor(spr_evictor, NULL);
	spr_nfreed = 0;
	eq_int(ktui_sprite_put(1, &a_pix, 2, 2, 'A'), sa,
	       "the same picture under the same key keeps its slot");
	eq_int(spr_nfreed, 0, "and hands nothing back");

	eq_int(ktui_sprite_put(1, &b_pix, 2, 2, 'A'), sa,
	       "a different picture under the same key reuses the slot");
	eq_int(spr_nfreed, 1, "and hands the old one back");
	ok(spr_nfreed == 1 && spr_freed_pix[0] == &a_pix,
	   "handing back exactly the picture it replaced");

	/* ── dropping is the owner's, so it calls nobody ───────────────── */
	spr_nfreed = 0;
	ktui_sprite_drop(1);
	eq_int(ktui_sprite_find(1), -1, "a dropped key is gone");
	eq_int(spr_nfreed, 0,
	       "dropping does not call the evictor: the owner is already there");

	/* ── clearing hands everything back ────────────────────────────── */
	spr_reset();
	ktui_sprite_evictor(spr_evictor, NULL);
	ktui_sprite_put(10, &a_pix, 1, 1, 'a');
	ktui_sprite_put(11, &b_pix, 1, 1, 'b');
	spr_nfreed = 0;
	ktui_sprite_clear();
	eq_int(spr_nfreed, 2, "clearing hands back every picture it held");
	eq_int(ktui_sprite_slots(), 0, "and empties the table");

	/* ── the byte budget ───────────────────────────────────────────── */
	spr_reset();
	ktui_sprite_put(20, &a_pix, 2, 2, 'a');
	eq_int((long long)ktui_sprite_bytes(), 0,
	       "with no cell size declared the byte budget is off");

	spr_reset();
	ktui_sprite_budget(1024 * 1024, 8, 16);
	ktui_sprite_put(20, &a_pix, 2, 2, 'a');
	eq_int((long long)ktui_sprite_bytes(), (long long)(2 * 8 * 2 * 16 * 4),
	       "a sprite costs its cells times the cell size, in ARGB");

	/* A cap too small for the next picture, and nobody to hand one back
	 * to: refused, and the total is left where it was. */
	spr_reset();
	ktui_sprite_budget(2 * 8 * 2 * 16 * 4, 8, 16);
	ok(ktui_sprite_put(20, &a_pix, 2, 2, 'a') >= 0,
	   "the first picture fits the cap exactly");
	size_t before = ktui_sprite_bytes();
	eq_int(ktui_sprite_put(21, &b_pix, 2, 2, 'b'), -1,
	       "the second is refused with no evictor to make room");
	eq_int((long long)ktui_sprite_bytes(), (long long)before,
	       "and the refusal left the total alone");

	/* With an evictor, room is made from the least recently used. */
	spr_reset();
	ktui_sprite_budget(2 * (2 * 8 * 2 * 16 * 4), 8, 16);
	ktui_sprite_evictor(spr_evictor, NULL);
	ktui_sprite_put(30, &a_pix, 2, 2, 'a');
	ktui_sprite_put(31, &b_pix, 2, 2, 'b');
	ktui_sprite_find(31);		/* touch 31, so 30 is the oldest */
	spr_nfreed = 0;
	ok(ktui_sprite_put(32, &c_pix, 2, 2, 'c') >= 0,
	   "a third picture is taken once there is an evictor");
	eq_int(spr_nfreed, 1, "exactly one picture was handed back");
	eq_int((long long)spr_freed[0], 30,
	       "and it is the least recently used one");
	eq_int(ktui_sprite_find(31), 1, "the one that was touched survived");

	/* ── what is on the screen is not evictable ────────────────────── */
	spr_reset();
	ktui_draw_clear();
	ktui_sprite_budget(2 * 8 * 2 * 16 * 4, 8, 16);
	ktui_sprite_evictor(spr_evictor, NULL);
	int son = ktui_sprite_put(40, &a_pix, 2, 2, 'a');
	KRect r = { 0, 0, 2, 2 };
	ktui_draw_sprite(r, son, 7, 0);
	ktui_draw_flush();		/* now the frame on screen names it */

	/*
	 * LOUD, BECAUSE THE DEPENDENCY IS INVISIBLE. ktui_offscreen_init() is a
	 * ONE-WAY LATCH — nothing clears it — and it makes ktui_draw_flush() a
	 * no-op, so a block that runs after one of the offscreen blocks would
	 * find `front` empty, every sprite would look evictable, and the two
	 * assertions below would pass for the wrong reason. This is why
	 * test_sprite() runs first in main().
	 */
	int fw = 0, fh = 0;
	const KtuiCell *frame = ktui_cells(&fw, &fh);
	ok(frame && fw > 0 && KTUI_IS_SPRITE(frame[0].ch) &&
	   (int)KTUI_SPRITE_SLOT(frame[0].ch) == son,
	   "a frame was presented, so the on-screen rule can be exercised");

	spr_nfreed = 0;
	eq_int(ktui_sprite_put(41, &b_pix, 2, 2, 'b'), -1,
	       "a sprite the cell buffer still names is not evictable");
	eq_int(spr_nfreed, 0, "so nothing was handed back");
	ktui_draw_clear();
	ktui_draw_flush();
	ok(ktui_sprite_put(41, &b_pix, 2, 2, 'b') >= 0,
	   "and once nothing draws it, it is");

	/* ── a text backend's substitute ───────────────────────────────── */
	spr_reset();
	int st = ktui_sprite_put(50, &a_pix, 2, 2, 0x2588);
	uint32_t tl = KTUI_SPRITE_BASE | ((uint32_t)st << 8);
	eq_int(ktui_sprite_text_cell(tl), 0x2588,
	       "a text backend puts the fallback in the top-left cell");
	eq_int(ktui_sprite_text_cell(tl | 1u), ' ',
	       "and blanks the rest, so two icons do not merge");
	eq_int(ktui_sprite_text_cell('Z'), 'Z',
	       "an ordinary character passes through untouched");

	/* ── a picture larger than a slot is a grid ────────────────────── */
	spr_reset();
	eq_int(ktui_sprite_put_tiled(100, 40, 30, 0x2588, spr_tile, NULL), 6,
	       "a 40x30 picture is six tiles");
	eq_int(spr_ntiles, 6, "and the callback was asked six times");

	int sx = -1, sy = -1;
	int tslot = ktui_sprite_tile_at(100, 40, 17, 3, &sx, &sy);
	ok(tslot >= 0, "cell 17,3 lands in a registered tile");
	eq_int(sx, 1, "at sub-cell x 1 of the second column");
	eq_int(sy, 3, "and sub-cell y 3");
	eq_int(tslot, ktui_sprite_tile_at(100, 40, 16, 0, NULL, NULL),
	       "which is the same tile cell 16,0 is in");

	/* The remainder tile is the remainder, not a padded full one. */
	eq_int(spr_tw[5], 8, "the last tile is the 8-cell remainder wide");
	eq_int(spr_th[5], 14, "and the 14-cell remainder tall");
	eq_int(spr_tw[0], 16, "while the first is a full slot");

	const KtuiSprite *last = ktui_sprite_get(
		ktui_sprite_tile_at(100, 40, 39, 29, NULL, NULL));
	ok(last && last->fallback == 0x2588,
	   "every tile carries the fallback, so a picture leaves a mark");

	/* ── an adjacent key must not collide ──────────────────────────── */
	int slots_before = ktui_sprite_slots();
	spr_ntiles = 0;
	eq_int(ktui_sprite_put_tiled(101, 40, 30, 0x2588, spr_tile, NULL), 6,
	       "a picture with an adjacent key registers six tiles of its own");
	eq_int(ktui_sprite_slots(), slots_before + 6,
	       "in six slots of their own — the stride is XORed, not added");

	/* ── all or nothing ────────────────────────────────────────────── */
	spr_reset();
	ktui_sprite_evictor(spr_evictor, NULL);
	spr_fail_at = 3;
	eq_int(ktui_sprite_put_tiled(200, 40, 30, 0x2588, spr_tile, NULL), -1,
	       "a tile the caller cannot supply abandons the whole picture");
	/* ktui_sprite_slots() is a HIGH-WATER MARK, not a live count: a dropped
	 * slot is emptied in place so the next put can reuse it. What "nothing
	 * is registered" means is that no tile can be found. */
	eq_int(ktui_sprite_tile_at(200, 40, 0, 0, NULL, NULL), -1,
	       "leaving no tile of it registered");
	eq_int(ktui_sprite_tile_at(200, 40, 17, 3, NULL, NULL), -1,
	       "not one of the tiles it had already taken either");
	eq_int(spr_nfreed, 3,
	       "and handing back the tiles it had already taken");

	/* ── dropping a tiled picture drops all of it ──────────────────── */
	spr_reset();
	ktui_sprite_put_tiled(300, 40, 30, 0x2588, spr_tile, NULL);
	ktui_sprite_drop_tiled(300, 40, 30);
	eq_int(ktui_sprite_tile_at(300, 40, 0, 0, NULL, NULL), -1,
	       "dropping a tiled picture drops every tile of it");

	spr_reset();
	ktui_draw_clear();
	ktui_draw_flush();
	ktui_backend_set(was);
}

/*
 * ── THE CONTRACT EVERY SURFACE ANSWERS ──────────────────────────────────
 *
 * The hint row and the Esc ladder, driven offscreen. A golden cannot check
 * this: a dump carries codepoints and no attributes, and the row's whole point
 * is that its CONTENT changes with the focus. So it is asserted here, where the
 * pushed hints and the classification are both readable.
 *
 * THE TOTAL RULE: every key the row advertises must be answered. A surface that
 * names a key `ktui_keys` returns PASS for is a surface advertising a key that
 * does nothing, which is the failure the whole wave exists to remove.
 */
static int lay_a, lay_b;
static int lay_a_up(void *u) { (void)u; return lay_a; }
static int lay_b_up(void *u) { (void)u; return lay_b; }
static void lay_a_close(void *u) { (void)u; lay_a = 0; }
static void lay_b_close(void *u) { (void)u; lay_b = 0; }

static char help_opened[64];
static void on_help(const char *doc, void *u)
{
	(void)u;
	snprintf(help_opened, sizeof(help_opened), "%s", doc ? doc : "");
}

/*
 * THE SLOT NAMES, which are what the colour picker hands a person.
 *
 * An array indexed by the enum: a slot added to `KT_NCOLOR` without a row here
 * would answer NULL and the picker would say nothing at all, and a row that
 * drifted would name the wrong colour — which is worse, because it reads
 * right.
 */
/*
 * A `file://` URI BACK TO A PATH. One surface hands a program a path and
 * another hands it a URI, so the decoder is what lets a caller take either —
 * and the host is the field that must be refused rather than dropped: ignoring
 * it opens THIS machine's copy of a file that names another machine's.
 */
static void test_uri_path(void)
{
	printf("\n==> a file:// URI comes back as the path it names\n");

	char out[256];

	ok(kb_uri_path("file:///home/kdos/My%20Report.pdf", out, sizeof(out)),
	   "a URI with an escape decodes");
	eq_str(out, "/home/kdos/My Report.pdf", "and the space comes back");

	ok(kb_uri_path("file://localhost/etc/hosts", out, sizeof(out)),
	   "localhost is this machine");
	eq_str(out, "/etc/hosts", "and drops out of the path");

	ok(!kb_uri_path("file://otherbox/etc/passwd", out, sizeof(out)),
	   "a URI naming another machine is refused, not silently localised");

	ok(kb_uri_path("/plain/path", out, sizeof(out)),
	   "a plain path is not a URI and is carried through");
	eq_str(out, "/plain/path", "unchanged");

	/* A NUL in the middle would end the path early and open a different
	 * file from the one named. */
	ok(!kb_uri_path("file:///etc/passwd%00.png", out, sizeof(out)),
	   "an escaped NUL is refused");

	/* Uppercase and lowercase hex are the same URI. */
	ok(kb_uri_path("file:///a%2fb", out, sizeof(out)), "lowercase hex");
	eq_str(out, "/a/b", "decodes the same as uppercase");

	ok(!kb_uri_path("file:///0123456789abcdef", out, 8),
	   "and a result that does not fit is refused rather than cut");
}

static void test_slot_names(void)
{
	printf("\n==> every colour slot has a name the picker can hand over\n");

	int named = 1, unique = 1;

	for (int i = 0; i < KT_NCOLOR; i++) {
		const char *a = ktui_slot_name(i);

		if (!a || !*a) {
			named = 0;
			continue;
		}
		for (int j = i + 1; j < KT_NCOLOR; j++) {
			const char *b = ktui_slot_name(j);

			if (b && !strcmp(a, b))
				unique = 0;
		}
	}
	ok(named, "all eight slots are named");
	ok(unique, "and no two share a name");
	ok(!ktui_slot_name(-1) && !ktui_slot_name(KT_NCOLOR),
	   "a value that is not a slot is not named");
	eq_str(ktui_slot_name(KT_ACCENT), "accent",
	       "and the name is the one a person would write");
}

static void test_keys(void)
{
	printf("\n==> the hint row follows the focus, and Esc unwinds one rung\n");

	KtuiKeys k = { 0 };
	KtuiEvent ev = { 0 };

	ev.type = KT_EVT_KEY;

	/* ── F1 is answered only where there is a page ───────────────── */
	ev.key = KT_K_F1;
	eq_int(ktui_keys(&k, &ev), KTUI_KEY_PASS,
	       "a surface with no documentation does not answer F1");
	k.doc = "kinstall.md#the-pages";
	k.help = on_help;
	help_opened[0] = '\0';
	eq_int(ktui_keys(&k, &ev), KTUI_KEY_TAKEN, "and one with a page does");
	eq_str(help_opened, "kinstall.md#the-pages",
	       "opening the page the surface named, not an index");

	/* ── Esc unwinds ONE rung per press, innermost first ─────────── */
	ktui_keys_layer(&k, "Back", lay_a_up, lay_a_close, NULL);
	ktui_keys_layer(&k, "Cancel", lay_b_up, lay_b_close, NULL);
	lay_a = lay_b = 1;
	ev.key = KT_K_ESC;
	eq_str(ktui_esc_verb(&k), "Cancel",
	       "the row names the INNERMOST open layer");
	eq_int(ktui_keys(&k, &ev), KTUI_KEY_TAKEN, "Esc takes the inner one");
	eq_int(lay_b, 0, "closing it");
	eq_int(lay_a, 1, "and leaving the one under it up");
	eq_str(ktui_esc_verb(&k), "Back", "which the row now names");
	eq_int(ktui_keys(&k, &ev), KTUI_KEY_TAKEN, "the next Esc takes that");
	eq_int(lay_a, 0, "closing it too");
	eq_str(ktui_esc_verb(&k), "Close",
	       "and with nothing open the row says Close");
	eq_int(ktui_keys(&k, &ev), KTUI_KEY_CLOSE,
	       "which is what the third Esc means");

	/*
	 * A LAYER THAT WENT DOWN BY ITSELF IS NOT STILL UP. A dialog dismissed
	 * by a click leaves no raised bit, because the predicate is asked at
	 * the instant Esc arrives rather than cached — this is the defect that
	 * makes an Esc ladder swallow a keystroke.
	 */
	lay_a = 1;
	lay_b = 0;
	eq_str(ktui_esc_verb(&k), "Back",
	       "a layer taken down elsewhere is skipped, not remembered");

	/* ── the row is pushed, and clears itself ────────────────────── */
	ktui_offscreen_init(60, 12);
	ktui_draw_init();
	ktui_hint_if(0, "F10", "Menu");
	ktui_hint_if(1, "Enter", "Open");
	eq_int(ktui_hint_row(&k, krect(0, 11, 60, 1), KT_BG), 1,
	       "a row is drawn when there is room");

	int cw = 0, chh = 0;
	const KtuiCell *cells = ktui_draw_cells(&cw, &chh);
	const KtuiCell *row = cells + 11 * cw;
	char got[61];

	for (int i = 0; i < 60 && i < cw; i++)
		got[i] = row[i].ch >= 0x20 && row[i].ch < 0x7f ?
			 (char)row[i].ch : ' ';
	got[60] = '\0';
	ok(strstr(got, "F1 help") != NULL,
	   "F1 comes from the DESCRIPTOR, so no surface pushes it per frame");
	ok(strstr(got, "Enter Open") != NULL, "and the one pushed with a true");
	ok(strstr(got, "F10") == NULL,
	   "and NOT the one whose condition was false");

	/* Drawn once, then gone: the pool is cleared by the draw, so a second
	 * row on a surface that pushed nothing is empty rather than a repeat
	 * of the last one. F1 survives because it is the DESCRIPTOR's and not
	 * a frame's, which is exactly the difference being asserted. */
	ktui_hint_row(&k, krect(0, 11, 60, 1), KT_BG);
	cells = ktui_draw_cells(&cw, &chh);
	row = cells + 11 * cw;
	for (int i = 0; i < 60 && i < cw; i++)
		got[i] = row[i].ch >= 0x20 && row[i].ch < 0x7f ?
			 (char)row[i].ch : ' ';
	got[60] = '\0';
	ok(strstr(got, "Enter Open") == NULL,
	   "a second row does not repeat the hints the first one drew");
	ok(strstr(got, "F1 help") != NULL, "though F1 is still named");

	/* A surface with NO page says nothing about F1, on the same row. */
	{
		KtuiKeys nodoc = { 0 };

		ktui_offscreen_init(60, 12);
		ktui_draw_init();
		ktui_hint("Enter", "Open");
		ktui_hint_row(&nodoc, krect(0, 11, 60, 1), KT_BG);
		cells = ktui_draw_cells(&cw, &chh);
		row = cells + 11 * cw;
		for (int i = 0; i < 60 && i < cw; i++)
			got[i] = row[i].ch >= 0x20 && row[i].ch < 0x7f ?
				 (char)row[i].ch : ' ';
		got[60] = '\0';
		ok(strstr(got, "F1") == NULL,
		   "a surface with no page neither answers F1 nor advertises it");
	}

	/* ── not on a short window ───────────────────────────────────── */
	ktui_offscreen_init(60, 6);
	ktui_draw_init();
	ktui_hint("Enter", "Open");
	eq_int(ktui_hint_row(&k, krect(0, 5, 60, 1), KT_BG), 0,
	       "and no row at all below eight rows");
}

/*
 * ── THE MENU ────────────────────────────────────────────────────────────
 *
 * One widget for a bar and for a popup, because they are the same list drawn
 * in two places. The two things worth asserting are the two that a second
 * implementation always gets wrong: that `sel` indexes the ITEMS and not the
 * drawn rows, so a hidden row cannot move the selection onto something else;
 * and that the drawing and the hit test ask the same `show` from the same
 * walk, so a click cannot run the row above the one under the pointer.
 */
enum { MI_NEW = 11, MI_OPEN, MI_SAVE, MI_QUIT };

static const KtuiMenuItem file_items[] = {
	{ "&New", MI_NEW, "Ctrl+N", 1 },
	{ "&Open", MI_OPEN, NULL, 1 },
	{ "", 0, NULL, 0 },			/* a rule */
	{ "&Save", MI_SAVE, NULL, 0 },		/* disabled */
	{ "&Quit", MI_QUIT, NULL, 1 },
};

static const KtuiMenuItem edit_items[] = {
	{ "&Copy", 21, NULL, 1 },
	{ "&Paste", 22, NULL, 1 },
};

static const KtuiMenuPane test_panes[] = {
	{ "&File", file_items, 5 },
	{ "&Edit", edit_items, 2 },
};

/* Open is hidden, which is what makes the index-not-row rule testable. */
static int menu_show(int i, void *user)
{
	(void)user;
	return i != 1;
}

/*
 * WHERE THIS SURFACE WOULD PUT A CONTEXT PANE, and whether it has one at all.
 * A real `ctx_at` answers where the KEYBOARD'S focus is drawn; the pair below
 * is a place a finger is not, so a menu opened at the finger can be told from
 * one opened at the focus.
 */
enum { ST_CTX_X = 30, ST_CTX_Y = 9 };
static int st_ctx_refuse;

static int st_ctx_at(int *x, int *y, void *user)
{
	(void)user;
	if (st_ctx_refuse)
		return 0;
	*x = ST_CTX_X;
	*y = ST_CTX_Y;
	return 1;
}

static void test_menu(void)
{
	printf("\n==> the menu: one widget, a bar and a popup\n");

	KtuiMenu m = { 0 };
	KtuiEvent ev = { 0 };
	int id = 0;

	m.pane = test_panes;
	m.npane = 2;
	m.show = menu_show;
	m.has_bar = 1;
	m.bar_row = 0;

	eq_int(ktui_menu_accel_of("&File"), 'f', "the mark names its letter");
	eq_int(ktui_menu_accel_of("Se&ttings"), 't', "wherever it falls");
	eq_int(ktui_menu_accel_of("R&&D"), 0,
	       "and a doubled mark is a literal ampersand, not an accelerator");
	eq_int(ktui_menu_accel_of("Plain"), 0, "no mark, no letter");

	/* ── the caret never rests on a rule or a hidden row ──────────── */
	ktui_menu_open(&m, 0, 2, 1);
	eq_int(ktui_menu_active(&m), 1, "the pane is down");
	eq_int(m.sel, 0, "opening lands on the first row that can be picked");

	ev.type = KT_EVT_KEY;
	ev.key = KT_K_DOWN;
	ktui_menu_event(&m, &ev, &id);
	eq_int(m.sel, 4,
	       "Down skips the hidden row, the rule AND the disabled row");
	ktui_menu_event(&m, &ev, &id);
	eq_int(m.sel, 0, "and wraps");

	/* ── a letter picks its own row, and only a pickable one ─────── */
	ev.key = 'q';
	eq_int(ktui_menu_event(&m, &ev, &id), KTUI_MENU_PICKED,
	       "a letter picks the row it marks");
	eq_int(id, MI_QUIT, "by ID, never by position");
	eq_int(ktui_menu_active(&m), 0, "and the pane goes down with it");

	ktui_menu_open(&m, 0, 2, 1);
	ev.key = 's';
	eq_int(ktui_menu_event(&m, &ev, &id), KTUI_MENU_TAKEN,
	       "a letter on a DISABLED row picks nothing");
	eq_int(id, MI_QUIT, "leaving the last answer alone");
	ev.key = 'o';
	eq_int(ktui_menu_event(&m, &ev, &id), KTUI_MENU_TAKEN,
	       "and one on a row `show` hid picks nothing either");

	/* ── Left and Right walk the bar; a popup has no neighbour ───── */
	ev.key = KT_K_RIGHT;
	eq_int(ktui_menu_event(&m, &ev, &id), KTUI_MENU_TAKEN, "Right on a bar");
	eq_int(m.open, 2, "moves to the next pane");
	m.has_bar = 0;
	eq_int(ktui_menu_event(&m, &ev, &id), KTUI_MENU_NONE,
	       "and on a popup it is not the menu's key to take");
	m.has_bar = 1;

	/* ── the hit test walks the same skip the drawing does ───────── */
	ktui_offscreen_init(40, 12);
	ktui_draw_init();
	ktui_menu_open(&m, 0, 2, 1);
	ktui_menu_draw(&m);
	eq_int(m.rows, 4, "four of the five items are shown");

	ev.type = KT_EVT_MOUSE;
	ev.press = KT_MP_PRESS;
	ev.btn = KT_MB_LEFT;
	ev.mx = m.x + 2;
	ev.my = m.y + 4;			/* the fourth drawn row */
	eq_int(ktui_menu_event(&m, &ev, &id), KTUI_MENU_PICKED,
	       "a click on the last drawn row picks");
	eq_int(id, MI_QUIT,
	       "the item that was DRAWN there, not the one at that index");

	/*
	 * ── THE CHORD COLUMN IS INSIDE THE PANE ─────────────────────
	 *
	 * ktui_draw_text_right() takes a WIDTH and ends the text at `x + w - 1`,
	 * so a caller that handed it the column to end on instead printed every
	 * chord the pane's own x to the RIGHT of the box — over the desktop,
	 * past every clamp the pane's rectangle went through, and with the hit
	 * test still measuring the box that was drawn.
	 *
	 * Opened well right of the origin, because a pane at x = 2 hides the
	 * defect: two cells of drift still land inside a twenty-cell box.
	 */
	{
		int gw = 0, gh = 0;
		const KtuiCell *g;
		int row, inside = 0, outside = 0;

		ktui_offscreen_init(80, 12);
		ktui_draw_init();
		ktui_menu_open(&m, 0, 30, 1);
		ktui_menu_draw(&m);

		g = ktui_draw_cells(&gw, &gh);
		row = m.y + 1;		/* &New, whose chord is Ctrl+N */
		for (int x = 0; x < gw; x++) {
			uint32_t ch = g[(size_t)row * gw + x].ch;

			if (ch != 'C')
				continue;
			if (x >= m.x && x < m.x + m.w)
				inside = 1;
			else
				outside = 1;
		}
		ok(inside, "a row's chord is drawn inside the pane");
		ok(!outside, "and nowhere else");

		for (int x = 0; x < gw; x++) {
			uint32_t ch = g[(size_t)row * gw + x].ch;

			if (x >= m.x && x < m.x + m.w)
				continue;
			if (ch && ch != ' ')
				outside = 1;
		}
		ok(!outside, "and the row outside the box is untouched");
		ktui_menu_close(&m);
	}

	/* ── a click away closes rather than picking ─────────────────── */
	ktui_offscreen_init(40, 12);
	ktui_draw_init();
	ktui_menu_open(&m, 0, 2, 1);
	ev.mx = 39;
	ev.my = 11;
	eq_int(ktui_menu_event(&m, &ev, &id), KTUI_MENU_TAKEN,
	       "a click outside the pane");
	eq_int(ktui_menu_active(&m), 0, "closes it");

	/* ── the keys contract routes into it ────────────────────────── */
	{
		KtuiKeys k = { 0 };
		KtuiEvent kev = { 0 };

		k.menu = &m;
		kev.type = KT_EVT_KEY;
		kev.key = KT_K_F10;
		eq_int(ktui_keys(&k, &kev), KTUI_KEY_TAKEN, "F10 opens the bar");
		eq_int(m.open, 1, "on its first pane");
		eq_str(ktui_esc_verb(&k), "Cancel",
		       "and the row says Escape cancels the menu, not closes");
		kev.key = KT_K_ESC;
		eq_int(ktui_keys(&k, &kev), KTUI_KEY_TAKEN, "Esc takes it down");
		eq_int(ktui_menu_active(&m), 0, "leaving nothing open");
		eq_str(ktui_esc_verb(&k), "Close",
		       "and the row goes back to naming the surface");

		/* Alt+letter opens a pane by its mark, from closed. */
		kev.key = 'e';
		kev.mods = KT_MOD_ALT;
		eq_int(ktui_keys(&k, &kev), KTUI_KEY_TAKEN,
		       "Alt+letter opens the pane it marks");
		eq_int(m.open, 2, "which is the second one here");
		kev.mods = 0;
		kev.key = KT_K_ENTER;
		eq_int(ktui_keys(&k, &kev), KTUI_KEY_MENU,
		       "and Enter on a row reports a pick through the contract");
		eq_int(k.menu_id, 21, "naming the item");

		/* A surface with no bar does not answer F10. */
		m.has_bar = 0;
		kev.key = KT_K_F10;
		eq_int(ktui_keys(&k, &kev), KTUI_KEY_PASS,
		       "a surface with no bar neither opens nor swallows F10");
		m.has_bar = 1;

		/*
		 * ── A LONG PRESS IS Shift+F10 WITH A FINGER ───────────
		 *
		 * Answered in the contract so every surface with a context
		 * pane inherits it, rather than thirty of them growing a
		 * touch path each — and AT THE FINGER, not at the keyboard's
		 * focus: a person holding a row expects the menu on that row.
		 * `ctx_at` is still asked, because a surface that refuses one
		 * must refuse the other.
		 */
		KtuiEvent tev = { 0 };

		tev.type = KT_EVT_TOUCH;
		tev.gesture = KT_GEST_LONG;
		tev.mx = 7;
		tev.my = 5;
		k.ctx_pane = 1;
		k.ctx_at = NULL;
		eq_int(ktui_keys(&k, &tev), KTUI_KEY_PASS,
		       "a long press on a surface that names no context is passed on");
		k.ctx_at = st_ctx_at;
		st_ctx_refuse = 1;
		eq_int(ktui_keys(&k, &tev), KTUI_KEY_PASS,
		       "and one that refuses refuses a finger too");
		st_ctx_refuse = 0;
		eq_int(ktui_keys(&k, &tev), KTUI_KEY_TAKEN,
		       "a long press opens the context pane");
		eq_int(m.open, 2, "the pane the surface named");
		/* `x` and `y` are the popup as DRAWN and are clamped on
		 * screen, which is why the assertion is that they are the
		 * finger's and not the focus's rather than an exact pair. */
		ok(m.x != ST_CTX_X || m.y != ST_CTX_Y,
		   "at the FINGER and not where the keyboard's focus is");
		kev.key = KT_K_ESC;
		eq_int(ktui_keys(&k, &kev), KTUI_KEY_TAKEN,
		       "and Esc takes it down again");

		/* AND ONLY A LONG PRESS. A tap is a click and every widget
		 * already handles one; a tap that opened a menu would put a
		 * pane under every finger. */
		tev.gesture = KT_GEST_TAP;
		eq_int(ktui_keys(&k, &tev), KTUI_KEY_PASS,
		       "a tap is a click and opens nothing");
	}
}

/*
 * ── WHAT A WIDGET SAYS IT IS ────────────────────────────────────────────
 *
 * A widget already knows which item has focus — it computes that every frame
 * from the same id the hit test uses — so a reader that worked it out again
 * from a grid of cells would be guessing at what the surface has in hand.
 * These assertions are that each widget states it, and states the position
 * facts a reader would otherwise have to count.
 *
 * SILENCE IS THE FAILURE MODE. The queue is cleared at the start of every
 * frame, so a frame in which nothing is focused announces nothing — a reader
 * told the wrong control is worse off than one told nothing.
 */
/* A row callback that draws the index and nothing else: the list needs one,
 * and what it paints is not what this block is about. */
static void a11y_row(int idx, int x, int y, int w, int sel, int focus,
		     void *user)
{
	char buf[16];

	(void)user;
	snprintf(buf, sizeof(buf), "row %d", idx);
	ktui_draw_text(x, y, w, buf, sel && focus ? KT_BG : KT_TEXT,
		       sel ? KT_ACCENT : KT_BG, 0);
}

static const KtuiA11y *a11y_find(int role)
{
	for (int i = 0; i < ktui_announce_count(); i++)
		if (ktui_announce_at(i)->role == role)
			return ktui_announce_at(i);
	return NULL;
}

static void test_ktui_announce(void)
{
	KtuiEvent ev = { 0 };
	const KtuiA11y *a;

	printf("\n==> a widget says what it is, and where in its set\n");

	if (ktui_offscreen_init(60, 20) != 0) {
		ok(0, "an offscreen surface for the announce test");
		return;
	}

	/* A BUTTON, FOCUSED. The frame is driven the way a program drives it:
	 * begin, draw, end — the focus lands on the first control claimed. */
	ktui_frame_begin(&ev);
	ktui_button(krect(1, 1, 12, 1), "Install", 1, 1);
	a = a11y_find(KT_A11Y_BUTTON);
	ok(a && !strcmp(a->label, "Install"),
	   "a focused button says its label");
	ktui_frame_end();

	/* AND THE QUEUE IS THIS FRAME'S. Nothing drawn, nothing said. */
	ktui_frame_begin(&ev);
	eq_int(ktui_announce_count(), 0,
	       "a frame that draws nothing announces nothing");
	ktui_frame_end();

	/* A CHECK SAYS ITS VALUE, because "on" and "off" is what the box is
	 * for and a label alone leaves a reader guessing. */
	int on = 1;

	ktui_frame_begin(&ev);
	ktui_check(1, 1, 20, "Night light", &on);
	a = a11y_find(KT_A11Y_CHECK);
	ok(a && !strcmp(a->label, "Night light") && !strcmp(a->value, "on"),
	   "a focused check says its label and its state");
	ktui_frame_end();

	/* A SECRET FIELD SAYS IT IS ONE AND NEVER WHAT IS IN IT. */
	char pw[64] = "hunter2";

	ktui_frame_begin(&ev);
	ktui_input(krect(1, 1, 20, 1), pw, sizeof(pw), 1, "Password");
	a = a11y_find(KT_A11Y_INPUT);
	ok(a && !strcmp(a->value, "hidden"),
	   "a secret field never announces its contents");
	ok(a && !strstr(a->value, "hunter2"), "and really never");
	ktui_frame_end();

	/* A LIST SAYS WHICH ITEM OF HOW MANY. The rows come from the caller's
	 * callback, so the position is what the widget can state. */
	KtuiList st = { 0 };

	st.sel = 2;
	ktui_frame_begin(&ev);
	ktui_list(krect(1, 1, 20, 5), &st, 9, a11y_row, NULL, 0);
	a = a11y_find(KT_A11Y_LIST);
	ok(a && a->index == 3 && a->count == 9, "a list says 3 of 9");
	ktui_frame_end();

	/* A TAB STRIP HAS ITS NAMES, so it says the name as well. */
	static const KtuiTab tabs[] = { { "General", NULL }, { "Keys", NULL },
					{ "Network", NULL } };

	ktui_frame_begin(&ev);
	ktui_tabs_draw(krect(0, 0, 40, 1), tabs, 3, 1, -1, 0);
	a = a11y_find(KT_A11Y_TAB);
	ok(a && !strcmp(a->label, "Keys") && a->index == 2 && a->count == 3,
	   "a tab strip says which tab, by name");
	ktui_frame_end();

	/* AND A MOVE SAYS THE NEW POSITION, from the key handler that made it. */
	int sel = 0;

	ktui_frame_begin(&ev);
	ktui_tabs_key(&sel, 3, 0, KT_K_RIGHT);
	a = a11y_find(KT_A11Y_TAB);
	ok(a && a->index == 2, "moving to the next tab says so");
	ktui_frame_end();

	/* A TEXT BLOCK SAYS WHICH LINE OF HOW MANY. */
	static char lines[4][256] = { "one", "two", "three", "" };
	KtuiTextArea ta = { 0 };
	int n = 3;

	ktui_frame_begin(&ev);
	ktui_textarea_key(&ta, lines[0], &n, 4, sizeof(lines[0]), KT_K_DOWN);
	a = a11y_find(KT_A11Y_TEXT);
	ok(a && a->index == 2 && a->count == 3, "a text block says line 2 of 3");
	ktui_frame_end();

	/* THE POINTER SAYS IT TOO. A table row reached by a press and an
	 * option reached by a press both move a caret a keyboard would have
	 * announced, so both announce. */
	KtuiTable tb = { 0 };

	ktui_frame_begin(&ev);
	ktui_table_pick(&tb, 9, 4, NULL, NULL);
	a = a11y_find(KT_A11Y_TABLE);
	ok(a && a->index == 5 && a->count == 9,
	   "a table row picked with the pointer says row 5 of 9");
	ktui_frame_end();

	KtuiDrop dd = { 0 };

	dd.open = 1;
	ktui_frame_begin(&ev);
	ktui_dropdown_hit(krect(0, 0, 12, 1), &dd, 4, 2, 3);
	a = a11y_find(KT_A11Y_CHOICE);
	ok(a && a->index == 3 && a->count == 4,
	   "an option clicked out of an open list says 3 of 4");
	ktui_frame_end();

	/* AND A HAND-DRAWN ROW LIST, which owns its own loop and still moves
	 * a caret somebody has to be told about. */
	KtuiEvent click = { 0 };
	int rsel = 0, rtop = 0;

	click.type = KT_EVT_MOUSE;
	click.btn = KT_MB_LEFT;
	click.press = KT_MP_PRESS;
	click.mx = 1;
	click.my = 3;
	ktui_frame_begin(&ev);
	eq_int(ktui_rows_event(krect(0, 0, 20, 8), &rsel, &rtop, 6, &click),
	       KTUI_ROWS_MOVED, "a press on another row moves the caret");
	a = a11y_find(KT_A11Y_LIST);
	ok(a && a->index == 4 && a->count == 6, "and says 4 of 6");
	ktui_frame_end();

	/*
	 * A MENU COUNTS THE ROWS A CARET CAN REACH, not the rows it draws.
	 * The pane below is five items around one rule, so the caret's third
	 * stop is "3 of 4" and never "4 of 5" — the number a person hears has
	 * to match the number of things they can pick.
	 */
	static const KtuiMenuItem mi[] = {
		{ "&Open", 1, NULL, 1 },
		{ "&Save", 2, NULL, 1 },
		{ NULL, 0, NULL, 0 },		/* a rule */
		{ "&Close", 3, NULL, 1 },
		{ "&Quit", 4, NULL, 1 },
	};
	static const KtuiMenuPane mp[] = { { "&File", mi, 5 } };
	KtuiMenu mu = { 0 };

	mu.pane = mp;
	mu.npane = 1;
	ktui_frame_begin(&ev);
	ktui_menu_open(&mu, 0, 0, 0);
	a = a11y_find(KT_A11Y_LIST);
	ok(a && !strcmp(a->label, "&Open") && a->index == 1 && a->count == 4,
	   "an opened menu says its first row, 1 of 4");
	ktui_frame_end();

	KtuiEvent down = { 0 };

	down.type = KT_EVT_KEY;
	down.key = KT_K_DOWN;
	/* One step per frame: the queue keeps every record a frame made, and
	 * two steps inside one would leave the first for a11y_find to answer
	 * with. */
	ktui_frame_begin(&ev);
	ktui_menu_event(&mu, &down, NULL);
	ktui_frame_end();

	ktui_frame_begin(&ev);
	ktui_menu_event(&mu, &down, NULL);
	a = a11y_find(KT_A11Y_LIST);
	ok(a && !strcmp(a->label, "&Close") && a->index == 3 && a->count == 4,
	   "and stepping over the rule says 3 of 4, by name");
	ktui_frame_end();

	/* THE QUEUE IS FIXED AND DROPS THE REST: nothing on the draw path
	 * allocates, and a frame with more to say than it holds says what
	 * fits. */
	ktui_frame_begin(&ev);
	for (int i = 0; i < KTUI_A11Y_MAX + 8; i++)
		ktui_announce(KT_A11Y_BUTTON, "x", NULL, 0, 0);
	eq_int(ktui_announce_count(), KTUI_A11Y_MAX,
	       "the queue is capped rather than grown");
	ktui_frame_end();
}

/*
 * BRACKETED PASTE, THROUGH THE REAL INPUT PATH.
 *
 * `ktui_input_next` reads descriptor 0, so a pipe standing in for the terminal
 * is the whole fixture. Two writes on purpose: a paste is not one read, and
 * the terminator split across two of them is the case that turns a paste into
 * a session-long one if the tail is taken for text.
 */
static void test_bracketed_paste(void)
{
	printf("\n==> a paste from the host terminal is text, not keys\n");

	int pfd[2];
	int saved = dup(0);

	if (pipe(pfd) != 0 || saved < 0) {
		ok(0, "a pipe standing in for the terminal");
		return;
	}
	dup2(pfd[0], 0);

	KtuiEvent ev;
	const char *out = NULL;

	/* Whole, in one write. */
	const char *p1 = "\033[200~pasted line\033[201~";
	ssize_t w = write(pfd[1], p1, strlen(p1));

	(void)w;
	for (int i = 0; i < 20 && !ktui_paste_take(&out); i++)
		ktui_input_next(&ev, 5);
	eq_str(out ? out : "", "pasted line",
	       "the bytes between the brackets are the paste");

	/*
	 * The terminator in pieces. `\033[20` at the tail of one read is not
	 * text: taken as text it leaves a `1~` that ends nothing, and the
	 * paste runs to the end of the session.
	 */
	out = NULL;
	const char *p2 = "\033[200~split\033[20";

	w = write(pfd[1], p2, strlen(p2));
	for (int i = 0; i < 5; i++)
		ktui_input_next(&ev, 5);
	ok(ktui_paste_take(&out) == 0, "an unterminated paste is not delivered");
	w = write(pfd[1], "1~", 2);
	out = NULL;
	for (int i = 0; i < 20 && !ktui_paste_take(&out); i++)
		ktui_input_next(&ev, 5);
	eq_str(out ? out : "", "split",
	       "and a terminator split across two reads still ends it");

	/*
	 * AND A CHORD INSIDE A PASTE IS NOT A CHORD. This is the whole point:
	 * a pasted line beginning with Ctrl+A would otherwise arm the leader
	 * key and swallow the character after it.
	 */
	out = NULL;
	const char *p3 = "\033[200~\001a\033[201~";

	w = write(pfd[1], p3, strlen(p3));
	for (int i = 0; i < 20 && !ktui_paste_take(&out); i++)
		ktui_input_next(&ev, 5);
	ok(out != NULL, "a control byte inside a paste stays in the paste");

	dup2(saved, 0);
	close(saved);
	close(pfd[0]);
	close(pfd[1]);
}

/*
 * THE CHORDS A TERMINAL CAN CARRY.
 *
 * A surface run with `--tty` reads its keys as escape sequences, and Super
 * arrives only in the kitty form `CSI <codepoint> ; <modifiers>[:<event>] u`.
 * Without that form a Super chord cannot be typed into such a surface at all.
 * The decoder reads standard input, so a pipe standing in for the terminal
 * feeds it the real bytes.
 */
static void feed_seq(int w, const char *bytes)
{
	ssize_t n = write(w, bytes, strlen(bytes));

	(void)n;
}

static void test_ktui_csi_u(void)
{
	printf("\n==> Super reaches a surface in a terminal\n");

	int pv[2];

	if (pipe(pv) != 0) {
		ok(0, "a pipe standing in for the terminal");
		return;
	}

	int saved = dup(0);

	if (saved < 0 || dup2(pv[0], 0) < 0) {
		ok(0, "standard input redirected to it");
		close(pv[0]);
		close(pv[1]);
		return;
	}

	KtuiEvent ev;

	feed_seq(pv[1], "\x1b[13;9u");
	eq_int(ktui_input_next(&ev, 50), 1, "CSI 13;9u decodes");
	eq_int(ev.key, KT_K_ENTER, "  as Return");
	eq_int(ev.mods, KT_MOD_SUPER, "  with Super, which is bit 8");

	feed_seq(pv[1], "\x1b[113;9u");
	eq_int(ktui_input_next(&ev, 50), 1, "CSI 113;9u decodes");
	eq_int(ev.key, 'q', "  as the letter the chord table was written with");
	eq_int(ev.mods, KT_MOD_SUPER, "  and Super");

	/*
	 * A RELEASE IS NOT A CHORD. Both halves of a keypress reaching the
	 * table fires every chord twice — once down, once up.
	 */
	feed_seq(pv[1], "\x1b[13;9:3u");
	eq_int(ktui_input_next(&ev, 50), 0, "a release is consumed and not reported");

	/* A repeat is a press: holding Super+Left tiles more than once. */
	feed_seq(pv[1], "\x1b[13;9:2u");
	eq_int(ktui_input_next(&ev, 50), 1, "a repeat is");
	eq_int(ev.key, KT_K_ENTER, "  the same key");

	/*
	 * THE SUB-PARAMETER MUST BE EATEN WHOLE. A parser that stops at the
	 * colon leaves the rest of the sequence in the buffer, and it is then
	 * decoded as characters typed into whatever has the focus.
	 */
	feed_seq(pv[1], "\x1b[13;9:3u\x1b[113;9u");
	eq_int(ktui_input_next(&ev, 50), 0, "a release before another chord is dropped");
	eq_int(ktui_input_next(&ev, 50), 1, "and the chord behind it still decodes");
	eq_int(ev.key, 'q', "  intact");
	eq_int(ev.mods, KT_MOD_SUPER, "  with its modifier");

	/* The older mask still means what it did: 5 is 4+1, ctrl and shift. */
	feed_seq(pv[1], "\x1b[1;5A");
	eq_int(ktui_input_next(&ev, 50), 1, "CSI 1;5A still decodes");
	eq_int(ev.key, KT_K_UP, "  as Up");
	eq_int(ev.mods, KT_MOD_CTRL, "  with Ctrl and nothing else");

	dup2(saved, 0);
	close(saved);
	close(pv[0]);
	close(pv[1]);
}

/* ── motion: ktui_anim.c ──────────────────────────────────────────────── */

static int64_t an_t;
static int an_clock_on, an_motion_on, an_motion_asked;

static int64_t an_now(void) { return an_t; }
static int an_animates(void) { return an_clock_on; }
static int an_motion(void)
{
	an_motion_asked++;
	return an_motion_on;
}
static void an_flush(const KtuiCell *c, KtuiCell *p, int w, int h, int f)
{
	(void)c; (void)p; (void)w; (void)h; (void)f;
}
static int an_poll(KtuiEvent *ev, int ms)
{
	(void)ms;
	ev->type = KT_EVT_TICK;
	return 0;
}
static void an_size(int *w, int *h) { *w = 80; *h = 24; }
static int an_caps(void) { return 0; }

static const KtuiBackend an_backend = {
	.name = "selftest-anim",
	.flush = an_flush,
	.poll_event = an_poll,
	.size = an_size,
	.caps = an_caps,
	.animates = an_animates,
};
/* A capture backend: the shape every golden's is, with no frame clock. */
static const KtuiBackend an_capture = {
	.name = "selftest-capture",
	.flush = an_flush,
	.poll_event = an_poll,
	.size = an_size,
	.caps = an_caps,
};

static int near(float a, float b) { return a - b < 1e-4f && b - a < 1e-4f; }

static void test_ktui_anim(void)
{
	printf("\n==> motion: the curves, the end value, the live window\n");

	const KtuiBackend *was = ktui_backend();
	KtuiAnim a, b, z;

	ktui_anim_set_clock(an_now);
	ktui_anim_set_motion_fn(an_motion);
	an_t = 1000;
	an_motion_on = 1;

	/* The curves: pinned ends, clamped outside, a NaN at the start, and
	 * each one never going backwards. */
	for (int e = KT_EASE_OUT; e <= KT_EASE_LINEAR; e++) {
		float last = -1.0f;
		int mono = 1;

		for (int i = 0; i <= 100; i++) {
			float v = ktui_ease(e, (float)i / 100.0f);

			if (v < last)
				mono = 0;
			last = v;
		}
		ok(near(ktui_ease(e, 0.0f), 0.0f) && near(ktui_ease(e, 1.0f), 1.0f),
		   "ease: 0 at 0 and 1 at 1");
		ok(near(ktui_ease(e, -3.0f), 0.0f) && near(ktui_ease(e, 7.0f), 1.0f),
		   "ease: clamped outside [0, 1]");
		ok(near(ktui_ease(e, __builtin_nanf("")), 0.0f), "ease: a NaN settles at 0");
		ok(mono, "ease: never goes backwards");
	}
	ok(ktui_ease(KT_EASE_OUT, 0.25f) > 0.25f,
	   "ease-out: ahead of linear early (fast, then settling)");
	ok(ktui_ease(KT_EASE_IN_OUT, 0.25f) < 0.25f &&
	   near(ktui_ease(KT_EASE_IN_OUT, 0.5f), 0.5f) &&
	   ktui_ease(KT_EASE_IN_OUT, 0.75f) > 0.75f,
	   "ease-in-out: slow at both ends, half way at the middle");

	/* THE TERMINAL: no backend is the tty one, and nothing moves there. */
	ktui_backend_set(NULL);
	ktui_anim_start(&a, 10.0f, 20.0f, 200, KT_EASE_OUT, 0);
	ok(a.still, "tty: an animation started there is still");
	ok(near(ktui_anim_value(&a), 20.0f), "tty: worth its end value at once");
	ok(!ktui_anim_live(), "tty: nothing is live, so nothing ticks");
	ok(ktui_anim_running(&a) && ktui_anim_left(&a) == 200,
	   "tty: still inside its time (a held state can ask)");
	ok(!ktui_anim_moving(), "tty: not moving");

	/* A capture backend — what a golden is drawn through — the same. */
	ktui_backend_set(&an_capture);
	ktui_anim_start(&a, 0.0f, 1.0f, 200, KT_EASE_OUT, 0);
	ok(a.still && near(ktui_anim_value(&a), 1.0f) && !ktui_anim_live(),
	   "capture backend (no frame clock): the end value, nothing live");

	/* A FRAME CLOCK WITH MOTION ON: it moves, and the live window covers
	 * it and no more. */
	ktui_backend_set(&an_backend);
	an_clock_on = 1;
	an_motion_asked = 0;
	ktui_anim_start(&a, 10.0f, 20.0f, 200, KT_EASE_OUT, 0);
	ok(an_motion_asked == 1, "start asks the motion question once");
	ok(!a.still && ktui_anim_moving(), "clocked, motion on: it moves");
	ok(near(ktui_anim_value(&a), 10.0f), "at its start: `from`");
	ok(ktui_anim_live(), "live while it runs");
	an_t += 100;
	{
		float v = ktui_anim_value(&a);

		ok(v > 15.0f && v < 20.0f,
		   "half way through an ease-out: past half, not there");
		ok(near(v, ktui_anim_value_at(&a, an_t)),
		   "value and value_at agree on the same clock");
	}
	ok(ktui_anim_left(&a) == 100, "left counts down");
	an_t += 100;
	ok(near(ktui_anim_value(&a), 20.0f) && !ktui_anim_running(&a) &&
	   ktui_anim_left(&a) == 0, "at its end: `to`, and over");
	ok(!ktui_anim_live(), "the live window closes with the last one");

	/* Two at once: the window is the later end, whichever started first. */
	ktui_anim_start(&a, 0.0f, 1.0f, 300, KT_EASE_OUT, 0);
	an_t += 50;
	ktui_anim_start(&b, 0.0f, 1.0f, 100, KT_EASE_OUT, 0);
	an_t += 200;
	ok(ktui_anim_live() && !ktui_anim_running(&b) && ktui_anim_running(&a),
	   "two: live until the later end");
	an_t += 50;
	ok(!ktui_anim_live(), "two: silent once both are over");

	/* A PULSE goes there and back and settles where it began. */
	ktui_anim_start(&a, 0.0f, 1.0f, 1000, KT_EASE_IN_OUT, 2);
	ok(near(ktui_anim_end(&a), 0.0f), "pulse: its end is `from`");
	an_t += 250;
	ok(near(ktui_anim_value(&a), 1.0f), "pulse: at the top of the first beat");
	an_t += 250;
	ok(near(ktui_anim_value(&a), 0.0f), "pulse: back down between beats");
	an_t += 250;
	ok(near(ktui_anim_value(&a), 1.0f), "pulse: at the top of the second");
	an_t += 125;
	{
		float v = ktui_anim_value(&a);

		ok(v > 0.0f && v < 1.0f, "pulse: in between on the way down");
	}
	an_t += 125;
	ok(near(ktui_anim_value(&a), 0.0f), "pulse: settled at rest");

	/* Stopped: settled at once. The window is a deadline and stays. */
	ktui_anim_start(&a, 0.0f, 1.0f, 400, KT_EASE_OUT, 0);
	an_t += 100;
	ktui_anim_stop(&a);
	ok(near(ktui_anim_value(&a), 1.0f) && !ktui_anim_running(&a),
	   "stop: the end value, and not running");
	ok(ktui_anim_live(), "stop: the window is not shortened");
	an_t += 300;

	/* A capture swapped in mid-way draws the end: a golden taken while
	 * something ran is still the settled picture. */
	ktui_anim_start(&a, 0.0f, 1.0f, 400, KT_EASE_OUT, 0);
	an_t += 100;
	ktui_backend_set(&an_capture);
	ok(near(ktui_anim_value(&a), 1.0f), "capture mid-way: the end value");
	ok(!ktui_anim_live(), "capture mid-way: nothing ticks");
	ktui_backend_set(&an_backend);
	an_t += 300;

	/* MOTION OFF: still from the start, and the window is not opened. */
	an_motion_on = 0;
	ktui_anim_start(&a, 0.0f, 1.0f, 400, KT_EASE_OUT, 0);
	ok(a.still && near(ktui_anim_value(&a), 1.0f),
	   "motion off: the end value at once");
	ok(!ktui_anim_live(), "motion off: nothing live, nothing ticks");
	ok(ktui_anim_running(&a) && !ktui_anim_moving(),
	   "motion off: still inside its time, not moving");
	an_motion_on = 1;

	/* A zeroed one was never started, and a zero duration never moves. */
	memset(&z, 0, sizeof(z));
	z.to = 5.0f;
	ok(near(ktui_anim_value(&z), 5.0f) && !ktui_anim_running(&z),
	   "never started: its end value, not running");
	an_t += 1000;
	ktui_anim_start(&a, 0.0f, 1.0f, 0, KT_EASE_OUT, 0);
	ok(a.still && near(ktui_anim_value(&a), 1.0f) && !ktui_anim_live(),
	   "zero duration: settled, nothing live");

	/* The clock turned off under a running one: no ticks promised. */
	ktui_anim_start(&a, 0.0f, 1.0f, 400, KT_EASE_OUT, 0);
	an_clock_on = 0;
	ok(!ktui_anim_live() && near(ktui_anim_value(&a), 1.0f),
	   "frame clock gone: not live, the end value");
	an_clock_on = 1;
	an_t += 400;

	ktui_anim_set_clock(NULL);
	ktui_anim_set_motion_fn(NULL);
	ok(ktui_anim_now() > 0, "the real clock answers");
	ktui_backend_set(was);
}

int main(void)
{
	kb_set_progname("selftest");

	/* FIRST, and it has to be: it needs a presented frame, and
	 * ktui_offscreen_init() below is a one-way latch that makes a flush a
	 * no-op for the rest of the process. Its own guard says so if this
	 * moves. */
	test_sprite();

	test_base();
	test_trash();
	test_colour();
	test_pkg();
	test_shelves();
	test_build();
	test_snap_layers();
	test_proc();
	test_chart();
	test_grid();
	test_shade();
	test_slider();
	test_rows();
	test_ktui_ids();
	test_ktui_layout();
	test_ktui_anim();
	test_pack();
	test_portup();
	test_gesture();
	test_uri_path();
	test_slot_names();
	test_keys();
	test_menu();
	test_ktui_announce();
	test_ktui_csi_u();
	test_bracketed_paste();

	printf("\n%d checks, %d failed\n", checks, failures);
	return failures ? 1 : 0;
}
