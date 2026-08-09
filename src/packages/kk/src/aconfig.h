/*
 * KK — the KDOS demo.  Forked from BB (C) 1997 AA-group; see LICENSE.notice.
 *
 * Upstream generated this file with autoconf, probing for headers that every
 * Linux libc has carried for twenty years. KK targets exactly one platform —
 * KDOS: musl, Linux, x86_64 — so the answers are written down instead of
 * discovered, and the whole autotools layer is gone with them.
 */

#ifndef ACONFIG_H
#define ACONFIG_H

#define PACKAGE "kk"
#define VERSION "1.0"

#define STDC_HEADERS	 1
#define RETSIGTYPE	 void

#define HAVE_ALLOCA	 1
#define HAVE_ALLOCA_H	 1
#define HAVE_FCNTL_H	 1
#define HAVE_LIMITS_H	 1
#define HAVE_SYS_IOCTL_H 1
#define HAVE_SYS_TIME_H	 1
#define HAVE_UNISTD_H	 1
#define TIME_WITH_SYS_TIME 1

#define HAVE_GETTIMEOFDAY 1
#define HAVE_SELECT	 1
#define HAVE_STRDUP	 1
#define HAVE_STRSTR	 1
#define HAVE_STRTOL	 1

/* Deliberately NOT defined:
 *   HAVE_MALLOC_H  — musl's <malloc.h> exists but the sources only want
 *                    malloc(), which <stdlib.h> already declares.
 *   HAVE_FTIME     — ftime(3) is removed from POSIX; timers.c uses
 *                    gettimeofday() when this is absent.
 *   HAVE_LONG_DOUBLE — config.h falls back to double, which is what x86_64
 *                    SSE math wants anyway.
 *   HAVE_USLEEP    — looks like the obvious win over the select(2) fallback
 *                    and is not: timers.c's tl_sleep() takes MICROSECONDS and
 *                    is routinely handed more than a second, which is exactly
 *                    the case POSIX leaves undefined for usleep(). Defining
 *                    it hangs the demo before the first frame — measured.
 *                    The HAVE_SELECT path takes a timeval and has no such
 *                    range limit.
 */

/* Where build.sh installs the modules. */
#ifndef SOUNDDIR
#define SOUNDDIR "/usr/share/kk"
#endif

#endif /* ACONFIG_H */
