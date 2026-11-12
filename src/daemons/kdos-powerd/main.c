/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   kdos-powerd / kdos-power — the firewall's named services, and the look of
 *   the machine before anyone logs in
 *
 *     $ kdos-power firewall ssh on
 *     $ kdos-power accent amber
 *     $ kdos-power splash phosphor
 *     $ kdos-power suspend
 *
 * Some of the machine's settings are root's files and a desktop that is not
 * root still changes them: which named services the firewall lets in
 * (/etc/nftables.d/50-kdos-services.nft), the accent everything drawn before
 * a login wears — the boot menu, the splash, the text console and the login
 * screen (/etc/kdos/accent, limine.conf, /etc/vtrgb, the sddm account's
 * files) — and the splash's motion (kdos.splash= on the kernel command line).
 * This daemon is the one thing between them. It runs as root under ksvc and
 * offers the same verbs twice: a unix socket in /run for the `kdos` command,
 * and io.kdos.Power1 on the system bus for System Settings (bus.c). The bus
 * face alone also starts Samba once an account can share folders, for
 * kded-kdos. One binary answers to both names, dispatched on its basename,
 * the same shape as kpkg and kdos-tools.
 *
 * SUSPEND, POWER-OFF AND REBOOT ARE elogind's. Plasma asks login1, polkit
 * answers for the person at the machine, kscreenlocker locks on
 * PrepareForSleep, and the sleep hooks run from elogind's system-sleep
 * directory. `kdos-power suspend|poweroff|reboot` is kept as a client-side
 * name for `loginctl` and never reaches this daemon, so there is one path to
 * the power state and one lock in front of it.
 *
 * THE SOCKET'S AUTHORISATION IS SO_PEERCRED, checked on the connection rather
 * than trusted from the message. The kernel fills the credentials in, and
 * nothing in the protocol names a user, so there is nothing to forge. `ping`,
 * `firewall list` and `firewall knocks` answer anybody: a liveness check, a
 * file every user can read, and counts of packets that were turned away. The
 * three changes — `firewall`, `accent` and `splash` — answer root and `wheel`
 * with no prompt, the group sudo and polkit treat as admin, so an
 * administrator's terminal needs no agent. Any other caller gets whatever
 * polkit says about the same action for that process, asked without a
 * prompt: the shipped policy lets the person at the machine set the accent
 * and the splash and lets nobody outside wheel open a port. The bus face asks
 * polkit for every caller and may prompt, which is the difference between a
 * window with an agent behind it and a socket without one.
 *
 * THE SOCKET PROTOCOL IS ONE LINE PER CONNECTION: `ping`, `firewall list`,
 * `firewall knocks`, `firewall <name> on|off`, `accent <scheme>`,
 * `splash <motion>`, then a reply and the socket closes. No length prefixes,
 * no multiplexing, no state: a parser is an attack surface and this one is a
 * handful of strcmp.
 */

#ifndef _GNU_SOURCE
#define _GNU_SOURCE	/* struct ucred, setgroups */
#endif
#include <errno.h>
#include <fcntl.h>
#include <grp.h>
#include <poll.h>
#include <pwd.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <sys/wait.h>
#include <unistd.h>

#include "kbase.h"
#include "kcolor.h"
#include "powerd.h"

/*
 * A BUILD WITHOUT THE BUS FACE, for testing/selftest.sh on a host that has no
 * sd-bus to compile bus.c against. The recipe always builds bus.c; what this
 * leaves is exactly the daemon on a machine whose system bus is down, which
 * is a state the shipped binary has to survive anyway.
 */
#ifdef KP_NO_BUS
int kp_bus_open(void) { return -1; }
void kp_bus_close(void) {}
int kp_bus_fd(void) { return -1; }
short kp_bus_events(void) { return 0; }
int kp_bus_timeout_ms(void) { return -1; }
void kp_bus_dispatch(void) {}
void kp_bus_tick(void) {}
int kp_bus_peer_may(pid_t pid, uid_t uid, const char *action)
{
	(void)pid; (void)uid; (void)action;
	return 0;
}
void kp_bus_fw_changed(void) {}
void kp_bus_accent_changed(void) {}
#endif

/*
 * The socket, overridable so the daemon and its authorisation can be exercised
 * without root, which is the only way this gets tested at all, since the real
 * path is in /run. It grants nothing: authorisation is SO_PEERCRED on the
 * connection, so pointing a client at a different socket only ever reaches a
 * different daemon.
 */
static const char *sock_path(void)
{
	const char *p = getenv("KDOS_POWERD_SOCKET");
	return p && *p ? p : KP_SOCKET;
}

/*
 * The /etc this daemon writes, overridable for the same reason. A daemon
 * pointed at another /etc is a test: it also leaves the kernel's ruleset,
 * sshd, the boot menu and the login screen alone, so a run as root cannot
 * reach the machine it runs on through a fixture.
 */
static const char *kp_etc(void)
{
	const char *e = getenv("KDOS_POWERD_ETC");

	return e && *e ? e : "/etc";
}

static bool kp_testing(void)
{
	const char *e = getenv("KDOS_POWERD_ETC");

	return e && *e;
}

/*
 * THE PROGRAMS A CHANGE RUNS: nft, kdos-bootctl and Samba's service script.
 * On the machine they are the installed ones. Under a fixture /etc a test
 * names a stand-in for each (KDOS_POWERD_NFT, KDOS_POWERD_BOOTCTL,
 * KDOS_POWERD_SAMBA) or none, and NULL is "run nothing", so a test can assert
 * what would have been run and can never reach the kernel's ruleset, the ESP
 * or a service.
 */
#define KP_NFT     "/usr/sbin/nft"
#define KP_BOOTCTL "kdos-bootctl"

static const char *kp_prog(const char *env, const char *real)
{
	const char *p;

	if (!kp_testing())
		return real;
	p = getenv(env);
	return p && *p ? p : NULL;
}

/* ── the allowed set ───────────────────────────────────────────────────── */

/*
 * `kdos-powerd --explain <user>` — what the socket lets that user do, and why.
 *
 * A refused `kdos theme` or firewall toggle is otherwise unattributable: the
 * daemon's stderr goes to the supervisor's log, which is the one place a user
 * watching a setting not change will not look.
 *
 * IT ASKS libkbase THE QUESTION THE SOCKET ASKS, so the explanation cannot
 * drift from the decision. polkit's half is named and not asked: it is about a
 * process, and there is no process here to ask it about. Exit 0 when the
 * group rule admits the three changes, 1 otherwise.
 */
static int explain(const char *user)
{
	struct passwd *pw = getpwnam(user);

	if (!pw) {
		printf("%s: no passwd entry — refused\n", user);
		return 1;
	}
	if (pw->pw_uid == 0) {
		printf("%s: uid 0 — permitted every verb\n", user);
		return 0;
	}
	if (kb_uid_allowed(pw->pw_uid, KP_ADMIN)) {
		printf("%s: uid %u, primary gid %u, in " KP_ADMIN
		       " — permitted every verb\n", user,
		       (unsigned)pw->pw_uid, (unsigned)pw->pw_gid);
		return 0;
	}
	printf("%s: uid %u, primary gid %u, not in " KP_ADMIN
	       " — permitted ping, the firewall list and its knocks; "
	       "`firewall` only where polkit grants " KP_ACT_FIREWALL
	       " and `accent` and `splash` only where it grants "
	       KP_ACT_ACCENT ", each without a prompt (the shipped policy: the "
	       "accent and the splash from an active session at the machine), "
	       "otherwise refused\n", user, (unsigned)pw->pw_uid,
	       (unsigned)pw->pw_gid);
	return 1;
}

/* ── the firewall ──────────────────────────────────────────────────────── */

/*
 * THE FIREWALL, AS NAMED SERVICES AND NEVER AS PORTS.
 *
 * THE TABLE IS HERE, NOT IN THE KCM, AND THAT IS THE WHOLE POINT. A client
 * that could name a port could open any port; a client that can only name
 * `ssh` can open exactly what this table says `ssh` is. Both faces hand the
 * table out (`firewall list`, ListFirewallServices) rather than letting a
 * client carry a copy, so there is one answer to what a name means.
 *
 * EVERY RULE ENDS IN ` accept`. What comes before it is the row's match, and a
 * closed row's line in the file is that match counting into the row's knock
 * counter instead (fw_write).
 *
 * `what` IS READ BY A PERSON: the Firewall page shows it under the name, and
 * `firewall list` prints it. One plain sentence each, naming a program only
 * where the person would know it; the service scripts are in daemons.md.
 *
 * THE FILE IS REWRITTEN WHOLE from the names that are on. Merging into an
 * existing file would mean parsing nftables syntax to find what to remove, and
 * a parser that got it wrong would leave a port open that the KCM showed as
 * closed. What a person writes by hand goes in another file beside it, which
 * this never reads or touches.
 */
static const struct {
	const char *name;
	const char *rule;
	const char *what;
} FW[] = {
	{ "ssh",   "tcp dport 22 accept",   "Logging in to this computer over SSH" },
	{ "http",  "tcp dport 80 accept",   "A web server on this computer" },
	{ "https", "tcp dport 443 accept",  "A web server on this computer, over HTTPS" },
	{ "ipp",   "tcp dport 631 accept",  "Sharing a printer with other computers" },
	{ "smb",   "tcp dport 445 accept",  "Sharing files with Windows and other computers (SMB)" },
	/* kiwix-serve's default, and the one offline-content port this desktop
	 * ships a reason for. */
	{ "kiwix", "tcp dport 8080 accept", "Offline content served by Kiwix" },
	/* The LAN servers whose ports ship a service script: each is
	 * unreachable from another machine until its name is on, whatever its
	 * own configuration says it listens on. */
	{ "mqtt",  "tcp dport { 1883, 8883 } accept",
	  "An MQTT broker for devices on the network (Mosquitto)" },
	{ "xmpp",  "tcp dport 5222 accept", "A chat server for XMPP clients (Prosody)" },
	{ "nfs",   "tcp dport 2049 accept", "Sharing files with other computers over NFS" },
	{ "babel", "udp dport 6696 accept", "Mesh routing with Babel" },
	{ "nut",   "tcp dport 3493 accept",
	  "Telling other computers how the UPS is doing (NUT)" },
	{ "snmp",  "udp dport 161 accept",
	  "Answering SNMP queries about this computer" },
	/* Voice runs over UDP and falls back to the TCP control channel, so
	 * both halves are one rule, in the concatenated form `syncthing` uses. */
	{ "mumble",
	  "meta l4proto . th dport { tcp . 64738, udp . 64738 } accept",
	  "A Mumble voice chat server" },
	{ "caldav", "tcp dport 5232 accept",
	  "Shared calendars and contacts (Radicale)" },
	{ "mail",  "tcp dport { 25, 143, 465, 587, 993 } accept",
	  "A mail server for the network (Maddy)" },
	{ "irc",   "tcp dport { 6667, 6697 } accept",
	  "An IRC chat server" },
	/* SSDP discovery on 1900 is how a player finds the server; 8200 is
	 * where it then fetches the media. */
	{ "dlna",  "meta l4proto . th dport { tcp . 8200, udp . 1900 } accept",
	  "Sharing media with TVs and players (DLNA)" },
	{ "tryton", "tcp dport 8000 accept",
	  "GNU Health for Tryton clients" },
	/* The shipped /etc/caddy/Caddyfile's listener, in front of kiwix. */
	{ "caddy", "tcp dport 8443 accept", "The Caddy web server's HTTPS site" },
	/* mosh-server binds the first free port of this range, one per session,
	 * after an SSH login that `ssh` has to be on for. */
	{ "mosh",  "udp dport 60000-61000 accept",
	  "Mosh sessions to this computer (needs SSH too)" },
	/* ONE RULE FOR THREE PORTS, because the state is read back by matching
	 * the rule text: sync on 22000 over TCP and QUIC, and the local
	 * discovery broadcast on 21027 that is how two machines find each
	 * other with no server. */
	{ "syncthing",
	  "meta l4proto . th dport { tcp . 22000, udp . 22000, udp . 21027 } accept",
	  "Syncing folders with Syncthing" },
	/* KDE Connect listens, and a phone announces itself, on the first free
	 * port of this range over both TCP and UDP (1716 in practice); file
	 * transfers take further ports from the same range. */
	{ "kdeconnect",
	  "meta l4proto { tcp, udp } th dport 1714-1764 accept",
	  "KDE Connect with a phone on the network" },
	/* Desktop sharing. krfb serves VNC on its default 5900 and krdp serves
	 * RDP on its default 3389 (krdpserversettings.kcfg ListenPort), each
	 * only while sharing is switched on in a session; these open the port
	 * to the LAN, and a port moved in either program's settings stays shut
	 * until a hand-written rule beside this file opens it. */
	{ "vnc",   "tcp dport 5900 accept",
	  "Sharing this desktop over VNC (Krfb)" },
	{ "rdp",   "tcp dport 3389 accept",
	  "Sharing this desktop over RDP (KRdp)" },
	{ "xonotic", "udp dport 26000 accept",
	  "Hosting a Xonotic game for the network" },
};
#define FW_N ((int)(sizeof(FW) / sizeof(FW[0])))

#define FW_FILE   "nftables.d/50-kdos-services.nft"
#define FW_ACCEPT " accept"
#define FW_KNOCK  "kdos_knock_"
/* Before every knock match: this machine's own broadcasts loop back in on
 * the LAN interface (kdeconnectd's identity packet to 1716, syncthing's on
 * 21027, SSDP on 1900), and counted they would be a knock with nobody
 * knocking — a prompt at every boot. */
#define FW_FOREIGN "fib saddr type != local "

int kp_fw_count(void) { return FW_N; }
const char *kp_fw_name(int i) { return FW[i].name; }
const char *kp_fw_what(int i) { return FW[i].what; }

int kp_fw_find(const char *name)
{
	for (int i = 0; i < FW_N; i++)
		if (!strcmp(FW[i].name, name))
			return i;
	return -1;
}

/* The length of row i's match, its rule without the closing ` accept`; -1 for
 * a rule that does not end in one, which then gets no knock counter. */
static int fw_match_len(int i)
{
	size_t n = strlen(FW[i].rule), k = strlen(FW_ACCEPT);

	if (n <= k || strcmp(FW[i].rule + n - k, FW_ACCEPT))
		return -1;
	return (int)(n - k);
}

/* Which names the file currently carries, by looking for each table entry's
 * own rule text — the file is this program's output, so an exact match is the
 * right test and a partial one would report a name that is not really on. A
 * knock line carries the match and no `accept`, so it never reads as open. */
void kp_fw_state(int *on)
{
	char path[320], buf[8192];

	for (int i = 0; i < FW_N; i++)
		on[i] = 0;
	snprintf(path, sizeof(path), "%s/%s", kp_etc(), FW_FILE);
	if (kb_read_file(path, buf, sizeof(buf)) <= 0)
		return;
	for (int i = 0; i < FW_N; i++)
		if (strstr(buf, FW[i].rule))
			on[i] = 1;
}

/* Put back what the file held before fw_write replaced it, or take the file
 * away if there was none. The check has to read the new file through
 * /etc/nftables.conf's include, so the new one is on disk before the answer is
 * known; a refused ruleset left there is what the next boot loads and what
 * `firewall list` reads back, while the kernel still runs the old one. */
static void fw_restore(const char *path, const char *prev)
{
	if (prev)
		(void)kb_write_file_atomic(path, prev);
	else
		unlink(path);
}

/* `nft <flag> -f <etc>/nftables.conf`, or nothing to run: 0 then. */
static int fw_nft_load(const char *nft, bool check)
{
	char conf[320];
	KbArgv a = { 0 };

	if (!nft)
		return 0;
	snprintf(conf, sizeof(conf), "%s/nftables.conf", kp_etc());
	kb_argv_add(&a, nft);
	if (check)
		kb_argv_add(&a, "--check");
	kb_argv_add(&a, "-f");
	kb_argv_add(&a, conf);
	kb_argv_end(&a);
	return kb_run(&a);
}

/*
 * THE FILE, and the knock counters in it.
 *
 * AN OPEN NAME IS ITS RULE. A CLOSED NAME IS ITS MATCH COUNTED: the same
 * packets the rule would have accepted, from another machine (FW_FOREIGN),
 * are counted into the named counter `kdos_knock_<name>`; they and this
 * machine's own fall through to the chain's policy, which drops them. The Firewall page and kdos-fwask read the counters to say that
 * something was turned away — KDE Connect's phone, a krdp client — and ask
 * whether to let it in, by name. A reload deletes and rebuilds the table, so
 * every change starts every counter from zero again. A port a hand-written
 * file beside this one opens is counted here too, because its rule comes
 * after this file's.
 */
static int fw_write(const int *on, char *out, size_t nout)
{
	const char *nft = kp_prog("KDOS_POWERD_NFT", KP_NFT);
	char path[320], tmp[336];
	char *prev;
	FILE *f;

	snprintf(path, sizeof(path), "%s/%s", kp_etc(), FW_FILE);
	snprintf(tmp, sizeof(tmp), "%s.new", path);
	f = fopen(tmp, "w");
	if (!f) {
		snprintf(out, nout, "err cannot write the rules\n");
		return -1;
	}
	fprintf(f,
		"# Written by kdos-powerd, for the Firewall settings page and\n"
		"# `kdos-power firewall`. Rewritten WHOLE on every change, so a\n"
		"# rule it does not recognise is dropped — anything hand-made\n"
		"# belongs in a file beside this one, which this never touches.\n"
		"#\n"
		"# An open name is its rule. A closed name's packets from another\n"
		"# machine are counted in kdos_knock_<name>; every closed name's\n"
		"# packets then meet the chain's policy.\n"
		"#\n"
		"# No `type`/`hook` line: that is what re-opens the existing\n"
		"# input chain rather than declaring a second one.\n"
		"\n"
		"table inet filter {\n");
	for (int i = 0; i < FW_N; i++)
		if (!on[i] && fw_match_len(i) > 0)
			fprintf(f, "\tcounter " FW_KNOCK "%s {\n\t}\n", FW[i].name);
	fprintf(f, "\tchain input {\n");
	for (int i = 0; i < FW_N; i++) {
		int m = fw_match_len(i);

		if (on[i])
			fprintf(f, "\t\t%s\n", FW[i].rule);
		else if (m > 0)
			fprintf(f, "\t\t" FW_FOREIGN "%.*s counter name \""
				FW_KNOCK "%s\"\n", m, FW[i].rule, FW[i].name);
	}
	fprintf(f, "\t}\n}\n");
	fflush(f);
	fsync(fileno(f));
	if (fclose(f) != 0) {
		unlink(tmp);
		snprintf(out, nout, "err cannot write the rules\n");
		return -1;
	}
	prev = kb_read_whole(path, NULL);
	if (!prev && errno != ENOENT) {
		/* A file that is there but cannot be read could not be put
		 * back if the new one is refused. */
		unlink(tmp);
		snprintf(out, nout, "err cannot read the current rules\n");
		return -1;
	}
	if (rename(tmp, path) != 0) {
		unlink(tmp);
		free(prev);
		snprintf(out, nout, "err cannot write the rules\n");
		return -1;
	}

	/*
	 * APPLIED BY RELOADING THE WHOLE FILE, because `/etc/nftables.conf`
	 * deletes and rebuilds `inet filter` and the included files are only
	 * reached from there. Only that table is replaced: netavark's and
	 * NetworkManager's NAT tables stay, so a toggle does not cut off running
	 * containers or hotspot clients. `nft --check` first: a bad ruleset
	 * refused is the previous one still in the kernel, and a bad ruleset
	 * half-applied is a machine with no firewall. Either refusal puts the
	 * previous file back, so the disk says what the kernel runs.
	 */
	if (fw_nft_load(nft, true) != 0) {
		fw_restore(path, prev);
		free(prev);
		snprintf(out, nout,
			 "err the ruleset would not load; nothing changed\n");
		return -1;
	}
	if (fw_nft_load(nft, false) != 0) {
		fw_restore(path, prev);
		free(prev);
		snprintf(out, nout, "err the ruleset did not apply\n");
		return -1;
	}
	free(prev);
	return 0;
}

/*
 * `ssh` IS A DAEMON AS WELL AS A PORT. sshd ships disabled — the marker
 * /etc/service.disabled/sshd is in the image — because the live accounts have
 * known passwords, and opening port 22 is how the desktop turns it on. On
 * takes the marker away and starts sshd; off puts the marker back and stops
 * it, so a closed port never hides a listening daemon that one more toggle,
 * or a hand-written rule beside this file, would expose. The marker is what
 * the next boot obeys; the start and stop are this boot.
 */
#define FW_SSHD_INIT "/etc/init.d/70_sshd.sh"

static void fw_sshd(bool on)
{
	char dir[320], marker[352];

	snprintf(dir, sizeof(dir), "%s/service.disabled", kp_etc());
	snprintf(marker, sizeof(marker), "%s/sshd", dir);
	if (on) {
		unlink(marker);
	} else {
		(void)mkdir(dir, 0755);
		(void)kb_write_file(marker, "");
	}
	if (kp_testing())
		return;

	KbArgv a = { 0 };

	kb_argv_add(&a, FW_SSHD_INIT);
	kb_argv_add(&a, on ? "start" : "stop");
	kb_argv_end(&a);
	(void)kb_run(&a);
}

int kp_fw_set(const char *name, bool open, char *out, size_t nout)
{
	int on[FW_N];
	int i = kp_fw_find(name);

	/* A NAME THIS TABLE DOES NOT CARRY IS NOT A PORT TO OPEN. */
	if (i < 0) {
		snprintf(out, nout, "err no service called that\n");
		return -1;
	}
	kp_fw_state(on);
	on[i] = open;
	if (fw_write(on, out, nout) != 0)
		return -1;
	if (!strcmp(name, "ssh"))
		fw_sshd(open);
	snprintf(out, nout, "ok %s %s\n", name, open ? "on" : "off");
	kp_bus_fw_changed();
	return 0;
}

/*
 * THE COUNTERS ARE THE KERNEL'S, read with `nft list counters`, which prints
 *
 *     table inet filter {
 *             counter kdos_knock_ssh {
 *                     packets 12 bytes 720
 *             }
 *     …
 *
 * A counter the kernel does not hold — a file written before the counters
 * existed, a hand-edited one — is -1, not 0: nothing was counted, which is
 * not the same as nothing having knocked. An open name's counter is -1 too,
 * because it has none.
 */
int kp_fw_knocks(long long *knocks)
{
	const char *nft = kp_prog("KDOS_POWERD_NFT", KP_NFT);
	int on[FW_N], cur = -1;
	KbArgv a = { 0 };
	KbBuf b = { 0 };

	for (int i = 0; i < FW_N; i++)
		knocks[i] = -1;
	if (!nft)
		return 0;
	kb_argv_add(&a, nft);
	kb_argv_add(&a, "list");
	kb_argv_add(&a, "counters");
	kb_argv_add(&a, "table");
	kb_argv_add(&a, "inet");
	kb_argv_add(&a, "filter");
	kb_argv_end(&a);
	if (kb_run_capture_buf(&a, &b) != 0) {
		kb_buf_free(&b);
		return -1;
	}
	kp_fw_state(on);
	for (char *ln = b.p; ln && *ln;) {
		char *nl = strchr(ln, '\n');
		char name[64];
		unsigned long long pk;

		if (nl)
			*nl = '\0';
		if (sscanf(ln, " counter " FW_KNOCK "%63[a-z0-9] {", name) == 1)
			cur = kp_fw_find(name);
		else if (cur >= 0 && sscanf(ln, " packets %llu", &pk) == 1) {
			if (!on[cur])
				knocks[cur] = (long long)pk;
			cur = -1;
		}
		ln = nl ? nl + 1 : NULL;
	}
	kb_buf_free(&b);
	return 0;
}

/* One row per service, then `ok`: the client draws this and carries no table
 * of its own. */
static void fw_list(char *out, size_t nout)
{
	int on[FW_N];
	size_t used = 0;

	kp_fw_state(on);
	for (int i = 0; i < FW_N; i++) {
		int k = snprintf(out + used, nout - used, "%s\t%s\t%s\n",
				 FW[i].name, on[i] ? "on" : "off", FW[i].what);

		if (k < 0 || (size_t)k >= nout - used)
			break;
		used += (size_t)k;
	}
	snprintf(out + used, nout - used, "ok\n");
}

/* One `<name>\t<packets>` row per closed name the kernel counts, then `ok`. */
static int fw_knock_list(char *out, size_t nout)
{
	long long k[FW_N];
	size_t used = 0;

	if (kp_fw_knocks(k) != 0) {
		snprintf(out, nout, "err the counters could not be read\n");
		return -1;
	}
	for (int i = 0; i < FW_N; i++) {
		if (k[i] < 0)
			continue;
		int w = snprintf(out + used, nout - used, "%s\t%lld\n",
				 FW[i].name, k[i]);

		if (w < 0 || (size_t)w >= nout - used)
			break;
		used += (size_t)w;
	}
	snprintf(out + used, nout - used, "ok\n");
	return 0;
}

/* `list`, `knocks` or `<name> on|off`. */
static int fw_verb(const char *arg, char *out, size_t nout)
{
	char name[64] = "", state[16] = "";

	if (!strcmp(arg, "list")) {
		fw_list(out, nout);
		return 0;
	}
	if (!strcmp(arg, "knocks"))
		return fw_knock_list(out, nout);
	if (sscanf(arg, "%63s %15s", name, state) != 2) {
		snprintf(out, nout,
			 "err usage: firewall list|knocks|<name> on|off\n");
		return -1;
	}
	bool want = !strcmp(state, "on");

	if (!want && strcmp(state, "off")) {
		snprintf(out, nout, "err a service is on or off\n");
		return -1;
	}
	return kp_fw_set(name, want, out, nout);
}

/* ── file sharing ──────────────────────────────────────────────────────── */

/*
 * SAMBA RUNS ONCE AN ACCOUNT CAN SHARE. 83_samba.sh starts smbd at boot only
 * when the sambashare group has a member, /var/lib/samba/usershares holds a
 * share or smb.conf defines one of its own, so a machine where nobody shares
 * listens on nothing. smbd must be up before the first folder can be shared:
 * `net usershare add` asks it to turn the ACL's names into SIDs. An account
 * the Share tab has just added to the group is a member from its next login,
 * so that login's kded-kdos asks the bus face for this, and the script's own
 * test decides again.
 *
 * NO ARGUMENT AND ONE FIXED COMMAND. The caller names nothing, so all it can
 * cause is what the next boot would do anyway. A service an administrator
 * disabled (/etc/service.disabled/samba) stays down: the marker is what the
 * boot obeys, and this obeys it too. A supervised service started twice
 * starts nothing the second time, so a repeated call costs one script run.
 * Nothing stops smbd again: removing the last share leaves it serving nothing
 * until the next boot, which then does not start it.
 */
#define KP_SAMBA_INIT "/etc/init.d/83_samba.sh"

int kp_share_start(char *out, size_t nout)
{
	const char *init = kp_prog("KDOS_POWERD_SAMBA", KP_SAMBA_INIT);
	char marker[352];
	KbArgv a = { 0 };

	snprintf(marker, sizeof(marker), "%s/service.disabled/samba",
		 kp_etc());
	if (access(marker, F_OK) == 0) {
		snprintf(out, nout, "err samba is disabled on this machine\n");
		return -1;
	}
	if (!init) {
		snprintf(out, nout, "ok\n");
		return 0;
	}
	if (access(init, X_OK) != 0) {
		snprintf(out, nout, "err samba is not installed\n");
		return -1;
	}
	kb_argv_add(&a, init);
	kb_argv_add(&a, "start");
	kb_argv_end(&a);
	if (kb_run(&a) != 0) {
		snprintf(out, nout, "err samba's service script failed\n");
		return -1;
	}
	snprintf(out, nout, "ok\n");
	return 0;
}

/* ── kdos-bootctl, which owns limine.conf ──────────────────────────────── */

/*
 * `kdos-bootctl <verb> <word>`, exec'd rather than linked: which keys a theme
 * owns in limine.conf, and how an entry's command line is rewritten, are the
 * bootloader tool's rules, and a copy of them in a daemon is a copy that goes
 * stale the next time Limine gains a key. false when it is not installed or
 * refused — a live medium is read-only and a machine may have no ESP.
 */
static bool run_bootctl(const char *verb, const char *word)
{
	const char *prog = kp_prog("KDOS_POWERD_BOOTCTL", KP_BOOTCTL);
	KbArgv a = { 0 };

	if (!prog || (!kp_testing() && !kb_have_prog(prog)))
		return false;
	kb_argv_add(&a, prog);
	kb_argv_add(&a, verb);
	kb_argv_add(&a, word);
	kb_argv_end(&a);
	return kb_run(&a) == 0;
}

/* ── the login screen ──────────────────────────────────────────────────── */

/*
 * THE LOGIN SCREEN WEARS THE ACCENT TOO. The greeter runs as the sddm account,
 * and its look is that account's own ~/.config/kdeglobals, plasmarc and
 * kcminputrc — the three files System Settings' "Apply Plasma Settings" copies
 * there — plus the picture the KDOS greeter theme shows,
 * ~/kdos-background.png. One set per accent is built into
 * /usr/share/kdos/greeter/<accent>/, a light accent's carrying its dark
 * partner because the greeter never follows day and night, so this copies
 * files and generates nothing as root.
 *
 * WRITTEN AS THE GREETER'S OWN ACCOUNT. sddm's home is that account's to
 * change, so a root writer there would follow whatever link the account had
 * left in place of a file. A child with sddm's uid and gid and no other group
 * makes the copy, so the files and ~/.config are the account's own, which the
 * greeter's KWin needs in order to write kwinoutputconfig.json beside them.
 * The home itself is prepared by root the way kdos-dm prepares it at every
 * start: made when missing, and the directory alone handed to the account,
 * 0750, only when it is under /var/lib, is no link and climbs nowhere: the
 * image seeds the home with its default accent's set, and a machine whose
 * home is missing or root's is one where `kdos theme` runs before kdos-dm.
 *
 * ALL OR NOTHING ON THE SOURCE: a set missing a file leaves the greeter as it
 * was rather than half in another accent. Each file is replaced by a rename,
 * so the greeter never reads a half-written one.
 *
 * A fixture /etc reaches no account: a test names the sets and a home of its
 * own (KDOS_POWERD_GREETER, KDOS_POWERD_SDDM_HOME), and the copy then runs as
 * whoever ran the test.
 */
#define KP_GREETER_SETS "/usr/share/kdos/greeter"
#define KP_GREETER_USER "sddm"
#define KP_GREETER_BG   "background.png"
#define KP_GREETER_PIC  "kdos-background.png"

static const char *const KP_GREETER_FILES[] = {
	"kdeglobals", "plasmarc", "kcminputrc", NULL
};

static int copy_atomic(const char *src, const char *dst)
{
	char tmp[1024], buf[65536];
	int in, out, ok = 0;

	if (snprintf(tmp, sizeof(tmp), "%s.new", dst) >= (int)sizeof(tmp))
		return -1;
	unlink(tmp);
	in = open(src, O_RDONLY | O_CLOEXEC);
	if (in < 0)
		return -1;
	out = open(tmp, O_WRONLY | O_CREAT | O_EXCL | O_NOFOLLOW | O_CLOEXEC,
		   0644);
	if (out < 0) {
		close(in);
		return -1;
	}
	for (;;) {
		ssize_t n = read(in, buf, sizeof(buf));

		if (n < 0 && errno == EINTR)
			continue;
		if (n < 0) {
			ok = -1;
			break;
		}
		if (n == 0)
			break;
		for (ssize_t off = 0; off < n && ok == 0;) {
			ssize_t w = write(out, buf + off, (size_t)(n - off));

			if (w < 0 && errno == EINTR)
				continue;
			if (w <= 0)
				ok = -1;
			else
				off += w;
		}
		if (ok)
			break;
	}
	close(in);
	if (ok == 0 && fsync(out) != 0)
		ok = -1;
	if (close(out) != 0)
		ok = -1;
	if (ok == 0 && rename(tmp, dst) != 0)
		ok = -1;
	if (ok)
		unlink(tmp);
	return ok;
}

/* The copy itself, run in the child: `set` is one accent's directory. */
static int greeter_copy(const char *set, const char *home)
{
	/* Sized for greeter_set's `set` and `home` plus a name each. */
	char src[416], cfg[280], dst[320];

	for (int i = 0; KP_GREETER_FILES[i]; i++) {
		snprintf(src, sizeof(src), "%s/%s", set, KP_GREETER_FILES[i]);
		if (access(src, R_OK) != 0)
			return -1;
	}
	snprintf(src, sizeof(src), "%s/%s", set, KP_GREETER_BG);
	if (access(src, R_OK) != 0)
		return -1;

	snprintf(cfg, sizeof(cfg), "%s/.config", home);
	if (mkdir(cfg, 0755) != 0 && errno != EEXIST)
		return -1;
	for (int i = 0; KP_GREETER_FILES[i]; i++) {
		snprintf(src, sizeof(src), "%s/%s", set, KP_GREETER_FILES[i]);
		snprintf(dst, sizeof(dst), "%s/%s", cfg, KP_GREETER_FILES[i]);
		if (copy_atomic(src, dst) != 0)
			return -1;
	}
	snprintf(src, sizeof(src), "%s/%s", set, KP_GREETER_BG);
	snprintf(dst, sizeof(dst), "%s/%s", home, KP_GREETER_PIC);
	return copy_atomic(src, dst);
}

/* kdos-dm's rule for the greeter's home: /var/lib/<something>, with no `.`
 * or `..` component, so a home of / or one outside /var/lib is never handed
 * to the account. */
static bool greeter_home_ok(const char *h)
{
	if (strncmp(h, "/var/lib/", 9) || !h[9])
		return false;
	for (const char *p = h; (p = strchr(p, '/')); p++)
		if (p[1] == '.' && (!p[2] || p[2] == '/' ||
				    (p[2] == '.' && (!p[3] || p[3] == '/'))))
			return false;
	return true;
}

static void greeter_home(const char *home, uid_t uid, gid_t gid)
{
	struct stat sb;

	if (!greeter_home_ok(home))
		return;
	if (lstat(home, &sb) != 0) {
		if (errno != ENOENT || mkdir(home, 0750) != 0)
			return;
	} else if (!S_ISDIR(sb.st_mode)) {
		return;
	}
	/* The path was just checked to be a directory and no link, under a
	 * /var/lib only root writes, so the account cannot swap it between
	 * the check and the chown. */
	if (chown(home, uid, gid) == 0)
		(void)chmod(home, 0750);
}

static bool greeter_set(const char *accent)
{
	const char *sets;
	char home[256], set[384];
	uid_t uid = 0;
	gid_t gid = 0;
	bool drop = !kp_testing();
	int st = 0;
	pid_t pid;

	if (drop) {
		struct passwd *pw = getpwnam(KP_GREETER_USER);

		if (!pw || pw->pw_uid == 0 || !pw->pw_dir ||
		    pw->pw_dir[0] != '/')
			return false;
		sets = KP_GREETER_SETS;
		uid = pw->pw_uid;
		gid = pw->pw_gid;
		kb_strlcpy(home, pw->pw_dir, sizeof(home));
		if (geteuid() == 0)
			greeter_home(home, uid, gid);
	} else {
		const char *h = getenv("KDOS_POWERD_SDDM_HOME");

		sets = getenv("KDOS_POWERD_GREETER");
		if (!sets || !*sets || !h || !*h)
			return false;
		kb_strlcpy(home, h, sizeof(home));
	}
	snprintf(set, sizeof(set), "%s/%s", sets, accent);

	pid = fork();
	if (pid < 0)
		return false;
	if (pid == 0) {
		if (drop && (setgroups(0, NULL) != 0 || setgid(gid) != 0 ||
			     setuid(uid) != 0 || getuid() != uid ||
			     geteuid() != uid))
			_exit(1);
		umask(022);
		_exit(greeter_copy(set, home) == 0 ? 0 : 1);
	}
	while (waitpid(pid, &st, 0) < 0)
		if (errno != EINTR)
			return false;
	return WIFEXITED(st) && WEXITSTATUS(st) == 0;
}

/* ── the accent, and the root-owned files that carry it ───────────────────
 *
 * `/etc/kdos/accent`, `/etc/vtrgb`, the ESP's `limine.conf` and the sddm
 * account's files are what is drawn before any session exists: the boot
 * menu, the splash, the text console and the login screen. Everything after
 * the login is the user's own files, which `kdos theme` writes as the user.
 *
 * ITS ARGUMENT IS TIGHTER THAN THE FIREWALL'S. An accent is one of eight
 * strings compiled into libkcolor. There is no path to aim and nothing to
 * traverse: a name that is not a scheme names nothing at all, and `kcol_find`
 * is the whole check — which is also what makes it safe to join onto the
 * greeter sets' directory.
 */
int kp_accent_set(const char *name, char *out, size_t nout)
{
	const KcolScheme *sc = kcol_find(name);
	/* `path` is `dir` plus a name, so it is the longer of the two — sized
	 * the same, a compiler that can see the concatenation refuses the
	 * build rather than the truncation. */
	char path[352], dir[320];

	if (!sc) {
		snprintf(out, nout, "err not an accent\n");
		return -1;
	}

	/* The name, for kdos-splash: it is started by the INITRAMFS, before any
	 * root filesystem exists, so it cannot read this at the moment it
	 * starts. `rcS` reads it after switch_root and tells the running splash
	 * over the FIFO it is already holding. */
	snprintf(dir, sizeof(dir), "%s/kdos", kp_etc());
	kb_mkdir_p(dir);
	snprintf(path, sizeof(path), "%s/accent", dir);
	if (kb_write_file_atomic(path, sc->name) != 0) {
		snprintf(out, nout, "err cannot write the accent\n");
		return -1;
	}
	kp_bus_accent_changed();

	/*
	 * THE BOOT MENU AND THE TEXT CONSOLE ARE kdos-bootctl's (`theme`), and
	 * the login screen is the copy above. NEITHER FAILING IS THIS VERB'S
	 * FAILURE: a live medium is read-only and a machine may have no ESP or
	 * no greeter; the accent still applied to everything else, and an `err`
	 * would make `kdos theme` look broken on the ISO, where every other
	 * surface retints. The reply names what stayed as it was.
	 */
	bool menu = run_bootctl("theme", sc->name);
	bool greeter = greeter_set(sc->name);

	if (menu && greeter)
		snprintf(out, nout, "ok %s\n", sc->name);
	else
		snprintf(out, nout, "ok %s (%s unchanged)\n", sc->name,
			 !menu && !greeter ? "boot menu and login screen"
			 : !menu ? "boot menu" : "login screen");
	return 0;
}

const char *kp_accent_get(void)
{
	char path[352], buf[64];
	const KcolScheme *sc;

	snprintf(path, sizeof(path), "%s/kdos/accent", kp_etc());
	if (kb_read_line_file(path, buf, sizeof(buf)) > 0 &&
	    (sc = kcol_find(buf)))
		return sc->name;
	return KCOL_DEFAULT_NAME;
}

/* ── the splash's motion ───────────────────────────────────────────────────
 *
 * kdos-splash draws a flat mark in Modern and a CRT power-on in Phosphor
 * mode. It runs in the initramfs before any root filesystem is mounted, so
 * the kernel command line is the one thing it can read, and the word is
 * `kdos.splash=` on every KDOS entry in limine.conf: kdos-bootctl's `splash`
 * verb writes it. The argument is one of two words, compared whole.
 */
static const char *const KP_MOTIONS[] = { "phosphor", "modern", NULL };

bool kp_splash_ok(const char *motion)
{
	for (int i = 0; KP_MOTIONS[i]; i++)
		if (!strcmp(motion, KP_MOTIONS[i]))
			return true;
	return false;
}

int kp_splash_set(const char *motion, char *out, size_t nout)
{
	if (!kp_splash_ok(motion)) {
		snprintf(out, nout,
			 "err not a splash motion (phosphor or modern)\n");
		return -1;
	}
	/* Unchanged is not an error, for the accent's reason: on the ISO the
	 * boot menu cannot be written, and Phosphor mode still turns on. */
	if (run_bootctl("splash", motion))
		snprintf(out, nout, "ok %s\n", motion);
	else
		snprintf(out, nout, "ok %s (boot menu unchanged)\n", motion);
	return 0;
}

/* ── the daemon ────────────────────────────────────────────────────────── */

/*
 * MAY THIS PEER MAKE A CHANGE? Root and KP_ADMIN by SO_PEERCRED, asked of
 * libkbase like every root daemon here asks it; otherwise polkit's answer for
 * the same action as the bus face, about this process and without a prompt,
 * because a socket has no agent behind it to show one.
 */
static bool peer_may(const struct ucred *cr, const char *action)
{
	if (kb_uid_allowed(cr->uid, KP_ADMIN))
		return true;
	return kp_bus_peer_may(cr->pid, cr->uid, action) == 1;
}

static void answer(int c)
{
	struct ucred cred = {0};
	socklen_t len = sizeof(cred);
	/* A client that connects and never speaks must not stall the bus
	 * face; the loop is single-threaded on purpose. */
	struct timeval tv = { .tv_sec = 2 };

	setsockopt(c, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
	if (getsockopt(c, SOL_SOCKET, SO_PEERCRED, &cred, &len) != 0) {
		(void)!write(c, "err not permitted\n", 18);
		return;
	}

	char buf[KP_MAX] = {0};
	ssize_t n = read(c, buf, sizeof(buf) - 1);

	if (n <= 0)
		return;
	buf[n] = '\0';
	buf[strcspn(buf, "\r\n")] = '\0';

	/* The list reply is many rows, so the reply buffer is sized for it. */
	static char msg[4096];
	enum { V_NONE, V_FIREWALL, V_ACCENT, V_SPLASH } verb = V_NONE;
	const char *action = NULL;

	if (!strcmp(buf, "ping")) {
		snprintf(msg, sizeof(msg), "ok\n");
	} else if (!strcmp(buf, "firewall list") ||
		   !strcmp(buf, "firewall knocks")) {
		fw_verb(buf + 9, msg, sizeof(msg));
	} else if (!strncmp(buf, "firewall ", 9)) {
		verb = V_FIREWALL;
		action = KP_ACT_FIREWALL;
	} else if (!strncmp(buf, "accent ", 7)) {
		verb = V_ACCENT;
		action = KP_ACT_ACCENT;
	} else if (!strncmp(buf, "splash ", 7)) {
		verb = V_SPLASH;
		action = KP_ACT_ACCENT;
	} else {
		snprintf(msg, sizeof(msg), "err unknown command\n");
	}

	if (verb != V_NONE && !peer_may(&cred, action)) {
		/* Refused with a reason, and logged: an unexplained refusal is
		 * unattributable, and this is the line that attributes it. */
		fprintf(stderr, "kdos-powerd: refused uid %u %.*s (not root, "
			"not in %s, and polkit did not grant %s)\n",
			(unsigned)cred.uid, (int)strcspn(buf, " "), buf,
			KP_ADMIN, action);
		snprintf(msg, sizeof(msg), "err not permitted\n");
	} else if (verb == V_FIREWALL) {
		fw_verb(buf + 9, msg, sizeof(msg));
	} else if (verb == V_ACCENT) {
		kp_accent_set(buf + 7, msg, sizeof(msg));
	} else if (verb == V_SPLASH) {
		kp_splash_set(buf + 7, msg, sizeof(msg));
	}
	(void)!write(c, msg, strlen(msg));
}

static int serve(void)
{
	const char *path = sock_path();

	if (geteuid() != 0) {
		/*
		 * Refused on the real socket, allowed on a test one — and said
		 * out loud, because an unprivileged daemon answers `ok` to a
		 * toggle whose ruleset it cannot load.
		 */
		if (!strcmp(path, KP_SOCKET)) {
			fprintf(stderr, "kdos-powerd: must run as root\n");
			return 1;
		}
		fprintf(stderr, "kdos-powerd: not root — the ruleset, sshd, "
				"the boot menu and the login screen will not "
				"change\n");
	}

	int srv = socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0);

	if (srv < 0) {
		fprintf(stderr, "kdos-powerd: socket: %s\n", strerror(errno));
		return 1;
	}

	struct sockaddr_un addr = { .sun_family = AF_UNIX };

	snprintf(addr.sun_path, sizeof(addr.sun_path), "%s", path);
	/* A socket left behind by a previous run would make bind() fail with
	 * EADDRINUSE forever; the supervisor would then restart us in a loop. */
	unlink(path);
	if (bind(srv, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
		fprintf(stderr, "kdos-powerd: bind %s: %s\n", path,
			strerror(errno));
		close(srv);
		return 1;
	}
	/*
	 * 0666 on the socket, with SO_PEERCRED as the real gate. A mode that
	 * LOOKED like the authorisation would invite someone to weaken the
	 * credential check because "the mode already handles it".
	 */
	chmod(path, 0666);
	if (listen(srv, 8) < 0) {
		fprintf(stderr, "kdos-powerd: listen: %s\n", strerror(errno));
		close(srv);
		return 1;
	}

	/* A client that hangs up before its reply must cost the write, never
	 * the daemon. Children get their signals back from libkbase. */
	signal(SIGPIPE, SIG_IGN);

	/*
	 * THE SOCKET COMES FIRST AND DOES NOT WAIT FOR THE BUS. The `kdos`
	 * command must still reach the firewall on a machine whose system bus
	 * is down, so a bus that cannot be reached is retried from the loop
	 * while the socket answers.
	 */
	(void)kp_bus_open();

	for (;;) {
		struct pollfd fds[2] = {
			{ .fd = srv, .events = POLLIN },
			{ .fd = kp_bus_fd(), .events = kp_bus_events() },
		};
		int nfds = fds[1].fd >= 0 ? 2 : 1;
		int rc = poll(fds, (nfds_t)nfds, kp_bus_timeout_ms());

		if (rc < 0) {
			if (errno == EINTR)
				continue;
			break;
		}
		if (nfds == 2)
			kp_bus_dispatch();
		kp_bus_tick();
		if (!(fds[0].revents & POLLIN))
			continue;
		/* CLOEXEC, because a verb runs programs that outlive it:
		 * `firewall ssh on` starts 70_sshd.sh, whose supervisor and
		 * sshd would otherwise inherit this connection and hold it
		 * open, so the client never reads the end of its reply and
		 * waits for ever. */
		int c = accept4(srv, NULL, NULL, SOCK_CLOEXEC);

		if (c < 0)
			continue;
		answer(c);
		close(c);
	}

	kp_bus_close();
	close(srv);
	unlink(path);
	return 1;
}

/* ── the client ────────────────────────────────────────────────────────── */

static int usage(void)
{
	fprintf(stderr,
		"usage: kdos-power suspend|poweroff|reboot   (through loginctl)\n"
		"       kdos-power ping\n"
		"       kdos-power firewall list|knocks|<service> on|off\n"
		"       kdos-power accent <scheme>\n"
		"       kdos-power splash phosphor|modern\n");
	return 2;
}

/*
 * One line, one connection: the whole protocol. 0 when the daemon answered
 * `ok`, 1 otherwise.
 *
 * `show` is the firewall verb: its reply is the product, a row per service
 * for `list` and `knocks` and the `ok`/`err` line for a toggle, so all of it
 * goes to STDOUT, where a caller capturing the table reads it. The reply is
 * read to EOF into a buffer as large as the daemon's, or a table of more than
 * one row arrives as its first line.
 */
static int request(const char *cmd, bool show)
{
	int fd = socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0);

	if (fd < 0)
		return 1;

	struct sockaddr_un addr = { .sun_family = AF_UNIX };

	snprintf(addr.sun_path, sizeof(addr.sun_path), "%s", sock_path());
	if (connect(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
		fprintf(stderr, "kdos-power: no kdos-powerd — `service "
				"start powerd`\n");
		close(fd);
		return 1;
	}

	char line[KP_MAX];
	int n = snprintf(line, sizeof(line), "%s\n", cmd);

	if (n < 0 || n >= (int)sizeof(line) ||
	    write(fd, line, (size_t)n) != n) {
		close(fd);
		return 1;
	}

	static char reply[4096];
	size_t got = 0;

	while (got < sizeof(reply) - 1) {
		ssize_t r = read(fd, reply + got, sizeof(reply) - 1 - got);

		if (r < 0 && errno == EINTR)
			continue;
		if (r <= 0)
			break;
		got += (size_t)r;
	}
	reply[got] = '\0';
	close(fd);
	if (got == 0) {
		fprintf(stderr, "kdos-power: kdos-powerd closed without an "
				"answer\n");
		return 1;
	}
	/* The verdict is the LAST line: a list is its rows, then `ok`. */
	char *last = reply + got;

	while (last > reply && (last[-1] == '\n' || last[-1] == '\r'))
		last--;
	while (last > reply && last[-1] != '\n')
		last--;
	int ok = !strncmp(last, "ok", 2);

	if (show) {
		fputs(reply, stdout);
		return ok ? 0 : 1;
	}
	if (ok) {
		/* An accent or a motion that applied everywhere but the boot
		 * menu or the login screen says so; the caller decides whether
		 * that matters. */
		const char *note = strchr(reply, '(');

		if (note)
			fprintf(stderr, "kdos-power: %.*s\n",
				(int)strcspn(note, "\r\n"), note);
		return 0;
	}
	reply[strcspn(reply, "\r\n")] = '\0';
	fprintf(stderr, "kdos-power: %s\n", reply);
	return 1;
}

/*
 * `suspend`, `poweroff` and `reboot` ARE loginctl's, exec'd by argv. The
 * client runs as the caller, so login1 sees the caller, polkit answers about
 * the caller's session, and kscreenlocker's sleep inhibitor gets its lock in
 * before the machine goes down — the same path the Leave menu takes.
 */
static int to_loginctl(const char *verb)
{
	char *argv[] = { "loginctl", (char *)verb, NULL };

	execvp(argv[0], argv);
	fprintf(stderr, "kdos-power: loginctl: %s — elogind owns suspend, "
			"power-off and reboot\n", strerror(errno));
	return 1;
}

static int client(int argc, char **argv)
{
	const char *cmd = argc > 1 ? argv[1] : NULL;
	const char *arg = argc > 2 ? argv[2] : NULL;
	const char *arg2 = argc > 3 ? argv[3] : NULL;

	if (!cmd || argc > 4)
		return usage();

	if (!strcmp(cmd, "suspend") || !strcmp(cmd, "poweroff") ||
	    !strcmp(cmd, "reboot"))
		return arg ? usage() : to_loginctl(cmd);

	/*
	 * `firewall` IS THE ONE VERB WITH TWO ARGUMENTS: `firewall list`,
	 * `firewall knocks` or `firewall <name> on|off`, joined into one
	 * request line.
	 */
	if (!strcmp(cmd, "firewall")) {
		char fw[KP_MAX];
		bool table = arg && (!strcmp(arg, "list") ||
				     !strcmp(arg, "knocks"));

		if (!arg || (table ? arg2 != NULL : !arg2))
			return usage();
		if (snprintf(fw, sizeof(fw), "firewall %s%s%s", arg,
			     arg2 ? " " : "", arg2 ? arg2 : "") >=
		    (int)sizeof(fw)) {
			fprintf(stderr, "kdos-power: argument too long\n");
			return 2;
		}
		return request(fw, true);
	}
	if (arg2)
		return usage();

	/*
	 * `accent` and `splash` are joined here rather than in the daemon's
	 * parser: the request line is `<verb> <arg>` and neither a scheme nor
	 * a motion carries a space, so one buffer and one strncmp on the far
	 * side is the whole protocol.
	 */
	if (!strcmp(cmd, "accent") || !strcmp(cmd, "splash")) {
		char line[KP_MAX];

		if (!arg)
			return usage();
		if (snprintf(line, sizeof(line), "%s %s", cmd, arg) >=
		    (int)sizeof(line)) {
			fprintf(stderr, "kdos-power: argument too long\n");
			return 2;
		}
		return request(line, false);
	}
	if (arg || strcmp(cmd, "ping"))
		return usage();
	return request("ping", false);
}

int main(int argc, char **argv)
{
	kb_set_progname("kdos-power");

	/* Dispatch on the basename, busybox-style: one binary, two names. The
	 * daemon and the client share the socket path and the command words, and
	 * two programs would be two places to keep them the same. */
	const char *me = strrchr(argv[0], '/');

	me = me ? me + 1 : argv[0];

	if (!strcmp(me, "kdos-powerd")) {
		if (argc == 3 && !strcmp(argv[1], "--explain"))
			return explain(argv[2]);
		/*
		 * THE WRITERS WITHOUT THE SOCKET, which is how their own rules
		 * get tested: what a firewall name, an accent or a motion may
		 * be, that the file is rewritten whole, that sshd follows
		 * `ssh`, that Samba's start obeys its disabled marker. The
		 * gate is SO_PEERCRED on a connection and cannot be
		 * exercised without two uids, so these would otherwise be
		 * asserted by nothing.
		 *
		 * THEY GRANT NOTHING. It is this binary run by whoever ran it,
		 * writing to an /etc that user could already write to; on the
		 * real path that is root's, and this changes neither who may
		 * connect nor what the daemon does for them.
		 */
		if (argc >= 3 && argc <= 4 && !strcmp(argv[1], "--firewall")) {
			static char msg[4096];
			char joined[128];
			int rc;

			snprintf(joined, sizeof(joined), "%s%s%s", argv[2],
				 argc == 4 ? " " : "", argc == 4 ? argv[3] : "");
			rc = fw_verb(joined, msg, sizeof(msg));
			fputs(msg, rc == 0 ? stdout : stderr);
			return rc == 0 ? 0 : 1;
		}
		if (argc == 2 && !strcmp(argv[1], "--file-sharing")) {
			char msg[128];
			int rc = kp_share_start(msg, sizeof(msg));

			fputs(msg, rc == 0 ? stdout : stderr);
			return rc == 0 ? 0 : 1;
		}
		if (argc == 3 && (!strcmp(argv[1], "--accent") ||
				  !strcmp(argv[1], "--splash"))) {
			char msg[128];
			int rc = argv[1][2] == 'a'
				? kp_accent_set(argv[2], msg, sizeof(msg))
				: kp_splash_set(argv[2], msg, sizeof(msg));

			fputs(msg, rc == 0 ? stdout : stderr);
			return rc == 0 ? 0 : 1;
		}
		if (argc > 1) {
			fprintf(stderr, "usage: kdos-powerd [--explain USER]\n"
					"       kdos-powerd --firewall list|knocks|"
					"<service> on|off\n"
					"       kdos-powerd --accent <scheme>\n"
					"       kdos-powerd --splash "
					"phosphor|modern\n"
					"       kdos-powerd --file-sharing\n");
			return 2;
		}
		return serve();
	}
	return client(argc, argv);
}
