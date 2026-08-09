/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   libkbuild — the build plan: what the next run narrows itself to
 * ---------------------------------
 */

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "kbuild.h"

/* ──────────────────────────────────────────────────────────────────────── */
/* Discovery                                                                */

char **kbuild_steps(const KbuildPhase *p, int *count)
{
	if (count)
		*count = 0;

	/* A packages.txt phase has no steps at all — the package list IS the
	 * work, and build.py runs it through kpkg rather than through *.sh. */
	char *pkgs = kb_path_join(p->dir_path, "packages.txt");
	int is_pkg_phase = kb_path_exists(pkgs) && !kb_is_dir(pkgs);
	free(pkgs);
	if (is_pkg_phase)
		return kb_calloc(1, sizeof(char *));

	int n = 0;
	char **all = kb_listdir(p->dir_path, &n);
	if (!all)
		return kb_calloc(1, sizeof(char *));

	char **out = kb_calloc((size_t)n + 1, sizeof(*out));
	int k = 0;
	for (int i = 0; i < n; i++) {
		size_t len = strlen(all[i]);
		/* glob("*.sh") semantics: a leading dot is never matched by a
		 * leading wildcard, and kb_listdir does not filter them. */
		if (all[i][0] == '.' || len < 3 || strcmp(all[i] + len - 3, ".sh"))
			continue;
		out[k++] = kb_strdup(all[i]);
	}
	kb_strv_free(all);
	if (count)
		*count = k;
	return out;
}

char **kbuild_packages(const KbuildPhase *p, int *count)
{
	if (count)
		*count = 0;

	char *path = kb_path_join(p->dir_path, "packages.txt");
	size_t len = 0;
	char *data = kb_read_all(path, &len);
	free(path);
	if (!data)
		return kb_calloc(1, sizeof(char *));

	int cap = 64, n = 0;
	char **out = kb_calloc((size_t)cap + 1, sizeof(*out));
	for (char *line = data, *next; line && *line; line = next) {
		char *nl = strchr(line, '\n');
		next = nl ? nl + 1 : NULL;
		if (nl)
			*nl = 0;

		char *s = line;
		while (*s && isspace((unsigned char)*s))
			s++;
		char *e = s + strlen(s);
		while (e > s && isspace((unsigned char)e[-1]))
			e--;
		*e = 0;
		if (!*s || *s == '#')
			continue;

		if (n == cap) {
			cap *= 2;
			char **nv = kb_calloc((size_t)cap + 1, sizeof(*nv));
			memcpy(nv, out, (size_t)n * sizeof(*out));
			free(out);
			out = nv;
		}
		out[n++] = kb_strdup(s);
	}
	free(data);
	if (count)
		*count = n;
	return out;
}

static int cmp_str(const void *a, const void *b)
{
	return strcmp(*(char *const *)a, *(char *const *)b);
}

char **kbuild_ports(const char *repo_root, int *count)
{
	static const char *REPOS[] = { "ports/core", "src/packages", NULL };

	if (count)
		*count = 0;
	int cap = 512, n = 0;
	char **out = kb_calloc((size_t)cap + 1, sizeof(*out));

	for (int r = 0; REPOS[r]; r++) {
		char *base = kb_path_join(repo_root, REPOS[r]);
		char **names = kb_listdir(base, NULL);
		if (!names) {
			free(base);
			continue;
		}
		for (char **e = names; *e; e++) {
			char *dir = kb_path_join(base, *e);
			char *recipe = kb_path_join(dir, "kpkgbuild");
			int have = kb_path_exists(recipe) && !kb_is_dir(recipe);
			free(recipe);
			free(dir);
			if (!have)
				continue;

			int dup = 0;		/* a set, so the second repo loses */
			for (int i = 0; i < n && !dup; i++)
				dup = !strcmp(out[i], *e);
			if (dup || n == cap)
				continue;
			out[n++] = kb_strdup(*e);
		}
		kb_strv_free(names);
		free(base);
	}

	qsort(out, (size_t)n, sizeof(*out), cmp_str);
	if (count)
		*count = n;
	return out;
}

int kbuild_package_index(const KbuildPhase *ph, int nph, const char *repo_root,
			 KbuildPkgRef *out, int max)
{
	int nports = 0;
	char **ports = kbuild_ports(repo_root, &nports);

	int n = 0;
	for (int i = 0; i < nports && n < max; i++) {
		kb_strlcpy(out[n].name, ports[i], sizeof(out[n].name));
		out[n].phase[0] = 0;		/* "" — belongs to no phase */
		n++;
	}
	kb_strv_free(ports);

	/* A packages.txt entry with no port of its own still lands in the index
	 * (python appends it), which is how a missing port stays visible. */
	for (int p = 0; p < nph; p++) {
		int npkg = 0;
		char **pkgs = kbuild_packages(&ph[p], &npkg);
		for (int i = 0; i < npkg; i++) {
			int at = -1;
			for (int k = 0; k < n; k++)
				if (!strcmp(out[k].name, pkgs[i])) {
					at = k;
					break;
				}
			if (at < 0) {
				if (n == max)
					continue;
				at = n++;
				kb_strlcpy(out[at].name, pkgs[i],
					   sizeof(out[at].name));
			}
			kb_strlcpy(out[at].phase, ph[p].dir_name,
				   sizeof(out[at].phase));
		}
		kb_strv_free(pkgs);
	}
	return n;
}

/* ──────────────────────────────────────────────────────────────────────── */
/* CLI token splitting                                                      */

/* `--rebuild "a, b c"` — spaces become commas, then split, then strip. A tab
 * is NOT a separator (python replaces only " "), it is only stripped off the
 * ends. Reproduced rather than tidied: a plan file round-trips through both. */
static int split_tokens(const char *value, char out[][64], int max)
{
	if (!value || !*value)
		return 0;

	char tmp[4096];
	kb_strlcpy(tmp, value, sizeof(tmp));
	for (char *c = tmp; *c; c++)
		if (*c == ' ')
			*c = ',';

	int n = 0;
	char *save = tmp;
	while (save && n < max) {
		char *comma = strchr(save, ',');
		if (comma)
			*comma = 0;

		char *s = save;
		while (*s && isspace((unsigned char)*s))
			s++;
		char *e = s + strlen(s);
		while (e > s && isspace((unsigned char)e[-1]))
			e--;
		*e = 0;
		if (*s)
			kb_strlcpy(out[n++], s, 64);

		save = comma ? comma + 1 : NULL;
	}
	return n;
}

/* ──────────────────────────────────────────────────────────────────────── */
/* Queries                                                                  */

int kbuild_plan_custom(const KbuildPlan *pl)
{
	return pl->has_phases || pl->nsteps || pl->nrebuild;
}

int kbuild_plan_narrows(const KbuildPlan *pl)
{
	return pl->has_phases || pl->nsteps;
}

int kbuild_plan_phase_selected(const KbuildPlan *pl, const char *dir_name)
{
	if (!pl->has_phases)
		return 1;
	for (int i = 0; i < pl->nphase; i++)
		if (!strcmp(pl->phase[i], dir_name))
			return 1;
	return 0;
}

int kbuild_plan_step_selected(const KbuildPlan *pl, const char *dir_name,
			      const char *basename)
{
	for (int i = 0; i < pl->nsteps; i++) {
		if (strcmp(pl->steps[i].dir, dir_name))
			continue;
		for (int k = 0; k < pl->steps[i].n; k++)
			if (!strcmp(pl->steps[i].step[k], basename))
				return 1;
		return 0;
	}
	return 1;		/* no step list for this phase: run them all */
}

int kbuild_plan_forced(const KbuildPlan *pl, const char *package)
{
	for (int i = 0; i < pl->nrebuild; i++)
		if (!strcmp(pl->rebuild[i], package))
			return 1;
	return 0;
}

static void sorted_copy(char dst[][64], const char src[][64], int n)
{
	for (int i = 0; i < n; i++)
		memcpy(dst[i], src[i], 64);
	for (int i = 1; i < n; i++) {
		char key[64];
		memcpy(key, dst[i], 64);
		int k = i - 1;
		while (k >= 0 && strcmp(dst[k], key) > 0) {
			memcpy(dst[k + 1], dst[k], 64);
			k--;
		}
		memcpy(dst[k + 1], key, 64);
	}
}

void kbuild_plan_summary(const KbuildPlan *pl, char *out, size_t cap)
{
	KbBuf b = {0};
	int first = 1;

	if (pl->has_phases) {
		char s[KBUILD_MAX_PHASES][64];
		sorted_copy(s, pl->phase, pl->nphase);
		kb_buf_str(&b, "phases: ");
		if (!pl->nphase)
			kb_buf_str(&b, "none");
		for (int i = 0; i < pl->nphase; i++)
			kb_buf_printf(&b, "%s%s", i ? ", " : "", s[i]);
		first = 0;
	}

	/* python iterates `sorted(self.steps)` — the phase keys, not the run
	 * order — and sorts each phase's scripts too. */
	int order[KBUILD_MAX_PHASES];
	for (int i = 0; i < pl->nsteps; i++)
		order[i] = i;
	for (int i = 1; i < pl->nsteps; i++) {
		int key = order[i], k = i - 1;
		while (k >= 0 && strcmp(pl->steps[order[k]].dir,
					pl->steps[key].dir) > 0) {
			order[k + 1] = order[k];
			k--;
		}
		order[k + 1] = key;
	}
	for (int i = 0; i < pl->nsteps; i++) {
		const KbuildPlanSteps *st = &pl->steps[order[i]];
		char s[KBUILD_MAX_STEPS][64];
		sorted_copy(s, st->step, st->n);
		kb_buf_printf(&b, "%s%s steps: ", first ? "" : "; ", st->dir);
		for (int k = 0; k < st->n; k++)
			kb_buf_printf(&b, "%s%s", k ? ", " : "", s[k]);
		first = 0;
	}

	if (pl->nrebuild) {
		char s[KBUILD_MAX_REBUILD][64];
		sorted_copy(s, pl->rebuild, pl->nrebuild);
		kb_buf_printf(&b, "%srebuild: ", first ? "" : "; ");
		for (int i = 0; i < pl->nrebuild; i++)
			kb_buf_printf(&b, "%s%s", i ? ", " : "", s[i]);
	}

	kb_strlcpy(out, b.p ? b.p : "", cap);
	kb_buf_free(&b);
}

/* ──────────────────────────────────────────────────────────────────────── */
/* Construction                                                             */

int kbuild_plan_from_cli(KbuildPlan *pl, const char *phases_arg,
			 const char *steps_arg, const char *rebuild_arg,
			 const KbuildPhase *ph, int nph, char *err, size_t errcap)
{
	memset(pl, 0, sizeof(*pl));
	if (err && errcap)
		err[0] = 0;

	/* A token is a phase's directory name or its short name, so both
	 * `--phases phase4` and `--phases 04_phase4` resolve. */
	char tok[KBUILD_MAX_PHASES][64];

	if (phases_arg && *phases_arg) {
		pl->has_phases = 1;
		int n = split_tokens(phases_arg, tok, KBUILD_MAX_PHASES);
		for (int i = 0; i < n; i++) {
			const KbuildPhase *m = kbuild_find(ph, nph, tok[i]);
			if (!m) {
				snprintf(err, errcap, "unknown phase: %s", tok[i]);
				return -1;
			}
			int dup = 0;
			for (int k = 0; k < pl->nphase; k++)
				dup |= !strcmp(pl->phase[k], m->dir_name);
			if (!dup)
				kb_strlcpy(pl->phase[pl->nphase++], m->dir_name,
					   sizeof(pl->phase[0]));
		}
	}

	if (steps_arg && *steps_arg) {
		int n = split_tokens(steps_arg, tok, KBUILD_MAX_PHASES);
		for (int i = 0; i < n; i++) {
			char *colon = strchr(tok[i], ':');
			if (!colon) {
				snprintf(err, errcap,
					 "--steps wants PHASE:script.sh, got '%s'",
					 tok[i]);
				return -1;
			}
			*colon = 0;
			const char *script = colon + 1;

			const KbuildPhase *m = kbuild_find(ph, nph, tok[i]);
			if (!m) {
				snprintf(err, errcap, "unknown phase: %s", tok[i]);
				return -1;
			}

			int nk = 0;
			char **known = kbuild_steps(m, &nk);
			int ok = (nk == 0);
			for (int k = 0; k < nk && !ok; k++)
				ok = !strcmp(known[k], script);
			if (!ok) {
				KbBuf list = {0};
				for (int k = 0; k < nk; k++)
					kb_buf_printf(&list, "%s%s", k ? ", " : "",
						      known[k]);
				snprintf(err, errcap,
					 "%s has no step '%s' (has: %s)",
					 m->dir_name, script,
					 list.p ? list.p : "");
				kb_buf_free(&list);
				kb_strv_free(known);
				return -1;
			}
			kb_strv_free(known);

			KbuildPlanSteps *slot = NULL;
			for (int k = 0; k < pl->nsteps; k++)
				if (!strcmp(pl->steps[k].dir, m->dir_name))
					slot = &pl->steps[k];
			if (!slot && pl->nsteps < KBUILD_MAX_PHASES) {
				slot = &pl->steps[pl->nsteps++];
				kb_strlcpy(slot->dir, m->dir_name,
					   sizeof(slot->dir));
			}
			if (slot && slot->n < KBUILD_MAX_STEPS) {
				int dup = 0;
				for (int k = 0; k < slot->n; k++)
					dup |= !strcmp(slot->step[k], script);
				if (!dup)
					kb_strlcpy(slot->step[slot->n++], script,
						   sizeof(slot->step[0]));
			}

			/* Naming a step of a phase implies running that phase. */
			if (pl->has_phases) {
				int dup = 0;
				for (int k = 0; k < pl->nphase; k++)
					dup |= !strcmp(pl->phase[k], m->dir_name);
				if (!dup)
					kb_strlcpy(pl->phase[pl->nphase++],
						   m->dir_name,
						   sizeof(pl->phase[0]));
			}
		}
		if (!pl->has_phases) {
			pl->has_phases = 1;
			for (int k = 0; k < pl->nsteps; k++)
				kb_strlcpy(pl->phase[pl->nphase++],
					   pl->steps[k].dir, sizeof(pl->phase[0]));
		}
	}

	/* A set, like the other two: `--rebuild zlib,zlib` forces zlib once. */
	static char raw[KBUILD_MAX_REBUILD][64];
	int nrb = split_tokens(rebuild_arg, raw, KBUILD_MAX_REBUILD);
	for (int i = 0; i < nrb; i++) {
		int dup = 0;
		for (int k = 0; k < pl->nrebuild && !dup; k++)
			dup = !strcmp(pl->rebuild[k], raw[i]);
		if (!dup)
			kb_strlcpy(pl->rebuild[pl->nrebuild++], raw[i],
				   sizeof(pl->rebuild[0]));
	}
	return 0;
}

const KbuildPhase *kbuild_find(const KbuildPhase *ph, int n, const char *token)
{
	for (int i = 0; i < n; i++)
		if (!strcmp(ph[i].dir_name, token) || !strcmp(ph[i].name, token))
			return &ph[i];
	return NULL;
}

/* ──────────────────────────────────────────────────────────────────────── */
/* Persistence — byte-identical to python's json.dump(..., indent=2)         */

static void json_list(KbBuf *b, const char dst[][64], int n, const char *indent)
{
	if (!n) {
		kb_buf_str(b, "[]");
		return;
	}
	char s[KBUILD_MAX_REBUILD][64];
	sorted_copy(s, dst, n);
	kb_buf_str(b, "[\n");
	for (int i = 0; i < n; i++)
		kb_buf_printf(b, "%s  \"%s\"%s\n", indent, s[i],
			      i + 1 < n ? "," : "");
	kb_buf_printf(b, "%s]", indent);
}

int kbuild_plan_save(const KbuildPlan *pl, const char *build_dir)
{
	KbBuf b = {0};
	kb_buf_str(&b, "{\n  \"phases\": ");
	if (!pl->has_phases)
		kb_buf_str(&b, "null");
	else
		json_list(&b, pl->phase, pl->nphase, "  ");

	kb_buf_str(&b, ",\n  \"steps\": ");
	if (!pl->nsteps) {
		kb_buf_str(&b, "{}");
	} else {
		kb_buf_str(&b, "{\n");
		for (int i = 0; i < pl->nsteps; i++) {
			kb_buf_printf(&b, "    \"%s\": ", pl->steps[i].dir);
			json_list(&b, pl->steps[i].step, pl->steps[i].n, "    ");
			kb_buf_printf(&b, "%s\n", i + 1 < pl->nsteps ? "," : "");
		}
		kb_buf_str(&b, "  }");
	}

	kb_buf_str(&b, ",\n  \"rebuild\": ");
	json_list(&b, pl->rebuild, pl->nrebuild, "  ");
	kb_buf_str(&b, "\n}");

	if (kb_mkdir_p(build_dir) < 0) {
		kb_buf_free(&b);
		return -1;
	}
	char *path = kb_path_join(build_dir, KBUILD_PLAN_FILE);
	int rc = kb_write_all(path, b.p, b.n);
	free(path);
	kb_buf_free(&b);
	return rc;
}

/* A scanner, not a JSON parser: the file has three known keys holding a null,
 * an object of string arrays, and two string arrays. Anything else in it is
 * ignored, and a malformed file reads as "no plan" rather than as a partial
 * one — a half-read plan would silently skip phases. */
static const char *skip_ws(const char *s)
{
	while (*s && isspace((unsigned char)*s))
		s++;
	return s;
}

static const char *json_key(const char *s, const char *key)
{
	char pat[64];
	snprintf(pat, sizeof(pat), "\"%s\"", key);
	const char *at = strstr(s, pat);
	if (!at)
		return NULL;
	at = skip_ws(at + strlen(pat));
	if (*at != ':')
		return NULL;
	return skip_ws(at + 1);
}

/* Reads `[ "a", "b" ]` into dst, returns the count, or -1 when s is not an
 * array. Leaves *end just past the closing bracket. */
static int json_strings(const char *s, char dst[][64], int max, const char **end)
{
	if (*s != '[')
		return -1;
	s++;
	int n = 0;
	for (;;) {
		s = skip_ws(s);
		if (*s == ']') {
			if (end)
				*end = s + 1;
			return n;
		}
		if (*s != '"')
			return -1;
		s++;
		const char *q = strchr(s, '"');
		if (!q)
			return -1;
		if (n < max) {
			size_t len = (size_t)(q - s);
			if (len >= 64)
				len = 63;
			memcpy(dst[n], s, len);
			dst[n][len] = 0;
			n++;
		}
		s = skip_ws(q + 1);
		if (*s == ',')
			s++;
		else if (*s != ']')
			return -1;
	}
}

int kbuild_plan_load(KbuildPlan *pl, const char *build_dir)
{
	memset(pl, 0, sizeof(*pl));

	char *path = kb_path_join(build_dir, KBUILD_PLAN_FILE);
	size_t len = 0;
	char *data = kb_read_all(path, &len);
	free(path);
	if (!data)
		return -1;

	int rc = -1;

	/* The document has to BE an object. Without this a file that is not
	 * JSON at all parses as "no keys found" — which reads as the plan that
	 * runs everything, the opposite of python's "no plan". */
	const char *head = skip_ws(data);
	const char *tail = data + len;
	while (tail > head && isspace((unsigned char)tail[-1]))
		tail--;
	if (*head != '{' || tail <= head || tail[-1] != '}')
		goto out;

	const char *v = json_key(data, "phases");
	if (v && !strncmp(v, "null", 4)) {
		pl->has_phases = 0;
	} else if (v) {
		int n = json_strings(v, pl->phase, KBUILD_MAX_PHASES, NULL);
		if (n < 0)
			goto out;
		pl->has_phases = 1;
		pl->nphase = n;
	}

	v = json_key(data, "steps");
	if (v && *v == '{') {
		const char *s = skip_ws(v + 1);
		while (*s == '"') {
			const char *q = strchr(s + 1, '"');
			if (!q)
				goto out;
			if (pl->nsteps == KBUILD_MAX_PHASES)
				goto out;
			KbuildPlanSteps *slot = &pl->steps[pl->nsteps];
			size_t klen = (size_t)(q - s - 1);
			if (klen >= sizeof(slot->dir))
				klen = sizeof(slot->dir) - 1;
			memcpy(slot->dir, s + 1, klen);
			slot->dir[klen] = 0;

			s = skip_ws(q + 1);
			if (*s != ':')
				goto out;
			s = skip_ws(s + 1);
			const char *end = NULL;
			int n = json_strings(s, slot->step, KBUILD_MAX_STEPS, &end);
			if (n < 0)
				goto out;
			slot->n = n;
			pl->nsteps++;

			s = skip_ws(end);
			if (*s == ',')
				s = skip_ws(s + 1);
		}
	}

	v = json_key(data, "rebuild");
	if (v) {
		int n = json_strings(v, pl->rebuild, KBUILD_MAX_REBUILD, NULL);
		if (n < 0)
			goto out;
		pl->nrebuild = n;
	}
	rc = 0;
out:
	free(data);
	if (rc < 0)
		memset(pl, 0, sizeof(*pl));
	return rc;
}
