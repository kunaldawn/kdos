/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   KD's Homebrew Linux Distro
 * ---------------------------------
 */

/*
 * Network interfaces, from /sys/class/net.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "kproc.h"

static void trim(char *s)
{
	if (!s)
		return;
	char *nl = strchr(s, '\n');
	if (nl)
		*nl = 0;
}

/*
 * A virtual interface is flagged rather than hidden, for the same reason a
 * loop device is: which ones are interesting is the caller's decision. The
 * test is the absence of a `device` link — a real NIC has one pointing at its
 * bus device, and veth/bridge/tun do not.
 */
static int iface_virtual(const char *name)
{
	char probe[512];
	snprintf(probe, sizeof(probe), "%s/class/net/%s/device", kpr_sys(), name);
	return !kb_path_exists(probe);
}

int kpr_net_list(KprIface **out)
{
	*out = NULL;
	int count = 0;
	char path[512];
	snprintf(path, sizeof(path), "%s/class/net", kpr_sys());
	char **names = kb_listdir(path, &count);
	if (!names)
		return 0;

	KprIface *v = kb_calloc((size_t)(count > 0 ? count : 1), sizeof(*v));
	int n = 0;
	for (char **e = names; *e; e++) {
		if ((*e)[0] == '.')
			continue;
		KprIface *i = &v[n++];
		memset(i, 0, sizeof(*i));
		kb_strlcpy(i->name, *e, sizeof(i->name));
		i->loopback = !strcmp(*e, "lo");
		i->virt = i->loopback || iface_virtual(*e);
		i->mtu = (int)kpr_num_sys(0, "class/net/%s/mtu", *e);
		/*
		 * speed is absent on a wireless device and on a link that is
		 * down, and reading it on some drivers returns -1. Either way
		 * the answer is "the driver does not publish one", which the
		 * renderer must not print as 0 Mbit.
		 */
		i->speed_mbit = (long)kpr_num_sys(-1, "class/net/%s/speed", *e);

		/*
		 * `up` is IFF_UP out of the flags word, not operstate.
		 * operstate is the CARRIER, and a loopback device reports it
		 * as "unknown" for ever — reading it as the up/down answer
		 * shows lo as down on every machine. flags is the kernel's own
		 * administrative state and is what `ip link` prints.
		 */
		char *fl = kpr_slurp_sys("class/net/%s/flags", *e);
		if (fl) {
			i->up = (strtoul(fl, NULL, 0) & 0x1u) != 0;
			free(fl);
		}
		/* The carrier, kept separate: an interface can be up with no
		 * cable in it, and the two states have different remedies. */
		char *op = kpr_slurp_sys("class/net/%s/operstate", *e);
		if (op) {
			trim(op);
			i->carrier = !strcmp(op, "up");
			free(op);
		}
		char *mac = kpr_slurp_sys("class/net/%s/address", *e);
		if (mac) {
			trim(mac);
			kb_strlcpy(i->mac, mac, sizeof(i->mac));
			free(mac);
		}
		char link[512], target[1024];
		snprintf(link, sizeof(link), "%s/class/net/%s/device/driver",
			 kpr_sys(), *e);
		ssize_t k = readlink(link, target, sizeof(target) - 1);
		if (k > 0) {
			target[k] = 0;
			kb_strlcpy(i->driver, kb_basename(target), sizeof(i->driver));
		}

		i->rx_bytes = (unsigned long long)kpr_num_sys(0, "class/net/%s/statistics/rx_bytes", *e);
		i->tx_bytes = (unsigned long long)kpr_num_sys(0, "class/net/%s/statistics/tx_bytes", *e);
		i->rx_pkts  = (unsigned long long)kpr_num_sys(0, "class/net/%s/statistics/rx_packets", *e);
		i->tx_pkts  = (unsigned long long)kpr_num_sys(0, "class/net/%s/statistics/tx_packets", *e);
		i->rx_err   = (unsigned long long)kpr_num_sys(0, "class/net/%s/statistics/rx_errors", *e);
		i->tx_err   = (unsigned long long)kpr_num_sys(0, "class/net/%s/statistics/tx_errors", *e);
		i->rx_drop  = (unsigned long long)kpr_num_sys(0, "class/net/%s/statistics/rx_dropped", *e);
		i->tx_drop  = (unsigned long long)kpr_num_sys(0, "class/net/%s/statistics/tx_dropped", *e);
	}
	kb_strv_free(names);
	*out = v;
	return n;
}

void kpr_net_free(KprIface *n) { free(n); }
