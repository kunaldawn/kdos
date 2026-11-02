/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   kdos-tools — the session and system bus, for the verbs that need one
 *
 * THE LIBRARY IS OPENED AT RUN TIME, NOT LINKED. This binary is also ksvc,
 * kdos-getty and the kdos-bootctl the initramfs copies with a hand-kept
 * library list, so a linked libbasu would be one more library every one of
 * them needs before it can start, for the sake of a theme switch. KDOS ships
 * the sd-bus API as libbasu; a development host has it as libsystemd, and the
 * API is the same one, which is what lets the selftest drive these verbs
 * against a private bus on any host. Without either there is no bus, and every
 * verb that needs one says so and fails.
 *
 * THE HANDLES ARE sd-bus's `sd_bus *` AND `sd_bus_message *`, carried as void
 * pointers so no file here needs an sd-bus header. `sd_bus_error` is the one
 * structure whose layout matters, and it is the public ABI: two strings and an
 * int.
 *
 * A CALL HAS A DEADLINE. sd-bus's own default is 25 seconds; a verb that
 * waited that long on a hung KWin would be a command that seemed to hang, so
 * every call gives up after KDT_BUS_TIMEOUT_US.
 * ---------------------------------
 */

#include <dlfcn.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "kdos-tools.h"

/* Long enough for loadEffect, which compiles a shader the first time, and for
 * evaluateScript, which lays out a panel; short enough that a hung peer reads
 * as an error rather than a hang. */
#define KDT_BUS_TIMEOUT_US 15000000ULL

typedef struct {
	const char *name;
	const char *message;
	int need_free;
} SdError;

struct KdtBus {
	void *lib;
	void *bus;
	int (*open_user)(void **bus);
	int (*open_system)(void **bus);
	void *(*close_unref)(void *bus);
	int (*set_timeout)(void *bus, uint64_t usec);
	int (*new_call)(void *bus, void **m, const char *dest,
			const char *path, const char *iface,
			const char *member);
	int (*appendv)(void *m, const char *types, va_list ap);
	int (*append_strv)(void *m, char **l);
	int (*call)(void *bus, void *m, uint64_t usec, SdError *err,
		    void **reply);
	int (*readv)(void *m, const char *types, va_list ap);
	int (*enter)(void *m, char type, const char *contents);
	int (*leave)(void *m);
	int (*peek)(void *m, char *type, const char **contents);
	int (*skip)(void *m, const char *types);
	void *(*unref)(void *m);
	void (*error_free)(SdError *e);
	/* the serving half: an object, a name and the loop */
	int (*add_object)(void *bus, void **slot, const char *path,
			  int (*cb)(void *m, void *ud, void *err), void *ud);
	int (*request_name)(void *bus, const char *name, uint64_t flags);
	int (*release_name)(void *bus, const char *name);
	int (*get_fd)(void *bus);
	int (*get_events)(void *bus);
	int (*process)(void *bus, void **ret);
	const char *(*get_member)(void *m);
	const char *(*get_interface)(void *m);
	int (*new_return)(void *call, void **m);
	int (*new_signal)(void *bus, void **m, const char *path,
			  const char *iface, const char *member);
	int (*open_c)(void *m, char type, const char *contents);
	int (*close_c)(void *m);
	int (*send)(void *bus, void *m, uint64_t *cookie);
	int (*reply_error)(void *call, const SdError *e);
};

/* The two names the sd-bus API ships under: KDOS's own, then a development
 * host's. Never a path: the loader's own search is what finds either. */
static const char *const LIBS[] = { "libbasu.so.0", "libsystemd.so.0" };

static int load(KdtBus *b)
{
	for (size_t i = 0; i < sizeof(LIBS) / sizeof(LIBS[0]) && !b->lib; i++)
		b->lib = dlopen(LIBS[i], RTLD_NOW | RTLD_LOCAL);
	if (!b->lib)
		return -1;
	*(void **)&b->open_user = dlsym(b->lib, "sd_bus_open_user");
	*(void **)&b->open_system = dlsym(b->lib, "sd_bus_open_system");
	*(void **)&b->close_unref = dlsym(b->lib, "sd_bus_flush_close_unref");
	*(void **)&b->set_timeout = dlsym(b->lib,
					  "sd_bus_set_method_call_timeout");
	*(void **)&b->new_call = dlsym(b->lib,
				       "sd_bus_message_new_method_call");
	*(void **)&b->appendv = dlsym(b->lib, "sd_bus_message_appendv");
	*(void **)&b->append_strv = dlsym(b->lib, "sd_bus_message_append_strv");
	*(void **)&b->call = dlsym(b->lib, "sd_bus_call");
	*(void **)&b->readv = dlsym(b->lib, "sd_bus_message_readv");
	*(void **)&b->enter = dlsym(b->lib, "sd_bus_message_enter_container");
	*(void **)&b->leave = dlsym(b->lib, "sd_bus_message_exit_container");
	*(void **)&b->peek = dlsym(b->lib, "sd_bus_message_peek_type");
	*(void **)&b->skip = dlsym(b->lib, "sd_bus_message_skip");
	*(void **)&b->unref = dlsym(b->lib, "sd_bus_message_unref");
	*(void **)&b->error_free = dlsym(b->lib, "sd_bus_error_free");
	*(void **)&b->add_object = dlsym(b->lib, "sd_bus_add_object");
	*(void **)&b->request_name = dlsym(b->lib, "sd_bus_request_name");
	*(void **)&b->release_name = dlsym(b->lib, "sd_bus_release_name");
	*(void **)&b->get_fd = dlsym(b->lib, "sd_bus_get_fd");
	*(void **)&b->get_events = dlsym(b->lib, "sd_bus_get_events");
	*(void **)&b->process = dlsym(b->lib, "sd_bus_process");
	*(void **)&b->get_member = dlsym(b->lib, "sd_bus_message_get_member");
	*(void **)&b->get_interface = dlsym(b->lib,
					    "sd_bus_message_get_interface");
	*(void **)&b->new_return = dlsym(b->lib,
					 "sd_bus_message_new_method_return");
	*(void **)&b->new_signal = dlsym(b->lib, "sd_bus_message_new_signal");
	*(void **)&b->open_c = dlsym(b->lib, "sd_bus_message_open_container");
	*(void **)&b->close_c = dlsym(b->lib,
				      "sd_bus_message_close_container");
	*(void **)&b->send = dlsym(b->lib, "sd_bus_send");
	*(void **)&b->reply_error = dlsym(b->lib, "sd_bus_reply_method_error");
	if (!b->open_user || !b->open_system || !b->close_unref ||
	    !b->set_timeout || !b->new_call || !b->appendv ||
	    !b->append_strv || !b->call ||
	    !b->readv || !b->enter || !b->leave || !b->peek || !b->skip ||
	    !b->unref || !b->error_free || !b->add_object ||
	    !b->request_name || !b->release_name || !b->get_fd ||
	    !b->get_events || !b->process || !b->get_member ||
	    !b->get_interface || !b->new_return || !b->new_signal ||
	    !b->open_c || !b->close_c || !b->send || !b->reply_error)
		return -1;
	return 0;
}

KdtBus *kdt_bus_open(int system)
{
	KdtBus *b = kb_calloc(1, sizeof(*b));

	if (load(b) != 0 ||
	    (system ? b->open_system(&b->bus) : b->open_user(&b->bus)) < 0) {
		b->bus = NULL;
		kdt_bus_close(b);
		return NULL;
	}
	b->set_timeout(b->bus, KDT_BUS_TIMEOUT_US);
	return b;
}

void kdt_bus_close(KdtBus *b)
{
	if (!b)
		return;
	if (b->bus)
		b->close_unref(b->bus);
	if (b->lib)
		dlclose(b->lib);
	free(b);
}

int kdt_bus_callv(KdtBus *b, const char *dest, const char *path,
		  const char *iface, const char *member, KdtMsg **reply,
		  char *err, size_t errn, const char *types, va_list ap)
{
	void *m = NULL, *r = NULL;
	SdError e = { 0 };
	int rc;

	if (err && errn)
		err[0] = '\0';
	if (reply)
		*reply = NULL;
	if (!b)
		return -1;
	rc = b->new_call(b->bus, &m, dest, path, iface, member);
	if (rc >= 0 && types && *types)
		rc = b->appendv(m, types, ap);
	if (rc >= 0)
		rc = b->call(b->bus, m, 0, &e, &r);
	if (m)
		b->unref(m);
	if (rc < 0) {
		if (err && errn)
			snprintf(err, errn, "%s",
				 e.message ? e.message
				 : e.name  ? e.name
					   : "the call could not be made");
		b->error_free(&e);
		if (r)
			b->unref(r);
		return -1;
	}
	b->error_free(&e);
	if (reply)
		*reply = r;
	else if (r)
		b->unref(r);
	return 0;
}

int kdt_bus_call(KdtBus *b, const char *dest, const char *path,
		 const char *iface, const char *member, KdtMsg **reply,
		 char *err, size_t errn, const char *types, ...)
{
	va_list ap;
	int rc;

	va_start(ap, types);
	rc = kdt_bus_callv(b, dest, path, iface, member, reply, err, errn,
			   types, ap);
	va_end(ap);
	return rc;
}

int kdt_bus_read(KdtBus *b, KdtMsg *m, const char *types, ...)
{
	va_list ap;
	int rc;

	if (!b || !m)
		return -1;
	va_start(ap, types);
	rc = b->readv(m, types, ap);
	va_end(ap);
	return rc;
}

int kdt_bus_enter(KdtBus *b, KdtMsg *m, char type, const char *contents)
{
	return b && m ? b->enter(m, type, contents) : -1;
}

int kdt_bus_leave(KdtBus *b, KdtMsg *m)
{
	return b && m ? b->leave(m) : -1;
}

int kdt_bus_peek(KdtBus *b, KdtMsg *m, char *type, const char **contents)
{
	return b && m ? b->peek(m, type, contents) : -1;
}

int kdt_bus_skip(KdtBus *b, KdtMsg *m, const char *types)
{
	return b && m ? b->skip(m, types) : -1;
}

void kdt_bus_free(KdtBus *b, KdtMsg *m)
{
	if (b && m)
		b->unref(m);
}

int kdt_bus_has_owner(KdtBus *b, const char *name)
{
	KdtMsg *r = NULL;
	int has = 0;

	if (kdt_bus_call(b, "org.freedesktop.DBus", "/org/freedesktop/DBus",
			 "org.freedesktop.DBus", "NameHasOwner", &r, NULL, 0,
			 "s", name) != 0)
		return -1;
	if (kdt_bus_read(b, r, "b", &has) < 0)
		has = -1;
	kdt_bus_free(b, r);
	return has < 0 ? -1 : !!has;
}

/*
 * ONE METHOD WITH ONE BOOLEAN ANSWER, which is most of what KWin's effect
 * interface returns. -1 is "no answer", never false: a refusal by the peer
 * and a peer that is not there are different sentences to a person.
 */
int kdt_bus_call_b(KdtBus *b, const char *dest, const char *path,
		   const char *iface, const char *member, char *err,
		   size_t errn, const char *types, ...)
{
	KdtMsg *r = NULL;
	va_list ap;
	int v = 0, rc;

	va_start(ap, types);
	rc = kdt_bus_callv(b, dest, path, iface, member, &r, err, errn, types,
			   ap);
	va_end(ap);
	if (rc != 0)
		return -1;
	if (kdt_bus_read(b, r, "b", &v) < 0)
		v = -1;
	kdt_bus_free(b, r);
	return v < 0 ? -1 : !!v;
}

/* The same, for a method whose answer is one string; malloc'd, or NULL. */
char *kdt_bus_call_s(KdtBus *b, const char *dest, const char *path,
		     const char *iface, const char *member, char *err,
		     size_t errn, const char *types, ...)
{
	KdtMsg *r = NULL;
	const char *s = NULL;
	char *out = NULL;
	va_list ap;
	int rc;

	va_start(ap, types);
	rc = kdt_bus_callv(b, dest, path, iface, member, &r, err, errn, types,
			   ap);
	va_end(ap);
	if (rc != 0)
		return NULL;
	if (kdt_bus_read(b, r, "s", &s) >= 0 && s)
		out = kb_strdup(s);
	kdt_bus_free(b, r);
	return out;
}

/*
 * A PROPERTY, through org.freedesktop.DBus.Properties.Get. The reply is a
 * variant, and the caller reads the value out of it with kdt_bus_read after
 * this has entered the variant: `sig` is the value's own signature, and a
 * property of another type is refused rather than misread.
 */
int kdt_bus_get_prop(KdtBus *b, const char *dest, const char *path,
		     const char *iface, const char *prop, const char *sig,
		     KdtMsg **reply, char *err, size_t errn)
{
	if (kdt_bus_call(b, dest, path, "org.freedesktop.DBus.Properties",
			 "Get", reply, err, errn, "ss", iface, prop) != 0)
		return -1;
	if (kdt_bus_enter(b, *reply, 'v', sig) <= 0) {
		if (err && errn)
			snprintf(err, errn, "%s.%s is not of type %s", iface,
				 prop, sig);
		kdt_bus_free(b, *reply);
		*reply = NULL;
		return -1;
	}
	return 0;
}

char *kdt_bus_get_prop_s(KdtBus *b, const char *dest, const char *path,
			 const char *iface, const char *prop, char *err,
			 size_t errn)
{
	KdtMsg *r = NULL;
	const char *s = NULL;
	char *out = NULL;

	if (kdt_bus_get_prop(b, dest, path, iface, prop, "s", &r, err, errn))
		return NULL;
	if (kdt_bus_read(b, r, "s", &s) >= 0 && s)
		out = kb_strdup(s);
	kdt_bus_free(b, r);
	return out;
}

static int append_b(KdtBus *b, void *m, int v, ...)
{
	va_list ap;
	int rc;

	va_start(ap, v);
	rc = b->appendv(m, "b", ap);
	va_end(ap);
	return rc;
}

/*
 * A method taking a string array and one boolean — locale1's SetLocale. An
 * array of a length known only at run time cannot be written out in a
 * variadic call, so it goes in through sd-bus's own strv form.
 */
int kdt_bus_call_strv(KdtBus *b, const char *dest, const char *path,
		      const char *iface, const char *member, char *err,
		      size_t errn, char **v, int n, int flag)
{
	void *m = NULL, *r = NULL;
	SdError e = { 0 };
	char **l;
	int rc;

	if (err && errn)
		err[0] = '\0';
	if (!b)
		return -1;
	l = kb_calloc((size_t)n + 1, sizeof(*l));
	for (int i = 0; i < n; i++)
		l[i] = v[i];
	rc = b->new_call(b->bus, &m, dest, path, iface, member);
	if (rc >= 0)
		rc = b->append_strv(m, l);
	if (rc >= 0)
		rc = append_b(b, m, 0, flag);
	if (rc >= 0)
		rc = b->call(b->bus, m, 0, &e, &r);
	free(l);
	if (m)
		b->unref(m);
	if (r)
		b->unref(r);
	if (rc < 0 && err && errn)
		snprintf(err, errn, "%s",
			 e.message ? e.message
			 : e.name  ? e.name
				   : "the call could not be made");
	b->error_free(&e);
	return rc < 0 ? -1 : 0;
}

/* ── serving ───────────────────────────────────────────────────────────── */

/*
 * AN OBJECT IS ONE CALLBACK, not a vtable. sd-bus's vtable is a structure
 * whose layout this file would have to restate without the header; a plain
 * object callback is a function pointer with a fixed signature, which is the
 * same in every sd-bus. The callback answers what it handles (a reply sent,
 * return 1) and returns 0 for the rest, which sd-bus refuses as an unknown
 * method.
 */
int kdt_bus_serve(KdtBus *b, const char *path, KdtHandler cb, void *ud)
{
	/* No slot asked for: the object then belongs to the bus and goes with
	 * it, where a slot held here would keep the bus alive past its close. */
	if (!b)
		return -1;
	return b->add_object(b->bus, NULL, path,
			     (int (*)(void *, void *, void *))cb, ud) < 0
		       ? -1 : 0;
}

int kdt_bus_request_name(KdtBus *b, const char *name)
{
	return b && b->request_name(b->bus, name, 0) >= 0 ? 0 : -1;
}

void kdt_bus_release_name(KdtBus *b, const char *name)
{
	if (b)
		b->release_name(b->bus, name);
}

int kdt_bus_fd(KdtBus *b, short *events)
{
	int ev;

	if (!b)
		return -1;
	ev = b->get_events(b->bus);
	*events = (short)(ev > 0 ? ev : 0x001);
	return b->get_fd(b->bus);
}

/* Everything that has arrived, dispatched. -1 when the bus has gone. */
int kdt_bus_dispatch(KdtBus *b)
{
	int r;

	if (!b)
		return -1;
	while ((r = b->process(b->bus, NULL)) > 0)
		;
	return r < 0 ? -1 : 0;
}

const char *kdt_bus_member(KdtBus *b, KdtMsg *m)
{
	const char *s = b && m ? b->get_member(m) : NULL;

	return s ? s : "";
}

const char *kdt_bus_interface(KdtBus *b, KdtMsg *m)
{
	const char *s = b && m ? b->get_interface(m) : NULL;

	return s ? s : "";
}

KdtMsg *kdt_bus_new_reply(KdtBus *b, KdtMsg *call)
{
	void *m = NULL;

	return b && b->new_return(call, &m) >= 0 ? m : NULL;
}

KdtMsg *kdt_bus_new_signal(KdtBus *b, const char *path, const char *iface,
			   const char *member)
{
	void *m = NULL;

	return b && b->new_signal(b->bus, &m, path, iface, member) >= 0 ? m
									 : NULL;
}

int kdt_bus_append(KdtBus *b, KdtMsg *m, const char *types, ...)
{
	va_list ap;
	int rc;

	if (!b || !m)
		return -1;
	va_start(ap, types);
	rc = b->appendv(m, types, ap);
	va_end(ap);
	return rc < 0 ? -1 : 0;
}

int kdt_bus_open_container(KdtBus *b, KdtMsg *m, char type,
			   const char *contents)
{
	return b && m && b->open_c(m, type, contents) >= 0 ? 0 : -1;
}

int kdt_bus_close_container(KdtBus *b, KdtMsg *m)
{
	return b && m && b->close_c(m) >= 0 ? 0 : -1;
}

/* Sent and released, whatever happened. */
int kdt_bus_send(KdtBus *b, KdtMsg *m)
{
	int rc;

	if (!b || !m)
		return -1;
	rc = b->send(b->bus, m, NULL);
	b->unref(m);
	return rc < 0 ? -1 : 0;
}

int kdt_bus_reply_error(KdtBus *b, KdtMsg *call, const char *name,
			const char *message)
{
	SdError e = { name, message, 0 };

	return b && b->reply_error(call, &e) >= 0 ? 1 : -1;
}
