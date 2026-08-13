/*
 * traycheck — kdos-shell's tray host, exercised against a real StatusNotifierItem
 * on a real bus.
 *
 * The fixture IS a second process, because that is the only honest shape for
 * this: SNI is a conversation between two peers on a session bus, and a test
 * that mocked either side would pass on exactly the bugs that shipped —
 * properties read from inside a bus callback (which never get their reply) and
 * a click sent to the interface spelling the app does not implement.
 *
 * The child publishes org.kde.StatusNotifierItem the way a Qt app does, waits
 * for a watcher to exist and registers with it. The parent is tray.c. Every
 * method the child receives is appended to argv[1], and the parent asserts on
 * that file.
 *
 * Run by testing/selftest.sh, which supplies the private dbus-daemon.
 */
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
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

#include "shell.h"

static const char *log_path;

static void note(const char *what)
{
	FILE *f = fopen(log_path, "a");
	if (!f)
		return;
	fprintf(f, "%s\n", what);
	fclose(f);
}

/* ── the child: one tray app ───────────────────────────────────────────── */

static int m_activate(sd_bus_message *m, void *u, sd_bus_error *e)
{
	(void)u; (void)e;
	note("Activate");
	return sd_bus_reply_method_return(m, NULL);
}
static int m_secondary(sd_bus_message *m, void *u, sd_bus_error *e)
{
	(void)u; (void)e;
	note("SecondaryActivate");
	return sd_bus_reply_method_return(m, NULL);
}
static int m_context(sd_bus_message *m, void *u, sd_bus_error *e)
{
	(void)u; (void)e;
	note("ContextMenu");
	return sd_bus_reply_method_return(m, NULL);
}

static int p_str(sd_bus *b, const char *p, const char *i, const char *prop,
		 sd_bus_message *reply, void *u, sd_bus_error *e)
{
	(void)b; (void)p; (void)i; (void)u; (void)e;
	const char *v = !strcmp(prop, "Id")         ? "KeePassXC"
			: !strcmp(prop, "Title")    ? "KeePassXC - Passwords"
			: !strcmp(prop, "Status")   ? "NeedsAttention"
			: !strcmp(prop, "IconName") ? "keepassxc"
						    : "";
	return sd_bus_message_append_basic(reply, 's', v);
}

static int p_is_menu(sd_bus *b, const char *p, const char *i, const char *prop,
		     sd_bus_message *reply, void *u, sd_bus_error *e)
{
	int no = 0;
	(void)b; (void)p; (void)i; (void)prop; (void)u; (void)e;
	return sd_bus_message_append_basic(reply, 'b', &no);
}

static const sd_bus_vtable item_vtable[] = {
	SD_BUS_VTABLE_START(0),
	SD_BUS_METHOD("Activate", "ii", NULL, m_activate,
		      SD_BUS_VTABLE_UNPRIVILEGED),
	SD_BUS_METHOD("SecondaryActivate", "ii", NULL, m_secondary,
		      SD_BUS_VTABLE_UNPRIVILEGED),
	SD_BUS_METHOD("ContextMenu", "ii", NULL, m_context,
		      SD_BUS_VTABLE_UNPRIVILEGED),
	SD_BUS_PROPERTY("Id", "s", p_str, 0, SD_BUS_VTABLE_PROPERTY_CONST),
	SD_BUS_PROPERTY("Title", "s", p_str, 0, SD_BUS_VTABLE_PROPERTY_CONST),
	SD_BUS_PROPERTY("Status", "s", p_str, 0, SD_BUS_VTABLE_PROPERTY_CONST),
	SD_BUS_PROPERTY("IconName", "s", p_str, 0, SD_BUS_VTABLE_PROPERTY_CONST),
	SD_BUS_PROPERTY("ItemIsMenu", "b", p_is_menu, 0,
			SD_BUS_VTABLE_PROPERTY_CONST),
	SD_BUS_VTABLE_END,
};

static void run_item(void)
{
	sd_bus *bus = NULL;
	char name[64];
	int registered = 0;

	if (sd_bus_open_user(&bus) < 0)
		_exit(2);
	snprintf(name, sizeof(name), "org.kde.StatusNotifierItem-%d-1",
		 (int)getpid());
	if (sd_bus_add_object_vtable(bus, NULL, "/StatusNotifierItem",
				     "org.kde.StatusNotifierItem", item_vtable,
				     NULL) < 0)
		_exit(2);
	if (sd_bus_request_name(bus, name, 0) < 0)
		_exit(2);

	/* A Qt app retries until a watcher exists; so does this. */
	for (int i = 0; i < 200; i++) {
		if (!registered &&
		    sd_bus_call_method(bus, "org.kde.StatusNotifierWatcher",
				       "/StatusNotifierWatcher",
				       "org.kde.StatusNotifierWatcher",
				       "RegisterStatusNotifierItem", NULL, NULL,
				       "s", name) >= 0)
			registered = 1;
		while (sd_bus_process(bus, NULL) > 0)
			;
		sd_bus_wait(bus, 50000);
	}
	sd_bus_unref(bus);
	_exit(0);
}

/* ── the parent: the host ──────────────────────────────────────────────── */

static int fail(const char *msg)
{
	fprintf(stderr, "traycheck: %s\n", msg);
	return 1;
}

static int log_has(const char *what)
{
	char line[128];
	FILE *f = fopen(log_path, "r");
	int found = 0;
	if (!f)
		return 0;
	while (fgets(line, sizeof(line), f)) {
		line[strcspn(line, "\n")] = 0;
		if (!strcmp(line, what))
			found = 1;
	}
	fclose(f);
	return found;
}

int main(int argc, char **argv)
{
	struct sh_state sh = {0};
	int rc = 0;

	if (argc < 2) {
		fprintf(stderr, "usage: traycheck <logfile>\n");
		return 2;
	}
	log_path = argv[1];
	unlink(log_path);

	pid_t child = fork();
	if (child == 0)
		run_item();
	if (child < 0)
		return 2;

	if (sh_tray_init(&sh) != 0) {
		kill(child, SIGTERM);
		return fail("the host could not reach the session bus");
	}

	for (int i = 0; i < 100 && sh_tray_count(&sh) < 1; i++) {
		sh_tray_dispatch(&sh);
		usleep(50000);
	}
	/* The properties are read on a LATER dispatch than the registration —
	 * deliberately, because the registration arrives inside a bus callback.
	 * A few more turns is what that costs. */
	for (int i = 0; i < 20; i++) {
		sh_tray_dispatch(&sh);
		usleep(20000);
	}

	if (sh_tray_count(&sh) != 1)
		rc = fail("the item never reached the watcher");
	else {
		const struct sh_tray_item *it = sh_tray_get(&sh, 0);
		if (strcmp(it->id, "KeePassXC"))
			rc = fail("Id was not read from the item");
		else if (it->status != SH_TRAY_ATTENTION)
			rc = fail("Status was not read from the item");
		else if (it->is_menu)
			rc = fail("ItemIsMenu came back wrong");

		sh_tray_activate(&sh, 0, SH_TRAY_BTN_LEFT, 10, 20);
		sh_tray_activate(&sh, 0, SH_TRAY_BTN_MIDDLE, 11, 21);
		sh_tray_activate(&sh, 0, SH_TRAY_BTN_RIGHT, 12, 22);
		for (int i = 0; i < 20; i++) {
			sh_tray_dispatch(&sh);
			usleep(20000);
		}
		if (!log_has("Activate"))
			rc = fail("a left click reached nothing");
		if (!log_has("SecondaryActivate"))
			rc = fail("a middle click reached nothing");
		if (!log_has("ContextMenu"))
			rc = fail("a right click reached nothing");
	}

	/* And the item going away must take its cell with it. */
	kill(child, SIGKILL);
	waitpid(child, NULL, 0);
	for (int i = 0; i < 100 && sh_tray_count(&sh) > 0; i++) {
		sh_tray_dispatch(&sh);
		usleep(20000);
	}
	if (sh_tray_count(&sh) != 0)
		rc = fail("a dead app kept its tray entry");

	sh_tray_free(&sh);
	if (!rc)
		printf("  tray: item registered, read, clicked and reaped\n");
	return rc;
}
