/*
 * A com.canonical.dbusmenu SERVER with a fixed tree, for kdos-traymenu.
 *
 * The one thing a golden of that surface cannot be taken without: a real
 * nested `(ia{sv}av)` reply, built by a real sd-bus and read by the real
 * reader. Everything the parser can get wrong is in this one tree —
 * a mnemonic underscore, a separator, a disabled row, a submenu with children,
 * two toggle states, a row marked invisible that must not be drawn, an
 * `icon-name`, a raw `icon-data` PNG, and a `shortcut`.
 *
 * THE TWO NESTED PROPERTIES ARE WHY THIS FIXTURE EXISTS AT ALL. `icon-data`
 * is `ay` and `shortcut` is `aas` — an array of arrays of strings — and a
 * reader that enters one of those containers wrongly does not fail on that
 * property, it fails on the NEXT one, because the message is left out of
 * step. Only a real message can catch that.
 *
 * IT OWNS A NAME ON THE SESSION BUS and answers three methods. `Event` writes
 * the id it was given to stdout, which is how the click half is asserted
 * without a display: the surface is a separate process and its only outward
 * effect is that call.
 *
 * Not shipped. testing/selftest.sh compiles it and nothing else does.
 */
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#if defined(__has_include)
#  if __has_include(<basu/sd-bus.h>)
#    include <basu/sd-bus.h>
#  else
#    include <systemd/sd-bus.h>
#  endif
#else
#  include <basu/sd-bus.h>
#endif

#define IFACE "com.canonical.dbusmenu"
#define PATH  "/MenuBar"
#define NAME  "org.kdos.test.TrayMenu"

/* One row of the tree, flat, with its parent. The reply is built by walking
 * this twice per level, which is what keeps the fixture readable. */
struct row {
	int id;
	int parent;
	const char *label;	/* NULL for a separator                    */
	int enabled;
	int visible;
	int submenu;
	const char *toggle;	/* NULL, "checkmark" or "radio"            */
	int state;		/* -2 for "does not publish one"           */
	const char *icon;	/* `icon-name`, or NULL                    */
	int png;		/* send `icon-data` on this row            */
	const char *chord;	/* one `shortcut` sequence, "+"-separated  */
};

/*
 * A 2x2 RGBA PNG, so `icon-data` is a real blob and not one the reader
 * happens to skip.
 *
 * GENERATE IT, NEVER TYPE IT — a PNG carries a CRC per chunk and a zlib
 * stream, and bytes written by hand satisfy neither.
 *
 * NOTHING HERE DECODES IT. A dump turns icons off, so no frame this fixture
 * feeds ever asks libpng anything; testing/fixtures/iconpng is what asserts
 * these bytes are a picture, against the same array.
 *
 * Kept as bytes rather than read from disk: a fixture that reads from disk is
 * a fixture with a path in it.
 */
static const unsigned char PNG1[] = {
	0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a, 0x00, 0x00, 0x00, 0x0d,
	0x49, 0x48, 0x44, 0x52, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x02,
	0x08, 0x06, 0x00, 0x00, 0x00, 0x72, 0xb6, 0x0d, 0x24, 0x00, 0x00, 0x00,
	0x13, 0x49, 0x44, 0x41, 0x54, 0x78, 0xda, 0x63, 0xf8, 0xcf, 0xc0, 0xf0,
	0x1f, 0x0c, 0x81, 0x34, 0x08, 0x34, 0x00, 0x00, 0x49, 0x49, 0x09, 0x78,
	0x9c, 0x51, 0x17, 0x92, 0x00, 0x00, 0x00, 0x00, 0x49, 0x45, 0x4e, 0x44,
	0xae, 0x42, 0x60, 0x82,
};

static const struct row ROWS[] = {
	/*                                              icon-name  png chord */
	{ 1, 0, "_Open Web GUI",  1, 1, 0, NULL,        -2,
	  "web-browser",  0, NULL },
	{ 2, 0, "Folders",        1, 1, 1, NULL,        -2,
	  "folder",       0, NULL },
	{ 3, 0, NULL,             1, 1, 0, NULL,        -2,
	  NULL,           0, NULL },
	{ 4, 0, "_Pause",         0, 1, 0, NULL,        -2,
	  NULL,           0, "Control+Shift+P" },
	{ 5, 0, "Never shown",    1, 0, 0, NULL,        -2,
	  NULL,           0, NULL },
	/* The one row with a picture of its own AND a chord: `icon-data`
	 * must be read past before `shortcut` is reached. */
	{ 6, 0, "_Quit",          1, 1, 0, NULL,        -2,
	  NULL,           1, "Control+Q" },
	{ 7, 2, "Documents",      1, 1, 0, "checkmark",  1,
	  NULL,           0, NULL },
	{ 8, 2, "Pictures",       1, 1, 0, "checkmark",  0,
	  NULL,           0, NULL },
	{ 9, 2, "Music",          1, 1, 0, "checkmark",  0,
	  NULL,           0, NULL },
};
#define NROWS ((int)(sizeof(ROWS) / sizeof(ROWS[0])))

static int put_bool(sd_bus_message *m, const char *key, int v)
{
	int r = sd_bus_message_open_container(m, 'e', "sv");

	if (r < 0)
		return r;
	if ((r = sd_bus_message_append_basic(m, 's', key)) < 0 ||
	    (r = sd_bus_message_open_container(m, 'v', "b")) < 0 ||
	    (r = sd_bus_message_append_basic(m, 'b', &v)) < 0)
		return r;
	sd_bus_message_close_container(m);
	return sd_bus_message_close_container(m);
}

static int put_str(sd_bus_message *m, const char *key, const char *v)
{
	int r = sd_bus_message_open_container(m, 'e', "sv");

	if (r < 0)
		return r;
	if ((r = sd_bus_message_append_basic(m, 's', key)) < 0 ||
	    (r = sd_bus_message_open_container(m, 'v', "s")) < 0 ||
	    (r = sd_bus_message_append_basic(m, 's', v)) < 0)
		return r;
	sd_bus_message_close_container(m);
	return sd_bus_message_close_container(m);
}

static int put_int(sd_bus_message *m, const char *key, int v)
{
	int r = sd_bus_message_open_container(m, 'e', "sv");

	if (r < 0)
		return r;
	if ((r = sd_bus_message_append_basic(m, 's', key)) < 0 ||
	    (r = sd_bus_message_open_container(m, 'v', "i")) < 0 ||
	    (r = sd_bus_message_append_basic(m, 'i', &v)) < 0)
		return r;
	sd_bus_message_close_container(m);
	return sd_bus_message_close_container(m);
}

/* `ay`, written the way an application's toolkit writes it. */
static int put_bytes(sd_bus_message *m, const char *key,
		     const unsigned char *v, size_t n)
{
	int r = sd_bus_message_open_container(m, 'e', "sv");

	if (r < 0)
		return r;
	if ((r = sd_bus_message_append_basic(m, 's', key)) < 0 ||
	    (r = sd_bus_message_open_container(m, 'v', "ay")) < 0 ||
	    (r = sd_bus_message_append_array(m, 'y', v, n)) < 0)
		return r;
	sd_bus_message_close_container(m);
	return sd_bus_message_close_container(m);
}

/*
 * `aas` — ONE sequence, written as the list of one that the property is. The
 * spec allows several and this sends one, because what the reader has to get
 * right is the nesting, not the count.
 */
static int put_chord(sd_bus_message *m, const char *key, const char *v)
{
	char buf[64], *tok, *save = NULL;
	int r = sd_bus_message_open_container(m, 'e', "sv");

	if (r < 0)
		return r;
	if ((r = sd_bus_message_append_basic(m, 's', key)) < 0 ||
	    (r = sd_bus_message_open_container(m, 'v', "aas")) < 0 ||
	    (r = sd_bus_message_open_container(m, 'a', "as")) < 0 ||
	    (r = sd_bus_message_open_container(m, 'a', "s")) < 0)
		return r;
	snprintf(buf, sizeof(buf), "%s", v);
	for (tok = strtok_r(buf, "+", &save); tok;
	     tok = strtok_r(NULL, "+", &save))
		if ((r = sd_bus_message_append_basic(m, 's', tok)) < 0)
			return r;
	sd_bus_message_close_container(m);
	sd_bus_message_close_container(m);
	sd_bus_message_close_container(m);
	return sd_bus_message_close_container(m);
}

static int put_item(sd_bus_message *m, int id);

/* The `av` of children of `id`. Every level is written even when it is empty:
 * the signature has the array in it whether or not anything is in the array. */
static int put_children(sd_bus_message *m, int id)
{
	int r = sd_bus_message_open_container(m, 'a', "v");

	if (r < 0)
		return r;
	for (int i = 0; i < NROWS; i++) {
		if (ROWS[i].parent != id)
			continue;
		r = sd_bus_message_open_container(m, 'v', "(ia{sv}av)");
		if (r < 0)
			return r;
		if ((r = put_item(m, ROWS[i].id)) < 0)
			return r;
		sd_bus_message_close_container(m);
	}
	return sd_bus_message_close_container(m);
}

static int put_item(sd_bus_message *m, int id)
{
	const struct row *row = NULL;
	int r;

	for (int i = 0; i < NROWS; i++)
		if (ROWS[i].id == id)
			row = &ROWS[i];

	r = sd_bus_message_open_container(m, 'r', "ia{sv}av");
	if (r < 0)
		return r;
	if ((r = sd_bus_message_append_basic(m, 'i', &id)) < 0)
		return r;
	if ((r = sd_bus_message_open_container(m, 'a', "{sv}")) < 0)
		return r;
	if (row) {
		if (row->label)
			put_str(m, "label", row->label);
		else
			put_str(m, "type", "separator");
		if (!row->enabled)
			put_bool(m, "enabled", 0);
		if (!row->visible)
			put_bool(m, "visible", 0);
		if (row->submenu)
			put_str(m, "children-display", "submenu");
		if (row->toggle) {
			put_str(m, "toggle-type", row->toggle);
			if (row->state != -2)
				put_int(m, "toggle-state", row->state);
		}
		if (row->icon)
			put_str(m, "icon-name", row->icon);
		if (row->png)
			put_bytes(m, "icon-data", PNG1, sizeof(PNG1));
		if (row->chord)
			put_chord(m, "shortcut", row->chord);
	} else {
		/* The root. `children-display` on it is what a real menu
		 * publishes and what nothing here reads; it is sent so the
		 * reply is the shape an application's is. */
		put_str(m, "children-display", "submenu");
	}
	sd_bus_message_close_container(m);
	if ((r = put_children(m, id)) < 0)
		return r;
	return sd_bus_message_close_container(m);
}

static int on_get_layout(sd_bus_message *m, void *u, sd_bus_error *e)
{
	sd_bus_message *reply = NULL;
	int parent = 0, depth = 0, r;

	(void)u;
	(void)e;
	if (sd_bus_message_read(m, "ii", &parent, &depth) < 0)
		return -EINVAL;
	/* The property list is read and ignored: this fixture sends every
	 * property it has, which is what an application that does not
	 * implement the filter does too. */
	sd_bus_message_skip(m, "as");

	r = sd_bus_message_new_method_return(m, &reply);
	if (r < 0)
		return r;
	if ((r = sd_bus_message_append(reply, "u", (uint32_t)1)) < 0 ||
	    (r = put_item(reply, parent)) < 0) {
		sd_bus_message_unref(reply);
		return r;
	}
	r = sd_bus_send(NULL, reply, NULL);
	sd_bus_message_unref(reply);
	return r < 0 ? r : 1;
}

static int on_about_to_show(sd_bus_message *m, void *u, sd_bus_error *e)
{
	(void)u;
	(void)e;
	printf("ABOUTTOSHOW\n");
	fflush(stdout);
	return sd_bus_reply_method_return(m, "b", 0);
}

static int on_event(sd_bus_message *m, void *u, sd_bus_error *e)
{
	const char *ev = NULL;
	int id = 0;

	(void)u;
	(void)e;
	if (sd_bus_message_read(m, "is", &id, &ev) >= 0) {
		printf("EVENT %d %s\n", id, ev ? ev : "");
		fflush(stdout);
	}
	return sd_bus_reply_method_return(m, "");
}

static const sd_bus_vtable VT[] = {
	SD_BUS_VTABLE_START(0),
	SD_BUS_METHOD("GetLayout", "iias", "u(ia{sv}av)", on_get_layout,
		      SD_BUS_VTABLE_UNPRIVILEGED),
	SD_BUS_METHOD("AboutToShow", "i", "b", on_about_to_show,
		      SD_BUS_VTABLE_UNPRIVILEGED),
	SD_BUS_METHOD("Event", "isvu", "", on_event,
		      SD_BUS_VTABLE_UNPRIVILEGED),
	SD_BUS_VTABLE_END
};

int main(int argc, char **argv)
{
	sd_bus *bus = NULL;
	sd_bus_slot *slot = NULL;
	/* Seconds to serve, so a run that nothing connects to still ends. */
	int secs = argc > 1 ? atoi(argv[1]) : 5;

	if (sd_bus_open_user(&bus) < 0) {
		fprintf(stderr, "menustub: no session bus\n");
		return 1;
	}
	if (sd_bus_add_object_vtable(bus, &slot, PATH, IFACE, VT, NULL) < 0 ||
	    sd_bus_request_name(bus, NAME, 0) < 0) {
		fprintf(stderr, "menustub: cannot serve %s\n", NAME);
		sd_bus_unref(bus);
		return 1;
	}
	printf("READY %s %s\n", NAME, PATH);
	fflush(stdout);
	for (int i = 0; i < secs * 100; i++) {
		if (sd_bus_process(bus, NULL) > 0)
			continue;
		sd_bus_wait(bus, 10000);
	}
	sd_bus_slot_unref(slot);
	sd_bus_unref(bus);
	return 0;
}
