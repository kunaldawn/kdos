/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   kdos-runner — KDOS in KRunner, over KRunner's D-Bus runner API
 *
 *     io.kdos.Runner  /runner   org.kde.krunner1   the command palette
 *     io.kdos.Runner  /library  org.kde.krunner1   the offline library
 *
 * Declared to KRunner by /usr/share/krunner/dbusplugins/kdos.desktop and
 * kdos-library.desktop (X-Plasma-API=DBus2), and started by the session
 * bus on the first Match through io.kdos.Runner.service. It exits after ten
 * minutes without a call; the next query starts it again.
 *
 * THE INTERFACE IS KRUNNER'S (krunner 6.31 src/data/org.kde.krunner1.xml,
 * dbusrunner.cpp):
 *
 *   Match(s query) -> a(sssida{sv})   id, text, icon, category relevance,
 *                                     relevance, {subtext, category, urls,
 *                                     actions}
 *   Actions() -> a(sss)               id, text, icon
 *   Run(s matchId, s actionId)
 *   Config() -> a{sv}                 MinLetterCount
 *   SetActivationToken(s)             just before Run
 *   Teardown()                        the query session ended
 *
 * WHY C ON THE BUS AND NOT A PLUGIN: a D-Bus runner links no Qt, so it is
 * not rebuilt for every Plasma minor, and a crash here costs KRunner one
 * runner's rows rather than its process.
 *
 * MATCH IS ANSWERED FROM MEMORY. The palette's sources are cached files
 * (sources.c) and answer in well under a millisecond. The library's two
 * servers are asked over loopback, and that Match is answered when both
 * have answered or KDOS_RUNNER_DEADLINE_MS (default 250) has passed: the
 * reply is deferred, never blocking, so a slow kiwix-serve delays the
 * library's rows alone and the palette keeps answering.
 *
 * AN ASK WAITS ON THE MODEL, NOT ON THE PALETTE. `ask <question>` on /runner
 * is answered when the local model has answered (ask.c), or with a "still
 * answering" row by KDOS_RUNNER_ASK_REPLY_MS (default 20000), inside
 * KRunner's 25-second reply timeout. A newer Match of another question
 * answers the older one at once with its palette rows alone.
 *
 * RUN EXECUTES ONLY WHAT MATCH BUILT. A match id names a row this process
 * returned, and the row's vector is executed as it was built (run.c); an id
 * it did not hand out does nothing. Teardown does not forget the session's
 * rows at once: on Wayland KRunner sends Run only once the compositor has
 * handed it an activation token, and Teardown, sent without waiting, can
 * arrive first. A torn-down row is kept ten seconds (KDOS_RUNNER_TORN_MS)
 * and then dropped.
 * ---------------------------------
 */

#include <errno.h>
#include <poll.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#if __has_include(<basu/sd-bus.h>)
#  include <basu/sd-bus.h>
#else
#  include <systemd/sd-bus.h>
#endif

#include "runner.h"

#define RN_BUS     "io.kdos.Runner"
#define RN_IFACE   "org.kde.krunner1"
#define RN_IDLE_MS (10 * 60 * 1000)
#define RN_STORE   1024

static sd_bus *g_bus;

static uint64_t now_ms(void)
{
	struct timespec ts;

	clock_gettime(CLOCK_MONOTONIC, &ts);
	return (uint64_t)ts.tv_sec * 1000 + (uint64_t)ts.tv_nsec / 1000000;
}

/* ── the rows of this session ──────────────────────────────────────────── */

typedef struct {
	char id[24];
	uint64_t torn;		/* when Teardown came; 0 while the session lasts */
	RnMatch m;
} Stored;

static Stored *g_store;
static int g_nstore;
static unsigned long g_seq;

/* Mark the session's rows torn down; they stay runnable a while longer. */
static void store_teardown(void)
{
	uint64_t now = now_ms();

	for (int i = 0; i < g_nstore; i++)
		if (!g_store[i].torn)
			g_store[i].torn = now;
}

static uint64_t torn_ms(void)
{
	const char *v = getenv("KDOS_RUNNER_TORN_MS");
	int t = v ? atoi(v) : 0;

	return t > 0 && t <= 600000 ? (uint64_t)t : 10000;
}

/* Drop the rows torn down longer ago than torn_ms(). */
static void store_expire(void)
{
	uint64_t now = now_ms(), keep = torn_ms();
	int o = 0;

	for (int i = 0; i < g_nstore; i++) {
		if (g_store[i].torn && now - g_store[i].torn >= keep) {
			rn_match_free(&g_store[i].m);
			continue;
		}
		if (o != i)
			g_store[o] = g_store[i];
		o++;
	}
	g_nstore = o;
}

/* Takes the row's vector; `m` is left without one. */
static const char *store_put(RnMatch *m)
{
	Stored *s;

	store_expire();
	/* A session that never tears down (a client that is not KRunner)
	 * must not grow without end: the oldest rows go first. */
	if (g_nstore >= RN_STORE) {
		for (int i = 0; i < RN_STORE / 2; i++)
			rn_match_free(&g_store[i].m);
		memmove(g_store, g_store + RN_STORE / 2,
			(size_t)(g_nstore - RN_STORE / 2) * sizeof(*g_store));
		g_nstore -= RN_STORE / 2;
	}
	if (!g_store)
		g_store = kb_calloc(RN_STORE, sizeof(*g_store));
	s = &g_store[g_nstore++];
	snprintf(s->id, sizeof(s->id), "k%lu", ++g_seq);
	s->torn = 0;
	s->m = *m;
	m->argc = 0;
	return s->id;
}

static const RnMatch *store_get(const char *id)
{
	for (int i = 0; i < g_nstore; i++)
		if (!strcmp(g_store[i].id, id))
			return &g_store[i].m;
	return NULL;
}

/* ── replies ───────────────────────────────────────────────────────────── */

static int append_rows(sd_bus_message *reply, RnList *l)
{
	int r = sd_bus_message_open_container(reply, 'a', "(sssida{sv})");

	for (int i = 0; r >= 0 && i < l->n; i++) {
		RnMatch *m = &l->v[i];
		const char *id = store_put(m);

		r = sd_bus_message_open_container(reply, 'r', "sssida{sv}");
		if (r >= 0)
			r = sd_bus_message_append(reply, "sssid", id, m->text,
						  m->icon, m->catrel, m->rel);
		if (r >= 0)
			r = sd_bus_message_open_container(reply, 'a', "{sv}");
		if (r >= 0 && m->subtext[0])
			r = sd_bus_message_append(reply, "{sv}", "subtext", "s",
						  m->subtext);
		if (r >= 0)
			r = sd_bus_message_append(reply, "{sv}", "category", "s",
						  m->category);
		if (r >= 0 && m->url[0])
			r = sd_bus_message_append(reply, "{sv}", "urls", "as", 1,
						  m->url);
		if (r >= 0 && m->multiline)
			r = sd_bus_message_append(reply, "{sv}", "multiline",
						  "b", 1);
		/* An empty list shows no action; leaving the key out would
		 * show every action the runner has. */
		if (r >= 0)
			r = m->term_action
				? sd_bus_message_append(reply, "{sv}", "actions",
							"as", 1, RN_ACT_TERM)
				: m->how == RN_RUN_COPY
				? sd_bus_message_append(reply, "{sv}", "actions",
							"as", 1, RN_ACT_COPY)
				: sd_bus_message_append(reply, "{sv}", "actions",
							"as", 0);
		if (r >= 0)
			r = sd_bus_message_close_container(reply);
		if (r >= 0)
			r = sd_bus_message_close_container(reply);
	}
	if (r >= 0)
		r = sd_bus_message_close_container(reply);
	return r;
}

static int reply_rows(sd_bus_message *m, RnList *l)
{
	sd_bus_message *reply = NULL;
	int r = sd_bus_message_new_method_return(m, &reply);

	if (r >= 0)
		r = append_rows(reply, l);
	if (r >= 0)
		r = sd_bus_send(NULL, reply, NULL);
	sd_bus_message_unref(reply);
	return r;
}

/* ── the library's deferred answers ────────────────────────────────────── */

typedef struct Pending {
	sd_bus_message *call;
	RnQuery q;
	RnList rows;
	RnAsk *asks;
	uint64_t deadline;
	struct Pending *next;
} Pending;

static Pending *g_pending;

static int deadline_ms(void)
{
	const char *v = getenv("KDOS_RUNNER_DEADLINE_MS");
	int d = v ? atoi(v) : 0;

	return d > 0 && d <= 5000 ? d : 250;
}

static int pending_done(const Pending *p)
{
	for (const RnAsk *a = p->asks; a; a = a->next)
		if (!a->done)
			return 0;
	return 1;
}

static void pending_finish(Pending *p)
{
	for (RnAsk *a = p->asks; a; a = a->next)
		if (a->done == 1)
			rn_library_answer(a, &p->q, &p->rows);
	rn_list_sort(&p->rows);
	reply_rows(p->call, &p->rows);
	sd_bus_message_unref(p->call);
	rn_list_free(&p->rows);
	rn_http_free(p->asks);
	free(p);
}

/* Answer every Match whose asks are all done or whose deadline passed. */
static void pending_sweep(void)
{
	uint64_t now = now_ms();

	for (Pending **pp = &g_pending; *pp;) {
		Pending *p = *pp;

		if (pending_done(p) || now >= p->deadline) {
			*pp = p->next;
			pending_finish(p);
			continue;
		}
		pp = &p->next;
	}
}

/* ── the asks' deferred answers ────────────────────────────────────────── */

typedef struct AskWait {
	sd_bus_message *call;
	RnList rows;		/* the palette's, sent with the ask's       */
	char q[512];
	uint64_t reply_by;
	struct AskWait *next;
} AskWait;

static AskWait *g_asks;

static void ask_finish(AskWait *w)
{
	rn_ask_rows(w->q, &w->rows);
	rn_list_sort(&w->rows);
	reply_rows(w->call, &w->rows);
	sd_bus_message_unref(w->call);
	rn_list_free(&w->rows);
	free(w);
}

/* Answer every Match whose question has its answer, whose time is up, or
 * which a newer question replaced. */
static void ask_sweep(void)
{
	uint64_t now = now_ms();

	rn_ask_tick();
	for (AskWait **pp = &g_asks; *pp;) {
		AskWait *w = *pp;

		if (rn_ask_final(w->q) || !rn_ask_current(w->q) ||
		    now >= w->reply_by) {
			*pp = w->next;
			ask_finish(w);
			continue;
		}
		pp = &w->next;
	}
}

/* ── org.kde.krunner1 ──────────────────────────────────────────────────── */

static int is_library(sd_bus_message *m)
{
	const char *path = sd_bus_message_get_path(m);

	return path && !strcmp(path, "/library");
}

static int m_match(sd_bus_message *m, void *ud, sd_bus_error *err)
{
	const char *query;
	RnList rows = { 0 };
	int r;

	(void)ud;
	(void)err;
	r = sd_bus_message_read(m, "s", &query);
	if (r < 0)
		return r;
	if (!is_library(m)) {
		const char *q = rn_ask_question(query);

		rn_palette_match(query, &rows);
		if (q) {
			rn_ask_want(q, rn_ask_quiet_ms());
			rn_ask_tick();
			if (!rn_ask_final(q)) {
				AskWait *w = kb_calloc(1, sizeof(*w));

				w->call = sd_bus_message_ref(m);
				w->rows = rows;
				kb_strlcpy(w->q, q, sizeof(w->q));
				w->reply_by = now_ms() +
					      (uint64_t)rn_ask_reply_ms();
				w->next = g_asks;
				g_asks = w;
				/* An older question's Match goes now. */
				ask_sweep();
				return 1;
			}
			rn_ask_rows(q, &rows);
		}
		r = reply_rows(m, &rows);
		rn_list_free(&rows);
		return r < 0 ? r : 1;
	}

	{
		const char *text = rn_library_query(query);
		Pending *p;

		if (!text)
			return reply_rows(m, &rows) < 0 ? -EIO : 1;
		p = kb_calloc(1, sizeof(*p));
		rn_query(&p->q, text);
		rn_library_match(&p->q, &p->rows);
		rn_library_asks(&p->q, &p->asks);
		for (RnAsk *a = p->asks; a; a = a->next)
			rn_http_start(a);
		p->call = sd_bus_message_ref(m);
		p->deadline = now_ms() + (uint64_t)deadline_ms();
		p->next = g_pending;
		g_pending = p;
		/* Answered from the loop: returning without a reply keeps
		 * the call open, and sd-bus sends none of its own. */
		return 1;
	}
}

static int m_actions(sd_bus_message *m, void *ud, sd_bus_error *err)
{
	(void)ud;
	(void)err;
	if (is_library(m))
		return sd_bus_reply_method_return(m, "a(sss)", 0);
	return sd_bus_reply_method_return(m, "a(sss)", 2, RN_ACT_TERM,
					  "Show the output in Konsole",
					  "utilities-terminal", RN_ACT_COPY,
					  "Copy the answer", "edit-copy");
}

static int m_config(sd_bus_message *m, void *ud, sd_bus_error *err)
{
	(void)ud;
	(void)err;
	/* Two letters is a favourite's code; the library wants three, since
	 * every docset and book holds thousands of two-letter prefixes. */
	return sd_bus_reply_method_return(m, "a{sv}", 1, "MinLetterCount", "i",
					  is_library(m) ? 3 : 2);
}

static int m_run(sd_bus_message *m, void *ud, sd_bus_error *err)
{
	const char *id, *action;
	const RnMatch *row;
	int r;

	(void)ud;
	(void)err;
	r = sd_bus_message_read(m, "ss", &id, &action);
	if (r < 0)
		return r;
	row = store_get(id);
	if (row)
		rn_run(row, !strcmp(action, RN_ACT_TERM) && row->term_action);
	return sd_bus_reply_method_return(m, "");
}

static int m_token(sd_bus_message *m, void *ud, sd_bus_error *err)
{
	const char *token;
	int r;

	(void)ud;
	(void)err;
	r = sd_bus_message_read(m, "s", &token);
	if (r < 0)
		return r;
	rn_run_token(token);
	return sd_bus_reply_method_return(m, "");
}

static int m_teardown(sd_bus_message *m, void *ud, sd_bus_error *err)
{
	(void)ud;
	(void)err;
	store_teardown();
	return sd_bus_reply_method_return(m, "");
}

static const sd_bus_vtable vtable[] = {
	SD_BUS_VTABLE_START(0),
	SD_BUS_METHOD("Match", "s", "a(sssida{sv})", m_match,
		      SD_BUS_VTABLE_UNPRIVILEGED),
	SD_BUS_METHOD("Actions", "", "a(sss)", m_actions,
		      SD_BUS_VTABLE_UNPRIVILEGED),
	SD_BUS_METHOD("Run", "ss", "", m_run, SD_BUS_VTABLE_UNPRIVILEGED),
	SD_BUS_METHOD("Config", "", "a{sv}", m_config,
		      SD_BUS_VTABLE_UNPRIVILEGED),
	SD_BUS_METHOD("SetActivationToken", "s", "", m_token,
		      SD_BUS_VTABLE_UNPRIVILEGED),
	SD_BUS_METHOD("Teardown", "", "", m_teardown,
		      SD_BUS_VTABLE_UNPRIVILEGED),
	SD_BUS_VTABLE_END
};

/* ── the loop ──────────────────────────────────────────────────────────── */

static int serve(void)
{
	uint64_t last = now_ms();
	int r;

	if ((r = sd_bus_open_user(&g_bus)) < 0) {
		fprintf(stderr, "kdos-runner: no session bus: %s\n", strerror(-r));
		return 1;
	}
	if (sd_bus_add_object_vtable(g_bus, NULL, "/runner", RN_IFACE, vtable,
				     NULL) < 0 ||
	    sd_bus_add_object_vtable(g_bus, NULL, "/library", RN_IFACE, vtable,
				     NULL) < 0) {
		fprintf(stderr, "kdos-runner: cannot publish the runner\n");
		return 1;
	}
	r = sd_bus_request_name(g_bus, RN_BUS, 0);
	if (r < 0) {
		/* Another instance holds the name: activation raced a start
		 * by hand. The one that holds it answers. */
		fprintf(stderr, "kdos-runner: %s is taken: %s\n", RN_BUS,
			strerror(-r));
		return r == -EEXIST ? 0 : 1;
	}

	rn_library_async(1);
	for (;;) {
		/* The bus, up to 64 library asks, and the local model's. */
		struct pollfd pfd[66];
		RnAsk *who[66];
		int n = 1, timeout;
		uint64_t usec, now, soonest;

		while ((r = sd_bus_process(g_bus, NULL)) > 0)
			last = now_ms();
		if (r < 0) {
			/* The session ending takes its bus with it, and
			 * that is the end of this program's job, not a
			 * failure. */
			if (r == -ECONNRESET || r == -ENOTCONN)
				break;
			fprintf(stderr, "kdos-runner: the bus failed: %s\n",
				strerror(-r));
			return 1;
		}
		pending_sweep();
		ask_sweep();

		now = now_ms();
		if (!g_pending && !g_asks && !rn_ask_busy() &&
		    now - last >= RN_IDLE_MS)
			break;
		soonest = g_pending || g_asks || rn_ask_busy()
			? UINT64_MAX : last + RN_IDLE_MS;
		for (AskWait *w = g_asks; w; w = w->next)
			if (w->reply_by < soonest)
				soonest = w->reply_by;
		if (rn_ask_due() >= 0 && now + (uint64_t)rn_ask_due() < soonest)
			soonest = now + (uint64_t)rn_ask_due();
		{
			RnAsk *h = rn_ask_http();
			short ev = h ? rn_http_events(h) : 0;

			if (ev) {
				pfd[n].fd = h->fd;
				pfd[n].events = ev;
				who[n] = h;
				n++;
			}
		}
		for (Pending *p = g_pending; p; p = p->next) {
			if (p->deadline < soonest)
				soonest = p->deadline;
			for (RnAsk *a = p->asks; a && n < 65; a = a->next) {
				short ev = rn_http_events(a);

				if (!ev)
					continue;
				pfd[n].fd = a->fd;
				pfd[n].events = ev;
				who[n] = a;
				n++;
			}
		}
		pfd[0].fd = sd_bus_get_fd(g_bus);
		pfd[0].events = (short)sd_bus_get_events(g_bus);
		if (sd_bus_get_timeout(g_bus, &usec) >= 0 && usec != UINT64_MAX) {
			struct timespec ts;
			uint64_t mono;

			clock_gettime(CLOCK_MONOTONIC, &ts);
			mono = (uint64_t)ts.tv_sec * 1000000 +
			       (uint64_t)ts.tv_nsec / 1000;
			usec = usec > mono ? (usec - mono + 999) / 1000 : 0;
			if (now + usec < soonest)
				soonest = now + usec;
		}
		timeout = soonest == UINT64_MAX ? -1
			: soonest <= now ? 0
			: soonest - now > INT32_MAX ? INT32_MAX
			: (int)(soonest - now);
		if (poll(pfd, (nfds_t)n, timeout) < 0 && errno != EINTR) {
			fprintf(stderr, "kdos-runner: poll: %s\n", strerror(errno));
			return 1;
		}
		for (int i = 1; i < n; i++)
			if (pfd[i].revents)
				rn_http_step(who[i], pfd[i].revents);
	}
	/* A Match sent to the name just before it was released is already on
	 * this connection: answer it, the library's with the rows it has, so
	 * that KRunner is not left waiting on a process that exited. */
	sd_bus_release_name(g_bus, RN_BUS);
	while (sd_bus_process(g_bus, NULL) > 0)
		;
	while (g_pending) {
		Pending *p = g_pending;

		g_pending = p->next;
		pending_finish(p);
	}
	while (g_asks) {
		AskWait *w = g_asks;

		g_asks = w->next;
		ask_finish(w);
	}
	sd_bus_flush_close_unref(g_bus);
	return 0;
}

/* ── by hand ───────────────────────────────────────────────────────────── */

/* `--query TEXT` and `--library TEXT` print what Match would return, the
 * library's servers asked synchronously under the same deadline. */
static int print_query(const char *text, int library)
{
	RnList rows = { 0 };
	KbBuf out = { 0 };

	if (!library) {
		rn_palette_match(text, &rows);
		rn_ask_collect(text, rn_ask_reply_ms(), &rows);
		rn_list_sort(&rows);
	} else
		rn_library_collect(text, deadline_ms(), &rows);
	rn_list_print(&rows, &out);
	if (out.p)
		fputs(out.p, stdout);
	kb_buf_free(&out);
	rn_list_free(&rows);
	return 0;
}

int main(int argc, char **argv)
{
	kb_set_progname("kdos-runner");
	signal(SIGPIPE, SIG_IGN);
	if (argc == 3 && !strcmp(argv[1], "--query"))
		return print_query(argv[2], 0);
	if (argc == 3 && !strcmp(argv[1], "--library"))
		return print_query(argv[2], 1);
	if (argc > 1) {
		fprintf(stderr, "usage: kdos-runner [--query TEXT | --library TEXT]\n"
				"  with no argument, serves io.kdos.Runner on the "
				"session bus\n");
		return 2;
	}
	return serve();
}
