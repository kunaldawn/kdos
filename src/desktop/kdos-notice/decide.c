/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   decide.c — which notice, and when it is news
 *
 * ONE NOTIFICATION PER CHANGE OF STATE. Each kind reduces its facts to a key
 * — the set of processes, the set of findings, the boot and its slot — and
 * posts only when the key differs from the one it last posted (state.c keeps
 * it across logins). The same CVE list is not announced at every login; a
 * new one replaces the old notification rather than stacking beside it.
 *
 * THE NOTICES, AND WHO MAKES A DECISION:
 *
 *   restarts   "Firefox and Konsole were updated"     persistent: restart
 *              "The desktop was updated"              persistent: log out
 *   cve        "3 known vulnerabilities in …"         persistent: update
 *   trial      "Running KDOS 0.2.14 on trial"         timed, once per boot
 *   rollback   "Your last update didn't start"        timed, once per slot
 *   oom        "Firefox was closed to free memory"    timed, per kill
 *   pending    "Software chosen during install is     persistent: install
 *              not installed yet"
 *   firmware   "This computer's hardware needs        persistent: install
 *              firmware-nvidia"
 *
 * Only a notice that waits for a person to act stays until dismissed; the
 * rest expire, and Plasma keeps them in its history.
 *
 * A notice whose state ends — every replaced program restarted, every CVE
 * fixed, the trial kept — is withdrawn, so the history does not keep a
 * claim that is no longer true on the screen.
 * ---------------------------------
 */

#include <grp.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "notice.h"

static const char *const kind_names[KN_COUNT] = {
	"restarts", "cve", "trial", "rollback", "oom", "pending", "firmware",
};

const char *kn_kind_name(int kind)
{
	return kind >= 0 && kind < KN_COUNT ? kind_names[kind] : "";
}

int kn_kind_of(const char *name)
{
	for (int i = 0; i < KN_COUNT; i++)
		if (!strcmp(kind_names[i], name))
			return i;
	return -1;
}

/* FNV-1a, as hex: a key that fits one line of the state file whatever the
 * size of the set it stands for. */
static void hash_hex(const char *s, char *out, size_t n)
{
	unsigned long long h = 1469598103934665603ULL;

	for (; *s; s++) {
		h ^= (unsigned char)*s;
		h *= 1099511628211ULL;
	}
	snprintf(out, n, "%016llx", h);
}

static int cmp_str(const void *a, const void *b)
{
	return strcmp(*(char *const *)a, *(char *const *)b);
}

static void post_close(int kind, KnPost *out)
{
	memset(out, 0, sizeof(*out));
	out->kind = kind;
	out->close = 1;
}

/* "A", "A and B", "A, B and C", "A, B and 3 others". */
static void join_names(char names[][128], int n, const char *others,
		       char *out, size_t cap)
{
	size_t o = 0;
	int shown = n > 3 ? 2 : n;

	out[0] = '\0';
	for (int i = 0; i < shown; i++) {
		const char *sep = !i ? "" : (i == n - 1 && n <= 3) ? " and " : ", ";
		int r = snprintf(out + o, cap - o, "%s%s", sep, names[i]);

		if (r < 0 || (size_t)r >= cap - o)
			return;
		o += (size_t)r;
	}
	if (n > 3)
		snprintf(out + o, cap - o, " and %d %s", n - 2, others);
}

/* ── restarts ──────────────────────────────────────────────────────────── */

int kn_decide_restarts(const KnRestarts *r, const char *boot,
		       const KnState *s, KnPost *out)
{
	char *pids[KN_PROCS], buf[KN_PROCS][300], set[KN_PROCS * 300 + 1];
	char names[KN_PROCS][128], key[192], h[17];
	int n = 0, napps = 0, desk = 0, other = 0;
	size_t o = 0;

	if (!r->ok)
		return 0;
	if (!r->n) {
		if (!s->key[KN_RESTARTS][0])
			return 0;
		post_close(KN_RESTARTS, out);
		return 1;
	}
	for (int i = 0; i < r->n; i++) {
		snprintf(buf[i], sizeof(buf[i]), "%d:%s", r->p[i].pid,
			 r->p[i].exe[0] ? r->p[i].exe : r->p[i].comm);
		pids[n++] = buf[i];
	}
	qsort(pids, (size_t)n, sizeof(pids[0]), cmp_str);
	set[0] = '\0';
	for (int i = 0; i < n; i++) {
		size_t l = strlen(pids[i]);

		memcpy(set + o, pids[i], l);
		o += l;
		set[o++] = '\n';
		set[o] = '\0';
	}
	hash_hex(set, h, sizeof(h));
	snprintf(key, sizeof(key), "%s:%s", boot, h);
	if (!strcmp(key, s->key[KN_RESTARTS]))
		return 0;

	/* One name per application, however many of its processes. */
	for (int i = 0; i < r->n; i++) {
		const KnProc *p = &r->p[i];
		int seen = 0;

		if (p->desktop) {
			desk = 1;
			continue;
		}
		if (!p->name[0]) {
			other++;
			continue;
		}
		for (int k = 0; k < napps; k++)
			seen |= !strcmp(names[k], p->name);
		if (!seen)
			kb_strlcpy(names[napps++], p->name, sizeof(names[0]));
	}

	memset(out, 0, sizeof(*out));
	out->kind = KN_RESTARTS;
	kb_strlcpy(out->key, key, sizeof(out->key));
	out->icon = "system-software-update";
	out->persistent = 1;
	out->urgency = 1;
	if (desk) {
		char apps[512];

		kb_strlcpy(out->summary, "The desktop was updated",
			   sizeof(out->summary));
		join_names(names, napps, "other applications", apps, sizeof(apps));
		snprintf(out->body, sizeof(out->body),
			 "Log out and back in to run the new version.%s%s%s",
			 napps ? " " : "", apps, napps ? (napps == 1 ?
			 " was updated too." : " were updated too.") : "");
		out->actions[0] = KN_ACT_LOGOUT;
		out->actions[1] = "Log Out";
		out->actions[2] = KN_ACT_DETAILS;
		out->actions[3] = "Show Programs";
	} else if (napps) {
		char apps[200];

		join_names(names, napps, "other applications", apps, sizeof(apps));
		snprintf(out->summary, sizeof(out->summary), "%s %s updated", apps,
			 napps == 1 ? "was" : "were");
		snprintf(out->body, sizeof(out->body),
			 "Restart %s to run the new version.%s",
			 napps == 1 ? "it" : "them",
			 other ? " Some background programs are running the "
				 "old version too." : "");
		out->actions[0] = KN_ACT_DETAILS;
		out->actions[1] = "Show Programs";
	} else {
		kb_strlcpy(out->summary, "Background programs were updated",
			   sizeof(out->summary));
		snprintf(out->body, sizeof(out->body),
			 "%d running %s the old version. Restart %s, or restart "
			 "the computer.", other, other == 1 ? "program is still "
			 "using" : "programs are still using",
			 other == 1 ? "it" : "them");
		out->actions[0] = KN_ACT_DETAILS;
		out->actions[1] = "Show Programs";
	}
	return 1;
}

/* ── CVE ───────────────────────────────────────────────────────────────── */

int kn_decide_cve(const KnCve *c, const KnState *s, KnPost *out)
{
	KbBuf set = { 0 };
	char *rows[KN_FINDINGS], buf[KN_FINDINGS][720], h[17], ports[512];
	int fixable = 0;

	if (!c->ok)
		return 0;
	if (!c->n) {
		if (!s->key[KN_CVE][0])
			return 0;
		post_close(KN_CVE, out);
		return 1;
	}
	for (int i = 0; i < c->n; i++) {
		snprintf(buf[i], sizeof(buf[i]), "%s %s %s", c->f[i].port,
			 c->f[i].version, c->f[i].cves);
		rows[i] = buf[i];
		fixable += c->f[i].fixed[0] != '\0';
	}
	qsort(rows, (size_t)c->n, sizeof(rows[0]), cmp_str);
	for (int i = 0; i < c->n; i++) {
		kb_buf_str(&set, rows[i]);
		kb_buf_str(&set, "\n");
	}
	hash_hex(set.p, h, sizeof(h));
	kb_buf_free(&set);
	if (!strcmp(h, s->key[KN_CVE]))
		return 0;

	memset(out, 0, sizeof(*out));
	out->kind = KN_CVE;
	kb_strlcpy(out->key, h, sizeof(out->key));
	out->icon = "security-medium";
	out->persistent = 1;
	out->urgency = 1;
	snprintf(out->summary, sizeof(out->summary),
		 "%d known %s in installed packages", c->ncve,
		 c->ncve == 1 ? "vulnerability" : "vulnerabilities");
	{
		/* join_names wants every name to say "and N others". */
		char all[KN_FINDINGS][128];
		int n = c->n < KN_FINDINGS ? c->n : KN_FINDINGS;

		for (int i = 0; i < n; i++)
			kb_strlcpy(all[i], c->f[i].port, sizeof(all[i]));
		join_names(all, n, "other packages", ports, sizeof(ports));
	}
	snprintf(out->body, sizeof(out->body), "In %s. %s", ports,
		 fixable == c->n ? "An update fixes every one of them."
		 : fixable ? "An update fixes some of them."
		 : "No fixed version is known yet.");
	out->actions[0] = KN_ACT_DETAILS;
	out->actions[1] = "Details";
	return 1;
}

/* ── the trial and the rollback ────────────────────────────────────────── */

static const char *slot_version(const KnBoot *b, const char *slot)
{
	int i = !strcmp(slot, "b");

	return b->ver[i][0] ? b->ver[i] : NULL;
}

int kn_decide_trial(const KnBoot *b, const char *boot, const KnState *s,
		    KnPost *out)
{
	char key[192];
	const char *v, *other;
	size_t bl = strlen(boot);

	if (!b->ok)
		return 0;
	if (!b->trying[0] || strcmp(b->running, b->trying)) {
		/* This boot's trial ended — kept or abandoned — while the
		 * notice was up: take it down. A key from another boot is
		 * gone with that boot already. */
		if (s->id[KN_TRIAL] && !strncmp(s->key[KN_TRIAL], boot, bl) &&
		    s->key[KN_TRIAL][bl] == ':') {
			post_close(KN_TRIAL, out);
			return 1;
		}
		return 0;
	}
	v = slot_version(b, b->running);
	snprintf(key, sizeof(key), "%s:%s:%s", boot, b->running, v ? v : "");
	if (!strcmp(key, s->key[KN_TRIAL]))
		return 0;
	other = b->active[0] && strcmp(b->active, b->running)
		? slot_version(b, b->active) : NULL;

	memset(out, 0, sizeof(*out));
	out->kind = KN_TRIAL;
	kb_strlcpy(out->key, key, sizeof(out->key));
	out->icon = "system-software-update";
	out->urgency = 0;
	if (v)
		snprintf(out->summary, sizeof(out->summary),
			 "Running KDOS %s on trial", v);
	else
		snprintf(out->summary, sizeof(out->summary),
			 "Running the new system version on trial");
	if (other)
		snprintf(out->body, sizeof(out->body),
			 "It is kept once this session has been up for a "
			 "minute. If it does not start properly, the computer "
			 "goes back to KDOS %s.", other);
	else
		snprintf(out->body, sizeof(out->body),
			 "It is kept once this session has been up for a "
			 "minute. If it does not start properly, the computer "
			 "goes back to the version before it.");
	out->actions[0] = KN_ACT_DETAILS;
	out->actions[1] = "Details";
	return 1;
}

int kn_decide_rollback(const KnBoot *b, const KnState *s, KnPost *out)
{
	char key[192];
	const char *now, *fv;
	int fi;

	if (!b->ok || !b->failed[0] || !strcmp(b->failed, b->running))
		return 0;
	fi = !strcmp(b->failed, "b");
	snprintf(key, sizeof(key), "%s:%s:%s", b->failed, b->ver[fi],
		 b->build[fi]);
	if (!strcmp(key, s->key[KN_ROLLBACK]))
		return 0;
	now = b->running[0] ? slot_version(b, b->running) : NULL;
	fv = b->ver[fi][0] ? b->ver[fi] : NULL;

	memset(out, 0, sizeof(*out));
	out->kind = KN_ROLLBACK;
	kb_strlcpy(out->key, key, sizeof(out->key));
	out->icon = "dialog-warning";
	out->urgency = 1;
	kb_strlcpy(out->summary, "Your last update didn't start",
		   sizeof(out->summary));
	if (now && fv)
		snprintf(out->body, sizeof(out->body),
			 "KDOS %s did not start properly, so KDOS went back to "
			 "%s.", fv, now);
	else if (now)
		snprintf(out->body, sizeof(out->body),
			 "So KDOS went back to %s.", now);
	else
		kb_strlcpy(out->body, "So KDOS went back to the version "
				      "before it.", sizeof(out->body));
	out->actions[0] = KN_ACT_DETAILS;
	out->actions[1] = "Details";
	return 1;
}

/* ── the memory killer ─────────────────────────────────────────────────── */

int kn_decide_oom(const KnOom *o, int prev, KnPost *out)
{
	char comm[64] = "", box[96] = "", mb[32] = "";
	const char *p;
	int n;

	if (!o->ok || prev < 0 || o->kills <= prev)
		return 0;
	n = o->kills - prev;
	/* "comm (appbox www) (pid 9, 812 MB)" or "comm (pid 9, 812 MB)". */
	p = o->last;
	{
		size_t l = strcspn(p, "(");

		while (l && p[l - 1] == ' ')
			l--;
		if (l >= sizeof(comm))
			l = sizeof(comm) - 1;
		memcpy(comm, p, l);
		comm[l] = '\0';
	}
	if ((p = strstr(o->last, "(appbox "))) {
		size_t l = strcspn(p + 8, ")");

		if (l < sizeof(box)) {
			memcpy(box, p + 8, l);
			box[l] = '\0';
		}
	}
	if ((p = strstr(o->last, ", ")) && strstr(p, " MB)")) {
		size_t l = strcspn(p + 2, " ");

		if (l < sizeof(mb)) {
			memcpy(mb, p + 2, l);
			mb[l] = '\0';
		}
	}

	memset(out, 0, sizeof(*out));
	out->kind = KN_OOM;
	snprintf(out->key, sizeof(out->key), "%d", o->kills);
	out->icon = "dialog-warning";
	out->urgency = 1;
	if (n > 1)
		snprintf(out->summary, sizeof(out->summary),
			 "%d programs were closed to free memory", n);
	else if (box[0])
		snprintf(out->summary, sizeof(out->summary),
			 "%s in the box %s was closed to free memory",
			 comm[0] ? comm : "A program", box);
	else
		snprintf(out->summary, sizeof(out->summary),
			 "%s was closed to free memory",
			 comm[0] ? comm : "A program");
	if (n > 1 && comm[0])
		snprintf(out->body, sizeof(out->body),
			 "The computer was running out of memory. The last was "
			 "%s%s%s.", comm, box[0] ? " in the box " : "", box);
	else if (mb[0])
		snprintf(out->body, sizeof(out->body),
			 "The computer was running out of memory, and it was "
			 "using %s MB.", mb);
	else
		kb_strlcpy(out->body, "The computer was running out of memory.",
			   sizeof(out->body));
	return 1;
}

/* ── what the installer left to install ───────────────────────────────── */

/*
 * THE INSTALLER'S CHOICES THAT ARE STILL WAITING. kinstall keeps two records
 * on the new disk. /var/lib/kdos/apps-pending names the boxed selections it
 * was asked for, which it never builds; Discover's KDOS backend shows them
 * under "Selected during install" and empties the file as it installs them.
 * /var/lib/kdos/groups-pending names the native groups chosen and not
 * installed: recorded alone on a medium that names no binary host, or left
 * when the binary host did not answer or a group failed. The body says only
 * that they are not installed yet, which is true of all three; Discover does
 * not list those, and `kdos update install --pending` installs them and
 * empties that file. A line is one id in either file, so the boxed half counts
 * selections, and the native half counts groups and names each by the title
 * its `group <id> <title>` line in /ports/groups gives it, the name the
 * installer offered it by; a group with no title there is named by its id.
 *
 * One notice covers both, keyed on both files' ids and not on the titles,
 * so a tree whose title changed does not say it again: it is said once per
 * content of the pair, and withdrawn when neither names anything. The body
 * says what each file holds and how it is installed, in plain text with the
 * command alone on a line of its own, since Plasma draws Markdown as the
 * characters it is written in and wraps a long line after a hyphen; an
 * untitled id is quoted, with non-breaking hyphens. One group, or the count
 * alone, ends with the command. Several groups alone put a one-line count
 * and the command first, then "They are:" and every title. When boxed
 * selections wait too, the groups' text and the command come first and
 * Discover's one-line count follows. In every layout the command stays
 * inside the popup's first lines rather than below its scroll. Open
 * Discover, and Discover's icon, are offered only when apps-pending names
 * something, since Discover has nothing to show for a group; a notice of
 * groups alone, whose remedy is a terminal command, carries the
 * software-install icon.
 */
/* A package name as kpkg and `kdos update` spell one. */
static int name_ok(const char *s)
{
	size_t n = strlen(s);

	return n && n < 128 && *s != '.' && *s != '-' &&
	       strspn(s, "abcdefghijklmnopqrstuvwxyz"
			 "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789._+-") == n;
}

/*
 * The title /ports/groups gives group `id`, read as probe.c and `kdos
 * update` read that file: a `#` starts a comment, and a `group <id>
 * <title>` line names it. "" when the file has no such line or no title.
 */
static void group_title(const char *titles, const char *id, char *out,
			size_t cap)
{
	size_t il = strlen(id);

	out[0] = '\0';
	for (const char *l = titles; l && *l;) {
		const char *nl = strchr(l, '\n');
		size_t len = nl ? (size_t)(nl - l) : strlen(l);
		const char *h = memchr(l, '#', len);
		const char *e;

		if (h)
			len = (size_t)(h - l);
		e = l + len;
		while (l < e && (*l == ' ' || *l == '\t'))
			l++;
		if (e - l > 6 && !strncmp(l, "group", 5) &&
		    (l[5] == ' ' || l[5] == '\t')) {
			const char *g = l + 6;

			while (g < e && (*g == ' ' || *g == '\t'))
				g++;
			if ((size_t)(e - g) >= il && !strncmp(g, id, il) &&
			    (g + il == e || g[il] == ' ' || g[il] == '\t' ||
			     g[il] == '\r')) {
				const char *t = g + il;

				while (t < e && (*t == ' ' || *t == '\t'))
					t++;
				while (e > t && (e[-1] == ' ' || e[-1] == '\t' ||
						 e[-1] == '\r'))
					e--;
				if (e > t) {
					size_t tl = (size_t)(e - t);

					if (tl >= cap)
						tl = cap - 1;
					memcpy(out, t, tl);
					out[tl] = '\0';
				}
				return;
			}
		}
		l = nl ? nl + 1 : l + strlen(l);
	}
}

/* Append `s` with the three characters Plasma's notification body reads
 * as markup written as entities, so a title is drawn as it is spelt. */
static void append_escaped(KbBuf *b, const char *s)
{
	for (; *s; s++) {
		if (*s == '&')
			kb_buf_str(b, "&amp;");
		else if (*s == '<')
			kb_buf_str(b, "&lt;");
		else if (*s == '>')
			kb_buf_str(b, "&gt;");
		else
			kb_buf_add(b, s, 1);
	}
}

/*
 * An untitled group's id as display text: in typographic quotes, each
 * hyphen a non-breaking one (U+2011), since Qt breaks a line after a hyphen
 * and "nosuch-" / "group" reads as two words. The id is never a command
 * argument here; the body's command line is plain ASCII.
 */
static void append_id(KbBuf *b, const char *id)
{
	kb_buf_str(b, "“");
	for (; *id; id++) {
		if (*id == '-')
			kb_buf_str(b, "\u2011");
		else
			kb_buf_add(b, id, 1);
	}
	kb_buf_str(b, "”");
}

/*
 * Count the ids in one record and append them to `ids`. Without `names` a
 * line is read as Discover reads apps-pending: trimmed, and skipped when it
 * is empty, a `#` comment or has a space inside. With `names` it is read as
 * `kdos update install --pending` reads groups-pending: cut at the first
 * space, tab or `#`, and counted when what is left is a package name, so
 * the notice names exactly what that command installs; each group is also
 * written into `names` by its title in `titles`, or its id as append_id
 * writes it, one name a line: a title may hold a comma or an "and", so any
 * separator inside a line would read as more groups than there are, and
 * Plasma's notification server turns each newline into a line break.
 * `names` is left empty when they do not fit in `cap`, since a cut list
 * would name a group that is not there.
 */
static int count_ids(const char *text, KbBuf *ids, const char *titles,
		     char *names, size_t cap, int *untitled)
{
	char *copy = kb_strdup(text ? text : ""), *save = NULL;
	KbBuf list = { 0 };
	int n = 0;

	if (names)
		names[0] = '\0';
	for (char *l = strtok_r(copy, "\n", &save); l;
	     l = strtok_r(NULL, "\n", &save)) {
		size_t len;

		if (names) {
			l[strcspn(l, " \t\r#")] = '\0';
			if (!name_ok(l))
				continue;
		} else {
			while (*l == ' ' || *l == '\t')
				l++;
			len = strlen(l);
			while (len && (l[len - 1] == ' ' ||
				       l[len - 1] == '\t' ||
				       l[len - 1] == '\r'))
				l[--len] = '\0';
			if (!*l || *l == '#' || strpbrk(l, " \t"))
				continue;
		}
		n++;
		kb_buf_printf(ids, "%s\n", l);
		if (names) {
			char title[192];

			group_title(titles, l, title, sizeof(title));
			if (!title[0])
				(*untitled)++;
			if (title[0])
				append_escaped(&list, title);
			else
				append_id(&list, l);
			kb_buf_str(&list, "\n");
		}
	}
	if (names && list.p) {
		size_t o = 0;
		int fits = 1;

		for (char *l = strtok_r(list.p, "\n", &save); l && fits;
		     l = strtok_r(NULL, "\n", &save)) {
			size_t need = (o ? 1 : 0) + strlen(l);

			if (o + need + 1 > cap) {
				fits = 0;
				break;
			}
			o += (size_t)snprintf(names + o, cap - o, "%s%s",
					      o ? "\n" : "", l);
		}
		if (!fits)
			names[0] = '\0';
	}
	kb_buf_free(&list);
	free(copy);
	return n;
}

void kn_parse_pending(const char *apps, const char *groups,
		      const char *titles, KnPending *out)
{
	KbBuf ids = { 0 };

	memset(out, 0, sizeof(*out));
	out->n = count_ids(apps, &ids, NULL, NULL, 0, NULL);
	kb_buf_printf(&ids, "--\n");
	out->ngroups = count_ids(groups, &ids, titles, out->groups,
				 sizeof(out->groups), &out->untitled);
	hash_hex(ids.p ? ids.p : "", out->key, sizeof(out->key));
	kb_buf_free(&ids);
}

/* The command on a line of its own, so no wrap splits it at a hyphen:
 * Plasma's notification server turns the newline into a line break, and the
 * line is the command alone, plain ASCII, as a reader copies it: `sudo` for
 * an administrator, `su -c` for any other account (user_admin below). "binary
 * host" is the installer's term for the same server. Each line fits the
 * popup's KN_POPUP_COLS unwrapped, so the command costs two popup lines
 * either way. */
#define KN_PENDING_SUDO "Once a binary host answers, run:\n" \
			"sudo kdos update install --pending"
#define KN_PENDING_SU "Once a binary host answers, run:\n" \
		      "su -c 'kdos update install --pending'"

/*
 * WHO RUNS THE COMMAND. sudo grants on membership of `wheel` (the sudo
 * port's `%wheel ALL=(ALL) ALL`), and an account the installer made without
 * the Administrator choice is outside it: sudo refuses that account, so its
 * notices name `su -c` and root's password instead. Membership is read from
 * the group database (getgrouplist), as sudo reads it, on every decision, so
 * an account added to `wheel` is told `sudo` at the next notice.
 * $KDOS_NOTICE_ADMIN_GROUP names another group in place of `wheel`, for the
 * tests; no group, no account or no answer reads as outside it.
 */
static int user_admin(void)
{
	const char *name = getenv("KDOS_NOTICE_ADMIN_GROUP");
	struct passwd *pw = getpwuid(getuid());
	struct group *gr;
	gid_t want, gids[256];
	int n = (int)(sizeof(gids) / sizeof(gids[0]));

	gr = getgrnam(name && *name ? name : "wheel");
	if (!gr || !pw)
		return 0;
	want = gr->gr_gid;
	if (getgrouplist(pw->pw_name, pw->pw_gid, gids, &n) < 0)
		return 0;
	for (int i = 0; i < n; i++)
		if (gids[i] == want)
			return 1;
	return 0;
}

/* What Plasma's notification popup shows before it scrolls: eight lines,
 * each 38 to 41 characters wide on a 1920x1080 session as its proportional
 * font's glyphs fall, so the count takes the narrow end. A body with groups
 * and boxed apps is laid out to these, so the command and the boxed-app
 * line stand above the scroll; a wider figure lets a title that wraps in
 * the popup push the boxed line below it. */
#define KN_POPUP_LINES 8
#define KN_POPUP_COLS 38

/* The popup lines `text` takes, each newline-separated line wrapped at word
 * breaks to KN_POPUP_COLS as the popup wraps it. A UTF-8 character and a
 * markup entity each count as one character, as they are drawn; a word
 * wider than the popup takes as many lines as it fills. */
static int popup_lines(const char *text)
{
	int lines = 0;

	for (const char *b = text; *b;) {
		const char *e = strchr(b, '\n');
		const char *end = e ? e : b + strlen(b);
		int col = 0;

		lines++;
		for (const char *w = b; w < end;) {
			int n = 0;

			while (w < end && *w == ' ')
				w++;
			if (w == end)
				break;
			while (w < end && *w != ' ') {
				if (*w == '&') {
					const char *semi = memchr(w, ';',
								  (size_t)(end - w));

					if (semi && semi - w <= 8)
						w = semi;
				}
				if (((unsigned char)*w & 0xC0) != 0x80)
					n++;
				w++;
			}
			if (col && col + 1 + n > KN_POPUP_COLS) {
				lines++;
				col = 0;
			}
			col += (col ? 1 : 0) + n;
			while (col > KN_POPUP_COLS) {
				lines++;
				col -= KN_POPUP_COLS;
			}
		}
		b = e ? e + 1 : end;
	}
	return lines;
}

/* Several groups by title, inside `budget` popup lines: a one-line header,
 * as many titles as fit, and "and N more" when not all do. The summary
 * already says the software was chosen during the install and is not
 * installed yet, so the header only counts. 0 when not even one title fits
 * beside the "and N more" line, and the caller says the count alone. */
static int list_groups(const KnPending *pd, int budget, char *out,
		       size_t cap)
{
	char titles[sizeof(pd->groups)], *save = NULL;
	const char *t[64];
	int n = 0, shown = 0, used;
	size_t o;

	kb_strlcpy(titles, pd->groups, sizeof(titles));
	for (char *l = strtok_r(titles, "\n", &save); l && n < 64;
	     l = strtok_r(NULL, "\n", &save))
		t[n++] = l;
	used = 1;
	for (int i = 0; i < n; i++)
		used += popup_lines(t[i]);
	if (n == pd->ngroups && used <= budget) {
		shown = n;
	} else {
		used = 2;
		while (shown < n && used + popup_lines(t[shown]) <= budget)
			used += popup_lines(t[shown++]);
		if (!shown)
			return 0;
	}
	o = (size_t)snprintf(out, cap, "%d KDOS application groups:",
			     pd->ngroups);
	for (int i = 0; i < shown && o < cap; i++)
		o += (size_t)snprintf(out + o, cap - o, "\n%s", t[i]);
	if (shown < pd->ngroups && o < cap)
		o += (size_t)snprintf(out + o, cap - o, "\nand %d more",
				      pd->ngroups - shown);
	return o < cap;
}

/* Several groups and nothing else waiting: a one-line count, the command,
 * then "They are:" and every title, one a line. The summary already says
 * the software was chosen during the install and is not installed yet, so
 * the count line says no more than that the groups wait, and the command
 * stands on the popup's third line. */
static void groups_alone(const KnPending *pd, const char *run, char *out,
			 size_t cap)
{
	char titles[sizeof(pd->groups)], *save = NULL;
	size_t o;

	kb_strlcpy(titles, pd->groups, sizeof(titles));
	o = (size_t)snprintf(out, cap, "%d KDOS application groups wait.\n"
			     "%s\nThey are:", pd->ngroups, run);
	for (char *l = strtok_r(titles, "\n", &save); l && o < cap;
	     l = strtok_r(NULL, "\n", &save))
		o += (size_t)snprintf(out + o, cap - o, "\n%s", l);
}

int kn_decide_pending(const KnPending *pd, const KnState *s, KnPost *out)
{
	char apps[384] = "", groups[640] = "";
	const char *run;
	int budget;

	if (pd->n <= 0 && pd->ngroups <= 0) {
		if (s->id[KN_PENDING] || s->key[KN_PENDING][0]) {
			post_close(KN_PENDING, out);
			return 1;
		}
		return 0;
	}
	if (!strcmp(pd->key, s->key[KN_PENDING]))
		return 0;
	memset(out, 0, sizeof(*out));
	out->kind = KN_PENDING;
	kb_strlcpy(out->key, pd->key, sizeof(out->key));
	out->icon = pd->n > 0 ? "plasmadiscover" : "system-software-install";
	out->urgency = 1;
	out->persistent = 1;
	kb_strlcpy(out->summary, "Software chosen during install is not "
		   "installed yet", sizeof(out->summary));
	/* Plasma's popup shows the top KN_POPUP_LINES of a body and scrolls
	 * the rest, so what the reader must act on goes first. Both kinds
	 * waiting: the groups and their command come first, and the boxed apps
	 * follow as one short line that names what it counts, the body's last.
	 * The command is the one thing the reader cannot get elsewhere (Open
	 * Discover is a button), and the boxed line is the only words saying
	 * apps wait too, so both stay inside KN_POPUP_LINES: the group list
	 * gives up titles to keep them there. Several groups alone put a
	 * one-line count and the command before "They are:" and every title,
	 * with no budget, so a scroll hides nothing but titles: the notice is
	 * the one place on the desktop that names them, since Discover does
	 * not list a group and there is no Details action to show the rest. */
	run = user_admin() ? KN_PENDING_SUDO : KN_PENDING_SU;
	if (pd->n == 1 && pd->ngroups > 0)
		kb_strlcpy(apps, "1 boxed app also waits in Discover.",
			   sizeof(apps));
	else if (pd->n > 1 && pd->ngroups > 0)
		snprintf(apps, sizeof(apps), "%d boxed apps also wait in "
			 "Discover.", pd->n);
	else if (pd->n == 1)
		kb_strlcpy(apps, "1 selection is waiting: Discover lists it "
			   "under Selected during install, and installs it "
			   "from there.", sizeof(apps));
	else if (pd->n > 1)
		snprintf(apps, sizeof(apps), "%d selections are waiting: "
			 "Discover lists them under Selected during install, "
			 "and installs them from there.", pd->n);
	budget = KN_POPUP_LINES - popup_lines(run) -
		 popup_lines(apps);
	/* "Not installed yet" and never "could not be installed": a medium
	 * with no binary host records the groups without trying them. */
	if (pd->ngroups == 1 && pd->groups[0])
		snprintf(groups, sizeof(groups), "%s%s was chosen during the "
			 "install and is not installed yet. %s",
			 pd->untitled ? "The KDOS application group " : "",
			 pd->groups, run);
	else if (pd->ngroups == 1)
		snprintf(groups, sizeof(groups), "1 KDOS application group "
			 "chosen during the install is not installed yet. %s",
			 run);
	else if (pd->ngroups > 1 && pd->groups[0] && !apps[0])
		groups_alone(pd, run, groups, sizeof(groups));
	else if (pd->ngroups > 1 && pd->groups[0] &&
		 list_groups(pd, budget, groups, sizeof(groups) -
			     strlen(run) - 1))
		snprintf(groups + strlen(groups), sizeof(groups) -
			 strlen(groups), "\n%s", run);
	else if (pd->ngroups > 1)
		snprintf(groups, sizeof(groups), "%d KDOS application groups "
			 "chosen during the install are not installed yet. %s",
			 pd->ngroups, run);
	snprintf(out->body, sizeof(out->body), "%s%s%s", groups,
		 apps[0] && groups[0] ? "\n" : "", apps);
	if (pd->n > 0) {
		out->actions[0] = KN_ACT_DETAILS;
		out->actions[1] = "Open Discover";
	}
	return 1;
}

/* ── firmware the hardware asks for ───────────────────────────────────── */

/*
 * A VENDOR FIRMWARE PORT THIS MACHINE NEEDS AND HAS NOT GOT. The medium
 * carries the common vendors' firmware; a device whose driver wants another
 * vendor's, or a firmware load the kernel logged as failed, names the port
 * that ships it. Said once per set of ports, withdrawn when `kdos firmware
 * sync` (or anything else) has installed them all. Install runs the sync in
 * Konsole through sudo, or for an account outside `wheel` through `su -c`,
 * which asks for root's password there (user_admin).
 */
int kn_decide_firmware(const KnFirmware *f, const KnState *s, KnPost *out)
{
	char names[KN_FW_PORTS][128], set[KN_FW_PORTS * 65 + 1], key[32];
	char list[200];
	const char *run;
	size_t o = 0;

	if (!f->ok)
		return 0;
	if (!f->n) {
		if (!s->key[KN_FIRMWARE][0])
			return 0;
		post_close(KN_FIRMWARE, out);
		return 1;
	}
	set[0] = '\0';
	for (int i = 0; i < f->n; i++) {
		int r = snprintf(set + o, sizeof(set) - o, "%s\n", f->port[i]);

		if (r > 0 && (size_t)r < sizeof(set) - o)
			o += (size_t)r;
		kb_strlcpy(names[i], f->port[i], sizeof(names[i]));
	}
	hash_hex(set, key, sizeof(key));
	if (!strcmp(key, s->key[KN_FIRMWARE]))
		return 0;
	memset(out, 0, sizeof(*out));
	out->kind = KN_FIRMWARE;
	kb_strlcpy(out->key, key, sizeof(out->key));
	out->icon = "hwinfo";
	out->urgency = 1;
	out->persistent = 1;
	join_names(names, f->n, "more", list, sizeof(list));
	snprintf(out->summary, sizeof(out->summary),
		 "This computer's hardware needs %s", list);
	run = user_admin() ? "sudo kdos firmware sync"
			   : "su -c 'kdos firmware sync' (root's password)";
	if (f->n == 1)
		snprintf(out->body, sizeof(out->body),
			 "Its firmware is not installed (%s). Install it with: "
			 "%s", f->reason[0], run);
	else
		snprintf(out->body, sizeof(out->body), "Their firmware is not "
			 "installed. Install it with: %s", run);
	out->actions[0] = KN_ACT_DETAILS;
	out->actions[1] = "Install";
	return 1;
}

/* ── what the actions run ──────────────────────────────────────────────── */

int kn_action_argv(int kind, const char *action, KbArgv *a)
{
	memset(a, 0, sizeof(*a));
	if (!strcmp(action, "default"))
		action = KN_ACT_DETAILS;
	if (strcmp(action, KN_ACT_DETAILS))
		return 0;
	switch (kind) {
	case KN_RESTARTS:
	case KN_CVE:
		kb_argv_add(a, "konsole");
		kb_argv_add(a, "--hold");
		kb_argv_add(a, "-e");
		kb_argv_add(a, "kdos");
		kb_argv_add(a, kind == KN_CVE ? "cve" : "restarts");
		break;
	case KN_TRIAL:
	case KN_ROLLBACK:
		kb_argv_add(a, "systemsettings");
		kb_argv_add(a, "kcm_kdos_system");
		break;
	case KN_FIRMWARE:
		kb_argv_add(a, "konsole");
		kb_argv_add(a, "--hold");
		kb_argv_add(a, "-e");
		if (user_admin()) {
			kb_argv_add(a, "sudo");
			kb_argv_add(a, "kdos");
			kb_argv_add(a, "firmware");
			kb_argv_add(a, "sync");
		} else {
			kb_argv_add(a, "su");
			kb_argv_add(a, "-c");
			kb_argv_add(a, "kdos firmware sync");
		}
		break;
	case KN_PENDING:
		kb_argv_add(a, "plasma-discover");
		kb_argv_add(a, "--category");
		kb_argv_add(a, "Selected during install");
		break;
	default:
		return 0;
	}
	kb_argv_end(a);
	return 1;
}
