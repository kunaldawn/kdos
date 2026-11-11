/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   kdos-sysupdated — the pieces, and the one state they share
 * ---------------------------------
 */

#ifndef SYSUPDATED_H
#define SYSUPDATED_H

#include <poll.h>
#include <stddef.h>
#include <stdint.h>
#include <sys/types.h>
#include <time.h>

/* basu on the target; libsystemd's sd-bus is the same API and is what a
 * developer host is likely to have. */
#if __has_include(<basu/sd-bus.h>)
#  include <basu/sd-bus.h>
#else
#  include <systemd/sd-bus.h>
#endif

#include "kbase.h"

#define SU_BUS       "org.freedesktop.sysupdate1"
#define SU_PATH      "/org/freedesktop/sysupdate1"
#define SU_MANAGER   "org.freedesktop.sysupdate1.Manager"
#define SU_TARGET    "org.freedesktop.sysupdate1.Target"
#define SU_JOB       "org.freedesktop.sysupdate1.Job"
#define SU_HOST_PATH "/org/freedesktop/sysupdate1/target/host"
#define SU_JOB_PATH  "/org/freedesktop/sysupdate1/job"

/* io.kdos.Slots1 is the System Versions page's contract,
 * src/desktop/kcm-kdos-system/io.kdos.Slots1.xml: its members, its keys and
 * its error names are that file's. */
#define SL_BUS       "io.kdos.Slots1"
#define SL_PATH      "/io/kdos/Slots1"
#define SL_IFACE     "io.kdos.Slots1"
#define SL_REFUSED   "io.kdos.Slots1.Error.Refused"

/* Errors of our own, under our own name: a client tells a refusal from a
 * failure by the name, and shows the message. */
#define ERR_BUSY        "io.kdos.Sysupdate.Error.Busy"
#define ERR_NO_UPDATE   "io.kdos.Sysupdate.Error.NoUpdate"
#define ERR_NO_VERSION  "io.kdos.Sysupdate.Error.NoSuchVersion"
#define ERR_NO_SLOTS    "io.kdos.Sysupdate.Error.NoSlots"
#define ERR_REFUSED     "io.kdos.Sysupdate.Error.Refused"
#define ERR_NOT_ACQ     "io.kdos.Sysupdate.Error.NotAcquired"
#define ERR_FAILED      "io.kdos.Sysupdate.Error.Failed"

/* The polkit actions. The sysupdate1 ids are systemd's own, so a rule written
 * for systemd-sysupdated names the same action here; their defaults are the
 * policy file's, which are stricter than systemd's. */
#define PK_CHECK      "org.freedesktop.sysupdate1.check"
#define PK_UPDATE     "org.freedesktop.sysupdate1.update"
#define PK_UPDATE_VER "org.freedesktop.sysupdate1.update-to-version"
#define PK_TRY        "io.kdos.slots.try"
#define PK_CHANNEL    "io.kdos.slots.channel"
#define PK_BLESS      "io.kdos.slots.bless"

/* What the job running now is doing. */
enum { JOB_NONE, JOB_ACQUIRE, JOB_INSTALL };

/* One sysupdate1 job: an object on the bus for as long as its child runs. */
typedef struct {
	int kind;			/* JOB_ACQUIRE or JOB_INSTALL */
	unsigned long long id;
	char path[96];
	unsigned progress;		/* 0-100 */
	unsigned done, total;
	char owner[256];		/* the unique name that started it */
	sd_bus_slot *slot;		/* its object */
} Job;

/* A child process: `kdos update`, run with a progress descriptor. */
typedef struct Child Child;
struct Child {
	pid_t pid;
	int fd;				/* the progress pipe, -1 at EOF */
	char line[2048];
	size_t len;
	int exited, status;		/* status: the exit code, or 128+sig */
	int kind;			/* JOB_ACQUIRE, JOB_INSTALL or CH_CHECK */
	sd_bus_message *call;		/* the call waiting for its reply */
	char version[64];		/* the version the call named */
	char error[512];		/* the last `refused`/`error` line */
	int replied;
	Child *next;
};

#define CH_CHECK 10

/* Everything the daemon knows, refreshed from the files that say it. */
typedef struct {
	/* configuration */
	int session;			/* --session: the session bus, a fixture */
	char root[512];			/* --root: every path is under it */
	char proc[512];			/* --proc: the process table */
	int bless_after;		/* seconds the greeter must stay up */
	int tick;			/* seconds between polls */
	int refresh_after;		/* seconds before CheckNew refetches */

	sd_bus *bus;
	int polkit;			/* polkit answers on this bus */

	/* the update document */
	int have_doc;			/* /var/lib/kdos/update.json was read */
	char offer[64];			/* "" when nothing is offered */
	char lane[16];			/* "image", "binhost" or "" */
	char installed[64];
	char tree[64];
	char date[16];
	char problem[32];
	char detail[256];
	char signer_key[32];
	char signer_name[64];
	long long verified;		/* when that key's signature was checked */
	char channel[512];
	char *notes;
	char **pkgs;			/* names of what is behind */
	int npkgs;
	int behind;
	long long size;

	/* the boot state, from `kdos-bootctl status --json` */
	int configured;
	int live;			/* the live medium: no state, no slots */
	char active[2], running[2], trying[2], failed[2];
	unsigned attempts;
	int bootnext;
	char uuid[2][96], crypt[2][96];
	char other_version[64];		/* the inactive slot's version */
	char *slots_json;		/* the whole `slots` object */

	/* the job */
	Job *job;
	unsigned long long next_id;
	char state[16];			/* io.kdos.Slots1.State */
	char version[64];		/* the version the job or trial is */
	char error[512];

	/* blessing */
	int blessed;
	time_t greeter_since;
	pid_t greeter_pid, greeter_kwin;
	int user_session_seen;
	time_t markgood_tried;

	Child *children;
	Child *refresh;			/* the update check, while it runs */
	int refresh_fetch;		/* ... and it asks the network */
	int refetch;			/* a CheckNew wants a fetch after it */
	sd_bus_message **waiters;	/* CheckNew calls waiting on it */
	int nwaiters;
} Su;

extern Su su;

/* main.c */
void su_log(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
void su_path(const char *rel, char *out, size_t cap);

/* state.c */
void state_reload_doc(void);
void state_reload_boot(void);
void state_recompute(void);
void state_version(char *out, size_t cap);
long long state_fetch_time(void);
int state_acquired(char *slot, size_t scap, char *ver, size_t vcap);
int state_slots_ok(void);
int state_live(void);
int state_doc_current(void);
int state_on_trial(void);
const char *state_current_offer(void);
void state_emit(void);
void state_channel(char *out, size_t cap);
int state_set_channel(const char *channel);
int state_append_state(sd_bus_message *m);

/* appstream.c */
int appstream_write(void);
void appstream_url(char *out, size_t cap);

/* json.c — a reader for the one-line documents `kdos update` and
 * `kdos-bootctl` write. Each value is a pointer into the document. */
const char *json_ws(const char *p);
const char *json_skip(const char *p);
const char *json_get(const char *obj, const char *key);
int json_string(const char *v, char *out, size_t cap);
int json_is_null(const char *v);
long long json_number(const char *v);
int json_true(const char *v);
const char *json_first(const char *container);
const char *json_next(const char *value);

/* polkit.c */
typedef void (*AuthDone)(sd_bus_message *m, void *ud, int allowed,
			 const char *err_name, const char *err_msg);
int auth_check(sd_bus_message *m, const char *action, int interactive,
	       AuthDone done, void *ud);

/* job.c */
Child *child_spawn(const char *const *argv, int progress, int kind);
void child_poll_fds(struct pollfd *pfd, int *n, int cap);
void child_on_readable(int fd);
void child_reap(void);
void child_cancel(void);
int run_sync(const char *const *argv, KbBuf *out, KbBuf *err);

/* bus.c */
int bus_setup(void);
int bus_refresh(int fetch);
void bus_job_progress(void);
void bus_job_removed(Job *j, int status);
Job *bus_job_new(int kind, const char *owner);
void bus_job_free(Job *j);
void bus_child_line(Child *c, const char *line);
void bus_child_done(Child *c);

/* bless.c */
void bless_setup(void);
void bless_tick(void);
void bless_evaluate(void);
int bless_graphical(void);
int bless_has(const char *mark);
int bless_inotify_fd(void);
void bless_on_inotify(void);

#endif
