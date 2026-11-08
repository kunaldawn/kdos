/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   kdos-settingsd — what the three interfaces share: the bus and polkit
 * ---------------------------------
 */

#ifndef KDOS_SETTINGSD_BUS_H
#define KDOS_SETTINGSD_BUS_H

#include <stdbool.h>
#include <stdint.h>

/* basu on the target; libsystemd's sd-bus is the same API and is what a
 * development host is likely to have. */
#if __has_include(<basu/sd-bus.h>)
#  include <basu/sd-bus.h>
#else
#  include <systemd/sd-bus.h>
#endif

#include "settingsd.h"

#define KS_HOSTNAME_PATH "/org/freedesktop/hostname1"
#define KS_TIMEDATE_PATH "/org/freedesktop/timedate1"
#define KS_LOCALE_PATH   "/org/freedesktop/locale1"
#define KS_HOSTNAME_IF   "org.freedesktop.hostname1"
#define KS_TIMEDATE_IF   "org.freedesktop.timedate1"
#define KS_LOCALE_IF     "org.freedesktop.locale1"

/*
 * ONE AUTHORISED CALL IN FLIGHT. A method checks its arguments, fills one of
 * these with what it will need, and hands it to ks_auth_run; the polkit
 * answer arrives later, and only then is `apply` called and the caller
 * answered. Everything the method read from its message is copied here,
 * because the message is read once and the copy is what outlives the wait.
 */
typedef struct KsAuth KsAuth;

/* 0: answered with an empty reply by the caller of apply. 1: apply sent its
 * own reply. Negative, or `err` set: answered with that error. */
typedef int (*ks_apply_fn)(KsAuth *a, sd_bus_error *err);

struct KsAuth {
	sd_bus_message *call;
	ks_apply_fn apply;
	const char *action;
	bool interactive;
	uint64_t since;		/* CLOCK_MONOTONIC µs when the call arrived */
	char s[4][KS_XKB_MAX];
	int b[2];
	int64_t x;
	char *text;		/* heap, freed with the KsAuth */
};

KsAuth *ks_auth_new(sd_bus_message *call, ks_apply_fn apply, int interactive);

/*
 * Ask polkit whether the caller may perform `action`, then apply and answer.
 * Root is answered at once without asking: it may write every one of these
 * files by hand, and polkit may be what is down. Returns 1, the "answered
 * later" a method handler returns; the KsAuth is owned from here on.
 */
int ks_auth_run(KsAuth *a, const char *action);

sd_bus *ks_bus(void);
uint64_t ks_now_mono(void);

/* `prog arg…` through the argument-vector builder; its exit status, or -1
 * when it is not installed. Never a shell: the arguments came over a bus. */
int ks_run(const char *prog, const char *a1, const char *a2, const char *a3);

int ks_hostname_add(sd_bus *bus);
int ks_timedate_add(sd_bus *bus);
int ks_locale_add(sd_bus *bus);

/* At start: a kernel with no name gets the one hostname1 would give it. */
void ks_hostname_init(void);

#endif /* KDOS_SETTINGSD_BUS_H */
