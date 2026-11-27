/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   notice.h — kdos-notice's pieces
 *
 *   facts.c    what the system says: `kdos restarts --json`, `kdos cve
 *              --json`, `kdos-bootctl status --json`, kdos-oomd's status,
 *              the installer's /var/lib/kdos/apps-pending and
 *              /var/lib/kdos/groups-pending, the tree's /ports/groups,
 *              `kdos firmware sync --dry-run --json`
 *   decide.c   facts plus what was already said -> what to post or close
 *   state.c    what was already said, kept across logins
 *   main.c     the session bus, the watches, the timers, the actions
 *   bless.c    --bless: confirm this boot once Plasma has stayed up
 *
 * facts.c and decide.c touch no bus, so the tests drive them with fixture
 * JSON on a bare host; main.c is the only file that posts.
 * ---------------------------------
 */

#ifndef KDOS_NOTICE_H
#define KDOS_NOTICE_H

#include <stddef.h>

#include "kbase.h"

/* The notices, one live notification each at most. */
enum {
	KN_RESTARTS,		/* programs running replaced code     */
	KN_CVE,			/* known vulnerabilities installed    */
	KN_TRIAL,		/* this boot is a trial slot          */
	KN_ROLLBACK,		/* the last trial did not start       */
	KN_OOM,			/* kdos-oomd closed something         */
	KN_PENDING,		/* choices made at install still wait  */
	KN_FIRMWARE,		/* a firmware port the hardware needs  */
	KN_COUNT
};

const char *kn_kind_name(int kind);
int kn_kind_of(const char *name);

/* ── facts ─────────────────────────────────────────────────────────────── */

#define KN_PROCS 64
#define KN_FINDINGS 128

typedef struct {
	int pid;
	char comm[64];
	char exe[256];
	char name[128];		/* the application's Name, or ""      */
	int desktop;		/* one of the session's own programs  */
} KnProc;

typedef struct {
	int ok;			/* the command answered               */
	int n;
	KnProc p[KN_PROCS];
} KnRestarts;

typedef struct {
	char port[64];
	char version[64];
	char fixed[64];
	char cves[512];		/* "CVE-a,CVE-b"                      */
} KnFinding;

typedef struct {
	int ok;
	int n;
	int ncve;		/* CVE ids across every finding       */
	KnFinding f[KN_FINDINGS];
} KnCve;

typedef struct {
	int ok;			/* answered and configured            */
	char running[4];	/* "a", "b", ""                       */
	char active[4];
	char trying[4];
	char failed[4];
	char ver[2][64];	/* slot a, slot b; "" when unknown    */
	char build[2][64];
} KnBoot;

typedef struct {
	int ok;			/* the daemon answered `ok …`         */
	int kills;
	char last[256];		/* "comm (appbox www) (pid 9, 812 MB)" */
} KnOom;

typedef struct {
	int n;			/* selection ids apps-pending names   */
	int ngroups;		/* group ids groups-pending names     */
	char groups[384];	/* those groups by their titles, or by
				 * id when the tree gives no title;
				 * one name a line, joined with "\n";
				 * markup-escaped; "" when they do not
				 * fit                                 */
	int untitled;		/* of those, named by id alone        */
	char key[32];		/* both files' ids, hashed: a changed
				 * title posts nothing new             */
} KnPending;

#define KN_FW_PORTS 16

typedef struct {
	int ok;			/* the command answered               */
	int n;
	char port[KN_FW_PORTS][64];
	char reason[KN_FW_PORTS][192];
} KnFirmware;

/* Parse the commands' JSON; 0 when the document is not theirs. Restarts
 * keeps only the processes `kdos restarts` marks `"replaced"` (an entry
 * without the key counts as replaced). */
int kn_parse_restarts(const char *json, KnRestarts *out);
int kn_parse_cve(const char *json, KnCve *out);
int kn_parse_boot(const char *json, KnBoot *out);
int kn_parse_oom(const char *line, KnOom *out);
int kn_parse_firmware(const char *json, KnFirmware *out);
/* The installer's two records, one id a line in each. `apps`, the boxed
 * selections, is read as Discover reads it: a line with a space (`group
 * <id>`) or a `#` comment is not counted. `groups`, the native groups, is
 * read as `kdos update install --pending` reads it: each line cut at its
 * first space, tab or `#`, and counted when a package name is left.
 * `titles` is the tree's groups file (/ports/groups), whose `group <id>
 * <title>` lines name each group as the installer offered it. NULL is an
 * empty file. */
void kn_parse_pending(const char *apps, const char *groups,
		      const char *titles, KnPending *out);

/* Run the command and parse it. */
void kn_read_restarts(KnRestarts *out);
void kn_read_cve(KnCve *out);
void kn_read_boot(KnBoot *out);
void kn_read_oom(KnOom *out);
void kn_read_pending(KnPending *out);
void kn_read_firmware(KnFirmware *out);

/* Name each process by its application's desktop entry, and mark the
 * session's own programs. */
void kn_name_procs(KnRestarts *r);

/* The kernel's id of this boot, so a notice about one boot is not repeated
 * on it and is asked again on the next. */
void kn_boot_id(char *out, size_t n);

/* ── what was said ─────────────────────────────────────────────────────── */

typedef struct {
	char key[KN_COUNT][192];	/* the state last posted, "" none     */
	unsigned id[KN_COUNT];		/* its notification, 0 when gone     */
} KnState;

void kn_state_load(KnState *s);
int kn_state_save(const KnState *s);

/* ── what to say ───────────────────────────────────────────────────────── */

#define KN_ACT_DETAILS "details"
#define KN_ACT_LOGOUT  "logout"

typedef struct {
	int kind;
	int close;		/* withdraw the live one instead      */
	char key[192];		/* the state this posts               */
	char summary[256];
	char body[1024];
	const char *icon;
	int persistent;		/* waits for a person: expire 0       */
	int urgency;		/* 0 low, 1 normal, 2 critical        */
	const char *actions[6];	/* id, label pairs, NULL-ended        */
} KnPost;

/* The posts these facts call for, given what `s` says was already posted.
 * Each fills at most one post per kind and returns 1, or returns 0 when
 * nothing changed. A source that did not answer changes nothing. */
int kn_decide_restarts(const KnRestarts *r, const char *boot,
		       const KnState *s, KnPost *out);
int kn_decide_cve(const KnCve *c, const KnState *s, KnPost *out);
int kn_decide_trial(const KnBoot *b, const char *boot, const KnState *s,
		    KnPost *out);
int kn_decide_rollback(const KnBoot *b, const KnState *s, KnPost *out);
/* `prev` is the count the session started with or last saw. */
int kn_decide_oom(const KnOom *o, int prev, KnPost *out);
int kn_decide_pending(const KnPending *pd, const KnState *s, KnPost *out);
int kn_decide_firmware(const KnFirmware *f, const KnState *s, KnPost *out);

/* What each action of a kind runs, as an argument vector; NULL-ended in
 * `a`. 0 when the action is not one that kind offers. */
int kn_action_argv(int kind, const char *action, KbArgv *a);

/* ── JSON, read in place (json.c) ──────────────────────────────────────── */

const char *json_ws(const char *p);
const char *json_skip(const char *p);
const char *json_get(const char *obj, const char *key);
const char *json_first(const char *p);
const char *json_next(const char *p);
int json_str(const char *p, char *out, size_t cap);
long long json_int(const char *p, long long def);
int json_is_null(const char *p);

#endif
