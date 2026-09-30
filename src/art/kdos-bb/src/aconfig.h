/*
 * aconfig.h — what upstream's `configure` produced, written out.
 *
 * The fork builds with no autotools at all. That is not tidiness: bb's 2001
 * autoconf probes the compiler with the K&R `main(){return(0);}` that GCC 14
 * promoted from a warning to an error, so `configure` failed with the
 * thoroughly misleading "C compiler cannot create executables" and needed six
 * -Wno- flags to get past a test whose answer was never in doubt.
 *
 * These are the answers for this target — Linux, musl, x86_64 — and they are
 * the ones configure.in asked for and no others. `ftime` is deliberately
 * absent: musl removed it, and defining it would make timers.c call a function
 * that is not there.
 */
#ifndef ACONFIG_H
#define ACONFIG_H

#define PACKAGE "kdos-bb"
#define VERSION "1.3.0"

#define STDC_HEADERS 1
#define TIME_WITH_SYS_TIME 1

#define HAVE_FCNTL_H 1
#define HAVE_LIMITS_H 1
#define HAVE_MALLOC_H 1
#define HAVE_SYS_IOCTL_H 1
#define HAVE_SYS_TIME_H 1
#define HAVE_UNISTD_H 1

#define HAVE_ALLOCA 1
#define HAVE_ALLOCA_H 1

#define HAVE_GETTIMEOFDAY 1
#define HAVE_SELECT 1
#define HAVE_STRDUP 1
#define HAVE_STRSTR 1
#define HAVE_STRTOL 1

#define RETSIGTYPE void

/* The demo is built against libmikmod unconditionally here — the port
 * depends on it, so a build without it is a packaging error rather than a
 * configuration. */
#define HAVE_LIBMIKMOD 1

#endif /* ACONFIG_H */
