/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   bus.c — org.freedesktop.sysupdate1 and io.kdos.Slots1
 *
 * THE CALL ORDER IS DISCOVER'S, and this file is shaped by it (Discover
 * 6.8.2, libdiscover/backends/SystemdSysupdateBackend):
 *
 *   Ping                       is the backend valid at all
 *   Manager.ListTargets        one target, host
 *   Target.GetAppStream        a URL; an empty list drops the target
 *   Target.GetVersion          empty drops it too
 *   Target.CheckNew            empty means nothing to offer
 *   Target.Acquire("", 0)      the reply's job path is then watched for
 *                              PropertiesChanged(Progress); Manager.JobRemoved
 *                              with status 0 ends it
 *   Target.Install("", 0)      the same, with no progress shown
 *
 * THE REPLY TO ACQUIRE WAITS FOR THE CHILD'S FIRST WORD. Discover shows a
 * failed call's message in a banner and a failed job as nothing but "failed",
 * so everything that can refuse an update — a release index, binhost index
 * or image manifest no trusted key signed, a file other than the one the
 * release index names, a package the binhost does not cover, a slot that
 * cannot be mounted — is
 * learned before the reply: `kdos update apply` writes `verified` or
 * `refused <reason>` first, and only `verified` creates the job.
 *
 * JOBREMOVED IS SENT ONLY FOR A JOB THIS DAEMON REPLIED WITH. Discover ends
 * whatever transaction it is waiting on at the first JobRemoved it sees,
 * whatever its id, so a stray one would end an update that is still running.
 * ---------------------------------
 */

#include <errno.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "sysupdated.h"

/* The child doing the job's work, from its spawn — before the job exists —
 * to its end. Non-NULL is "an update is running". */
static Child *job_child;

/* ── helpers ───────────────────────────────────────────────────────────── */

static int reply_error(sd_bus_message *m, const char *name, const char *fmt,
		       ...) __attribute__((format(printf, 3, 4)));
static int reply_error(sd_bus_message *m, const char *name, const char *fmt,
		       ...)
{
	char msg[512];
	va_list ap;
	va_start(ap, fmt);
	vsnprintf(msg, sizeof(msg), fmt, ap);
	va_end(ap);
	sd_bus_error e = SD_BUS_ERROR_MAKE_CONST(name, msg);
	return sd_bus_reply_method_error(m, &e);
}

static void auth_denied(sd_bus_message *m, const char *en, const char *em)
{
	sd_bus_error e = SD_BUS_ERROR_MAKE_CONST(
		en ? en : SD_BUS_ERROR_ACCESS_DENIED,
		em ? em : "Access denied");
	sd_bus_reply_method_error(m, &e);
}

/* The running system's version, with its slot: "0.2.13 (slot A)". The
 * version alone where there are no slots, and "unknown" where nothing says
 * — Discover skips a target whose version is empty. */
static void running_version(char *out, size_t cap)
{
	char v[64];
	state_version(v, sizeof(v));
	if (!v[0])
		kb_strlcpy(v, "unknown", sizeof(v));
	if (su.configured && su.running[0])
		snprintf(out, cap, "%s (slot %c)", v,
			 su.running[0] == 'b' ? 'B' : 'A');
	else
		snprintf(out, cap, "%s", v);
}

/* ── jobs ──────────────────────────────────────────────────────────────── */

static int job_get(sd_bus *bus, const char *path, const char *iface,
		   const char *prop, sd_bus_message *reply, void *ud,
		   sd_bus_error *err)
{
	Job *j = ud;
	(void)bus;
	(void)path;
	(void)iface;
	(void)err;
	if (!strcmp(prop, "Id"))
		return sd_bus_message_append(reply, "t", (uint64_t)j->id);
	if (!strcmp(prop, "Type"))
		return sd_bus_message_append(reply, "s", j->kind == JOB_ACQUIRE
							      ? "acquire"
							      : "install");
	if (!strcmp(prop, "Offline"))
		return sd_bus_message_append(reply, "b", 0);
	return sd_bus_message_append(reply, "u", (uint32_t)j->progress);
}

static void cont_cancel(sd_bus_message *m, void *ud, int allowed,
			const char *en, const char *em)
{
	(void)ud;
	if (!allowed) {
		auth_denied(m, en, em);
		return;
	}
	if (!su.job || !job_child || su.job->kind != JOB_ACQUIRE) {
		reply_error(m, ERR_FAILED, "there is no acquire to cancel");
		return;
	}
	su_log("cancelling job %llu", su.job->id);
	kill(job_child->pid, SIGTERM);
	sd_bus_reply_method_return(m, "");
}

/* The client that started a job may cancel it without asking polkit again;
 * anybody else needs the update permission. */
static int job_cancel(sd_bus_message *m, void *ud, sd_bus_error *err)
{
	Job *j = ud;
	const char *s = sd_bus_message_get_sender(m);
	(void)err;
	if (j != su.job)
		return reply_error(m, ERR_FAILED, "this job has finished");
	if (j->kind != JOB_ACQUIRE)
		return reply_error(m, ERR_FAILED, "an install is not "
				   "cancelled: it is the short arm of a slot "
				   "already filled");
	if (s && !strcmp(s, j->owner)) {
		cont_cancel(m, NULL, 1, NULL, NULL);
		return 1;
	}
	return auth_check(m, PK_UPDATE, 1, cont_cancel, NULL);
}

static const sd_bus_vtable job_vtable[] = {
	SD_BUS_VTABLE_START(0),
	SD_BUS_PROPERTY("Id", "t", job_get, 0, SD_BUS_VTABLE_PROPERTY_CONST),
	SD_BUS_PROPERTY("Type", "s", job_get, 0, SD_BUS_VTABLE_PROPERTY_CONST),
	SD_BUS_PROPERTY("Offline", "b", job_get, 0,
			SD_BUS_VTABLE_PROPERTY_CONST),
	SD_BUS_PROPERTY("Progress", "u", job_get, 0,
			SD_BUS_VTABLE_PROPERTY_EMITS_CHANGE),
	SD_BUS_METHOD("Cancel", "", "", job_cancel, SD_BUS_VTABLE_UNPRIVILEGED),
	SD_BUS_VTABLE_END
};

Job *bus_job_new(int kind, const char *owner)
{
	Job *j = kb_calloc(1, sizeof(*j));
	j->kind = kind;
	j->id = ++su.next_id;
	/* systemd's own shape: a path element may not start with a digit. */
	snprintf(j->path, sizeof(j->path), SU_JOB_PATH "/_%llu", j->id);
	kb_strlcpy(j->owner, owner ? owner : "", sizeof(j->owner));
	if (sd_bus_add_object_vtable(su.bus, &j->slot, j->path, SU_JOB,
				     job_vtable, j) < 0) {
		free(j);
		return NULL;
	}
	return j;
}

void bus_job_free(Job *j)
{
	if (!j)
		return;
	sd_bus_slot_unref(j->slot);
	free(j);
}

void bus_job_progress(void)
{
	if (su.job)
		sd_bus_emit_properties_changed(su.bus, su.job->path, SU_JOB,
					       "Progress", NULL);
}

void bus_job_removed(Job *j, int status)
{
	su_log("job %llu (%s) removed with status %d", j->id,
	       j->kind == JOB_ACQUIRE ? "acquire" : "install", status);
	sd_bus_emit_signal(su.bus, SU_PATH, SU_MANAGER, "JobRemoved", "toi",
			   (uint64_t)j->id, j->path, (int32_t)status);
}

/* ── the children's words ──────────────────────────────────────────────── */

void bus_child_line(Child *c, const char *line)
{
	if (c != job_child)
		return;
	if (!strncmp(line, "refused ", 8)) {
		kb_strlcpy(c->error, line + 8, sizeof(c->error));
		kb_strlcpy(su.error, c->error, sizeof(su.error));
		if (c->call && !c->replied) {
			reply_error(c->call, ERR_REFUSED, "%s", c->error);
			c->replied = 1;
		}
		state_emit();
	} else if (!strncmp(line, "verified ", 9)) {
		if (!c->call || c->replied)
			return;
		Job *j = bus_job_new(c->kind, sd_bus_message_get_sender(c->call));
		if (!j) {
			reply_error(c->call, ERR_FAILED, "the job could not be "
				    "put on the bus");
			c->replied = 1;
			kill(c->pid, SIGTERM);
			return;
		}
		su.job = j;
		su.error[0] = 0;
		sd_bus_reply_method_return(c->call, "sto", c->version,
					   (uint64_t)j->id, j->path);
		c->replied = 1;
		su_log("job %llu: %s %s", j->id, c->kind == JOB_ACQUIRE
						      ? "acquiring" : "installing",
		       c->version);
		state_recompute();
		state_emit();
	} else if (!strncmp(line, "item ", 5)) {
		unsigned d = 0, t = 0;
		if (!su.job || sscanf(line + 5, "%u %u", &d, &t) != 2 || !t ||
		    d > t)
			return;
		/* 100 only at the end: a job at 100 that is still running
		 * reads as one that hung. */
		unsigned p = (unsigned)((unsigned long long)d * 100 / t);
		if (p > 99)
			p = 99;
		su.job->done = d;
		su.job->total = t;
		if (p != su.job->progress) {
			su.job->progress = p;
			bus_job_progress();
		}
		state_emit();
	} else if (!strncmp(line, "error ", 6)) {
		kb_strlcpy(c->error, line + 6, sizeof(c->error));
		kb_strlcpy(su.error, c->error, sizeof(su.error));
		state_emit();
	}
}

static void reply_waiters(void)
{
	const char *offer = state_current_offer();
	for (int i = 0; i < su.nwaiters; i++) {
		sd_bus_reply_method_return(su.waiters[i], "s", offer);
		sd_bus_message_unref(su.waiters[i]);
	}
	free(su.waiters);
	su.waiters = NULL;
	su.nwaiters = 0;
}

/*
 * `kdos update check --out`, rewriting the update document; with `fetch`,
 * `--fetch` first, which downloads the signed tree and the release's signed
 * index, and through that index alone the binhost index and the slot image
 * manifest, and nothing else; a file the index does not name, or names with
 * another hash, is refused there and offers nothing. One check at a time: a
 * fetch asked for while a local check runs follows it, so the CheckNew that
 * asked is answered from the network as it asked. 0 when a check is running, -1 when none could be started.
 */
int bus_refresh(int fetch)
{
	char doc[1024];

	if (su.refresh) {
		if (fetch && !su.refresh_fetch)
			su.refetch = 1;
		return 0;
	}
	su_path("/var/lib/kdos/update.json", doc, sizeof(doc));
	const char *argv_local[] = { "kdos", "update", "check", "--out", doc,
				     NULL };
	const char *argv_fetch[] = { "kdos", "update", "check", "--fetch",
				     "--out", doc, NULL };
	su.refresh = child_spawn(fetch ? argv_fetch : argv_local, 0, CH_CHECK);
	su.refresh_fetch = fetch && su.refresh;
	return su.refresh ? 0 : -1;
}

void bus_child_done(Child *c)
{
	if (c == su.refresh) {
		su.refresh = NULL;
		if (c->status != 0 && c->status != 1)
			su_log("the update check exited with status %d",
			       c->status);
		state_reload_doc();
		if (su.refetch) {
			su.refetch = 0;
			if (!job_child && bus_refresh(1) == 0)
				return;
		}
		reply_waiters();
		return;
	}
	if (c != job_child)
		return;
	job_child = NULL;
	if (c->call && !c->replied) {
		if (c->error[0])
			reply_error(c->call, ERR_FAILED, "%s", c->error);
		else
			reply_error(c->call, ERR_FAILED, "kdos update exited "
				    "with status %d before it began", c->status);
		c->replied = 1;
	}
	if (su.job) {
		int status = c->status == 0 ? 0
			   : c->status == 4 ? -ECANCELED
			   : c->status;
		if (status == 0) {
			su.job->progress = 100;
			su.job->done = su.job->total;
			bus_job_progress();
			su.error[0] = 0;
		} else if (!su.error[0]) {
			snprintf(su.error, sizeof(su.error), "kdos update exited "
				 "with status %d", c->status);
		}
		Job *j = su.job;
		su.job = NULL;
		bus_job_removed(j, status);
		bus_job_free(j);
	}
	state_reload_boot();
	state_recompute();
	state_emit();
}

/* ── Manager ───────────────────────────────────────────────────────────── */

static int m_list_targets(sd_bus_message *m, void *ud, sd_bus_error *err)
{
	(void)ud;
	(void)err;
	return sd_bus_reply_method_return(m, "a(sso)", 1, "host", "kdos",
					  SU_HOST_PATH);
}

static int m_list_jobs(sd_bus_message *m, void *ud, sd_bus_error *err)
{
	(void)ud;
	(void)err;
	if (!su.job)
		return sd_bus_reply_method_return(m, "a(tsuo)", 0);
	return sd_bus_reply_method_return(m, "a(tsuo)", 1, (uint64_t)su.job->id,
					  su.job->kind == JOB_ACQUIRE ? "acquire"
								      : "install",
					  (uint32_t)su.job->progress,
					  su.job->path);
}

static int m_list_appstream(sd_bus_message *m, void *ud, sd_bus_error *err)
{
	char url[1200];
	(void)ud;
	(void)err;
	appstream_url(url, sizeof(url));
	return sd_bus_reply_method_return(m, "as", 1, url);
}

static const sd_bus_vtable manager_vtable[] = {
	SD_BUS_VTABLE_START(0),
	SD_BUS_METHOD("ListTargets", "", "a(sso)", m_list_targets,
		      SD_BUS_VTABLE_UNPRIVILEGED),
	SD_BUS_METHOD("ListJobs", "", "a(tsuo)", m_list_jobs,
		      SD_BUS_VTABLE_UNPRIVILEGED),
	SD_BUS_METHOD("ListAppStream", "", "as", m_list_appstream,
		      SD_BUS_VTABLE_UNPRIVILEGED),
	SD_BUS_SIGNAL("JobRemoved", "toi", 0),
	SD_BUS_VTABLE_END
};

/* ── Target ────────────────────────────────────────────────────────────── */

static int t_get(sd_bus *bus, const char *path, const char *iface,
		 const char *prop, sd_bus_message *reply, void *ud,
		 sd_bus_error *err)
{
	(void)bus;
	(void)path;
	(void)iface;
	(void)ud;
	(void)err;
	if (!strcmp(prop, "Class"))
		return sd_bus_message_append(reply, "s", "host");
	if (!strcmp(prop, "Name"))
		return sd_bus_message_append(reply, "s", "kdos");
	return sd_bus_message_append(reply, "s", "/");
}

static int t_get_version(sd_bus_message *m, void *ud, sd_bus_error *err)
{
	char v[128];
	(void)ud;
	(void)err;
	running_version(v, sizeof(v));
	return sd_bus_reply_method_return(m, "s", v);
}

static int t_get_appstream(sd_bus_message *m, void *ud, sd_bus_error *err)
{
	return m_list_appstream(m, ud, err);
}

static void cont_checknew(sd_bus_message *m, void *ud, int allowed,
			  const char *en, const char *em)
{
	(void)ud;
	if (!allowed) {
		auth_denied(m, en, em);
		return;
	}
	state_reload_boot();
	if (!state_slots_ok()) {
		sd_bus_reply_method_return(m, "s", "");
		return;
	}
	/*
	 * A FETCH AT MOST EVERY --refresh-after SECONDS (0: never), and only of
	 * the signed tree and the indexes: `kdos update check --fetch` downloads
	 * no package and no source, and what CheckNew then offers is what the
	 * release index named and the host keyring verified. Discover asks on every start and on every
	 * "Check for updates"; the first after an hour asks the network, and
	 * the others are answered from the document. Never while an update is
	 * running, which holds the lock the fetch would take. A check already
	 * running — the one that makes the document this root's — is waited
	 * for, so the answer is never the document it is replacing.
	 */
	long long last = state_fetch_time();
	int fetch = su.refresh_after > 0 && !job_child &&
		    (long long)time(NULL) - last >= su.refresh_after;
	if ((fetch || su.refresh) && bus_refresh(fetch) == 0) {
		su.waiters = kb_realloc(su.waiters, (size_t)(su.nwaiters + 1) *
						    sizeof(*su.waiters));
		su.waiters[su.nwaiters++] = sd_bus_message_ref(m);
		return;
	}
	sd_bus_reply_method_return(m, "s", state_current_offer());
}

static int t_check_new(sd_bus_message *m, void *ud, sd_bus_error *err)
{
	(void)ud;
	(void)err;
	return auth_check(m, PK_CHECK, 1, cont_checknew, NULL);
}

static int t_list(sd_bus_message *m, void *ud, sd_bus_error *err)
{
	char v[64];
	int offline = 0;
	const char *offer = state_current_offer();
	(void)ud;
	(void)err;
	if (sd_bus_message_read(m, "b", &offline) < 0)
		return reply_error(m, SD_BUS_ERROR_INVALID_ARGS, "expected b");
	state_version(v, sizeof(v));
	if (offer[0] && strcmp(offer, v))
		return sd_bus_reply_method_return(m, "as", 2, offer,
						  v[0] ? v : "unknown");
	return sd_bus_reply_method_return(m, "as", 1, v[0] ? v : "unknown");
}

/* The shape systemd-sysupdate's Describe prints, for the two versions this
 * machine knows: the one it runs, and the one it is offered. */
static int t_describe(sd_bus_message *m, void *ud, sd_bus_error *err)
{
	const char *ver;
	int offline = 0;
	char v[64];
	const char *offer = state_current_offer();
	(void)ud;
	(void)err;
	if (sd_bus_message_read(m, "sb", &ver, &offline) < 0)
		return reply_error(m, SD_BUS_ERROR_INVALID_ARGS, "expected sb");
	state_version(v, sizeof(v));
	int is_inst = v[0] && !strcmp(ver, v);
	int is_offer = offer[0] && !strcmp(ver, offer);
	if (!is_inst && !is_offer)
		return reply_error(m, ERR_NO_VERSION, "this machine knows no "
				   "version %s", ver);
	KbBuf b = {0};
	kb_buf_str(&b, "{\"version\":");
	kb_json_str(&b, ver);
	kb_buf_printf(&b, ",\"newest\":%s,\"available\":%s,\"installed\":%s,"
		      "\"obsolete\":false,\"protected\":%s,\"incomplete\":false,"
		      "\"changelogUrls\":[],\"contents\":[]}",
		      is_offer || !offer[0] ? "true" : "false",
		      is_offer ? "true" : "false", is_inst ? "true" : "false",
		      is_inst ? "true" : "false");
	int r = sd_bus_reply_method_return(m, "s", b.p);
	kb_buf_free(&b);
	return r;
}

/* Nothing is vacuumed: the only other instance is the inactive slot, which
 * is the way back; the binhost cache is pruned against each new index by
 * `kdos update fetch`, and an image's bundles are streamed, never kept. */
static int t_vacuum(sd_bus_message *m, void *ud, sd_bus_error *err)
{
	(void)ud;
	(void)err;
	return sd_bus_reply_method_return(m, "u", 0u);
}

typedef struct {
	int kind;
	char version[64];
} StartReq;

static void cont_start(sd_bus_message *m, void *ud, int allowed,
		       const char *en, const char *em)
{
	StartReq *rq = ud;
	if (!allowed) {
		auth_denied(m, en, em);
		free(rq);
		return;
	}
	if (job_child) {
		reply_error(m, ERR_BUSY, "an update is already running");
		free(rq);
		return;
	}
	/*
	 * EVERY CHECK t_start MADE IS MADE AGAIN: the polkit wait can last
	 * minutes, and in it a CheckNew can replace the offer or start a
	 * fetch. `kdos update apply` takes whatever is offered or acquired
	 * when it runs, so a version other than the one authorised is refused
	 * here rather than installed.
	 */
	if (su.refresh && su.refresh_fetch) {
		reply_error(m, ERR_BUSY, "the update check is still running; "
			    "try again when it has finished");
		free(rq);
		return;
	}
	state_reload_boot();
	if (!state_slots_ok() || state_on_trial()) {
		reply_error(m, ERR_REFUSED, "this machine can no longer take "
			    "an update into its other slot");
		free(rq);
		return;
	}
	char now[64] = "", aslot[4];
	if (rq->kind == JOB_ACQUIRE)
		kb_strlcpy(now, state_current_offer(), sizeof(now));
	else if (state_acquired(aslot, sizeof(aslot), now, sizeof(now)) != 0)
		now[0] = 0;
	if (strcmp(now, rq->version)) {
		reply_error(m, ERR_NO_VERSION, "%s is not %s version now",
			    rq->version, rq->kind == JOB_ACQUIRE ?
			    "the offered" : "the acquired");
		free(rq);
		return;
	}
	const char *argv[] = { "kdos", "update", "apply",
			       rq->kind == JOB_ACQUIRE ? "--acquire" : "--arm",
			       "--mount", "--progress", "3", NULL };
	Child *c = child_spawn(argv, 1, rq->kind);
	if (!c) {
		reply_error(m, ERR_FAILED, "kdos update could not be started");
		free(rq);
		return;
	}
	c->call = sd_bus_message_ref(m);
	kb_strlcpy(c->version, rq->version, sizeof(c->version));
	job_child = c;
	free(rq);
}

/*
 * Acquire and Install take "" (the offered version, which is what Discover
 * sends) or that version by name; any other version is one this machine
 * cannot install. A named version asks polkit for update-to-version, as
 * systemd-sysupdated does.
 */
static int t_start(sd_bus_message *m, int kind)
{
	const char *ver;
	uint64_t flags = 0;
	char aslot[4], aver[64];

	if (sd_bus_message_read(m, "st", &ver, &flags) < 0)
		return reply_error(m, SD_BUS_ERROR_INVALID_ARGS, "expected st");
	if (job_child)
		return reply_error(m, ERR_BUSY, "an update is already running");
	/* A fetch holds the update lock the apply would take. */
	if (su.refresh && su.refresh_fetch)
		return reply_error(m, ERR_BUSY, "the update check is still "
				   "running; try again when it has finished");
	state_reload_boot();
	if (!state_slots_ok())
		return reply_error(m, ERR_NO_SLOTS, "this machine has no second "
				   "root slot to install an update into; "
				   "`kdos update apply` updates it in place");
	if (state_on_trial())
		return reply_error(m, ERR_REFUSED, "this boot is the trial of "
				   "slot %c; an update is installed once it is "
				   "confirmed or rolled back",
				   su.running[0] == 'b' ? 'B' : 'A');
	StartReq *rq = kb_calloc(1, sizeof(*rq));
	rq->kind = kind;
	if (kind == JOB_ACQUIRE) {
		const char *offer = state_current_offer();
		if (!offer[0]) {
			free(rq);
			return reply_error(m, ERR_NO_UPDATE, "no update is "
					   "available");
		}
		kb_strlcpy(rq->version, offer, sizeof(rq->version));
	} else {
		if (state_acquired(aslot, sizeof(aslot), aver,
				   sizeof(aver)) != 0) {
			free(rq);
			return reply_error(m, ERR_NOT_ACQ, "nothing has been "
					   "acquired to install");
		}
		kb_strlcpy(rq->version, aver, sizeof(rq->version));
	}
	if (ver[0] && strcmp(ver, rq->version)) {
		char have[64];
		kb_strlcpy(have, rq->version, sizeof(have));
		free(rq);
		return reply_error(m, ERR_NO_VERSION, "%s is not available; "
				   "%s is", ver, have);
	}
	return auth_check(m, ver[0] ? PK_UPDATE_VER : PK_UPDATE, 1, cont_start,
			  rq);
}

static int t_acquire(sd_bus_message *m, void *ud, sd_bus_error *err)
{
	(void)ud;
	(void)err;
	return t_start(m, JOB_ACQUIRE);
}

static int t_install(sd_bus_message *m, void *ud, sd_bus_error *err)
{
	(void)ud;
	(void)err;
	return t_start(m, JOB_INSTALL);
}

static const sd_bus_vtable target_vtable[] = {
	SD_BUS_VTABLE_START(0),
	SD_BUS_PROPERTY("Class", "s", t_get, 0, SD_BUS_VTABLE_PROPERTY_CONST),
	SD_BUS_PROPERTY("Name", "s", t_get, 0, SD_BUS_VTABLE_PROPERTY_CONST),
	SD_BUS_PROPERTY("Path", "s", t_get, 0, SD_BUS_VTABLE_PROPERTY_CONST),
	SD_BUS_METHOD("List", "b", "as", t_list, SD_BUS_VTABLE_UNPRIVILEGED),
	SD_BUS_METHOD("Describe", "sb", "s", t_describe,
		      SD_BUS_VTABLE_UNPRIVILEGED),
	SD_BUS_METHOD("CheckNew", "", "s", t_check_new,
		      SD_BUS_VTABLE_UNPRIVILEGED),
	SD_BUS_METHOD("Acquire", "st", "sto", t_acquire,
		      SD_BUS_VTABLE_UNPRIVILEGED),
	SD_BUS_METHOD("Install", "st", "sto", t_install,
		      SD_BUS_VTABLE_UNPRIVILEGED),
	SD_BUS_METHOD("Vacuum", "", "u", t_vacuum, SD_BUS_VTABLE_UNPRIVILEGED),
	SD_BUS_METHOD("GetAppStream", "", "as", t_get_appstream,
		      SD_BUS_VTABLE_UNPRIVILEGED),
	SD_BUS_METHOD("GetVersion", "", "s", t_get_version,
		      SD_BUS_VTABLE_UNPRIVILEGED),
	SD_BUS_VTABLE_END
};

/* ── io.kdos.Slots1 ────────────────────────────────────────────────────── */

static int sl_get(sd_bus *bus, const char *path, const char *iface,
		  const char *prop, sd_bus_message *reply, void *ud,
		  sd_bus_error *err)
{
	char chan[520];
	int armed = su.configured && su.trying[0];
	(void)bus;
	(void)path;
	(void)iface;
	(void)ud;
	(void)err;
	if (!strcmp(prop, "Configured"))
		return sd_bus_message_append(reply, "b", su.configured);
	if (!strcmp(prop, "RunningSlot"))
		return sd_bus_message_append(reply, "s", su.running);
	if (!strcmp(prop, "ActiveSlot"))
		return sd_bus_message_append(reply, "s", su.active);
	if (!strcmp(prop, "TrySlot"))
		return sd_bus_message_append(reply, "s", su.trying);
	if (!strcmp(prop, "TryAttempts"))
		return sd_bus_message_append(reply, "u",
					     armed ? su.attempts : 0u);
	if (!strcmp(prop, "TryBootNext"))
		return sd_bus_message_append(reply, "b", armed && su.bootnext);
	if (!strcmp(prop, "Channel")) {
		state_channel(chan, sizeof(chan));
		return sd_bus_message_append(reply, "s", chan);
	}
	return state_append_state(reply);
}

/*
 * One slot, in the contract's keys. What it holds comes from `kdos-bootctl
 * status --json`'s `slots`; whether it may be tried is decided here by the
 * rule kdos-bootctl's `try` applies, so the page greys out exactly what the
 * call would refuse.
 */
static int append_slot(sd_bus_message *r, int i)
{
	const char *id = i ? "b" : "a";
	const char *o = su.slots_json ? json_get(su.slots_json, id) : NULL;
	char title[200] = "", version[64] = "", tree[64] = "", kernel[128] = "";
	const char *v;
	long long when = 0;

	if (o && (v = json_get(o, "title")))
		json_string(v, title, sizeof(title));
	if (o && (v = json_get(o, "version")))
		json_string(v, version, sizeof(version));
	if (o && (v = json_get(o, "tree")))
		json_string(v, tree, sizeof(tree));
	if (o && (v = json_get(o, "kernel")))
		json_string(v, kernel, sizeof(kernel));
	if (o && (v = json_get(o, "tree_time")))
		when = json_number(v);

	int present = su.uuid[i][0] != 0;
	int confirmed = su.active[0] == id[0];
	int trial = su.trying[0] == id[0];
	int failed = su.failed[0] == id[0];
	const char *reason = !present ? "This slot has no system in it."
			   : confirmed ? "This is already the version the "
					 "computer starts."
			   : !kernel[0] ? "Its kernel is not on the boot "
					  "partition."
			   : "";
	const char *state = trial ? "trial" : failed ? "failed"
			  : confirmed ? "confirmed" : present ? "installed"
			  : "empty";
	if (!present) {
		version[0] = tree[0] = kernel[0] = 0;
		when = 0;
	}

	int rc = sd_bus_message_open_container(r, 'a', "{sv}");
#define KV(k, t, val)                                                       \
	do {                                                                \
		if (rc >= 0)                                                \
			rc = sd_bus_message_append(r, "{sv}", k, t, val);   \
	} while (0)
	KV("id", "s", id);
	KV("title", "s", title);
	KV("present", "b", present);
	KV("version", "s", version);
	KV("buildTime", "x", (int64_t)when);
	KV("treeCommit", "s", tree);
	KV("kernel", "s", kernel);
	KV("uuid", "s", su.uuid[i]);
	KV("encrypted", "b", su.crypt[i][0] != 0);
	KV("running", "b", su.running[0] == id[0]);
	KV("confirmed", "b", confirmed);
	KV("trial", "b", trial);
	KV("failed", "b", failed);
	KV("bootable", "b", !reason[0] && !trial);
	KV("reason", "s", reason);
	KV("state", "s", state);
#undef KV
	if (rc >= 0)
		rc = sd_bus_message_close_container(r);
	return rc;
}

/* Exactly two dictionaries, slot a first; none on a machine with no boot
 * state. */
static int sl_list_slots(sd_bus_message *m, void *ud, sd_bus_error *err)
{
	sd_bus_message *r = NULL;
	(void)ud;
	(void)err;
	state_reload_boot();
	int rc = sd_bus_message_new_method_return(m, &r);
	if (rc >= 0)
		rc = sd_bus_message_open_container(r, 'a', "a{sv}");
	if (rc >= 0 && su.configured) {
		rc = append_slot(r, 0);
		if (rc >= 0)
			rc = append_slot(r, 1);
	}
	if (rc >= 0)
		rc = sd_bus_message_close_container(r);
	if (rc >= 0)
		rc = sd_bus_send(NULL, r, NULL);
	sd_bus_message_unref(r);
	return rc;
}

/* kdos-bootctl, run for a Slots1 method: its refusal goes back in its own
 * words, the first line of what it wrote to stderr. */
static void run_bootctl(sd_bus_message *m, const char *const *argv)
{
	KbBuf out = {0}, errb = {0};
	int rc = run_sync(argv, &out, &errb);
	if (rc != 0) {
		const char *e = errb.p && errb.p[0] ? errb.p
						    : "kdos-bootctl refused";
		reply_error(m, SL_REFUSED, "%.*s", (int)strcspn(e, "\n"), e);
	} else {
		sd_bus_reply_method_return(m, "");
	}
	if (out.p && out.p[0])
		su_log("%s: %.*s", argv[1], (int)strcspn(out.p, "\n"), out.p);
	kb_buf_free(&out);
	kb_buf_free(&errb);
	state_reload_boot();
	state_recompute();
	state_emit();
}

static void cont_try(sd_bus_message *m, void *ud, int allowed,
		     const char *en, const char *em)
{
	char *slot = ud;
	if (!allowed) {
		auth_denied(m, en, em);
	} else if (job_child) {
		reply_error(m, SL_REFUSED, "an update is running");
	} else {
		/* One boot of it: "Start this version next time" is a single
		 * attempt, and a version that fails it is simply not started
		 * again. */
		const char *argv[] = { "kdos-bootctl", "try", slot, "1", NULL };
		run_bootctl(m, argv);
	}
	free(slot);
}

static int sl_try(sd_bus_message *m, void *ud, sd_bus_error *err)
{
	const char *slot;
	(void)ud;
	(void)err;
	if (sd_bus_message_read(m, "s", &slot) < 0)
		return reply_error(m, SD_BUS_ERROR_INVALID_ARGS, "expected s");
	if (strcmp(slot, "a") && strcmp(slot, "b"))
		return reply_error(m, SD_BUS_ERROR_INVALID_ARGS,
				   "no slot named '%s'", slot);
	return auth_check(m, PK_TRY, 1, cont_try, kb_strdup(slot));
}

static void cont_cancel_try(sd_bus_message *m, void *ud, int allowed,
			    const char *en, const char *em)
{
	(void)ud;
	if (!allowed) {
		auth_denied(m, en, em);
		return;
	}
	if (job_child) {
		reply_error(m, SL_REFUSED, "an update is running");
		return;
	}
	const char *argv[] = { "kdos-bootctl", "cancel", NULL };
	run_bootctl(m, argv);
}

static int sl_cancel_try(sd_bus_message *m, void *ud, sd_bus_error *err)
{
	(void)ud;
	(void)err;
	return auth_check(m, PK_TRY, 1, cont_cancel_try, NULL);
}

static void cont_channel(sd_bus_message *m, void *ud, int allowed,
			 const char *en, const char *em)
{
	char *channel = ud;
	if (!allowed) {
		auth_denied(m, en, em);
	} else {
		int rc = state_set_channel(channel);
		if (rc == -1)
			reply_error(m, SD_BUS_ERROR_INVALID_ARGS,
				    "not an update channel: %s", channel);
		else if (rc != 0)
			reply_error(m, SL_REFUSED, "cannot write "
				    "/etc/kdos/update.conf");
		else {
			su_log("update channel: %s",
			       channel[0] ? channel : "the KDOS releases");
			sd_bus_reply_method_return(m, "");
			state_emit();
		}
	}
	free(channel);
}

static int sl_set_channel(sd_bus_message *m, void *ud, sd_bus_error *err)
{
	const char *channel;
	(void)ud;
	(void)err;
	if (sd_bus_message_read(m, "s", &channel) < 0)
		return reply_error(m, SD_BUS_ERROR_INVALID_ARGS, "expected s");
	return auth_check(m, PK_CHANNEL, 1, cont_channel, kb_strdup(channel));
}

static void cont_bless(sd_bus_message *m, void *ud, int allowed,
		       const char *en, const char *em)
{
	(void)ud;
	if (!allowed) {
		auth_denied(m, en, em);
		return;
	}
	if (bless_graphical() != 0) {
		reply_error(m, SL_REFUSED, "cannot write the graphical marker");
		return;
	}
	sd_bus_reply_method_return(m, "");
	state_emit();
}

static int sl_bless(sd_bus_message *m, void *ud, sd_bus_error *err)
{
	(void)ud;
	(void)err;
	return auth_check(m, PK_BLESS, 0, cont_bless, NULL);
}

#define SL_PROP(name, sig)                                                  \
	SD_BUS_PROPERTY(name, sig, sl_get, 0,                               \
			SD_BUS_VTABLE_PROPERTY_EMITS_CHANGE)

static const sd_bus_vtable slots_vtable[] = {
	SD_BUS_VTABLE_START(0),
	SL_PROP("Configured", "b"),
	SL_PROP("RunningSlot", "s"),
	SL_PROP("ActiveSlot", "s"),
	SL_PROP("TrySlot", "s"),
	SL_PROP("TryAttempts", "u"),
	SL_PROP("TryBootNext", "b"),
	SL_PROP("Channel", "s"),
	SL_PROP("State", "a{sv}"),
	SD_BUS_METHOD("ListSlots", "", "aa{sv}", sl_list_slots,
		      SD_BUS_VTABLE_UNPRIVILEGED),
	SD_BUS_METHOD("Try", "s", "", sl_try, SD_BUS_VTABLE_UNPRIVILEGED),
	SD_BUS_METHOD("CancelTry", "", "", sl_cancel_try,
		      SD_BUS_VTABLE_UNPRIVILEGED),
	SD_BUS_METHOD("SetChannel", "s", "", sl_set_channel,
		      SD_BUS_VTABLE_UNPRIVILEGED),
	SD_BUS_METHOD("Bless", "", "", sl_bless, SD_BUS_VTABLE_UNPRIVILEGED),
	SD_BUS_SIGNAL("SlotsChanged", "", 0),
	SD_BUS_VTABLE_END
};

/* ── setup ─────────────────────────────────────────────────────────────── */

int bus_setup(void)
{
	int r = su.session ? sd_bus_open_user(&su.bus)
			   : sd_bus_open_system(&su.bus);
	if (r < 0) {
		su_log("cannot connect to the %s bus: %s",
		       su.session ? "session" : "system", strerror(-r));
		return -1;
	}
	if ((r = sd_bus_add_object_vtable(su.bus, NULL, SU_PATH, SU_MANAGER,
					  manager_vtable, NULL)) < 0 ||
	    (r = sd_bus_add_object_vtable(su.bus, NULL, SU_HOST_PATH, SU_TARGET,
					  target_vtable, NULL)) < 0 ||
	    (r = sd_bus_add_object_vtable(su.bus, NULL, SL_PATH, SL_IFACE,
					  slots_vtable, NULL)) < 0) {
		su_log("cannot register the objects: %s", strerror(-r));
		return -1;
	}
	/* -2 when somebody else owns either name: a restart cannot change
	 * that, and the supervisor is told so by the exit status. */
	if ((r = sd_bus_request_name(su.bus, SU_BUS, 0)) < 0 ||
	    (r = sd_bus_request_name(su.bus, SL_BUS, 0)) < 0) {
		su_log("cannot own %s and %s: %s", SU_BUS, SL_BUS,
		       strerror(-r));
		return r == -EEXIST ? -2 : -1;
	}
	return 0;
}
