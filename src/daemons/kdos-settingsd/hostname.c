/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   kdos-settingsd — org.freedesktop.hostname1
 *
 * THREE NAMES, IN systemd-hostnamed's ORDER. The static name is
 * /etc/hostname, which 05_hostname.sh gives the kernel at boot; the transient
 * name lives only in the kernel and in this process; the default is the
 * image's. The kernel always carries the first of the three that is set, so
 * setting a transient name on a machine with a static one changes nothing
 * the kernel shows — plasma-setup sets both, and the static one wins. The
 * `127.0.1.1` line of /etc/hosts follows whatever name the kernel carries.
 *
 * The pretty name, the icon, the chassis, the deployment and the location
 * are /etc/machine-info, read by nothing at boot and by every program that
 * wants a human name for the machine.
 *
 * EVERY PROPERTY systemd's hostname1 HAS, whether or not a KDE program reads
 * it: a GetAll is how most clients read any of them, and a client written
 * against systemd may look up one this file left out. A value the machine
 * cannot give is the type's "unknown" (empty, UINT64_MAX, VMADDR_CID_ANY),
 * never an error, because one failing getter fails GetAll for every
 * property beside it. Describe, the JSON digest of all of them, is not
 * served: nothing on this system calls it.
 * ---------------------------------
 */

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/utsname.h>
#include <unistd.h>
#include <linux/vm_sockets.h>

#include "kbase.h"
#include "bus.h"

#define A_HOSTNAME "org.freedesktop.hostname1.set-hostname"
#define A_STATIC   "org.freedesktop.hostname1.set-static-hostname"
#define A_INFO     "org.freedesktop.hostname1.set-machine-info"
#define A_UUID     "org.freedesktop.hostname1.get-product-uuid"
#define A_SERIAL   "org.freedesktop.hostname1.get-hardware-serial"

#define E_NO_UUID   "org.freedesktop.hostname1.NoProductUUID"
#define E_NO_SERIAL "org.freedesktop.hostname1.NoHardwareSerial"

static char g_transient[KS_HOST_MAX + 1];

/* A test tree never renames the machine it runs on. The name the kernel
 * would have been given is kept here and reported in its place, so the
 * precedence above is still what the fixture asserts. */
static char g_test_kernel[KS_HOST_MAX + 1];
static bool g_test_kernel_set;

static void kernel_hostname(char *out, size_t n)
{
	if (ks_test_tree() && g_test_kernel_set) {
		kb_strlcpy(out, g_test_kernel, n);
		return;
	}
	if (gethostname(out, n) != 0)
		out[0] = 0;
	out[n - 1] = 0;
}

static void wanted_hostname(char *out, size_t n)
{
	ks_static_hostname(out, n);
	if (!*out)
		kb_strlcpy(out, g_transient, n);
	if (!*out)
		ks_default_hostname(out, n);
}

static int apply_kernel(sd_bus_error *err)
{
	char want[KS_HOST_MAX + 1], now[KS_HOST_MAX + 1];

	wanted_hostname(want, sizeof(want));
	kernel_hostname(now, sizeof(now));
	if (strcmp(want, now)) {
		if (ks_test_tree()) {
			kb_strlcpy(g_test_kernel, want, sizeof(g_test_kernel));
			g_test_kernel_set = true;
		} else if (sethostname(want, strlen(want)) != 0) {
			return sd_bus_error_setf(err, SD_BUS_ERROR_FAILED,
						 "cannot set the kernel hostname: %s",
						 strerror(errno));
		}
	}
	/* The name is set either way; a hosts file that cannot be written is
	 * said in the log rather than reported as the call failing. */
	if (ks_set_hosts_name(want) != 0)
		fprintf(stderr, "kdos-settingsd: /etc/hosts not updated: %s\n",
			strerror(errno));
	return 0;
}

/*
 * A KERNEL WITH NO NAME IS GIVEN THE DEFAULT ONE. 05_hostname.sh names the
 * kernel from /etc/hostname and does nothing when the file is gone, which
 * is how SetStaticHostname("") leaves it, so the next boot comes up as
 * `(none)`, a name nothing resolves. systemd's init gives the default there;
 * this daemon does when it starts. A kernel that has a name of its own, a
 * DHCP lease's or one set by hand, is left with it.
 */
void ks_hostname_init(void)
{
	sd_bus_error err = SD_BUS_ERROR_NULL;
	char now[KS_HOST_MAX + 1];

	kernel_hostname(now, sizeof(now));
	if ((!*now || !strcmp(now, "(none)")) && apply_kernel(&err) < 0)
		fprintf(stderr, "kdos-settingsd: %s\n", err.message);
	sd_bus_error_free(&err);
}

/* ── /etc/machine-info ───────────────────────────────────────────────────── */

typedef struct {
	const char *key, *prop, *action;
	bool (*ok)(const char *);
} KsInfo;

/* SetPrettyHostname is authorised as the static name is, as systemd does:
 * both are what the machine is called. */
static const KsInfo INFO[] = {
	{ "PRETTY_HOSTNAME", "PrettyHostname", A_STATIC, ks_text_ok },
	{ "ICON_NAME",	     "IconName",       A_INFO,	 ks_icon_ok },
	{ "CHASSIS",	     "Chassis",	       A_INFO,	 ks_chassis_ok },
	{ "DEPLOYMENT",	     "Deployment",     A_INFO,	 ks_deployment_ok },
	{ "LOCATION",	     "Location",       A_INFO,	 ks_text_ok },
};

enum { I_PRETTY, I_ICON, I_CHASSIS, I_DEPLOYMENT, I_LOCATION };

/* The raw value, held to its own rule on the way out as well as on the way
 * in: a string that is not UTF-8 cannot be sent at all, and one bad field
 * would fail GetAll for every other property with it. */
static void info_get(int i, char *out, size_t n)
{
	char p[4096];

	out[0] = 0;
	if (ks_etc_path(p, sizeof(p), "machine-info"))
		ks_env_read1(p, INFO[i].key, out, n);
	if (!INFO[i].ok(out))
		out[0] = 0;
}

static void icon_name(char *out, size_t n)
{
	char ch[32];

	info_get(I_ICON, out, n);
	if (*out)
		return;
	ks_chassis(ch, sizeof(ch));
	if (*ch)
		snprintf(out, n, "computer-%s", ch);
	else
		kb_strlcpy(out, "computer", n);
}

/* ── the hardware ────────────────────────────────────────────────────────── */

/*
 * A DMI field, unless machine-info names its own: machine-info(5)'s
 * HARDWARE_* keys are how an image or an administrator corrects a firmware
 * that reports a placeholder or the wrong board. `alt` is the board's file
 * for the same field, for a firmware that fills only that one.
 */
static const struct {
	const char *prop, *file, *alt, *key;
} HW[] = {
	{ "HardwareVendor",  "sys_vendor",	  "board_vendor",  "HARDWARE_VENDOR" },
	{ "HardwareModel",   "product_name",	  "board_name",	   "HARDWARE_MODEL" },
	{ "HardwareSKU",     "product_sku",	  NULL,		   "HARDWARE_SKU" },
	{ "HardwareVersion", "product_version",	  "board_version", "HARDWARE_VERSION" },
	{ "FirmwareVersion", "bios_version",	  NULL,		   NULL },
	{ "FirmwareVendor",  "bios_vendor",	  NULL,		   NULL },
	{ "ChassisAssetTag", "chassis_asset_tag", NULL,		   NULL },
};
#define HW_N ((int)(sizeof(HW) / sizeof(HW[0])))

/* Lenovo's product_name is a machine-type code (`11R9CTO1WW`) and its
 * product_version the model people know (`ThinkCentre M75s Gen 2`), which
 * is the one systemd's DMI database reports as the model. */
static bool lenovo(void)
{
	char v[64];

	ks_dmi("sys_vendor", v, sizeof(v));
	return !strcasecmp(v, "LENOVO");
}

static void hw_field(const char *prop, char *out, size_t n)
{
	char p[4096];

	out[0] = 0;
	for (int i = 0; i < HW_N; i++) {
		if (strcmp(prop, HW[i].prop))
			continue;
		if (HW[i].key && ks_etc_path(p, sizeof(p), "machine-info"))
			ks_env_read1(p, HW[i].key, out, n);
		if (!ks_text_ok(out))
			out[0] = 0;
		if (!*out && !strcmp(prop, "HardwareModel") && lenovo())
			ks_dmi("product_version", out, n);
		if (!*out)
			ks_dmi(HW[i].file, out, n);
		if (!*out && HW[i].alt)
			ks_dmi(HW[i].alt, out, n);
		return;
	}
}

/* ── properties ──────────────────────────────────────────────────────────── */

static const char *hostname_source(void)
{
	char k[KS_HOST_MAX + 1], s[KS_HOST_MAX + 1];

	kernel_hostname(k, sizeof(k));
	ks_static_hostname(s, sizeof(s));
	if (*s && !strcmp(k, s))
		return "static";
	if (*g_transient && !strcmp(k, g_transient))
		return "transient";
	return "default";
}

static int get_prop(sd_bus *bus, const char *path, const char *iface,
		    const char *prop, sd_bus_message *reply, void *userdata,
		    sd_bus_error *err)
{
	char v[KS_TEXT_MAX + 64];
	struct utsname u;

	(void)bus, (void)path, (void)iface, (void)userdata, (void)err;
	v[0] = 0;
	if (!strcmp(prop, "Hostname")) {
		kernel_hostname(v, KS_HOST_MAX + 1);
	} else if (!strcmp(prop, "StaticHostname")) {
		ks_static_hostname(v, sizeof(v));
	} else if (!strcmp(prop, "DefaultHostname")) {
		ks_default_hostname(v, sizeof(v));
	} else if (!strcmp(prop, "HostnameSource")) {
		kb_strlcpy(v, hostname_source(), sizeof(v));
	} else if (!strcmp(prop, "PrettyHostname")) {
		info_get(I_PRETTY, v, sizeof(v));
	} else if (!strcmp(prop, "IconName")) {
		icon_name(v, sizeof(v));
	} else if (!strcmp(prop, "Chassis")) {
		ks_chassis(v, sizeof(v));
	} else if (!strcmp(prop, "Deployment")) {
		info_get(I_DEPLOYMENT, v, sizeof(v));
	} else if (!strcmp(prop, "Location")) {
		info_get(I_LOCATION, v, sizeof(v));
	} else if (!strncmp(prop, "Kernel", 6)) {
		if (uname(&u) == 0)
			kb_strlcpy(v, !strcmp(prop, "KernelName") ? u.sysname
				   : !strcmp(prop, "KernelRelease") ? u.release
				   : u.version, sizeof(v));
	} else if (!strcmp(prop, "OperatingSystemPrettyName")) {
		ks_os_release("PRETTY_NAME", v, sizeof(v));
		if (!*v)
			ks_os_release("NAME", v, sizeof(v));
		if (!*v)
			kb_strlcpy(v, "Linux", sizeof(v));
	} else if (!strcmp(prop, "OperatingSystemCPEName")) {
		ks_os_release("CPE_NAME", v, sizeof(v));
	} else if (!strcmp(prop, "OperatingSystemImageID")) {
		ks_os_release("IMAGE_ID", v, sizeof(v));
	} else if (!strcmp(prop, "OperatingSystemImageVersion")) {
		ks_os_release("IMAGE_VERSION", v, sizeof(v));
	} else if (!strcmp(prop, "HomeURL")) {
		ks_os_release("HOME_URL", v, sizeof(v));
	} else {
		hw_field(prop, v, sizeof(v));
	}
	if (!ks_text_ok(v))
		v[0] = 0;
	return sd_bus_message_append(reply, "s", v);
}

/* A date systemd reports in µs, UINT64_MAX for "unknown". */
static int get_date(sd_bus *bus, const char *path, const char *iface,
		    const char *prop, sd_bus_message *reply, void *userdata,
		    sd_bus_error *err)
{
	uint64_t t = UINT64_MAX;
	char v[64];

	(void)bus, (void)path, (void)iface, (void)userdata, (void)err;
	if (!strcmp(prop, "FirmwareDate"))
		ks_dmi("bios_date", v, sizeof(v));
	else
		ks_os_release("SUPPORT_END", v, sizeof(v));
	if (!*v || !ks_parse_date(v, &t))
		t = UINT64_MAX;
	return sd_bus_message_append(reply, "t", t);
}

static bool boot_id(uint8_t out[16])
{
	char v[64];

	return kb_read_line_file("/proc/sys/kernel/random/boot_id", v,
				 sizeof(v)) > 0 && ks_uuid_parse(v, out);
}

/* Sixteen bytes, or none when the machine holds no id: a made-up id would
 * be one some other machine might have as well. */
static int get_id(sd_bus *bus, const char *path, const char *iface,
		  const char *prop, sd_bus_message *reply, void *userdata,
		  sd_bus_error *err)
{
	uint8_t id[16] = { 0 };
	bool ok;

	(void)bus, (void)path, (void)iface, (void)userdata, (void)err;
	ok = !strcmp(prop, "MachineID") ? ks_machine_id(id) : boot_id(id);
	return sd_bus_message_append_array(reply, 'y', id, ok ? sizeof(id) : 0);
}

/* The machine's own vsock address when it is a guest with a vsock device;
 * VMADDR_CID_ANY otherwise, which is systemd's "none". */
static int get_cid(sd_bus *bus, const char *path, const char *iface,
		   const char *prop, sd_bus_message *reply, void *userdata,
		   sd_bus_error *err)
{
	unsigned int cid = VMADDR_CID_ANY;
	int fd = open("/dev/vsock", O_RDONLY | O_CLOEXEC);

	(void)bus, (void)path, (void)iface, (void)prop, (void)userdata;
	(void)err;
	if (fd >= 0) {
		if (ioctl(fd, IOCTL_VM_SOCKETS_GET_LOCAL_CID, &cid) != 0)
			cid = VMADDR_CID_ANY;
		close(fd);
	}
	return sd_bus_message_append(reply, "u", (uint32_t)cid);
}

/* ── methods ─────────────────────────────────────────────────────────────── */

static int bad_args(sd_bus_error *err, const char *what)
{
	return sd_bus_error_setf(err, SD_BUS_ERROR_INVALID_ARGS, "%s", what);
}

static int apply_transient(KsAuth *a, sd_bus_error *err)
{
	int r;

	kb_strlcpy(g_transient, a->s[0], sizeof(g_transient));
	r = apply_kernel(err);
	sd_bus_emit_properties_changed(ks_bus(), KS_HOSTNAME_PATH,
				       KS_HOSTNAME_IF, "Hostname",
				       "HostnameSource", NULL);
	return r;
}

static int m_set_hostname(sd_bus_message *m, void *u, sd_bus_error *err)
{
	const char *name;
	int inter, r;
	KsAuth *a;

	(void)u;
	r = sd_bus_message_read(m, "sb", &name, &inter);
	if (r < 0)
		return r;
	if (*name && !ks_hostname_ok(name))
		return bad_args(err, "not a valid hostname");
	a = ks_auth_new(m, apply_transient, inter);
	kb_strlcpy(a->s[0], name, sizeof(a->s[0]));
	return ks_auth_run(a, A_HOSTNAME);
}

static int apply_static(KsAuth *a, sd_bus_error *err)
{
	int r;

	if (ks_set_static_hostname(a->s[0]) != 0)
		return sd_bus_error_setf(err, SD_BUS_ERROR_FAILED,
					 "cannot write /etc/hostname: %s",
					 strerror(errno));
	r = apply_kernel(err);
	sd_bus_emit_properties_changed(ks_bus(), KS_HOSTNAME_PATH,
				       KS_HOSTNAME_IF, "StaticHostname",
				       "Hostname", "HostnameSource", NULL);
	return r;
}

static int m_set_static(sd_bus_message *m, void *u, sd_bus_error *err)
{
	char cur[KS_HOST_MAX + 1];
	const char *name;
	int inter, r;
	KsAuth *a;

	(void)u;
	r = sd_bus_message_read(m, "sb", &name, &inter);
	if (r < 0)
		return r;
	if (*name && !ks_hostname_ok(name))
		return bad_args(err, "not a valid hostname");
	ks_static_hostname(cur, sizeof(cur));
	if (!strcmp(cur, name))
		return sd_bus_reply_method_return(m, NULL);
	a = ks_auth_new(m, apply_static, inter);
	kb_strlcpy(a->s[0], name, sizeof(a->s[0]));
	return ks_auth_run(a, A_STATIC);
}

static int apply_info(KsAuth *a, sd_bus_error *err)
{
	const KsInfo *f = &INFO[a->b[0]];
	char p[4096];
	KsEnv e;
	int r;

	if (!ks_etc_path(p, sizeof(p), "machine-info") ||
	    ks_env_load(&e, p) != 0)
		return sd_bus_error_setf(err, SD_BUS_ERROR_FAILED,
					 "cannot read /etc/machine-info: %s",
					 strerror(errno));
	ks_env_set(&e, f->key, a->s[0][0] ? a->s[0] : NULL);
	r = ks_env_save(&e, p);
	ks_env_free(&e);
	if (r != 0)
		return sd_bus_error_setf(err, SD_BUS_ERROR_FAILED,
					 "cannot write /etc/machine-info: %s",
					 strerror(errno));
	/* The icon is named after the chassis until one is set, so a chassis
	 * change is an icon change too. */
	if (a->b[0] == I_CHASSIS)
		sd_bus_emit_properties_changed(ks_bus(), KS_HOSTNAME_PATH,
					       KS_HOSTNAME_IF, "Chassis",
					       "IconName", NULL);
	else
		sd_bus_emit_properties_changed(ks_bus(), KS_HOSTNAME_PATH,
					       KS_HOSTNAME_IF, f->prop, NULL);
	return 0;
}

static int set_info(sd_bus_message *m, int i, sd_bus_error *err)
{
	char cur[KS_TEXT_MAX];
	const char *v;
	int inter, r;
	KsAuth *a;

	r = sd_bus_message_read(m, "sb", &v, &inter);
	if (r < 0)
		return r;
	if (!INFO[i].ok(v))
		return bad_args(err, "not a valid value for this field");
	info_get(i, cur, sizeof(cur));
	if (!strcmp(cur, v))
		return sd_bus_reply_method_return(m, NULL);
	a = ks_auth_new(m, apply_info, inter);
	a->b[0] = i;
	kb_strlcpy(a->s[0], v, sizeof(a->s[0]));
	return ks_auth_run(a, INFO[i].action);
}

static int m_set_pretty(sd_bus_message *m, void *u, sd_bus_error *e)
{
	(void)u;
	return set_info(m, I_PRETTY, e);
}

static int m_set_icon(sd_bus_message *m, void *u, sd_bus_error *e)
{
	(void)u;
	return set_info(m, I_ICON, e);
}

static int m_set_chassis(sd_bus_message *m, void *u, sd_bus_error *e)
{
	(void)u;
	return set_info(m, I_CHASSIS, e);
}

static int m_set_deployment(sd_bus_message *m, void *u, sd_bus_error *e)
{
	(void)u;
	return set_info(m, I_DEPLOYMENT, e);
}

static int m_set_location(sd_bus_message *m, void *u, sd_bus_error *e)
{
	(void)u;
	return set_info(m, I_LOCATION, e);
}

/*
 * THE PRODUCT UUID AND THE SERIAL ARE ASKED FOR, NEVER SHOWN. Each names
 * this one machine to whoever is given it, which is why the kernel makes
 * their DMI files root's alone; a caller that is not root gets them only
 * past polkit, under systemd's two action ids, and the file is read by this
 * daemon once polkit has said yes.
 */
static int apply_uuid(KsAuth *a, sd_bus_error *err)
{
	sd_bus_message *reply = NULL;
	uint8_t id[16];
	char v[64];
	int r;

	ks_dmi("product_uuid", v, sizeof(v));
	if (!ks_uuid_parse(v, id))
		return sd_bus_error_setf(err, E_NO_UUID,
					 "the firmware reports no product UUID");
	r = sd_bus_message_new_method_return(a->call, &reply);
	if (r >= 0)
		r = sd_bus_message_append_array(reply, 'y', id, sizeof(id));
	if (r >= 0)
		r = sd_bus_send(NULL, reply, NULL);
	sd_bus_message_unref(reply);
	return r < 0 ? r : 1;
}

static int m_get_uuid(sd_bus_message *m, void *u, sd_bus_error *err)
{
	int inter, r;

	(void)u, (void)err;
	r = sd_bus_message_read(m, "b", &inter);
	if (r < 0)
		return r;
	return ks_auth_run(ks_auth_new(m, apply_uuid, inter), A_UUID);
}

/* The product's serial, else the board's, else a device tree's. */
static int apply_serial(KsAuth *a, sd_bus_error *err)
{
	char v[KS_TEXT_MAX], p[4096];
	int r;

	ks_dmi("product_serial", v, sizeof(v));
	if (!*v)
		ks_dmi("board_serial", v, sizeof(v));
	if (!*v && ks_sys_path(p, sizeof(p),
			       "firmware/devicetree/base/serial-number") &&
	    (kb_read_line_file(p, v, sizeof(v)) < 0 || !ks_text_ok(v)))
		v[0] = 0;
	if (!*v)
		return sd_bus_error_setf(err, E_NO_SERIAL,
					 "the firmware reports no serial number");
	r = sd_bus_reply_method_return(a->call, "s", v);
	return r < 0 ? r : 1;
}

static int m_get_serial(sd_bus_message *m, void *u, sd_bus_error *err)
{
	(void)u, (void)err;
	return ks_auth_run(ks_auth_new(m, apply_serial, 0), A_SERIAL);
}

/* Every method UNPRIVILEGED: on the system bus sd-bus otherwise demands
 * CAP_SYS_ADMIN of the caller itself, and polkit would never be asked. */
#define M(name, sig, out, fn) \
	SD_BUS_METHOD(name, sig, out, fn, SD_BUS_VTABLE_UNPRIVILEGED)
#define P(name, flag) SD_BUS_PROPERTY(name, "s", get_prop, 0, flag)
#define CHG SD_BUS_VTABLE_PROPERTY_EMITS_CHANGE
#define CONST SD_BUS_VTABLE_PROPERTY_CONST

static const sd_bus_vtable hostname_vtable[] = {
	SD_BUS_VTABLE_START(0),
	P("Hostname", CHG),
	P("StaticHostname", CHG),
	P("PrettyHostname", CHG),
	P("DefaultHostname", CONST),
	P("HostnameSource", CHG),
	P("IconName", CHG),
	P("Chassis", CHG),
	P("Deployment", CHG),
	P("Location", CHG),
	P("KernelName", CONST),
	P("KernelRelease", CONST),
	P("KernelVersion", CONST),
	P("OperatingSystemPrettyName", CONST),
	P("OperatingSystemCPEName", CONST),
	SD_BUS_PROPERTY("OperatingSystemSupportEnd", "t", get_date, 0, CONST),
	P("HomeURL", CONST),
	P("OperatingSystemImageID", CONST),
	P("OperatingSystemImageVersion", CONST),
	P("HardwareVendor", CONST),
	P("HardwareModel", CONST),
	P("HardwareSKU", CONST),
	P("HardwareVersion", CONST),
	P("FirmwareVersion", CONST),
	P("FirmwareVendor", CONST),
	SD_BUS_PROPERTY("FirmwareDate", "t", get_date, 0, CONST),
	SD_BUS_PROPERTY("MachineID", "ay", get_id, 0, CONST),
	SD_BUS_PROPERTY("BootID", "ay", get_id, 0, CONST),
	SD_BUS_PROPERTY("VSockCID", "u", get_cid, 0, CONST),
	P("ChassisAssetTag", CONST),
	M("SetHostname", "sb", NULL, m_set_hostname),
	M("SetStaticHostname", "sb", NULL, m_set_static),
	M("SetPrettyHostname", "sb", NULL, m_set_pretty),
	M("SetIconName", "sb", NULL, m_set_icon),
	M("SetChassis", "sb", NULL, m_set_chassis),
	M("SetDeployment", "sb", NULL, m_set_deployment),
	M("SetLocation", "sb", NULL, m_set_location),
	M("GetProductUUID", "b", "ay", m_get_uuid),
	M("GetHardwareSerial", "", "s", m_get_serial),
	SD_BUS_VTABLE_END
};

int ks_hostname_add(sd_bus *bus)
{
	return sd_bus_add_object_vtable(bus, NULL, KS_HOSTNAME_PATH,
					KS_HOSTNAME_IF, hostname_vtable, NULL);
}
