# The daemons

KDOS has no logind and no polkit agent on the path between the desktop and the kernel.
In their place are five small root daemons, each answering a few fixed questions on a Unix socket
in `/run`: suspend the machine, mount a USB stick, say which application is spending the battery,
kill a runaway program before the desktop freezes, and mount application packs. This chapter
describes each of them, together with three programs that sit close to privilege without being
root daemons: the per-box Wayland socket `kdos-boxsock`, the portal backend and the lock screen.

It is written for two readers:

- **An administrator** who wants to know what each daemon allows, who may ask it, which file
  configures it and how to find out why something was refused. Read [At a glance](#at-a-glance)
  and [The shape they share](#the-shape-they-share), then the section for the daemon in question.
- **A contributor** changing a daemon or adding one. Read the whole chapter, then
  [Adding a root daemon](#adding-a-root-daemon).

Why the authorisation is shaped this way, and what it defends against, is argued in
[The security model](../03-architecture/security-model.md). How init starts services is in
[Boot and init](../03-architecture/boot-and-init.md). Every socket path and verb on the system is
listed in [Filesystem and IPC](../06-reference/filesystem-and-ipc.md).

## At a glance

Each root daemon is one binary in `/usr/sbin`, started at boot by a script in `/etc/init.d` and
kept running by `ksvc`, the service supervisor in `kdos-tools`.

| Daemon | Socket | Command-line client | Other clients | Init script | Skipped at boot when |
|---|---|---|---|---|---|
| `kdos-powerd` | `/run/kdos-powerd.sock` | `kdos-power` | The panel's and the Start menu's power items, the power and sleep keys, the compositor's lid handler `kdos-lid`, the `kdos-firewall`, `kdos-time` and `kdos-users` [surfaces](../06-reference/glossary.md) (windows and pop-ups drawn by `kdos-shell`), `kdos theme`, `kdos doctor` (checks that the socket exists) | `55_powerd.sh` | The binary is missing |
| `kdos-energyd` | `/run/kdos-energyd.sock` | `kdos-energy` | `kdos-res` (the Energy and Boxes pages), `kdos doctor` (checks that the socket exists) | `56_energyd.sh` | The binary is missing, or no `/sys/class/powercap/*/energy_uj` is readable |
| `kdos-oomd` | `/run/kdos-oomd.sock` | — | `kdos doctor` (checks that the socket exists) | `57_oomd.sh` | The binary is missing, or `/proc/pressure/memory` is not writable (the kernel has PSI off) |
| `kdos-mountd` | `/run/kdos-mountd.sock` | `kdos-mount` | The `kdos-devices`, `kdos-disks` and `kdos-connect` surfaces, and the session helper `kdos-mediad` | `58_mountd.sh` | The binary is missing |
| `kdos-packd` | `/run/kdos-packd.sock` | — | `kdos-appbox` (and `kdos app`, which runs it), `kdos doctor` | `59_packd.sh` | The binary is missing, or `erofs` is absent from `/proc/filesystems` even after `modprobe erofs` |

`kdos-powerd`, `kdos-energyd` and `kdos-mountd` are each one binary with two names. Run under the
daemon's name it serves the socket; run under the client's name (`kdos-power`, `kdos-energy`,
`kdos-mount`) it sends one request and prints the answer.

### Starting and stopping one by hand

The `service` command (a second name for `ksvc`) takes the verb first and then the **service
name**, which is the init script's name without its two-digit prefix and `.sh` suffix:

```sh
service status powerd
service restart mountd
service stop packd
service start energyd
service list                # every service, whether it starts at boot, and its state
service disable oomd        # do not start it at boot
```

`rcS` and `rcK` are the boot and shutdown scripts in `/etc/init.d` that run every numbered service
script in order (see [Boot and init](../03-architecture/boot-and-init.md)). `disable` creates
`/etc/service.disabled/<name>`, which both skip; `enable` removes it. A name that matches no
script exactly is matched as a substring, so `service status mount` also reaches `58_mountd.sh`.
The daemon's full name (`kdos-powerd`) matches nothing: it contains the service name rather than
the other way round.

The substring match finds the script for `status`, `start`, `stop` and `restart` only. `enable` and
`disable` check that some script matches, then write or remove the marker under the name exactly
as typed, while `rcS` and `rcK` look for the marker under the script's own service name. They
therefore need the exact service name: `service disable mount` reports success, creates
`/etc/service.disabled/mount`, and disables nothing.

### Who may ask for what

| Daemon | root | `wheel` (administrators) | `seat` (the person at the machine) | Anyone else |
|---|---|---|---|---|
| `kdos-powerd` | Every verb | Every verb | `ping`, `suspend`, `poweroff`, `reboot` | Refused |
| `kdos-energyd` | Every verb | Every verb | Refused | Refused |
| `kdos-oomd` | Every verb | Every verb | Every verb | Refused |
| `kdos-mountd` | Every verb | Every verb | Every verb | Refused |
| `kdos-packd` | Every verb | Every verb | Refused | Refused |

`seat` is the group seatd hands the display to. The installer keeps the desktop account in `seat`
on every install and takes a non-administrator out of `wheel`; see
[kinstall](kinstall.md#what-the-rest-of-the-tree-provides). Such an account keeps the lid, the
power keys and removable media, and loses sudo and every configuration verb.

## The shape they share

Every root daemon here is built the same way. Knowing the pattern once tells you how to run,
diagnose and review all five.

### Supervision and logging

A daemon runs in the foreground and never forks into the background. The init script calls
`supervise <name> <binary>` from `/etc/init.d/service_helper`; the supervisor leads its own process
group, writes `/run/<full name>.pid`, and restarts the daemon five seconds after it exits.
`service stop <name>` sends SIGTERM to the whole group, waits a second, then sends SIGKILL to
whatever is left.

The daemon's output goes to syslog and to a small file in `/run`. The supervisor points the
daemon's standard output and error, and its own *Starting* and *Exited* lines, at a forwarder
process. Each line becomes a daemon-facility syslog message tagged with the daemon's full name, and
is also appended to `/run/kdos-svc.<full name>.log`: for example `/run/kdos-svc.kdos-powerd.log`,
tagged `kdos-powerd`. Files and tags use the full name; only the `service` command takes the short
one. The file holds at most 64 KiB; when it would grow past that it is renamed to
`/run/kdos-svc.<full name>.log.old` and a fresh one starts. The file is what you read before
`syslogd` is up or while it restarts.

The daemon never keeps the descriptors it inherited. Under `rcS` those point at the boot step's
capture file on the `/run` tmpfs, so a daemon that crash-looped would fill memory until the next
reboot, and a daemon that logs only to standard error would never reach `/var/log/messages`. The
forwarder ignores the SIGTERM that `service stop` sends the group and exits only when the daemon's
end of the pipe closes, so what a daemon prints while shutting down is kept and it never writes
into a pipe with no reader.

The init script checks the machine first and skips with a reason. A daemon that cannot do its job
on this machine (no energy counter, no PSI, no EROFS) is skipped and prints
`[SKIP] <name>: <reason>` at boot, because a daemon that refuses to start under a respawn loop is
restarted every five seconds for as long as the machine is up. `service_helper` also offers
`supervise --final-exit <code>`, which stops the respawn on an exit status restarting cannot
change. None of these five daemons uses it: each one's usual refusal is caught by its init script,
and a refusal the script cannot see (`kdos-energyd` finding no usable energy domain after the
script's glob matched) is restarted every five seconds.

### The socket and the identity check

Each daemon owns one socket in `/run`, named after the daemon, and removes a stale one before
binding so that a socket file left by a crash cannot hold the name.

The socket is mode 0666, and anyone may connect; the real check is the caller's identity. The
daemon asks the kernel for the connecting process's user id (`SO_PEERCRED`, which the caller cannot
forge) and answers `err not permitted` to anyone it does not admit. The file mode is left open on
purpose: a mode that looked like the authorisation is a mode somebody eventually loosens.

There is one implementation of that check, `kb_uid_allowed()` in `libkbase`. It admits uid 0
outright; otherwise it looks the uid up in the password database and reads `/etc/group`, and admits
the account when the group's id is its primary group or its name is in the group's member list.
Membership is therefore read at the moment of the request, not from the caller's process
credentials, and a group change applies to the next request without a new login. No daemon keeps
its own copy of the rule: a copy per daemon is a rule that gets tightened on one socket and stays
loose on the others. A daemon that needs a different rule states the difference beside its own
call.

The client never names a path. Every verb takes an identifier from a list the daemon itself
published: a row number, a pack id, a service name.

### One request per connection

The client connects, writes one short line, reads the answer and the connection closes. There is
no session state. The one exception is `kdos-mountd`'s `subscribe`, which keeps its connection open
and is written to when something changes; even then the daemon remembers only a file descriptor.

Each daemon serves one request at a time. `kdos-energyd` and `kdos-oomd` give a connection two
seconds to send its request, because the same loop drives their sampler and their pressure
trigger, and a client that connected and said nothing would otherwise stop them. `kdos-mountd` sets
no such limit although its loop also forwards hotplug events, so a connection that never speaks
holds back every `changed` notification until it closes. `kdos-powerd` and `kdos-packd` set none
either, so a connection that never speaks delays every later request until it closes.

### Fixtures, skips and test sockets

`kdos-energyd`, `kdos-oomd`, `kdos-mountd` and `kdos-packd` each have a fixture mode: the daemon
can be pointed at a recorded copy of the system state and made to print what it would do without
doing it. This is how the selection logic, which decides what gets mounted, formatted or killed, is
tested without a machine to lose. `kdos-powerd` has none; its `--explain` and `--set-*` flags
run the same rules against a scratch `/etc` instead (see
[Diagnosis and testing](#diagnosis-and-testing)).

Each daemon reads another socket path from an environment variable (`KDOS_POWERD_SOCKET`,
`KDOS_ENERGYD_SOCKET`, `KDOS_OOMD_SOCKET`, `KDOS_MOUNTD_SOCKET`, `KDOS_PACKD_SOCKET`) so the test
suite can run it unprivileged. Moving the socket grants nothing: the check is the caller's
identity, never the path. `kdos-powerd`, `kdos-oomd`, `kdos-mountd` and `kdos-packd` refuse to
start on their real socket unless they are root, because an unprivileged daemon would answer `ok`
to work it cannot do. `kdos-energyd` has no such check; it refuses to start when it cannot read the
energy counter, which without root it cannot.

A socket address holds at most 107 bytes of path. `kdos-packd` refuses to start on a longer path,
because truncating would bind a socket at a path nobody asked for, and the next start would fail
with "address already in use" for a file that appears not to exist. `kdos-powerd`, `kdos-energyd`,
`kdos-oomd` and `kdos-mountd` cut an over-long path to fit and bind at the shortened one.

## kdos-powerd

Suspend, power-off and reboot for a desktop that does not run as root, plus the few `/etc` writes
that belong to an administrator: the time zone, the autologin account, the firewall's open services
and the accent colour of the boot menu and text console.

A setuid helper would put an argument-parsing root process within every user's reach for three
verbs. polkit is installed, but its answer is to ask for a password, which a closing lid cannot
do. There is no logind. The daemon is the smallest thing that can sit between the desktop and
`/sys/power/state`.

| Verb | Who | Does |
|---|---|---|
| `ping` | root, `seat`, `wheel` | Answers `ok`. Also tells a client whether it would be allowed to suspend |
| `suspend` | root, `seat`, `wheel` | Suspend to RAM |
| `poweroff` | root, `seat`, `wheel` | Power off |
| `reboot` | root, `seat`, `wheel` | Reboot |
| `timezone <Area/City>` | root, `wheel` | Point `/etc/localtime` and `TZ` at a zone, and set the Wi-Fi country from it |
| `autologin <user>\|off` | root, `wheel` | Choose which account tty1 logs in without asking, or none |
| `firewall list` | root, `wheel` | One row per named service: name, `on` or `off`, and a short description of what the service is for (the For column below, in the daemon's words; no port appears), tab-separated, then `ok` |
| `firewall <service> on\|off` | root, `wheel` | Open or close one named service |
| `accent <scheme>` | root, `wheel` | Recolour the boot menu, the text console and the boot splash |

The daemon checks the list the other way round: a `seat` member may use exactly the four words
`ping`, `suspend`, `poweroff` and `reboot`, and every other verb is an administrator's. A verb added
later is therefore an administrator's unless someone deliberately adds it to that list. Every
refusal is written to the daemon's log with the caller's uid, so a dead power key can be attributed.

### The client

```sh
kdos-power [--no-lock] suspend|poweroff|reboot|ping
kdos-power timezone <Area/City>
kdos-power autologin <user>|off
kdos-power firewall list|<service> on|off
kdos-power accent <scheme>
```

The request is one line of at most 62 bytes plus its newline (63 in all); the daemon reads nothing
past that.

| Exit status | Means |
|---|---|
| 0 | The daemon answered `ok`, or closed the connection without answering any verb other than `ping` (for `suspend`, `poweroff` and `reboot` that is the machine going away) |
| 1 | The daemon answered `err …`, or is not running (`kdos-power: no kdos-powerd — ` followed by the hint ``service kdos-powerd start``, which ksvc rejects; see below) |
| 2 | Bad usage, or an argument too long for the request line |

When the daemon is not running, start it with `service start powerd`. The command the client
suggests puts the daemon's full name where the verb goes, so ksvc answers
`Error: Unknown command 'kdos-powerd'` and starts nothing (see
[Starting and stopping one by hand](#starting-and-stopping-one-by-hand)).

For every verb except `firewall`, an error is printed to standard error as `kdos-power: <reply>`.
`firewall` prints the daemon's whole reply on standard output, the `err` line included, because
`kdos-firewall` reads it through a capture that discards standard error.

### Suspend locks the screen first

`kdos-power suspend` locks the session before the machine sleeps, so a resume never hands back an
unlocked desktop. The order is:

1. Send `ping`. This runs the same identity check as `suspend`, so a caller who is not allowed to
   suspend, or a machine with no `kdos-powerd`, is refused before the screen is locked. Locking
   and then not suspending would be a password prompt in exchange for nothing.
2. If no process named `kdos-lock` is running, start one in its own session and wait up to two
   seconds for it to print `locked`, which it does once the compositor confirms the session is
   locked. An existing lock client is left alone, because the compositor refuses a second one.
3. Send `suspend`. If the lock did not confirm in time, the client prints
   `kdos-power: no lock confirmation within 2s — suspending anyway` and suspends regardless: a
   broken lock screen must not turn the suspend key into a no-op.

`--no-lock`, or a non-empty `KDOS_NO_LOCK_ON_SUSPEND` in the environment for a caller that cannot
change its own arguments, skips steps 1 and 2.

The daemon answers `ok` before the machine goes down, so the client is not left waiting for a reply
from a suspended kernel.

### What happens around a suspend

The daemon is the only thing on the machine that knows a suspend is happening (there is no logind
to announce one), so it tells the two programs that must act around it:

1. `sync`.
2. NetworkManager's `Sleep(true)`, through `dbus-send --system --print-reply`, waited for with a
   five-second reply timeout so the devices are down before the kernel freezes them.
3. `tlp suspend`.
4. Write `mem` to `/sys/power/state` (suspend to RAM). Hibernation is not offered, because the
   initramfs sets up no resume device.
5. After waking, or at once if the kernel refused the write: `tlp resume`, then NetworkManager's
   `Sleep(false)`.

NetworkManager sleeps first and wakes last, so the radios TLP restores are back before it rescans.
Without step 2, NetworkManager wakes trusting a Wi-Fi association and DHCP lease that the time
asleep has ended. Without `tlp resume`, the radio states TLP saved, and the settings the firmware
resets across suspend, are not restored. Each hook is skipped when `/usr/bin/dbus-send` or
`/usr/sbin/tlp` is not installed, and none of them can stop the suspend.

### Power-off and reboot

`poweroff` and `reboot` ask init first: SIGUSR2 to process 1 for a power-off, SIGTERM for a reboot.
Init then runs its shutdown entries, and `/etc/init.d/rcK` stops every service in reverse order,
`kdos-powerd` among them, before anything is unmounted. Only if the daemon is still alive sixty
seconds later, which means init ignored the signal, does it call `reboot(2)` directly. The wait is
that long because `rcK` stops every supervised service numbered above `55_powerd.sh` first, at a
cost of about a second each, and a shorter wait would cut the power mid-shutdown.

### accent

`accent <scheme>` takes one of the eight scheme names compiled into `libkcolor`: `phosphor`,
`amber`, `ice`, `bone`, `norton`, `borland`, `perfect` or `paper`. The name is matched by
`kcol_find()`; anything else is refused with `err not an accent`. There is no path to aim and
nothing to traverse, which is why this verb is safe to reach from a session.

It writes only root-owned files:

- `/etc/kdos/accent`, which `rcS` reads to recolour the boot splash that is already running;
- the boot menu (`limine.conf` on the EFI system partition, the ESP) and the text-console palette
  (`/etc/vtrgb`), by running `kdos-bootctl theme <scheme>`, which owns both files.
  `kdos-bootctl` is found through the daemon's inherited `PATH`, as `iw` is for the
  [timezone](#timezone) verb; every other program the daemon runs is named by absolute path.

It recolours nothing on the desktop. That is `kdos theme`'s job, which runs as the user and touches
only the user's own files; `kdos theme` calls this verb for the parts that need root.

A machine with no writable ESP is not a failure. The live medium is read-only, and a machine may
have no `/boot/efi` at all. When `kdos-bootctl` fails or is absent, the verb answers
`ok <scheme> (boot menu unchanged)` and succeeds, because refusing would make `kdos theme` look
broken on the ISO, where everything else recolours correctly.

### timezone

The time zone is set here rather than by a separate daemon because it is the same question: the
files are root's, the person changing them is the one administering the machine, and `wheel` is
already the answer to who that is.

A zone name is checked twice:

1. **As characters.** Letters, digits, `+`, `-`, `_` and `/` only; no dot at all; no leading,
   trailing or doubled slash; at most 64 characters. A zone is `Area/City` or `Area/Sub/City`, so a
   slash must be allowed, which makes `../../etc/shadow` look legal; this rule is what stops it.
   Failure answers `err not a zone name`.
2. **As a file.** It must exist under `/usr/share/zoneinfo`. Failure answers `err no such zone`.

Then both halves are written, each to a temporary name and renamed into place:

- `/etc/localtime` becomes a symlink to the zone file. Programs that read the zoneinfo tree follow
  it.
- `/etc/profile.d/20-timezone.sh` exports `TZ=':/etc/localtime'`. musl reads `TZ`, and it takes
  precedence where it is set, which is on every KDOS login. The colon form points musl at the same
  file, so the two can never name different rules. Writing only the symlink would leave `date`
  reporting the old zone in every shell that had already read the profile.

**The Wi-Fi country follows the zone.** The kernel's wireless layer starts in the "world"
regulatory domain, which keeps every 5 GHz DFS channel (one shared with radar) closed and caps
transmit power until something names a country, and some drivers never take one from an access
point. The verb looks the zone up in `zone.tab`, which maps each zone to exactly one country code,
then:

- writes `options cfg80211 ieee80211_regdom=<CC>` to `/etc/modprobe.d/kdos-regdom.conf`, which
  takes effect from the next boot;
- runs `iw reg set <CC>` for the running boot, when an `iw` is found through the daemon's
  inherited `PATH`; `kdos-bootctl` for [accent](#accent) is the other program looked up that way.

A zone with no country, such as `UTC`, removes the file. Neither step failing fails the verb: a
machine with no radio still has a time zone. The installer writes the same file from the zone
chosen during installation.

### autologin

`autologin <user>` sets the `autologin` key in `/etc/kdos/login.conf`, which decides whether tty1
logs straight into the desktop. `autologin off` turns it off.

- The account must be one that can log in. `kb_users()` in `libkbase` decides that; a name it
  would not list is refused with `err no such account`. Pointing autologin at a service account
  with `nologin` would give a machine that boots to a login nobody can complete.
- `off` replaces the key line with the fixed text `#autologin = kdos`, whatever account was set,
  rather than emptying the value. The login program only hands `agetty --autologin` a non-empty
  name, so an empty value would read as a setting and behave as none.
- Only the key line changes. A line counts as the key when it starts with `autologin`, or with
  one `#` then `autologin`, and contains `=`. Prose that merely mentions the key is left alone.
- A file too large to rewrite safely is refused (`err login.conf is too large`) rather than
  truncated: half a configuration file is a machine whose login settings are whatever survived.

The file is written to `login.conf.new`, flushed, and renamed over the original.

### firewall

`firewall` names services, never ports, and the table of what each name means belongs to the
daemon. A client that could name a port could open any port; a client that can only name `ssh`
opens exactly what the daemon's table says `ssh` is. `kdos-firewall` asks for the list rather than
carrying its own copy.

| Service | Opens | For |
|---|---|---|
| `ssh` | TCP 22 | Incoming SSH (`70_sshd.sh`) |
| `http` | TCP 80 | A web server on this machine |
| `https` | TCP 443 | A TLS web server on this machine |
| `ipp` | TCP 631 | Sharing a printer with CUPS |
| `smb` | TCP 445 | Sharing files over SMB |
| `kiwix` | TCP 8080 | `kiwix-serve` |
| `mdns` | UDP 5353 | mDNS beyond the default rule |
| `mqtt` | TCP 1883, 8883 | An MQTT broker for LAN devices (`73_mosquitto`) |
| `xmpp` | TCP 5222 | XMPP clients (`74_prosody`) |
| `nfs` | TCP 2049 | Sharing files over NFSv4 (`72_nfsd`) |
| `babel` | UDP 6696 | Babel mesh routing (`31_babeld`) |
| `nut` | TCP 3493 | UPS status for other machines (`56_nut`) |
| `snmp` | UDP 161 | SNMP queries of this machine (`71_snmpd`) |
| `mumble` | TCP 64738, UDP 64738 | A Mumble voice server (`75_mumble-server`) |
| `caldav` | TCP 5232 | Shared calendars and contacts (`77_radicale`) |
| `mail` | TCP 25, 143, 465, 587, 993 | A LAN mail server (`78_maddy`) |
| `irc` | TCP 6667, 6697 | An IRC server (`79_ngircd`) |
| `dlna` | TCP 8200, UDP 1900 | A DLNA media server (`84_minidlna`) |
| `tryton` | TCP 8000 | Tryton clients of GNU Health (`85_gnuhealth`) |
| `caddy` | TCP 8443 | Caddy's shipped site (HTTPS) |
| `mosh` | UDP 60000–61000 | Incoming mosh sessions (needs `ssh` on as well) |
| `syncthing` | TCP 22000, UDP 22000, UDP 21027 | Syncthing sync and local discovery |
| `kdeconnect` | TCP 1714–1764, UDP 1714–1764 | KDE Connect with a phone on the LAN |
| `vnc` | TCP 5900 | This desktop over VNC (`wayvnc`, which listens on 127.0.0.1 until its config or `wayvnc 0.0.0.0` says otherwise) |
| `xonotic` | UDP 26000 | Hosting a Xonotic game for the LAN |

How a change is applied:

1. The daemon reads which names are on by looking for each rule's exact text in
   `/etc/nftables.d/50-kdos-services.nft`.
2. It rewrites that file **whole** from the names that are on, as additions to the `input` chain of
   the `inet filter` table. It never merges, because merging means parsing nftables syntax, and a
   parser that got it wrong would leave a port open that the surface showed as closed. Anything you
   write by hand belongs in another file under `/etc/nftables.d`, which the daemon never reads or
   touches. The new file is renamed into place before anything checks it.
3. It runs `nft --check -f /etc/nftables.conf`. If the ruleset would not load, it answers
   `err the ruleset would not load; nothing changed` and the kernel keeps the previous rules, but
   the rewritten file stays on disk: `firewall list` then reports the requested state, and the next
   boot loads that file. Turning the service back to its previous state rewrites the file as it
   was.
4. It runs `nft -f /etc/nftables.conf`. That file deletes and rebuilds only its own `inet filter`
   table, so the NAT that netavark (the container network backend) sets up for rootful containers
   and NetworkManager's for a hotspot survive a toggle. If the load itself fails, it answers
   `err the ruleset did not apply`.

A successful toggle answers `ok <service> on|off`. An unknown name answers
`err no service called that`; a state other than `on` or `off` answers `err a service is on or off`.

### Diagnosis and testing

```sh
kdos-powerd --explain <user>
kdos-powerd --set-timezone <Area/City>
kdos-powerd --set-autologin <user|off>
kdos-powerd --firewall list
kdos-powerd --firewall <service> <on|off>
```

`--explain` answers "would this user be allowed, and why". It is the first thing to run when the
power key or the panel's power item does nothing, because a refusal otherwise shows up only in the
daemon's log. It asks `libkbase` the same question the socket asks, needs no privilege, and prints
one line, for example:

```text
alice: uid 1001, primary gid 1001, in seat only — permitted to suspend, power off and reboot; refused the configuration verbs
```

It exits 0 when the user is allowed anything and 1 when refused.

The other flags run a verb's own rules without the socket, which is the only way to test them: the
socket's check needs two different users to exercise. They grant nothing, because the binary is
writing to an `/etc` the person running it could already write to. Three environment variables
point them at a scratch tree: `KDOS_POWERD_ETC` replaces `/etc` (and then skips `nft` and `iw`),
`KDOS_POWERD_ZONEDIR` replaces `/usr/share/zoneinfo`, and `KDOS_POWERD_SOCKET` moves the socket.

## kdos-energyd

Per-application energy attribution: which application is using the processor's power.

Measuring the processor's energy is routine: the kernel exposes the counter under
`/sys/class/powercap`. Attribution is the hard part on an ordinary Linux desktop: an application is
dozens of processes in scattered groups, and nothing owns enough of the system to name them. On
KDOS every boxed application (one running in its own [box](../06-reference/glossary.md), a rootless
container) runs under a known name, listed in `/usr/share/kdos/alien-apps`, so the attribution
starts from an identity rather than a guess.

| Verb | Answers |
|---|---|
| `ping` | `ok` |
| `report` | The text report |
| `report-json` | The same report as one JSON object |

```sh
kdos-energy [--json|ping]
```

`kdos-energy` prints the report (or JSON with `--json`) and exits 0. It exits 1 when the daemon is
not running or answered `err`, and 2 on bad usage. An `err` answer goes to standard error. The
Energy page in `kdos-res` shows the same report, and its Boxes page shows each box's share. When
the daemon is not running the client suggests `service kdos-energyd start`, which ksvc rejects
as an unknown command; the command is `service start energyd` (see
[Starting and stopping one by hand](#starting-and-stopping-one-by-hand)).

This is the report the test fixture in `testing/fixtures/energy` produces (thirty seconds of
recorded samples) when replayed with the fixture's own application table, as the test suite runs
it:

```sh
KDOS_ALIEN_APPS=testing/fixtures/energy/alien-apps kdos-energyd --fixture testing/fixtures/energy
```

Without that table the host's `/usr/share/kdos/alien-apps` names the processes instead, and the
rows differ.

```text
KDOS energy  —  30 s of samples, RAPL package-0

  firefox-esr (appbox kdos-apps)          75.5%  ███████████████       gpu 75.0%
  kdos-comp                               15.4%  ███                   gpu 25.0%
  gimp (appbox kdos-apps)                  0.4%                        gpu  0.0%
  short-lived and exited processes         8.7%

  shares are of ATTRIBUTABLE energy — 57% of the package total; the rest is the idle floor
  idle floor 15.00 W, the lowest average power seen in 3 samples (still falling — early shares read low)
  an uncore domain is present, so the integrated GPU's energy is already counted
  the gpu column is engine TIME from drm fdinfo, never energy
  relative only. RAPL cannot see the panel, the radio or the disk, so there are no watt-hours here.
```

Answers go to root and `wheel` only: on a multi-user machine this list is what everyone else is
running.

Figures are shares, never watt-hours. The counter (Intel RAPL, read from
`/sys/class/powercap`) measures the processor package. It cannot see the screen, which is the
largest single draw on a laptop, nor the radio, the storage or a discrete graphics card. "This
application was 41% of attributable CPU energy today" is a measurement; "this application used 12%
of your battery" would be a guess presented as a unit.

The daemon samples every ten seconds. The interval is fixed and not configurable, for the reason
given below, and it is short enough to stay far inside the counter's wrap period. A machine that
wakes from a long suspend resumes sampling from the wake rather than catching up on the missed
windows. Six rules each change the answer:

- Nested domains are dropped. The power interface lists a package beside that package's own
  sub-domains, so summing the list counts the cores twice (in the fixture, a 15 W idle floor
  becomes 26.25 W). A domain inside another's directory is a sub-domain and is skipped. The
  platform-wide `psys` domain goes the other way: it contains the packages, so where it exists it
  replaces them.
- The counter wraps, roughly every half hour at a laptop's power draw. A naive subtraction would
  produce one enormous negative reading with nothing in the output saying so. The daemon reads each
  domain's `max_energy_range_uj` and corrects a single wrap per window.
- The idle floor is subtracted before anything is attributed. A processor draws power with
  nothing running, and a share model that ignored this would report a machine sitting at a login
  prompt as 90% one process. The floor is the lowest average power seen, and it is printed with the
  answer; with fewer than 30 samples the report says the floor is still falling.
- The floor is applied when the report is made, not per sample window. The floor can only fall,
  so charging each window the floor as it stood then would throw away the first window, which is
  usually the busiest because something has been launched. Each application carries weighted sums
  and the report subtracts once, with the floor as it finally stands.
- The denominator is the whole system's energy, not the sum of surviving processes. A build that
  starts and exits inside one window is gone by the next sample, and dividing by the survivors would
  hand its energy to them. That difference gets its own line: *short-lived and exited processes*.
- The graphics column is engine time, never energy. Nothing on the machine says what GPU time
  cost in joules. On integrated graphics it is already inside the package figure (the report says
  so when an `uncore` domain exists); on a discrete card it is outside the counter entirely, and
  the report says that instead. A driver that publishes no per-client statistics gets no column
  rather than a column of zeroes.

**Why a daemon rather than a one-shot command.** The counter runs freely, so a one-shot command
could only report what happened while it was watching. The counter is also readable only by root
since Linux 5.10, because the PLATYPUS side-channel attack showed that fine-grained unprivileged
reads can recover cryptographic keys. What leaves this daemon is a per-application percentage over
minutes; the raw counter and the sampling interval are never republished, and no client can ask for
a shorter interval. There is no write path into the power interface at all.

The init script skips the daemon when no `/sys/class/powercap/*/energy_uj` is readable, which covers
most virtual machines and every non-x86 processor. The daemon itself also refuses to start there,
naming the powercap path in its log, because a daemon sampling an unreadable counter would report a
machine that uses no energy, which reads as "nothing is draining the battery". `kdos doctor` tells
the two cases apart: a machine with no energy domain at all, and a daemon that is not running.

`kdos-energyd --fixture <dir> [--json]` replays recorded snapshots (`<dir>/0`, `<dir>/1`, … each
holding a `proc` tree, a `powercap` tree and an optional `dt` in seconds) through the same sampler
and report, and prints the result. `KDOS_ENERGY_PROC` and `KDOS_ENERGY_POWERCAP` point the reader
at other `/proc` and powercap trees, and `KDOS_ALIEN_APPS` at another application-name table.

## kdos-oomd

Kills the process that is starving the machine of memory, before the desktop freezes.

The kernel has its own out-of-memory killer, but it fires when an *allocation fails*. On a machine
with swap that is often minutes after the desktop stopped responding, the whole session spent
thrashing while the kernel still had pages to hand out. The kernel's pressure interface (PSI, in
`/proc/pressure/memory`) reports instead that the machine is *stalling* on memory, which is what a
frozen desktop feels like. Boxed applications make this likely: a browser and a 3D slicer on one
modest machine.

### How it decides

- It waits on the kernel; it does not poll. It writes the trigger `full 150000 1000000` into the
  pressure file (150 ms of full stall within a one-second window, meaning every runnable thread was
  stuck on memory for 15% of the last second) and sleeps until the kernel signals it. A sampling
  loop would itself compete for processor time with the stall it is trying to notice.
- The desktop is never a victim. Protected are process 1, kernel threads, the daemon itself,
  anything whose `oom_score_adj` is -500 or lower (the opt-out other user-space killers honour),
  and these by process name: `kdos-comp`, `kdos-shell`, `kdos-desk`, `kdos-notifyd`, `Xwayland`,
  `dbus-daemon`, `seatd`, `ksvc`, `kdos-powerd`, `kdos-energyd`, `kdos-oomd`, `wireplumber`, and
  every process whose name starts with `pipewire`. Killing the compositor to save the desktop is
  not a trade, and killing the audio session manager leaves a machine whose sound stopped for no
  visible reason.
- A box over its declared memory budget goes first. A box profile's `memory` key is passed to
  the container, but a rootless container on a machine with no cgroup delegation accepts the limit
  and ignores it; this rule is what makes the key mean something. The daemon adds up the resident
  memory of each box's processes, and when a box is over its budget the largest process in that box
  is the victim. Budgets come from each user's own box profiles,
  `/home/<user>/.config/kdos/boxes/<box>.conf`; `KDOS_BOX_PROFILES`, when set, replaces that search
  with one directory (the test suite uses it).
- Otherwise boxed processes are preferred, but not absolutely. A boxed application is the likely
  culprit, is supervised, and relaunches in seconds, while a host process is more often session
  state. The largest boxed process is chosen when it holds at least half the resident memory of the
  largest eligible process overall; otherwise the largest process overall goes. An absolute
  preference would kill a small boxed helper while a host process many times its size kept the
  machine stalled.
- The victim is named by its box. Identity comes from the same container-supervisor walk in
  `libkproc` that `kdos-res` and `kdos-energyd` use.
- The memory comes back immediately. The daemon takes a handle on the process first (a pidfd,
  so the next step cannot land on a recycled process id), sends SIGKILL, then calls
  `process_mrelease` to free the victim's memory at once rather than whenever it is reaped. On a
  kernel without that call the release is skipped and the kill stands.
- At most one scan every ten seconds. The cool-down starts when the trigger fires, not when a
  kill succeeds, so a trigger that finds nothing killable does not rescan every entry in `/proc`
  once a second on a machine that is already stalling.

The daemon shields itself from the emergency it watches: at start-up it sets its own
`oom_score_adj` to -1000 and locks its memory with `mlockall`. Each kill is logged with the victim
and the ten-second pressure averages that caused it, in the form
`kdos-oomd: killed <name> [(appbox <box>)] (pid <pid>, <size> MB) — memory pressure avg10 full=<f> some=<s>`.

### The socket

Nothing in the protocol names a process, so there is nothing to aim. The socket takes no argument
and answers root, `seat` and `wheel`:

| Verb | Answers |
|---|---|
| `ping` | `ok` |
| `status` | `ok trigger 'full 150000 1000000', kills <n>` and, after the first kill, `, last: <name> [(appbox <box>)] (pid <pid>, <size> MB)` |

`kdos doctor` reports whether its socket is present.

### Testing it

`kdos-oomd --fixture <dir>` reads a recorded `/proc` tree, prints the process it would kill as
`would kill <name> … (pid <pid>, <size> kB)` with the recorded pressure, and signals nobody. It
exits 0 when there is a candidate and 1 (printing `no candidate`) when there is none.

`testing/oomd-fire.sh` exercises the real path in a booted virtual machine: it allocates and
touches memory until the trigger fires, then asks the socket what happened rather than watching
the allocating process die (the kernel's own killer would also have got it, later). A passing run
shows `status` going from `kills 0` to `kills 1, last: <name> (pid <pid>, <size> MB)` while the
script that started the allocation keeps running; the recorded run on a 4 GB guest answered
`kills 1, last: python3 (pid 1615, 3644 MB)`. That run has one large process to
choose; which process the daemon picks on a busy desktop is what `--fixture` tests.

## kdos-mountd

Removable media, encrypted volumes, SMART health and network shares for a desktop that is not root.
udisks2 serves the natively ported toolkit applications (see
[Administration](../02-user-guide/administration.md)); for the desktop's own surfaces this daemon is
the whole of what stands between them and `mount`. Both read `/proc/mounts`, so a device udisks2
mounted under `/run/media/<user>` is listed here as mounted, with its mountpoint.

The client never names a path or a mountpoint. It asks for a row out of a list the daemon
published, and the daemon decides the device, the mountpoint and the options. A design that takes
"a path and a mountpoint" ends with someone mounting a stick over `/etc` from a shell.

### Verbs

A **row** below is a number from the daemon's own `list`; a **share row** is a number from
`shares`. `<count>` is the byte length of a second frame, described under
[The request format](#the-request-format).

| Request | Does | Answer |
|---|---|---|
| `list` | The eligible devices | One row each: index, kernel name, label, filesystem, size in GiB (`28.7G`), mountpoint, tab-separated, with `-` for an empty label or mountpoint; then `ok` |
| `mount <row>` | Mount the device | `ok <mountpoint>`; a device already mounted answers with its mountpoint |
| `unmount <row>` | Unmount it and remove the mountpoint the daemon made | `ok <mountpoint>` |
| `eject <row>` | Power the medium down. An optical disc ejects its own drive; any other device ejects its **parent disk**, because a stop command sent to one partition means nothing to the hardware | `ok <kname>` |
| `unlock <row> <count>` | Open a LUKS volume. The passphrase is the second frame | `ok kdos-<kname>` |
| `close <row>` | Close the mapping `unlock` made | `ok kdos-<kname>` |
| `format <row> <fs> <count>` | Write a filesystem. **Off unless `format = yes`.** The second frame is the device's kernel name, typed by the person | `ok <kname>` |
| `smart <row>` | The drive's health | `ok <model>\t<serial>\t<health>` |
| `cifs <server> <share> <user> <domain> <count>` | Mount an SMB share. The password is the second frame | `ok <mountpoint>` |
| `krb5 <server> <share> <user\|-> <domain\|->` | Mount an SMB share with the Kerberos ticket `kinit` left in the caller's credential cache. **No second frame** | `ok <mountpoint>` |
| `shares` | The mounted network shares | One row each: index, `//server/share`, mountpoint, tab-separated; then `ok` |
| `browse` | Machines that answered an mDNS and a NetBIOS broadcast at the time of the request | One row each: `name<TAB>address`; then `ok` |
| `disconnect <share row>` | Unmount a network share | `ok //server/share` |
| `subscribe` | Hold the connection open and write `changed` on every block-device hotplug event | `ok`, then `changed` lines; never closes |
| `ping` | — | `ok` |

Every verb answers root, `seat` and `wheel`: mounting a stick is the desktop user's job whether or
not they administer the machine. Every failure answers `err <reason>`, for example
`err unmount it first` or `err no such device`.

### The command-line client

```sh
kdos-mount list
kdos-mount mount <index>
kdos-mount unmount <index>
kdos-mount smart <index>
kdos-mount write <index> <image> <disk>
kdos-mount shares
kdos-mount browse
kdos-mount krb5 <server> <share> <user|-> <domain|->
kdos-mount ping
kdos-mount subscribe
```

For example:

```sh
$ kdos-mount list
0	sdb1	KDOS	vfat	28.7G	-
1	sdb2	backup	ext4	120.0G	/media/kdos/backup
ok
$ kdos-mount mount 0
ok /media/kdos/KDOS
```

The client prints the daemon's reply unchanged, flushing after every read so that `subscribe` can
be piped into another program.

| Exit status | Means |
|---|---|
| 0 | The daemon answered, **including when the answer was `err …`**. A script must check standard output for a leading `err` to tell a failed mount |
| 2 | No `kdos-mountd` to ask (`kdos-mount: no kdos-mountd on <socket path> (<reason>)`), or bad usage |

The index is re-rendered as a number before it is sent, through `atoi`, so an argument that is not a
number becomes row 0: `kdos-mount mount sdb1` acts on row 0 rather than being refused.
`kdos-mount write <index> <image> <disk>` writes an image (see
[Writing an image](#writing-an-image)): it opens the image itself, as you, and prints the daemon's
progress lines as they arrive. The other verbs that carry a secret (`unlock`, `format`, `cifs`), and
`eject`, `close` and `disconnect`, are reached from the desktop's surfaces rather than from
`kdos-mount`:

| Surface | Uses |
|---|---|
| `kdos-devices` | `list`, `mount`, `unmount` |
| `kdos-disks` | `list`, `mount`, `unmount`, `unlock`, `close`, `format` (always as `ext4`), `smart`, `write` |
| `kdos-connect` | `cifs`, `krb5`, `shares`, `browse`, `disconnect` |
| `kdos-mediad` | `subscribe`, `list`, `mount`, `eject` |

### The request format

A request is one line, plus a second frame where a secret is involved.

- **Frame one** is a verb and up to five tokens, separated by spaces or tabs, ending in a newline.
  The line must be shorter than 1,024 bytes (`err too long` otherwise). That ceiling is set by
  `cifs` alone: a DNS name may be 253 bytes, a share 80, a username 104 and an NT domain 255, so a
  legal corporate share spells a request of about seven hundred.
- **Frame two** is exactly the number of bytes frame one's last token declared, from 1 to 512. A
  passphrase travels as a frame and not a token because the tokeniser splits on spaces and a
  passphrase may contain them. A connection that closes before the whole frame arrives is answered
  `err short frame`.
- **A descriptor** rides on frame one for `write` only: the image, attached as `SCM_RIGHTS`. The
  daemon reads frame one with `recvmsg` so the descriptor is not lost, keeps at most one, and closes
  one that arrives with any other verb before it looks at the verb.

Every token is checked before it means anything, and the token count is fixed per verb:

- A row is one to three digits and must be inside the list the daemon has published for this request.
- A count is one to four digits and between 1 and 512. A bad row or count on a two-frame verb
  answers `err bad request`.
- A trailing token nobody asked for makes the request unknown (`err unknown command`) rather than
  ignored. A dispatcher that read an index and discarded the rest would accept
  `mount 0 rm -rf /` as a well-formed mount.
- A line with eight or more tokens is refused as `err too many arguments`.

The device list is rescanned on every request rather than cached, so a stick pulled out between two
requests is never still offered.

### Which devices are offered

The daemon walks `/sys/block`, ignoring `loop`, `ram`, `zram` and `dm-` devices, and considers each
partition of a disk, and the whole disk when none of its partitions qualifies (a stick formatted
without a partition table is the usual case). A device appears in `list` only
if every one of these holds:

| Rule | Why |
|---|---|
| Its disk is removable, or is on USB | An internal disk is the administrator's. An external drive in a USB enclosure reports itself as non-removable, so the bus is checked too |
| It carries a filesystem the daemon recognises | See the list below |
| It is not named in `/etc/fstab`, by device path or by `LABEL=` | An entry there is a decision somebody already made |
| It is not the medium this system booted from | Offering to unmount the live medium is offering to end the session |

A device that is already mounted is listed, with its mountpoint in the last column. The boot
medium is a device mounted at `/`, `/mnt/iso` or `/boot`, and in a live session (root on an
overlay) any ISO 9660 medium. The live ISO does not appear in the running system's `/proc/mounts`
at all, because the initramfs moved the root out from under those mounts, so this broad rule is
the only reliable one; the cost is that a second data CD cannot be mounted while running live.

The label and filesystem type are read directly from the superblock rather than through a
detection library. The daemon recognises, in this order:

| On disk | Reported as |
|---|---|
| LUKS (checked first, because a LUKS header written over an old filesystem still carries that filesystem's superblock) | `crypto_LUKS` |
| ext2, ext3, ext4 | `ext4` |
| FAT12/16/32 | `vfat` |
| NTFS | `ntfs3` |
| exFAT | `exfat` |
| ISO 9660 | `iso9660` |
| btrfs | `btrfs` |

**Kernel support is checked at mount time.** Before calling `mount(2)`, the daemon checks the type
against `/proc/filesystems` and answers `err this kernel cannot mount <type>` when the driver is
missing, instead of failing halfway.

**Mount options.** `nosuid` and `nodev` always; `noexec` unless `exec = yes` is set in
`/etc/kdos/mountd.conf`. A setuid-root binary on somebody else's stick is a local root hole, so
executing from removable media is something you opt into. FAT, exFAT and NTFS have no ownership of
their own, so they are mounted with `uid=` and `gid=` set to the caller and `fmask=0117,dmask=0007`;
on a native filesystem the mountpoint itself is given to the caller.

**The mountpoint** is `/media/<user>/<label>`, created mode 0700 and owned by the caller, and
removed on unmount. The label is reduced to the characters `A–Z a–z 0–9 . _ -` before it becomes a
path component, because it is whatever somebody else's computer wrote into a superblock. A device
with no usable label is named after its kernel name, and failing that `disk`. A path too long to
be a mountpoint is refused rather than shortened.

### /etc/kdos/mountd.conf

The image ships no `mountd.conf`; create it to change a default. The daemon reads it on each
request that needs it.

| Line | Default when absent | Effect |
|---|---|---|
| `exec = yes` | `noexec` | Removable media and network shares are mounted without `noexec`, so programs on them can run |
| `format = yes` | `format` refused | The `format` verb is allowed |
| `write = yes` | `write` refused | The `write` verb is allowed |

The daemon searches the whole file for the text `exec = yes` or `exec=yes` (and likewise for
`format` and `write`) anywhere, comments included. A commented-out `# exec = yes` therefore still
turns the setting on. To turn a setting off, delete the text rather than commenting it out.

### Encrypted volumes

The passphrase reaches `cryptsetup open` on standard input, through `--key-file=-`, and never in an
argument list: `/proc/<pid>/cmdline` is readable by every user for the life of the process. One
buffer holds it, and every way out of the request wipes it. A wrong passphrase and a header the
installed `cryptsetup` will not open give the same answer, `err could not unlock`, so a failure
says nothing about how close a guess was.

The mapping name is the daemon's: `kdos-<kname>`, derived from the row. A client cannot ask for a
mapping called anything else, and `close` finds the same name from the same row without being told
it. Unlocking a volume that is already open is not an error.

After an `unlock`, the opened mapping appears in `list` beside the container it came from, so the
filesystem inside can be mounted like any other row. The daemon finds it by the name it chose
itself rather than by walking `/sys/block/dm-*`; a mapping this daemon did not open, such as
someone's own `cryptsetup open` of a root volume, is not offered. The row keeps the container's
physical disk, so every destructive verb is still judged by that disk.

### SMART

`smart` answers for the whole drive, not the partition: SMART is a property of the drive, so a row
per partition would repeat one answer and point a raw-device tool at an offset nothing owns.

`smartctl` needs the raw block device, which no session may open. The alternatives, a setuid binary
or a sudo rule, are both wider holes than one daemon answering one question. The daemon runs
`smartctl -H -i` on the parent disk and filters the output down to three tab-separated fields: the
model (`Device Model` or `Model Number`), the serial and the health line (`overall-health` or
`SMART Health Status`), with `-` for a field it did not find. Its exit status is a bitfield rather
than a failure flag: bits 3 to 7 mean the drive is unwell, which is often the answer rather than
the absence of one, so only an empty capture is treated as "nothing learnt".

### Network shares

`cifs` mounts an SMB share with a password. `mount(2)` cannot open an SMB session by itself
(dialect negotiation, authentication and the tree connect all happen inside `mount.cifs`), so this
verb runs that helper.

- Each of the four names is checked against its own character allowlist. Server: letters,
  digits, `.` and `-`, up to 253 characters, not starting or ending with a separator and with no
  `..`. Share: letters, digits, `.`, `_`, `-` and `$`, up to 80. User: letters, digits, `.`, `_`,
  `@` and `-`, up to 104. Domain: letters, digits, `.` and `-`, up to 255, or `-` for none.
  `mount.cifs` builds its option string by joining fields and escapes nothing but the password, so
  a comma in any of these would become a new mount option, and a `/` or `\` in a server would
  silently re-aim the mount. What is not on the list is refused rather than quoted.
- The password reaches the helper on a file descriptor (`PASSWD_FD=0`, with the bytes on the
  child's standard input). The other ways `mount.cifs` accepts one are worse: an option string is
  visible in the process list, an environment value in `/proc/<pid>/environ`, and a file is a file
  somebody has to delete.
- Names musl cannot resolve are resolved here. `nsswitch.conf` has no effect on musl and there
  is no winbind, so a `.local` name and a bare NetBIOS name are the two shapes `getaddrinfo` never
  answers. The normal resolver is tried first (a name in `/etc/hosts` is one somebody wrote down,
  and a broadcast must not override it); then a `.local` name goes to `avahi-resolve-host-name` and
  a bare name to `nmblookup`. An address or a dotted DNS name never triggers a broadcast. Only the
  address is substituted: the share mounts under the name that was typed, with the address passed
  in `ip=`, so the mountpoint is one a person recognises. A name nothing answers to is refused
  before anything is loaded or created.
- The `cifs` module is loaded first. Nothing else on the image loads it, and
  `/proc/filesystems` lists only what the kernel already has, so checking support before `modprobe`
  would refuse every first connection.
- The mount belongs to the caller. It lands at `/media/<user>/<server>-<share>` with
  `uid=`, `gid=`, `file_mode=0600` and `dir_mode=0700`, because a server without Unix extensions
  reports every file as owned by root. `nosuid` and `nodev` always; `noexec` unless `exec = yes`. A
  share that is already mounted answers with its existing mountpoint. When the helper refuses, the
  first line of its own error message is the answer.

`shares` lists what `/proc/mounts` says is connected (type `cifs` or `smb3`). No list is held
between requests, so a server that went away, or a share another session mounted, is reported as
it is at that moment. `disconnect` rebuilds the same list before acting on a row.

`browse` is two broadcasts, not a directory. There is no browse master to ask (samba is built
without winbind and without a domain controller), so the list is `avahi-browse -ptrk _smb._tcp` for
machines that advertise the service, plus `nmblookup -S -- '*'` for machines that answer a NetBIOS
query. Only a machine with a `<20>` (file server) entry is offered, since one without it is sharing
nothing. At most 32 machines are listed. A browse row is not an index: `cifs` and `krb5` take a
server name.

`krb5` mounts with `sec=krb5`, using the ticket already in the caller's credential cache. No
secret crosses the socket: the kernel's cifs module raises a `cifs.spnego` key request,
`request-key` runs `cifs.upcall` against that cache, and the helper hands back the SPNEGO blob.
That is why it is a verb of its own rather than `cifs` with an empty password.

- The mount passes `cruid=<caller>`. Without it the upcall would look in root's credential cache,
  which is empty, and the mount would fail with `Required key not available`, naming no user.
- `-` means "no username" and "no domain", and for Kerberos that is the normal case: the principal
  in the ticket already says who you are.
- `/usr/sbin/cifs.upcall` and `/etc/request-key.d/cifs.spnego.conf` are checked before the module
  is loaded and before the mountpoint is made. Without them the kernel's error is that same
  unhelpful `Required key not available`, so the daemon names the missing file instead and leaves
  no empty directory under `/media`.

### What a destructive verb refuses

`eject`, `unlock`, `format` and `write` pass through one check before they act. It refuses a device
that is mounted (`err unmount it first`), a device on the boot medium, and a device node that has changed
since the scan.

The boot medium is refused by the disk, not by the partition. A live USB carries an ISO 9660
partition and a FAT EFI partition beside it. Every per-partition rule would offer the EFI partition
(it is removable, it is FAT, it is unmounted and no fstab names it), and formatting it would destroy
the running session. In a live session any disk carrying an ISO 9660 partition is treated as the
boot disk, whole: `err that is the medium this session booted from`.

The device node is checked again at the moment of use. Between the scan that built the row and
the call that acts on it, a path could become a symlink or a different device. The daemon opens it
with `O_NOFOLLOW` and requires a block device whose device number matches the one `/sys` recorded.

`format` requires the person to type the device's kernel name; a flag or the word yes is not
enough. The daemon compares the second frame with the name it put in the list itself, by exact
length and content, so a client cannot send a confirmation it was never shown. A mismatch
answers `err type <kname> to confirm`.

**`format` is opt-in**: without `format = yes` in `/etc/kdos/mountd.conf` it answers
``err format is off; set `format = yes` in /etc/kdos/mountd.conf``. The filesystem is one of four,
checked against a table (anything else answers `err unknown filesystem`), and every new filesystem
is labelled `KDOS`:

| Filesystem | Command |
|---|---|
| `ext4` | `/usr/sbin/mkfs.ext4 -F -L KDOS <node>` |
| `btrfs` | `/usr/bin/mkfs.btrfs -f -L KDOS <node>` |
| `vfat` | `/usr/sbin/mkfs.vfat -I -n KDOS <node>` |
| `exfat` | `/usr/sbin/mkfs.exfat -n KDOS <node>` |

### Writing an image

`write <row> <count>` puts a disk image, such as an installer or a live system, over the whole disk
the row is on, and then reads the disk back and compares it with the image. The image arrives as an
open descriptor attached to the request, never as a path. The client opened it as the person asking,
so it can only be a file that person can read; a root daemon that took a path would open it as root,
and `write 0 /etc/shadow` would copy a file the caller cannot read onto a stick the caller can.

**`write` is opt-in**: without `write = yes` in `/etc/kdos/mountd.conf` it answers
``err write is off; set `write = yes` in /etc/kdos/mountd.conf``. It is a key of its own because
turning on formatting says nothing about replacing a whole disk. Beyond the check every destructive
verb passes, it refuses:

| Refusal | Answer |
|---|---|
| No descriptor attached | `err no image: the file travels as a descriptor, not a name` |
| Any row on the same disk mounted, or an unlocked mapping on it | `err unmount <kname> first`, `err close <kname> first` |
| Any partition of the disk, or the disk itself, that `/etc/fstab` names or the running system is mounted from — partitions `list` leaves out, which the write would replace all the same | `err <name> is in /etc/fstab or holds the running system` |
| A second frame that is not the disk's name (`sdb` for a row on `sdb1`) | `err type <disk> to confirm` |
| A descriptor that is not a regular file with something in it | `err the image is not a file with something in it` |
| An image larger than the disk | `err the image is <n>G and <disk> is <m>G` |
| The disk opened by anyone else | `err <disk> is in use — unmount and close everything on it` |

The disk is opened with `O_EXCL`, which on a block device is the kernel's exclusive claim: it fails
while any partition of the disk is mounted, mapped or claimed, and while the write holds it a mount,
a format or a second write of that disk fails in turn. The node is opened with `O_NOFOLLOW` and its
device number is checked against `/sys`, like every other verb's.

The copy runs in a double-forked worker, so the daemon goes on answering every other client for the
minutes a write takes. The connection stays open and carries the worker's progress, one line per
step of about one per cent:

```text
progress write 1048576 3000000
progress verify 3000000 3000000
ok sdb 3000000 verified
```

After the copy the data is synced and the device's cached pages are dropped (`BLKFLSBUF`), so the
verify reads what the disk hands back rather than what the worker just wrote; a stick that wraps
writes past its real capacity fails there with `err verify failed: <disk> does not hold the image
at byte <n>`. Last, the daemon asks the kernel to re-read the partition table (`BLKRRPART`), which
makes the new partitions appear and sends every subscriber `changed`. A worker whose client has gone
carries on to the end; its progress lines are dropped.

A disk with no filesystem the daemon recognises is not in `list` at all, so an image cannot be
written onto it through this verb; and in a live session any disk carrying an ISO 9660 partition is
treated as the boot disk, so a stick that already holds another system's image is refused there.

Every program it runs is named by absolute path: `/usr/bin/eject`, `/usr/sbin/cryptsetup`,
`/usr/sbin/smartctl`, the `mkfs` programs above, `/sbin/modprobe`, `/sbin/mount.cifs`,
`/usr/bin/avahi-resolve-host-name`, `/usr/bin/avahi-browse` and `/usr/bin/nmblookup`. Otherwise a
root process would look programs up through an inherited `PATH`. `eject`, `cryptsetup`, `smartctl`,
`nmblookup` and `avahi-resolve-host-name` get `--` before the device or name, so a leading dash
cannot become an option. The `mkfs` node and the `mount.cifs` share are paths the daemon built
itself (`/dev/<kname>` and `//<checked server>/<checked share>`), so neither can begin with a dash.

### How the desktop hears about a new device

`kdos-mediad` is the subscriber. It is a name of the `kdos-shell` binary, started once per session
by the compositor alongside `kdos-notifyd`:

1. `kdos-mediad` sends `subscribe` and keeps the connection open.
2. The daemon listens to the kernel's own hotplug broadcast (`NETLINK_KOBJECT_UEVENT`), so hotplug
   works with no udev rule file and without udev running. It reacts to block-device `add`,
   `remove` and `change` events; `change` is included because that is what a drive reports when a
   disc goes into a tray that was already there.
3. On every block-device `add`, `remove` or `change` event the daemon writes `changed` to every
   subscriber, without checking whether the offered list is different; an internal disk or a
   mapping changing sends it too. It cannot say *which* device, because a row number is only true
   of the list it came with, so the subscriber asks again with `list` and compares.
4. `kdos-mediad` raises a notification with **Open** and **Eject** buttons. When a button is
   clicked it reads the list again and finds the device by kernel name, rather than trusting the
   row the notification was built from.

The daemon only says that something moved; the session decides what it means. The daemon is root,
starts before anybody logs in, and has no session bus to send a notification on. At most four
subscribers are held at once; a fifth is answered `err too many subscribers`. A subscriber that
hangs up is dropped before anything is written to it.

The panel does not subscribe. A subscription would put a socket read in the panel's frame loop,
which keeps blocking calls out of the way of drawing (the tray follows the same rule; see
[kdos-shell](kdos-shell.md#the-tray)). `kdos-devices` asks when it is opened.

If the daemon cannot open the hotplug socket it logs
`kdos-mountd: no uevent source; subscribe will report nothing` and serves everything else.

### Fixtures

```sh
kdos-mountd --fixture <sys> [dev]
kdos-mountd --fixture-serve <sys> [dev]
```

`--fixture` prints what the daemon would offer from a recorded `/sys` tree (and optionally a `/dev`
tree), one row per device with its size in bytes and a final `<n> eligible`, and mounts nothing.
`--fixture-serve` runs the real request handling over a real socket (`KDOS_MOUNTD_SOCKET`) with the
fixture's trees, admits any caller, and prints each command it would run (with its environment and
the byte count, never the bytes, of anything it would feed on standard input) instead of running
it. No name is resolved and nothing is broadcast. That is how a `format` aimed at the boot medium
is proved to be refused without a disk to lose. A `write` in these modes prints `write <node>` and
copies the image into the file `KDOS_MOUNTD_SINK` names instead of the disk, and verifies against
that file, so the copy and the read-back really run with nothing that looks like a device touched.

Only in these two modes does the daemon read the path overrides `KDOS_MOUNTD_SYS`,
`KDOS_MOUNTD_DEV`, `KDOS_MOUNTD_MOUNTS`, `KDOS_MOUNTD_FSTAB`, `KDOS_MOUNTD_MEDIA`,
`KDOS_MOUNTD_CONF`, `KDOS_MOUNTD_SINK` and `KDOS_MOUNTD_UEVENT` (a FIFO standing in for the
kernel's hotplug socket).
The daemon started by the init script reads none of them: an environment variable that moved its
idea of `/dev` would be a way to aim a format at any device on the machine.

The committed fixture, `testing/fixtures/mountd`, is a recorded `/sys/block` tree with six hand-built
device images. Five are on removable disks and are offered: a FAT32 stick labelled `KDOSSTICK`, a
LUKS container, a live-USB pair (an ISO 9660 partition and a FAT ESP beside it) and an optical
disc. One is an ext4 filesystem on an internal disk and is never offered. The fixture also carries
an `fstab` that claims the stick by label, and a `mounts-live` table from a booted live medium with
one mounted network share. The internal disk carries a real superblock so that a broken removable
check shows up as an extra row rather than as nothing.

## kdos-packd

The only program on the system that mounts an application pack. A pack is a signed, read-only EROFS
image holding an application, a runtime it depends on, or data; how packs are built, verified and
combined into a box is in [Packs and boxes](../03-architecture/packs-and-boxes.md).

| Verb | Does |
|---|---|
| `list` | Every pack the machine can see: id, version, kind, state (`mounted`, `installed` or `available`), size in bytes, and origin (`store` or `medium`), tab-separated, then `ok` |
| `info <id>` | One pack's metadata, then its `state`, `origin` and, when mounted, `mountpoint` |
| `mount <id>`, `unmount <id>` | Mount or release a pack. A pack still used by a box is not unmounted |
| `compose <box> <id>…` | Build a box's overlay stack from 1 to 32 packs. Ids past the 32nd are ignored rather than refused |
| `decompose <box>` | Tear it down |
| `install <file>` | Verify a pack in the staging directory and move it into the store |
| `remove <id>` | Take a pack out of the store |
| `rollback <id>` | Return to a retained earlier version |
| `graft <id>`, `ungraft <id>` | Place a data pack's contents where its consumer looks for them, or remove them (a [graft](../06-reference/glossary.md)) |
| `ping` | `ok` |
| `status` | Tab-separated lines: `store`, `medium` and `staging` directories, `retain` count, mount `route` (`file-backed`, `loop device`, or `not yet used`), `packs` (installed, available and mounted counts), `boxes` (how many are composed), and a `box` line for each composed box (name, merged root, `ephemeral` or `persistent`), then `ok` |

Every verb answers root and `wheel` only. The clients on the system are `kdos-appbox` and
`kdos doctor`. Launching a boxed application sends `info` for the pack's environment and `compose`
for its box; `kdos-box start` sends `compose` and `kdos-box remove` sends `decompose`.
`kdos-appbox genlaunchers --packs` sends `list` and then `mount` for each installed pack.
`kdos app import`, `kdos-appbox import` and `kdos-box import` copy a pack into the staging
directory and send `install`. `kdos-appbox status` and `kdos doctor` ask for `status`, and
`kdos doctor` reports the mount route. `kdos app install` and `kdos app remove` build and delete
container images and ask this daemon nothing. No program on the system sends `unmount`, `remove`,
`rollback`, `graft` or `ungraft`; those verbs are reached only by a client that speaks the socket
directly.

An id is at most 63 characters from `[A-Za-z0-9._-]` and does not begin with `.` or `-`; anything
else is refused before it is looked up.

Paths:

| Path | What |
|---|---|
| `/var/lib/kdos/packs` | The store; the current version of a pack is `<id>.kpack` |
| `/var/lib/kdos/packs/staging` | Where an unprivileged download lands; mode 01777, set when the daemon starts |
| `/var/lib/kdos/packs/mnt` | Mountpoints |
| `/mnt/iso/packs` | Packs on the boot medium |
| `/var/lib/kdos/pack-manifest` | Every graft made, so an ungraft removes exactly what was added |
| `/etc/kdos/keys/packs` | The keys packs are verified against, separate from `/etc/kdos/keys`, which is `kpkg`'s package-repository ring |
| `/etc/kdos/packd.conf` | Configuration |

Unlike `kdos-mountd`'s overrides, the daemon reads these environment variables in normal operation,
not only in fixture mode. The init script sets none of them:

| Variable | Effect |
|---|---|
| `KDOS_PACK_STORE`, `KDOS_PACK_MEDIUM`, `KDOS_PACK_MANIFEST` | Replace the store, the boot-medium pack directory and the graft record |
| `KDOS_PACK_HOME`, `KDOS_PACK_SHARE` | Replace the caller's home directory and `/usr/share` as graft and box-upper destinations |
| `KDOS_PACK_RETAIN` | Overrides `retain` in `packd.conf` (below) |
| `KDOS_REQUIRE_SIG` | Refuse every unsigned pack at install (`<id> is unsigned and KDOS_REQUIRE_SIG is set`), for a machine that installs only what it can attribute |

All of them are also listed in
[Filesystem and IPC](../06-reference/filesystem-and-ipc.md#environment-variables).

### Installing and verifying

**The client never names a path, with one exception:** `install` takes a file name inside the
staging directory. A name containing `/`, the name `..`, an empty name, or a name with a character
outside `[A-Za-z0-9._-]` is refused. `status` publishes the staging directory and the retention
count, so a program writing a download into the store does not have to work either out for itself;
a second definition of where an unprivileged write is allowed is the kind of thing that drifts.

Verification happens in the daemon, where the mount happens, because a client that verified and
then asked a daemon to mount would have verified nothing:

1. If the staging directory holds a `PACKAGES` index whose signature is good and which lists this
   pack, the pack's SHA-256 must match the index entry; a mismatch is refused
   (`<id> does not match the signed index`).
2. Otherwise the pack's own signature block is checked against `/etc/kdos/keys/packs`. A good
   signature is accepted; a bad one, a hash mismatch, or a signature by a key the ring does not
   hold is refused.
3. A pack with no signature at all is accepted and logged as unsigned, unless `KDOS_REQUIRE_SIG`
   is set.

A pack in the store was verified when it was written and only root can write the store, so it is
not re-hashed at each mount. A pack on the boot medium was never installed, so it is verified at its
first mount; there a bad signature, a hash mismatch or a missing key is refused, and an unsigned
pack mounts. Every pack is mounted read-only with `nosuid` and `nodev`, and a data pack with
`noexec` as well.

The list is rescanned on every request, so a medium pulled out between two requests is not still
offered. At start-up the daemon adopts whatever packs and boxes are already mounted, so a restart
while boxes are running does not lose track of them.

**An install replaces the old version safely.** If the old version is mounted but not in use, it is
unmounted before the file is swapped, so the next compose reads the new file. If it is composed
into a running box, the install is refused (`<id> is composed into <n> box(es) — stop them, then
install`), the same rule `remove` and `rollback` apply. Without this, a box composed after an update
would put the new application's layer over the old runtime's still-mounted files, and the
application would fail on a library only the new runtime carries. The version being replaced is
kept as `<id>-<version>.kpack` beside the new `<id>.kpack`.

### Retention and rollback

`/etc/kdos/packd.conf` sets how many superseded versions of each pack the store keeps:

```ini
retain = 1
```

| Value | Effect |
|---|---|
| `1` (shipped) | The most recent update can be undone |
| `0` | Nothing is kept; `rollback` answers `err no earlier <id> is kept` |
| Up to `8` | That many, newest first by version; higher values are treated as 8 |

The daemon reads `retain` from the file once, when it first needs it, and keeps that value for the
rest of its life; restart it (`service restart packd`) after changing the file.
`KDOS_PACK_RETAIN`, when set, is read on every use.

The sweep that deletes older versions runs after an install and at no other time, so nothing deletes
a rollback while somebody is deciding whether to use it. `rollback` keeps the current file as
`<id>-<version>.kpack`, renames the retained version whose version string sorts last (compared as
text) into its place, and answers `ok <id> <current> -> <restored>`. The version rolled away
from stays in the store under its own version.

### Diagnosis and testing

`kdos-packd --fixture <store> [medium]` lists the packs in a scratch store with their signature
state, composes each application pack, grafts each data pack (printing each graft's destination),
and prints the result, mounting nothing. `KDOS_PACKD_VERBOSE` shows its log lines; `KDOS_KEYS`
points it at another key directory. The daemon reads `KDOS_KEYS` only in fixture mode; the
`kdos-pack` tool reads it in normal use, to choose the ring its `verify` and `info` check against
(default `/etc/kdos/keys/packs`).

The two mount routes, reference counting and adoption of existing mounts at start-up are described
in [Packs and boxes](../03-architecture/packs-and-boxes.md).

## kdos-boxsock

`kdos-boxsock` is not a root daemon and has no socket in `/run`. It is installed in `/usr/bin`, runs
as the desktop user, one process per box, and gives that box its own Wayland socket so the
compositor always knows which box a window came from.

```sh
kdos-boxsock <box> [instance-id]
```

What it does:

1. Checks the box name: 1 to 64 characters from `[A-Za-z0-9._-]`, and not `.` or `..`.
2. Takes a lock on `$XDG_RUNTIME_DIR/kdos-box-<box>@<display>.lock`. If another `kdos-boxsock` for
   the same box and compositor already holds it, this one exits 0 and does nothing. The lock, not
   the socket file, answers "is this box already served", because a socket file is also left
   behind by a crash.
3. Binds `$XDG_RUNTIME_DIR/kdos-box-<box>@<display>.sock`, mode 0700, so only this user can
   connect.
4. Hands it to the compositor through the Wayland security-context protocol, tagged with the engine
   name `io.kdos.appbox`, the box name as the application id, and the instance id (the box name
   when no instance is given).
5. Prints the socket path on standard output and stays alive holding the descriptor that keeps the
   tag valid. When the compositor exits, it removes the socket and exits too.

Every client that connects on that socket is tagged by the compositor itself; the client never
sees the tag, so it cannot forge, choose or drop it. That tag is what the compositor's sandbox
filter reads, what the box chip on a title bar shows, and what lets the panel say which box a
window came from.

`<display>` is taken from `$WAYLAND_DISPLAY`: its last path component, reduced to
`[A-Za-z0-9._-]` and cut to twelve characters, or `session` when nothing is left. It is in the path
because the listener belongs to the compositor this process connected to; a path keyed on the box
alone would send a later launch through a compositor that may already be gone. `kdos-appbox`
derives the same path from the same variables, so the two agree with nothing passed between them.

If the compositor does not offer the security-context protocol, `kdos-boxsock` exits 1 with
`compositor has no security-context-v1 — not tagging` rather than bind an untagged socket, and
`kdos-appbox` runs the box on the session's shared socket. The box then starts, unconfined at the
protocol level, instead of failing to start.

It is a separate program for two reasons:

- The launcher replaces itself with the container command, so it cannot hold anything for the box's
  lifetime, and the sandbox lasts exactly as long as the descriptor stays open. Something has to
  outlive the launch.
- `kdos-appbox` links only `libkbase`, `libktui`, `libkcolor` and `libkxdg`, none of which speaks
  Wayland. Speaking Wayland would add a client library and generated protocol code to a program
  whose short dependency list is a deliberate property.

## xdg-desktop-portal-kdos

The portal backend, installed as `/usr/lib/xdg-desktop-portal-kdos` and started on demand over the
session bus. It implements the FileChooser, Settings, AppChooser and Access backend interfaces; the
access question that the Camera, Screenshot and Location portals ask is answered through
`kdos-prompt`. It is covered in
[The session](../03-architecture/session.md#the-kdos-backend), including the two rules that matter
most: every request is answered, and the bus loop never blocks on a dialog.

## kdos-lock

The lock screen, in `/usr/bin`. It is not a root daemon, but it is the other long-lived program
close to privilege. It is an `ext-session-lock-v1` client: it covers every output with a lock
surface and checks the password by running `kdos-checkpass`, handing it the password on standard
input.

- No password is accepted before the compositor confirms the lock. Until then the session may
  still be on screen, and the field refuses keys.
- The password buffer is wiped after every attempt and its length is capped.
- There is no user switching, no power menu and no notification area on the lock screen, since
  each would be a way to reach something else from a locked session.

Once the compositor confirms the session is locked, `kdos-lock` prints `locked` on standard output;
`kdos-power suspend` waits for that line.

The compositor, not the lock program, owns the locked state. If the lock program crashes, the
screens stay covered and a new lock client may take over. See
[kdos-comp](kdos-comp.md#idle-dim-lock-and-lid).

### kdos-checkpass

`kdos-checkpass` (`/usr/bin`, mode 4755) is the only setuid piece of the lock screen: `/etc/shadow`
is readable only by root, and the lock screen must not run as root.

- It takes no arguments and checks the password of the user who ran it, identified by the real user
  id, never by a name it was given. An argument would make it an oracle for any account's password.
- It reads the password on standard input, because argument lists are visible to every user. A
  single trailing newline is stripped.
- It parses `/etc/shadow` directly rather than through the name-service layer, so no NSS module runs
  in a setuid process.
- It drops privilege as soon as the hash is read, so `crypt()` runs as the caller, and compares the
  result in constant time.

| `kdos-checkpass` exit | Means |
|---|---|
| 0 | Correct |
| 1 | Wrong |
| 2 | Could not tell: no such user, no readable shadow entry, an account with no password or a locked one (an empty, `!` or `*` hash), or an argument was given |

Each wrong answer takes one second before `kdos-checkpass` exits, which limits how fast a password
can be guessed by driving it directly; the lock screen adds its own pause after a failure. An
account with no password, or a locked one, cannot unlock the lock screen at all.

## Adding a root daemon

A new daemon should meet all of these. Where an existing one does not, the exception is stated in
its section: `kdos-mountd` breaks rule 6 (see
[One request per connection](#one-request-per-connection)), and `kdos-powerd` breaks rule 8 for
`kdos-bootctl` and `iw` (see [accent](#accent)).

1. It runs in the foreground and is started by an `/etc/init.d` script through `supervise`.
2. Its script skips with a printed reason, before supervision, when the machine cannot support it.
3. It owns exactly one socket in `/run`, mode 0666, and refuses to serve that socket unless it is
   root (or, like `kdos-energyd`, cannot start at all without root's access).
4. It authorises on the caller's credentials (root and `wheel`, plus `seat` for what the person at
   the machine needs without administering it) by calling `kb_uid_allowed()`, never a copy of that
   test, and answers `err not permitted` otherwise.
5. No verb takes a path. Identifiers come from a list the daemon published.
6. A connection that never speaks cannot stall anything the daemon does on a timer or a trigger.
7. It has a `--fixture` mode that decides and prints without acting, or, like `kdos-powerd`,
   flags that run each verb's rules against a scratch tree.
8. Every program it runs is named by absolute path and executed through an argument vector, never a
   shell.
9. It links only libraries whose every line you are willing to run as root.
10. Its refusals are documented in this chapter, including the ones that look like limitations.

Where the pieces go is fixed as well. The source and its recipe (`kpkgbuild` and `build.sh`) are
`src/daemons/<name>/`, the area that holds the five root daemons above (`kdos-boxsock`,
`xdg-desktop-portal-kdos` and `kdos-lock` are session programs, sources under `src/desktop/` and
named under the list's `src-desktop` heading); the port is named under the `src-daemons` heading of
`script/phases/50_desktop/packages.txt`, the only phase whose `PORT_REPO` includes `src/daemons`;
and its init script is `fs/etc/init.d/<NN>_<service>.sh`, whose number places it after
everything it needs, since `rcS` starts the scripts in numeric order (the five here are 55 to 59).

## See also

- [How KDOS differs](../01-philosophy/how-kdos-differs.md#init-and-service-supervision): the init
  and supervision these daemons run under, set against other distributions
- [Architecture overview](../03-architecture/overview.md): where these daemons sit in the running
  system
- [Boot and init](../03-architecture/boot-and-init.md): how `rcS` starts them and `rcK` stops them
- [The security model](../03-architecture/security-model.md): the authorisation argument in full
- [Packs and boxes](../03-architecture/packs-and-boxes.md): what the pack daemon implements
- [kdos-appbox](kdos-appbox.md): the pack daemon's client and the launcher that starts
  `kdos-boxsock`
- [kdos-res](kdos-res.md): the monitor that shows the energy daemon's answer
- [kinstall](kinstall.md): which groups the installed account ends up in
- [How KDOS is built](../05-developer/how-kdos-is-built.md#the-desktop-50_desktop): the build
  phase that compiles every daemon here
- [The ports catalogue](../06-reference/ports-catalogue.md#srcdaemons): the daemon ports and
  their versions
- [Filesystem and IPC](../06-reference/filesystem-and-ipc.md): every socket and verb in full
- [Glossary](../06-reference/glossary.md): box, graft, pack, surface and the other terms used here

<!-- book-nav -->
---

*Part IV — Programs, chapter 26.* Previous: [25. kdos-appbox](kdos-appbox.md) · [Contents](../README.md) · Next: [27. kinstall](kinstall.md)
