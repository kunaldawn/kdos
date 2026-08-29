/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   the metadata blob — flat key = value, and nothing cleverer
 * ---------------------------------
 *
 * The same shape a kpkgbuild uses, for the same reason: it parses at the
 * kb_ level, a person can read a pack's description with `tail -c` and their
 * eyes, and there is no parser here for anybody to find a bug in.
 *
 * The blob is NOT NUL-terminated on disk — it is a span between two offsets —
 * so every loop here is bounded by `len` and never by a terminator. A value
 * longer than its field is TRUNCATED and a line with no `=` is skipped; an
 * unknown key is ignored, because a pack from a newer KDOS must stay readable
 * rather than becoming unopenable for saying one more thing.
 */

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "kbase.h"
#include "kpack.h"

const char *kpk_kind_name(KpkKind k)
{
	switch (k) {
	case KPK_KIND_BASE:	return "base";
	case KPK_KIND_RUNTIME:	return "runtime";
	case KPK_KIND_APP:	return "app";
	case KPK_KIND_DATA:	return "data";
	default:		return "unknown";
	}
}

KpkKind kpk_kind_parse(const char *s)
{
	if (!strcmp(s, "base"))		return KPK_KIND_BASE;
	if (!strcmp(s, "runtime"))	return KPK_KIND_RUNTIME;
	if (!strcmp(s, "app"))		return KPK_KIND_APP;
	if (!strcmp(s, "data"))		return KPK_KIND_DATA;
	return KPK_KIND_UNKNOWN;
}

static void trim(char *s)
{
	size_t n = strlen(s);
	while (n && (s[n - 1] == ' ' || s[n - 1] == '\t' || s[n - 1] == '\r'))
		s[--n] = 0;
}

/*
 * `rt-gtk >= 1`, `base`, `rt-qt = 2.1`. The operator is optional; without one
 * any version satisfies, which is what a runtime that has never broken
 * compatibility should be able to say.
 */
static int req_parse(const char *v, KpkReq *r)
{
	char buf[192];
	char *p, *op;

	memset(r, 0, sizeof(*r));
	kb_strlcpy(buf, v, sizeof(buf));
	p = buf;
	while (*p == ' ')
		p++;
	op = p;
	while (*op && *op != ' ' && *op != '\t')
		op++;
	if (*op)
		*op++ = 0;
	if (!*p)
		return -1;
	kb_strlcpy(r->name, p, sizeof(r->name));

	while (*op == ' ' || *op == '\t')
		op++;
	if (!*op)
		return 0;
	p = op;
	while (*p && *p != ' ' && *p != '\t')
		p++;
	if (*p)
		*p++ = 0;
	if (strcmp(op, ">=") && strcmp(op, ">") && strcmp(op, "=") &&
	    strcmp(op, "<=") && strcmp(op, "<"))
		return -1;
	kb_strlcpy(r->op, op, sizeof(r->op));
	while (*p == ' ' || *p == '\t')
		p++;
	if (!*p)
		return -1;	/* an operator with no version is not a bound */
	kb_strlcpy(r->ver, p, sizeof(r->ver));
	return 0;
}

/* `<path-in-pack> <destination>` — two whitespace-separated fields. */
static int graft_parse(const char *v, KpkGraft *g)
{
	char buf[KPK_PATH * 2 + 4];
	char *p, *q;

	memset(g, 0, sizeof(*g));
	kb_strlcpy(buf, v, sizeof(buf));
	p = buf;
	while (*p == ' ')
		p++;
	q = p;
	while (*q && *q != ' ' && *q != '\t')
		q++;
	if (!*q)
		return -1;
	*q++ = 0;
	while (*q == ' ' || *q == '\t')
		q++;
	if (!*p || !*q)
		return -1;
	kb_strlcpy(g->from, p, sizeof(g->from));
	kb_strlcpy(g->to, q, sizeof(g->to));
	return 0;
}

int kpk_meta_parse(const char *text, size_t len, KpkMeta *m)
{
	size_t off = 0;

	if (!text)
		return -1;
	memset(m, 0, sizeof(*m));
	kb_strlcpy(m->arch, "x86_64", sizeof(m->arch));
	kb_strlcpy(m->release, "1", sizeof(m->release));

	while (off < len) {
		char line[1024];
		size_t end = off, n;
		char *eq, *key, *val;

		while (end < len && text[end] != '\n')
			end++;
		n = end - off;
		/* A line longer than the buffer is DROPPED rather than cut in
		 * half: half a value is a value nobody wrote. */
		if (n < sizeof(line)) {
			memcpy(line, text + off, n);
			line[n] = 0;

			key = line;
			while (*key == ' ' || *key == '\t')
				key++;
			if (*key && *key != '#' && (eq = strchr(key, '=')) != NULL) {
				*eq = 0;
				val = eq + 1;
				trim(key);
				while (*val == ' ' || *val == '\t')
					val++;
				trim(val);

				if (!strcmp(key, "id"))
					kb_strlcpy(m->id, val, sizeof(m->id));
				else if (!strcmp(key, "kind"))
					m->kind = kpk_kind_parse(val);
				else if (!strcmp(key, "name"))
					kb_strlcpy(m->name, val, sizeof(m->name));
				else if (!strcmp(key, "version"))
					kb_strlcpy(m->version, val, sizeof(m->version));
				else if (!strcmp(key, "release"))
					kb_strlcpy(m->release, val, sizeof(m->release));
				else if (!strcmp(key, "arch"))
					kb_strlcpy(m->arch, val, sizeof(m->arch));
				else if (!strcmp(key, "summary"))
					kb_strlcpy(m->summary, val, sizeof(m->summary));
				else if (!strcmp(key, "category"))
					kb_strlcpy(m->category, val, sizeof(m->category));
				else if (!strcmp(key, "licence"))
					kb_strlcpy(m->licence, val, sizeof(m->licence));
				else if (!strcmp(key, "size"))
					m->size = strtoull(val, NULL, 10);
				else if (!strcmp(key, "installed"))
					m->installed = strtoull(val, NULL, 10);
				else if (!strcmp(key, "launch_cold"))
					m->launch_cold = atoi(val);
				else if (!strcmp(key, "recommended"))
					m->recommended = !strcmp(val, "yes") ||
							 !strcmp(val, "1");
				else if (!strcmp(key, "description")) {
					size_t have = strlen(m->description);
					if (have && have + 1 < sizeof(m->description))
						m->description[have++] = '\n';
					kb_strlcpy(m->description + have, val,
						   sizeof(m->description) - have);
				} else if (!strcmp(key, "requires")) {
					/* One requirement per line, so a name
					 * with a space in it is impossible
					 * rather than ambiguous. */
					if (m->nreq < KPK_REQ_MAX &&
					    req_parse(val, &m->req[m->nreq]) == 0)
						m->nreq++;
				} else if (!strcmp(key, "provides")) {
					if (m->nprov < KPK_LIST_MAX)
						kb_strlcpy(m->provides[m->nprov++],
							   val, KPK_ID_MAX);
				} else if (!strcmp(key, "desktop")) {
					if (m->ndesktop < KPK_LIST_MAX)
						kb_strlcpy(m->desktop[m->ndesktop++],
							   val, 128);
				} else if (!strcmp(key, "mime")) {
					if (m->nmime < KPK_LIST_MAX)
						kb_strlcpy(m->mime[m->nmime++],
							   val, 128);
				} else if (!strcmp(key, "command")) {
					if (m->ncmd < KPK_LIST_MAX)
						kb_strlcpy(m->command[m->ncmd++],
							   val, KPK_ID_MAX);
				} else if (!strcmp(key, "needs")) {
					if (m->nneeds < KPK_LIST_MAX)
						kb_strlcpy(m->needs[m->nneeds++],
							   val, KPK_ID_MAX);
				} else if (!strcmp(key, "graft")) {
					if (m->ngraft < KPK_GRAFT_MAX &&
					    graft_parse(val, &m->graft[m->ngraft]) == 0)
						m->ngraft++;
				} else if (!strcmp(key, "boxgraft")) {
					if (m->nboxgraft < KPK_GRAFT_MAX &&
					    graft_parse(val, &m->boxgraft[m->nboxgraft]) == 0)
						m->nboxgraft++;
				} else if (!strcmp(key, "env")) {
					/* NAME=value, and the NAME half must be
					 * a name: a variable called `PATH ` or
					 * one carrying a newline is somebody
					 * shaping an environment from an
					 * untrusted artefact. */
					const char *e = strchr(val, '=');
					int ok = e && e != val;
					for (const char *c = val; ok && c < e; c++)
						if (!isalnum((unsigned char)*c) && *c != '_')
							ok = 0;
					if (ok && m->nenv < KPK_ENV_MAX)
						kb_strlcpy(m->env[m->nenv++], val, 256);
				}
			}
		}
		off = end + 1;
	}
	return 0;
}

char *kpk_meta_render(const KpkMeta *m, size_t *len)
{
	KbBuf b = {0};

	kb_buf_printf(&b, "id          = %s\n", m->id);
	kb_buf_printf(&b, "kind        = %s\n", kpk_kind_name(m->kind));
	kb_buf_printf(&b, "name        = %s\n", m->name);
	kb_buf_printf(&b, "version     = %s\n", m->version);
	kb_buf_printf(&b, "release     = %s\n", m->release);
	kb_buf_printf(&b, "arch        = %s\n", m->arch);
	if (m->summary[0])
		kb_buf_printf(&b, "summary     = %s\n", m->summary);
	if (m->category[0])
		kb_buf_printf(&b, "category    = %s\n", m->category);
	if (m->licence[0])
		kb_buf_printf(&b, "licence     = %s\n", m->licence);
	if (m->description[0]) {
		const char *p = m->description;
		while (*p) {
			const char *nl = strchr(p, '\n');
			int n = nl ? (int)(nl - p) : (int)strlen(p);
			kb_buf_printf(&b, "description = %.*s\n", n, p);
			if (!nl)
				break;
			p = nl + 1;
		}
	}
	for (int i = 0; i < m->nreq; i++) {
		if (m->req[i].op[0])
			kb_buf_printf(&b, "requires    = %s %s %s\n",
				      m->req[i].name, m->req[i].op, m->req[i].ver);
		else
			kb_buf_printf(&b, "requires    = %s\n", m->req[i].name);
	}
	for (int i = 0; i < m->nprov; i++)
		kb_buf_printf(&b, "provides    = %s\n", m->provides[i]);
	for (int i = 0; i < m->ndesktop; i++)
		kb_buf_printf(&b, "desktop     = %s\n", m->desktop[i]);
	for (int i = 0; i < m->nmime; i++)
		kb_buf_printf(&b, "mime        = %s\n", m->mime[i]);
	for (int i = 0; i < m->ncmd; i++)
		kb_buf_printf(&b, "command     = %s\n", m->command[i]);
	for (int i = 0; i < m->nneeds; i++)
		kb_buf_printf(&b, "needs       = %s\n", m->needs[i]);
	for (int i = 0; i < m->ngraft; i++)
		kb_buf_printf(&b, "graft       = %s %s\n", m->graft[i].from,
			      m->graft[i].to);
	for (int i = 0; i < m->nboxgraft; i++)
		kb_buf_printf(&b, "boxgraft    = %s %s\n", m->boxgraft[i].from,
			      m->boxgraft[i].to);
	for (int i = 0; i < m->nenv; i++)
		kb_buf_printf(&b, "env         = %s\n", m->env[i]);
	if (m->installed)
		kb_buf_printf(&b, "installed   = %llu\n", m->installed);
	if (m->launch_cold)
		kb_buf_printf(&b, "launch_cold = %d\n", m->launch_cold);
	if (m->recommended)
		kb_buf_str(&b, "recommended = yes\n");

	if (len)
		*len = b.n;
	return b.p;
}

/*
 * An id is a path component in `/var/lib/kdos/packs` and a filename on the
 * medium, so it is [A-Za-z0-9._-] and nothing else. Every other check here is
 * about a pack being USABLE; this one is about it being SAFE.
 */
static int id_ok(const char *s)
{
	if (!*s || strlen(s) >= KPK_ID_MAX)
		return 0;
	if (*s == '.' || *s == '-')
		return 0;
	for (const char *c = s; *c; c++)
		if (!isalnum((unsigned char)*c) && *c != '.' && *c != '_' &&
		    *c != '-')
			return 0;
	return 1;
}

int kpk_meta_valid(const KpkMeta *m, char *err, size_t cap)
{
	if (!id_ok(m->id)) {
		snprintf(err, cap, "id is missing or not [A-Za-z0-9._-]");
		return -1;
	}
	if (m->kind == KPK_KIND_UNKNOWN) {
		snprintf(err, cap, "%s: kind must be base, runtime, app or data",
			 m->id);
		return -1;
	}
	if (!m->version[0]) {
		snprintf(err, cap, "%s: no version", m->id);
		return -1;
	}
	for (int i = 0; i < m->nreq; i++)
		if (!id_ok(m->req[i].name)) {
			snprintf(err, cap, "%s: requires a name that is not an id",
				 m->id);
			return -1;
		}
	/*
	 * A data pack is mounted noexec and is never composed into a box root,
	 * so a desktop entry or a command in one names something that can
	 * never run. Refusing it at build time is the difference between a
	 * broken menu row and a build error.
	 */
	if (m->kind == KPK_KIND_DATA && (m->ndesktop || m->ncmd)) {
		snprintf(err, cap,
			 "%s: a data pack is mounted noexec — it cannot carry a "
			 "desktop entry or a command", m->id);
		return -1;
	}
	if (m->kind != KPK_KIND_DATA && (m->ngraft || m->nboxgraft)) {
		snprintf(err, cap, "%s: graft and boxgraft are data-pack keys",
			 m->id);
		return -1;
	}
	/*
	 * `env` IS NOT A DATA-PACK KEY, and a runtime is the reason. A boxed Qt
	 * application is themed by QT_QPA_PLATFORMTHEME, and which value works
	 * depends entirely on which platform theme is INSTALLED — which is a
	 * fact about the runtime under it and about nothing else. Declaring it
	 * in the pack that installs the packages is what stops the value and
	 * the packages drifting apart; deriving it anywhere else is a second
	 * copy of the same fact.
	 *
	 * The scope is a box: these are exported into a container the caller
	 * was already going to start, from a pack whose payload hash and
	 * signature were checked before it was mounted.
	 */
	/* A graft destination is joined onto a directory this daemon owns. */
	for (int i = 0; i < m->ngraft; i++)
		if (strstr(m->graft[i].to, "..") ||
		    m->graft[i].to[0] == '/' || strstr(m->graft[i].from, "..")) {
			snprintf(err, cap, "%s: graft path escapes its root", m->id);
			return -1;
		}
	/* A boxgraft lands under ~/.local/share/kdos/packs/<id>, or — as
	 * `~/<path>` — at that place in the home, for a program that reads its
	 * own directory. Either way it may not climb out. */
	for (int i = 0; i < m->nboxgraft; i++) {
		const char *to = m->boxgraft[i].to;
		int home = !strncmp(to, "~/", 2);
		if (strstr(m->boxgraft[i].from, "..") ||
		    (home ? (to[2] == 0 || to[2] == '/' || strstr(to, ".."))
			  : !id_ok(to))) {
			snprintf(err, cap, "%s: boxgraft name is not an id or a "
				 "~/path", m->id);
			return -1;
		}
	}
	if (err && cap)
		err[0] = 0;
	return 0;
}
