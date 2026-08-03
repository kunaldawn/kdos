/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   KDOS Installer — answer file
 * ---------------------------------
 */

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "kinstall.h"

Config cfg;

/* Only the services a person would actually want to decide about. udev,
 * dbus, seatd and the rest are not choices, they are the system. */
const Service ki_services[] = {
	{ "networkmanager", "NetworkManager", "wired, wifi and VPN", 1 },
	{ "bluetooth", "Bluetooth", "bluez daemon", 1 },
	{ "alsa", "ALSA state", "restore mixer levels at boot", 1 },
	{ "cups", "CUPS", "printing daemon", 0 },
	{ "sshd", "OpenSSH server", "accepts remote logins", 0 },
};

int ki_nservices = (int)(sizeof(ki_services) / sizeof(ki_services[0]));

void conf_defaults(void)
{
	memset(&cfg, 0, sizeof(cfg));
	strlcpy_(cfg.keymap, "us", sizeof(cfg.keymap));
	strlcpy_(cfg.tz, "UTC0", sizeof(cfg.tz));
	strlcpy_(cfg.tz_label, "UTC", sizeof(cfg.tz_label));
	strlcpy_(cfg.fstype, "ext4", sizeof(cfg.fstype));
	strlcpy_(cfg.hostname, "kdos", sizeof(cfg.hostname));
	strlcpy_(cfg.username, "kdos", sizeof(cfg.username));
	strlcpy_(cfg.fullname, "KDOS User", sizeof(cfg.fullname));
	strlcpy_(cfg.theme, "phosphor", sizeof(cfg.theme));
	cfg.plan = PLAN_WIPE;
	cfg.format_esp = 1;
	cfg.swap = SWAP_FILE;
	cfg.swap_mb = 4096;
	cfg.user_wheel = 1;
	cfg.root_locked = 1;
	cfg.with_appbox = 1;
	cfg.reboot_after = 1;

	for (int i = 0; i < ki_nservices; i++)
		if (!ki_services[i].def_on)
			cfg.svc_off |= 1u << i;
}

/* ──────────────────────────────────────────────────────────────────────── */

static void set_kv(const char *k, const char *v)
{
	if (!strcmp(k, "keymap"))
		strlcpy_(cfg.keymap, v, sizeof(cfg.keymap));
	else if (!strcmp(k, "timezone"))
		strlcpy_(cfg.tz, v, sizeof(cfg.tz));
	else if (!strcmp(k, "timezone_label"))
		strlcpy_(cfg.tz_label, v, sizeof(cfg.tz_label));
	else if (!strcmp(k, "disk"))
		strlcpy_(cfg.disk, v, sizeof(cfg.disk));
	else if (!strcmp(k, "plan"))
		cfg.plan = !strcmp(v, "reuse") ? PLAN_REUSE
			 : !strcmp(v, "manual") ? PLAN_MANUAL : PLAN_WIPE;
	else if (!strcmp(k, "esp"))
		strlcpy_(cfg.part_esp, v, sizeof(cfg.part_esp));
	else if (!strcmp(k, "root"))
		strlcpy_(cfg.part_root, v, sizeof(cfg.part_root));
	else if (!strcmp(k, "format_esp"))
		cfg.format_esp = atoi(v);
	else if (!strcmp(k, "fstype"))
		strlcpy_(cfg.fstype, v, sizeof(cfg.fstype));
	else if (!strcmp(k, "swap"))
		cfg.swap = !strcmp(v, "file") ? SWAP_FILE
			 : !strcmp(v, "partition") ? SWAP_PART : SWAP_NONE;
	else if (!strcmp(k, "swap_mb"))
		cfg.swap_mb = atol(v);
	else if (!strcmp(k, "hostname"))
		strlcpy_(cfg.hostname, v, sizeof(cfg.hostname));
	else if (!strcmp(k, "username"))
		strlcpy_(cfg.username, v, sizeof(cfg.username));
	else if (!strcmp(k, "fullname"))
		strlcpy_(cfg.fullname, v, sizeof(cfg.fullname));
	else if (!strcmp(k, "password"))
		strlcpy_(cfg.userpass, v, sizeof(cfg.userpass));
	else if (!strcmp(k, "root_password"))
		strlcpy_(cfg.rootpass, v, sizeof(cfg.rootpass));
	else if (!strcmp(k, "root_locked"))
		cfg.root_locked = atoi(v);
	else if (!strcmp(k, "theme"))
		strlcpy_(cfg.theme, v, sizeof(cfg.theme));
	else if (!strcmp(k, "alien_apps"))
		cfg.with_appbox = atoi(v);
	else if (!strcmp(k, "reboot"))
		cfg.reboot_after = atoi(v);
	else if (!strcmp(k, "services")) {
		cfg.svc_off = 0;
		for (int i = 0; i < ki_nservices; i++) {
			const char *p = strstr(v, ki_services[i].name);
			if (!p)
				cfg.svc_off |= 1u << i;
		}
	}
}

int conf_load(const char *path)
{
	char buf[8192];
	if (read_file(path, buf, sizeof(buf)) < 0)
		return -1;

	char *save = NULL;
	for (char *line = strtok_r(buf, "\n", &save); line;
	     line = strtok_r(NULL, "\n", &save)) {
		while (*line == ' ' || *line == '\t')
			line++;
		if (!*line || *line == '#')
			continue;
		char *eq = strchr(line, '=');
		if (!eq)
			continue;
		*eq = 0;
		char *k = line, *v = eq + 1;
		size_t n = strlen(k);
		while (n && (k[n - 1] == ' ' || k[n - 1] == '\t'))
			k[--n] = 0;
		while (*v == ' ' || *v == '\t')
			v++;
		n = strlen(v);
		while (n && (v[n - 1] == ' ' || v[n - 1] == '\r'))
			v[--n] = 0;
		set_kv(k, v);
	}
	return 0;
}

int conf_save(const char *path)
{
	char buf[4096], svc[256] = "";
	for (int i = 0; i < ki_nservices; i++) {
		if (cfg.svc_off & (1u << i))
			continue;
		if (svc[0])
			strncat(svc, " ", sizeof(svc) - strlen(svc) - 1);
		strncat(svc, ki_services[i].name, sizeof(svc) - strlen(svc) - 1);
	}

	snprintf(buf, sizeof(buf),
		 "# KDOS installer answer file\n"
		 "# kinstall --config <this file> [--unattended]\n"
		 "#\n"
		 "# Passwords are deliberately NOT written here. Set them with\n"
		 "# password= / root_password= yourself if you accept a plaintext\n"
		 "# credential on the medium this file lives on.\n"
		 "\n"
		 "keymap         = %s\n"
		 "timezone       = %s\n"
		 "timezone_label = %s\n"
		 "\n"
		 "disk           = %s\n"
		 "plan           = %s\n"
		 "esp            = %s\n"
		 "root           = %s\n"
		 "format_esp     = %d\n"
		 "fstype         = %s\n"
		 "swap           = %s\n"
		 "swap_mb        = %ld\n"
		 "\n"
		 "hostname       = %s\n"
		 "username       = %s\n"
		 "fullname       = %s\n"
		 "root_locked    = %d\n"
		 "\n"
		 "theme          = %s\n"
		 "alien_apps     = %d\n"
		 "services       = %s\n"
		 "reboot         = %d\n",
		 cfg.keymap, cfg.tz, cfg.tz_label,
		 cfg.disk,
		 cfg.plan == PLAN_REUSE ? "reuse"
					: cfg.plan == PLAN_MANUAL ? "manual" : "wipe",
		 cfg.part_esp, cfg.part_root, cfg.format_esp, cfg.fstype,
		 cfg.swap == SWAP_FILE ? "file"
				       : cfg.swap == SWAP_PART ? "partition" : "none",
		 cfg.swap_mb,
		 cfg.hostname, cfg.username, cfg.fullname, cfg.root_locked,
		 cfg.theme, cfg.with_appbox, svc, cfg.reboot_after);

	return write_file(path, buf);
}
