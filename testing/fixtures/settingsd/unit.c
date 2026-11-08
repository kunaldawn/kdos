/*
 * unit — kdos-settingsd's files and rules, with no bus.
 *
 * store.c is where every value is checked and every file written, and it
 * includes no sd-bus, so this half of the daemon is asserted on any host —
 * the bus half (run.sh) needs an sd-bus and a dbus-daemon, which a bare host
 * may not have. Run by testing/selftest.sh against the tree tree.sh builds:
 *
 *     unit <tree>
 *
 * Every check prints one line; the exit status is the number that failed.
 */
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "kbase.h"
#include "../../../src/daemons/kdos-settingsd/settingsd.h"

static int fails;
static char T[2048];

#define CHECK(cond, what)                                                  \
	do {                                                               \
		if (cond) {                                                \
			printf("  ok    %s\n", what);                      \
		} else {                                                   \
			printf("  FAIL  %s (%s:%d)\n", what, __FILE__, __LINE__); \
			fails++;                                           \
		}                                                          \
	} while (0)

static const char *tp(const char *rel)
{
	static char buf[8][4096];
	static int i;

	i = (i + 1) % 8;
	snprintf(buf[i], sizeof(buf[i]), "%s/%s", T, rel);
	return buf[i];
}

static char *slurp(const char *rel)
{
	char *s = kb_read_whole(tp(rel), NULL);

	return s ? s : kb_strdup("<absent>");
}

static void values(void)
{
	static const char *const host_ok[] = {
		"kdos", "work-box", "a.b.c", "x-y", "a1", "4th-floor",
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
		NULL
	};
	static const char *const host_bad[] = {
		"", "-lead", "trail-", "a..b", ".a", "a.", "sp ace", "a/b",
		"ünï", "a-.b", "x_y",
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa.aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
		NULL
	};
	int ok = 1;

	for (int i = 0; host_ok[i]; i++)
		ok &= ks_hostname_ok(host_ok[i]);
	CHECK(ok, "hostnames sethostname and DNS both take are taken");
	ok = 1;
	for (int i = 0; host_bad[i]; i++)
		if (ks_hostname_ok(host_bad[i])) {
			printf("        taken: '%s'\n", host_bad[i]);
			ok = 0;
		}
	CHECK(ok, "and empty labels, edge hyphens, spaces, slashes, non-ASCII, "
		  "underscores and 64-byte labels are not");

	CHECK(ks_text_ok("Kunal's Laptop — the one by the window"),
	      "free text takes UTF-8");
	CHECK(!ks_text_ok("two\nlines") && !ks_text_ok("tab\there") &&
	      !ks_text_ok("del\x7f"), "and refuses a C0 control or DEL");
	CHECK(!ks_text_ok("\xc2\x85") && !ks_text_ok("\xc0\xaf") &&
	      !ks_text_ok("\xed\xa0\x80") && !ks_text_ok("\xf4\x90\x80\x80") &&
	      !ks_text_ok("\xe2\x82"),
	      "and a C1 control, an overlong slash, a surrogate, a code point "
	      "past U+10FFFF and a truncated sequence");
	CHECK(ks_icon_ok("computer-laptop") && !ks_icon_ok("../evil") &&
	      !ks_icon_ok(".hidden"), "an icon is a file name in a theme");
	CHECK(ks_chassis_ok("laptop") && ks_chassis_ok("") &&
	      !ks_chassis_ok("spaceship"), "a chassis is one of systemd's ten");
}

static void envfiles(void)
{
	char v[256], *s;
	KsEnv e;

	kb_write_file(tp("etc/env.test"),
		      "# a comment = with an equals sign\n"
		      "KEEP=as-it-was\n"
		      "PRETTY=\"Old \\\"one\\\"\"\n"
		      "SINGLE='a $b'\n");
	CHECK(ks_env_load(&e, tp("etc/env.test")) == 0, "an env file loads");
	CHECK(ks_env_get(&e, "PRETTY", v, sizeof(v)) && !strcmp(v, "Old \"one\""),
	      "a double-quoted value is unescaped");
	CHECK(ks_env_get(&e, "SINGLE", v, sizeof(v)) && !strcmp(v, "a $b"),
	      "a single-quoted value is taken literally");
	CHECK(!ks_env_get(&e, "PRETTY", v, 4) && !v[0],
	      "a value that does not fit is no value, not a prefix");
	ks_env_set(&e, "PRETTY", "My \"box\" costs $5 `now` \\o/");
	ks_env_set(&e, "NEW", "plain-value");
	ks_env_save(&e, tp("etc/env.test"));
	ks_env_free(&e);

	s = slurp("etc/env.test");
	CHECK(strstr(s, "# a comment = with an equals sign\n") &&
	      strstr(s, "KEEP=as-it-was\n") && strstr(s, "NEW=plain-value\n"),
	      "a rewrite keeps every comment and every key it was not asked "
	      "about, and writes a plain value bare");
	free(s);
	ks_env_load(&e, tp("etc/env.test"));
	CHECK(ks_env_get(&e, "PRETTY", v, sizeof(v)) &&
	      !strcmp(v, "My \"box\" costs $5 `now` \\o/"),
	      "quotes, dollars, backquotes and backslashes survive the trip");
	ks_env_set(&e, "PRETTY", NULL);
	ks_env_set(&e, "KEEP", NULL);
	ks_env_set(&e, "NEW", NULL);
	ks_env_set(&e, "SINGLE", NULL);
	CHECK(e.n == 1 && e.line[0][0] == '#',
	      "removing every key leaves the comment and nothing else");
	ks_env_free(&e);

	memset(&e, 0, sizeof(e));
	ks_env_save(&e, tp("etc/env.test"));
	CHECK(!kb_path_exists(tp("etc/env.test")), "and an empty set removes the file");

	ks_os_release("PRETTY_NAME", v, sizeof(v));
	CHECK(!strcmp(v, "KDOS — the fixture's \"own\" name"),
	      "os-release's PRETTY_NAME is read through its quoting");
}

static void hostnames(void)
{
	char v[128], *s;

	ks_static_hostname(v, sizeof(v));
	CHECK(!strcmp(v, "kdos"), "the shipped /etc/hostname, with no newline, reads");
	ks_default_hostname(v, sizeof(v));
	CHECK(!strcmp(v, "kdos"), "the default hostname is the image's own");
	CHECK(ks_set_static_hostname("workbox") == 0, "a static hostname is written");
	ks_static_hostname(v, sizeof(v));
	CHECK(!strcmp(v, "workbox"), "and reads back");
	kb_write_file(tp("etc/hostname"), "bad name!\n");
	ks_static_hostname(v, sizeof(v));
	CHECK(!v[0], "a hand-written invalid name reads as no name");
	CHECK(ks_set_static_hostname("") == 0 && !kb_path_exists(tp("etc/hostname")),
	      "an empty static hostname removes the file");
	kb_write_file(tp("etc/hostname"), "kdos");

	CHECK(ks_set_hosts_name("workbox") == 0, "the hosts line is written");
	s = slurp("etc/hosts");
	CHECK(strstr(s, "\n127.0.1.1   workbox\n") && !strstr(s, "   kdos") &&
	      strstr(s, "127.0.0.1   localhost\n") && strstr(s, "ff02::2"),
	      "127.0.1.1 names the new name and every other line is kept");
	free(s);
	kb_write_file(tp("etc/hosts"), "127.0.0.1 localhost\n127.0.1.1\told\n"
				       "127.0.1.1 older\n");
	ks_set_hosts_name("kdos");
	s = slurp("etc/hosts");
	CHECK(!strcmp(s, "127.0.0.1 localhost\n127.0.1.1   kdos\n"),
	      "a second 127.0.1.1 line is dropped rather than left to disagree");
	free(s);
	unlink(tp("etc/hosts"));
	ks_set_hosts_name("kdos");
	s = slurp("etc/hosts");
	CHECK(strstr(s, "127.0.0.1   localhost\n") && strstr(s, "127.0.1.1   kdos\n"),
	      "a missing hosts file is made with localhost and the name");
	free(s);

	kb_write_file(tp("etc/machine-info"), "CHASSIS=laptop\n");
	ks_chassis(v, sizeof(v));
	CHECK(!strcmp(v, "laptop"), "machine-info's chassis wins over the firmware's");
	unlink(tp("etc/machine-info"));
}

/* Each firmware witness in systemd-hostnamed's order, by taking away the
 * one before it. */
static void firmware(void)
{
	char v[128];
	uint8_t id[16];
	uint64_t t;

	ks_chassis(v, sizeof(v));
	CHECK(!strcmp(v, "desktop"), "DMI's chassis type 3 is a desktop");
	kb_write_file(tp("sys/class/dmi/id/chassis_type"), "99\n");
	ks_chassis(v, sizeof(v));
	CHECK(!strcmp(v, "laptop"),
	      "a DMI type with no row falls to ACPI's profile (2, mobile)");
	unlink(tp("sys/firmware/acpi/pm_profile"));
	ks_chassis(v, sizeof(v));
	CHECK(!strcmp(v, "tablet"), "and then to the device tree's chassis-type");
	unlink(tp("sys/firmware/devicetree/base/chassis-type"));
	ks_chassis(v, sizeof(v));
	CHECK(!v[0], "and with no witness at all there is no chassis");
	kb_write_file(tp("sys/class/dmi/id/sys_vendor"), "QEMU\n");
	ks_chassis(v, sizeof(v));
	CHECK(!strcmp(v, "vm"), "a hypervisor's DMI vendor is a virtual machine");
	kb_write_file(tp("sys/class/dmi/id/sys_vendor"), "Microsoft Corporation\n");
	kb_write_file(tp("sys/class/dmi/id/product_name"), "Surface Laptop 5\n");
	ks_chassis(v, sizeof(v));
	CHECK(strcmp(v, "vm") != 0, "a Surface is Microsoft's and no virtual machine");
	kb_write_file(tp("sys/class/dmi/id/product_name"), "Virtual Machine\n");
	ks_chassis(v, sizeof(v));
	CHECK(!strcmp(v, "vm"), "and Hyper-V is both");
	kb_write_file(tp("sys/class/dmi/id/sys_vendor"), "Google\n");
	kb_write_file(tp("sys/class/dmi/id/product_name"), "Eve\n");
	ks_chassis(v, sizeof(v));
	CHECK(strcmp(v, "vm") != 0, "a Chromebook is Google's and no virtual machine");
	kb_write_file(tp("sys/class/dmi/id/product_name"), "Google Compute Engine\n");
	ks_chassis(v, sizeof(v));
	CHECK(!strcmp(v, "vm"), "and Compute Engine is one");
	kb_write_file(tp("sys/class/dmi/id/sys_vendor"), "Amazon EC2\n");
	kb_write_file(tp("sys/class/dmi/id/product_name"), "c6g.metal\n");
	ks_chassis(v, sizeof(v));
	CHECK(strcmp(v, "vm") != 0, "an EC2 .metal instance is the machine itself");
	kb_write_file(tp("sys/class/dmi/id/product_name"), "c6g.large\n");
	ks_chassis(v, sizeof(v));
	CHECK(!strcmp(v, "vm"), "and any other EC2 instance is a virtual machine");
	kb_write_file(tp("sys/class/dmi/id/sys_vendor"), "LENOVO\n");
	kb_write_file(tp("sys/class/dmi/id/product_name"), "21AH00ABUS\n");
	kb_write_file(tp("sys/class/dmi/id/chassis_type"), "3\n");

	ks_dmi("sys_vendor", v, sizeof(v));
	CHECK(!strcmp(v, "LENOVO"), "a DMI field reads, its newline off");
	ks_dmi("product_version", v, sizeof(v));
	CHECK(!strcmp(v, "ThinkPad T14 Gen 3"), "and its padding off");
	ks_dmi("chassis_asset_tag", v, sizeof(v));
	CHECK(!v[0], "a vendor's placeholder, in any case, is no value");
	ks_dmi("no_such_field", v, sizeof(v));
	CHECK(!v[0], "and a missing field is none");

	CHECK(ks_parse_date("03/14/2024", &t) && t == 1710374400ULL * 1000000ULL,
	      "DMI's MM/DD/YYYY is midnight UTC that day, in µs");
	CHECK(ks_parse_date("2031-12-31", &t) && t == 1956441600ULL * 1000000ULL,
	      "and os-release's YYYY-MM-DD");
	CHECK(ks_parse_date("02/29/2024", &t) && !ks_parse_date("02/29/2023", &t) &&
	      !ks_parse_date("2100-02-29", &t) && ks_parse_date("2000-02-29", &t),
	      "a leap day is a day only in a leap year");
	CHECK(!ks_parse_date("13/01/2024", &t) && !ks_parse_date("2024-04-31", &t) &&
	      !ks_parse_date("2024-01-01x", &t) && !ks_parse_date("yesterday", &t) &&
	      !ks_parse_date("", &t), "a day off the calendar or a trailing "
				      "character is no date");

	CHECK(ks_machine_id(id) && id[0] == 0x01 && id[1] == 0x23 &&
	      id[15] == 0xef, "/etc/machine-id is its sixteen bytes");
	kb_write_file(tp("etc/machine-id"), "not an id\n");
	CHECK(!ks_machine_id(id), "and a file that holds no id is none");
	kb_write_file(tp("etc/machine-id"), "0123456789abcdef0123456789abcdef\n");
	CHECK(ks_uuid_parse("4c4c4544-0042-3510-8052-b4c04f4d4433", id) &&
	      id[0] == 0x4c && id[15] == 0x33 &&
	      !ks_uuid_parse("4c4c4544004235108052b4c04f4d4433", id) &&
	      !ks_uuid_parse("4c4c4544-0042-3510-8052-b4c04f4d443g", id),
	      "a UUID is read in its dashed form and nothing else");
}

static void zones(void)
{
	char v[128], cc[3], *s, **list;
	int n, sorted = 1, have = 0;

	ks_timezone(v, sizeof(v));
	CHECK(!strcmp(v, "UTC"), "no /etc/localtime is UTC");
	CHECK(ks_zone_check("Europe/Berlin") == 0, "a zone file is a zone");
	CHECK(ks_zone_check("../../etc/shadow") == -1 &&
	      ks_zone_check("Europe/../../etc") == -1 &&
	      ks_zone_check("/etc/shadow") == -1 &&
	      ks_zone_check("zone.tab") == -1 && ks_zone_check("a//b") == -1,
	      "a traversal, an absolute path, a dot and a doubled slash are "
	      "refused by the character rule");
	CHECK(ks_zone_check("Mars/Olympus") == -2 && ks_zone_check("Europe") == -2 &&
	      ks_zone_check("Europe/Notazone") == -2,
	      "a missing name, a directory and a file without TZif are no zone");

	CHECK(ks_set_timezone("Europe/Berlin", cc) == 0 && !strcmp(cc, "DE"),
	      "a zone is set, and its country found");
	ks_timezone(v, sizeof(v));
	CHECK(!strcmp(v, "Europe/Berlin"), "and /etc/localtime reads back as the zone");
	s = slurp("etc/profile.d/20-timezone.sh");
	CHECK(strstr(s, "export TZ=':/etc/localtime'\n") != NULL,
	      "TZ names the link rather than a rules string");
	free(s);
	s = slurp("etc/modprobe.d/kdos-regdom.conf");
	CHECK(strstr(s, "options cfg80211 ieee80211_regdom=DE\n") != NULL,
	      "the zone's country becomes cfg80211's regulatory domain");
	free(s);
	CHECK(ks_set_timezone("UTC", cc) == 0 && !cc[0] &&
	      !kb_path_exists(tp("etc/modprobe.d/kdos-regdom.conf")),
	      "a zone with no country removes the old country");
	unlink(tp("etc/localtime"));
	kb_write_file(tp("etc/localtime"), "TZif2 a copy\n");
	ks_timezone(v, sizeof(v));
	CHECK(!v[0], "a copied file is a zone nobody can name");
	unlink(tp("etc/localtime"));

	list = ks_list_timezones(&n);
	for (int i = 0; i < n; i++) {
		if (i && strcmp(list[i - 1], list[i]) >= 0)
			sorted = 0;
		have += !strcmp(list[i], "US/Eastern") + !strcmp(list[i], "GB") +
			!strcmp(list[i], "UTC") + !strcmp(list[i], "Europe/Berlin");
	}
	CHECK(n == 6 && have == 4 && sorted && !list[n],
	      "ListTimezones is every zone and link in tzdata.zi, and UTC, "
	      "sorted, once each");
	kb_strv_free(list);
}

static void ntp(void)
{
	CHECK(ks_ntp_available(), "chrony's script and chronyd make NTP available");
	CHECK(ks_ntp_enabled(), "with no marker it is enabled");
	CHECK(ks_set_ntp_marker(false) == 0 &&
	      kb_path_exists(tp("etc/service.disabled/chrony")) && !ks_ntp_enabled(),
	      "disabling writes the marker rcS reads");
	CHECK(ks_set_ntp_marker(true) == 0 && ks_ntp_enabled(),
	      "and enabling removes it");
	chmod(tp("etc/init.d/35_chrony.sh"), 0644);
	CHECK(!ks_ntp_available() && !ks_ntp_enabled(),
	      "a script that cannot run is no NTP");
	chmod(tp("etc/init.d/35_chrony.sh"), 0755);
}

static void locales(void)
{
	char vals[KS_LOCALE_KEYS][KS_NAME_MAX], got[KS_LOCALE_KEYS][KS_ASSIGN_MAX];
	int k;
	bool empty;
	char *s;

	CHECK(ks_locale_get(got) == 1 && !strcmp(got[0], "LANG=C.UTF-8"),
	      "the shipped locale.conf is LANG=C.UTF-8, and its comment no "
	      "assignment");
	/* SDDM takes every line holding an `=` for a variable, comments too. */
	s = slurp("etc/locale.conf");
	k = 1;
	for (char *l = s, *nl; l && *l; l = nl ? nl + 1 : NULL) {
		nl = strchr(l, '\n');
		if (nl)
			*nl = 0;
		if (strchr(l, '=') && !ks_locale_assign_ok(l, &(int){ 0 }, &empty))
			k = 0;
	}
	free(s);
	CHECK(k, "and no line of it holds an `=` SDDM would take for a variable");
	CHECK(ks_locale_assign_ok("LANG=en_GB.UTF-8", &k, &empty) && k == 0 &&
	      !empty, "LANG=en_GB.UTF-8 is an assignment");
	CHECK(ks_locale_assign_ok("LC_TIME=", &k, &empty) && empty,
	      "NAME= is an assignment that unsets");
	CHECK(ks_locale_assign_ok("LANGUAGE=en_GB:en", &k, &empty) &&
	      !ks_locale_assign_ok("LANG=en_GB:en", &k, &empty),
	      "a colon list is LANGUAGE's alone");
	CHECK(!ks_locale_assign_ok("FOO=bar", &k, &empty) &&
	      !ks_locale_assign_ok("LANG", &k, &empty) &&
	      !ks_locale_assign_ok("LANG=en\"GB", &k, &empty) &&
	      !ks_locale_assign_ok("LANG=a b", &k, &empty) &&
	      !ks_locale_assign_ok("LANGX=en", &k, &empty),
	      "an unknown name, no value, a quote and a space are refused");

	memset(vals, 0, sizeof(vals));
	strcpy(vals[0], "en_GB.UTF-8");
	strcpy(vals[4], "en_DK.UTF-8");
	CHECK(ks_locale_set(vals) == 0, "a locale is written");
	s = slurp("etc/locale.conf");
	CHECK(!strcmp(s, "LANG=en_GB.UTF-8\nLC_TIME=en_DK.UTF-8\n"),
	      "as bare NAME=value lines and nothing else, the shape SDDM reads");
	free(s);
	CHECK(ks_locale_get(got) == 2 && !strcmp(got[0], "LANG=en_GB.UTF-8") &&
	      !strcmp(got[1], "LC_TIME=en_DK.UTF-8"), "and reads back in order");
	memset(vals, 0, sizeof(vals));
	CHECK(ks_locale_set(vals) == 0 && !kb_path_exists(tp("etc/locale.conf")),
	      "no locale at all removes the file");
}

static void keyboards(void)
{
	char a[64], b[64];
	KsXkb x, y;
	char *s;
	static const struct {
		const char *vt, *layout, *variant;
	} TABLE[] = {
		{ "us", "us", "" }, { "uk", "gb", "" }, { "de", "de", "" },
		{ "de-latin1", "de", "" },
		{ "de-latin1-nodeadkeys", "de", "nodeadkeys" },
		{ "de_CH-latin1", "ch", "" }, { "fr_CH-latin1", "ch", "fr" },
		{ "sg-latin1", "ch", "" }, { "slovene", "si", "" },
		{ "croat", "hr", "" }, { "la-latin1", "latam", "" },
		{ "dvorak-l", "us", "dvorak-l" }, { "dvorak-fr", "fr", "dvorak" },
		{ "dvorak", "us", "dvorak" }, { "fr-latin9", "fr", "latin9" },
		{ "pl2", "pl", "" }, { "br-abnt2", "br", "" },
		{ "et", "ee", "" }, { "sv-latin1", "se", "" }, { "cf", "ca", "" },
		{ "sr-cy", "rs", "" }, { "fa", "ir", "" }, { "azerty", "fr", "" },
		{ "defkeymap", "us", "" }, { "us-acentos", "us", "intl" },
		{ "tr_f-latin5", "tr", "f" }, { "en-latin9", "us", "colemak" },
		{ "mac-uk", "gb", "mac" }, { "mac-dvorak", "us", "dvorak" },
		{ "atari-de", "de", "" }, { "amiga-us", "us", "" },
		{ NULL, NULL, NULL }
	};
	static const char *const NONE[] = {
		"keypad", "backspace", "euro2", "carpalx-full", "sunt5-de-latin1",
		"apple-a1048-sv", "mac-euro", "pc110", NULL
	};
	int ok = 1;

	CHECK(ks_keymap_name_ok("de-latin1-nodeadkeys") &&
	      ks_keymap_name_ok("fr_CH-latin1") && !ks_keymap_name_ok("../x") &&
	      !ks_keymap_name_ok("/etc/passwd") && !ks_keymap_name_ok("-x"),
	      "a keymap is a name loadkeys looks up, never a path");
	CHECK(ks_keymap_installed("de-latin1") && ks_keymap_installed("dvorak") &&
	      !ks_keymap_installed("de") && !ks_keymap_installed("de-lat"),
	      "an installed keymap is found by its whole name, a prefix is not");

	/* The table on its own, where no symbols are installed to check the
	 * answers against. */
	setenv("KDOS_SETTINGSD_SHARE", tp("no-share"), 1);
	for (int i = 0; TABLE[i].vt; i++)
		if (!ks_vt_to_xkb(TABLE[i].vt, a, sizeof(a), b, sizeof(b)) ||
		    strcmp(a, TABLE[i].layout) || strcmp(b, TABLE[i].variant)) {
			printf("        %s gave %s(%s)\n", TABLE[i].vt, a, b);
			ok = 0;
		}
	CHECK(ok, "the console-to-XKB table, the two-letter rule, the names "
		  "that rule reads wrongly and another maker's maps");
	ok = 1;
	for (int i = 0; NONE[i]; i++)
		if (ks_vt_to_xkb(NONE[i], a, sizeof(a), b, sizeof(b)) || a[0]) {
			printf("        %s gave %s(%s)\n", NONE[i], a, b);
			ok = 0;
		}
	CHECK(ok, "an include map and a Sun or Apple model's map name no layout");
	CHECK(!ks_vt_to_xkb("", a, sizeof(a), b, sizeof(b)) &&
	      !ks_vt_to_xkb("1x", a, sizeof(a), b, sizeof(b)) &&
	      !ks_vt_to_xkb("mac-", a, sizeof(a), b, sizeof(b)),
	      "an empty keymap and one not starting with two letters give nothing");
	setenv("KDOS_SETTINGSD_SHARE", tp("share"), 1);
	CHECK(!ks_vt_to_xkb("pl2", a, sizeof(a), b, sizeof(b)) && !a[0],
	      "a derived layout xkeyboard-config does not list is not given");
	CHECK(ks_vt_to_xkb("fr-latin9", a, sizeof(a), b, sizeof(b)) &&
	      !strcmp(a, "fr") && !b[0] &&
	      ks_vt_to_xkb("mac-de-latin1", a, sizeof(a), b, sizeof(b)) &&
	      !strcmp(a, "de") && !b[0],
	      "a variant the list does not give its layout is dropped, the layout kept");
	CHECK(ks_vt_to_xkb("de-latin1-nodeadkeys", a, sizeof(a), b, sizeof(b)) &&
	      !strcmp(a, "de") && !strcmp(b, "nodeadkeys"),
	      "and one it does give is kept");
	kb_write_file(tp("share/X11/xkb/symbols/pc"), "");
	CHECK(!ks_vt_to_xkb("pc110", a, sizeof(a), b, sizeof(b)) &&
	      !ks_vt_to_xkb("pcx", a, sizeof(a), b, sizeof(b)),
	      "a symbols file that is no layout in the list is not one");

	CHECK(ks_xkb_to_vt("gb", "", a, sizeof(a)) && !strcmp(a, "uk") &&
	      ks_xkb_to_vt("us", "dvorak", a, sizeof(a)) && !strcmp(a, "dvorak") &&
	      ks_xkb_to_vt("de", "", a, sizeof(a)) && !strcmp(a, "de-latin1") &&
	      ks_xkb_to_vt("de", "nodeadkeys", a, sizeof(a)) &&
	      !strcmp(a, "de-latin1-nodeadkeys") &&
	      ks_xkb_to_vt("ch", "", a, sizeof(a)) && !strcmp(a, "sg") &&
	      ks_xkb_to_vt("ch", "fr", a, sizeof(a)) && !strcmp(a, "fr_CH") &&
	      ks_xkb_to_vt("us,de", ",nodeadkeys", a, sizeof(a)) && !strcmp(a, "us"),
	      "XKB to the console: the table, the name itself, its -latin1 form, "
	      "and the first of several layouts");
	CHECK(ks_xkb_to_vt("de", "mac", a, sizeof(a)) && !strcmp(a, "de-latin1") &&
	      ks_xkb_to_vt("ch", "de_nodeadkeys", a, sizeof(a)) && !strcmp(a, "sg"),
	      "a variant the console has no map for still finds the layout's");
	CHECK(!ks_xkb_to_vt("it", "", a, sizeof(a)) && !a[0],
	      "a layout with no installed console map leaves the console alone");

	memset(&x, 0, sizeof(x));
	strcpy(x.layout, "de,us");
	strcpy(x.model, "pc105");
	strcpy(x.variant, "nodeadkeys,dvorak");
	strcpy(x.options, "grp:alt_shift_toggle,ctrl:nocaps");
	CHECK(ks_xkb_check(&x) == NULL, "two layouts, their variants and two "
					 "options from the rules list pass");
#define BAD(field, val, what)                                       \
	do {                                                        \
		y = x;                                              \
		strcpy(y.field, val);                               \
		CHECK(ks_xkb_check(&y) != NULL, what);              \
	} while (0)
	BAD(layout, "xx", "a layout the list does not carry is refused");
	BAD(model, "pc999", "so is a model");
	BAD(variant, "dvorak,nodeadkeys", "and a variant under the wrong layout");
	BAD(options, "grp", "and an option group's own name");
	BAD(options, "evil;rm", "and a character no XKB name uses");
	BAD(layout, "us,de,fr,ch,gb", "and a fifth layout");
	BAD(layout, "us,,de", "and an empty layout in the list");
	BAD(variant, "a,b,c", "and more variants than layouts");
	y = x;
	y.layout[0] = 0;
	CHECK(ks_xkb_check(&y) != NULL, "and a variant with no layout");

	ks_xkb_effective(&x);
	CHECK(!strcmp(x.layout, "us") && !x.variant[0],
	      "the shipped xkb.conf follows the console keymap");
	ks_set_keymap("de-latin1-nodeadkeys");
	ks_xkb_effective(&x);
	CHECK(!strcmp(x.layout, "de") && !strcmp(x.variant, "nodeadkeys"),
	      "and follows it when it changes");
	memset(&x, 0, sizeof(x));
	strcpy(x.layout, "gb");
	strcpy(x.model, "pc105");
	CHECK(ks_set_xkb(&x) == 0, "an explicit layout is written");
	ks_xkb_effective(&y);
	CHECK(!strcmp(y.layout, "gb") && !strcmp(y.model, "pc105") && !y.variant[0],
	      "and is kept over the console's");
	s = slurp("etc/kdos/xkb.conf");
	CHECK(strstr(s, "\nXKBLAYOUT=gb\n") && strstr(s, "\nXKBMODEL=pc105\n") &&
	      strstr(s, "# AN EMPTY XKBLAYOUT FOLLOWS THE CONSOLE"),
	      "xkb.conf keeps its explanation around the new values");
	free(s);
	ks_set_keymap("us");
}

int main(int argc, char **argv)
{
	char path[4096];

	if (argc != 2) {
		fprintf(stderr, "usage: unit <tree>\n");
		return 2;
	}
	kb_strlcpy(T, argv[1], sizeof(T));
	setenv("KDOS_SETTINGSD_ETC", tp("etc"), 1);
	setenv("KDOS_SETTINGSD_SHARE", tp("share"), 1);
	setenv("KDOS_SETTINGSD_SYS", tp("sys"), 1);
	snprintf(path, sizeof(path), "%s:%s", tp("bin"), getenv("PATH") ?: "");
	setenv("PATH", path, 1);

	values();
	envfiles();
	hostnames();
	firmware();
	zones();
	ntp();
	locales();
	keyboards();
	return fails;
}
