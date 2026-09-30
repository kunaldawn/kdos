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
#include "kpkg.h"

/* ──────────────────────────────────────────────────────────────────────── */
/* Discovery                                                                */

/* A regular file at <dir>/<leaf>. */
static int file_at(const char *dir, const char *leaf)
{
	char *path = kb_path_join(dir, leaf);
	int ok = kb_path_exists(path) && !kb_is_dir(path);
	free(path);
	return ok;
}

static int dir_at(const char *dir, const char *leaf)
{
	char *path = kb_path_join(dir, leaf);
	int ok = kb_is_dir(path);
	free(path);
	return ok;
}

int kbuild_is_package_phase(const KbuildPhase *p)
{
	int file = file_at(p->dir_path, KBUILD_PKG_FILE);
	int dir = dir_at(p->dir_path, KBUILD_PKG_DIR);
	if (file && dir)
		return -1;
	return file || dir;
}

char **kbuild_list_files(const KbuildPhase *p, int *count)
{
	if (count)
		*count = 0;
	if (kbuild_is_package_phase(p) <= 0)
		return kb_calloc(1, sizeof(char *));

	if (file_at(p->dir_path, KBUILD_PKG_FILE)) {
		char **out = kb_calloc(2, sizeof(*out));
		out[0] = kb_path_join(p->dir_path, KBUILD_PKG_FILE);
		if (count)
			*count = 1;
		return out;
	}

	/* packages.d/<name>.txt in byte order — kb_listdir sorts with strcmp,
	 * which is the order `LC_ALL=C` gives the same glob in shell. Anything
	 * else in the directory is not part of the list. */
	char *dir = kb_path_join(p->dir_path, KBUILD_PKG_DIR);
	int n = 0;
	char **names = kb_listdir(dir, &n);
	char **out = kb_calloc((size_t)n + 1, sizeof(*out));
	int k = 0;
	for (int i = 0; i < n; i++) {
		size_t len = strlen(names[i]);
		if (names[i][0] == '.' || len < 5 ||
		    strcmp(names[i] + len - 4, ".txt") ||
		    !file_at(dir, names[i]))
			continue;
		out[k++] = kb_path_join(dir, names[i]);
	}
	kb_strv_free(names);
	free(dir);
	if (count)
		*count = k;
	return out;
}

char **kbuild_steps(const KbuildPhase *p, int *count)
{
	if (count)
		*count = 0;

	/* A package phase has no steps at all — the package list IS the
	 * work, and the driver runs it through kpkg rather than through *.sh.
	 * A phase with both lists is refused at discovery, and has no steps
	 * either: running its scripts instead would be a third guess. */
	if (kbuild_is_package_phase(p))
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

	/* Every list file read into one buffer, in order, as if it were one
	 * file. The newline between two files is what keeps the last name of
	 * one from joining the first name of the next when a file does not
	 * end in one. */
	KbBuf all = {0};
	int nfiles = 0;
	char **files = kbuild_list_files(p, &nfiles);
	for (int f = 0; f < nfiles; f++) {
		size_t len = 0;
		char *data = kb_read_all(files[f], &len);
		if (!data)
			continue;
		kb_buf_add(&all, data, len);
		kb_buf_str(&all, "\n");
		free(data);
	}
	kb_strv_free(files);
	char *data = all.p;
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
	kb_buf_free(&all);
	if (count)
		*count = n;
	return out;
}

/* A shelf banner, `# <shelf> — `, the shelf an id of lowercase letters,
 * digits and `-`: testing/phaseclosure.py's SHELF_BANNER_RE. */
static int shelf_banner(const char *s)
{
	if (s[0] != '#' || s[1] != ' ' ||
	    !(islower((unsigned char)s[2]) || isdigit((unsigned char)s[2])))
		return 0;
	const char *p = s + 2;
	while (islower((unsigned char)*p) || isdigit((unsigned char)*p) ||
	       *p == '-')
		p++;
	return !strncmp(p, " \xe2\x80\x94 ", 5);
}

int kbuild_packages_order_run(const KbuildPhase *ph, char ***names)
{
	int split = !file_at(ph->dir_path, KBUILD_PKG_FILE);
	int nfiles = 0;
	char **files = kbuild_list_files(ph, &nfiles);

	int cap = 16, n = 0;
	char **out = kb_calloc((size_t)cap + 1, sizeof(*out));
	for (int f = 0; f < nfiles; f++) {
		if (split && strcmp(kb_basename(files[f]), KBUILD_ORDER_FILE))
			continue;
		size_t len = 0;
		char *data = kb_read_all(files[f], &len);
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
			if (!split && shelf_banner(s))
				break;
			if (!*s || *s == '#')
				continue;
			if (n == cap) {
				cap *= 2;
				char **nv = kb_calloc((size_t)cap + 1,
						      sizeof(*nv));
				memcpy(nv, out, (size_t)n * sizeof(*out));
				free(out);
				out = nv;
			}
			out[n++] = kb_strdup(s);
		}
		free(data);
	}
	kb_strv_free(files);
	*names = out;
	return n;
}

char **kbuild_ports(const char *repo_root, int *count, char *err,
		    size_t errcap)
{
	/* The areas every package phase from 40 on can see. The desktop's own
	 * areas reach the index through their phase's list instead. */
	static const char *REPOS[] = { "ports/core", "src/system", "src/art",
				       NULL };

	if (count)
		*count = 0;
	KbBuf list = {0};
	for (int r = 0; REPOS[r]; r++) {
		char *base = kb_path_join(repo_root, REPOS[r]);
		kb_buf_printf(&list, "%s%s", r ? " " : "", base);
		free(base);
	}

	/* On the heap: a KpConf carries every repository's shelf list. */
	KpConf *c = kb_calloc(1, sizeof(*c));

	/*
	 * NO CORE PORTS IS A BROKEN TREE, NOT AN EMPTY ONE. The picker lists
	 * what this returns, and a list holding only our own ports is
	 * indistinguishable from a tree the walker does not understand.
	 * ports/core is walked alone first so the other areas cannot hide it.
	 */
	char *core = kb_path_join(repo_root, REPOS[0]);
	kp_conf_set_repos(c, core);
	int ncore = 0;
	char **out = kp_ports_scan(c, &ncore, err, errcap);
	kb_strv_free(out);
	out = NULL;
	if (!ncore) {
		if (err && errcap && !err[0])
			snprintf(err, errcap, "no port found under %s", core);
	} else {
		kp_conf_set_repos(c, list.p);
		out = kp_ports_scan(c, count, err, errcap);
	}
	free(core);
	free(c);
	kb_buf_free(&list);
	return out;
}

int kbuild_package_index(const KbuildPhase *ph, int nph, const char *repo_root,
			 KbuildPkgRef *out, int max, char *err, size_t errcap)
{
	int nports = 0;
	char **ports = kbuild_ports(repo_root, &nports, err, errcap);
	if (!ports)
		return -1;

	int n = 0;
	for (int i = 0; i < nports && n < max; i++) {
		kb_strlcpy(out[n].name, ports[i], sizeof(out[n].name));
		out[n].phase[0] = 0;		/* "" — belongs to no phase */
		n++;
	}
	kb_strv_free(ports);

	/* A list entry with no port of its own still lands in the index,
	 * which is how a missing port stays visible. */
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
/* A phase's repositories, as the host sees them                            */

/* Nothing but blanks and an optional comment from here to the end. */
static int tail_is_comment(const char *t)
{
	while (*t && isspace((unsigned char)*t))
		t++;
	return !*t || *t == '#';
}

/* The value of one `[export ]PORT_REPO=` line, or -1 when the line is not
 * one. The match is testing/phaseclosure.py's REPO_RE, lazy value and all:
 * a quoted value ends at the first matching quote followed only by blanks
 * and a comment, and when no quote closes that way the quote is part of an
 * unquoted value, which ends where only blanks and a comment remain. */
static int port_repo_value(const char *line, char *out, size_t cap)
{
	const char *s = line;
	while (*s && isspace((unsigned char)*s))
		s++;
	if (!strncmp(s, "export", 6) && isspace((unsigned char)s[6])) {
		s += 6;
		while (*s && isspace((unsigned char)*s))
			s++;
	}
	if (strncmp(s, "PORT_REPO=", 10))
		return -1;
	s += 10;

	const char *v = s;
	size_t len = (size_t)-1;
	if (*s == '"' || *s == '\'') {
		for (const char *e = s + 1; *e; e++)
			if (*e == *s && tail_is_comment(e + 1)) {
				v = s + 1;
				len = (size_t)(e - v);
				break;
			}
	}
	if (len == (size_t)-1)
		for (len = 0; !tail_is_comment(v + len); len++)
			;
	if (len >= cap)
		len = cap - 1;
	memcpy(out, v, len);
	out[len] = 0;
	return 0;
}

int kbuild_phase_repos(const KbuildPhase *ph, const char *repo_root,
		       char *out, size_t cap)
{
	/* The last assignment wins, as it would in the sourced file. */
	char value[2048] = "/ports/core";
	size_t len = 0;
	char *data = ph->env_file[0] ? kb_read_all(ph->env_file, &len) : NULL;
	for (char *line = data, *next; line && *line; line = next) {
		char *nl = strchr(line, '\n');
		next = nl ? nl + 1 : NULL;
		if (nl)
			*nl = 0;
		port_repo_value(line, value, sizeof(value));
	}
	free(data);

	KbBuf b = {0};
	int n = 0;
	for (char *t = strtok(value, " \t\r\n"); t; t = strtok(NULL, " \t\r\n")) {
		const char *rel;
		if (!strncmp(t, "/ports/", 7) || !strcmp(t, "/ports"))
			rel = t + 1;			/* <repo>/ports/...  */
		else if (!strncmp(t, "/kdos/", 6))
			rel = t + 6;			/* <repo>/...        */
		else
			for (rel = t; *rel == '/'; rel++)
				;
		kb_buf_printf(&b, "%s%s/%s", n ? " " : "", repo_root, rel);
		n++;
	}
	int fits = b.n < cap;
	if (cap)
		kb_strlcpy(out, fits && b.p ? b.p : "", cap);
	kb_buf_free(&b);
	return fits ? n : -1;
}

/* ──────────────────────────────────────────────────────────────────────── */
/* CLI token splitting                                                      */

/* `--rebuild "a, b c"` — spaces become commas, then split, then strip. Only a
 * space separates: a tab is stripped off the ends and never splits, so a
 * tab-bearing value arrives whole and the caller below is the one that
 * refuses it. */
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

	/* Phase keys in name order, not run order, and each phase's scripts
	 * sorted too: one plan has to summarise the same way every time. */
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
	 * `--phases system` and `--phases 41_system` resolve. */
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
		/*
		 * REBUILD IS THE ONLY FREE-FORM FIELD. Phases and steps are
		 * resolved against real directories above; a rebuild token is
		 * whatever was typed, and it is written into the plan file
		 * between bare quotes. A quote or a backslash in it produces a
		 * file that neither this loader nor any JSON reader can parse,
		 * so the next build silently narrows to something other than
		 * what was asked. A package name has none of those characters,
		 * so refusing here costs nothing and catches the nonsense name
		 * before the build starts.
		 */
		for (const char *c = raw[i]; *c; c++) {
			if (isalnum((unsigned char)*c) || *c == '.' ||
			    *c == '_' || *c == '+' || *c == '-')
				continue;
			snprintf(err, errcap,
				 "--rebuild wants a package name, got '%s'",
				 raw[i]);
			return -1;
		}

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
/* Persistence — two-space-indented JSON the loader round-trips             */

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
	/* Replaced, never truncated in place. A save killed part-way through
	 * an open-and-write leaves a file the loader can only read as "no
	 * plan", which turns the next build into a full one with no warning;
	 * a temp-and-rename leaves the previous plan intact instead. */
	char *path = kb_path_join(build_dir, KBUILD_PLAN_FILE);
	int rc = kb_write_file_atomic(path, b.p ? b.p : "");
	free(path);
	kb_buf_free(&b);
	return rc;
}

/* A scanner, not a JSON parser: the file has three known keys holding a null,
 * an object of string arrays, and two string arrays.
 *
 * IT CONSUMES THE WHOLE DOCUMENT. Every value is scanned to its own end, the
 * steps object must close, and the last value must be followed by the
 * document's closing brace as the last non-space byte. A truncated file —
 * which a kill or a full disk during kbuild_plan_save leaves behind — would
 * otherwise read as a valid narrowing plan, and the next build would skip
 * phases without a word. Anything that does not scan clean to the end is "no
 * plan", which runs everything. */
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
	 * runs everything, where a file that is not a plan has to fail. */
	const char *head = skip_ws(data);
	const char *tail = data + len;
	while (tail > head && isspace((unsigned char)tail[-1]))
		tail--;
	if (*head != '{' || tail <= head || tail[-1] != '}')
		goto out;

	/* The end of the last value scanned. The document's closing brace has
	 * to follow it, or a file cut off mid-object reads as a whole plan. */
	const char *last = head + 1;

	const char *v = json_key(data, "phases");
	if (v && !strncmp(v, "null", 4)) {
		pl->has_phases = 0;
		last = v + 4;
	} else if (v) {
		int n = json_strings(v, pl->phase, KBUILD_MAX_PHASES, &last);
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
		if (*s != '}')
			goto out;
		last = s + 1;
	}

	v = json_key(data, "rebuild");
	if (v) {
		int n = json_strings(v, pl->rebuild, KBUILD_MAX_REBUILD, &last);
		if (n < 0)
			goto out;
		pl->nrebuild = n;
	}

	/* The closing brace must be the last non-space byte, and must come
	 * straight after the last value scanned: a file cut off mid-object
	 * otherwise reads as a whole plan, because the inner object's brace
	 * satisfies a test that only looks at the document's ends. */
	last = skip_ws(last);
	if (last != tail - 1 || *last != '}')
		goto out;
	rc = 0;
out:
	free(data);
	if (rc < 0)
		memset(pl, 0, sizeof(*pl));
	return rc;
}
