/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   kdos-powerd — the firewall's named services, the look of the machine
 *   before a login, and Samba once a folder is shared, for a desktop that is
 *   not root
 *
 * Two faces on one set of verbs: the SO_PEERCRED socket in main.c, which the
 * `kdos` command and `kdos-power` speak, and io.kdos.Power1 on the system bus
 * in bus.c, which System Settings speaks. Both reach the same writers below,
 * so a name means the same thing whichever face asked for it.
 */

#ifndef KDOS_POWERD_H
#define KDOS_POWERD_H

#include <stdbool.h>
#include <stddef.h>
#include <sys/types.h>

#define KP_SOCKET   "/run/kdos-powerd.sock"
#define KP_ADMIN    "wheel"	/* every verb over the socket, with no prompt */
#define KP_MAX      64

#define KP_BUS_NAME  "io.kdos.Power1"
#define KP_BUS_PATH  "/io/kdos/Power1"
#define KP_BUS_IFACE "io.kdos.Power1"

/* The polkit actions, shipped in io.kdos.power1.policy. The socket asks the
 * same two for a caller outside KP_ADMIN, so a rule that widens or narrows
 * one widens or narrows it on both faces. The accent's action also covers the
 * splash motion: both are how the machine looks before anyone logs in. */
#define KP_ACT_FIREWALL "io.kdos.firewall.set"
#define KP_ACT_ACCENT   "io.kdos.accent.set"
/* Starting Samba for the shares users have made: the bus face alone asks it,
 * because the caller is kded-kdos in a session. */
#define KP_ACT_SHARING  "io.kdos.filesharing.start"

/* ── the firewall table (main.c) ───────────────────────────────────────── */

int kp_fw_count(void);
const char *kp_fw_name(int i);
const char *kp_fw_what(int i);
/* The table index of `name`, or -1: a name the table does not carry is not a
 * port anybody may open. */
int kp_fw_find(const char *name);
/* on[kp_fw_count()]: which names the rules file carries now. */
void kp_fw_state(int *on);
/* Rewrite the rules file with `name` on or off and load it. 0 and an
 * `ok …` line in `out`, or -1 and an `err …` line. */
int kp_fw_set(const char *name, bool open, char *out, size_t nout);
/* knocks[kp_fw_count()]: the packets each closed name's counter has counted
 * since the ruleset was last loaded; -1 for an open name and for one whose
 * counter the kernel does not hold. 0, or -1 when the counters could not be
 * read at all. */
int kp_fw_knocks(long long *knocks);

/* ── file sharing (main.c) ─────────────────────────────────────────────── */

/* Run Samba's service script with `start`, which serves the usershares and
 * any share smb.conf defines, or starts nothing when there are none. 0 and
 * `ok`, or -1 and `err …` when samba is disabled, missing or would not
 * start. */
int kp_share_start(char *out, size_t nout);

/* ── the accent and the splash motion (main.c) ─────────────────────────── */

/* 0 and `ok <scheme>[ (<what> unchanged)]`, `<what>` naming the boot menu,
 * the login screen or both where that part could not be written; -1 and
 * `err …` for a name that is not a scheme. */
int kp_accent_set(const char *name, char *out, size_t nout);
/* The scheme /etc/kdos/accent names, or the palette's default when it names
 * none this build knows. */
const char *kp_accent_get(void);
/* `phosphor` or `modern`, kdos-splash's motion from the next boot: 0 and
 * `ok <motion>[ (boot menu unchanged)]`, or -1 and `err …` for any other
 * word. */
bool kp_splash_ok(const char *motion);
int kp_splash_set(const char *motion, char *out, size_t nout);

/* ── the bus face (bus.c) ──────────────────────────────────────────────── */

/* Connect, export io.kdos.Power1 and own its name. 0 on success; -1 leaves
 * the daemon serving the socket alone, and kp_bus_timeout_ms() counts down
 * to the main loop's next try. */
int kp_bus_open(void);
void kp_bus_close(void);
/* The fd and events to poll while connected; -1 when not. */
int kp_bus_fd(void);
short kp_bus_events(void);
/* Milliseconds until the bus needs attention or the next reconnect is due;
 * -1 for neither. */
int kp_bus_timeout_ms(void);
/* Process everything pending. A lost connection is closed here and a
 * reconnect scheduled; the socket face keeps answering meanwhile. */
void kp_bus_dispatch(void);
/* Reconnect when one is due. */
void kp_bus_tick(void);

/* Ask polkit, without a prompt, whether the process `pid` (uid `uid`, both
 * from SO_PEERCRED) may perform `action`. 1 yes; 0 for no, for a prompt that
 * nobody could answer, and for a polkit that cannot be reached. */
int kp_bus_peer_may(pid_t pid, uid_t uid, const char *action);

/* Tell the bus a setting changed, whichever face changed it. */
void kp_bus_fw_changed(void);
void kp_bus_accent_changed(void);

#endif
