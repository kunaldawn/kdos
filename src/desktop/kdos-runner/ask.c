/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   ask.c — `ask <question>`, answered by the language model on this machine
 *
 * llama-server serves an OpenAI-compatible API on 127.0.0.1:8082
 * (fs/etc/init.d/88_llama-server.sh) in router mode: it offers every model
 * under /usr/share/llama.cpp/models and refuses a request that names none.
 * So an ask is two exchanges over http.c's loopback client:
 *
 *   GET  /v1/models             the model to name: one already loaded, or
 *                               the first offered
 *   POST /v1/chat/completions   the question, with a short system prompt
 *                               and a token cap, not streamed
 *
 * LOOPBACK ONLY. http.c connects to 127.0.0.1 and nothing else; the port is
 * the one thing that changes (KDOS_RUNNER_LLAMA_PORT, for the tests). A
 * server that is not listening is said to be absent, and the question goes
 * nowhere.
 *
 * ONE QUESTION AT A TIME, ASKED ONCE THE TYPING STOPS. KRunner sends Match on
 * every keystroke, and every prefix of a question is a question. An ask
 * starts only after KDOS_RUNNER_ASK_QUIET_MS (600) with no newer one; a newer
 * question drops the exchange in flight, closing its socket, which ends the
 * server's work on it.
 *
 * THIRTY SECONDS, AND AN ANSWER THAT OUTLIVES ITS MATCH. The exchange is given
 * KDOS_RUNNER_ASK_TIMEOUT_MS (30000) from its start. KRunner gives up on a
 * Match reply after 25 seconds, so main.c answers the Match by
 * KDOS_RUNNER_ASK_REPLY_MS (20000) with a "still answering" row and lets the
 * exchange run on; its answer is kept ten minutes, and the same question asked
 * again is answered from it at once.
 * ---------------------------------
 */

#include <ctype.h>
#include <poll.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <time.h>

#include "runner.h"

#define ASK_KEEP_MS   (10 * 60 * 1000)
#define ASK_ANSWER    8192
#define ASK_TOKENS    256
#define ASK_SYSTEM    "Answer briefly, in at most three sentences."

enum {
	AQ_IDLE,
	AQ_QUIET,		/* waiting for the typing to stop             */
	AQ_MODELS,		/* GET /v1/models                             */
	AQ_CHAT,		/* POST /v1/chat/completions                  */
	AQ_ANSWERED,
	AQ_ABSENT,		/* nothing listens on the port                */
	AQ_FAILED,
	AQ_TIMEOUT,
};

static struct {
	int state;
	char q[512];
	char model[160];
	char err[256];
	char *answer;
	uint64_t start_at, give_up, done_at;
	RnAsk *http;
} A;

static uint64_t now_ms(void)
{
	struct timespec ts;

	clock_gettime(CLOCK_MONOTONIC, &ts);
	return (uint64_t)ts.tv_sec * 1000 + (uint64_t)ts.tv_nsec / 1000000;
}

static int env_ms(const char *var, int def, int max)
{
	const char *v = getenv(var);
	int n = v ? atoi(v) : -1;

	return n >= 0 && n <= max ? n : def;
}

int rn_ask_port(void)
{
	int p = env_ms("KDOS_RUNNER_LLAMA_PORT", 8082, 65535);

	return p > 0 ? p : 8082;
}

int rn_ask_quiet_ms(void)
{
	return env_ms("KDOS_RUNNER_ASK_QUIET_MS", 600, 5000);
}

int rn_ask_reply_ms(void)
{
	/* Under KRunner's 25-second D-Bus timeout, with room to send. */
	int r = env_ms("KDOS_RUNNER_ASK_REPLY_MS", 20000, 24000);

	return r > 0 ? r : 20000;
}

static int timeout_ms(void)
{
	int t = env_ms("KDOS_RUNNER_ASK_TIMEOUT_MS", 30000, 120000);

	return t > 0 ? t : 30000;
}

const char *rn_ask_question(const char *text)
{
	const char *p = text ? text : "";

	while (*p == ' ' || *p == '\t')
		p++;
	if (strncasecmp(p, "ask", 3) || (p[3] != ' ' && p[3] != '\t'))
		return NULL;
	p += 3;
	while (*p == ' ' || *p == '\t')
		p++;
	return *p ? p : NULL;
}

/* The question as Match keys it: trimmed, inner space runs collapsed. */
static void normalise(const char *in, char *out, size_t cap)
{
	size_t o = 0;
	int space = 0;

	for (const char *p = in; *p && o + 1 < cap; p++) {
		if (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r') {
			space = o > 0;
			continue;
		}
		if (space && o + 2 < cap)
			out[o++] = ' ';
		space = 0;
		out[o++] = *p;
	}
	out[o] = '\0';
}

static void drop_http(void)
{
	rn_http_free(A.http);
	A.http = NULL;
}

static void reset(void)
{
	drop_http();
	free(A.answer);
	memset(&A, 0, sizeof(A));
}

static void finish(int state, const char *err)
{
	drop_http();
	A.state = state;
	A.done_at = now_ms();
	if (err)
		kb_strlcpy(A.err, err, sizeof(A.err));
}

void rn_ask_want(const char *question, int quiet)
{
	char q[sizeof(A.q)];
	uint64_t now = now_ms();

	normalise(question, q, sizeof(q));
	if (!q[0])
		return;
	if (A.state != AQ_IDLE && !strcmp(A.q, q)) {
		if (A.state < AQ_ANSWERED)
			return;
		if (A.state == AQ_ANSWERED && now - A.done_at < ASK_KEEP_MS)
			return;
		/* A failure, or an answer kept too long: asked again, and
		 * at once, since nobody is typing it. */
		quiet = 0;
	}
	reset();
	kb_strlcpy(A.q, q, sizeof(A.q));
	A.state = AQ_QUIET;
	A.start_at = now + (uint64_t)quiet;
}

static int is_current(const char *question)
{
	char q[sizeof(A.q)];

	normalise(question, q, sizeof(q));
	return A.state != AQ_IDLE && !strcmp(A.q, q);
}

int rn_ask_final(const char *question)
{
	return is_current(question) && A.state >= AQ_ANSWERED;
}

int rn_ask_current(const char *question)
{
	return is_current(question);
}

int rn_ask_busy(void)
{
	return A.state > AQ_IDLE && A.state < AQ_ANSWERED;
}

/* ── the two exchanges ─────────────────────────────────────────────────── */

static void json_esc(KbBuf *b, const char *s)
{
	static const char hex[] = "0123456789abcdef";

	kb_buf_str(b, "\"");
	for (const unsigned char *p = (const unsigned char *)s; *p; p++) {
		if (*p == '"' || *p == '\\') {
			char e[2] = { '\\', (char)*p };

			kb_buf_add(b, e, 2);
		} else if (*p < 0x20) {
			char e[6] = { '\\', 'u', '0', '0', hex[*p >> 4],
				      hex[*p & 15] };

			kb_buf_add(b, e, 6);
		} else {
			kb_buf_add(b, p, 1);
		}
	}
	kb_buf_str(b, "\"");
}

static RnAsk *new_http(void)
{
	RnAsk *a = kb_calloc(1, sizeof(*a));

	a->fd = -1;
	a->port = rn_ask_port();
	a->kind = RN_ASK_LLAMA;
	return a;
}

static void start_models(void)
{
	RnAsk *a = new_http();

	snprintf(a->req, sizeof(a->req),
		 "GET /v1/models HTTP/1.0\r\nHost: 127.0.0.1:%d\r\n"
		 "Accept: application/json\r\nConnection: close\r\n\r\n",
		 a->port);
	A.http = a;
	A.state = AQ_MODELS;
	rn_http_start(a);
}

static int start_chat(void)
{
	RnAsk *a = new_http();
	KbBuf body = { 0 };
	int w;

	kb_buf_str(&body, "{\"model\":");
	json_esc(&body, A.model);
	kb_buf_printf(&body, ",\"stream\":false,\"max_tokens\":%d,"
		      "\"messages\":[{\"role\":\"system\",\"content\":",
		      ASK_TOKENS);
	json_esc(&body, ASK_SYSTEM);
	kb_buf_str(&body, "},{\"role\":\"user\",\"content\":");
	json_esc(&body, A.q);
	kb_buf_str(&body, "}]}");
	w = snprintf(a->req, sizeof(a->req),
		     "POST /v1/chat/completions HTTP/1.0\r\n"
		     "Host: 127.0.0.1:%d\r\nContent-Type: application/json\r\n"
		     "Content-Length: %zu\r\nConnection: close\r\n\r\n%s",
		     a->port, body.n, body.p);
	kb_buf_free(&body);
	if (w < 0 || (size_t)w >= sizeof(a->req)) {
		free(a);
		return -1;
	}
	A.http = a;
	A.state = AQ_CHAT;
	rn_http_start(a);
	return 0;
}

/* The status code of an answered exchange and its body; 0 when the answer is
 * not HTTP at all. */
static int status_of(const RnAsk *a, const char **body)
{
	const char *p = a->resp.p, *b;

	*body = NULL;
	if (!p || strncmp(p, "HTTP/1.", 7) || strlen(p) < 12)
		return 0;
	b = strstr(p, "\r\n\r\n");
	*body = b ? b + 4 : "";
	return atoi(p + 9);
}

/* What a refusal says about itself: llama-server's {"error":{"message"}}. */
static void server_error(int code, const char *body, char *out, size_t cap)
{
	const char *e = body ? json_get(body, "error") : NULL;
	char msg[200] = "";

	if (e && !json_str(json_get(e, "message"), msg, sizeof(msg)))
		json_str(e, msg, sizeof(msg));
	if (msg[0])
		snprintf(out, cap, "The server said %d: %s", code, msg);
	else
		snprintf(out, cap, "The server answered %d", code);
}

static int pick_model(const char *body)
{
	const char *data = json_get(body, "data");
	char first[sizeof(A.model)] = "";

	for (const char *e = data ? json_first(data) : NULL; e; e = json_next(e)) {
		char id[sizeof(A.model)], st[32] = "";
		const char *status = json_get(e, "status");

		if (!json_str(json_get(e, "id"), id, sizeof(id)) || !id[0])
			continue;
		if (status)
			json_str(json_get(status, "value"), st, sizeof(st));
		if (!strcmp(st, "loaded")) {
			kb_strlcpy(A.model, id, sizeof(A.model));
			return 1;
		}
		if (!first[0])
			kb_strlcpy(first, id, sizeof(first));
	}
	if (!first[0])
		return 0;
	kb_strlcpy(A.model, first, sizeof(A.model));
	return 1;
}

/* The answer's text: a reasoning model's <think>…</think> dropped, and the
 * white space around it. */
static char *clean_answer(char *s)
{
	char *e;

	while (isspace((unsigned char)*s))
		s++;
	if (!strncmp(s, "<think>", 7) && (e = strstr(s, "</think>")))
		s = e + 8;
	while (isspace((unsigned char)*s))
		s++;
	e = s + strlen(s);
	while (e > s && isspace((unsigned char)e[-1]))
		*--e = '\0';
	return s;
}

static void got_models(void)
{
	const char *body;
	int code = status_of(A.http, &body);
	char err[256];

	if (code != 200) {
		server_error(code, body, err, sizeof(err));
		finish(AQ_FAILED, err);
		return;
	}
	if (!pick_model(body)) {
		finish(AQ_FAILED, "The server offers no model");
		return;
	}
	drop_http();
	if (start_chat() != 0)
		finish(AQ_FAILED, "The question is too long to send");
}

static void got_answer(void)
{
	const char *body, *choice, *msg;
	int code = status_of(A.http, &body);
	char *text = kb_calloc(ASK_ANSWER, 1), *clean, err[256];

	if (code != 200) {
		server_error(code, body, err, sizeof(err));
		free(text);
		finish(AQ_FAILED, err);
		return;
	}
	choice = json_first(json_get(body, "choices"));
	msg = choice ? json_get(choice, "message") : NULL;
	if (!msg || !json_str(json_get(msg, "content"), text, ASK_ANSWER) ||
	    !*(clean = clean_answer(text))) {
		free(text);
		finish(AQ_FAILED, "The model gave no answer");
		return;
	}
	A.answer = kb_strdup(clean);
	free(text);
	finish(AQ_ANSWERED, NULL);
}

void rn_ask_tick(void)
{
	for (;;) {
		int was = A.state;
		uint64_t now = now_ms();

		switch (A.state) {
		case AQ_QUIET:
			if (now < A.start_at)
				return;
			A.give_up = now + (uint64_t)timeout_ms();
			start_models();
			break;
		case AQ_MODELS:
		case AQ_CHAT:
			if (now >= A.give_up) {
				finish(AQ_TIMEOUT, NULL);
				break;
			}
			if (!A.http->done)
				return;
			if (A.http->done < 0) {
				/* Nothing at all came back from the first
				 * exchange: nothing is listening. */
				if (A.state == AQ_MODELS && !A.http->resp.n)
					finish(AQ_ABSENT, NULL);
				else
					finish(AQ_FAILED, "The server closed "
						"the connection");
				break;
			}
			if (A.state == AQ_MODELS)
				got_models();
			else
				got_answer();
			break;
		default:
			return;
		}
		if (A.state == was)
			return;
	}
}

RnAsk *rn_ask_http(void)
{
	return A.state == AQ_MODELS || A.state == AQ_CHAT ? A.http : NULL;
}

long rn_ask_due(void)
{
	uint64_t now = now_ms(), at;

	if (A.state == AQ_QUIET)
		at = A.start_at;
	else if (A.state == AQ_MODELS || A.state == AQ_CHAT)
		at = A.give_up;
	else
		return -1;
	return at > now ? (long)(at - now) : 0;
}

/* ── rows ──────────────────────────────────────────────────────────────── */

static RnMatch *row(RnList *out, const char *icon)
{
	RnMatch *m = rn_add(out);

	if (!m)
		return NULL;
	kb_strlcpy(m->category, "Local model", sizeof(m->category));
	kb_strlcpy(m->icon, icon, sizeof(m->icon));
	m->catrel = RN_CAT_HIGHEST;
	m->rel = 1.0;
	return m;
}

/* `s` into `out`, cut on a character boundary with an ellipsis when long. */
static void fit(char *out, size_t cap, const char *s)
{
	size_t n = strlen(s);

	if (n < cap) {
		memcpy(out, s, n + 1);
		return;
	}
	n = cap - 4;
	while (n && ((unsigned char)s[n] & 0xc0) == 0x80)
		n--;
	memcpy(out, s, n);
	memcpy(out + n, "…", 4);
}

static void again(RnMatch *m)
{
	m->how = RN_RUN_REQUERY;
	rn_arg(m, "kdos");
	rn_argf(m, "ask %s", A.q);
}

void rn_ask_rows(const char *question, RnList *out)
{
	RnMatch *m;

	if (!is_current(question))
		return;
	switch (A.state) {
	case AQ_ANSWERED:
		if (!(m = row(out, "dialog-information")))
			return;
		fit(m->text, sizeof(m->text), A.answer);
		snprintf(m->subtext, sizeof(m->subtext),
			 "%s · Enter copies the answer", A.model);
		m->multiline = 1;
		m->how = RN_RUN_COPY;
		rn_arg(m, A.answer);
		break;
	case AQ_ABSENT:
		if (!(m = row(out, "dialog-information")))
			return;
		kb_strlcpy(m->text, "No local model is answering",
			   sizeof(m->text));
		snprintf(m->subtext, sizeof(m->subtext),
			 "llama-server answers on 127.0.0.1:%d once a "
			 "language-model pack is installed", rn_ask_port());
		break;
	case AQ_FAILED:
	case AQ_TIMEOUT:
		if (!(m = row(out, "dialog-warning")))
			return;
		kb_strlcpy(m->text, "The local model did not answer",
			   sizeof(m->text));
		if (A.state == AQ_TIMEOUT)
			snprintf(m->subtext, sizeof(m->subtext),
				 "No answer in %d seconds · Enter asks again",
				 timeout_ms() / 1000);
		else
			snprintf(m->subtext, sizeof(m->subtext),
				 "%s · Enter asks again", A.err);
		again(m);
		break;
	default:
		if (!(m = row(out, "view-refresh")))
			return;
		snprintf(m->text, sizeof(m->text), "Still answering: %s", A.q);
		snprintf(m->subtext, sizeof(m->subtext),
			 "%s · Enter asks again", A.model[0] ? A.model
			 : "The local model");
		again(m);
		break;
	}
}

void rn_ask_reset(void)
{
	reset();
}

/* ── by hand ───────────────────────────────────────────────────────────── */

void rn_ask_collect(const char *text, int wait, RnList *out)
{
	const char *q = rn_ask_question(text);
	uint64_t end = now_ms() + (uint64_t)(wait > 0 ? wait : 0);

	if (!q)
		return;
	rn_ask_want(q, 0);
	for (;;) {
		struct pollfd pfd;
		RnAsk *h;
		uint64_t now;
		long due;
		int t;

		rn_ask_tick();
		now = now_ms();
		if (rn_ask_final(q) || now >= end)
			break;
		h = rn_ask_http();
		due = rn_ask_due();
		t = (int)(end - now);
		if (due >= 0 && due < t)
			t = (int)due;
		if (!h || !rn_http_events(h)) {
			if (poll(NULL, 0, t > 0 ? t : 0) < 0)
				break;
			continue;
		}
		pfd.fd = h->fd;
		pfd.events = rn_http_events(h);
		pfd.revents = 0;
		if (poll(&pfd, 1, t > 0 ? t : 0) < 0)
			break;
		if (pfd.revents)
			rn_http_step(h, pfd.revents);
	}
	rn_ask_rows(q, out);
}
