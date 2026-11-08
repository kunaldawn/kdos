# The daemons

KDOS keeps six root daemons of its own. Each answers a few fixed questions, on a Unix socket in
`/run`, on the system bus, or both: open a named service in the firewall, say which application is
spending the battery, kill a runaway program before the desktop freezes, mount application packs,
set the machine's name, clock and keyboard, and install a system update into the other root slot.
Everything else a desktop asks of root, such as suspending, logging in or mounting a USB stick,
belongs to programs KDOS ports from upstream: elogind, polkit, udisks2 and the rest. This chapter
describes KDOS's six, together with the per-box Wayland socket `kdos-boxsock`, which sits close to
privilege without being a root daemon, the other services `ksvc` runs beside them, and the two
login-record services, `utmpd` and `wtmpd`.

It is written for two readers:

- **An administrator** who wants to know what each daemon allows, who may ask it, which file
  configures it and how to find out why something was refused. Read [At a glance](#at-a-glance)
  and [The shape they share](#the-shape-they-share), then the section for the daemon in question.
- **A contributor** changing a daemon or adding one. Read the whole chapter, then
  [Adding a root daemon](#adding-a-root-daemon).

Why the authorisation is shaped this way, and what it defends against, is argued in
[The security model](../03-architecture/security-model.md). How init starts services is in
[Boot and init](../03-architecture/boot-and-init.md). Every socket path, bus name and verb on the
system is listed in [Filesystem and IPC](../06-reference/filesystem-and-ipc.md).

## At a glance

Each root daemon is one binary in `/usr/sbin`, started at boot by a script in `/etc/init.d` and
kept running by `ksvc`, the service supervisor in `kdos-tools`. None of them ships a D-Bus
activation file: a bus name is taken by the supervised daemon or by nobody.

| Daemon | Socket | System bus | Command-line client | Other clients | Init script | Skipped at boot when |
|---|---|---|---|---|---|---|
| `kdos-settingsd` | — | `org.freedesktop.hostname1`, `org.freedesktop.timedate1`, `org.freedesktop.locale1` | `kdos timezone`, `kdos hostname`, `kdos locale`, `kdos keymap` | System Settings' Date & Time page, plasma-setup, the login screen's KWin (`kwin_wayland --locale1`), startplasma, System Monitor's OS sensors | `43_settingsd.sh` | The binary is missing |
| `kdos-sysupdated` | — | `org.freedesktop.sysupdate1`, `io.kdos.Slots1` | `kdos update`, `kdos-bootctl` (which it runs) | Discover, the System Versions page, `kdos-notice` | `46_sysupdated.sh` | The binary is missing |
| `kdos-powerd` | `/run/kdos-powerd.sock` | `io.kdos.Power1` | `kdos-power` | The Firewall settings page, `kdos-fwask` and `kded-kdos` (over the bus), `kdos theme`, `kdos phosphor`, `kdos doctor` | `55_powerd.sh` | The binary is missing |
| `kdos-energyd` | `/run/kdos-energyd.sock` | — | `kdos-energy` | System Monitor's KDOS sensors (`ksystemstats-kdos`), `kdos doctor` | `56_energyd.sh` | The binary is missing, or no `/sys/class/powercap/*/energy_uj` is readable |
| `kdos-oomd` | `/run/kdos-oomd.sock` | — | — | `kdos doctor`, `kdos-notice` | `57_oomd.sh` | The binary is missing, or `/proc/pressure/memory` is not writable (the kernel has PSI off) |
| `kdos-packd` | `/run/kdos-packd.sock` | — | — | `kdos-appbox` (and `kdos app`, which runs it), Discover's KDOS backend, `kdos doctor` | `59_packd.sh` | The binary is missing, or `erofs` is absent from `/proc/filesystems` even after `modprobe erofs` |

`kdos-powerd` and `kdos-energyd` are each one binary with two names. Run under the daemon's name
it serves the socket; run under the client's name (`kdos-power`, `kdos-energy`) it sends one
request and prints the answer.

### Starting and stopping one by hand

The `service` command (a second name for `ksvc`) takes the verb first and then the **service
name**, which is the init script's name without its two-digit prefix and `.sh` suffix:

```sh
service status powerd
service restart settingsd
service stop packd
service start energyd
service list                # every service, whether it starts at boot, and its state
service disable oomd        # do not start it at boot
```

`rcS` and `rcK` are the boot and shutdown scripts in `/etc/init.d` that run every numbered service
script in order (see [Boot and init](../03-architecture/boot-and-init.md)). `disable` creates
`/etc/service.disabled/<name>`, which both skip; `enable` removes it. A name that matches no
script exactly is matched as a substring, so `service status pack` also reaches `59_packd.sh`.
The daemon's full name (`kdos-powerd`) matches nothing: it contains the service name rather than
the other way round.

The substring match finds the script for `status`, `start`, `stop` and `restart` only. `enable` and
`disable` check that some script matches, then write or remove the marker under the name exactly
as typed, while `rcS` and `rcK` look for the marker under the script's own service name. They
therefore need the exact service name: `service disable pack` reports success, creates
`/etc/service.disabled/pack`, and disables nothing.

### Who may ask for what

Two kinds of gate stand in front of these daemons. A socket asks the kernel who the caller is and
admits root and the `wheel` group, the administrators; a bus method asks polkit about the caller
and follows the action's defaults and whatever `/etc/polkit-1/rules.d` says. The person at the
machine is the active local session, as elogind reports it.

| Daemon | root | `wheel` | The active local session | Anyone else |
|---|---|---|---|---|
| `kdos-settingsd` | Every method, without asking polkit | polkit's answer | Time zone and network time without a password; the clock, locale, keyboard and names after an administrator's password | Refused |
| `kdos-sysupdated` | Every method, without asking polkit | polkit's answer | Checking and `Bless` without a password; installing, trying a slot and changing the channel after an administrator's password | Refused |
| `kdos-powerd`, socket | Every verb | Every verb | `ping`, `firewall list`, `firewall knocks`, and `accent` and `splash` when polkit grants them without a prompt | `ping`, `firewall list` and `firewall knocks` |
| `kdos-powerd`, bus | Every method | polkit's answer | The accent and the splash without a password; the firewall after an administrator's password | The reads only |
| `kdos-energyd` | Every verb | Every verb | Refused | Refused |
| `kdos-oomd` | Every verb | Every verb | Every verb | Refused |
| `kdos-packd` | Every verb | Every verb | Refused | Refused |

The non-administrator column of a socket is never a group: `kdos-oomd` asks elogind whether the
caller has the active session on a seat, and `kdos-powerd` asks polkit. A login elogind does not
count as active, such as an ssh session, gets the administrators' answer or nothing. Each daemon's
`--explain <user>` says which row a user falls in.

## The shape they share

Every root daemon here is built the same way. Knowing the pattern once tells you how to run,
diagnose and review all six.

### Supervision and logging

A daemon runs in the foreground and never forks into the background. The init script calls
`supervise <name> <binary>` from `/etc/init.d/service_helper`; the supervisor leads its own process
group, writes `/run/<full name>.pid`, and restarts the daemon five seconds after it exits.
`service stop <name>` sends SIGTERM to the whole group, waits for it to empty, checking every 50
ms, and sends SIGKILL to whatever is left when the stop grace runs out. The grace is five seconds
unless the init script passed `supervise --stop-grace <seconds>`; none of these six daemons does.
`supervise` on a name whose supervisor is alive starts nothing and says `[SKIP]`, so a second
`service start` cannot leave a daemon nothing stops. A `SIGTERM` sent to the supervisor alone is
passed on to its group, so the daemon goes with it. A daemon whose supervisor was killed outright is
still found through the state file: `service status` reports it as `[FAIL]` rather than `[DOWN]`,
`service start` refuses to start a second beside it, and `service stop` ends it.

The daemon's output goes to syslog and to a small file in `/run`. The supervisor points the
daemon's standard output and error, and its own *Starting* and *Exited* lines, at a forwarder
process. Each line becomes a daemon-facility syslog message tagged with the daemon's full name
(`supervise --facility <name>` picks another facility; none of these six daemons uses it), and
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
restarted every five seconds for as long as the machine is up. A refusal the script cannot see
(`kdos-energyd` finding no usable energy domain after the script's glob matched) is restarted
every five seconds, and `service status` reports it as `[FAIL]` with its exit status rather than
`[ OK ]` (see [ksvc and service](kdos-command.md#ksvc-and-service)).

The two bus-only daemons use two more `supervise` options:

- `--restart-with dbus` restarts the daemon whenever the system bus's own service is restarted or
  stopped, and holds each start while the bus is between two runs. A daemon left holding names on
  a bus that has gone answers nobody.
- `--final-exit 3` stops the respawn when the daemon exits 3, which `kdos-settingsd` and
  `kdos-sysupdated` do when another process already owns one of their names. Restarting cannot
  change that.

An init script whose daemon owns a bus name calls `wait_bus_name <name>` after `supervise`, which
holds the boot for up to ten seconds until the name has an owner, so the scripts after it find the
name taken. `kdos-powerd`'s script waits for `io.kdos.Power1` only when a system bus is running:
its socket answers whether or not the bus is up, and it retries the bus itself.

### The socket and the identity check

The four daemons with a socket each own one in `/run`, named after the daemon, and remove a stale
one before binding so that a socket file left by a crash cannot hold the name.

The listening socket and every accepted connection are close-on-exec (`SOCK_CLOEXEC` on the
`socket()` and on `accept4()`), so no program a daemon runs inherits a client's connection. A
program that kept one open would hold it past the reply: `kdos-powerd`'s `firewall ssh on` starts
`sshd` under its supervisor, and a client whose connection a running program still holds never
reads the end of its answer.

The socket is mode 0666, and anyone may connect; the real check is the caller's identity. The
daemon asks the kernel for the connecting process's credentials (`SO_PEERCRED`, which the caller
cannot forge) and answers `err not permitted` to anyone it does not admit. The file mode is left
open on purpose: a mode that looked like the authorisation is a mode somebody eventually loosens.

There is one implementation of the group check, `kb_uid_allowed()` in `libkbase`. It admits uid 0
outright; otherwise it looks the uid up in the password database and reads `/etc/group`, and admits
the account when the group's id is its primary group or its name is in the group's member list.
Membership is therefore read at the moment of the request, not from the caller's process
credentials, and a group change applies to the next request without a new login. No daemon keeps
its own copy of the rule: a copy per daemon is a rule that gets tightened on one socket and stays
loose on the others. A daemon that admits more than root and `wheel` states the difference beside
its own call, as `kdos-oomd` and `kdos-powerd` do.

The client never names a path. Every verb takes an identifier from a list the daemon itself
published: a firewall service name, an accent, a pack id.

### One request per connection

The client connects, writes one short line, reads the answer and the connection closes. There is
no session state.

Each daemon serves one request at a time. `kdos-powerd`, `kdos-energyd` and `kdos-oomd` give a
connection two seconds to send its request, because the same loop drives `kdos-powerd`'s bus face,
`kdos-energyd`'s sampler and `kdos-oomd`'s pressure trigger, and a client that connected and said
nothing would otherwise stop them. `kdos-packd` sets no such limit, so a connection that never
speaks delays every later request until it closes.

The two bus daemons never block on polkit. An interactive check lasts as long as somebody takes to
type a password, so the question goes out asynchronously and the method call is answered when
polkit is; the bus keeps answering other callers meanwhile.

### Fixtures, skips and test sockets

`kdos-energyd`, `kdos-oomd` and `kdos-packd` each have a fixture mode: the daemon can be pointed
at a recorded copy of the system state and made to print what it would do without doing it. This
is how the selection logic, which decides what gets measured, mounted or killed, is tested without
a machine to lose. `kdos-powerd` has none; its `--explain`, `--firewall`, `--accent`, `--splash` and
`--file-sharing` flags run the same rules against a scratch `/etc` instead (see
[Diagnosis and testing](#diagnosis-and-testing)). `kdos-settingsd` reads a scratch tree from
environment variables, and `kdos-sysupdated` serves a test bus over a fixture root; each is
described in its own section.

Each socket daemon reads another socket path from an environment variable (`KDOS_POWERD_SOCKET`,
`KDOS_ENERGYD_SOCKET`, `KDOS_OOMD_SOCKET`, `KDOS_PACKD_SOCKET`) so the test suite can run it
unprivileged. Moving the socket grants nothing: the check is the caller's identity, never the
path. `kdos-powerd`, `kdos-oomd` and `kdos-packd` refuse to start on their real socket unless they
are root, because an unprivileged daemon would answer `ok` to work it cannot do. `kdos-energyd` has
no such check; it refuses to start when it cannot read the energy counter, which without root it
cannot.

A socket address holds at most 107 bytes of path. `kdos-packd` refuses to start on a longer path,
because truncating would bind a socket at a path nobody asked for, and the next start would fail
with "address already in use" for a file that appears not to exist. `kdos-powerd`, `kdos-energyd`
and `kdos-oomd` cut an over-long path to fit and bind at the shortened one.

## kdos-powerd

The few root-owned settings a desktop that is not root still changes: which named services the
firewall lets in, the accent that everything drawn before a login wears (the boot menu, the
splash, the text console and the login screen), and the splash's motion. It offers the same verbs
twice: a socket for the `kdos` command, and `io.kdos.Power1` on the system bus for System Settings.
The bus face alone also starts Samba once a folder has been shared; see
[Starting Samba for a shared folder](#starting-samba-for-a-shared-folder).

Suspend, power-off and reboot are not this daemon's. They belong to elogind, which Plasma's Leave
menu, PowerDevil and the lid switch all reach through `org.freedesktop.login1`; see
[What happens around a suspend](#what-happens-around-a-suspend).

| Verb | Who, over the socket | Does |
|---|---|---|
| `ping` | Anyone | Answers `ok` |
| `firewall list` | Anyone | One row per named service: name, `on` or `off`, and what the service is for (the For column below, in the daemon's words), tab-separated, then `ok` |
| `firewall knocks` | Anyone | One row per named service: name and the number of packets turned away while it was closed, or `-1` where nothing was counted, then `ok` |
| `firewall <service> on\|off` | root, `wheel`, or polkit's `io.kdos.firewall.set` without a prompt | Open or close one named service |
| `accent <scheme>` | root, `wheel`, or polkit's `io.kdos.accent.set` without a prompt | Recolour the boot menu, the splash, the text console and the login screen |
| `splash phosphor\|modern` | The same as `accent` | Choose the boot splash's motion |

The three reads answer anybody: a liveness check, a file every user can read, and counts of
packets that were dropped. Every refusal is written to the daemon's log with the caller's uid and
the action polkit did not grant, so a setting that did not change can be attributed.

### The client

```sh
kdos-power suspend|poweroff|reboot
kdos-power ping
kdos-power firewall list|knocks|<service> on|off
kdos-power accent <scheme>
kdos-power splash phosphor|modern
```

`suspend`, `poweroff` and `reboot` never reach the daemon. The client replaces itself with
`loginctl suspend`, `loginctl poweroff` or `loginctl reboot`, run as the caller, so elogind sees
the caller, polkit answers about the caller's session, and the screen locker's sleep inhibitor
gets its lock in before the machine goes down: the same path the Leave menu takes. When `loginctl`
cannot be run the client says that elogind owns these and exits 1.

Every other verb is one request line of at most 63 bytes, newline included; the daemon reads
nothing past that.

| Exit status | Means |
|---|---|
| 0 | The daemon answered `ok` |
| 1 | The daemon answered `err …`, closed without answering, or is not running (``kdos-power: no kdos-powerd — `service start powerd` ``) |
| 2 | Bad usage, or an argument too long for the request line |

When the daemon is not running, start it with the command the client prints, `service start
powerd` (see [Starting and stopping one by hand](#starting-and-stopping-one-by-hand)).

`firewall` prints the daemon's whole reply on standard output, the `err` line included, so a caller
capturing the table reads all of it. For `accent` and `splash` an error is printed to standard error
as `kdos-power: <reply>`, and a success that left something unchanged prints the daemon's note the
same way, such as `kdos-power: (boot menu unchanged)`.

### io.kdos.Power1

The bus face, at `/io/kdos/Power1` on interface `io.kdos.Power1`, is what the Firewall settings
page and `kdos-fwask` call. Root owns the name; anybody may call it, and polkit decides who may
change anything.

| Member | Kind | Does |
|---|---|---|
| `ListFirewallServices() → a(sbs)` | Method | Each named service: name, whether it is open, what it is for |
| `ListFirewallKnocks() → a(st)` | Method | Each closed service and the packets turned away |
| `SetFirewallService(s name, b open)` | Method | polkit `io.kdos.firewall.set` |
| `SetAccent(s scheme)` | Method | polkit `io.kdos.accent.set` |
| `SetSplash(s motion)` | Method | polkit `io.kdos.accent.set` |
| `StartFileSharing()` | Method | polkit `io.kdos.filesharing.start`; runs `/etc/init.d/83_samba.sh start` |
| `Accent` (`s`) | Property, emits change | The scheme `/etc/kdos/accent` names |
| `Accents` (`as`) | Property, constant | Every scheme this build knows |
| `FirewallServicesChanged()` | Signal | After any firewall change, from either face |

| Action | Active local session | Inactive | Any other |
|---|---|---|---|
| `io.kdos.firewall.set` | `auth_admin_keep` | `no` | `no` |
| `io.kdos.accent.set` | `yes` | `no` | `no` |
| `io.kdos.filesharing.start` | `yes` | `no` | `no` |

The subject polkit is asked about is the caller's bus name, which polkitd resolves to a process
and a session itself, so nothing the caller says about who it is takes part. A caller that did not
allow a prompt is not prompted: a challenge it could have answered comes back as the standard
`InteractiveAuthorizationRequired` error rather than `AccessDenied`. A bad name answers
`io.kdos.Power1.Error.NoSuchService`, `NoSuchAccent` or `NoSuchMotion`.

The socket comes up before the bus and does not wait for it. A system bus that cannot be reached is
retried every five seconds from the daemon's loop while the socket answers, so the `kdos` command
reaches the firewall on a machine whose bus is down.

### Starting Samba for a shared folder

`smbd` does not run until something is shared. At boot, Samba's service script
`/etc/init.d/83_samba.sh` makes `/var/lib/samba/usershares` (root's, group `sambashare`, mode
1770) and starts `smbd` only when that directory holds a usershare or `/etc/samba/smb.conf` defines
a share below `[global]`; otherwise it prints `[SKIP] samba: nothing is shared`. A folder shared
from Dolphin's Share tab is a file `net usershare add` writes into the directory as the user, with
`smbd` still down.

The session's `kded-kdos` module watches the directory and, when it goes from holding no share to
holding one, calls `StartFileSharing()`; a share already there when the session starts is not
asked about, because the boot decided on it. The method takes no argument, so a caller names nothing:
after polkit's `io.kdos.filesharing.start`, which the person at the machine is granted, the daemon
runs `/etc/init.d/83_samba.sh start` as an argument vector, and the script decides again whether
anything is there to serve. That is all the next boot would do, which is why the action needs no
password. The call is refused with `io.kdos.Power1.Error.Failed` while
`/etc/service.disabled/samba` exists, the boot's own marker for a service an administrator turned
off, or when Samba is not installed. A service already supervised is not started twice, so a
repeated call costs one script run. Nothing stops `smbd` again: removing the last share leaves it
serving nothing until the next boot, which does not start it.

### What happens around a suspend

elogind runs a suspend; this daemon takes no part in it. What runs, in order:

1. Whatever asked (`kdos-power suspend`, the Leave menu, PowerDevil, or the lid, which
   `/etc/elogind/logind.conf.d/50-kdos.conf` sets to suspend unless the machine is docked) calls
   login1's `Suspend`, and polkit answers for the caller's session.
2. elogind emits `PrepareForSleep(true)` and waits up to five seconds (`InhibitDelayMaxSec=5`) for
   the delay inhibitors to let go. kscreenlocker locks the session there, and NetworkManager, built
   with `-Dsuspend_resume=elogind`, takes its devices down.
3. The hooks in `/usr/lib/elogind/system-sleep` run, TLP's `49-tlp-sleep` among them, which saves
   the radio states and applies TLP's suspend settings.
4. The kernel suspends to RAM. `/etc/elogind/sleep.conf.d/50-kdos.conf` turns hibernation and the
   hybrid modes off, because swap on KDOS is zram, which holds nothing across a power-off.
5. On waking the hooks run again with `post`, and `PrepareForSleep(false)` brings NetworkManager
   back.

The power key powers off (`HandlePowerKey=poweroff`), which is init's ordinary shutdown through
`rcK` (see [Boot and init](../03-architecture/boot-and-init.md#shutdown)).

### accent

`accent <scheme>` takes one of the eight scheme names compiled into `libkcolor`: `phosphor`,
`amber`, `ice`, `bone`, `norton`, `borland`, `perfect` or `paper`. The name is matched by
`kcol_find()`; anything else is refused with `err not an accent`. There is no path to aim and
nothing to traverse, which is why this verb is safe to reach from a session.

It writes only root-owned files and the login screen's own:

- `/etc/kdos/accent`, which `rcS` reads to recolour the boot splash that is already running, and
  which the bus face's `Accent` property reports;
- the boot menu (`limine.conf` on the EFI system partition, the ESP) and the text-console palette
  (`/etc/vtrgb`), by running `kdos-bootctl theme <scheme>`, which owns both files. `kdos-bootctl` is
  found through the daemon's inherited `PATH`; every other program the daemon runs is named by
  absolute path;
- the login screen. The SDDM greeter runs as the `sddm` account and takes its look from that
  account's `~/.config/kdeglobals`, `plasmarc` and `kcminputrc` and the picture
  `~/kdos-background.png`. One set per accent is built into `/usr/share/kdos/greeter/<accent>/`, a
  light accent's carrying its dark partner because the greeter never follows day and night, and
  the daemon copies the accent's set into the `sddm` home. The copy runs in a child with the
  `sddm` account's uid and gid and no other group, so a link the account left in place of a file
  is never followed as root. A set missing a file leaves the greeter as it was, and each file is
  replaced by a rename.

It recolours nothing inside a session. That is `kdos theme`'s job, which runs as the user and
touches only the user's own files; `kdos theme` calls this verb for the parts that need root.

A machine with no writable ESP, or no greeter, is not a failure. The live medium is read-only, and
a machine may have no `/boot/efi` at all. When a part cannot be written the verb still succeeds and
names what stayed as it was: `ok <scheme> (boot menu unchanged)`, `(login screen unchanged)` or
`(boot menu and login screen unchanged)`. Refusing would make `kdos theme` look broken on the ISO,
where everything else recolours correctly.

### splash

`splash phosphor|modern` sets the boot splash's motion: a CRT power-on with Phosphor mode, a flat
mark without it. The splash runs in the initramfs before any root filesystem is mounted, so the
kernel command line is the one thing it can read; the verb runs `kdos-bootctl splash <motion>`,
which writes `kdos.splash=<motion>` on every KDOS entry in `limine.conf`. Any other word is refused
with `err not a splash motion (phosphor or modern)`. On a machine whose boot menu cannot be written
the answer is `ok <motion> (boot menu unchanged)`, for the accent's reason. `kdos phosphor on` and
`off` send it.

### firewall

`firewall` names services, never ports, and the table of what each name means belongs to the
daemon. A client that could name a port could open any port; a client that can only name `ssh`
opens exactly what the daemon's table says `ssh` is. The Firewall settings page asks for the list
rather than carrying its own copy.

| Service | Opens | For |
|---|---|---|
| `ssh` | TCP 22 | Incoming SSH (`70_sshd.sh`). Turning it on also removes `/etc/service.disabled/sshd` and starts `sshd`; turning it off puts the marker back and stops `sshd` |
| `http` | TCP 80 | A web server on this machine |
| `https` | TCP 443 | A TLS web server on this machine |
| `ipp` | TCP 631 | Sharing a printer with CUPS |
| `smb` | TCP 445 | Sharing files over SMB |
| `kiwix` | TCP 8080 | `kiwix-serve` |
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
| `vnc` | TCP 5900 | This desktop over VNC (krfb, on its default port, while sharing is on in a session) |
| `rdp` | TCP 3389 | This desktop over RDP (krdp, on its default port, while sharing is on in a session) |
| `xonotic` | UDP 26000 | Hosting a Xonotic game for the LAN |

A port moved in krfb's or krdp's own settings stays shut until a hand-written rule beside the
daemon's file opens it.

**A closed service is counted.** In the daemon's file an open service is its rule; a closed one is
the same match counting into a named counter, `kdos_knock_<service>`, after which the packets meet
the chain's policy and are dropped. `firewall knocks` and `ListFirewallKnocks` read those counters
with `nft list counters`, which is how the Firewall page and `kdos-fwask` can say that a phone or an
RDP client was turned away and offer to let it in by name. A reload rebuilds the table, so every
change starts every counter from zero; a counter the kernel does not hold reads `-1`, which is not
the same as nothing having knocked.

How a change is applied:

1. The daemon reads which names are on by looking for each rule's exact text in
   `/etc/nftables.d/50-kdos-services.nft`.
2. It rewrites that file **whole** from the names that are on, as additions to the `input` chain of
   the `inet filter` table. It never merges, because merging means parsing nftables syntax, and a
   parser that got it wrong would leave a port open that the settings page showed as closed.
   Anything you write by hand belongs in another file under `/etc/nftables.d`, which the daemon
   never reads or touches. The new file is renamed into place before anything checks it, because
   the check reads it through `/etc/nftables.conf`'s include. The daemon reads the current file
   first so that it can put it back; a file that exists but cannot be read refuses the toggle with
   `err cannot read the current rules`.
3. It runs `nft --check -f /etc/nftables.conf`. If the ruleset would not load, the previous
   `50-kdos-services.nft` is put back, or removed if there was none, and the answer is
   `err the ruleset would not load; nothing changed`. The kernel keeps the previous rules, and
   `firewall list` and the next boot see the previous state too.
4. It runs `nft -f /etc/nftables.conf`. That file deletes and rebuilds only its own `inet filter`
   table, so the NAT that netavark (the container network backend) sets up for rootful containers
   and NetworkManager's for a hotspot survive a toggle. If the load itself fails, the previous file
   is put back the same way and the answer is `err the ruleset did not apply`.

A successful toggle answers `ok <service> on|off` and emits `FirewallServicesChanged`. An unknown
name answers `err no service called that`; a state other than `on` or `off` answers
`err a service is on or off`.

### Diagnosis and testing

```sh
kdos-powerd --explain <user>
kdos-powerd --firewall list|knocks
kdos-powerd --firewall <service> <on|off>
kdos-powerd --accent <scheme>
kdos-powerd --splash phosphor|modern
kdos-powerd --file-sharing
```

`--explain` answers "would this user be allowed, and why". It is the first thing to run when a
settings change is refused, because a refusal otherwise shows up only in the daemon's log. It asks
`libkbase` the same question the socket asks, needs no privilege, and prints one line, for example:

```text
alice: uid 1001, primary gid 1001, not in wheel — permitted ping, the firewall list and its knocks; `firewall` only where polkit grants io.kdos.firewall.set and `accent` and `splash` only where it grants io.kdos.accent.set, each without a prompt (the shipped policy: the accent and the splash from an active session at the machine), otherwise refused
```

polkit's half is named and not asked: polkit answers about a process, and there is no process to
ask it about. It exits 0 when the group rule admits the three changes and 1 otherwise.

The other flags run a verb's own rules without the socket, which is the only way to test them: the
socket's check needs two different users to exercise. They grant nothing, because the binary is
writing to an `/etc` the person running it could already write to. `KDOS_POWERD_ETC` replaces
`/etc`, and a daemon pointed at another `/etc` leaves the kernel's ruleset, `sshd`, Samba, the boot menu
and the login screen alone: `KDOS_POWERD_NFT`, `KDOS_POWERD_BOOTCTL` and `KDOS_POWERD_SAMBA` name
stand-ins for `nft`, `kdos-bootctl` and Samba's service script (unset, nothing is run), and `KDOS_POWERD_GREETER` and
`KDOS_POWERD_SDDM_HOME` name the greeter sets and a home to copy them into. `KDOS_POWERD_SOCKET`
moves the socket.

## kdos-energyd

Per-application energy attribution: which application is using the processor's power.

Measuring the processor's energy is routine: the kernel exposes the counter under
`/sys/class/powercap`. Attribution is the hard part on an ordinary Linux desktop: an application is
dozens of processes in scattered groups, and nothing owns enough of the system to name them. On
KDOS every boxed application (one running in its own [box](../06-reference/glossary.md#box), a rootless
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
not running or answered `err`, and 2 on bad usage. An `err` answer goes to standard error. System
Monitor's KDOS sensors (`ksystemstats-kdos`) read `report-json`; each row there carries the display
name, and beside it `app` and `box` (`null` for a process in no box), so a reader that groups by
box never parses the display string. When the daemon is not running the client suggests
`service start energyd` (see [Starting and stopping one by hand](#starting-and-stopping-one-by-hand)).

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
  kwin_wayland                            15.4%  ███                   gpu 25.0%
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
given below, and it is short enough to stay far inside the counter's wrap period. Seven rules each
change the answer:

- Nested domains are dropped. The power interface lists a package beside that package's own
  sub-domains, so summing the list counts the cores twice (in the fixture, a 15 W idle floor
  becomes 26.25 W). A domain inside another's directory is a sub-domain and is skipped. The
  platform-wide `psys` domain goes the other way: it contains the packages, so where it exists it
  replaces them.
- The counter wraps, roughly every half hour at a laptop's power draw. A naive subtraction would
  produce one enormous negative reading with nothing in the output saying so. The daemon reads each
  domain's `max_energy_range_uj` and corrects a single wrap per window.
- A window the machine slept through is dropped. The daemon compares how far `CLOCK_BOOTTIME` and
  `CLOCK_MONOTONIC` moved across each window; a difference over one second means a suspend inside
  it. The counter after a resume runs from wherever the firmware left it, so the delta across one is
  a wrap that never happened or a reset read as one. Such windows are counted and left out, and the
  report says how many (`suspended_windows` in the JSON).
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

A process's box is found by walking its parents to the container supervisor that started it, not
from its cgroup: a box started where rootless podman could write no cgroup of its own sits in its
parent's, and an elogind session's delegated subtree is named for the session, not the box.

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
holding a `proc` tree, a `powercap` tree, an optional `dt` in seconds and an optional `slept`, the
seconds the machine was suspended inside that step's window) through the same sampler and report,
and prints the result. `KDOS_ENERGY_PROC` and `KDOS_ENERGY_POWERCAP` point the reader at other
`/proc` and powercap trees, and `KDOS_ALIEN_APPS` at another application-name table.

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
- A stall kills only when memory is short. When the trigger fires the daemon reads
  `/proc/meminfo` and kills nothing unless `MemAvailable` is at most 10% of `MemTotal`. A single
  busy process stalls every task that wants to run when it waits on reclaim itself: a copy
  streaming gigabytes through the page cache, such as the installer's `rsync`, fires the trigger
  with most of memory available and nothing else asking for any, and killing it would free
  nothing anyone was short of. A process pushing the working set out is the case that also takes
  `MemAvailable` down. A `/proc/meminfo` that cannot be read leaves the stall as the only test.
- A process can be protected by its `oom_score_adj`. A process whose
  `/proc/<pid>/oom_score_adj` is -500 or lower is never a victim; -1000 is the kernel's "never
  kill" and -500 is the threshold other user-space killers honour. Only root can lower the value,
  and a child inherits it, so a root program shields everything it will start by lowering its own
  first (`choom -n -1000 -p $$` in a shell). This is the contract a program uses to keep work it
  must finish out of reach; no program outside the desktop is protected by name.
- The desktop is never a victim. Protected are process 1, kernel threads, the daemon itself, every
  process whose name starts with `pipewire`, and these by process name: `kwin_wayland`,
  `kwin_wayland_wr`, `plasmashell`, `ksmserver`, `kded6`, `plasma_session`, `kglobalacceld`,
  `kscreenlocker_g`, `Xwayland`, `xdg-desktop-por`, `polkit-kde-auth`, `ksecretd`, `sddm`,
  `sddm-helper`, `sddm-greeter-qt`, `elogind`, `turnstiled`, `dbus-daemon`, `ksvc`, `kdos-powerd`,
  `kdos-energyd`, `kdos-oomd`, `kdos-settingsd`, `accounts-daemon` and `wireplumber`. Killing the
  compositor to save the desktop is not a trade, and killing the audio session manager leaves a
  machine whose sound stopped for no visible reason.
- Every name in that list is a process name as the kernel keeps it, which is at most 15 bytes. The
  kernel cuts the executable's name there, so `kwin_wayland_wrapper` is `kwin_wayland_wr`,
  `kscreenlocker_greet` is `kscreenlocker_g`, `sddm-greeter-qt6` is `sddm-greeter-qt` and
  `polkit-kde-authentication-agent-1` is `polkit-kde-auth`. A name spelled in full would match
  nothing, and the process it was meant to spare would be the first one chosen.
  `xdg-desktop-por` is every portal at once, the front end and the KDE back end. The greeter's
  names run only before a login and the lock screen's only while the screen is locked; protecting a
  name that is not running costs nothing.
- A box over its declared memory budget goes first. A box profile's `memory` key is passed to
  the container, and the kernel enforces it only when the box's cgroup sits in an elogind session's
  delegated subtree; a box started anywhere else has the limit accepted and ignored. This rule is
  what makes the key mean something there too (see
  [Memory and grants](kdos-appbox.md#memory-and-grants)). The daemon adds up the resident
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
  `libkproc` that `kdos-energyd` uses.
- The memory comes back immediately. The daemon takes a handle on the process first (a pidfd,
  so the next step cannot land on a recycled process id), sends SIGKILL, then calls
  `process_mrelease` to free the victim's memory at once rather than whenever it is reaped. On a
  kernel without that call the release is skipped and the kill stands.
- At most one scan every ten seconds. The cool-down starts when the trigger fires, not when a
  kill succeeds, so a trigger that finds nothing killable does not rescan every entry in `/proc`
  once a second on a machine that is already stalling.

The daemon shields itself from the emergency it watches: at start-up it sets its own
`oom_score_adj` to -1000 and locks its memory with `mlockall`. Each kill is logged with the victim,
the ten-second pressure averages and the memory available when it chose, in the form
`kdos-oomd: killed <name> [(appbox <box>)] (pid <pid>, <size> MB) — memory pressure avg10 full=<f> some=<s>, <avail> of <total> MB available`.
A trigger that finds memory not short logs nothing.

### The socket

Nothing in the protocol names a process, so there is nothing to aim. The socket takes no argument
and answers root, `wheel` and a user with the active session on a seat:

| Verb | Answers |
|---|---|
| `ping` | `ok` |
| `status` | `ok trigger 'full 150000 1000000', kills <n>` and, after the first kill, `, last: <name> [(appbox <box>)] (pid <pid>, <size> MB)` |
| `protected` | `ok` and the protected names, space-separated, the prefix rule last as `pipewire*` |

The active-session answer comes from elogind's `sd_uid_get_state()`, read through `libelogind`,
which the daemon opens at first use rather than links. The daemon exists for the minute memory runs
out, and a session library that is missing or broken must cost the read tier and never the killer.
Root and `wheel` never reach that question.

`kdos doctor` reports whether the socket is present and lists the protected names that are not
running, naming those that run only at certain times. `kdos-notice` reads `status` to say how many
processes have been killed since boot.

### Testing it

`kdos-oomd --fixture <dir>` reads a recorded `/proc` tree, prints the process it would kill as
`would kill <name> … (pid <pid>, <size> kB)` with the recorded pressure, and signals nobody. It
exits 0 when there is a candidate and 1 (printing `no candidate`) when there is none. A tree with a
`meminfo` whose `MemAvailable` is above 10% of `MemTotal` prints
`no kill: <avail> of <total> kB available` and exits 1, the answer the daemon gives a trigger
there; a tree without one is judged on the stall alone.

`kdos-oomd --protected` prints the protected names one a line, the prefix rule last, which is what
the check that holds each name to 15 bytes reads. `kdos-oomd --explain <user>` prints whether that
user may read the socket and why, and exits 0 when permitted.

`testing/oomd-fire.sh` exercises the real path in a booted virtual machine: it allocates and
touches memory until the trigger fires, then asks the socket what happened rather than watching
the allocating process die (the kernel's own killer would also have got it, later). A passing run
shows `status` going from `kills 0` to `kills 1, last: <name> (pid <pid>, <size> MB)` while the
script that started the allocation keeps running. That run has one large process to choose; which
process the daemon picks on a busy desktop is what `--fixture` tests.

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
| `install <file> [<index>]` | Copy a pack out of the staging directory, verify the copy and move it into the store. `<index>` is a second staged file name, a signed index with its signature staged as `<index>.sig`; the daemon reads no index a request does not name |
| `remove <id>` | Take a pack out of the store |
| `rollback <id>` | Return to a retained earlier version |
| `graft <id>`, `ungraft <id>` | Place a data pack's contents where its consumer looks for them, or remove them (a [graft](../06-reference/glossary.md#graft)) |
| `ping` | `ok` |
| `status` | Tab-separated lines: `store`, `medium` and `staging` directories, `retain` count, mount `route` (`file-backed`, `loop device`, or `not yet used`), `packs` (installed, available and mounted counts), `boxes` (how many are composed), and a `box` line for each composed box (name, merged root, `ephemeral` or `persistent`), then `ok` |

Every verb answers root and `wheel` only. Launching a boxed application sends `info` for the pack's
environment and `compose` for its box; `kdos-box start` sends `compose` and `kdos-box remove` sends
`decompose`. `kdos-appbox genlaunchers --packs` sends `list` and then `mount` for each installed
pack. `kdos app import`, `kdos-appbox import` and `kdos-box import` copy a pack into the staging
directory and send `install`, and an import of a data pack sends `graft`. Discover's KDOS backend
sends `list` and `info` to show the packs, and `graft`, `ungraft` and `remove` when a person
installs or removes one. `kdos-appbox status` and `kdos doctor` ask for `status`, and `kdos doctor`
reports the mount route. `kdos app install` and `kdos app remove` build and delete container images
and ask this daemon nothing. No program on the system sends `unmount` or `rollback`; those verbs
are reached only by a client that speaks the socket directly.

An id is at most 63 characters from `[A-Za-z0-9._-]` and does not begin with `.` or `-`; anything
else is refused before it is looked up.

Paths:

| Path | What |
|---|---|
| `/var/lib/kdos/packs` | The store; the current version of a pack is `<id>.kpack` |
| `/var/lib/kdos/packs/staging` | Where an unprivileged download lands; mode 01777, set when the daemon starts |
| `/var/lib/kdos/packs/mnt` | Mountpoints |
| `/mnt/iso/packs` | Packs on the boot medium |
| `/var/lib/kdos/packs/pack-manifest` | Every graft made, so an ungraft removes exactly what was added; in the shared store, so at start the daemon makes every host graft it names that this root lacks |
| `/var/lib/kdos/packs/pack-manifest.ungrafted` | The paths an ungraft dropped, so the other slot unlinks its own copy of each at its next start |
| `/etc/kdos/keys/packs` | The keys packs are verified against, separate from `/etc/kdos/keys`, which is `kpkg`'s package-repository ring |
| `/etc/kdos/packd.conf` | Configuration |

The daemon reads these environment variables in normal operation, not only in fixture mode. The
init script sets none of them:

| Variable | Effect |
|---|---|
| `KDOS_PACK_STORE`, `KDOS_PACK_MEDIUM`, `KDOS_PACK_MANIFEST` | Replace the store, the boot-medium pack directory and the graft record |
| `KDOS_PACK_HOME`, `KDOS_PACK_SHARE` | Replace the caller's home directory and `/usr/share` as graft and box-upper destinations |
| `KDOS_PACK_RETAIN` | Overrides `retain` in `packd.conf` (below); a negative value counts as `0`, as it does in the file |
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
then asked a daemon to mount would have verified nothing. The daemon first copies the staged pack,
and the staged index the request names and its `<index>.sig` when there is one, into private root-owned
files in the store (`.install.part`, `.install-index.part`, mode 0644), and everything after reads
the copies. The staged file belongs to whoever put it in a directory every user may write, so a
check on it would say nothing about the bytes a later read found. A staged symlink or anything
other than a regular file is refused with `err cannot read <name> from the staging directory`.
An install therefore needs free space in the store for a second copy of the pack while it runs.

1. If the staging directory held a `PACKAGES` index whose signature is good and which lists this
   pack, the pack's SHA-256 must match the index entry; a mismatch is refused
   (`<id> does not match the signed index`).
2. Otherwise the pack's own signature block is checked against `/etc/kdos/keys/packs`. A good
   signature is accepted; a bad one, a hash mismatch, or a signature by a key the ring does not
   hold is refused.
3. A pack with no signature at all is accepted and logged as unsigned, unless `KDOS_REQUIRE_SIG`
   is set.

A pack that passes has its copy renamed to `<id>.kpack`, and only then is the staged file deleted;
a refused install deletes the copies and leaves the staged file where it was.

A pack in the store was verified when it was written and only root can write the store, so it is
not re-hashed at each mount. A pack on the boot medium was never installed, so it is verified at its
first mount; there a bad signature, a hash mismatch or a missing key is refused, and an unsigned
pack mounts. Every pack is mounted read-only with `nosuid` and `nodev`, and a data pack with
`noexec` as well.

The list is rescanned on every request, so a medium pulled out between two requests is not still
offered. At start-up the daemon adopts whatever packs and boxes are already mounted, so a restart
while boxes are running does not lose track of them, and then makes every host graft the store's
`pack-manifest` names for a data pack in the store and this root lacks, so a data pack grafted from
the other A/B slot is in place in this one; see
[Grafts and data packs](../03-architecture/packs-and-boxes.md#grafts-and-data-packs).

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
`<id>-<version>.kpack`, renames the newest retained version (by version comparison, as the sweep
keeps them, so `1.10` is newer than `1.9`) into its place, and answers
`ok <id> <current> -> <restored>`. The version rolled away from stays in the store under its own
version. A retained copy of the installed version itself, left by installing the same version
again, is never the one restored; with nothing else retained the answer is
`err no earlier <id> is kept`.

The store's row for an id is `<id>.kpack` and no other file. A retained `<id>-<version>.kpack` is
material for `rollback` and is never listed, mounted or composed. The sweep and `rollback` count a
file as a retained version of an id only when its own metadata names that id, because an id may
contain `-`: `foo-bar.kpack` is the installed pack `foo-bar`, not a version `bar` of `foo`.

### Diagnosis and testing

`kdos-packd --fixture <store> [medium]` lists the packs in a scratch store with their signature
state, composes each application pack, grafts each data pack (printing each graft's destination),
and prints the result, mounting nothing. `KDOS_PACKD_VERBOSE` shows its log lines; `KDOS_KEYS`
points it at another key directory. The daemon reads `KDOS_KEYS` only in fixture mode; the
`kdos-pack` tool reads it in normal use, to choose the ring its `verify` and `info` check against
(default `/etc/kdos/keys/packs`). `kdos-packd --fixture-graft <store> <id>` is the `graft` verb and
`kdos-packd --fixture-start <store>` the start-up re-graft over a scratch store, each mounting
nothing but writing the links and the record for real, under `KDOS_PACK_SHARE` (instead of
`/usr/share`) and `KDOS_PACK_HOME` (instead of the caller's home); two share directories over one
store stand for the two slots of an A/B machine.

The two mount routes, reference counting and adoption of existing mounts at start-up are described
in [Packs and boxes](../03-architecture/packs-and-boxes.md).

## kdos-settingsd

The machine's name, clock, time zone, locale and keyboards, on the system bus. KDE changes these
by calling three interfaces systemd defined, `org.freedesktop.hostname1`,
`org.freedesktop.timedate1` and `org.freedesktop.locale1`, rather than by writing files. The Date &
Time page of System Settings saves through timedate1 and nothing else; plasma-setup uses all three
on an OEM first boot; the login screen's compositor runs `kwin_wayland --locale1` and takes its
keyboard layout from locale1, following it when it changes; startplasma copies that layout into
each session; System Monitor's OS sensors read hostname1. KDOS has no systemd, so this daemon
answers for them, with every method and property systemd's three interfaces have except
hostname1's `Describe`, which nothing on the system calls. It is C on `basu` and `libkbase` and
links no glib and no Qt. What a person sees of it, page by page, is in
[KDOS's settings pages](kdos-settings.md#kdos-settingsd).

### Where each setting is kept

Each setting is kept in the file the rest of the machine already reads, and nothing is cached:
every getter reads its file, so a value the installer or a person with an editor wrote is what the
next caller sees.

| Setting | File |
|---|---|
| Static hostname | `/etc/hostname`, which `05_hostname.sh` gives the kernel at boot; the `127.0.1.1` line of `/etc/hosts` follows the name, because musl resolves the machine's own name from that file and nothing else before the DNS |
| Pretty name, icon, chassis, deployment, location | `/etc/machine-info` |
| Time zone | `/etc/localtime`, a symlink into `/usr/share/zoneinfo`, and `/etc/profile.d/20-timezone.sh`, which exports `TZ=':/etc/localtime'` |
| Wi-Fi country | `/etc/modprobe.d/kdos-regdom.conf`, from the zone |
| Network time | Whether `/etc/service.disabled/chrony` exists |
| Locale | `/etc/locale.conf`, bare `NAME=value` lines, which SDDM puts into every session |
| Console keymap | `/etc/keymap`, which `kdos-getty` loads on every console |
| Graphical keyboard | `/etc/kdos/xkb.conf` (`XKBMODEL`, `XKBLAYOUT`, `XKBVARIANT`, `XKBOPTIONS`), never under `/etc/X11` |

Both halves of a time zone are written or the call fails. musl reads `TZ` where it is set, and
every KDOS login sets it, so writing only the link would leave every new login reporting the old
zone. The colon form points musl at the link itself, the one value that cannot drift from it, and
it is the form Qt reads as "follow `/etc/localtime`".

**The Wi-Fi country follows the zone.** The kernel's wireless layer starts in the "world"
regulatory domain, which keeps every 5 GHz DFS channel (one shared with radar) closed and caps
transmit power until something names a country, and some drivers never take one from an access
point. The daemon looks the zone up in `zone.tab`, which maps each zone to exactly one country
code, writes `options cfg80211 ieee80211_regdom=<CC>` to `/etc/modprobe.d/kdos-regdom.conf` for the
next boot, and runs `iw reg set <CC>` for this one. A zone with no country, such as `UTC`, removes
the file. Neither step failing fails the zone: a machine with no radio still has a time zone. The
installer writes the same file from the zone chosen during installation.

**An empty layout follows the console.** The installer writes the console keymap alone and the
shipped `xkb.conf` leaves `XKBLAYOUT` empty, so locale1's `X11Layout` is then the layout nearest
`/etc/keymap`, and the login screen has the installed keyboard from the first boot. A derived
layout is given only when `xkeyboard-config` carries it: KWin falls back to US for a layout it
cannot compile and says so only in its log.

**The hardware clock is UTC.** The kernel reads it as UTC at boot and writes it back every eleven
minutes while chrony has the clock synchronised, so `LocalRTC` is false and `SetLocalRTC(true)` is
refused. A clock set by hand is not synchronised, so `SetTime` writes the hardware clock itself.

**Network time is chrony.** `CanNTP` is true when chrony's init script and `chronyd` are both
installed. `SetNTP(true)` removes the marker and runs `service start chrony`; `SetNTP(false)` puts
it back and runs `service stop chrony`. On a machine without chrony, `SetNTP(true)` answers
`NoNTPSupport` and `SetNTP(false)` succeeds, as systemd-timedated's does: the Date & Time page sends
`SetNTP(false)` on every Apply when `CanNTP` is false, and refusing it would fail the whole Apply.

### Checking, then asking, then writing

A method validates its arguments before polkit is asked, so a bad value is refused without a
password prompt, and polkit answers before a single byte is written. Every value is checked
against a character set and, where the machine carries the list, against the list: a zone must be
a regular file in the zone tree that starts with the `TZif` magic, a console keymap must be one of
`kbd`'s keymaps and an XKB layout one of `xkeyboard-config`'s `rules/evdev.lst`. A name that passes
the character rule and names nothing is refused, because a keymap nobody can load and a layout no
compositor can compile both fail silently later, at the login screen, where nobody can type the way
out.

Root is answered without asking polkit. Everyone else gets polkit's answer under systemd's own
action ids, because plasma-setup's rule and the Date & Time page name them:

| Action | Active local session |
|---|---|
| `org.freedesktop.timedate1.set-time` | `auth_admin_keep` |
| `org.freedesktop.timedate1.set-timezone` | `yes` |
| `org.freedesktop.timedate1.set-ntp` | `yes` |
| `org.freedesktop.timedate1.set-local-rtc` | `auth_admin_keep` |
| `org.freedesktop.locale1.set-locale` | `auth_admin_keep` |
| `org.freedesktop.locale1.set-keyboard` | `auth_admin_keep` |
| `org.freedesktop.hostname1.set-hostname`, `set-static-hostname`, `set-machine-info` | `auth_admin_keep` |
| `org.freedesktop.hostname1.get-product-uuid`, `get-hardware-serial` | `auth_admin_keep` |

`set-time` implies `set-timezone` and `set-ntp`, so the Date & Time page's one password covers its
three calls. The last two guard the values that identify the machine, which the kernel makes
root's alone. An interactive question may stay open for five minutes, after which the call is
refused; no answer from polkit is a refusal.

The programs it runs, `service`, `loadkeys` and `iw`, are argument vectors found through `PATH` and
skipped when missing, because every argument they are given came over a bus.

### Starting and testing it

`43_settingsd.sh` starts the daemon under `ksvc` after `polkitd` and before the display manager,
with `--restart-with dbus` and `--final-exit 3`, and waits for all three names. It exits 3 when
another process owns one of them. A kernel left with no hostname, which is what
`SetStaticHostname("")` and a missing `/etc/hostname` produce, is given the default name
(`DEFAULT_HOSTNAME=` in os-release, else `kdos`) when the daemon starts.

`KDOS_SETTINGSD_ETC`, `KDOS_SETTINGSD_SHARE` and `KDOS_SETTINGSD_SYS` replace `/etc`, `/usr/share`
and `/sys`, and the first also keeps the daemon off the machine it runs on: with it set the
kernel's hostname, the clock and the hardware clock are left alone. `testing/selftest.sh` drives
every method that way, over a private bus, against a polkit stand-in in
`testing/fixtures/settingsd/`.

## kdos-sysupdated

The bus face of a KDOS system update. Discover's stock SystemdSysupdate backend is how Plasma shows
an operating-system update, and this daemon serves `org.freedesktop.sysupdate1` the way Discover
6.8.2 calls it, so Discover drives a KDOS update with no patch. It does no update work itself:
every check, download, install and boot decision is a run of `kdos update` or `kdos-bootctl`,
executed with an argument vector, and the daemon waits for them without blocking the bus. It also
serves `io.kdos.Slots1` for the System Versions page and the update notifier, and it is the root
half of the boot confirmation that keeps a new slot. What an update is, where it comes from and how
the slots are chosen are in [Updating a machine](../03-architecture/packaging.md#updating-a-machine)
and [A/B slot selection](../03-architecture/boot-and-init.md#ab-slot-selection); how Discover
drives it is in [Discover](discover.md#system-updates).

### What Discover calls

| Call | Answers |
|---|---|
| `Peer.Ping` | Whether the backend is there at all |
| `Manager.ListTargets` | One target, `host`, at `/org/freedesktop/sysupdate1/target/host` |
| `Manager.ListJobs`, `ListAppStream` | The running job; the AppStream URL |
| `Target.GetAppStream` | `file:///run/kdos/sysupdate/io.kdos.system.metainfo.xml` |
| `Target.GetVersion` | The running version |
| `Target.CheckNew` | The version on offer, or empty for nothing |
| `Target.List`, `Describe` | The installed and offered versions |
| `Target.Acquire("", 0)` | All the long work: `kdos update apply --acquire --mount --progress 3`. Its reply is a job path, whose `Progress` Discover watches |
| `Target.Install("", 0)` | The short arm: `kdos update apply --arm --mount --progress 3` |
| `Target.Vacuum` | `0`: the slots are the history, and nothing is vacuumed |
| `Job.Cancel` | Stops the running child |
| `Manager.JobRemoved` | Sent once, when a job this daemon replied with ends |

**Acquire answers only after `kdos update` has said `verified` or `refused`.** Discover shows a
failed call's message in a banner and a failed job as nothing but "failed", so everything that can
refuse an update (a release index, binhost index or image manifest no trusted key signed, a file
the release index does not name, a package the binhost does not cover, a slot that cannot be
mounted) is learned before the reply, and only an update that can go
ahead becomes a job. `JobRemoved` is sent only for a job this daemon replied with, because Discover
ends whatever transaction it is waiting on at the first one it sees.

`Acquire` and `Install` take `""`, the offered version, which is what Discover sends, or that
version by name; any other version answers that it is not available. Only one job runs at a time,
and a call during the hourly check is refused until the check finishes, because the fetch holds the
update lock the apply would take. Every one of these is checked again once polkit has answered, since an
administrator's prompt can stay open for minutes: a call whose version is not the one offered or
acquired at that moment is refused, and nothing other than the version that was authorised is installed.

**A trial boot is offered nothing.** While this boot is the trial of the other slot, the slot an
update would fill is the confirmed way back, so `CheckNew` answers empty and `Acquire` is refused
until the trial is confirmed or rolled back. A machine with no second root slot is refused too,
with a reply that names `kdos update apply`, which updates it in place.

**The update document belongs to the root that wrote it.** `CheckNew` reads
`/var/lib/kdos/update.json` and, at most once an hour (`--refresh-after`), runs
`kdos update check --fetch` to fetch the signed tree and release index, and through that index the
binhost index and slot image manifest, so what it offers is only what the index named. The
document is in each
slot, so the first boot of a slot, or the boot a rollback returns to, finds one written before
that root was what it is now; a document whose `installed` is not the running version offers
nothing, and the daemon rewrites it at start with `kdos update check --out`, which downloads
nothing. The daemon watches `/var/lib/kdos` and regenerates the AppStream file whenever the
document changes, so a check typed at a shell reaches Discover too.

**The AppStream file lives on `/run`.** `/run/kdos/sysupdate/io.kdos.system.metainfo.xml` carries
the release notes and, in its text, whether the update is the signed system image or the release's
packages (the document's `lane`), the signer's fingerprint, the packages it changes and the size
still to download; the copy in `/usr/share/kdos/update/` is the fallback that names the system and offers
nothing. Discover asks `127.0.0.1:3129` for an update's size and shows 0 when nothing answers;
nothing listens there.

### io.kdos.Slots1

The System Versions page and `kdos-notice` read the slots through `io.kdos.Slots1`: the properties
`Configured`, `RunningSlot`, `ActiveSlot`, `TrySlot`, `TryAttempts`, `TryBootNext`, `Channel` and
`State`, the methods `ListSlots`, `Try`, `CancelTry`, `SetChannel` and `Bless`, and the signal
`SlotsChanged`. Its contract is the page's own `io.kdos.Slots1.xml`.

### Confirming a boot

A slot is confirmed only when two marks exist, tested together, both root's regular files in
`/run/kdos/bless/`:

| Mark | Written when |
|---|---|
| `userspace` | The end of `rcS`: every service started |
| `graphical` | A Plasma session answered for a minute (`Bless`, which `kdos-notice --bless` calls), or the login greeter stayed up for a minute on a machine nobody logs in to |

Then the daemon runs `kdos-bootctl mark-good`, once per boot, and writes `/run/kdos/bless/confirmed`
after it succeeds. Either mark alone confirms nothing: a userland that boots and a desktop that
crashes is the failure a trial exists to catch. The greeter counts only on a machine with no
autologin and before any user session has existed this boot, because a greeter that came back
after the session crashed must not confirm the slot that crashed it. The greeter is an
`sddm-greeter` and a `kwin_wayland` both running as SDDM's account, `sddm`, the uid
`/etc/passwd` gives it: the names alone are not enough, since any account can start two processes
called that. A machine whose `inittab`
starts no display manager has no graphical half to wait for, and the first mark is enough there.

### Who may call it

Root is answered without asking polkit, and the bus policy lets only root own the two names.

| Action | Methods | Active local session | Inactive and any other |
|---|---|---|---|
| `org.freedesktop.sysupdate1.check` | `CheckNew` | `yes` | `auth_admin` |
| `org.freedesktop.sysupdate1.update` | `Acquire`, `Install`, `Cancel` | `auth_admin_keep` | `auth_admin` |
| `org.freedesktop.sysupdate1.update-to-version` | `Acquire` and `Install` naming a version | `auth_admin` | `auth_admin` |
| `io.kdos.slots.try` | `Try`, `CancelTry` | `auth_admin_keep` | `auth_admin` |
| `io.kdos.slots.channel` | `SetChannel` | `auth_admin_keep` | `auth_admin` |
| `io.kdos.slots.bless` | `Bless`, never interactive | `yes` | `no` |

### Running and testing it

```text
kdos-sysupdated [--session] [--root DIR] [--proc DIR]
                [--bless-after SECONDS] [--tick SECONDS] [--refresh-after SECONDS]
```

| Option | Default | Effect |
|---|---|---|
| `--session` | off | Serve the session bus, which is where Discover looks with `DISCOVER_TEST_SYSUPDATE=1` |
| `--root DIR` | `/` | Read and write every file under a fixture root |
| `--proc DIR` | `/proc` | Read processes from another tree |
| `--bless-after` | 60 | Seconds the greeter must stay up to earn the graphical mark |
| `--tick` | 5 | Seconds between checks of the boot state |
| `--refresh-after` | 3600 | Seconds between fetches by `CheckNew`; `0` never fetches |

`--session --root` together is how the whole flow runs without root. Without `--session` the
daemon refuses to start unless it is root. It exits 1 when its bus goes away, for the supervisor to
restart it with the bus, 2 on bad usage, and 3 when another process owns one of its names.
`SIGHUP` rereads every file it serves from. The children's standard error is the daemon's, so a
failed update is read line by line in syslog under `kdos-sysupdated`.

`46_sysupdated.sh` starts it under `ksvc` with `--restart-with dbus` and `--final-exit 3` and waits
for `org.freedesktop.sysupdate1`. The interface is tested against Discover's own XML:
`tests/org.freedesktop.sysupdate1.xml` beside the source is discover-6.8.2's, byte for byte, and
`testing/selftest.sh` diffs the daemon's introspection against it and against the Slots1 contract.

## kdos-boxsock

`kdos-boxsock` is not a root daemon and has no socket in `/run`. It is installed in `/usr/bin`, runs
as the desktop user, one process per box launcher, and gives that launcher a Wayland socket of its
own that KWin tags, so the compositor always knows which box a window came from. `kdos-appbox` runs
it on every launch; the box's side of it is in [kdos-appbox](kdos-appbox.md#the-launch-path).

```sh
kdos-boxsock [-s confined|tagged] <box> [<launcher-id>]
```

What it does:

1. Checks the box name: 1 to 63 characters from `[A-Za-z0-9._-]`, the first a letter or a digit,
   so an option is never taken for a name. The launcher id is the box itself, the box plus one more
   component (one launcher of a box with several, `<box>.<stem>`), or the box less its last
   component (a throwaway box, `<launcher-id>.tw<n>`, whose windows are its application's); it
   defaults to the box. `-s` is `confined` unless given. `-h` or `--help` alone prints the usage and
   exits 0; bad usage exits 2.
2. Checks that the box's private runtime directory, `$XDG_RUNTIME_DIR/kdos/box/<box>`, exists and is
   this user's alone. `kdos-appbox` makes it; a box sees it as its whole `/run/user/<uid>`, so a
   socket bound anywhere else is one the box cannot reach.
3. Takes a lock on `$XDG_RUNTIME_DIR/kdos/box/.<box>.wl[.<stem>]@<display>.lock`, beside the private
   directory and not inside it, so nothing in the box can take it. The holder writes its pid and its
   engine into the lock. A second run for the same socket prints the holder's path when the holder
   serves the same state, or stops the holder and takes over when it serves the other one.
4. Binds `wl[.<stem>]@<display>` in the private directory, mode 0700, so only this user can
   connect.
5. Hands it to KWin through the Wayland security-context protocol, tagged with the engine
   `io.kdos.appbox+private` for a confined box or `io.kdos.appbox+tagged` for one with a way past
   the tag, the launcher id as the application id, and the box as the instance id.
6. Prints `ready <path>` on standard output, or `error <reason>` when it cannot tag, closes
   standard output so its caller reads to end of file, and stays alive holding the descriptor that
   keeps the tag valid. When the compositor exits, it removes the socket and exits too.

Every client that connects on that socket is tagged by KWin itself; the client never sees the tag,
so it cannot forge, choose or drop it. What the tag carries is read in three places:

- **The application id is the launcher id.** KWin replaces a tagged window's app id with the
  context's unless the window's own starts with it, and sets the window's class and desktop file
  name from the result. The launcher id is the file name of the launcher `genlaunchers` wrote, so
  the window shows that launcher's icon, lights its pinned task and matches its box's window rule.
- **The instance id is the box.** KWin's sandbox policy reads that box's `grant` key from
  `~/.config/kdos/boxes/<box>.conf`, and the window decoration's badge names it.
- **The engine is the confinement state.** `kdos-appbox` decides which boundary held for the box
  and the decoration shows it. An engine is fixed when a context is committed, so a holder serving
  one state is replaced by a run asking for the other, and the replacement is what the next client
  connects through.

`<display>` is taken from `$WAYLAND_DISPLAY`: its last path component, reduced to
`[A-Za-z0-9._-]` and cut to twelve characters, or `session` when nothing is left. It is in the path
because the listener belongs to the compositor this process connected to; a path keyed on the box
alone would send a later launch through a compositor that may already be gone. `kdos-appbox`
derives the same path from the same variables, so the two agree with nothing passed between them.

If the compositor does not offer the security-context protocol, `kdos-boxsock` answers
`error the compositor has no security-context-v1` rather than bind an untagged socket, and
`kdos-appbox` refuses to start the box with that line as the reason. An untagged socket would be
indistinguishable from the compositor's own.

It is a separate program for two reasons:

- The launcher replaces itself with the container command, so it cannot hold anything for the box's
  lifetime, and the tag lasts exactly as long as the descriptor stays open. Something has to
  outlive the launch.
- `kdos-appbox` links only `libkbase`, `libktui`, `libkcolor` and `libkxdg`, none of which speaks
  Wayland. Speaking Wayland would add a client library and generated protocol code to a program
  whose short dependency list is a deliberate property.

The port is `src/system/kdos-boxsock`, built in the `42_graphics` phase with `libwayland-client`.

## The other services ksvc runs

These root services come from upstream ports, and KDOS starts each from `/etc/init.d` under
`supervise` like its own daemons. None of them is started by D-Bus activation: each port ships no
activation file, so a caller that came before the name was owned is refused rather than handed a
second, unsupervised copy that would then hold the name the supervised one needs.

| Init script | Service | What it is for |
|---|---|---|
| `40_elogind.sh` | `elogind` | `org.freedesktop.login1`: every login registers here, KWin takes the seat through it, polkit asks it who is at the machine, and Plasma's Shut Down, Restart and Suspend are its calls. Configured by `/etc/elogind/logind.conf.d/50-kdos.conf` and `sleep.conf.d/50-kdos.conf`. Run with `SYSTEMD_REBOOT_TO_BOOT_LOADER_ENTRY=1`, so login1's `RebootToBootLoaderEntry` is the file `kdos-bootctl try` writes, `/run/systemd/reboot-to-boot-loader-entry`, and the logout screen can name the slot a restart boots |
| `41_polkitd.sh` | `polkitd` | The authorisation every bus daemon here asks. What KDOS adds to the defaults is `/etc/polkit-1/rules.d/50-kdos.rules` |
| `44_accounts.sh` | `accounts-daemon` | `org.freedesktop.Accounts`: the Users page, plasma-setup's first account, and the avatar the login screen shows |
| `48_iio-sensor-proxy.sh` | `iio-sensor-proxy` | `net.hadess.SensorProxy`, for screen rotation and ambient-light brightness. Started only where udev tagged a sensor it can drive |
| `55_tlp.sh` | `tlp` | Not supervised: it applies TLP's settings at boot and exits |
| `56_tlppd.sh` | `tlp-pd` | Power profiles for PowerDevil and the battery applet, as `org.freedesktop.UPower.PowerProfiles` and `net.hadess.PowerProfiles`; each switch runs `tlp power-saver\|balanced\|performance` |
| `65_turnstiled.sh` | `turnstiled` | Every login's runtime directory and every user's services under `ksvc`; the greeter cannot start without it |
| `85_crond.sh` | `crond` | Users' crontabs, from cronie. The system's own scheduled jobs are snooze's timers, and cronie reads no `/etc/crontab` |

`elogind`, `accounts-daemon`, `iio-sensor-proxy` and `tlp-pd` are restarted with the system bus
(`--restart-with dbus`). How each fits into the boot and the session is in
[Boot and init](../03-architecture/boot-and-init.md) and [The session](../03-architecture/session.md).

## utmpd and wtmpd

Two services that are not KDOS's own and not root keep the login records musl cannot: `utmpd`
holds who is logged in now and `wtmpd` appends the history to `/var/log/wtmp`. Both come from the
`utmps` port, each run as `utmps-utmpd` or `utmps-wtmpd` behind `s6-ipcserver` (from `s6`) as the
`utmp` account, and `06_utmps.sh` starts both under `ksvc` with the supervised names `utmpd` and
`wtmpd`. The service name for `service` and `/etc/service.disabled` is `utmps`, and disabling it
leaves `who`, `w`, `users` and `last` with nothing to report. What each socket and file holds, who
may write to them and why every reader is built against utmps is in
[Login records](../03-architecture/boot-and-init.md#login-records).

## Adding a root daemon

A new daemon should meet all of these. Where an existing one does not, the exception is stated in
its section: `kdos-powerd` and `kdos-settingsd` find `kdos-bootctl`, `service`, `loadkeys` and `iw`
through `PATH` (rule 8).

1. It runs in the foreground and is started by an `/etc/init.d` script through `supervise`, with no
   D-Bus activation file.
2. Its script skips with a printed reason, before supervision, when the machine cannot support it.
3. It owns exactly one socket in `/run`, mode 0666, or its names on the system bus with a bus
   policy that lets only root own them, and refuses to serve its real socket or the system bus
   unless it is root (or, like `kdos-energyd`, cannot start at all without root's access).
4. A socket authorises on the caller's credentials, root and `wheel`, by calling
   `kb_uid_allowed()`, never a copy of that test, and answers `err not permitted` otherwise; what it
   admits beyond that comes from elogind or polkit, never from a group. A bus method asks polkit
   about the caller's bus name, asynchronously, under an action id with its own `.policy` file.
5. No verb takes a path. Identifiers come from a list the daemon published.
6. A connection that never speaks, and a polkit prompt nobody answers, cannot stall anything the
   daemon does on a timer, a trigger or the bus.
7. It has a `--fixture` mode that decides and prints without acting, or, like `kdos-powerd`,
   `kdos-settingsd` and `kdos-sysupdated`, a way to run its rules against a scratch tree.
8. Every program it runs is named by absolute path and executed through an argument vector, never a
   shell.
9. It links only libraries whose every line you are willing to run as root: `libkbase`,
   `libkcolor`, `libkproc` and `basu` today, and no toolkit.
10. Its refusals are documented in this chapter, including the ones that look like limitations.

Where the pieces go is fixed as well. The source and its recipe (`kpkgbuild` and `build.sh`) are
`src/daemons/<name>/`, the area that holds the six root daemons above (`kdos-boxsock` is a session
program, under `src/system/`); the port is named under the `src-daemons` heading of
`script/phases/50_desktop/packages.txt`, the only phase whose `PORT_REPO` includes `src/daemons`;
and its init script is `fs/etc/init.d/<NN>_<service>.sh`, whose number places it after everything
it needs, since `rcS` starts the scripts in numeric order. The six here are 43, 46, 55, 56, 57 and
59: the bus daemons after `polkitd` (41), the others after the network.

## See also

- [How KDOS differs](../01-philosophy/how-kdos-differs.md#init-and-service-supervision): the init
  and supervision these daemons run under, set against other distributions
- [Architecture overview](../03-architecture/overview.md): where these daemons sit in the running
  system
- [Boot and init](../03-architecture/boot-and-init.md): how `rcS` starts them and `rcK` stops them
- [The security model](../03-architecture/security-model.md): the authorisation argument in full
- [Packaging](../03-architecture/packaging.md#updating-a-machine): what a system update is, which
  `kdos-sysupdated` drives
- [Packs and boxes](../03-architecture/packs-and-boxes.md): what the pack daemon implements
- [Discover](discover.md#system-updates): the system updates `kdos-sysupdated` serves
- [KDOS's settings pages](kdos-settings.md): the Firewall page over `io.kdos.Power1`, and the
  pages `kdos-settingsd` answers
- [kdos-appbox](kdos-appbox.md): the pack daemon's client and the launcher that starts
  `kdos-boxsock`
- [kinstall](kinstall.md): which groups the installed account ends up in
- [How KDOS is built](../05-developer/how-kdos-is-built.md#the-desktop-50_desktop): the build
  phase that compiles every daemon here
- [The ports catalogue](../06-reference/ports-catalogue.md#srcdaemons): the daemon ports and
  their versions
- [Filesystem and IPC](../06-reference/filesystem-and-ipc.md): every socket, bus name and verb in
  full
- [Glossary](../06-reference/glossary.md): box, graft, pack and the other terms used here

<!-- book-nav -->
---

*Part IV — Programs, chapter 26.* Previous: [25. kdos-appbox](kdos-appbox.md) · [Contents](../README.md) · Next: [27. kinstall](kinstall.md)
