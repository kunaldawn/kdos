/*
 * SHL - Library Log/Debug Interface
 *
 * Copyright (c) 2010-2013 David Herrmann <dh.herrmann@gmail.com>
 * Dedicated to the Public Domain
 */

/*
 * Library Log/Debug Interface
 *
 * A library must not write to anybody's stderr, so every object carries an
 * `llog` function pointer and an `llog_data` and the macros below submit
 * through them. A NULL pointer means logging is off, which is the default and
 * what every consumer in this tree leaves it at.
 *
 * THIS HEADER IS INTERNAL. It is not installed and not part of kvt.h; the
 * public surface is `kvt_log_t` there, whose signature must stay identical to
 * `llog_submit_t` below or a consumer's logger is called with a shuffled
 * argument list.
 *
 * An object used with these macros must have both an `llog` field of type
 * llog_submit_t and an `llog_data` field; llog_printf() reads them by name.
 *
 * llog_debug() compiles to nothing unless BUILD_ENABLE_DEBUG is defined, so a
 * debug call on a hot path costs nothing in a shipped build — but its
 * arguments are still type-checked against the format string.
 */

#ifndef KVT_SHL_LLOG_H
#define KVT_SHL_LLOG_H

#include <errno.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdlib.h>

enum llog_severity {
	LLOG_FATAL = 0,
	LLOG_ALERT = 1,
	LLOG_CRITICAL = 2,
	LLOG_ERROR = 3,
	LLOG_WARNING = 4,
	LLOG_NOTICE = 5,
	LLOG_INFO = 6,
	LLOG_DEBUG = 7,
	LLOG_SEV_NUM,
};

typedef void (*llog_submit_t) (void *data,
			       const char *file,
			       int line,
			       const char *func,
			       const char *subs,
			       unsigned int sev,
			       const char *format,
			       va_list args);

static inline __attribute__((format(printf, 8, 9)))
void llog_format(llog_submit_t llog,
		 void *data,
		 const char *file,
		 int line,
		 const char *func,
		 const char *subs,
		 unsigned int sev,
		 const char *format,
		 ...)
{
	int saved_errno = errno;
	va_list list;

	if (llog) {
		va_start(list, format);
		errno = saved_errno;
		llog(data, file, line, func, subs, sev, format, list);
		va_end(list);
	}
}

#ifndef LLOG_SUBSYSTEM
static const char *LLOG_SUBSYSTEM __attribute__((__unused__));
#endif

#define LLOG_DEFAULT __FILE__, __LINE__, __func__, LLOG_SUBSYSTEM

#define llog_printf(obj, sev, format, ...) \
	llog_format((obj)->llog, \
		    (obj)->llog_data, \
		    LLOG_DEFAULT, \
		    (sev), \
		    (format), \
		    ##__VA_ARGS__)
#define llog_dprintf(obj, data, sev, format, ...) \
	llog_format((obj), \
		    (data), \
		    LLOG_DEFAULT, \
		    (sev), \
		    (format), \
		    ##__VA_ARGS__)

/* The no-op the logging macros collapse to when a subsystem has no logger.
 * Its arguments are deliberately unused; naming them is what keeps the
 * printf format check on the call sites. */
static inline __attribute__((format(printf, 4, 5)))
void llog_dummyf(llog_submit_t llog, void *data, unsigned int sev,
		 const char *format, ...)
{
	(void)llog;
	(void)data;
	(void)sev;
	(void)format;
}

/*
 * Helpers
 * They pick up all the default values and submit the message to the
 * llog-subsystem. The llog_debug() function will discard the message unless
 * BUILD_ENABLE_DEBUG is defined.
 */

#ifdef BUILD_ENABLE_DEBUG
	#define llog_ddebug(obj, data, format, ...) \
		llog_dprintf((obj), (data), LLOG_DEBUG, (format), ##__VA_ARGS__)
	#define llog_debug(obj, format, ...) \
		llog_ddebug((obj)->llog, (obj)->llog_data, (format), ##__VA_ARGS__)
#else
	#define llog_ddebug(obj, data, format, ...) \
		llog_dummyf((obj), (data), LLOG_DEBUG, (format), ##__VA_ARGS__)
	#define llog_debug(obj, format, ...) \
		llog_ddebug((obj)->llog, (obj)->llog_data, (format), ##__VA_ARGS__)
#endif

#define llog_warning(obj, format, ...) \
	llog_printf((obj), LLOG_WARNING, (format), ##__VA_ARGS__)
#define llog_error(obj, format, ...) \
	llog_printf((obj), LLOG_ERROR, (format), ##__VA_ARGS__)

#endif /* KVT_SHL_LLOG_H */
