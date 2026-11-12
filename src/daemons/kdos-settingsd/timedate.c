/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   kdos-settingsd — org.freedesktop.timedate1
 *
 * THE CLOCK, THE ZONE AND WHETHER CHRONY KEEPS THE CLOCK. The Date & Time
 * page calls SetNTP, then SetTime when NTP is off, then SetTimezone, in that
 * order and each after the one before has answered: SetTime is refused while
 * NTP is on, so SetNTP must have taken effect, marker and all, before it
 * replies.
 *
 * THE RTC IS UTC, ALWAYS. The kernel reads it as UTC at boot (RTC_HCTOSYS)
 * and writes the system clock back into it every eleven minutes while chrony
 * has the clock synchronised (RTC_SYSTOHC, and chrony.conf's `rtcsync`), so a
 * local-time RTC would be rewritten in UTC within minutes and the next boot
 * would start hours off. LocalRTC is therefore false and SetLocalRTC refuses
 * true. A clock set by hand is not synchronised, so the kernel never writes
 * it back; SetTime writes the RTC itself, or the time set would last only
 * until the next boot.
 * ---------------------------------
 */

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <errno.h>
#include <fcntl.h>
#include <linux/rtc.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/timex.h>
#include <time.h>
#include <unistd.h>

#include "kbase.h"
#include "bus.h"

#define A_TIME	    "org.freedesktop.timedate1.set-time"
#define A_TIMEZONE  "org.freedesktop.timedate1.set-timezone"
#define A_LOCAL_RTC "org.freedesktop.timedate1.set-local-rtc"
#define A_NTP	    "org.freedesktop.timedate1.set-ntp"

#define E_NTP_ON    "org.freedesktop.timedate1.AutomaticTimeSyncEnabled"
#define E_NO_NTP    "org.freedesktop.timedate1.NoNTPSupport"

#define USEC 1000000LL

/* ── the RTC ─────────────────────────────────────────────────────────────── */

static int rtc_open(void)
{
	int fd = open("/dev/rtc0", O_RDONLY | O_CLOEXEC);

	return fd >= 0 ? fd : open("/dev/rtc", O_RDONLY | O_CLOEXEC);
}

static bool rtc_read(time_t *out)
{
	struct rtc_time rt;
	struct tm tm = { 0 };
	int fd = rtc_open(), r;

	if (fd < 0)
		return false;
	r = ioctl(fd, RTC_RD_TIME, &rt);
	close(fd);
	if (r < 0)
		return false;
	tm.tm_sec = rt.tm_sec;
	tm.tm_min = rt.tm_min;
	tm.tm_hour = rt.tm_hour;
	tm.tm_mday = rt.tm_mday;
	tm.tm_mon = rt.tm_mon;
	tm.tm_year = rt.tm_year;
	*out = timegm(&tm);
	return *out != (time_t)-1;
}

static void rtc_write(time_t t)
{
	struct rtc_time rt = { 0 };
	struct tm tm;
	int fd;

	if (!gmtime_r(&t, &tm) || (fd = rtc_open()) < 0)
		return;
	rt.tm_sec = tm.tm_sec;
	rt.tm_min = tm.tm_min;
	rt.tm_hour = tm.tm_hour;
	rt.tm_mday = tm.tm_mday;
	rt.tm_mon = tm.tm_mon;
	rt.tm_year = tm.tm_year;
	rt.tm_wday = tm.tm_wday;
	rt.tm_yday = tm.tm_yday;
	/* Best effort: a machine with no RTC keeps the new time until the
	 * next boot, which is all it ever had. */
	if (ioctl(fd, RTC_SET_TIME, &rt) < 0)
		fprintf(stderr, "kdos-settingsd: RTC not written: %s\n",
			strerror(errno));
	close(fd);
}

static int64_t realtime_usec(void)
{
	struct timespec ts;

	clock_gettime(CLOCK_REALTIME, &ts);
	return (int64_t)ts.tv_sec * USEC + ts.tv_nsec / 1000;
}

/* A test tree never moves the clock of the machine it runs on; the call is
 * still checked and authorised all the way here, which is what the fixture
 * asserts. */
static int set_clock(int64_t usec, sd_bus_error *err)
{
	struct timespec ts;

	if (ks_test_tree())
		return 0;
	ts.tv_sec = (time_t)(usec / USEC);
	ts.tv_nsec = (long)(usec % USEC) * 1000;
	if (clock_settime(CLOCK_REALTIME, &ts) != 0)
		return sd_bus_error_setf(err, SD_BUS_ERROR_FAILED,
					 "cannot set the clock: %s",
					 strerror(errno));
	rtc_write(ts.tv_sec);
	return 0;
}

/* ── properties ──────────────────────────────────────────────────────────── */

/* Synchronised is what systemd-timedated reports: the kernel's maximum error
 * under sixteen seconds. STA_UNSYNC is not the test, because chrony sets it
 * on purpose to keep the kernel's hands off the RTC in some setups. */
static bool ntp_synced(void)
{
	struct timex tx = { 0 };

	return adjtimex(&tx) >= 0 && tx.maxerror < 16000000;
}

static int get_prop(sd_bus *bus, const char *path, const char *iface,
		    const char *prop, sd_bus_message *reply, void *userdata,
		    sd_bus_error *err)
{
	char z[KS_NAME_MAX];
	time_t t;

	(void)bus, (void)path, (void)iface, (void)userdata, (void)err;
	if (!strcmp(prop, "Timezone")) {
		ks_timezone(z, sizeof(z));
		return sd_bus_message_append(reply, "s", z);
	}
	if (!strcmp(prop, "LocalRTC"))
		return sd_bus_message_append(reply, "b", 0);
	if (!strcmp(prop, "CanNTP"))
		return sd_bus_message_append(reply, "b", ks_ntp_available());
	if (!strcmp(prop, "NTP"))
		return sd_bus_message_append(reply, "b", ks_ntp_enabled());
	if (!strcmp(prop, "NTPSynchronized"))
		return sd_bus_message_append(reply, "b", ntp_synced());
	if (!strcmp(prop, "TimeUSec"))
		return sd_bus_message_append(reply, "t",
					     (uint64_t)realtime_usec());
	/* An RTC that cannot be read is time zero rather than an error: one
	 * failing getter fails GetAll for every property with it. */
	return sd_bus_message_append(reply, "t", rtc_read(&t) && t > 0
				     ? (uint64_t)t * (uint64_t)USEC : 0);
}

/* ── methods ─────────────────────────────────────────────────────────────── */

static int apply_timezone(KsAuth *a, sd_bus_error *err)
{
	char cc[3];

	if (ks_set_timezone(a->s[0], cc) != 0)
		return sd_bus_error_setf(err, SD_BUS_ERROR_FAILED,
					 "cannot write /etc/localtime: %s",
					 strerror(errno));
	/* Now as well as at the next boot, which reads modprobe.d. A zone
	 * with no country returns the radio to the world domain, as the
	 * removed modprobe.d file does at the next boot; skipping it would
	 * leave the previous country's channels and power in force. */
	ks_run("iw", "reg", "set", cc[0] ? cc : "00");
	sd_bus_emit_properties_changed(ks_bus(), KS_TIMEDATE_PATH,
				       KS_TIMEDATE_IF, "Timezone", NULL);
	return 0;
}

static int m_set_timezone(sd_bus_message *m, void *u, sd_bus_error *err)
{
	char cur[KS_NAME_MAX];
	const char *zone;
	int inter, r;
	KsAuth *a;

	(void)u;
	r = sd_bus_message_read(m, "sb", &zone, &inter);
	if (r < 0)
		return r;
	r = ks_zone_check(zone);
	if (r == -1)
		return sd_bus_error_setf(err, SD_BUS_ERROR_INVALID_ARGS,
					 "not a time zone name");
	if (r < 0)
		return sd_bus_error_setf(err, SD_BUS_ERROR_INVALID_ARGS,
					 "no such time zone");
	ks_timezone(cur, sizeof(cur));
	if (!strcmp(cur, zone))
		return sd_bus_reply_method_return(m, NULL);
	a = ks_auth_new(m, apply_timezone, inter);
	kb_strlcpy(a->s[0], zone, sizeof(a->s[0]));
	return ks_auth_run(a, A_TIMEZONE);
}

static int apply_ntp(KsAuth *a, sd_bus_error *err)
{
	bool on = a->b[0];
	int r;

	/* The marker first: it is what the next boot obeys, and it is what
	 * the NTP property reads, so SetTime sees the new state the moment
	 * this call has answered. `service` is waited for, the loop with it:
	 * chronyd still running when SetTime moves the clock would step it
	 * straight back, so the answer comes only once it has stopped, within
	 * the supervisor's grace. */
	if (ks_set_ntp_marker(on) != 0)
		return sd_bus_error_setf(err, SD_BUS_ERROR_FAILED,
					 "cannot write the chrony marker: %s",
					 strerror(errno));
	r = ks_run("service", on ? "start" : "stop", "chrony", NULL);
	sd_bus_emit_properties_changed(ks_bus(), KS_TIMEDATE_PATH,
				       KS_TIMEDATE_IF, "NTP", NULL);
	if (r != 0)
		return sd_bus_error_setf(err, SD_BUS_ERROR_FAILED,
					 "chrony is %s at the next boot, but "
					 "`service %s chrony` failed (%d)",
					 on ? "enabled" : "disabled",
					 on ? "start" : "stop", r);
	return 0;
}

static int m_set_ntp(sd_bus_message *m, void *u, sd_bus_error *err)
{
	int on, inter, r;
	KsAuth *a;

	(void)u;
	r = sd_bus_message_read(m, "bb", &on, &inter);
	if (r < 0)
		return r;
	/* Off is already the state of a machine with no chrony, so only on
	 * is refused there, as systemd-timedated refuses it. The Date & Time
	 * page sends SetNTP(false) on every Apply when CanNTP is false, and an
	 * error here would fail the whole Apply, the zone with it. */
	if (!on == !ks_ntp_enabled())
		return sd_bus_reply_method_return(m, NULL);
	if (!ks_ntp_available())
		return sd_bus_error_setf(err, E_NO_NTP,
					 "chrony is not installed");
	a = ks_auth_new(m, apply_ntp, inter);
	a->b[0] = !!on;
	return ks_auth_run(a, A_NTP);
}

/* The time asked for, moved on by however long polkit took to answer, so a
 * password typed slowly does not set the clock back by the time it took. */
static int apply_time(KsAuth *a, sd_bus_error *err)
{
	return set_clock(a->x + (int64_t)(ks_now_mono() - a->since), err);
}

static int m_set_time(sd_bus_message *m, void *u, sd_bus_error *err)
{
	int64_t usec, now, target;
	int rel, inter, r;
	KsAuth *a;

	(void)u;
	r = sd_bus_message_read(m, "xbb", &usec, &rel, &inter);
	if (r < 0)
		return r;
	if (ks_ntp_enabled())
		return sd_bus_error_setf(err, E_NTP_ON,
					 "automatic time synchronisation is on");
	/* Moving the clock by nothing is done, and needs nobody's password. */
	if (rel && !usec)
		return sd_bus_reply_method_return(m, NULL);
	now = realtime_usec();
	if (rel) {
		if (usec == INT64_MIN || (usec > 0 && now > INT64_MAX - usec) ||
		    (usec < 0 && now <= -usec))
			return sd_bus_error_setf(err, SD_BUS_ERROR_INVALID_ARGS,
						 "time value overflow");
		target = now + usec;
	} else {
		if (usec <= 0)
			return sd_bus_error_setf(err, SD_BUS_ERROR_INVALID_ARGS,
						 "invalid absolute time");
		target = usec;
	}
	a = ks_auth_new(m, apply_time, inter);
	a->x = target;
	return ks_auth_run(a, A_TIME);
}

/* fix_system: the RTC is believed and the clock set from it, which is what an
 * installer or a live medium wants. In a test tree the clock is not moved. */
static int apply_fix_system(KsAuth *a, sd_bus_error *err)
{
	time_t t;

	(void)a;
	if (ks_test_tree())
		return 0;
	if (!rtc_read(&t))
		return sd_bus_error_setf(err, SD_BUS_ERROR_FAILED,
					 "cannot read the RTC");
	return set_clock((int64_t)t * USEC, err);
}

static int m_set_local_rtc(sd_bus_message *m, void *u, sd_bus_error *err)
{
	int local, fix, inter, r;

	(void)u;
	r = sd_bus_message_read(m, "bbb", &local, &fix, &inter);
	if (r < 0)
		return r;
	if (local)
		return sd_bus_error_setf(err, SD_BUS_ERROR_NOT_SUPPORTED,
			"the RTC is kept in UTC: the kernel writes it back in "
			"UTC while the clock is synchronised");
	if (!fix)
		return sd_bus_reply_method_return(m, NULL);
	return ks_auth_run(ks_auth_new(m, apply_fix_system, inter),
			   A_LOCAL_RTC);
}

static int m_list_timezones(sd_bus_message *m, void *u, sd_bus_error *err)
{
	sd_bus_message *reply = NULL;
	char **v;
	int n, r;

	(void)u, (void)err;
	v = ks_list_timezones(&n);
	r = sd_bus_message_new_method_return(m, &reply);
	if (r >= 0)
		r = sd_bus_message_append_strv(reply, v);
	if (r >= 0)
		r = sd_bus_send(NULL, reply, NULL);
	sd_bus_message_unref(reply);
	kb_strv_free(v);
	return r;
}

#define M(name, sig, out, fn) \
	SD_BUS_METHOD(name, sig, out, fn, SD_BUS_VTABLE_UNPRIVILEGED)
#define CHG SD_BUS_VTABLE_PROPERTY_EMITS_CHANGE

static const sd_bus_vtable timedate_vtable[] = {
	SD_BUS_VTABLE_START(0),
	SD_BUS_PROPERTY("Timezone", "s", get_prop, 0, CHG),
	SD_BUS_PROPERTY("LocalRTC", "b", get_prop, 0, CHG),
	SD_BUS_PROPERTY("CanNTP", "b", get_prop, 0, 0),
	SD_BUS_PROPERTY("NTP", "b", get_prop, 0, CHG),
	SD_BUS_PROPERTY("NTPSynchronized", "b", get_prop, 0, 0),
	SD_BUS_PROPERTY("TimeUSec", "t", get_prop, 0, 0),
	SD_BUS_PROPERTY("RTCTimeUSec", "t", get_prop, 0, 0),
	M("SetTime", "xbb", NULL, m_set_time),
	M("SetTimezone", "sb", NULL, m_set_timezone),
	M("SetLocalRTC", "bbb", NULL, m_set_local_rtc),
	M("SetNTP", "bb", NULL, m_set_ntp),
	M("ListTimezones", "", "as", m_list_timezones),
	SD_BUS_VTABLE_END
};

int ks_timedate_add(sd_bus *bus)
{
	return sd_bus_add_object_vtable(bus, NULL, KS_TIMEDATE_PATH,
					KS_TIMEDATE_IF, timedate_vtable, NULL);
}
