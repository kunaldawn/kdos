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
 *   pending    "3 applications chosen during install  persistent: install
 *              are not installed yet"
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

/* ── what the installer could not install ─────────────────────────────── */

/*
 * THE INSTALLER'S CHOICES THAT ARE STILL WAITING. When kinstall can neither
 * import nor build an application it was asked for, it records the id in
 * /var/lib/kdos/apps-pending, and Discover's KDOS backend shows those under
 * "Selected during install". The notice says so once per content of the
 * file and is withdrawn when the file names nothing, which is when Discover
 * has installed them all.
 */
void kn_parse_pending(const char *text, KnPending *out)
{
	char *copy = kb_strdup(text ? text : ""), *save = NULL;
	KbBuf ids = { 0 };

	memset(out, 0, sizeof(*out));
	for (char *l = strtok_r(copy, "\n", &save); l;
	     l = strtok_r(NULL, "\n", &save)) {
		while (*l == ' ' || *l == '\t')
			l++;
		size_t n = strlen(l);
		while (n && (l[n - 1] == ' ' || l[n - 1] == '\t' ||
			     l[n - 1] == '\r'))
			l[--n] = '\0';
		if (!*l || *l == '#' || strpbrk(l, " \t"))
			continue;
		out->n++;
		kb_buf_printf(&ids, "%s\n", l);
	}
	hash_hex(ids.p ? ids.p : "", out->key, sizeof(out->key));
	kb_buf_free(&ids);
	free(copy);
}

int kn_decide_pending(const KnPending *pd, const KnState *s, KnPost *out)
{
	if (pd->n <= 0) {
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
	out->icon = "plasmadiscover";
	out->urgency = 1;
	out->persistent = 1;
	if (pd->n == 1)
		kb_strlcpy(out->summary, "An application chosen during install "
			   "is not installed yet", sizeof(out->summary));
	else
		snprintf(out->summary, sizeof(out->summary),
			 "%d applications chosen during install are not "
			 "installed yet", pd->n);
	kb_strlcpy(out->body, "Discover lists them under Selected during "
		   "install, and installs them from there.",
		   sizeof(out->body));
	out->actions[0] = KN_ACT_DETAILS;
	out->actions[1] = "Open Discover";
	return 1;
}

/* ── firmware the hardware asks for ───────────────────────────────────── */

/*
 * A VENDOR FIRMWARE PORT THIS MACHINE NEEDS AND HAS NOT GOT. The medium
 * carries the common vendors' firmware; a device whose driver wants another
 * vendor's, or a firmware load the kernel logged as failed, names the port
 * that ships it. Said once per set of ports, withdrawn when `kdos firmware
 * sync` (or anything else) has installed them all.
 */
int kn_decide_firmware(const KnFirmware *f, const KnState *s, KnPost *out)
{
	char names[KN_FW_PORTS][128], set[KN_FW_PORTS * 65 + 1], key[32];
	char list[200];
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
	if (f->n == 1)
		snprintf(out->body, sizeof(out->body),
			 "Its firmware is not installed (%s). Install it with: "
			 "sudo kdos firmware sync", f->reason[0]);
	else
		kb_strlcpy(out->body, "Their firmware is not installed. "
			   "Install it with: sudo kdos firmware sync",
			   sizeof(out->body));
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
		kb_argv_add(a, "sudo");
		kb_argv_add(a, "kdos");
		kb_argv_add(a, "firmware");
		kb_argv_add(a, "sync");
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
