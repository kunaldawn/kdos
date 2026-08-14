/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   xdg-desktop-portal-kdos — FileChooser and Settings
 *
 * THE GAP THIS CLOSES. xdg-desktop-portal-wlr implements ScreenCast and
 * Screenshot and nothing else, and the usual second backend is
 * xdg-desktop-portal-gtk — which cannot exist here, because there is no GTK on
 * the host. So `kdos-portals.conf` said `default=none` and every boxed
 * application's Open and Save dialog fell back to whatever the toolkit inside
 * the container shipped: a rounded, antialiased GTK dialog in the middle of a
 * text-mode desktop, every single time anybody opened a file.
 *
 * This is the backend half of the portal — the `org.freedesktop.impl.portal.*`
 * side, which the main xdg-desktop-portal daemon calls into. It does no
 * sandboxing and no permission checking of its own; that is the front end's
 * job, and duplicating it here would be a second policy to keep in agreement.
 *
 * WHAT IT DOES IS SPAWN kdos-pick AND READ ITS STDOUT. That is the whole
 * mechanism, and it is deliberate: the chooser is a normal program with a
 * normal interface, so it can be run by hand, scripted, or replaced, and this
 * file stays a bus adapter rather than becoming a second file browser.
 *
 * THREE RULES, each one a way portals are usually got wrong:
 *
 *   - EVERY REQUEST IS ANSWERED. Response 0 is success, 1 is cancelled, 2 is
 *     an error — and a request that is simply not replied to leaves the
 *     application blocked forever with a half-drawn window. This is the same
 *     lesson as the ScreenCast zero-streams trap already recorded in
 *     CLAUDE.md, arriving on a different interface.
 *   - `parent_window` IS IGNORED, and that is written down rather than hidden.
 *     KDOS has no xdg-foreign, so there is no way to position a dialog over the
 *     window that asked for it. It opens centred.
 *   - THE CHOOSER IS EXEC'D WITH argv, never a command string. Filter patterns
 *     and file names arrive from other applications over a bus.
 * ---------------------------------
 */

/* Guarded: kdos-boxsock shipped an unguarded one of these and it collided with
 * the -D_GNU_SOURCE the build already passes, under -Werror. */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <time.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

/* basu on the target; libsystemd's sd-bus is the same API and is what a
 * developer host is likely to have. The same shape tray.c already uses. */
#if __has_include(<basu/sd-bus.h>)
#  include <basu/sd-bus.h>
#else
#  include <systemd/sd-bus.h>
#endif

#define PORTAL_BUS "org.freedesktop.impl.portal.desktop.kdos"
#define PORTAL_PATH "/org/freedesktop/portal/desktop"

#define MAX_URIS 64
#define MAX_ARGS 32

/* ── running the chooser ───────────────────────────────────────────────── */

struct picked {
	char *uri[MAX_URIS];
	int n;
	int rc;				/* kdos-pick's exit status */
};

static void picked_free(struct picked *p)
{
	for (int i = 0; i < p->n; i++)
		free(p->uri[i]);
	p->n = 0;
}

/*
 * A chooser that is still open, and the call that is waiting for it.
 *
 * THE BUS LOOP MUST NOT BLOCK ON A DIALOG. The first version of this file
 * forked kdos-pick and sat in waitpid() inside the method handler, which meant
 * that for as long as anybody had a file dialog open this process answered
 * nothing at all: a second application's Open request queued behind the first,
 * and — worse, because it happens on every launch — a boxed app asking
 * Settings for the colour scheme hung until the dialog was dismissed. A portal
 * backend is a server; a server that stops serving while it thinks is a server
 * that is down.
 *
 * So the fork happens in the handler, the sd_bus_message is REFFED and the
 * handler returns "handled" without replying; the pipe joins the main loop's
 * poll set, and the reply is built when the child's stdout reaches EOF. sd-bus
 * supports exactly this — a method call may be answered at any later point
 * while a reference to it is held.
 */
struct pending {
	struct pending *next;
	sd_bus_message *call;
	pid_t pid;
	int fd;
	int savefiles;			/* the code mapping differs */
	char *buf;
	size_t len, cap;
};

static struct pending *pendings;

/*
 * Fork kdos-pick and register it. The read and the wait happen later and in
 * that order, which is the rule this file has always had: a chooser that
 * printed more than a pipe buffer would block in write() while this blocked in
 * waitpid(), and neither would ever move.
 */
static int start_picker(const char *const argv[], sd_bus_message *call,
			int savefiles)
{
	int fds[2];

	if (pipe(fds) != 0)
		return -1;

	pid_t pid = fork();
	if (pid < 0) {
		close(fds[0]);
		close(fds[1]);
		return -1;
	}
	if (pid == 0) {
		close(fds[0]);
		dup2(fds[1], STDOUT_FILENO);
		close(fds[1]);
		execvp(argv[0], (char *const *)argv);
		_exit(127);
	}
	close(fds[1]);

	/*
	 * NON-BLOCKING, and this is not belt and braces: `pending_read` drains
	 * until EOF or EAGAIN, and on a blocking pipe the read AFTER the one
	 * poll() promised would block until the chooser wrote again or exited
	 * — which is the whole thing this rewrite exists to stop doing.
	 */
	fcntl(fds[0], F_SETFL, O_NONBLOCK);

	struct pending *p = calloc(1, sizeof(*p));
	if (!p) {
		close(fds[0]);
		return -1;
	}
	p->call = sd_bus_message_ref(call);
	p->pid = pid;
	p->fd = fds[0];
	p->savefiles = savefiles;
	p->next = pendings;
	pendings = p;
	return 0;
}

/* Split what the chooser printed into the URI list the reply carries. */
static void pending_parse(struct pending *pend, struct picked *out)
{
	memset(out, 0, sizeof(*out));
	for (size_t i = 0; i < pend->len && out->n < MAX_URIS;) {
		size_t j = i;
		while (j < pend->len && pend->buf[j] != '\n')
			j++;
		size_t n = j - i;
		while (n && pend->buf[i + n - 1] == '\r')
			n--;
		if (n) {
			char *s = malloc(n + 1);
			if (s) {
				memcpy(s, pend->buf + i, n);
				s[n] = '\0';
				out->uri[out->n++] = s;
			}
		}
		i = j + 1;
	}
}

static void pending_free(struct pending *p)
{
	sd_bus_message_unref(p->call);
	free(p->buf);
	free(p);
}

/* ── reading the options a portal request carries ──────────────────────── */

/*
 * The `filters` option is
 *   a(sa(us))  — a list of (label, list of (kind, pattern))
 * where kind 0 is a glob and kind 1 is a MIME type.
 *
 * Only the FIRST filter is applied and only its globs. kdos-pick takes one
 * filter, and a MIME type cannot be matched without a shared-mime-info lookup
 * that this process has no business doing — a filter that silently matched
 * nothing would be worse than one that is not applied, so a MIME-only filter
 * is dropped and the chooser shows everything.
 */
static int read_first_filter(sd_bus_message *m, char *out, size_t len)
{
	int r = sd_bus_message_enter_container(m, 'a', "(sa(us))");
	if (r <= 0)
		return 0;

	/*
	 * The array is read TO THE END even once a usable filter is found.
	 *
	 * sd_bus_message_exit_container() fails with -EINVAL on a container that
	 * is only partly consumed, and the caller then abandons the rest of the
	 * options dictionary — so a request carrying `filters` followed by
	 * `current_folder` lost the folder, and a second filter entry made the
	 * whole OpenFile fail. Keep the first match, skip the remainder.
	 */
	bool got = false;
	for (;;) {
		r = sd_bus_message_enter_container(m, 'r', "sa(us)");
		if (r <= 0)
			break;

		const char *label = NULL;
		if (sd_bus_message_read(m, "s", &label) < 0) {
			sd_bus_message_exit_container(m);
			break;
		}

		char pats[512] = { 0 };
		if (sd_bus_message_enter_container(m, 'a', "(us)") > 0) {
			const char *pat;
			unsigned kind;
			while (sd_bus_message_read(m, "(us)", &kind, &pat) > 0) {
				if (kind != 0)
					continue;	/* a MIME type */
				if (pats[0] &&
				    strlen(pats) + strlen(pat) + 2 < sizeof(pats))
					strcat(pats, " ");
				if (strlen(pats) + strlen(pat) + 1 < sizeof(pats))
					strcat(pats, pat);
			}
			sd_bus_message_exit_container(m);
		}
		sd_bus_message_exit_container(m);

		if (pats[0] && !got) {
			snprintf(out, len, "%s:%s", label ? label : "Files", pats);
			got = true;
		}
	}
	sd_bus_message_exit_container(m);
	return got ? 1 : 0;
}

struct options {
	char filter[600];
	char current_name[256];
	char current_folder[1024];
	bool multiple;
	bool directory;
};

static void read_options(sd_bus_message *m, struct options *o)
{
	memset(o, 0, sizeof(*o));

	if (sd_bus_message_enter_container(m, 'a', "{sv}") <= 0)
		return;

	while (sd_bus_message_enter_container(m, 'e', "sv") > 0) {
		const char *key = NULL;
		if (sd_bus_message_read(m, "s", &key) < 0) {
			sd_bus_message_exit_container(m);
			break;
		}

		if (!strcmp(key, "multiple")) {
			int v = 0;
			sd_bus_message_read(m, "v", "b", &v);
			o->multiple = v != 0;
		} else if (!strcmp(key, "directory")) {
			int v = 0;
			sd_bus_message_read(m, "v", "b", &v);
			o->directory = v != 0;
		} else if (!strcmp(key, "current_name")) {
			const char *v = NULL;
			if (sd_bus_message_read(m, "v", "s", &v) >= 0 && v)
				snprintf(o->current_name, sizeof(o->current_name),
					 "%s", v);
		} else if (!strcmp(key, "current_folder")) {
			/*
			 * `ay`, and it is NUL-TERMINATED in the byte array —
			 * the spec says so and every implementation relies on
			 * it. Reading it as a string would be a type error on
			 * the bus.
			 */
			const void *data = NULL;
			size_t n = 0;
			if (sd_bus_message_enter_container(m, 'v', "ay") > 0) {
				if (sd_bus_message_read_array(m, 'y', &data, &n) > 0 &&
				    data && n)
					snprintf(o->current_folder,
						 sizeof(o->current_folder), "%.*s",
						 (int)n, (const char *)data);
				sd_bus_message_exit_container(m);
			}
		} else if (!strcmp(key, "filters")) {
			if (sd_bus_message_enter_container(m, 'v', "a(sa(us))") > 0) {
				read_first_filter(m, o->filter, sizeof(o->filter));
				sd_bus_message_exit_container(m);
			}
		} else {
			sd_bus_message_skip(m, "v");
		}
		sd_bus_message_exit_container(m);
	}
	sd_bus_message_exit_container(m);
}

/* ── the two FileChooser methods ───────────────────────────────────────── */

/*
 * Reply shape for both: `(ua{sv})` — a response code and a dictionary. The
 * codes are 0 success, 1 cancelled by the user, 2 ended some other way.
 * Distinguishing 1 from 2 matters: an application that treats "the user pressed
 * Escape" as an error puts up a message about a failure that did not happen.
 */
static int reply_uris(sd_bus_message *call, uint32_t code, struct picked *p)
{
	sd_bus_message *reply = NULL;
	int r = sd_bus_message_new_method_return(call, &reply);
	if (r < 0)
		return r;

	if ((r = sd_bus_message_append(reply, "u", code)) < 0 ||
	    (r = sd_bus_message_open_container(reply, 'a', "{sv}")) < 0)
		goto out;

	if (code == 0 && p && p->n > 0) {
		if ((r = sd_bus_message_open_container(reply, 'e', "sv")) < 0 ||
		    (r = sd_bus_message_append(reply, "s", "uris")) < 0 ||
		    (r = sd_bus_message_open_container(reply, 'v', "as")) < 0 ||
		    (r = sd_bus_message_open_container(reply, 'a', "s")) < 0)
			goto out;
		for (int i = 0; i < p->n; i++)
			if ((r = sd_bus_message_append(reply, "s", p->uri[i])) < 0)
				goto out;
		if ((r = sd_bus_message_close_container(reply)) < 0 ||
		    (r = sd_bus_message_close_container(reply)) < 0 ||
		    (r = sd_bus_message_close_container(reply)) < 0)
			goto out;
	}

	if ((r = sd_bus_message_close_container(reply)) < 0)
		goto out;
	r = sd_bus_send(NULL, reply, NULL);
out:
	sd_bus_message_unref(reply);
	return r;
}

static int file_chooser(sd_bus_message *m, void *userdata, sd_bus_error *err,
			bool save)
{
	const char *handle, *app_id, *parent, *title;
	(void)userdata;
	(void)err;

	if (sd_bus_message_read(m, "osss", &handle, &app_id, &parent, &title) < 0)
		return reply_uris(m, 2, NULL);

	struct options o;
	read_options(m, &o);

	const char *argv[MAX_ARGS];
	int n = 0;
	argv[n++] = "kdos-pick";
	argv[n++] = "--title";
	argv[n++] = (title && *title) ? title : (save ? "Save File" : "Open File");
	if (save)
		argv[n++] = "--save";
	if (o.directory)
		argv[n++] = "--directory";
	if (o.multiple && !save)
		argv[n++] = "--multiple";
	if (o.current_name[0]) {
		argv[n++] = "--name";
		argv[n++] = o.current_name;
	}
	if (o.current_folder[0]) {
		argv[n++] = "--dir";
		argv[n++] = o.current_folder;
	}
	if (o.filter[0]) {
		argv[n++] = "--filter";
		argv[n++] = o.filter;
	}
	argv[n] = NULL;

	/*
	 * Answered later, from the main loop, when the chooser exits — see
	 * `struct pending`. Returning 1 here is "handled"; the reply carries
	 * the same serial because the message is still referenced.
	 */
	if (start_picker(argv, m, 0) != 0)
		return reply_uris(m, 2, NULL);
	return 1;
}

static int method_open_file(sd_bus_message *m, void *u, sd_bus_error *e)
{
	return file_chooser(m, u, e, false);
}

static int method_save_file(sd_bus_message *m, void *u, sd_bus_error *e)
{
	return file_chooser(m, u, e, true);
}

/*
 * SaveFiles — "save this whole set into a folder you pick".
 *
 * Implemented as a directory choice, which is exactly what it is: the front end
 * hands the application a folder and it writes the files itself. Answering
 * `not supported` would be the easy road and would break the one case it
 * exists for, which is a mail client saving every attachment at once.
 */
static int method_save_files(sd_bus_message *m, void *userdata, sd_bus_error *err)
{
	const char *handle, *app_id, *parent, *title;
	(void)userdata;
	(void)err;

	if (sd_bus_message_read(m, "osss", &handle, &app_id, &parent, &title) < 0)
		return reply_uris(m, 2, NULL);

	struct options o;
	read_options(m, &o);

	const char *argv[MAX_ARGS];
	int n = 0;
	argv[n++] = "kdos-pick";
	argv[n++] = "--directory";
	argv[n++] = "--title";
	argv[n++] = (title && *title) ? title : "Save Files";
	if (o.current_folder[0]) {
		argv[n++] = "--dir";
		argv[n++] = o.current_folder;
	}
	argv[n] = NULL;

	if (start_picker(argv, m, 1) != 0)
		return reply_uris(m, 2, NULL);
	return 1;
}

/*
 * The chooser has closed its stdout. Read what is left, reap it, and answer
 * the call that has been waiting.
 *
 * Exit 0 WITH at least one URI is the only success. A chooser that exited 0
 * having printed nothing reported to the application as "here is your file"
 * with an empty list is the shape of bug that makes a save write nowhere.
 *
 * 0 with a URI is success and 1 is "the user cancelled" — but 127 is execvp
 * failing and 2 is kdos-pick refusing to start, and reporting either as a
 * cancellation tells the application a lie it cannot act on. Those are
 * response 2, an error, which is what a portal says when it could not ask the
 * question at all.
 */
static void pending_finish(struct pending *pend)
{
	int status = 0;
	while (waitpid(pend->pid, &status, 0) < 0 && errno == EINTR)
		;
	int rc = WIFEXITED(status) ? WEXITSTATUS(status) : 1;

	struct picked p;
	pending_parse(pend, &p);

	uint32_t code;
	if (pend->savefiles) {
		code = (rc == 0 && p.n > 0) ? 0 : 1;
	} else if (rc == 0) {
		code = p.n > 0 ? 0 : 1;
	} else if (rc == 1) {
		code = 1;
	} else {
		code = 2;
	}
	reply_uris(pend->call, code, &p);
	picked_free(&p);
	close(pend->fd);
}

/* Drain one pending chooser's pipe. Returns 1 when it has finished. */
static int pending_read(struct pending *pend)
{
	for (;;) {
		if (pend->len + 4096 > pend->cap) {
			size_t cap = pend->cap ? pend->cap * 2 : 8192;
			char *nb = realloc(pend->buf, cap);
			if (!nb)
				return 1;	/* out of memory: answer with what we have */
			pend->buf = nb;
			pend->cap = cap;
		}
		ssize_t n = read(pend->fd, pend->buf + pend->len,
				 pend->cap - pend->len);
		if (n > 0) {
			pend->len += (size_t)n;
			continue;
		}
		if (n == 0)
			return 1;		/* EOF: the chooser is done */
		if (errno == EINTR)
			continue;
		if (errno == EAGAIN || errno == EWOULDBLOCK)
			return 0;
		return 1;			/* a real error is also "done" */
	}
}

/* ── Settings ──────────────────────────────────────────────────────────── */

/*
 * `org.freedesktop.appearance color-scheme` is the one setting worth answering
 * and the answer is always 1 — "prefer dark". KDOS has no light palette: all
 * four accents are phosphor on near-black, and a toolkit told "no preference"
 * picks its own light theme and stands out from everything around it.
 *
 * `accent-color` is a (ddd) of doubles in 0..1. It is read from the same
 * one-word file kdos-shell and kdos-comp read, so an application that honours
 * it wears the accent the desktop is wearing.
 */
static int read_accent(double *r, double *g, double *b)
{
	/* The default is phosphor #39ff14 — the same literal libkcolor carries,
	 * repeated here rather than linked because this process links sd-bus
	 * and nothing else. */
	*r = 0x39 / 255.0;
	*g = 0xff / 255.0;
	*b = 0x14 / 255.0;

	const char *home = getenv("HOME");
	const char *cache = getenv("XDG_CACHE_HOME");
	char path[1024];
	if (cache && *cache)
		snprintf(path, sizeof(path), "%s/kdos/theme", cache);
	else if (home)
		snprintf(path, sizeof(path), "%s/.cache/kdos/theme", home);
	else
		return 0;

	FILE *f = fopen(path, "r");
	if (!f)
		return 0;
	char name[64] = { 0 };
	if (fgets(name, sizeof(name), f)) {
		char *nl = strchr(name, '\n');
		if (nl)
			*nl = '\0';
		if (!strcmp(name, "amber")) {
			*r = 1.0; *g = 0xb0 / 255.0; *b = 0.0;
		} else if (!strcmp(name, "ice")) {
			*r = 0x7d / 255.0; *g = 0xf9 / 255.0; *b = 1.0;
		} else if (!strcmp(name, "bone")) {
			*r = 0xe8 / 255.0; *g = 0xe6 / 255.0; *b = 0xdf / 255.0;
		}
	}
	fclose(f);
	return 0;
}

static int append_setting(sd_bus_message *reply, const char *key)
{
	int r;
	if (!strcmp(key, "color-scheme"))
		return sd_bus_message_append(reply, "v", "u", (uint32_t)1);
	if (!strcmp(key, "accent-color")) {
		double cr, cg, cb;
		read_accent(&cr, &cg, &cb);
		if ((r = sd_bus_message_open_container(reply, 'v', "(ddd)")) < 0 ||
		    (r = sd_bus_message_append(reply, "(ddd)", cr, cg, cb)) < 0)
			return r;
		return sd_bus_message_close_container(reply);
	}
	return -ENOENT;
}

static int method_settings_read(sd_bus_message *m, void *userdata,
				sd_bus_error *err)
{
	const char *ns, *key;
	(void)userdata;

	if (sd_bus_message_read(m, "ss", &ns, &key) < 0)
		return sd_bus_error_set_const(err, SD_BUS_ERROR_INVALID_ARGS, "bad args");
	if (strcmp(ns, "org.freedesktop.appearance"))
		return sd_bus_error_set_const(err,
			"org.freedesktop.portal.Error.NotFound", "no such namespace");

	sd_bus_message *reply = NULL;
	int r = sd_bus_message_new_method_return(m, &reply);
	if (r < 0)
		return r;
	if ((r = append_setting(reply, key)) < 0) {
		sd_bus_message_unref(reply);
		return sd_bus_error_set_const(err,
			"org.freedesktop.portal.Error.NotFound", "no such key");
	}
	r = sd_bus_send(NULL, reply, NULL);
	sd_bus_message_unref(reply);
	return r;
}

static int method_settings_read_all(sd_bus_message *m, void *userdata,
				    sd_bus_error *err)
{
	(void)userdata;
	(void)err;
	/* The requested namespaces are ignored: there is exactly one, and
	 * filtering a single-entry set against a pattern list would be more
	 * code than the set. */
	sd_bus_message_skip(m, "as");

	sd_bus_message *reply = NULL;
	int r = sd_bus_message_new_method_return(m, &reply);
	if (r < 0)
		return r;

	static const char *const keys[] = { "color-scheme", "accent-color" };

	if ((r = sd_bus_message_open_container(reply, 'a', "{sa{sv}}")) < 0 ||
	    (r = sd_bus_message_open_container(reply, 'e', "sa{sv}")) < 0 ||
	    (r = sd_bus_message_append(reply, "s", "org.freedesktop.appearance")) < 0 ||
	    (r = sd_bus_message_open_container(reply, 'a', "{sv}")) < 0)
		goto out;

	for (size_t i = 0; i < sizeof(keys) / sizeof(keys[0]); i++) {
		if ((r = sd_bus_message_open_container(reply, 'e', "sv")) < 0 ||
		    (r = sd_bus_message_append(reply, "s", keys[i])) < 0 ||
		    (r = append_setting(reply, keys[i])) < 0 ||
		    (r = sd_bus_message_close_container(reply)) < 0)
			goto out;
	}

	if ((r = sd_bus_message_close_container(reply)) < 0 ||
	    (r = sd_bus_message_close_container(reply)) < 0 ||
	    (r = sd_bus_message_close_container(reply)) < 0)
		goto out;
	r = sd_bus_send(NULL, reply, NULL);
out:
	sd_bus_message_unref(reply);
	return r;
}

/* ── vtables ───────────────────────────────────────────────────────────── */

static const sd_bus_vtable file_chooser_vtable[] = {
	SD_BUS_VTABLE_START(0),
	SD_BUS_METHOD("OpenFile", "osssa{sv}", "ua{sv}", method_open_file,
		      SD_BUS_VTABLE_UNPRIVILEGED),
	SD_BUS_METHOD("SaveFile", "osssa{sv}", "ua{sv}", method_save_file,
		      SD_BUS_VTABLE_UNPRIVILEGED),
	SD_BUS_METHOD("SaveFiles", "osssa{sv}", "ua{sv}", method_save_files,
		      SD_BUS_VTABLE_UNPRIVILEGED),
	SD_BUS_VTABLE_END
};

static const sd_bus_vtable settings_vtable[] = {
	SD_BUS_VTABLE_START(0),
	SD_BUS_METHOD("ReadAll", "as", "a{sa{sv}}", method_settings_read_all,
		      SD_BUS_VTABLE_UNPRIVILEGED),
	SD_BUS_METHOD("Read", "ss", "v", method_settings_read,
		      SD_BUS_VTABLE_UNPRIVILEGED),
	SD_BUS_SIGNAL("SettingChanged", "ssv", 0),
	SD_BUS_VTABLE_END
};

int main(int argc, char **argv)
{
	sd_bus *bus = NULL;
	sd_bus_slot *fc = NULL, *st = NULL;
	int r;

	for (int i = 1; i < argc; i++) {
		if (!strcmp(argv[i], "--version")) {
			printf("xdg-desktop-portal-kdos 0.1.0\n");
			return 0;
		}
		fprintf(stderr, "usage: xdg-desktop-portal-kdos\n");
		return 2;
	}

	/*
	 * A chooser that dies must not become a zombie, and this process may
	 * outlive many of them. SIGPIPE off for the same reason kdos-appbox
	 * turns it off: writing to a bus peer that has gone must be an error
	 * code, not a signal.
	 */
	signal(SIGPIPE, SIG_IGN);

	r = sd_bus_open_user(&bus);
	if (r < 0) {
		fprintf(stderr, "xdg-desktop-portal-kdos: no session bus: %s\n",
			strerror(-r));
		return 1;
	}

	r = sd_bus_add_object_vtable(bus, &fc, PORTAL_PATH,
				     "org.freedesktop.impl.portal.FileChooser",
				     file_chooser_vtable, NULL);
	if (r < 0)
		goto fail;
	r = sd_bus_add_object_vtable(bus, &st, PORTAL_PATH,
				     "org.freedesktop.impl.portal.Settings",
				     settings_vtable, NULL);
	if (r < 0)
		goto fail;

	/*
	 * The name is requested LAST, after both interfaces exist. The main
	 * portal daemon calls the moment the name appears, and a call that
	 * landed before the vtable was installed would come back as
	 * UnknownMethod — which the daemon caches as "this backend cannot do
	 * FileChooser" for the rest of the session.
	 */
	r = sd_bus_request_name(bus, PORTAL_BUS, 0);
	if (r < 0)
		goto fail;

	/*
	 * sd_bus_wait() would be the one-liner and it is exactly what cannot be
	 * used here: it waits on the bus and on nothing else, and every open
	 * file chooser is a pipe this process has to watch at the same time.
	 * So the loop is the documented sd-bus integration — process until it
	 * has nothing left, then poll the bus fd together with the choosers'.
	 */
	for (;;) {
		r = sd_bus_process(bus, NULL);
		if (r < 0)
			break;
		if (r > 0)
			continue;

		struct pollfd fds[1 + 16];
		struct pending *watched[16];
		int nfds = 0, nw = 0;

		fds[nfds].fd = sd_bus_get_fd(bus);
		fds[nfds].events = (short)sd_bus_get_events(bus);
		fds[nfds].revents = 0;
		nfds++;

		for (struct pending *p = pendings; p && nw < 16; p = p->next) {
			fds[nfds].fd = p->fd;
			fds[nfds].events = POLLIN;
			fds[nfds].revents = 0;
			watched[nw++] = p;
			nfds++;
		}

		uint64_t usec = 0;
		int timeout = -1;
		if (sd_bus_get_timeout(bus, &usec) >= 0 &&
		    usec != (uint64_t)-1) {
			struct timespec ts;
			clock_gettime(CLOCK_MONOTONIC, &ts);
			uint64_t now = (uint64_t)ts.tv_sec * 1000000
				+ (uint64_t)ts.tv_nsec / 1000;
			timeout = usec > now
				? (int)((usec - now + 999) / 1000) : 0;
		}

		if (poll(fds, (nfds_t)nfds, timeout) < 0) {
			if (errno == EINTR)
				continue;
			break;
		}

		for (int i = 0; i < nw; i++) {
			if (!fds[i + 1].revents)
				continue;
			if (!pending_read(watched[i]))
				continue;
			pending_finish(watched[i]);
			/* Unlink before freeing: the list is walked again on
			 * the next turn and a freed node in it is the crash
			 * this loop must not have while a dialog is open. */
			for (struct pending **pp = &pendings; *pp;
			     pp = &(*pp)->next) {
				if (*pp == watched[i]) {
					*pp = watched[i]->next;
					break;
				}
			}
			pending_free(watched[i]);
		}
	}

fail:
	if (r < 0)
		fprintf(stderr, "xdg-desktop-portal-kdos: %s\n", strerror(-r));
	sd_bus_slot_unref(st);
	sd_bus_slot_unref(fc);
	sd_bus_unref(bus);
	return r < 0 ? 1 : 0;
}
