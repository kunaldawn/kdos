/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   fwcore — what kdos-fwask decides, with no bus in it
 *
 * The decision is three facts and a memory. A closed service's knock counter
 * rose since the last look; the program that serves it is running as this
 * user; and this user has not been asked about that service on this network.
 * Then, and only then, the person is asked.
 *
 * Everything here is pure enough to run on a bare host: the counter
 * arithmetic, the process scan (under a root that a test can point
 * elsewhere), and the answers file. main.c owns the buses.
 */

#ifndef KDOS_FWCORE_H
#define KDOS_FWCORE_H

#include <stddef.h>
#include <sys/types.h>

/* ── what the prompt knows ─────────────────────────────────────────────── */

/*
 * A SERVICE IS ASKED ABOUT ONLY WHEN ITS PROGRAM IS KNOWN. A knock on `http`
 * with no web server running is a port scan, not a person whose phone cannot
 * connect, and a prompt for it would teach people to click Allow. Each row
 * names kdos-powerd's service, the process name (comm, at most 15 bytes) that
 * serves it in a session, and the sentence the person reads.
 */
typedef struct {
	const char *service;
	const char *program;
	const char *app;
	const char *summary;
} FaAsk;

extern const FaAsk fa_asks[];
extern const int fa_nasks;

const FaAsk *fa_find(const char *service);

/* ── names that are allowed through ────────────────────────────────────── */

/*
 * A TOKEN IS CHECKED BEFORE IT IS STORED OR SENT. Service names come from the
 * daemon and network ids from NetworkManager, and both are written into a file
 * this program reads back: a newline or a space in either would forge a second
 * record. Lowercase letters, digits and '-' only, 1 to max-1 bytes.
 */
int fa_token_ok(const char *s, size_t max);

/* ── the knock counters ────────────────────────────────────────────────── */

#define FA_NAME 32
#define FA_MAX_KNOCKS 64

typedef struct {
	char name[FA_NAME];
	unsigned long long packets;
} FaKnock;

typedef struct {
	FaKnock k[FA_MAX_KNOCKS];
	int n;
} FaKnocks;

/*
 * Which names rose between `prev` and `now`, into rose[] (at most `max`);
 * `prev` becomes `now`. A name seen for the first time counts its whole
 * count as new: a phone that knocked before the session started is still a
 * phone that cannot connect. A count that FELL is a ruleset reload (every
 * firewall change is one), and the new count is all new.
 */
int fa_knocks_rose(FaKnocks *prev, const FaKnocks *now, char rose[][FA_NAME],
		   int max);

/* ── is it running, as this user ───────────────────────────────────────── */

/*
 * A process named `comm` whose owner is `uid`, found by reading
 * <proc_root>/<pid>/comm and stat'ing <proc_root>/<pid>. Another user's
 * KDE Connect is that user's question, not this one's.
 */
int fa_running(const char *proc_root, const char *comm, uid_t uid);

/* ── the answers ───────────────────────────────────────────────────────── */

#define FA_NET 40
#define FA_ANSWER 12
#define FA_MEMORY_KEEP 256

typedef struct {
	char net[FA_NET];
	char name[FA_NAME];
	char answer[FA_ANSWER];
} FaAnswer;

typedef struct {
	FaAnswer *a;
	int n, cap;
} FaMemory;

/*
 * The file is one record per line, `<network> <service> <answer>`, appended as
 * each answer is given. A line that is not three valid tokens is skipped, not
 * fatal: the file is the user's, and a hand edit must not stop the prompts
 * from working. Returns the records kept, or -1 when the file exists and
 * cannot be read.
 */
int fa_memory_load(FaMemory *m, const char *path);
int fa_memory_asked(const FaMemory *m, const char *net, const char *name);
/*
 * Append one answer. Once the file holds more than FA_MEMORY_KEEP records it
 * is rewritten whole, newest last, keeping the most recent FA_MEMORY_KEEP;
 * the order matters because a later answer for the same pair wins.
 */
int fa_memory_record(FaMemory *m, const char *path, const char *net,
		     const char *name, const char *answer);
void fa_memory_free(FaMemory *m);

/* ── the words ─────────────────────────────────────────────────────────── */

/*
 * TEXT FROM ELSEWHERE IS ESCAPED BEFORE IT IS PUT IN A NOTIFICATION. A
 * network's name is whoever set up the access point's choice, a daemon's
 * refusal is its own words, and the notification server reads the body as
 * markup: `&`, `<` and `>` become entities, so neither can open a tag or a
 * link, and control characters become spaces. `cut`, when not 0, is the most
 * input bytes kept, moved back to a character boundary; the output is always
 * terminated, and truncated rather than overrun.
 */
void fa_escape(char *out, size_t n, const char *s, size_t cut);

/*
 * The body of a prompt: where it is blocked and what each answer does. The
 * network's own name is quoted when NetworkManager gave one. `may` is 0 when
 * polkit said this user may not change the firewall, and the body then says
 * who can instead of offering a button that would only fail.
 */
void fa_body(char *out, size_t n, const FaAsk *ask, const char *netname,
	     int may);

#endif
