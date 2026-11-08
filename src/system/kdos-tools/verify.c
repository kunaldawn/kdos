/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   kdos verify — is this file the one its checksum or signature names?
 *
 *     kdos verify [--notify] [--] FILE...
 *
 * Dolphin's Verify action (servicemenus/kdos-verify.desktop) runs it with
 * --notify on the selected files; a person runs it from a prompt.
 *
 * WHAT IT CHECKS, PER FILE, with nothing fetched and nothing executed:
 *
 *   a checksum   <file>.sha512, <file>.sha256, then SHA512SUMS and
 *                SHA256SUMS beside it. A per-file sidecar's first hash line
 *                is the file's whatever name it carries, since a download
 *                renamed after the fact keeps its sidecar; a list's line
 *                must name the file. GNU (`<hex>  name`, `<hex> *name`), BSD
 *                (`SHA256 (name) = <hex>`) and a bare `<hex>` are read.
 *   a pack       a .kpack's own signature block, through libkpack against
 *                /etc/kdos/keys/packs ($KDOS_KEYS), the ring kdos-packd
 *                trusts packs from. Its payload digest is checked first.
 *   a sidecar    <file>.sig in libksig's `kdos-sig-1` form — what kpkg
 *                writes beside a package and a release's SHA256SUMS —
 *                against /etc/kdos/keys ($KPKG_KEYRING), kpkg's ring. A
 *                checksum list with such a .sig is verified with it.
 *
 * A .sig in any other form (a GPG detached signature) is named and left
 * alone: nothing here can check it, and saying "bad" would be a lie.
 *
 * EXIT STATUS, the worst over every file: 1 a check failed (a hash that does
 * not match, a signature no trusted key verifies, a pack whose bytes do not
 * hash to its own digest); 2 a file that could not be read, or a usage error;
 * 3 nothing vouches for a file (no checksum, an unsigned pack, a signature
 * this machine holds no key for); 0 every file passed at least one check and
 * failed none. Unverifiable is not verified, so 3 is never 0.
 * ---------------------------------
 */

#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>
#include <unistd.h>

#include "kbase.h"
#include "kpack.h"
#include "ksig.h"
#include "monocypher/monocypher-ed25519.h"

#include "kdos-tools.h"

enum { V_PASS, V_NOTHING, V_UNREADABLE, V_FAIL };

/* A checksum list larger than this is not one; reading it whole would let a
 * file named SHA256SUMS hold the verifier's memory. */
#define SUMS_CAP (4u << 20)

static const char *pack_ring(void)
{
	const char *e = getenv("KDOS_KEYS");

	return e && *e ? e : "/etc/kdos/keys/packs";
}

static const char *pkg_ring(void)
{
	const char *e = getenv("KPKG_KEYRING");

	return e && *e ? e : "/etc/kdos/keys";
}

/* ── hashing ───────────────────────────────────────────────────────────── */

static void hex_of(const uint8_t *b, size_t n, char *out)
{
	static const char h[] = "0123456789abcdef";

	for (size_t i = 0; i < n; i++) {
		out[2 * i] = h[b[i] >> 4];
		out[2 * i + 1] = h[b[i] & 15];
	}
	out[2 * n] = '\0';
}

/* SHA-256 and SHA-512 of the file in one read. 0, or -1 with errno set. */
static int hash_file(const char *path, char s256[65], char s512[129])
{
	KbSha256 a;
	crypto_sha512_ctx b;
	uint8_t d[64];
	char buf[65536];
	int fd = open(path, O_RDONLY | O_CLOEXEC);

	if (fd < 0)
		return -1;
	kb_sha256_init(&a);
	crypto_sha512_init(&b);
	for (;;) {
		ssize_t n = read(fd, buf, sizeof(buf));

		if (n < 0) {
			if (errno == EINTR)
				continue;
			close(fd);
			return -1;
		}
		if (n == 0)
			break;
		kb_sha256_update(&a, buf, (size_t)n);
		crypto_sha512_update(&b, (const uint8_t *)buf, (size_t)n);
	}
	close(fd);
	kb_sha256_final(&a, s256);
	crypto_sha512_final(&b, d);
	hex_of(d, 64, s512);
	return 0;
}

/* ── checksum files ────────────────────────────────────────────────────── */

static int is_hex(const char *p, size_t n)
{
	for (size_t i = 0; i < n; i++)
		if (!((p[i] >= '0' && p[i] <= '9') || (p[i] >= 'a' && p[i] <= 'f') ||
		      (p[i] >= 'A' && p[i] <= 'F')))
			return 0;
	return 1;
}

/* The name a list line gives, without a leading `./` or directory: a list
 * written in a release directory names its files bare or with `./`. */
static int names_file(const char *name, size_t n, const char *base)
{
	const char *slash = NULL;

	for (size_t i = 0; i < n; i++)
		if (name[i] == '/')
			slash = name + i;
	if (slash) {
		n -= (size_t)(slash + 1 - name);
		name = slash + 1;
	}
	return strlen(base) == n && !memcmp(name, base, n);
}

/*
 * The hash of `base` in a checksum file's text, `hexlen` characters, into
 * `out`. With `any`, the first hash line counts whatever it names (a per-file
 * sidecar). 1 found, 0 not.
 */
static int find_hash(char *text, const char *base, size_t hexlen, int any,
		     char *out)
{
	char *save = NULL;

	for (char *ln = strtok_r(text, "\n", &save); ln;
	     ln = strtok_r(NULL, "\n", &save)) {
		size_t len = strlen(ln);
		const char *hex = NULL, *name = NULL;
		size_t namelen = 0;

		while (len && (ln[len - 1] == '\r' || ln[len - 1] == ' ' ||
			       ln[len - 1] == '\t'))
			ln[--len] = '\0';
		while (*ln == ' ' || *ln == '\t')
			ln++, len--;
		if (!len || *ln == '#')
			continue;
		if (len > hexlen + 4 && (!strncmp(ln, "SHA256 (", 8) ||
					 !strncmp(ln, "SHA512 (", 8))) {
			/* BSD: SHA256 (name) = hex */
			char *close = strstr(ln, ") = ");

			if (!close || strlen(close + 4) != hexlen)
				continue;
			name = ln + 8;
			namelen = (size_t)(close - name);
			hex = close + 4;
		} else if (len >= hexlen && is_hex(ln, hexlen) &&
			   (len == hexlen || ln[hexlen] == ' ' ||
			    ln[hexlen] == '\t')) {
			hex = ln;
			name = ln + hexlen;
			while (*name == ' ' || *name == '\t')
				name++;
			if (*name == '*')
				name++;
			namelen = strlen(name);
		} else {
			continue;
		}
		if (!is_hex(hex, hexlen))
			continue;
		if (!any && (!namelen || !names_file(name, namelen, base)))
			continue;
		memcpy(out, hex, hexlen);
		out[hexlen] = '\0';
		for (size_t i = 0; i < hexlen; i++)
			if (out[i] >= 'A' && out[i] <= 'F')
				out[i] = (char)(out[i] - 'A' + 'a');
		return 1;
	}
	return 0;
}

/* ── one file's report ─────────────────────────────────────────────────── */

typedef struct {
	int pass, fail;
	KbBuf said;		/* "; "-joined findings */
} Report;

static void say(Report *r, const char *fmt, ...)
	__attribute__((format(printf, 2, 3)));

static void say(Report *r, const char *fmt, ...)
{
	char line[1024];
	va_list ap;

	va_start(ap, fmt);
	vsnprintf(line, sizeof(line), fmt, ap);
	va_end(ap);
	kb_buf_printf(&r->said, "%s%s", r->said.n ? "; " : "", line);
}

/* Is `sig` a libksig signature file? A .sig in another format is not ours. */
static int is_kdos_sig(const char *sig)
{
	char line[32];

	if (kb_read_line_file(sig, line, sizeof(line)) < 0)
		return 0;
	return !strncmp(line, "kdos-sig-1 ", 11);
}

/* `<path>.sig` against kpkg's ring; `what` names the thing signed. */
static void check_sidecar_sig(Report *r, const char *path, const char *what)
{
	char sig[4200], who[KSIG_ID_HEX];
	KsigRing ring;

	snprintf(sig, sizeof(sig), "%s.sig", path);
	if (!kb_path_exists(sig))
		return;
	if (!is_kdos_sig(sig)) {
		say(r, "%s.sig is not a KDOS signature and was not checked",
		    kb_basename(path));
		return;
	}
	memset(&ring, 0, sizeof(ring));
	ksig_ring_load(&ring, pkg_ring());
	if (ring.n == 0) {
		say(r, "%s is signed, but %s holds no key to check it", what,
		    pkg_ring());
		return;
	}
	if (ksig_verify_path(&ring, path, sig, who) == 0) {
		r->pass++;
		say(r, "%s is signed by key %s", what, who);
	} else {
		r->fail++;
		say(r, "%s carries a signature no key in %s verifies", what,
		    pkg_ring());
	}
}

static void check_sums(Report *r, const char *path, const char *s256,
		       const char *s512)
{
	static const struct {
		const char *suffix;	/* after the file's own name, or NULL */
		const char *list;	/* in the file's directory, or NULL   */
		int is512;
	} SRC[] = {
		{ ".sha512", NULL, 1 },
		{ ".sha256", NULL, 0 },
		{ NULL, "SHA512SUMS", 1 },
		{ NULL, "SHA256SUMS", 0 },
	};
	const char *base = kb_basename(path);
	char dir[4096];
	int side_done = 0, list_done = 0;

	kb_strlcpy(dir, path, sizeof(dir));
	{
		char *slash = strrchr(dir, '/');

		if (slash)
			*slash = '\0';
		else
			kb_strlcpy(dir, ".", sizeof(dir));
		if (!dir[0])
			kb_strlcpy(dir, "/", sizeof(dir));
	}
	for (size_t i = 0; i < sizeof(SRC) / sizeof(SRC[0]); i++) {
		char side[4200], want[129];
		size_t hexlen = SRC[i].is512 ? 128 : 64, len = 0;
		struct stat st;
		char *text;
		const char *label;

		/* The first sidecar and the first list that name the file
		 * answer; SHA-512 is looked for before SHA-256 in each. */
		if (SRC[i].suffix ? side_done : list_done)
			continue;
		if (SRC[i].suffix)
			snprintf(side, sizeof(side), "%s%s", path,
				 SRC[i].suffix);
		else
			snprintf(side, sizeof(side), "%s/%s", dir, SRC[i].list);
		if (stat(side, &st) != 0 || !S_ISREG(st.st_mode))
			continue;
		label = kb_basename(side);
		if (!strcmp(label, base))
			continue;
		if ((size_t)st.st_size > SUMS_CAP ||
		    !(text = kb_read_whole(side, &len))) {
			say(r, "%s could not be read", label);
			continue;
		}
		if (!find_hash(text, base, hexlen, SRC[i].suffix != NULL,
			       want)) {
			free(text);
			if (!SRC[i].suffix)
				continue;
			say(r, "%s holds no %s hash", label,
			    SRC[i].is512 ? "SHA-512" : "SHA-256");
			continue;
		}
		free(text);
		if (SRC[i].suffix)
			side_done = 1;
		else
			list_done = 1;
		if (!strcmp(want, SRC[i].is512 ? s512 : s256)) {
			r->pass++;
			say(r, "%s matches %s", SRC[i].is512 ? "SHA-512" : "SHA-256",
			    label);
		} else {
			r->fail++;
			say(r, "%s does NOT match %s",
			    SRC[i].is512 ? "SHA-512" : "SHA-256", label);
		}
		if (!SRC[i].suffix)
			check_sidecar_sig(r, side, label);
	}
}

static int ends_with(const char *s, const char *suf)
{
	size_t a = strlen(s), b = strlen(suf);

	return a >= b && !strcasecmp(s + a - b, suf);
}

static void check_pack(Report *r, const char *path)
{
	KpkPack p;
	KsigRing ring;
	char who[KSIG_ID_HEX] = "";

	if (kpk_open(path, &p) != 0) {
		r->fail++;
		say(r, "it is named .kpack and is not a KDOS pack");
		return;
	}
	memset(&ring, 0, sizeof(ring));
	ksig_ring_load(&ring, pack_ring());
	switch (kpk_verify(&p, &ring, who)) {
	case KPK_SIG_GOOD:
		r->pass++;
		say(r, "pack %s is signed by key %s", p.meta.id, who);
		break;
	case KPK_SIG_NONE:
		say(r, "pack %s carries no signature", p.meta.id);
		break;
	case KPK_SIG_NOKEY:
		say(r, "pack %s is signed, but %s holds no key to check it",
		    p.meta.id, pack_ring());
		break;
	case KPK_SIG_HASH:
		r->fail++;
		say(r, "pack %s does NOT hash to its own digest", p.meta.id);
		break;
	default:
		r->fail++;
		say(r, "pack %s carries a signature no key in %s verifies",
		    p.meta.id, pack_ring());
		break;
	}
}

static int verify_one(const char *path, Report *r)
{
	char s256[65], s512[129];
	struct stat st;

	if (stat(path, &st) != 0) {
		say(r, "%s", strerror(errno));
		return V_UNREADABLE;
	}
	if (!S_ISREG(st.st_mode)) {
		say(r, "not a regular file");
		return V_UNREADABLE;
	}
	if (hash_file(path, s256, s512) != 0) {
		say(r, "%s", strerror(errno));
		return V_UNREADABLE;
	}
	check_sums(r, path, s256, s512);
	if (ends_with(path, ".kpack"))
		check_pack(r, path);
	check_sidecar_sig(r, path, "it");
	if (r->fail)
		return V_FAIL;
	if (r->pass)
		return V_PASS;
	if (!r->said.n)
		say(r, "no checksum file and no KDOS signature beside it");
	return V_NOTHING;
}

/* ── the verb ──────────────────────────────────────────────────────────── */

/*
 * A file's name as the notification shows it. kb_notify hands the summary and
 * the body to gdbus as arguments, and gdbus takes one that begins with a dash
 * for an option of its own and drops the whole call, so such a name is shown
 * as `./-name`. The body is markup to the notification server, so `&`, `<` and
 * `>` in it are escaped, or a file's name could draw a link there.
 */
static void escaped(KbBuf *b, const char *text, int markup)
{
	for (const char *p = text; *p; p++) {
		if (markup && *p == '&')
			kb_buf_str(b, "&amp;");
		else if (markup && *p == '<')
			kb_buf_str(b, "&lt;");
		else if (markup && *p == '>')
			kb_buf_str(b, "&gt;");
		else
			kb_buf_add(b, p, 1);
	}
}

static void shown_name(KbBuf *b, const char *name, int markup)
{
	if (name[0] == '-')
		kb_buf_str(b, "./");
	escaped(b, name, markup);
}

static int usage(void)
{
	fprintf(stderr, "usage: kdos verify [--notify] [--] FILE...\n");
	return 2;
}

int kdt_verify_main(int argc, char **argv)
{
	int notify = 0, first = 0, worst = V_PASS, n = 0;
	int count[4] = { 0 };
	KbBuf body = { 0 };
	const char *only = NULL;

	for (; first < argc; first++) {
		if (!strcmp(argv[first], "--notify"))
			notify = 1;
		else if (!strcmp(argv[first], "--")) {
			first++;
			break;
		} else if (argv[first][0] == '-')
			return usage();
		else
			break;
	}
	if (first >= argc)
		return usage();

	for (int i = first; i < argc; i++, n++) {
		Report r = { 0 };
		int v = verify_one(argv[i], &r);
		static const char *const WORD[] = {
			"verified", "not verified", "unreadable", "FAILED"
		};

		count[v]++;
		if (v > worst)
			worst = v;
		if (v == V_UNREADABLE)
			fprintf(stderr, "kdos verify: %s: %s\n", argv[i],
				r.said.p);
		else
			printf("%s: %s — %s\n", argv[i], WORD[v], r.said.p);
		if (body.n)
			kb_buf_str(&body, "\n");
		shown_name(&body, kb_basename(argv[i]), 1);
		kb_buf_printf(&body, ": %s — ", WORD[v]);
		escaped(&body, r.said.p, 1);
		only = argv[i];
		kb_buf_free(&r.said);
	}

	if (notify) {
		KbBuf sum = { 0 };

		if (n == 1) {
			shown_name(&sum, kb_basename(only), 0);
			kb_buf_printf(&sum, " %s",
				      worst == V_PASS ? "is verified"
				      : worst == V_FAIL ? "FAILED verification"
				      : worst == V_UNREADABLE ? "could not be read"
				      : "could not be verified");
		} else if (worst == V_FAIL) {
			kb_buf_printf(&sum, "%d of %d files FAILED verification",
				      count[V_FAIL], n);
		} else if (worst == V_PASS) {
			kb_buf_printf(&sum, "All %d files are verified", n);
		} else {
			kb_buf_printf(&sum, "%d of %d files are verified",
				      count[V_PASS], n);
		}
		kb_notify("kdos", sum.p, body.p ? body.p : "");
		kb_buf_free(&sum);
	}
	kb_buf_free(&body);
	/* The order of the codes is not the order of the states: a failed
	 * check (1) outranks an unreadable file (2), which outranks a file
	 * nothing vouches for (3). */
	return worst == V_FAIL ? 1 : worst == V_UNREADABLE ? 2
	     : worst == V_NOTHING ? 3 : 0;
}
