/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   kdos-notifyd — org.freedesktop.Notifications, as character cells
 *
 *   ╔══════════════════════════════╗
 *   ║ Battery low                  ║
 *   ║ 9% remaining                 ║
 *   ╚══════════════════════════════╝
 *
 * The session's notification daemon. Every alien app expects one to exist:
 * without a bus name owning org.freedesktop.Notifications, a GTK app's
 * `notify_notification_show` fails and — worse — anything using gdbus waits out
 * its default 25-SECOND reply timeout first. kdos-appbox already carries a
 * comment about that timeout for the same reason.
 *
 * TWO EVENT SOURCES, ONE POLL. A notification daemon is idle almost always, so
 * it waits on the Wayland fd and the bus fd together rather than polling either
 * on a timer. The third deadline is the next toast's expiry, which is what the
 * poll timeout is computed from — so an idle session wakes this process exactly
 * never, and a session with a 5-second toast wakes it once.
 */

#include <errno.h>
#include <poll.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/un.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

/*
 * KDOS ships basu, which is sd-bus extracted from systemd; a development host
 * usually has libsystemd, whose sd-bus is the same API. Selecting between them
 * here rather than in the build is what lets this daemon be exercised on a
 * machine that is not KDOS — and every defect found in this rewrite was found
 * by running something, not by reading it.
 */
#if defined(__has_include)
#  if __has_include(<basu/sd-bus.h>)
#    include <basu/sd-bus.h>
#  else
#    include <systemd/sd-bus.h>
#  endif
#else
#  include <basu/sd-bus.h>
#endif

#include "kwl.h"
#include "shell.h"

#define MAX_TOASTS 4
#define TOAST_COLS 40
#define MAX_ACTS 3
#define BODY_W (TOAST_COLS - 4)
#define BODY_LINES 3

/* Style flags, one byte per byte of the cleaned body. */
enum { ST_B = 1, ST_I = 2, ST_U = 4, ST_A = 8 };

struct toast {
	uint32_t id;
	char summary[128];
	char body[256];		/* markup stripped, C0 sanitized       */
	uint8_t style[256];	/* ST_* per body byte                  */
	char href[256];		/* the FIRST link's target             */
	int nlinks;		/* how many <a href> the body carried  */
	int body_rows;		/* wrapped height, fixed at receipt    */
	char app[64];
	char act_id[MAX_ACTS][48];
	char act_label[MAX_ACTS][24];
	int nact;
	int64_t expires_ms;	/* monotonic; 0 = never */
	/*
	 * WHAT IS LEFT OF THE COUNTDOWN WHILE THE POINTER IS ON IT.
	 *
	 * A toast that disappears while it is being read is a toast that has
	 * to be read twice — and it cannot be, because it is gone. Every
	 * notification daemon of the last fifteen years pauses on hover; here
	 * it costs one field: the remaining time is banked, `expires_ms` goes
	 * to 0 (never), and the leave puts it back with a floor under it so a
	 * toast the pointer merely crossed does not vanish in the same frame.
	 */
	int64_t paused_ms;
	int urgent;
};

/* Which toast the pointer is over, or -1. */
static int hover_toast = -1;
static void hover_resume_all(void);

static struct toast toasts[MAX_TOASTS];
static int ntoasts;
static uint32_t next_id = 1;
static sd_bus *bus;

/* Where the last frame put each toast's buttons — a hit map that outlives
 * what it describes is how a click lands on the wrong action. Rebuilt by
 * draw_toasts(), cleared by drop_at(). */
static struct {
	int row;
	int x[MAX_ACTS], end[MAX_ACTS];
} act_hit[MAX_TOASTS];

static int64_t now_ms(void)
{
	struct timespec ts;
	clock_gettime(CLOCK_MONOTONIC, &ts);
	return (int64_t)ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}

/* ── the history, and the centre that reads it ──────────────────────────────
 *
 * A NOTIFICATION THAT EXPIRED IS NOT A NOTIFICATION THAT WAS READ, and until
 * now this daemon threw one away the moment its five seconds were up. Every
 * desktop of the last decade keeps a list — the thing that arrived while you
 * were in another workspace, the download that finished while the screen was
 * locked — and on KDOS the case is sharper than elsewhere: every fat
 * application lives in a container, so a boxed app's notification is often the
 * ONLY thing that ever says the work it was doing has finished.
 *
 * The shape is kdos-clip's, deliberately, because that is the one this desktop
 * already has: the DAEMON owns the list and a short connection per request
 * hands it out over a unix socket in $XDG_RUNTIME_DIR, so the front end is an
 * ordinary program that can be run by hand, scripted or replaced, and the
 * panel can ask for one number once a second without either process knowing
 * anything about the other's drawing.
 *
 * `unseen` is what the panel's badge counts: entries that have arrived since
 * somebody last opened the centre. Cleared by the `seen` command and by
 * nothing else — a count that cleared itself on a timer would be a count
 * nobody trusts.
 */
#define NHIST 64

struct hentry {
	char app[64];
	char summary[128];
	char body[256];
	char href[256];
	time_t when;			/* wall clock, for the list      */
	int urgent;
};

static struct hentry hist[NHIST];
static int nhist;			/* newest LAST, like every ring here */
static int unseen;
/*
 * DO NOT DISTURB, which is the other half of a notification centre: without
 * somewhere for a notification to GO, silencing toasts would mean losing them.
 * With the history in place it is honest — the toast is not drawn, the entry
 * is kept, and the badge says how many are waiting.
 */
static int dnd;

static void hist_push(const struct toast *t)
{
	struct hentry *h;

	if (nhist >= NHIST) {
		memmove(&hist[0], &hist[1], (NHIST - 1) * sizeof(hist[0]));
		nhist = NHIST - 1;
	}
	h = &hist[nhist++];
	memset(h, 0, sizeof(*h));
	snprintf(h->app, sizeof(h->app), "%s", t->app);
	snprintf(h->summary, sizeof(h->summary), "%s", t->summary);
	snprintf(h->body, sizeof(h->body), "%s", t->body);
	snprintf(h->href, sizeof(h->href), "%s", t->href);
	h->when = time(NULL);
	h->urgent = t->urgent;
	if (unseen < NHIST)
		unseen++;
}

/* ── the socket ────────────────────────────────────────────────────────────
 *
 * kdos-clip's shape, down to the mode: $XDG_RUNTIME_DIR is already 0700 and
 * what this hands out is exactly as private as the session it belongs to.
 * One word per connection, one answer, and the daemon never blocks on a
 * client — a notification daemon that stalled while somebody read a list
 * would be a notification daemon that stopped answering the bus.
 */
static int notify_sock_path(char *out, size_t n)
{
	const char *run = getenv("XDG_RUNTIME_DIR");

	if (!run || !*run)
		return -1;
	return snprintf(out, n, "%s/kdos-notify.sock", run) < (int)n ? 0 : -1;
}

static void open_href(const char *href);

static void serve_client(int c)
{
	char buf[64] = {0};
	ssize_t n = read(c, buf, sizeof(buf) - 1);

	if (n <= 0)
		return;
	buf[n] = '\0';
	buf[strcspn(buf, "\r\n")] = '\0';

	if (!strcmp(buf, "count")) {
		/* `<unseen> <total> <dnd>` — everything the panel's badge
		 * needs in one line, because it asks once a second. */
		dprintf(c, "%d %d %d\n", unseen, nhist, dnd);
	} else if (!strcmp(buf, "list")) {
		/*
		 * NEWEST FIRST, which is the order a list of things that
		 * happened is read in — and tab separated, which is safe
		 * because sanitize() has already turned every tab and newline
		 * in somebody else's string into a space.
		 */
		for (int i = nhist - 1; i >= 0; i--)
			dprintf(c, "%d\t%lld\t%d\t%s\t%s\t%s\n", i,
				(long long)hist[i].when, hist[i].urgent,
				hist[i].app, hist[i].summary, hist[i].body);
		(void)!write(c, "ok\n", 3);
	} else if (!strcmp(buf, "seen")) {
		unseen = 0;
		(void)!write(c, "ok\n", 3);
	} else if (!strncmp(buf, "open ", 5)) {
		int i = atoi(buf + 5);

		if (i >= 0 && i < nhist && hist[i].href[0])
			open_href(hist[i].href);
		(void)!write(c, "ok\n", 3);
	} else if (!strncmp(buf, "forget ", 7)) {
		int i = atoi(buf + 7);

		if (i >= 0 && i < nhist) {
			memmove(&hist[i], &hist[i + 1],
				(size_t)(nhist - i - 1) * sizeof(hist[0]));
			nhist--;
			if (unseen > nhist)
				unseen = nhist;
		}
		(void)!write(c, "ok\n", 3);
	} else if (!strcmp(buf, "clear")) {
		nhist = 0;
		unseen = 0;
		(void)!write(c, "ok\n", 3);
	} else if (!strncmp(buf, "dnd", 3)) {
		const char *a = buf + 3;

		while (*a == ' ')
			a++;
		if (!strcmp(a, "on"))
			dnd = 1;
		else if (!strcmp(a, "off"))
			dnd = 0;
		else if (!strcmp(a, "toggle") || !*a)
			dnd = !dnd;
		dprintf(c, "%d\n", dnd);
	} else {
		(void)!write(c, "err unknown command\n", 20);
	}
}

/* ── the stack ─────────────────────────────────────────────────────────── */

static void emit_closed(uint32_t id, uint32_t reason)
{
	if (bus)
		sd_bus_emit_signal(bus, "/org/freedesktop/Notifications",
				   "org.freedesktop.Notifications",
				   "NotificationClosed", "uu", id, reason);
}

/* reason: 1 expired, 2 dismissed by the user, 3 closed by a CloseNotification
 * call. The spec requires the signal either way, and a client that is waiting
 * for it before it will show another notification hangs without it. */
static void drop_at(int i, uint32_t reason)
{
	if (i < 0 || i >= ntoasts)
		return;
	emit_closed(toasts[i].id, reason);
	/* On the way OUT, whatever took it out: expiry, a click, or the
	 * client's own CloseNotification. The centre is the answer to "what
	 * was that", and the ones a person never saw are exactly the ones
	 * that answer it. */
	hist_push(&toasts[i]);
	memmove(&toasts[i], &toasts[i + 1],
		(size_t)(ntoasts - i - 1) * sizeof(toasts[0]));
	ntoasts--;
	/* The stack moved under the POINTER as well as under the hit map: a
	 * held index that outlives its toast holds the wrong one's clock. */
	hover_resume_all();
	/*
	 * The stack just moved under the hit map. libkwl hands the pointer
	 * drain a whole BATCH from one read, so a double-click on a button
	 * dismissed the first toast and then found the same row and column
	 * still claimed by the map — and fired the SECOND notification's
	 * action, which nobody aimed at. The next frame rebuilds it.
	 */
	memset(act_hit, 0, sizeof(act_hit));
}

static void expire_due(void)
{
	int64_t t = now_ms();
	for (int i = ntoasts - 1; i >= 0; i--)
		if (toasts[i].expires_ms && toasts[i].expires_ms <= t)
			drop_at(i, 1);
}

/* The pointer arrived on `i` (or on nothing). Banks and restores the
 * countdown; the floor is a second and a half, so crossing the corner of the
 * screen never costs a notification. */
#define TOAST_RESUME_MIN 1500

/*
 * Put every held countdown back. EVERY, not the one `hover_toast` names: the
 * stack moves under the pointer whenever a toast is dropped, and an index that
 * outlives what it pointed at would leave a paused toast paused for the rest
 * of the session with nothing left to resume it.
 */
static void hover_resume_all(void)
{
	for (int i = 0; i < ntoasts; i++) {
		if (!toasts[i].paused_ms)
			continue;
		int64_t left = toasts[i].paused_ms;

		if (left < TOAST_RESUME_MIN)
			left = TOAST_RESUME_MIN;
		toasts[i].expires_ms = now_ms() + left;
		toasts[i].paused_ms = 0;
	}
	hover_toast = -1;
}

static void hover_set(int i)
{
	if (i == hover_toast)
		return;
	hover_resume_all();
	hover_toast = i;
	if (i >= 0 && i < ntoasts && toasts[i].expires_ms) {
		int64_t left = toasts[i].expires_ms - now_ms();

		toasts[i].paused_ms = left > 0 ? left : TOAST_RESUME_MIN;
		toasts[i].expires_ms = 0;	/* held, not forgotten */
	}
}

/* How long until something needs doing, or -1 to wait forever. */
static int next_timeout(void)
{
	int64_t best = -1, t = now_ms();
	for (int i = 0; i < ntoasts; i++) {
		if (!toasts[i].expires_ms)
			continue;
		int64_t d = toasts[i].expires_ms - t;
		if (d < 0)
			d = 0;
		if (best < 0 || d < best)
			best = d;
	}
	return best < 0 ? -1 : (int)best;
}

/* ── text, as it arrives off the bus ───────────────────────────────────── */

/* A summary or body comes from any program on the bus; `\n` and `\t` become
 * the space they meant and every other C0 (and DEL) is dropped rather than
 * painted as a junk glyph. */
static void sanitize(char *s)
{
	char *w = s;

	for (char *r = s; *r; r++) {
		unsigned char c = (unsigned char)*r;
		if (c == '\n' || c == '\t')
			*w++ = ' ';
		else if (c >= 0x20 && c != 0x7f)
			*w++ = *r;
	}
	*w = '\0';
}

/*
 * The markup subset: <b> <i> <u> <a href="..."> and the four entities.
 * Unknown tags are STRIPPED, never shown — advertising `body-markup` means a
 * client may send anything HTML-shaped, and a literal `<img>` painted into a
 * toast is the failure the capability list used to avoid by lying the other
 * way. Only the FIRST link's target is kept; the count is what decides
 * whether a click can open anything (one link is unambiguous, two is a menu
 * this surface does not have).
 */
static void parse_markup(const char *in, char *out, uint8_t *style, size_t cap,
			 char *href, size_t hrefcap, int *nlinks)
{
	size_t o = 0;
	unsigned cur = 0;

	*nlinks = 0;
	href[0] = '\0';
	for (const char *r = in; *r && o + 1 < cap;) {
		if (*r == '<') {
			const char *end = strchr(r, '>');
			if (!end) {
				/* A lone `<` is text, not a tag. */
				out[o] = '<';
				style[o++] = (uint8_t)cur;
				r++;
				continue;
			}
			const char *t = r + 1;
			size_t tn = (size_t)(end - t);
			int off = tn > 0 && *t == '/';
			if (off) {
				t++;
				tn--;
			}
			char c0 = tn ? (char)(*t | 0x20) : 0;
			if (tn == 1 && c0 == 'b') {
				cur = off ? cur & ~ST_B : cur | ST_B;
			} else if (tn == 1 && c0 == 'i') {
				cur = off ? cur & ~ST_I : cur | ST_I;
			} else if (tn == 1 && c0 == 'u') {
				cur = off ? cur & ~ST_U : cur | ST_U;
			} else if (c0 == 'a' &&
				   (tn == 1 || (!off && (t[1] == ' ' ||
							 t[1] == '\t')))) {
				if (off) {
					cur &= ~ST_A;
				} else {
					cur |= ST_A;
					for (const char *p = t; p + 5 < end; p++) {
						if (strncasecmp(p, "href=", 5))
							continue;
						p += 5;
						char q = (*p == '"' || *p == '\'')
							 ? *p++ : 0;
						if (!*nlinks) {
							size_t k = 0;
							while (p < end &&
							       k + 1 < hrefcap &&
							       (q ? *p != q
								  : (*p != ' ' &&
								     *p != '\t')))
								href[k++] = *p++;
							href[k] = '\0';
						}
						(*nlinks)++;
						break;
					}
				}
			}
			/* every other tag: stripped */
			r = end + 1;
			continue;
		}
		if (*r == '&') {
			static const struct { const char *e; char c; } ENT[] = {
				{ "&amp;", '&' }, { "&lt;", '<' },
				{ "&gt;", '>' },  { "&quot;", '"' },
			};
			int hit = 0;
			for (size_t k = 0; k < sizeof(ENT) / sizeof(ENT[0]); k++) {
				size_t el = strlen(ENT[k].e);
				if (!strncmp(r, ENT[k].e, el)) {
					out[o] = ENT[k].c;
					style[o++] = (uint8_t)cur;
					r += el;
					hit = 1;
					break;
				}
			}
			if (hit)
				continue;
		}
		out[o] = *r;
		style[o++] = (uint8_t)cur;
		r++;
	}
	out[o] = '\0';
}

/*
 * prompt.c's wrap, returning RANGES into the cleaned body so every byte keeps
 * its style. Cells, not bytes: a continuation byte is not a column.
 */
static int wrap_ranges(const char *s, int w, int starts[BODY_LINES],
		       int lens[BODY_LINES])
{
	int n = 0;
	const char *p = s;

	if (w < 8)
		w = 8;
	while (*p && n < BODY_LINES) {
		int used = 0, last_space = -1;
		const char *start = p;
		while (*p && used < w) {
			if (*p == ' ')
				last_space = (int)(p - start);
			if (((unsigned char)*p & 0xc0) != 0x80)
				used++;
			p++;
		}
		int len = (int)(p - start);
		if (*p && last_space > 0) {
			len = last_space;
			p = start + last_space + 1;
		}
		starts[n] = (int)(start - s);
		lens[n] = len;
		n++;
	}
	return n;
}

/* ── the bus interface ─────────────────────────────────────────────────── */

static int method_notify(sd_bus_message *m, void *userdata, sd_bus_error *err)
{
	const char *app, *icon, *summary, *body;
	uint32_t replaces;
	int32_t timeout;
	int r;

	(void)userdata;
	(void)err;

	r = sd_bus_message_read(m, "susss", &app, &replaces, &icon, &summary,
				&body);
	if (r < 0)
		return r;
	/*
	 * actions arrive as pairs — id, label — flattened into one string
	 * array. The first three pairs become buttons; the rest are read past,
	 * because a toast three rows tall has one row to spend on them.
	 */
	char aid[MAX_ACTS][48], alab[MAX_ACTS][24];
	int nact = 0;
	r = sd_bus_message_enter_container(m, 'a', "s");
	if (r < 0)
		return r;
	for (;;) {
		const char *s1 = NULL, *s2 = NULL;
		r = sd_bus_message_read(m, "s", &s1);
		if (r <= 0)
			break;
		r = sd_bus_message_read(m, "s", &s2);
		if (r <= 0)
			break;	/* an odd count is a malformed client */
		if (nact < MAX_ACTS && s1 && *s1) {
			snprintf(aid[nact], sizeof(aid[0]), "%s", s1);
			snprintf(alab[nact], sizeof(alab[0]), "%s",
				 s2 && *s2 ? s2 : s1);
			sanitize(alab[nact]);
			nact++;
		}
	}
	sd_bus_message_exit_container(m);

	/*
	 * The hints are NOT skipped, because one of them is `urgency` and it is
	 * the only thing in this protocol that says a notification matters.
	 * Skipping the lot left every toast at normal: a critical one — the
	 * battery about to run out, a disk that stopped answering — was drawn
	 * in the same accent as "download finished", and the daemon carried an
	 * `urgent` field that nothing ever set.
	 *
	 * 0 low, 1 normal, 2 critical. Anything else is normal: a hint is a
	 * value from another program and this is not the place to be strict.
	 */
	int urgency = 1;
	r = sd_bus_message_enter_container(m, 'a', "{sv}");
	if (r < 0)
		return r;
	while (sd_bus_message_enter_container(m, 'e', "sv") > 0) {
		const char *key = NULL;
		r = sd_bus_message_read(m, "s", &key);
		if (r < 0)
			return r;
		if (key && !strcmp(key, "urgency") &&
		    sd_bus_message_enter_container(m, 'v', "y") > 0) {
			uint8_t u = 1;
			r = sd_bus_message_read(m, "y", &u);
			if (r < 0)
				return r;
			urgency = u;
			sd_bus_message_exit_container(m);
		} else {
			/* Every other hint, and an `urgency` that is not the
			 * byte the spec says it is. */
			r = sd_bus_message_skip(m, "v");
			if (r < 0)
				return r;
		}
		sd_bus_message_exit_container(m);
	}
	sd_bus_message_exit_container(m);
	r = sd_bus_message_read(m, "i", &timeout);
	if (r < 0)
		return r;

	/*
	 * DND: while $XDG_RUNTIME_DIR/kdos-dnd exists (the panel creates and
	 * removes it), a non-critical notification is answered — an id, and
	 * NotificationClosed so no client waits on it — and never stored.
	 * Nothing is queued for later either: a DND that ambushes on exit is
	 * worse than one that drops. Critical still shows; that is what the
	 * urgency byte is FOR.
	 */
	if (urgency < 2) {
		const char *rt = getenv("XDG_RUNTIME_DIR");
		char dnd[512];
		struct stat st;
		if (rt && *rt) {
			snprintf(dnd, sizeof(dnd), "%s/kdos-dnd", rt);
			if (stat(dnd, &st) == 0) {
				uint32_t id = replaces ? replaces : next_id++;
				emit_closed(id, 1);
				return sd_bus_reply_method_return(m, "u", id);
			}
		}
	}

	struct toast *t = NULL;
	if (replaces) {
		for (int i = 0; i < ntoasts; i++)
			if (toasts[i].id == replaces)
				t = &toasts[i];
	}
	if (!t) {
		/*
		 * Full: drop the OLDEST, not the newest. A stack that refuses
		 * new notifications when it is full shows you stale ones while
		 * hiding the thing that just happened.
		 */
		if (ntoasts == MAX_TOASTS)
			drop_at(0, 1);
		t = &toasts[ntoasts++];
		memset(t, 0, sizeof(*t));
		t->id = replaces ? replaces : next_id++;
	}

	snprintf(t->app, sizeof(t->app), "%s", app ? app : "");
	sanitize(t->app);
	snprintf(t->summary, sizeof(t->summary), "%s", summary ? summary : "");
	sanitize(t->summary);
	/* Sanitize BEFORE the markup pass, so a control byte cannot split a
	 * tag; every field is written unconditionally — a `replaces` reuses
	 * the slot and stale actions on a replaced toast are a wrong click. */
	char raw[256];
	snprintf(raw, sizeof(raw), "%s", body ? body : "");
	sanitize(raw);
	parse_markup(raw, t->body, t->style, sizeof(t->body), t->href,
		     sizeof(t->href), &t->nlinks);
	int st_[BODY_LINES], ln_[BODY_LINES];
	t->body_rows = t->body[0] ? wrap_ranges(t->body, BODY_W, st_, ln_) : 0;
	t->nact = nact;
	for (int i = 0; i < nact; i++) {
		snprintf(t->act_id[i], sizeof(t->act_id[0]), "%s", aid[i]);
		snprintf(t->act_label[i], sizeof(t->act_label[0]), "%s",
			 alab[i]);
	}
	t->urgent = urgency >= 2;
	/*
	 * -1 means "the server decides"; 0 means "never expire", and that is
	 * honoured rather than clamped — a "battery critical" notification is
	 * supposed to stay up.
	 *
	 * And when the server decides, it decides differently for a CRITICAL
	 * one: it stays until it is dismissed, which is what every other
	 * daemon does and what the urgency is for. A five-second toast about a
	 * battery at 3% is a toast that will be missed.
	 */
	t->expires_ms = timeout == 0	 ? 0
			: timeout > 0	 ? now_ms() + timeout
			: t->urgent	 ? 0
					 : now_ms() + 5000;

	/*
	 * DO NOT DISTURB KEEPS IT AND DOES NOT SHOW IT. The id is still
	 * returned and NotificationClosed is still emitted, so the client
	 * cannot tell the difference and nothing hangs waiting; the entry goes
	 * straight to the history, where the badge counts it. An URGENT one is
	 * shown anyway — the urgency level exists to say "this one is not
	 * routine", and a Do Not Disturb that hid a battery-critical warning
	 * would be a switch nobody dares leave on.
	 */
	if (dnd && !t->urgent) {
		uint32_t id = t->id;

		drop_at((int)(t - toasts), 1);
		return sd_bus_reply_method_return(m, "u", id);
	}

	return sd_bus_reply_method_return(m, "u", t->id);
}

static int method_close(sd_bus_message *m, void *userdata, sd_bus_error *err)
{
	uint32_t id;
	int r;
	(void)userdata;
	(void)err;

	r = sd_bus_message_read(m, "u", &id);
	if (r < 0)
		return r;
	for (int i = 0; i < ntoasts; i++)
		if (toasts[i].id == id) {
			drop_at(i, 3);
			break;
		}
	return sd_bus_reply_method_return(m, "");
}

static int method_caps(sd_bus_message *m, void *userdata, sd_bus_error *err)
{
	(void)userdata;
	(void)err;
	/*
	 * `actions` and `body-markup` are honest now: the button row and the
	 * markup scanner exist. Still no `icon-static` — there is nowhere to
	 * put a pixmap in a text row. A capability list that overstates gets
	 * sent things it then draws as literal tags.
	 */
	return sd_bus_reply_method_return(m, "as", 3, "body", "actions",
					  "body-markup");
}

static int method_info(sd_bus_message *m, void *userdata, sd_bus_error *err)
{
	(void)userdata;
	(void)err;
	return sd_bus_reply_method_return(m, "ssss", "kdos-notifyd", "KDOS",
					  "0.1.0", "1.2");
}

static const sd_bus_vtable notify_vtable[] = {
	SD_BUS_VTABLE_START(0),
	SD_BUS_METHOD("Notify", "susssasa{sv}i", "u", method_notify,
		      SD_BUS_VTABLE_UNPRIVILEGED),
	SD_BUS_METHOD("CloseNotification", "u", "", method_close,
		      SD_BUS_VTABLE_UNPRIVILEGED),
	SD_BUS_METHOD("GetCapabilities", "", "as", method_caps,
		      SD_BUS_VTABLE_UNPRIVILEGED),
	SD_BUS_METHOD("GetServerInformation", "", "ssss", method_info,
		      SD_BUS_VTABLE_UNPRIVILEGED),
	SD_BUS_SIGNAL("NotificationClosed", "uu", 0),
	SD_BUS_SIGNAL("ActionInvoked", "us", 0),
	SD_BUS_VTABLE_END
};

/* ── drawing ───────────────────────────────────────────────────────────── */

/* How many rows the toasts currently need. The surface is sized to this and
 * not to MAX_TOASTS: a cell grid has no transparent cell, so every unused row
 * of the surface is painted in the theme background — sized for the maximum,
 * the daemon left an opaque rectangle on the desktop for the whole session
 * with nothing in it. */
/* One height rule for the three walkers below — border, summary, wrapped
 * body, an optional button row, border. */
static int toast_height(const struct toast *t)
{
	return 3 + t->body_rows + (t->nact ? 1 : 0);
}

static int toast_rows(void)
{
	int rows = 0;
	for (int i = 0; i < ntoasts; i++)
		rows += toast_height(&toasts[i]);
	return rows;
}

/* One wrapped body line, as runs of equal style. <b> is the house fill —
 * swapped slots, never KT_A_REVERSE over a label. <i> maps to the body's own
 * dim, which is honest rather than loud: the grid has no second slant. */
static void draw_body_line(const struct toast *t, int start, int len, int x,
			   int y)
{
	int i = start, end = start + len;

	while (i < end && x < ktui_w - 2) {
		uint8_t st = t->style[i];
		int j = i;
		while (j < end && t->style[j] == st)
			j++;
		char run[256];
		int rn = j - i;
		if (rn > (int)sizeof(run) - 1)
			rn = (int)sizeof(run) - 1;
		memcpy(run, t->body + i, (size_t)rn);
		run[rn] = '\0';
		int fg = KT_DIM, bg = KT_SURFACE, attr = KT_A_NONE;
		if (st & ST_A)
			fg = KT_ACCENT;
		if (st & ST_U)
			attr |= KT_A_UNDERLINE;
		if (st & ST_B) {
			fg = KT_SURFACE;
			bg = KT_TEXT;
		}
		x += ktui_draw_text(x, y, ktui_w - 2 - x, run, fg, bg, attr);
		i = j;
	}
}

static void draw_toasts(void)
{
	int w = ktui_w, h = ktui_h;
	if (w < 4 || h < 2)
		return;

	ktui_draw_clear();
	memset(act_hit, 0, sizeof(act_hit));
	int y = 0;
	for (int i = 0; i < ntoasts && y + 3 <= h; i++) {
		const struct toast *t = &toasts[i];
		int rows = toast_height(t);
		/*
		 * CLAMP, do not break. The surface is resized to fit the toasts
		 * and the compositor answers with a configure one round trip
		 * later, so for one frame the grid is the size it was BEFORE the
		 * toast arrived. Breaking there drew the cleared background and
		 * nothing else — an empty box in the corner, which is exactly
		 * what a notification daemon must never produce. Three rows is
		 * border, summary, border; the body and the buttons are what
		 * get dropped.
		 */
		if (y + rows > h)
			rows = h - y;
		if (rows < 3)
			break;
		KRect r = krect(0, y, w, rows);
		int accent = t->urgent ? KT_ERR : KT_ACCENT;
		/*
		 * THE HELD ONE SAYS SO. A countdown that quietly stops is a
		 * countdown nobody can tell has stopped — and the border is
		 * the only part of a toast that is not already carrying
		 * something, so the frame goes to the secondary colour while
		 * the pointer is holding it. Urgent keeps its own colour: a
		 * warning that changed colour under the hand would be saying
		 * something it does not mean.
		 */
		if (i == hover_toast && !t->urgent)
			accent = KT_WARN;

		ktui_draw_fill(r, KT_SURFACE);
		/* The application's own name in the border, where a title goes.
		 * It was captured and never drawn, and "which program is
		 * telling me this" is half of what a toast has to say. */
		ktui_draw_box(r, t->app[0] ? t->app : NULL, accent, KT_SURFACE,
			      1);
		ktui_draw_text(2, y + 1, w - 4, t->summary, KT_TEXT, KT_SURFACE,
			       KT_A_NONE);
		int ry = y + 2;
		if (t->body[0]) {
			int starts[BODY_LINES], lens[BODY_LINES];
			int nl = wrap_ranges(t->body, BODY_W, starts, lens);
			for (int l = 0; l < nl && ry < y + rows - 1; l++, ry++)
				draw_body_line(t, starts[l], lens[l], 2, ry);
		}
		if (t->nact && ry < y + rows - 1) {
			/* The button row, pick.c's pattern: dim brackets, the
			 * label filled with the toast's accent. */
			int x = 2;
			act_hit[i].row = ry;
			for (int a = 0; a < t->nact; a++) {
				char lab[28];
				snprintf(lab, sizeof(lab), " %s ",
					 t->act_label[a]);
				int lw = ktui_utf8_width(lab);
				if (x + lw + 2 > w - 2)
					break;
				ktui_draw_text(x, ry, 1, "[", KT_DIM,
					       KT_SURFACE, KT_A_NONE);
				ktui_draw_text(x + 1, ry, lw, lab, KT_SURFACE,
					       accent, KT_A_NONE);
				ktui_draw_text(x + 1 + lw, ry, 1, "]", KT_DIM,
					       KT_SURFACE, KT_A_NONE);
				act_hit[i].x[a] = x;
				act_hit[i].end[a] = x + lw + 2;
				x = act_hit[i].end[a] + 1;
			}
		}
		y += rows;
	}
	ktui_draw_flush();
}

/*
 * Which toast a row belongs to, or -1. Walks the same accumulation
 * draw_toasts() does rather than dividing by a fixed height — toast heights
 * vary with body and buttons, so a division would dismiss the wrong one as
 * soon as two of different heights were stacked.
 */
static int toast_at_row(int row)
{
	int y = 0;
	for (int i = 0; i < ntoasts; i++) {
		int rows = toast_height(&toasts[i]);
		if (row >= y && row < y + rows)
			return i;
		y += rows;
	}
	return -1;
}

/* Fire-and-forget, argv only — the href came off the bus. Double-forked so
 * this daemon neither reaps nor waits on a browser. */
static void open_href(const char *href)
{
	pid_t pid = fork();

	if (pid == 0) {
		if (fork() == 0) {
			setsid();
			execlp("kdos-appbox", "kdos-appbox", "open", href,
			       (char *)NULL);
			_exit(127);
		}
		_exit(0);
	} else if (pid > 0) {
		waitpid(pid, NULL, 0);
	}
}

/* ── main ──────────────────────────────────────────────────────────────── */

int notifyd_main(int argc, char **argv)
{
	const char *font = NULL;
	for (int i = 1; i < argc; i++) {
		if (!strcmp(argv[i], "--font") && i + 1 < argc)
			font = argv[++i];
		else {
			fprintf(stderr, "usage: kdos-notifyd [--font NAME]\n");
			return 2;
		}
	}

	int r = sd_bus_open_user(&bus);
	if (r < 0) {
		fprintf(stderr, "kdos-notifyd: no session bus: %s\n",
			strerror(-r));
		return 1;
	}
	r = sd_bus_add_object_vtable(bus, NULL, "/org/freedesktop/Notifications",
				     "org.freedesktop.Notifications",
				     notify_vtable, NULL);
	if (r < 0) {
		fprintf(stderr, "kdos-notifyd: cannot export the interface: %s\n",
			strerror(-r));
		return 1;
	}
	/*
	 * No REPLACE_EXISTING: if something already owns the name it is doing
	 * this job, and two daemons answering Notify means every notification
	 * appears twice or once at random.
	 */
	r = sd_bus_request_name(bus, "org.freedesktop.Notifications", 0);
	if (r < 0) {
		fprintf(stderr, "kdos-notifyd: another notification daemon is "
				"already running\n");
		return 1;
	}

	/*
	 * The centre's socket. A failure here is NOT fatal: the toasts are the
	 * job and the history is the extra, and a session with no
	 * XDG_RUNTIME_DIR should still get its notifications drawn.
	 */
	char spath[256];
	int srv = -1;
	if (notify_sock_path(spath, sizeof(spath)) == 0) {
		srv = socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0);
		if (srv >= 0) {
			struct sockaddr_un sa = { .sun_family = AF_UNIX };

			snprintf(sa.sun_path, sizeof(sa.sun_path), "%s", spath);
			unlink(spath);
			if (bind(srv, (struct sockaddr *)&sa, sizeof(sa)) < 0) {
				close(srv);
				srv = -1;
			} else {
				chmod(spath, 0600);
				listen(srv, 4);
			}
		}
	}
	signal(SIGPIPE, SIG_IGN);

	/*
	 * Top right, not centred. An overlay with no anchor is centred, which is
	 * right for the launcher and wrong for a toast — a notification that
	 * covers the middle of the screen covers the thing the user was doing
	 * when it arrived.
	 */
	KwlConfig cfg = {
		.role = KWL_ROLE_OVERLAY,
		.corner = KWL_CORNER_TOP_RIGHT,
		.cols = TOAST_COLS,
		.rows = 3,
		.app_id = "kdos-notifyd",
		.font = font,
	};
	sh_theme_from_cache();
	if (kwl_init(&cfg) != 0) {
		fprintf(stderr, "kdos-notifyd: no compositor or no layer-shell\n");
		return 1;
	}
	ktui_draw_init();

	int wl_fd = kwl_fd();
	int bus_fd = sd_bus_get_fd(bus);

	/* Same live retint as the panel: this daemon outlives an accent
	 * change too, and a toast in the old accent after one is a toast the
	 * desktop has already stopped wearing. */
	sh_theme_watch();

	int shown_rows = -1;
	while (!kwl_should_close()) {
		if (sh_theme_dirty) {
			sh_theme_dirty = 0;
			sh_theme_from_cache();
			ktui_draw_invalidate();
		}
		/*
		 * Drain the bus BEFORE deciding what to draw. This used to sit
		 * after draw_toasts(), so a Notify was read only on the way
		 * INTO poll: the toast joined the list, poll slept until its
		 * expiry, expire_due() removed it at the top of the next pass,
		 * and the toast was never drawn at all — accepted, given an
		 * id, invisible. Traced with WAYLAND_DEBUG: the surface never
		 * saw a single request between hide and expiry.
		 */
		while (sd_bus_process(bus, NULL) > 0)
			;
		expire_due();
		/*
		 * Resize BEFORE drawing, and only when the row count changed:
		 * every request costs a configure round trip, and asking for the
		 * size it already has on every pass of a poll loop is a commit
		 * storm. With no toasts the surface still exists — layer-shell
		 * has no "hide" — so it shrinks to the smallest thing that can
		 * be committed and sits in the corner instead of the middle.
		 */
		/*
		 * Never fewer than four rows while anything is up: that is one
		 * toast WITH a body, so the common case needs no second
		 * configure before it can be drawn in full.
		 */
		int want = toast_rows();
		if (want > 0 && want < 4)
			want = 4;
		if (want != shown_rows) {
			/*
			 * With nothing to show the surface is DESTROYED, not
			 * shrunk: kwl_overlay_hide()/show() replace the old
			 * one-cell workaround, and the show path completes the
			 * initial-commit handshake before returning, so the
			 * first toast after an idle period is drawn on a
			 * surface that exists. The resize lesson still holds:
			 * the configure lands in kwl_pump() below and the cell
			 * buffer follows only through ktui_draw_resize().
			 */
			if (want > 0)
				kwl_overlay_show(TOAST_COLS, want);
			else
				kwl_overlay_hide();
			shown_rows = want;
			/*
			 * show() has already dispatched the configure, so the
			 * resize is pending NOW — apply it before this pass's
			 * draw, or the first toast is painted at the old size.
			 */
			if (ktui_resized) {
				ktui_resized = 0;
				ktui_draw_resize();
				ktui_draw_invalidate();
			}
		}
		draw_toasts();

		struct pollfd fds[3] = {
			{ .fd = wl_fd, .events = POLLIN },
			{ .fd = bus_fd, .events = POLLIN },
			{ .fd = srv, .events = POLLIN },
		};
		int r2 = poll(fds, srv >= 0 ? 3 : 2, next_timeout());
		if (r2 < 0 && errno != EINTR)
			break;
		if (fds[0].revents) {
			/*
			 * CLICK DISMISSES IT. The surface takes no keyboard —
			 * a toast must never steal focus — so the pointer is
			 * the only way a person can make one go away before
			 * its timeout, and a notification that cannot be
			 * dismissed is one people learn to work around by not
			 * looking at that corner of the screen. Reason 2 is
			 * the spec's "dismissed by the user", which is what a
			 * client waiting on NotificationClosed is told.
			 */
			KtuiEvent ev;
			while (ktui_backend()->poll_event(&ev, 0)) {
				if (ev.type != KT_EVT_MOUSE)
					continue;
				/*
				 * HOVER HOLDS THE COUNTDOWN. libkwl reports a
				 * motion only when the pointer changed cell —
				 * and an off-grid one is its LEAVE, which is
				 * what resumes the timer. See hover_set.
				 */
				if (ev.press == KT_MP_DRAG) {
					hover_set(ev.mx < 0 ? -1
							    : toast_at_row(ev.my));
					continue;
				}
				if (ev.press != KT_MP_PRESS)
					continue;
				if (ev.btn != KT_MB_LEFT &&
				    ev.btn != KT_MB_MIDDLE &&
				    ev.btn != KT_MB_RIGHT)
					continue;
				int i = toast_at_row(ev.my);
				if (i < 0)
					continue;
				/*
				 * Left answers what was AIMED at: a button
				 * emits ActionInvoked, a single-link body
				 * opens its link; both then close with the
				 * spec's reason 2. Middle and right always
				 * plain-dismiss — the way out that cannot
				 * accidentally do something.
				 */
				if (ev.btn == KT_MB_LEFT) {
					int a = -1;
					if (toasts[i].nact &&
					    act_hit[i].row == ev.my)
						for (int k = 0; k < toasts[i].nact; k++)
							if (act_hit[i].end[k] > act_hit[i].x[k] &&
							    ev.mx >= act_hit[i].x[k] &&
							    ev.mx < act_hit[i].end[k]) {
								a = k;
								break;
							}
					if (a >= 0) {
						sd_bus_emit_signal(bus,
							"/org/freedesktop/Notifications",
							"org.freedesktop.Notifications",
							"ActionInvoked", "us",
							toasts[i].id,
							toasts[i].act_id[a]);
						drop_at(i, 2);
						continue;
					}
					if (toasts[i].nlinks == 1 &&
					    toasts[i].href[0]) {
						open_href(toasts[i].href);
						drop_at(i, 2);
						continue;
					}
				}
				drop_at(i, 2);
			}
		}
		/*
		 * The configure that answers kwl_overlay_resize() lands here, and
		 * the cell buffer does NOT follow by itself — `ktui_w`/`ktui_h`
		 * come from ktui_draw_resize(), exactly as panel.c and
		 * launcher.c already do it. Without this the surface grew and
		 * draw_toasts() went on believing it had the old three rows, so
		 * a toast WITH A BODY (four rows) failed its `y + rows > h`
		 * guard and the daemon painted an empty box. It only became
		 * reachable when the surface started being resized at all.
		 */
		if (srv >= 0 && (fds[2].revents & POLLIN)) {
			int c = accept(srv, NULL, NULL);

			if (c >= 0) {
				/*
				 * A DEADLINE ON THE READ, because this process
				 * owns org.freedesktop.Notifications for the
				 * whole session: a client that connects and
				 * then says nothing would otherwise stop every
				 * toast on the machine until it went away.
				 * Two hundred milliseconds is forever for a
				 * peer that is about to write one word.
				 */
				struct timeval tv = { .tv_sec = 0,
						      .tv_usec = 200000 };

				setsockopt(c, SOL_SOCKET, SO_RCVTIMEO, &tv,
					   sizeof(tv));
				setsockopt(c, SOL_SOCKET, SO_SNDTIMEO, &tv,
					   sizeof(tv));
				serve_client(c);
				close(c);
			}
		}
		if (ktui_resized) {
			ktui_resized = 0;
			ktui_draw_resize();
			ktui_draw_invalidate();
		}
	}

	if (srv >= 0) {
		close(srv);
		unlink(spath);
	}
	sd_bus_unref(bus);
	kwl_shutdown();
	return 0;
}
