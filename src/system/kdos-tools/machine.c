/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   kdos timezone | hostname | locale | keymap | in-vm — the machine's name,
 *   clock and language
 *
 *   kdos timezone [Area/City]       print, or set, the time zone
 *   kdos hostname [name]            print, or set, the static host name
 *   kdos locale [NAME=value...]     print, or replace, the locale settings
 *   kdos keymap [map]               print, or set, the console keymap
 *   kdos in-vm                      exit 0 inside a virtual machine
 *
 * THE SAME SERVICES SYSTEM SETTINGS USES: timedate1, hostname1 and locale1,
 * served by kdos-settingsd, which writes the files KDOS reads (/etc/localtime,
 * /etc/hostname, /etc/locale.conf, the console keymap) and decides who may
 * change them through polkit — `interactive` is true, so a person outside
 * wheel is asked by the polkit agent rather than refused outright. A command
 * that wrote the files itself would be a second way in with its own rules.
 *
 * Exit 0 done, 1 the service refused or did not answer (its message is
 * printed: InvalidArgs means the zone, name or map is not one), 2 usage.
 * ---------------------------------
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "kdos-tools.h"

static int usage(const char *verb)
{
	if (!strcmp(verb, "timezone"))
		fprintf(stderr, "usage: kdos timezone [Area/City]\n");
	else if (!strcmp(verb, "hostname"))
		fprintf(stderr, "usage: kdos hostname [name]\n");
	else if (!strcmp(verb, "locale"))
		fprintf(stderr, "usage: kdos locale [NAME=value...]\n");
	else if (!strcmp(verb, "keymap"))
		fprintf(stderr, "usage: kdos keymap [map]\n");
	else
		fprintf(stderr, "usage: kdos in-vm\n");
	return 2;
}

static KdtBus *system_bus(const char *verb)
{
	KdtBus *b = kdt_bus_open(1);

	if (!b)
		fprintf(stderr, "kdos %s: the system bus could not be "
				"reached\n", verb);
	return b;
}

static int failed(const char *verb, const char *err)
{
	fprintf(stderr, "kdos %s: %s\n", verb, *err ? err : "no answer");
	return 1;
}

static int print_prop(const char *verb, const char *svc, const char *path,
		      const char *iface, const char *prop, const char *dflt)
{
	KdtBus *b = system_bus(verb);
	char err[256] = "", *v;

	if (!b)
		return 1;
	v = kdt_bus_get_prop_s(b, svc, path, iface, prop, err, sizeof(err));
	kdt_bus_close(b);
	if (!v)
		return failed(verb, err);
	printf("%s\n", *v ? v : dflt);
	free(v);
	return 0;
}

static int zone(int argc, char **argv)
{
	KdtBus *b;
	char err[256] = "";
	int rc;

	if (argc > 1 || (argc == 1 && !*argv[0]))
		return usage("timezone");
	if (!argc)
		return print_prop("timezone", "org.freedesktop.timedate1",
				  "/org/freedesktop/timedate1",
				  "org.freedesktop.timedate1", "Timezone",
				  "UTC");
	if (!(b = system_bus("timezone")))
		return 1;
	rc = kdt_bus_call(b, "org.freedesktop.timedate1",
			  "/org/freedesktop/timedate1",
			  "org.freedesktop.timedate1", "SetTimezone", NULL, err,
			  sizeof(err), "sb", argv[0], 1);
	kdt_bus_close(b);
	return rc ? failed("timezone", err) : 0;
}

static int hostname(int argc, char **argv)
{
	KdtBus *b;
	char err[256] = "";
	int rc;

	if (argc > 1 || (argc == 1 && !*argv[0]))
		return usage("hostname");
	if (!argc)
		return print_prop("hostname", "org.freedesktop.hostname1",
				  "/org/freedesktop/hostname1",
				  "org.freedesktop.hostname1", "Hostname", "");
	if (!(b = system_bus("hostname")))
		return 1;
	rc = kdt_bus_call(b, "org.freedesktop.hostname1",
			  "/org/freedesktop/hostname1",
			  "org.freedesktop.hostname1", "SetStaticHostname",
			  NULL, err, sizeof(err), "sb", argv[0], 1);
	kdt_bus_close(b);
	return rc ? failed("hostname", err) : 0;
}

/* Every assignment locale1 holds, one a line, as /etc/locale.conf spells
 * them. */
static int locale_print(void)
{
	KdtBus *b = system_bus("locale");
	KdtMsg *r = NULL;
	char err[256] = "";
	const char *s;

	if (!b)
		return 1;
	if (kdt_bus_get_prop(b, "org.freedesktop.locale1",
			     "/org/freedesktop/locale1",
			     "org.freedesktop.locale1", "Locale", "as", &r, err,
			     sizeof(err)) != 0) {
		kdt_bus_close(b);
		return failed("locale", err);
	}
	if (kdt_bus_enter(b, r, 'a', "s") > 0) {
		while (kdt_bus_read(b, r, "s", &s) > 0)
			printf("%s\n", s);
		kdt_bus_leave(b, r);
	}
	kdt_bus_free(b, r);
	kdt_bus_close(b);
	return 0;
}

/*
 * THE LIST REPLACES THE FILE WHOLE: locale1's SetLocale is given every
 * assignment there is to be, and a name left out is removed. The argument
 * vector is handed over as the array it already is; each element must look
 * like NAME=value, which is checked here so a typo is a usage error and not
 * a service refusal worded in D-Bus.
 */
static int locale_set(int argc, char **argv)
{
	KdtBus *b;
	char err[256] = "";
	int rc;

	for (int i = 0; i < argc; i++) {
		const char *eq = strchr(argv[i], '=');

		if (!eq || eq == argv[i])
			return usage("locale");
	}
	if (!(b = system_bus("locale")))
		return 1;
	rc = kdt_bus_call_strv(b, "org.freedesktop.locale1",
			       "/org/freedesktop/locale1",
			       "org.freedesktop.locale1", "SetLocale", err,
			       sizeof(err), argv, argc, 1);
	kdt_bus_close(b);
	return rc ? failed("locale", err) : 0;
}

static int keymap(int argc, char **argv)
{
	KdtBus *b;
	char err[256] = "";
	int rc;

	if (argc > 1 || (argc == 1 && !*argv[0]))
		return usage("keymap");
	if (!argc)
		return print_prop("keymap", "org.freedesktop.locale1",
				  "/org/freedesktop/locale1",
				  "org.freedesktop.locale1", "VConsoleKeymap",
				  "us");
	if (!(b = system_bus("keymap")))
		return 1;
	/* convert=true: the graphical layout follows the console's, which is
	 * what one command asking for "the keyboard" means. */
	rc = kdt_bus_call(b, "org.freedesktop.locale1",
			  "/org/freedesktop/locale1",
			  "org.freedesktop.locale1", "SetVConsoleKeyboard",
			  NULL, err, sizeof(err), "ssbb", argv[0], "", 1, 1);
	kdt_bus_close(b);
	return rc ? failed("keymap", err) : 0;
}

int kdt_machine_main(const char *verb, int argc, char **argv)
{
	if (!strcmp(verb, "in-vm")) {
		/* No output at all: a session script asks with the exit
		 * status alone. */
		if (argc)
			return usage(verb);
		return kb_in_vm() ? 0 : 1;
	}
	if (argc == 1 && (!strcmp(argv[0], "-h") || !strcmp(argv[0], "--help"))) {
		usage(verb);
		return 0;
	}
	if (!strcmp(verb, "timezone"))
		return zone(argc, argv);
	if (!strcmp(verb, "hostname"))
		return hostname(argc, argv);
	if (!strcmp(verb, "keymap"))
		return keymap(argc, argv);
	return argc ? locale_set(argc, argv) : locale_print();
}
