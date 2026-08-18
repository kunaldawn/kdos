/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   the tray — a StatusNotifierItem host, drawn as cells
 *
 * The tray is the one part of a desktop that is pure D-Bus. There is no Wayland
 * protocol for it and there never was: an app announces an ITEM on the bus, a
 * WATCHER keeps the list, and a HOST displays it. KDOS is all three, because
 * nothing else here is.
 *
 * Why it matters more on KDOS than elsewhere: every app is in a box, and a
 * boxed app that minimises to a tray which does not exist has minimised to
 * nowhere. kdeconnect, keepassxc, nextcloud and syncthing all do this.
 *
 * Three things this file refuses to do:
 *
 *   - Block the panel. Every property read has a 300 ms ceiling and every
 *     method call to an item is FIRE AND FORGET. A tray icon whose app has
 *     wedged must cost a redraw, never the clock.
 *   - Render an icon. The panel is a character grid; an item is one cell
 *     carrying the first letter of its Id, coloured by its status. That is a
 *     deliberate loss — see the identity argument in KDOS-DESKTOP.md — and it
 *     is also why `Id` is read rather than `IconName`: a letter from a name a
 *     human chose beats a letter from a theme lookup that will never happen.
 *   - Draw the item's MENU. `com.canonical.dbusmenu` is a second protocol with
 *     a nested-variant layout tree, and rendering it as cells is its own piece
 *     of work. Right-click sends ContextMenu, which is what the spec says to
 *     do and what an app with its own menu window answers correctly. Apps that
 *     set ItemIsMenu expect the host to draw the menu and will do nothing —
 *     that is the known gap, stated rather than hidden.
 */

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

/* KDOS ships basu; a development host usually has libsystemd, whose sd-bus is
 * the same API. Same selection notifyd.c makes, and for the same reason: this
 * has to be runnable somewhere other than KDOS. */
#if defined(__has_include)
#  if __has_include(<basu/sd-bus.h>)
#    include <basu/sd-bus.h>
#  else
#    include <systemd/sd-bus.h>
#  endif
#else
#  include <basu/sd-bus.h>
#endif

#include "shell.h"

#define WATCHER_NAME  "org.kde.StatusNotifierWatcher"
#define WATCHER_PATH  "/StatusNotifierWatcher"
#define WATCHER_IFACE "org.kde.StatusNotifierWatcher"
#define SNI_IFACE_KDE "org.kde.StatusNotifierItem"
#define SNI_IFACE_FDO "org.freedesktop.StatusNotifierItem"

/* A property read that outlives this is a broken app, not a slow bus. */
#define PROP_TIMEOUT_US 300000

/* How long one dispatch may spend reading properties. See sh_tray_dispatch. */
#define REFRESH_BUDGET_MS 100

struct sh_tray {
	sd_bus *bus;
	sd_bus_slot *vtable_slot;
	sd_bus_slot *owner_slot;
	sd_bus_slot *item_slot;
	sd_bus_slot *watcher_slot;
	int own_watcher;		/* we own the watcher name */
	int adopt_pending;		/* a new foreign watcher appeared */
	char uniq[64];			/* our own unique bus name */
	char host[64];			/* our per-process host name */
	struct sh_tray_item items[SH_MAX_TRAY];
	int nitems;
	/* Where the next property read starts. A scan from zero refreshed the
	 * first eligible item and stopped, and an app that animates its icon
	 * (nextcloud syncing, syncthing) sets needs_props several times a
	 * second — so items after it were never read at all. */
	int refresh_cur;
};

/* ── item bookkeeping ──────────────────────────────────────────────────── */

/* Monotonic, for the refresh budget: `time()` is a second's resolution and the
 * budget is a tenth of one. */
static int64_t now_ms(void)
{
	struct timespec ts;

	clock_gettime(CLOCK_MONOTONIC, &ts);
	return (int64_t)ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}

static struct sh_tray_item *find_item(struct sh_tray *t, const char *service,
				      const char *path)
{
	for (int i = 0; i < t->nitems; i++)
		if (!strcmp(t->items[i].service, service) &&
		    (!path || !strcmp(t->items[i].path, path)))
			return &t->items[i];
	return NULL;
}

static void drop_item(struct sh_tray *t, int i)
{
	if (i < 0 || i >= t->nitems)
		return;
	char gone[SH_TRAY_NAME];
	snprintf(gone, sizeof(gone), "%s", t->items[i].service);
	memmove(&t->items[i], &t->items[i + 1],
		(size_t)(t->nitems - i - 1) * sizeof(t->items[0]));
	t->nitems--;
	/* The array moved under the cursor. */
	if (t->refresh_cur > i)
		t->refresh_cur--;
	if (t->own_watcher) {
		sd_bus_emit_signal(t->bus, WATCHER_PATH, WATCHER_IFACE,
				   "StatusNotifierItemUnregistered", "s", gone);
		sd_bus_emit_properties_changed(t->bus, WATCHER_PATH,
					       WATCHER_IFACE,
					       "RegisteredStatusNotifierItems",
					       NULL);
	}
}

/* ── reading an item ───────────────────────────────────────────────────── */

static void copy_str(char *dst, size_t len, const char *src)
{
	if (!src)
		return;
	snprintf(dst, len, "%s", src);
}

/*
 * One GetAll rather than five Gets.
 *
 * Not a micro-optimisation: each call carries the timeout, so five of them
 * against a wedged app is a second and a half of dead panel where one is
 * 300 ms.
 */
static int getall(struct sh_tray *t, struct sh_tray_item *it, const char *iface)
{
	sd_bus_message *m = NULL, *reply = NULL;
	sd_bus_error err = SD_BUS_ERROR_NULL;
	int r, nkeys = 0;

	r = sd_bus_message_new_method_call(t->bus, &m, it->service, it->path,
					   "org.freedesktop.DBus.Properties",
					   "GetAll");
	if (r < 0)
		goto out;
	r = sd_bus_message_append(m, "s", iface);
	if (r < 0)
		goto out;
	r = sd_bus_call(t->bus, m, PROP_TIMEOUT_US, &err, &reply);
	if (r < 0)
		goto out;

	r = sd_bus_message_enter_container(reply, 'a', "{sv}");
	if (r < 0)
		goto out;
	while (sd_bus_message_enter_container(reply, 'e', "sv") > 0) {
		const char *key = NULL, *contents = NULL, *val = NULL;
		char type = 0;

		if (sd_bus_message_read_basic(reply, 's', &key) < 0)
			break;
		if (sd_bus_message_peek_type(reply, &type, &contents) < 0)
			break;
		nkeys++;

		if (contents && !strcmp(contents, "s") &&
		    sd_bus_message_enter_container(reply, 'v', "s") > 0) {
			sd_bus_message_read_basic(reply, 's', &val);
			if (!strcmp(key, "Id"))
				copy_str(it->id, sizeof(it->id), val);
			else if (!strcmp(key, "Title"))
				copy_str(it->title, sizeof(it->title), val);
			else if (!strcmp(key, "IconName"))
				copy_str(it->icon, sizeof(it->icon), val);
			else if (!strcmp(key, "Status") && val)
				it->status = !strcmp(val, "NeedsAttention")
						     ? SH_TRAY_ATTENTION
					     : !strcmp(val, "Active")
						     ? SH_TRAY_ACTIVE
						     : SH_TRAY_PASSIVE;
			sd_bus_message_exit_container(reply);
		} else if (contents && !strcmp(contents, "b") &&
			   sd_bus_message_enter_container(reply, 'v', "b") > 0) {
			int b = 0;
			sd_bus_message_read_basic(reply, 'b', &b);
			if (!strcmp(key, "ItemIsMenu"))
				it->is_menu = b;
			sd_bus_message_exit_container(reply);
		} else {
			sd_bus_message_skip(reply, "v");
		}
		sd_bus_message_exit_container(reply);
	}
	sd_bus_message_exit_container(reply);
	/*
	 * An EMPTY dict is not an answer. Several bindings reply to a GetAll for
	 * an interface the object does not implement with a well-formed empty
	 * a{sv} rather than an error, and counting that as success is what stops
	 * the probe below ever trying the other spelling — which is the exact
	 * failure it exists to fix, keyed on "the call did not error" instead of
	 * "the interface answered". Every SNI item publishes Id and Status.
	 */
	r = nkeys > 0 ? 0 : -1;
out:
	sd_bus_error_free(&err);
	sd_bus_message_unref(m);
	sd_bus_message_unref(reply);
	return r;
}

/*
 * KDE apps publish org.kde.StatusNotifierItem and a few publish the freedesktop
 * spelling. Try the one we believe in, then the other — and only record the
 * switch when the OTHER one actually answered. Recording it on the first
 * failure instead is what made every click go to an interface the app did not
 * implement: the properties came back empty and Activate went nowhere, silently,
 * because a fire-and-forget call has nothing to report.
 */
static void refresh_item(struct sh_tray *t, struct sh_tray_item *it)
{
	const char *first = it->fdo_iface ? SNI_IFACE_FDO : SNI_IFACE_KDE;
	const char *second = it->fdo_iface ? SNI_IFACE_KDE : SNI_IFACE_FDO;

	if (getall(t, it, first) == 0) {
		it->needs_props = 0;
		it->prop_fails = 0;
	} else if (getall(t, it, second) == 0) {
		it->fdo_iface = !it->fdo_iface;
		it->needs_props = 0;
		it->prop_fails = 0;
	} else {
		/*
		 * BOTH failed: keep needs_props and back off. Clearing it here
		 * left an item registered while the app was still starting up
		 * with no name for the rest of the session; retrying every
		 * dispatch against a wedged app is 600 ms of dead panel per
		 * second. Doubling up to about a minute is the middle.
		 */
		if (it->prop_fails < 6)
			it->prop_fails++;
		it->next_try = time(NULL) + (1 << it->prop_fails);
	}

	/* Something to draw, whatever the app told us. An item with no Id and
	 * no Title still exists and still has to be clickable. */
	if (!it->id[0])
		copy_str(it->id, sizeof(it->id),
			 it->title[0] ? it->title : it->service);
}

/*
 * An adopted item arrives with no sender, so its `owner` starts as the service
 * name — which is a WELL-KNOWN name, and the change signals every item sends
 * carry the UNIQUE one. Matching against the wrong spelling meant an adopted
 * item never updated. The unique name is asked for asynchronously; the reply
 * finds the item by service, because items memmove and a pointer would not
 * survive a drop in between.
 */
struct owner_lookup {
	struct sh_tray *t;
	char service[SH_TRAY_NAME];
};

static int on_get_name_owner(sd_bus_message *m, void *userdata,
			     sd_bus_error *err)
{
	struct owner_lookup *l = userdata;
	const char *owner = NULL;
	(void)err;

	if (!sd_bus_message_is_method_error(m, NULL) &&
	    sd_bus_message_read(m, "s", &owner) >= 0 && owner && *owner) {
		struct sh_tray_item *it = find_item(l->t, l->service, NULL);
		if (it)
			copy_str(it->owner, sizeof(it->owner), owner);
	}
	free(l);
	return 0;
}

static void resolve_owner(struct sh_tray *t, const char *service)
{
	if (service[0] == ':')
		return;			/* already a unique name */
	struct owner_lookup *l = calloc(1, sizeof(*l));
	if (!l)
		return;
	l->t = t;
	copy_str(l->service, sizeof(l->service), service);
	if (sd_bus_call_method_async(t->bus, NULL, "org.freedesktop.DBus",
				     "/org/freedesktop/DBus",
				     "org.freedesktop.DBus", "GetNameOwner",
				     on_get_name_owner, l, "s", service) < 0)
		free(l);
}

/*
 * "service", "service/path" and "/path" all mean an item, and which spelling
 * arrives depends on the toolkit: Qt/KDE send the service name, libappindicator
 * sends "/StatusNotifierItem" and means "me, the sender". Getting this wrong is
 * why a tray shows KDE apps and not GTK ones.
 */
static struct sh_tray_item *add_item(struct sh_tray *t, const char *arg,
				     const char *sender)
{
	char service[SH_TRAY_NAME], path[SH_TRAY_NAME];

	if (!arg || !*arg)
		return NULL;
	if (arg[0] == '/') {
		if (!sender)
			return NULL;
		snprintf(service, sizeof(service), "%s", sender);
		snprintf(path, sizeof(path), "%s", arg);
	} else {
		const char *slash = strchr(arg, '/');
		if (slash) {
			size_t n = (size_t)(slash - arg);
			if (n >= sizeof(service))
				n = sizeof(service) - 1;
			memcpy(service, arg, n);
			service[n] = 0;
			snprintf(path, sizeof(path), "%s", slash);
		} else {
			snprintf(service, sizeof(service), "%s", arg);
			snprintf(path, sizeof(path), "%s",
				 "/StatusNotifierItem");
		}
	}

	struct sh_tray_item *it = find_item(t, service, path);
	if (it)
		return it;
	if (t->nitems >= SH_MAX_TRAY)
		return NULL;

	it = &t->items[t->nitems];
	memset(it, 0, sizeof(*it));
	copy_str(it->service, sizeof(it->service), service);
	copy_str(it->path, sizeof(it->path), path);
	/* No sender means we adopted this from another watcher and do not know
	 * the unique name behind it yet; the service name stands in until the
	 * async GetNameOwner below answers. */
	copy_str(it->owner, sizeof(it->owner), sender ? sender : service);
	if (!sender)
		resolve_owner(t, service);
	/*
	 * Deferred, never read here: add_item runs inside a bus callback, and a
	 * SYNCHRONOUS call from inside one does not get its reply — sd-bus is
	 * already processing a message. That is why the first version of this
	 * file drew items with no name at all.
	 */
	it->needs_props = 1;
	t->nitems++;
	return it;
}

/* ── the item's own signals ────────────────────────────────────────────── */

static int on_item_signal(sd_bus_message *m, void *userdata,
			  sd_bus_error *err)
{
	struct sh_tray *t = userdata;
	const char *sender = sd_bus_message_get_sender(m);
	(void)err;

	if (!sender)
		return 0;
	for (int i = 0; i < t->nitems; i++)
		if (!strcmp(t->items[i].owner, sender) ||
		    !strcmp(t->items[i].service, sender))
			t->items[i].needs_props = 1;
	return 0;
}

/*
 * An app that died still has an entry in our list until the bus tells us its
 * name went away. Without this the panel keeps a dead letter forever and a
 * click on it goes to a name nobody owns.
 *
 * The same signal is how the WATCHER role changes hands: the name was
 * requested with SD_BUS_NAME_QUEUE, so when a foreign watcher (waybar, say)
 * exits, the bus hands the name to us and announces it here — without this
 * the tray was dead from that moment until the panel was restarted.
 */
static int on_name_owner_changed(sd_bus_message *m, void *userdata,
				 sd_bus_error *err)
{
	struct sh_tray *t = userdata;
	const char *name = NULL, *old = NULL, *new = NULL;
	(void)err;

	if (sd_bus_message_read(m, "sss", &name, &old, &new) < 0)
		return 0;
	if (!name)
		return 0;
	if (!strcmp(name, WATCHER_NAME)) {
		int mine = new && *new && !strcmp(new, t->uniq);
		if (mine && !t->own_watcher) {
			t->own_watcher = 1;
			/* Apps re-register when the watcher changes hands, but
			 * only if told there is a host behind the new one. */
			sd_bus_emit_signal(t->bus, WATCHER_PATH, WATCHER_IFACE,
					   "StatusNotifierHostRegistered", "");
		} else if (!mine && new && *new) {
			/* Deferred to the next dispatch: adopting reads the
			 * new watcher's list SYNCHRONOUSLY, and a sync call
			 * from inside a bus callback never gets its reply. */
			t->own_watcher = 0;
			t->adopt_pending = 1;
		}
		return 0;
	}
	if (new && *new)
		return 0;
	for (int i = t->nitems - 1; i >= 0; i--)
		if (!strcmp(t->items[i].service, name) ||
		    !strcmp(t->items[i].owner, name))
			drop_item(t, i);
	return 0;
}

/* ── the watcher ───────────────────────────────────────────────────────── */

static int method_register_item(sd_bus_message *m, void *userdata,
				sd_bus_error *ret_error)
{
	struct sh_tray *t = userdata;
	const char *arg = NULL;
	int r = sd_bus_message_read(m, "s", &arg);
	(void)ret_error;

	if (r < 0)
		return sd_bus_reply_method_return(m, NULL);

	struct sh_tray_item *it = add_item(t, arg, sd_bus_message_get_sender(m));
	if (it) {
		sd_bus_emit_signal(t->bus, WATCHER_PATH, WATCHER_IFACE,
				   "StatusNotifierItemRegistered", "s",
				   it->service);
		sd_bus_emit_properties_changed(t->bus, WATCHER_PATH,
					       WATCHER_IFACE,
					       "RegisteredStatusNotifierItems",
					       NULL);
	}
	return sd_bus_reply_method_return(m, NULL);
}

static int method_register_host(sd_bus_message *m, void *userdata,
				sd_bus_error *ret_error)
{
	struct sh_tray *t = userdata;
	(void)ret_error;
	sd_bus_emit_signal(t->bus, WATCHER_PATH, WATCHER_IFACE,
			   "StatusNotifierHostRegistered", "");
	return sd_bus_reply_method_return(m, NULL);
}

static int prop_items(sd_bus *bus, const char *path, const char *iface,
		      const char *prop, sd_bus_message *reply, void *userdata,
		      sd_bus_error *ret_error)
{
	struct sh_tray *t = userdata;
	int r;
	(void)bus; (void)path; (void)iface; (void)prop; (void)ret_error;

	r = sd_bus_message_open_container(reply, 'a', "s");
	if (r < 0)
		return r;
	for (int i = 0; i < t->nitems; i++) {
		char entry[SH_TRAY_NAME * 2];
		snprintf(entry, sizeof(entry), "%s%s", t->items[i].service,
			 t->items[i].path);
		r = sd_bus_message_append_basic(reply, 's', entry);
		if (r < 0)
			return r;
	}
	return sd_bus_message_close_container(reply);
}

static int prop_host_registered(sd_bus *bus, const char *path,
				const char *iface, const char *prop,
				sd_bus_message *reply, void *userdata,
				sd_bus_error *ret_error)
{
	int yes = 1;
	(void)bus; (void)path; (void)iface; (void)prop; (void)userdata;
	(void)ret_error;
	return sd_bus_message_append_basic(reply, 'b', &yes);
}

static int prop_version(sd_bus *bus, const char *path, const char *iface,
			const char *prop, sd_bus_message *reply, void *userdata,
			sd_bus_error *ret_error)
{
	int v = 0;
	(void)bus; (void)path; (void)iface; (void)prop; (void)userdata;
	(void)ret_error;
	return sd_bus_message_append_basic(reply, 'i', &v);
}

static const sd_bus_vtable watcher_vtable[] = {
	SD_BUS_VTABLE_START(0),
	SD_BUS_METHOD("RegisterStatusNotifierItem", "s", NULL,
		      method_register_item, SD_BUS_VTABLE_UNPRIVILEGED),
	SD_BUS_METHOD("RegisterStatusNotifierHost", "s", NULL,
		      method_register_host, SD_BUS_VTABLE_UNPRIVILEGED),
	SD_BUS_PROPERTY("RegisteredStatusNotifierItems", "as", prop_items, 0,
			SD_BUS_VTABLE_PROPERTY_EMITS_CHANGE),
	SD_BUS_PROPERTY("IsStatusNotifierHostRegistered", "b",
			prop_host_registered, 0,
			SD_BUS_VTABLE_PROPERTY_EMITS_CHANGE),
	SD_BUS_PROPERTY("ProtocolVersion", "i", prop_version, 0,
			SD_BUS_VTABLE_PROPERTY_CONST),
	SD_BUS_SIGNAL("StatusNotifierItemRegistered", "s", 0),
	SD_BUS_SIGNAL("StatusNotifierItemUnregistered", "s", 0),
	SD_BUS_SIGNAL("StatusNotifierHostRegistered", "", 0),
	SD_BUS_SIGNAL("StatusNotifierHostUnregistered", "", 0),
	SD_BUS_VTABLE_END,
};

/* ── being a host under somebody else's watcher ────────────────────────── */

/*
 * We are not always the watcher. Another host may own the name — waybar, or a
 * second kdos-shell running `--dump` beside the live panel — and the protocol's
 * whole point is that a host works either way. So when the name is taken, read
 * the list from whoever has it and follow its signals.
 *
 * Without this, `kdos-shell --dump` in a live session draws an EMPTY tray while
 * the panel two inches above it shows four items, which is exactly the kind of
 * quietly wrong output a dump exists to not produce.
 */
static int on_watcher_signal(sd_bus_message *m, void *userdata,
			     sd_bus_error *err)
{
	struct sh_tray *t = userdata;
	const char *arg = NULL;
	(void)err;

	if (sd_bus_message_read(m, "s", &arg) < 0 || !arg)
		return 0;
	if (sd_bus_message_is_signal(m, WATCHER_IFACE,
				     "StatusNotifierItemRegistered")) {
		add_item(t, arg, NULL);
	} else {
		for (int i = t->nitems - 1; i >= 0; i--)
			if (!strcmp(t->items[i].service, arg))
				drop_item(t, i);
	}
	return 0;
}

static void adopt_items(struct sh_tray *t)
{
	sd_bus_message *reply = NULL;
	sd_bus_error err = SD_BUS_ERROR_NULL;
	const char *entry = NULL;

	if (sd_bus_call_method(t->bus, WATCHER_NAME, WATCHER_PATH,
			       "org.freedesktop.DBus.Properties", "Get", &err,
			       &reply, "ss", WATCHER_IFACE,
			       "RegisteredStatusNotifierItems") < 0)
		goto out;
	if (sd_bus_message_enter_container(reply, 'v', "as") < 0)
		goto out;
	if (sd_bus_message_enter_container(reply, 'a', "s") < 0)
		goto out;
	while (sd_bus_message_read_basic(reply, 's', &entry) > 0)
		add_item(t, entry, NULL);
	sd_bus_message_exit_container(reply);
	sd_bus_message_exit_container(reply);
out:
	sd_bus_error_free(&err);
	sd_bus_message_unref(reply);
}

/* ── the public half ───────────────────────────────────────────────────── */

int sh_tray_init(struct sh_state *sh)
{
	struct sh_tray *t = calloc(1, sizeof(*t));
	char host[64];
	int r;

	if (!t)
		return -1;
	sh->tray = t;

	r = sd_bus_open_user(&t->bus);
	if (r < 0)
		goto fail;

	r = sd_bus_add_object_vtable(t->bus, &t->vtable_slot, WATCHER_PATH,
				     WATCHER_IFACE, watcher_vtable, t);
	if (r < 0)
		goto fail;

	const char *uniq = NULL;
	if (sd_bus_get_unique_name(t->bus, &uniq) >= 0 && uniq)
		copy_str(t->uniq, sizeof(t->uniq), uniq);

	/*
	 * Requesting the watcher name is allowed to FAIL. Another host (a
	 * waybar the user prefers) may already own it, and then we are a host
	 * and not the watcher — which is the protocol working as designed, not
	 * an error worth refusing to start over. But we QUEUE for the name, so
	 * that watcher exiting hands the role to us (announced through
	 * NameOwnerChanged) instead of leaving the session with no watcher and
	 * a tray that is dead until the panel restarts.
	 */
	r = sd_bus_request_name(t->bus, WATCHER_NAME, 0);
	t->own_watcher = r >= 0;
	if (!t->own_watcher)
		sd_bus_request_name(t->bus, WATCHER_NAME, SD_BUS_NAME_QUEUE);

	/* The host name is per-process by spec, and owning it is what an app
	 * checks before it bothers to publish an item at all. */
	snprintf(host, sizeof(host), "org.kde.StatusNotifierHost-%d",
		 (int)getpid());
	copy_str(t->host, sizeof(t->host), host);
	sd_bus_request_name(t->bus, host, 0);
	if (t->own_watcher) {
		sd_bus_emit_signal(t->bus, WATCHER_PATH, WATCHER_IFACE,
				   "StatusNotifierHostRegistered", "");
	} else {
		sd_bus_call_method_async(t->bus, NULL, WATCHER_NAME,
					 WATCHER_PATH, WATCHER_IFACE,
					 "RegisterStatusNotifierHost", NULL,
					 NULL, "s", host);
		sd_bus_add_match(t->bus, &t->watcher_slot,
				 "type='signal',interface='" WATCHER_IFACE "'",
				 on_watcher_signal, t);
		adopt_items(t);
	}

	sd_bus_add_match(t->bus, &t->owner_slot,
			 "type='signal',sender='org.freedesktop.DBus',"
			 "interface='org.freedesktop.DBus',"
			 "member='NameOwnerChanged'",
			 on_name_owner_changed, t);
	/* One match for every item's change signals rather than one per item:
	 * the callback finds the item by sender anyway, and a match per item
	 * would have to be added and removed in step with the list. */
	sd_bus_add_match(t->bus, &t->item_slot,
			 "type='signal',interface='" SNI_IFACE_KDE "'",
			 on_item_signal, t);
	sd_bus_add_match(t->bus, NULL,
			 "type='signal',interface='" SNI_IFACE_FDO "'",
			 on_item_signal, t);
	return 0;

fail:
	sh_tray_free(sh);
	return -1;
}

/*
 * Called once per panel loop, which is at most once a second.
 *
 * That is the tray's whole latency budget and it is deliberate: a panel that
 * woke on every bus message would wake on every notification, every property
 * change of every app, and redraw a clock that changes once a minute. An icon
 * that appears within a second of an app starting is indistinguishable from
 * one that appears instantly.
 */
void sh_tray_dispatch(struct sh_state *sh)
{
	struct sh_tray *t = sh->tray;
	if (!t || !t->bus)
		return;
	while (sd_bus_process(t->bus, NULL) > 0)
		;
	if (t->adopt_pending) {
		/* A new foreign watcher took the name: introduce our host to
		 * it and read its list, both outside any callback. */
		t->adopt_pending = 0;
		sd_bus_call_method_async(t->bus, NULL, WATCHER_NAME,
					 WATCHER_PATH, WATCHER_IFACE,
					 "RegisterStatusNotifierHost", NULL,
					 NULL, "s", t->host);
		adopt_items(t);
	}
	/*
	 * Property reads happen HERE, outside every callback, for the reason in
	 * refresh_item's neighbour above — and on a BUDGET rather than one item
	 * per dispatch: the read has a 300 ms ceiling per interface, so a burst
	 * of sixteen wedged registrations serviced in one loop was ~10 s of dead
	 * panel. The budget is spent AFTER a read, so a burst that answers
	 * quickly drains in one tick (six items resolving one per second is six
	 * seconds of placeholder cells) while one wedged item still costs its
	 * ceiling and no more.
	 *
	 * And the scan starts from a CURSOR. A failed item's next_try pushes it
	 * past the others, but a SUCCEEDING one is eligible again the moment it
	 * emits its next NewIcon — an animated tray icon at items[0] therefore
	 * took every read for as long as it was animating.
	 */
	time_t now = time(NULL);
	int64_t started = now_ms();
	int cur = t->refresh_cur;
	for (int k = 0; k < t->nitems; k++) {
		int i = (cur + k) % t->nitems;
		if (!t->items[i].needs_props || t->items[i].next_try > now)
			continue;
		refresh_item(t, &t->items[i]);
		t->refresh_cur = (i + 1) % t->nitems;
		if (now_ms() - started >= REFRESH_BUDGET_MS)
			break;
	}
	sd_bus_flush(t->bus);
}

int sh_tray_count(const struct sh_state *sh)
{
	return sh->tray ? sh->tray->nitems : 0;
}

const struct sh_tray_item *sh_tray_get(const struct sh_state *sh, int i)
{
	if (!sh->tray || i < 0 || i >= sh->tray->nitems)
		return NULL;
	return &sh->tray->items[i];
}

/*
 * A click is fire-and-forget, and that is the rule this file exists to keep:
 * the reply carries nothing we need, and an app that takes four seconds to
 * open its window must not take the panel with it.
 */
void sh_tray_activate(struct sh_state *sh, int i, int button, int x, int y)
{
	struct sh_tray *t = sh->tray;
	const char *method;

	if (!t || i < 0 || i >= t->nitems)
		return;
	struct sh_tray_item *it = &t->items[i];

	switch (button) {
	case SH_TRAY_BTN_MIDDLE:
		method = "SecondaryActivate";
		break;
	case SH_TRAY_BTN_RIGHT:
		method = "ContextMenu";
		break;
	default:
		/* An item that says it IS a menu has no useful Activate: the
		 * spec's answer is to show the menu, and ContextMenu is the only
		 * request that asks for one. */
		method = it->is_menu ? "ContextMenu" : "Activate";
		break;
	}

	sd_bus_call_method_async(t->bus, NULL, it->service, it->path,
				 it->fdo_iface ? SNI_IFACE_FDO : SNI_IFACE_KDE,
				 method, NULL, NULL, "ii", x, y);
	sd_bus_flush(t->bus);
}

/*
 * The wheel, delivered as the spec's Scroll — a volume item (pasystray,
 * kmix) expects exactly this and nothing else the host can send changes its
 * level. Fire-and-forget for the same reason every other item call is.
 */
void sh_tray_scroll(struct sh_state *sh, int i, int delta)
{
	struct sh_tray *t = sh->tray;

	if (!t || !t->bus || i < 0 || i >= t->nitems)
		return;
	struct sh_tray_item *it = &t->items[i];
	sd_bus_call_method_async(t->bus, NULL, it->service, it->path,
				 it->fdo_iface ? SNI_IFACE_FDO : SNI_IFACE_KDE,
				 "Scroll", NULL, NULL, "is", delta, "vertical");
	sd_bus_flush(t->bus);
}

/*
 * One notification, through the connection the tray already holds — the
 * battery warning's route. Async with no reply handler: a notification must
 * never gate a frame, and there is nothing to do with the returned id.
 */
void sh_tray_notify(struct sh_state *sh, const char *summary, const char *body)
{
	struct sh_tray *t = sh->tray;

	if (!t || !t->bus)
		return;
	sd_bus_call_method_async(t->bus, NULL,
				 "org.freedesktop.Notifications",
				 "/org/freedesktop/Notifications",
				 "org.freedesktop.Notifications", "Notify",
				 NULL, NULL, "susssasa{sv}i",
				 "kdos-shell", (uint32_t)0, "", summary, body,
				 0, 0, (int32_t)-1);
	sd_bus_flush(t->bus);
}

void sh_tray_free(struct sh_state *sh)
{
	struct sh_tray *t = sh->tray;
	if (!t)
		return;
	if (t->bus) {
		sd_bus_slot_unref(t->watcher_slot);
		sd_bus_slot_unref(t->item_slot);
		sd_bus_slot_unref(t->owner_slot);
		sd_bus_slot_unref(t->vtable_slot);
		sd_bus_flush(t->bus);
		sd_bus_unref(t->bus);
	}
	free(t);
	sh->tray = NULL;
}

/*
 * The session-bus connection, for the ONE other widget that needs one.
 *
 * The MPRIS cell reads whichever player is up; opening a second connection for
 * it would mean a second fd, a second unique name and a second thing to
 * dispatch, in a process whose whole rule is that nothing blocks the frame.
 * NULL when there is no bus, which is a session with no tray and no media
 * controls rather than a panel that failed to start.
 */
void *sh_tray_bus(const struct sh_state *sh)
{
	return sh && sh->tray ? sh->tray->bus : NULL;
}
