/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   runner.h — the KDOS KRunner runner's pieces
 *
 * Two runners in one process, on one bus name:
 *
 *   /runner    the command palette: kdos verbs, two-letter codes, routes,
 *              KDOS settings pages, boxes, packs on the boot medium,
 *              chords                                      (match.c)
 *   /library   the offline library: books, manual pages, Zeal docsets,
 *              places, kiwix-serve and Kolibri             (library.c)
 *
 * and, on /runner, `ask <question>` for the language model this machine
 * serves on loopback                                       (ask.c)
 *
 * A match is a row KRunner shows and the argument vector Run executes. The
 * vector is built here, element by element, from what the query and the
 * files said; nothing ever joins it into a string a shell reads (run.c).
 * ---------------------------------
 */

#ifndef KDOS_RUNNER_H
#define KDOS_RUNNER_H

#include <stddef.h>
#include <time.h>

#include "kbase.h"

#define RN_ARGV   24		/* elements of one match's vector          */
#define RN_TEXT   1024		/* an answer of the local model is a row  */
#define RN_MAX    48		/* matches one Match returns               */

/* KRunner's QueryMatch::CategoryRelevance. */
#define RN_CAT_HIGHEST 100
#define RN_CAT_HIGH    70
#define RN_CAT_MOD     50
#define RN_CAT_LOW     30
#define RN_CAT_LOWEST  0

/* The actions a match may offer besides its default: the same vector in
 * Konsole, held open, for a verb whose answer is printed; and the answer of
 * the local model, copied. */
#define RN_ACT_TERM "terminal"
#define RN_ACT_COPY "copy"

/* How Run starts the vector. */
enum {
	RN_RUN_DETACH,		/* in the background; a failure is a toast   */
	RN_RUN_TERM,		/* in Konsole, held open after it exits      */
	RN_RUN_COPY,		/* argv[0] onto the clipboard, through Klipper */
	RN_RUN_REQUERY,		/* KRunner opened again on argv[1], with the
				 * runner whose id is argv[0] alone       */
};

typedef struct {
	char text[RN_TEXT];
	char subtext[RN_TEXT];
	char icon[128];
	char category[64];
	char url[1024];		/* "urls" property; "" for none             */
	int catrel;		/* RN_CAT_*                                  */
	double rel;		/* 0..1, within the category                 */
	int how;		/* RN_RUN_*                                  */
	int term_action;	/* offer RN_ACT_TERM                         */
	int multiline;		/* KRunner shows the text on several lines   */
	int argc;
	char *argv[RN_ARGV];	/* owned                                     */
} RnMatch;

typedef struct {
	RnMatch *v;
	int n, cap;
} RnList;

/* A new row at the end of `l`, zeroed. NULL once the list holds RN_MAX. */
RnMatch *rn_add(RnList *l);
void rn_arg(RnMatch *m, const char *s);
void rn_argf(RnMatch *m, const char *fmt, ...)
	__attribute__((format(printf, 2, 3)));
void rn_match_free(RnMatch *m);
void rn_list_free(RnList *l);
/* Highest category first, then relevance, then text: the order a test can
 * compare and the order KRunner would show within one runner. */
void rn_list_sort(RnList *l);
/* One block per row: text, subtext, icon, category and relevance, then the
 * vector one element per line. What `--query` prints and the tests diff. */
void rn_list_print(const RnList *l, KbBuf *out);

/* ── the query ─────────────────────────────────────────────────────────── */

#define RN_WORDS 16

typedef struct {
	char raw[512];		/* trimmed, inner runs of space collapsed    */
	char low[512];		/* the same, ASCII-lowercased                */
	int nw;
	char *w[RN_WORDS];	/* words of `low`, in place                  */
	char *rw[RN_WORDS];	/* the same words in `raw`'s case            */
	char buf[512];		/* `low` cut into words                      */
	char rbuf[512];		/* `raw` cut into words                      */
} RnQuery;

void rn_query(RnQuery *q, const char *text);
/* `raw` from word `i` to the end, as typed. */
const char *rn_query_rest(const RnQuery *q, int i);
/* Does each word of `q` start a word of `hay` (separators: space . - _ /),
 * in any order? 0 if not; otherwise a score that grows with the share of
 * `hay` the query covers. */
int rn_words_match(const char *hay, const RnQuery *q);
int rn_prefix(const char *word, const char *typed);

/* ── the files the palette reads (sources.c) ───────────────────────────── */

/* `abs` under $KDOS_RUNNER_ROOT, the fixture root; `abs` itself unset. */
const char *rn_sys(const char *abs, char *out, size_t n);

/* Every reader below caches what it parsed and re-reads only when a file's
 * modification time or size changed, so a keystroke costs a stat(). */

typedef struct {
	char id[128];		/* desktop-entry id, without .desktop        */
	char code[4];		/* two letters, lowercased; "" for none      */
	char name[128];		/* the entry's Name, or the id               */
	char icon[128];
} RnFavorite;

int rn_favorites(const RnFavorite **out);

typedef struct {
	char name[96];		/* verb.noun                                 */
	char value[512];	/* the argument vector as written            */
} RnRoute;

int rn_routes(const RnRoute **out);

/* The <name> of each ~/.config/kdos/boxes/<name>.conf. */
int rn_boxes(const char *const **out);

typedef struct {
	char id[128];		/* the file name less .kpack or .ktar        */
	char path[512];		/* absolute, for Discover's --local-filename */
	int set;		/* a .ktar set rather than one pack           */
} RnMediumPack;

/* The packs and sets on the boot medium: $KDOS_PACK_MEDIUM, the variable
 * Discover's KDOS backend reads, or /mnt/iso/packs. */
int rn_medium(const RnMediumPack **out);

typedef struct {
	char component[96];	/* kwin, plasmashell, services/org.kde.konsole.desktop */
	char action[96];	/* the action id                              */
	char label[160];	/* what the component calls it               */
	char keys[128];		/* "Meta+T, Meta+Shift+T"; "" when unbound   */
} RnChord;

int rn_chords(const RnChord **out);

/* Name and Icon of an application by desktop-entry id. 0 when no entry of
 * that id is installed or it is hidden. */
int rn_desktop_entry(const char *id, char *name, size_t nn, char *icon,
		     size_t in);

/* Drop every cache, so the next read re-parses: the tests change fixture
 * files faster than a filesystem's timestamp resolution. */
void rn_sources_reset(void);

/* $XDG_CONFIG_HOME and $XDG_DATA_HOME, or their ~ defaults. */
void rn_config_home(char *out, size_t n);
void rn_data_home(char *out, size_t n);

/* ── the palette (match.c) ─────────────────────────────────────────────── */

void rn_palette_match(const char *query, RnList *out);

/* ── the library (library.c) ───────────────────────────────────────────── */

/* The local half: books, manual pages, docsets, places. */
void rn_library_match(const RnQuery *q, RnList *out);

/* The query the library answers, leading blanks dropped, or NULL when the
 * text is blank. */
const char *rn_library_query(const char *text);

/* One loopback HTTP question the library asks a local server. */
typedef struct RnAsk RnAsk;

/* The servers to ask about `q`: kiwix-serve once per book, Kolibri once. */
int rn_library_asks(const RnQuery *q, RnAsk **out);
/* Turn an answered ask into rows. */
void rn_library_answer(RnAsk *a, const RnQuery *q, RnList *out);
void rn_library_reset(void);
/* Build a missing book index in a child instead of in place: what the
 * server does, since it answers the palette from the same loop. */
void rn_library_async(int on);
/* Everything, the servers asked and waited for up to `deadline` ms: what
 * Match answers, without a bus. */
void rn_library_collect(const char *text, int deadline, RnList *out);

/* The Zeal half of the local library (zeal.c). */
void rn_zeal_match(const RnQuery *q, RnList *out);
void rn_zeal_reset(void);
/* 0 when this build has no SQLite and searches no docset. */
int rn_zeal_built(void);

/* ── loopback HTTP (http.c) ────────────────────────────────────────────── */

struct RnAsk {
	int fd;
	int port;
	int kind;		/* RN_ASK_*                                  */
	char book[160];		/* kiwix: the name the server routes          */
	char title[160];	/* kiwix: the book's title, for the subtext   */
	char req[4096];
	size_t sent, reqlen;
	KbBuf resp;
	int done;		/* 1 answered, -1 failed                      */
	RnAsk *next;
};

enum { RN_ASK_KIWIX, RN_ASK_KOLIBRI, RN_ASK_LLAMA };

/* Start a non-blocking connect to 127.0.0.1:port for `a->req`. -1 when the
 * socket cannot even be made; a refused connect shows up as done = -1. */
int rn_http_start(RnAsk *a);
/* The poll events this ask waits for, and progress when they arrive. */
short rn_http_events(const RnAsk *a);
void rn_http_step(RnAsk *a, short revents);
/* The body of a 200 answer, or NULL. */
const char *rn_http_body(const RnAsk *a);
void rn_http_free(RnAsk *a);
void rn_urlenc(KbBuf *b, const char *s, int keep_slash);

/* ── the local model (ask.c) ─────────────────────────────────────────── */

/* The question after a leading `ask `, or NULL when the text is no ask. */
const char *rn_ask_question(const char *text);
/* Ask it once `quiet` ms pass with no newer question; the same question in
 * flight, or answered in the last ten minutes, is kept as it is. A different
 * one drops the exchange in flight. */
void rn_ask_want(const char *question, int quiet);
/* Advance the exchange: its start, its two requests, its timeout. */
void rn_ask_tick(void);
/* The exchange in flight, for the poll set; NULL when there is none. */
RnAsk *rn_ask_http(void);
/* Milliseconds until rn_ask_tick has a timer to act on; -1 for none. */
long rn_ask_due(void);
/* Is this the question asked last, and has it an answer or a failure? */
int rn_ask_current(const char *question);
int rn_ask_final(const char *question);
/* A question is waiting to start or is being answered. */
int rn_ask_busy(void);
/* The rows for the question as it stands: the answer, a failure, or
 * "still answering". None for a question that is not the current one. */
void rn_ask_rows(const char *question, RnList *out);
void rn_ask_reset(void);
/* The whole ask, waited for up to `wait` ms with no quiet: --query and the
 * tests. */
void rn_ask_collect(const char *text, int wait, RnList *out);
int rn_ask_port(void);
int rn_ask_quiet_ms(void);
int rn_ask_reply_ms(void);

/* ── JSON, read in place (json.c) ──────────────────────────────────────── */

const char *json_ws(const char *p);
const char *json_skip(const char *p);
/* The value of `key` in the object at `obj`, or NULL. */
const char *json_get(const char *obj, const char *key);
/* The first element of the array at `p`, then each next one; NULL at end. */
const char *json_first(const char *p);
const char *json_next(const char *p);
/* The string at `p`, unescaped, into out. 0 when `p` is not a string. */
int json_str(const char *p, char *out, size_t cap);
long long json_int(const char *p, long long def);

/* `&amp;` and the numeric references, in place: kiwix-serve renders its
 * suggestions through an HTML template. Tags are dropped too. */
void rn_html_text(char *s);

/* ── running a match (run.c) ───────────────────────────────────────────── */

/* The token KRunner hands over just before Run, for the window that opens
 * to take focus. Consumed by the next run. */
void rn_run_token(const char *token);
/* 0 when the child was started; it is never waited for here. */
int rn_run(const RnMatch *m, int in_terminal);

/* The vector a match would exec, one element per line, for the tests. */
void rn_run_describe(const RnMatch *m, int in_terminal, KbBuf *out);

#endif
