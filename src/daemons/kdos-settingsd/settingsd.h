/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   kdos-settingsd — what the files under /etc say, and the bus faces on them
 *
 * Two halves with one line between them. store.c is the files: where each
 * setting is kept, what a valid value is, and the console-to-XKB tables. It
 * includes no sd-bus, so testing/fixtures/settingsd/unit.c links it on a host
 * that has no bus library at all. hostname.c, timedate.c and locale.c are the
 * three interfaces; main.c is the bus, the loop and the polkit question.
 * ---------------------------------
 */

#ifndef KDOS_SETTINGSD_H
#define KDOS_SETTINGSD_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* ── where the tree is ────────────────────────────────────────────────────
 *
 * `/etc`, `/usr/share` and `/sys`, each replaceable by a variable so the
 * fixture can drive every method against a temporary tree and its own
 * firmware. A replaced `/etc` is also the one switch that keeps the daemon
 * off the machine it runs on: with it set, the kernel hostname, the clock
 * and the RTC are left alone (see ks_test_tree). No variable grants
 * anything — whoever sets the environment of a root daemon is root already.
 */
const char *ks_etc(void);	/* $KDOS_SETTINGSD_ETC, or /etc */
const char *ks_share(void);	/* $KDOS_SETTINGSD_SHARE, or /usr/share */
const char *ks_sys(void);	/* $KDOS_SETTINGSD_SYS, or /sys */
bool ks_test_tree(void);	/* $KDOS_SETTINGSD_ETC is set */

/* `<ks_etc()>/<rel>` and the others into out; false on overflow. */
bool ks_etc_path(char *out, size_t n, const char *rel);
bool ks_share_path(char *out, size_t n, const char *rel);
bool ks_sys_path(char *out, size_t n, const char *rel);

/* ── environment-style files ──────────────────────────────────────────────
 *
 * `KEY=value` lines: /etc/machine-info, /etc/os-release, /etc/kdos/xkb.conf.
 * The whole file is held as lines, so a rewrite changes the keys it names and
 * keeps every comment and every key it does not — the shipped xkb.conf is
 * mostly the explanation of its four keys, and a rewrite that dropped it
 * would leave a file nobody can read.
 */
typedef struct {
	char **line;
	int n, cap;
} KsEnv;

/* 0, with an empty set for a file that does not exist; -1 on a read error. */
int ks_env_load(KsEnv *e, const char *path);
void ks_env_free(KsEnv *e);
/* The value unquoted into out. Returns true when the key is assigned. */
bool ks_env_get(const KsEnv *e, const char *key, char *out, size_t n);
/* Replace the key's line, or append one. NULL removes every line naming it;
 * "" keeps the key with an empty value. */
void ks_env_set(KsEnv *e, const char *key, const char *value);
/* Atomic. A set with no lines at all removes the file instead. */
int ks_env_save(const KsEnv *e, const char *path);
/* One key from one file; "" when absent. */
void ks_env_read1(const char *path, const char *key, char *out, size_t n);

/* ── values ───────────────────────────────────────────────────────────────── */

#define KS_HOST_MAX 64		/* HOST_NAME_MAX: what sethostname(2) takes */
#define KS_TEXT_MAX 256		/* a pretty hostname, a location */
#define KS_NAME_MAX 128		/* a zone, a keymap, a locale value */
#define KS_XKB_MAX 256		/* one XKB field, commas included */

bool ks_hostname_ok(const char *s);
bool ks_text_ok(const char *s);		/* UTF-8, no control character */
bool ks_icon_ok(const char *s);
bool ks_chassis_ok(const char *s);
bool ks_deployment_ok(const char *s);

/* ── hostname ─────────────────────────────────────────────────────────────── */

void ks_static_hostname(char *out, size_t n);	/* "" when unset or invalid */
int ks_set_static_hostname(const char *name);	/* "" removes /etc/hostname */
/* /etc/hosts' `127.0.1.1` line, naming `name`; written only when it differs. */
int ks_set_hosts_name(const char *name);
void ks_default_hostname(char *out, size_t n);
/* One key of os-release, "" when absent. */
void ks_os_release(const char *key, char *out, size_t n);
/* The chassis machine-info names, else "vm" in a virtual machine, else the
 * one the firmware reports (DMI, ACPI, the device tree); "" for none. */
void ks_chassis(char *out, size_t n);

/* ── the firmware and the ids ─────────────────────────────────────────────── */

/* One file of <sys>/class/dmi/id, trimmed; "" when absent, unreadable, not
 * text, or a vendor's placeholder for "unset". */
void ks_dmi(const char *file, char *out, size_t n);
/* `MM/DD/YYYY` or `YYYY-MM-DD` to µs at midnight UTC; false for no date. */
bool ks_parse_date(const char *s, uint64_t *usec);
/* /etc/machine-id's 16 bytes; false when it holds no id. */
bool ks_machine_id(uint8_t out[16]);
/* A dashed UUID's 16 bytes; false when `s` is not one. */
bool ks_uuid_parse(const char *s, uint8_t out[16]);

/* ── time zone ─────────────────────────────────────────────────────────────
 *
 * The name `/etc/localtime` points at: "UTC" when the link does not exist,
 * "" when it is not a link into the zone tree.
 */
void ks_timezone(char *out, size_t n);
/* 0 when `zone` names a zone file; -1 for a name that is not a zone name,
 * -2 for one that names no zone file. */
int ks_zone_check(const char *zone);
/* The link, the profile and the regulatory domain. `cc` receives the zone's
 * country ("" for none) for the caller to hand to `iw`. */
int ks_set_timezone(const char *zone, char cc[3]);
/* Every zone name tzdata.zi carries, links included, and UTC; sorted. */
char **ks_list_timezones(int *count);

/* ── network time ──────────────────────────────────────────────────────────── */

bool ks_ntp_available(void);	/* a chrony service script and chronyd */
bool ks_ntp_enabled(void);	/* available, and not marked disabled */
int ks_set_ntp_marker(bool enabled);

/* ── locale ─────────────────────────────────────────────────────────────── */

#define KS_LOCALE_KEYS 14
#define KS_ASSIGN_MAX (KS_NAME_MAX + 32)	/* `LC_IDENTIFICATION=<value>` */

/* `NAME=value` with NAME one of the variables locale1 carries. `key` gets
 * the index of NAME in the canonical order; `empty` is set for `NAME=`. */
bool ks_locale_assign_ok(const char *a, int *key, bool *empty);
/* The assignments /etc/locale.conf holds, canonical order. */
int ks_locale_get(char out[KS_LOCALE_KEYS][KS_ASSIGN_MAX]);
/* Values indexed by key; "" leaves the variable out. Every key empty removes
 * the file. */
int ks_locale_set(char vals[KS_LOCALE_KEYS][KS_NAME_MAX]);
const char *ks_locale_key(int i);

/* ── keyboards ─────────────────────────────────────────────────────────────── */

typedef struct {
	char model[KS_XKB_MAX];
	char layout[KS_XKB_MAX];
	char variant[KS_XKB_MAX];
	char options[KS_XKB_MAX];
} KsXkb;

bool ks_keymap_name_ok(const char *s);
bool ks_keymap_installed(const char *name);
void ks_keymap(char *out, size_t n);		/* /etc/keymap, "" when unset */
int ks_set_keymap(const char *name);		/* "" removes /etc/keymap */

/* What /etc/kdos/xkb.conf holds, as written. */
void ks_xkb_stored(KsXkb *x);
/* What a compositor should use: the stored values, with an empty layout
 * filled in from the console keymap through the conversion table. */
void ks_xkb_effective(KsXkb *x);
int ks_set_xkb(const KsXkb *x);
/* NULL when the four fields are acceptable, else why not. Checked against
 * xkeyboard-config's rules list where it is installed. */
const char *ks_xkb_check(const KsXkb *x);

/* Console keymap name to XKB layout and variant. False when the table and
 * the two-letter rule give nothing xkeyboard-config carries. */
bool ks_vt_to_xkb(const char *keymap, char *layout, size_t ln, char *variant,
		  size_t vn);
/* The first XKB layout and variant to an installed console keymap. */
bool ks_xkb_to_vt(const char *layout, const char *variant, char *keymap,
		  size_t kn);

#endif /* KDOS_SETTINGSD_H */
