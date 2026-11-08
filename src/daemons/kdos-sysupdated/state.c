/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   state.c — what the files say: the update document, update.state, the
 *             running root's version and the boot state
 *
 * THE DAEMON COMPUTES NONE OF ITS ANSWERS. What is behind, whether a newer
 * slot image or the binhost covers it, and which lane an update takes is
 * `kdos update check`'s, written to
 * /var/lib/kdos/update.json; what each slot holds and which one boots next
 * is `kdos-bootctl status --json`'s. A second program re-deriving either
 * would be a second answer that drifts, so this file only reads them.
 * ---------------------------------
 */

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "sysupdated.h"

/* ── the running root's version ───────────────────────────────────────── */

/* One KEY=value of an os-release-shaped file, quotes off, printable ASCII
 * only; "" when absent. The same reading kdos-bootctl does. */
static void osr_value(const char *data, const char *key, char *out, size_t n)
{
	size_t kl = strlen(key);

	out[0] = 0;
	for (const char *l = data; l && *l;) {
		const char *nl = strchr(l, '\n');
		size_t len = nl ? (size_t)(nl - l) : strlen(l);
		if (len > kl && !strncmp(l, key, kl) && l[kl] == '=') {
			const char *v = l + kl + 1, *end = l + len;
			char q = 0;
			size_t o = 0;
			if (v < end && (*v == '"' || *v == '\''))
				q = *v++;
			for (; v < end && o + 1 < n; v++) {
				unsigned char ch = (unsigned char)*v;
				if (q && *v == q)
					break;
				if (ch < 0x20 || ch > 0x7e || ch == '"' ||
				    ch == '\'' || ch == '\\')
					continue;
				out[o++] = *v;
			}
			while (o && out[o - 1] == ' ')
				o--;
			out[o] = 0;
			return;
		}
		l = nl ? nl + 1 : NULL;
	}
}

/*
 * The version this system is: /etc/kdos-release's VERSION, which the image
 * and every `kdos update` that fills a root write, else os-release's
 * VERSION_ID with `+BUILD_ID` when the image was stamped. kdos-bootctl names
 * slots by the same rule (bootctl.c, ident_from_root), so a version this
 * daemon reports and the one the logout screen shows are one string.
 */
void state_version(char *out, size_t cap)
{
	char p[1024], vid[24] = "", bid[32] = "";
	char *d;

	out[0] = 0;
	su_path("/etc/kdos-release", p, sizeof(p));
	if ((d = kb_read_all(p, NULL))) {
		osr_value(d, "VERSION", out, cap);
		free(d);
		if (out[0])
			return;
	}
	su_path("/etc/os-release", p, sizeof(p));
	if ((d = kb_read_all(p, NULL))) {
		osr_value(d, "VERSION_ID", vid, sizeof(vid));
		osr_value(d, "BUILD_ID", bid, sizeof(bid));
		free(d);
	}
	if (vid[0])
		snprintf(out, cap, "%s%s%s", vid, bid[0] ? "+" : "", bid);
}

/* ── update.state ──────────────────────────────────────────────────────── */

/* The `<step> <code> <time> <detail>` line of update.state; 0 when found. */
static int state_line(const char *step, char *code, size_t ccap,
		      long long *when, char *detail, size_t dcap)
{
	char p[1024];
	size_t sl = strlen(step);
	int found = -1;

	code[0] = 0;
	if (detail)
		detail[0] = 0;
	*when = 0;
	su_path("/var/lib/kdos/update.state", p, sizeof(p));
	char *data = kb_read_all(p, NULL);
	for (char *l = data; l && *l && found;) {
		char *nl = strchr(l, '\n');
		if (nl)
			*nl = 0;
		if (!strncmp(l, step, sl) && l[sl] == ' ') {
			char *q = l + sl + 1;
			size_t cl = strcspn(q, " ");
			snprintf(code, ccap, "%.*s", (int)cl, q);
			q += cl;
			while (*q == ' ')
				q++;
			*when = atoll(q);
			q += strcspn(q, " ");
			while (*q == ' ')
				q++;
			if (detail)
				kb_strlcpy(detail, strcmp(q, "-") ? q : "", dcap);
			found = 0;
		}
		l = nl ? nl + 1 : NULL;
	}
	free(data);
	return found;
}

/* When the network was last asked for a newer tree; 0 for never. */
long long state_fetch_time(void)
{
	char code[32];
	long long t;
	return state_line("fetch", code, sizeof(code), &t, NULL, 0) == 0 ? t : 0;
}

static int fetch_offline(void)
{
	char code[32];
	long long t;
	return state_line("fetch", code, sizeof(code), &t, NULL, 0) == 0 &&
	       !strcmp(code, "no-network");
}

/* The slot an acquire filled and the version it holds, waiting for its arm;
 * 0 when there is one. */
int state_acquired(char *slot, size_t scap, char *ver, size_t vcap)
{
	char code[32], detail[256];
	long long t;

	slot[0] = ver[0] = 0;
	if (state_line("apply", code, sizeof(code), &t, detail,
		       sizeof(detail)) != 0 || strcmp(code, "acquired"))
		return -1;
	/* `slot <x> <version>` */
	if (strncmp(detail, "slot ", 5) || !detail[5] || detail[6] != ' ')
		return -1;
	snprintf(slot, scap, "%c", detail[5]);
	kb_strlcpy(ver, detail + 7, vcap);
	return ver[0] ? 0 : -1;
}

/* ── the update document ───────────────────────────────────────────────── */

static void doc_clear(void)
{
	su.have_doc = 0;
	su.offer[0] = su.installed[0] = su.tree[0] = su.date[0] = 0;
	su.lane[0] = 0;
	su.problem[0] = su.detail[0] = 0;
	su.signer_key[0] = su.signer_name[0] = su.channel[0] = 0;
	su.verified = 0;
	free(su.notes);
	su.notes = NULL;
	for (int i = 0; i < su.npkgs; i++)
		free(su.pkgs[i]);
	free(su.pkgs);
	su.pkgs = NULL;
	su.npkgs = 0;
	su.behind = 0;
	su.size = 0;
}

static void doc_str(const char *doc, const char *key, char *out, size_t cap)
{
	const char *v = json_get(doc, key);
	out[0] = 0;
	if (v)
		json_string(v, out, cap);
}

/*
 * /var/lib/kdos/update.json, as `kdos update check --out` writes it: one
 * line, rewritten whole by rename. A missing or unreadable document is a
 * machine with nothing offered, never a stale offer kept from before.
 */
void state_reload_doc(void)
{
	char p[1024];
	char *doc;

	doc_clear();
	su_path("/var/lib/kdos/update.json", p, sizeof(p));
	if ((doc = kb_read_all(p, NULL)) && json_ws(doc)[0] == '{') {
		const char *v;
		su.have_doc = 1;
		doc_str(doc, "offer", su.offer, sizeof(su.offer));
		doc_str(doc, "lane", su.lane, sizeof(su.lane));
		doc_str(doc, "installed", su.installed, sizeof(su.installed));
		doc_str(doc, "tree", su.tree, sizeof(su.tree));
		doc_str(doc, "date", su.date, sizeof(su.date));
		doc_str(doc, "problem", su.problem, sizeof(su.problem));
		doc_str(doc, "detail", su.detail, sizeof(su.detail));
		doc_str(doc, "channel", su.channel, sizeof(su.channel));
		if ((v = json_get(doc, "behind")))
			su.behind = (int)json_number(v);
		if ((v = json_get(doc, "size")))
			su.size = json_number(v);
		if ((v = json_get(doc, "signed_by")) && !json_is_null(v)) {
			const char *k = json_get(v, "key");
			const char *nm = json_get(v, "name");
			if (k)
				json_string(k, su.signer_key,
					    sizeof(su.signer_key));
			if (nm)
				json_string(nm, su.signer_name,
					    sizeof(su.signer_name));
			if ((v = json_get(doc, "verified")))
				su.verified = json_number(v);
		}
		if ((v = json_get(doc, "notes")) && !json_is_null(v)) {
			su.notes = kb_calloc(16385, 1);
			json_string(v, su.notes, 16385);
		}
		if ((v = json_get(doc, "packages"))) {
			int cap = 0;
			for (const char *e = json_first(v); e; e = json_next(e)) {
				char nm[128];
				const char *n = json_get(e, "name");
				if (!n || json_string(n, nm, sizeof(nm)) != 0 ||
				    !nm[0])
					continue;
				if (su.npkgs + 1 > cap) {
					cap = cap ? cap * 2 : 32;
					su.pkgs = kb_realloc(su.pkgs, (size_t)cap *
							     sizeof(*su.pkgs));
				}
				su.pkgs[su.npkgs++] = kb_strdup(nm);
			}
		}
	}
	free(doc);
	appstream_write();
	state_recompute();
	state_emit();
}

/* ── the boot state ────────────────────────────────────────────────────── */

static void slot_field(const char *doc, const char *key, char out[2])
{
	char v[8];
	doc_str(doc, key, v, sizeof(v));
	out[0] = (v[0] == 'a' || v[0] == 'b') && !v[1] ? v[0] : 0;
	out[1] = 0;
}

/*
 * `kdos-bootctl status --json`, the one reader of the ESP's boot state. Run
 * synchronously: it reads one small file and exits, and the answer has to be
 * current before any method that depends on it replies.
 */
void state_reload_boot(void)
{
	const char *argv[] = { "kdos-bootctl", "status", "--json", NULL };
	KbBuf out = {0};

	su.configured = 0;
	su.active[0] = su.running[0] = su.trying[0] = su.failed[0] = 0;
	su.attempts = 0;
	su.bootnext = 0;
	su.uuid[0][0] = su.uuid[1][0] = su.crypt[0][0] = su.crypt[1][0] = 0;
	su.other_version[0] = 0;
	free(su.slots_json);
	su.slots_json = NULL;
	run_sync(argv, &out, NULL);
	if (out.p && json_ws(out.p)[0] == '{') {
		const char *v = json_get(out.p, "configured");
		su.configured = v && json_true(v);
		slot_field(out.p, "active", su.active);
		slot_field(out.p, "running", su.running);
		slot_field(out.p, "trying", su.trying);
		slot_field(out.p, "failed", su.failed);
		if ((v = json_get(out.p, "attempts")))
			su.attempts = (unsigned)json_number(v);
		su.bootnext = (v = json_get(out.p, "bootnext")) && json_true(v);
		doc_str(out.p, "slot_a", su.uuid[0], sizeof(su.uuid[0]));
		doc_str(out.p, "slot_b", su.uuid[1], sizeof(su.uuid[1]));
		doc_str(out.p, "crypt_a", su.crypt[0], sizeof(su.crypt[0]));
		doc_str(out.p, "crypt_b", su.crypt[1], sizeof(su.crypt[1]));
		const char *slots = json_get(out.p, "slots");
		if (slots) {
			const char *end = json_skip(slots);
			if (end) {
				size_t n = (size_t)(end - slots);
				su.slots_json = kb_calloc(n + 1, 1);
				memcpy(su.slots_json, slots, n);
			}
		}
		if (su.slots_json && su.active[0]) {
			char other[2] = { su.active[0] == 'a' ? 'b' : 'a', 0 };
			const char *o = json_get(su.slots_json, other);
			const char *ov = o ? json_get(o, "version") : NULL;
			if (ov)
				json_string(ov, su.other_version,
					    sizeof(su.other_version));
		}
	}
	kb_buf_free(&out);
	state_recompute();
}

/* Both slots have a root: an update has somewhere to go that is not the
 * running system. A machine with one root updates with `kdos update apply`
 * at a shell, and Discover is offered nothing. */
int state_slots_ok(void)
{
	return su.configured && su.uuid[0][0] && su.uuid[1][0];
}

/*
 * THE DOCUMENT DESCRIBES THE SYSTEM THAT WROTE IT, and `installed` says which
 * version that was. /var/lib/kdos is each root's own, so the first boot of the
 * other slot, or the boot a rollback returns to, reads a document written
 * before this root was what it is now; its offer was measured against
 * packages this root does not have. Such a document, or none at all, is not
 * current: it offers nothing, and the daemon rewrites it with a check of this
 * root (bus_refresh). A root that names no version is compared as "".
 */
int state_doc_current(void)
{
	char v[64];
	if (!su.have_doc)
		return 0;
	state_version(v, sizeof(v));
	return !strcmp(su.installed, v);
}

/*
 * What CheckNew answers: the document's offer, unless the document is not
 * this root's, there is nowhere to put the update, the last fetch could not
 * reach the network (an offer the machine cannot download is not one), this
 * boot is a trial (the slot an update would go into is the confirmed way
 * back, and `kdos update apply` refuses it until the trial ends), or the
 * inactive slot is already armed with exactly that version — the update is
 * done and waits for the restart.
 */
int state_on_trial(void)
{
	return su.trying[0] && su.trying[0] == su.running[0];
}

const char *state_current_offer(void)
{
	if (!su.offer[0] || !state_doc_current() || !state_slots_ok() ||
	    fetch_offline() || state_on_trial())
		return "";
	if (su.trying[0] && su.trying[0] != su.running[0] &&
	    !strcmp(su.other_version, su.offer))
		return "";
	return su.offer;
}

/* io.kdos.Slots1.State and Version, from the job, the boot state and the
 * acquire record, in that order. */
void state_recompute(void)
{
	char aslot[4], aver[64];

	su.version[0] = 0;
	if (su.job) {
		kb_strlcpy(su.state, su.job->kind == JOB_ACQUIRE ? "acquiring"
								 : "installing",
			   sizeof(su.state));
		kb_strlcpy(su.version, su.offer, sizeof(su.version));
	} else if (su.trying[0] && su.trying[0] == su.running[0]) {
		kb_strlcpy(su.state, "trial", sizeof(su.state));
		state_version(su.version, sizeof(su.version));
	} else if (su.trying[0]) {
		kb_strlcpy(su.state, "armed", sizeof(su.state));
		kb_strlcpy(su.version, su.other_version, sizeof(su.version));
	} else if (state_acquired(aslot, sizeof(aslot), aver,
				  sizeof(aver)) == 0) {
		kb_strlcpy(su.state, "acquired", sizeof(su.state));
		kb_strlcpy(su.version, aver, sizeof(su.version));
	} else {
		kb_strlcpy(su.state, "idle", sizeof(su.state));
	}
}

/* ── the update channel ────────────────────────────────────────────────── */

/* The `tree =` of /etc/kdos/update.conf: where `kdos update` takes a newer
 * tree from. "" is the KDOS releases. */
void state_channel(char *out, size_t cap)
{
	char p[1024];
	char *data;

	out[0] = 0;
	su_path("/etc/kdos/update.conf", p, sizeof(p));
	if (!(data = kb_read_all(p, NULL)))
		return;
	for (char *l = data; l && *l;) {
		char *nl = strchr(l, '\n');
		if (nl)
			*nl = 0;
		while (*l == ' ' || *l == '\t')
			l++;
		if (!strncmp(l, "tree", 4) &&
		    (l[4] == ' ' || l[4] == '\t' || l[4] == '=')) {
			char *v = l + 4;
			while (*v == ' ' || *v == '\t' || *v == '=')
				v++;
			v[strcspn(v, " \t\r")] = 0;
			kb_strlcpy(out, v, cap);
		}
		l = nl ? nl + 1 : NULL;
	}
	free(data);
}

/*
 * A channel is "", an absolute directory, or an http, https or file URL, with
 * no blank in it and no character outside printable ASCII: `kdos update`
 * hands it to curl as one argument, and update.conf is read a line at a time.
 * What arrives from any of them is verified against /etc/kdos/keys/tree, so
 * the channel chooses a place and never a key.
 */
static int channel_ok(const char *c)
{
	if (!*c)
		return 1;
	if (strlen(c) > 512)
		return 0;
	for (const char *s = c; *s; s++)
		if ((unsigned char)*s <= 0x20 || (unsigned char)*s >= 0x7f ||
		    *s == '#' || *s == '"' || *s == '\'' || *s == '\\')
			return 0;
	if (c[0] == '/')
		return 1;
	static const char *const schemes[] = { "https://", "http://", "file:///",
					       NULL };
	for (int i = 0; schemes[i]; i++) {
		size_t n = strlen(schemes[i]);
		if (!strncmp(c, schemes[i], n) && c[n])
			return 1;
	}
	return 0;
}

/* update.conf with its `tree =` line replaced, or removed for "", every other
 * line kept as it was. 0, -1 for a channel that is not one, -2 when the file
 * could not be written. */
int state_set_channel(const char *channel)
{
	char p[1024], d[1024];
	KbBuf b = {0};

	if (!channel_ok(channel))
		return -1;
	su_path("/etc/kdos/update.conf", p, sizeof(p));
	char *data = kb_read_all(p, NULL);
	for (char *l = data; l && *l;) {
		char *nl = strchr(l, '\n');
		if (nl)
			*nl = 0;
		const char *k = l;
		while (*k == ' ' || *k == '\t')
			k++;
		int tree = !strncmp(k, "tree", 4) &&
			   (k[4] == ' ' || k[4] == '\t' || k[4] == '=');
		if (!tree)
			kb_buf_printf(&b, "%s\n", l);
		l = nl ? nl + 1 : NULL;
	}
	free(data);
	if (channel[0])
		kb_buf_printf(&b, "tree = %s\n", channel);
	kb_buf_str(&b, "");
	su_path("/etc/kdos", d, sizeof(d));
	kb_mkdir_p(d);
	int rc = kb_write_file_atomic(p, b.p) == 0 ? 0 : -2;
	kb_buf_free(&b);
	return rc;
}

/* ── telling the bus ───────────────────────────────────────────────────── */

/*
 * io.kdos.Slots1.State: the contract's keys (job, jobDone, jobTotal,
 * jobProgress, rebootRequired, onTrial, blessUserspace, blessGraphical,
 * rolledBack), then the boot state's own `trying`, `running` and `active`
 * and the snake-case job keys the Discover notifier reads, then what this
 * daemon adds — `phase`, `version`, `offer`, `error`, `blessed`, and the key
 * that signed the binhost index this machine last verified (`signer`,
 * `signerKey`, and `verified` in unix seconds, 0 for none), which About this
 * System shows. A client ignores a key it does not know, so every reader
 * finds its own.
 */
int state_append_state(sd_bus_message *m)
{
	const char *job = !su.job ? "" : su.job->kind == JOB_ACQUIRE ? "acquire"
								      : "install";
	unsigned done = su.job ? su.job->done : 0;
	unsigned total = su.job ? su.job->total : 0;
	unsigned prog = su.job ? su.job->progress : 0;
	int armed = su.trying[0] && su.trying[0] != su.running[0];
	int trial = su.trying[0] && su.trying[0] == su.running[0];
	int r;

	r = sd_bus_message_open_container(m, 'a', "{sv}");
#define KV(k, t, v)                                                         \
	do {                                                                \
		if (r >= 0)                                                 \
			r = sd_bus_message_append(m, "{sv}", k, t, v);      \
	} while (0)
	KV("job", "s", job);
	KV("jobDone", "u", done);
	KV("jobTotal", "u", total);
	KV("jobProgress", "u", prog);
	KV("rebootRequired", "b", armed);
	KV("onTrial", "b", trial);
	KV("blessUserspace", "b", bless_has("userspace"));
	KV("blessGraphical", "b", bless_has("graphical"));
	KV("rolledBack", "s", su.failed);
	KV("trying", "s", su.trying);
	KV("running", "s", su.running);
	KV("active", "s", su.active);
	KV("job_done", "u", done);
	KV("job_total", "u", total);
	KV("phase", "s", su.state);
	KV("version", "s", su.version);
	KV("offer", "s", state_current_offer());
	KV("error", "s", su.error);
	KV("blessed", "b", su.blessed);
	KV("signer", "s", su.signer_name);
	KV("signerKey", "s", su.signer_key);
	KV("verified", "x", (int64_t)su.verified);
#undef KV
	if (r >= 0)
		r = sd_bus_message_close_container(m);
	return r;
}

/* What a Slots1 reader last saw, so a change is announced once and an
 * unchanged reload announces nothing. */
static struct {
	int configured, bootnext;
	unsigned attempts;
	char running[2], active[2], trying[2];
	char channel[520];
	char state[2048];
	char *slots;
} seen = { .configured = -1 };

/* Every State value, as one string, for the comparison. */
static void state_print(char *out, size_t cap)
{
	snprintf(out, cap, "%s|%u|%u|%u|%d|%d|%d|%s|%s|%s|%s|%s|%s|%s|%d",
		 !su.job ? "" : su.job->kind == JOB_ACQUIRE ? "a" : "i",
		 su.job ? su.job->done : 0, su.job ? su.job->total : 0,
		 su.job ? su.job->progress : 0, bless_has("userspace"),
		 bless_has("graphical"), su.blessed, su.failed, su.trying,
		 su.running, su.active, su.state, su.version,
		 state_current_offer(), su.blessed);
	size_t n = strlen(out);
	snprintf(out + n, cap - n, "|%s|%s|%s|%lld", su.error, su.signer_name,
		 su.signer_key, su.verified);
}

void state_emit(void)
{
	const char *ch[10];
	int n = 0;
	char chan[520], st[2048];

	if (!su.bus)
		return;
	state_channel(chan, sizeof(chan));
	state_print(st, sizeof(st));
	if (seen.configured != su.configured) {
		seen.configured = su.configured;
		ch[n++] = "Configured";
	}
#define CHANGED_S(f, val, name)                                     \
	do {                                                        \
		if (strcmp(seen.f, val)) {                          \
			kb_strlcpy(seen.f, val, sizeof(seen.f));    \
			ch[n++] = name;                             \
		}                                                   \
	} while (0)
	CHANGED_S(running, su.running, "RunningSlot");
	CHANGED_S(active, su.active, "ActiveSlot");
	CHANGED_S(trying, su.trying, "TrySlot");
	CHANGED_S(channel, chan, "Channel");
	CHANGED_S(state, st, "State");
#undef CHANGED_S
	if (seen.attempts != su.attempts) {
		seen.attempts = su.attempts;
		ch[n++] = "TryAttempts";
	}
	if (seen.bootnext != su.bootnext) {
		seen.bootnext = su.bootnext;
		ch[n++] = "TryBootNext";
	}
	ch[n] = NULL;
	if (n)
		sd_bus_emit_properties_changed_strv(su.bus, SL_PATH, SL_IFACE,
						    (char **)ch);

	/* SlotsChanged: anything ListSlots returns. */
	const char *slots = su.slots_json ? su.slots_json : "";
	char extra[64];
	snprintf(extra, sizeof(extra), "|%s|%s|%s|%u", su.active, su.trying,
		 su.failed, su.attempts);
	size_t need = strlen(slots) + strlen(extra) + 1;
	char *now = kb_calloc(need, 1);
	snprintf(now, need, "%s%s", slots, extra);
	if (!seen.slots || strcmp(seen.slots, now)) {
		int first = !seen.slots;
		free(seen.slots);
		seen.slots = now;
		if (!first)
			sd_bus_emit_signal(su.bus, SL_PATH, SL_IFACE,
					   "SlotsChanged", "");
	} else {
		free(now);
	}
}
