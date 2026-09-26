# The daemons

This page covers the long-running helpers that let an ordinary desktop account do things only root
may do: suspend the machine, mount a USB stick, see which application is draining the battery, kill
a runaway program before the desktop freezes, and mount application packs. It also covers the
three privileged-adjacent pieces that sit beside them: the per-box Wayland socket, the portal
backend and the lock screen.

It is written for two readers:

- **An administrator** who wants to know what each daemon allows, who may ask it, which file
  configures it, and how to check why something was refused. Read [The shape they
  share](#the-shape-they-share), then the section for the daemon in question.
- **A contributor** changing a daemon or adding one. Read the whole page, then
  [Adding a root daemon](#adding-a-root-daemon).

Why the authorisation is shaped this way, and what it defends against, is in
[The security model](../03-architecture/security-model.md). Every socket path and verb in one list
is in [Filesystem and IPC](../06-reference/filesystem-and-ipc.md).

## At a glance

There are five root daemons. Each is one binary installed in `/usr/sbin`, started at boot by a
script in `/etc/init.d` and kept running by the `ksvc` service supervisor.

| Daemon | Socket | Command-line client | Other clients | Init script | Skipped at boot when |
|---|---|---|---|---|---|
| `kdos-powerd` | `/run/kdos-powerd.sock` | `kdos-power` | The panel's power items, the compositor's lid handler `kdos-lid` (which runs `kdos-power suspend` when the lid closes), `kdos-firewall`, `kdos-time`, `kdos-users`, `kdos theme` | `55_powerd.sh` | The binary is missing |
| `kdos-energyd` | `/run/kdos-energyd.sock` | `kdos-energy` | `kdos-res` (Energy and Boxes pages) | `56_energyd.sh` | No readable RAPL energy counter under `/sys/class/powercap` |
| `kdos-oomd` | `/run/kdos-oomd.sock` | — | None; `kdos doctor` checks the socket exists | `57_oomd.sh` | `/proc/pressure/memory` is not writable (the kernel has PSI off) |
| `kdos-mountd` | `/run/kdos-mountd.sock` | `kdos-mount` | `kdos-devices`, `kdos-disks`, `kdos-connect`, `kdos-mediad` | `58_mountd.sh` | The binary is missing |
| `kdos-packd` | `/run/kdos-packd.sock` | — | `kdos-appbox` (and `kdos app`, which runs it), `kdos doctor` | `59_packd.sh` | The kernel cannot mount EROFS, even after `modprobe erofs` |

`kdos-powerd`, `kdos-energyd` and `kdos-mountd` are each one binary with two names: run as the
daemon name it serves, run as the client name (`kdos-power`, `kdos-energy`, `kdos-mount`) it sends
one request and prints the answer.

To start, stop or inspect one by hand, use the service name, which is the script name without its
number and `.sh`:

```sh
service powerd status
service mountd stop
service packd start
```

Who may ask each daemon for what:

| Daemon | root | `wheel` (administrators) | `seat` (the desktop user) | Anyone else |
|---|---|---|---|---|
| `kdos-powerd` | Every verb | Every verb | `ping`, `suspend`, `poweroff`, `reboot` | Refused |
| `kdos-energyd` | Every verb | Every verb | Refused | Refused |
| `kdos-oomd` | Every verb | Every verb | Every verb | Refused |
| `kdos-mountd` | Every verb | Every verb | Every verb | Refused |
| `kdos-packd` | Every verb | Every verb | Refused | Refused |

`seat` is the group seatd hands the display to. The installer keeps the desktop account in `seat`
on every install, and takes a non-administrator out of `wheel` — see
[kinstall](kinstall.md#what-the-rest-of-the-tree-provides). Such an account keeps the lid, the
power keys and removable media, and loses sudo and every configuration verb.

## The shape they share

Every root daemon here is built the same way. Knowing the pattern once tells you how to run,
diagnose and review all five.

- **It runs in the foreground, under `ksvc`.** A daemon never forks into the background. The
  supervisor owns the process, writes its pid file and restarts it if it exits.
- **Its output goes to syslog and to a small file in `/run`.** The supervisor points the daemon's
  standard output and error, and its own *Starting* and *Exited* lines, at a forwarder process.
  Each line becomes a daemon-facility syslog message tagged with the daemon's full name, and is
  also appended to `/run/kdos-svc.<full name>.log` — for example `/run/kdos-svc.kdos-powerd.log`,
  tagged `kdos-powerd`. Files and tags use the full name; only the `service` command takes the
  short one (`powerd`). That file holds at most 64 KiB; when it would grow past that it is renamed
  to `/run/kdos-svc.<full name>.log.old` and a fresh one starts. The file is what you read before
  `syslogd` is up or while it restarts.

  The daemon never keeps the descriptors it inherited. Under the boot script `rcS` those point at
  the boot step's capture file on the `/run` tmpfs, so a daemon that crash-looped would fill memory
  until the next reboot, and a daemon that logs only to standard error (`chronyd -d`, for example)
  would never reach `/var/log/messages`. The forwarder ignores the SIGTERM that `service stop`
  sends the whole process group and exits only when the daemon's end of the pipe closes, so what a
  daemon prints while shutting down is kept and it never writes into a pipe with no reader.
- **It owns one socket in `/run`,** named after the daemon.
- **The socket is mode 0666; the real check is the caller's identity.** Anyone may connect. The
  daemon asks the kernel for the connecting process's user id (`SO_PEERCRED`, which the caller
  cannot forge) and answers `err not permitted` to anyone it does not admit. The file mode is left
  open on purpose: a mode that looked like the authorisation is a mode somebody eventually loosens.
- **There is one implementation of that check, `kb_uid_allowed()` in `libkbase`.** No daemon keeps
  its own copy. A copy per daemon is a rule that gets tightened on one socket and stays loose on the
  others, with nothing to show which is which. A daemon that needs a different rule states the
  difference beside its own call.
- **One request per connection.** The client connects, writes one short line, reads the answer and
  the connection closes. There is no session state. The single exception is a subscription (only
  `kdos-mountd` has one), which keeps its connection open and is written to when something changes.
  Even then the daemon remembers only a file descriptor. A daemon with a subscription polls its
  sockets rather than blocking in `accept()`, or one subscriber would keep every other client
  waiting forever.
- **The client never names a path.** Every verb takes an identifier from a list the daemon itself
  published: a row number, a pack id, a service name.
- **It has a fixture mode.** Each daemon can be pointed at a recorded copy of the system state and
  made to print what it would do without doing it. This is how the selection logic, which decides
  what gets mounted, formatted or killed, is tested without a machine to lose.
- **Its init script checks first and skips with a reason.** A daemon that cannot do its job on this
  machine (no energy counter, no PSI, no EROFS) is skipped and prints `[SKIP] <name>: <reason>` at
  boot, because a daemon that refuses to start under a respawn loop is a boot that never settles. A
  refusal only the daemon itself can detect is given to the supervisor as an exit status with
  `supervise --final-exit <code>`, on which it stops instead of restarting.
- **Its socket can be moved for testing.** Each daemon reads another socket path from an
  environment variable (`KDOS_POWERD_SOCKET`, `KDOS_ENERGYD_SOCKET`, `KDOS_OOMD_SOCKET`,
  `KDOS_MOUNTD_SOCKET`, `KDOS_PACKD_SOCKET`) so the test suite can run it unprivileged. Moving the
  socket grants nothing: the check is the caller's identity, never the path. `kdos-powerd`,
  `kdos-oomd`, `kdos-mountd` and `kdos-packd` refuse to start on their real socket unless they are
  root, because an unprivileged daemon would answer `ok` to work it cannot do. `kdos-energyd` has
  no such check; it refuses to start when it cannot read the energy counter, which without root it
  cannot.

## kdos-powerd

Suspend, power-off and reboot for a desktop that does not run as root, plus the few `/etc` writes
that belong to an administrator: the time zone, the autologin account, the firewall's open services
and the accent colour of the boot menu and text console.

| Verb | Who | Does |
|---|---|---|
| `ping` | root, `seat`, `wheel` | Answers `ok`. Also tells a client whether it would be allowed to suspend |
| `suspend` | root, `seat`, `wheel` | Suspend to RAM |
| `poweroff` | root, `seat`, `wheel` | Power off |
| `reboot` | root, `seat`, `wheel` | Reboot |
| `timezone <Area/City>` | root, `wheel` | Point `/etc/localtime` and `TZ` at a zone, and set the Wi-Fi country from it |
| `autologin <user>\|off` | root, `wheel` | Choose which account tty1 logs in without asking, or none |
| `firewall list` | root, `wheel` | One row per named service: name, `on` or `off`, and what it opens, tab-separated, then `ok` |
| `firewall <service> on\|off` | root, `wheel` | Open or close one named service |
| `accent <scheme>` | root, `wheel` | Recolour the boot menu, the text console and the boot splash |

The daemon checks the admin list the other way round: `seat` may use exactly the four words
`ping`, `suspend`, `poweroff` and `reboot`, and every other verb is an administrator's. A verb added
later is therefore an administrator's unless someone deliberately adds it to that list.

### The client

```
kdos-power [--no-lock] suspend|poweroff|reboot|ping
kdos-power timezone <Area/City>
kdos-power autologin <user>|off
kdos-power firewall list|<service> on|off
kdos-power accent <scheme>
```

| Exit status | Means |
|---|---|
| 0 | The daemon answered `ok`, or closed without answering a `suspend`, `poweroff` or `reboot` (the machine went away) |
| 1 | The daemon answered `err …`, or is not running (`kdos-power: no kdos-powerd — service kdos-powerd start`) |
| 2 | Bad usage, or an argument too long for the 64-byte request line |

For every verb except `firewall`, an error is printed to standard error as `kdos-power: <reply>`.
`firewall` prints the daemon's whole reply on standard output, the `err` line included, because
`kdos-firewall` reads it through a capture that discards standard error.

### Suspend locks the screen first

`kdos-power suspend` locks the session before the machine sleeps, so a resume never hands back an
unlocked desktop. The order is:

1. Send `ping`. This runs the same identity check as `suspend`, so a caller who is not allowed to
   suspend — or a machine with no `kdos-powerd` — is refused before the screen is locked. Locking
   and then not suspending would be a password prompt in exchange for nothing.
2. If no `kdos-lock` is already running, start one and wait up to two seconds for it to print
   `locked`, which it does once the compositor confirms the session is locked.
3. Send `suspend`. If the lock did not confirm in time, the client prints
   `kdos-power: no lock confirmation within 2s — suspending anyway` and suspends regardless: a
   broken lock screen must not turn the suspend key into a no-op.

`--no-lock`, or `KDOS_NO_LOCK_ON_SUSPEND=1` in the environment for a caller that cannot change its
own arguments, skips steps 1 and 2.

The daemon answers `ok` before the machine goes down, so the client is not left waiting for a reply
from a suspended kernel.

### What happens around a suspend

The daemon is the only thing on the machine that knows a suspend is happening — there is no logind
to announce one — so it tells the two programs that must act around it:

1. `sync`.
2. NetworkManager's `Sleep(true)`, through `dbus-send --system`.
3. `tlp suspend`.
4. Write `mem` to `/sys/power/state` (suspend to RAM; hibernation is not offered, because the
   initramfs sets up no resume device).
5. After waking: `tlp resume`, then NetworkManager's `Sleep(false)`.

Without step 2, NetworkManager wakes trusting a Wi-Fi association and DHCP lease that the time
asleep has ended. Without step 5's `tlp resume`, the radio states TLP saved, and the settings the
firmware resets across suspend, are not restored. Each hook is skipped when its program is not
installed, and none of them can stop the suspend.

### Power-off and reboot

`poweroff` and `reboot` ask init first: SIGUSR2 to process 1 for a power-off, SIGTERM for a reboot.
Init then runs its shutdown entries, and `/etc/init.d/rcK` stops every service — `kdos-powerd`
among them — before anything is unmounted. Only if the daemon is still alive sixty seconds later,
which means init ignored the signal, does it call `reboot(2)` directly. The wait is that long
because `rcK` stops every supervised service ahead of `55_powerd.sh` at a cost of about a second
each, and a shorter wait would cut the power mid-shutdown.

### accent

`accent <scheme>` takes one of the eight scheme names compiled into `libkcolor`: `phosphor`,
`amber`, `ice`, `bone`, `norton`, `borland`, `perfect` or `paper`. The name is matched by
`kcol_find()`; anything else is refused with `err not an accent`. There is no path to aim and
nothing to traverse, which is why this verb is safe to reach from a session.

It writes only the root-owned files:

- `/etc/kdos/accent`, which `rcS` reads to recolour the running boot splash;
- the boot menu (`limine.conf` on the ESP) and the text-console palette (`/etc/vtrgb`), by running
  `kdos-bootctl theme <scheme>`, which owns both files.

It recolours nothing on the desktop. That is `kdos theme`'s job, which runs as the user and touches
only the user's own files; `kdos theme` calls this verb for the parts that need root.

A machine with no writable ESP is not a failure. The live medium is read-only, and a machine may
have no `/boot/efi` at all. The verb then answers `ok <scheme> (boot menu unchanged)` and succeeds,
because refusing would make `kdos theme` look broken on the ISO, where everything else recolours
correctly.

### timezone

The time zone is set here rather than by a separate daemon because it is the same question: the
files are root's, the person changing them is the one administering the machine, and `wheel` is
already the answer to who that is.

A zone name is checked twice:

1. **As characters.** Letters, digits, `+`, `-`, `_` and `/` only; no dot at all; no leading,
   trailing or doubled slash; at most 64 characters. A zone is `Area/City` or `Area/Sub/City`, so a
   slash must be allowed — which makes `../../etc/shadow` look legal, and this rule is what stops
   it. Failure answers `err not a zone name`.
2. **As a file.** It must exist under `/usr/share/zoneinfo`. Failure answers `err no such zone`.

Then both halves are written:

- `/etc/localtime` becomes a symlink to the zone file. Programs that read the zoneinfo tree follow
  it.
- `/etc/profile.d/20-timezone.sh` exports `TZ=':/etc/localtime'`. musl reads `TZ`, and it wins
  where it is set — which is on every KDOS login. The colon form points musl at the same file, so
  the two can never name different rules. Writing only the symlink would leave `date` reporting the
  old zone in every shell that had already read the profile.

**The Wi-Fi country follows the zone.** The kernel's wireless layer starts in the "world"
regulatory domain, which keeps every 5 GHz DFS channel closed and caps transmit power until
something names a country — and some drivers never take one from an access point. The verb looks
the zone up in `zone.tab`, which maps each zone to exactly one country code, then:

- writes `options cfg80211 ieee80211_regdom=<CC>` to `/etc/modprobe.d/kdos-regdom.conf`, which
  takes effect from the next boot;
- runs `iw reg set <CC>` for this boot.

A zone with no country, such as `UTC`, removes the file. Neither step failing fails the verb: a
machine with no radio still has a time zone. The installer writes the same file from the zone
chosen during installation.

### autologin

`autologin <user>` sets the `autologin` key in `/etc/kdos/login.conf`, which decides whether tty1
logs straight into the desktop. `autologin off` turns it off.

- **The account must be one that can log in.** `kb_users()` in `libkbase` decides that; a name it
  would not list is refused with `err no such account`. Pointing autologin at, say, a service
  account with `nologin` would give a machine that boots to a login nobody can complete.
- **Off comments the line out** (`#autologin = kdos`) rather than emptying the value. The login
  program only hands `agetty --autologin` a non-empty name, so an empty value would read as a
  setting and behave as none; the commented line still carries a name for whoever turns it back on.
- **Only the key line changes.** A line counts as the key when it starts with `autologin`, or with
  one `#` then `autologin`, and contains `=`. Prose that merely mentions the key is left alone.
- **A file too large to rewrite safely is refused** (`err login.conf is too large`) rather than
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
| `caddy` | TCP 8443 | Caddy's shipped site (HTTPS) |
| `mosh` | UDP 60000–61000 | Incoming mosh sessions (needs `ssh` on as well) |
| `syncthing` | TCP 22000, UDP 22000, UDP 21027 | Syncthing sync and local discovery |

How a change is applied:

1. The daemon reads which names are on by looking for each rule's exact text in
   `/etc/nftables.d/50-kdos-services.nft`.
2. It rewrites that file **whole** from the names that are on. It never merges, because merging
   means parsing nftables syntax, and a parser that got it wrong would leave a port open that the
   surface showed as closed. Anything you write by hand belongs in another file under
   `/etc/nftables.d`, which the daemon never reads or touches.
3. It runs `nft --check -f /etc/nftables.conf`. If the ruleset would not load, it answers
   `err the ruleset would not load; nothing changed` and the previous rules stay in the kernel.
4. It runs `nft -f /etc/nftables.conf`. That file deletes and rebuilds only its own `inet filter`
   table, so netavark's NAT for rootful containers and NetworkManager's for a hotspot survive a
   toggle.

An unknown name answers `err no service called that`; a state other than `on` or `off` answers
`err a service is on or off`.

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
daemon's log. It needs no privilege and prints one line, for example:

```
alice: uid 1001, primary gid 1001, in seat only — permitted to suspend, power off and reboot; refused the configuration verbs
```

It exits 0 when the user is allowed anything and 1 when refused.

The write flags run a verb's own rules without the socket, which is the only way to test them:
the socket's check needs two different users to exercise. They grant nothing — the binary is
writing to an `/etc` the person running it could already write to. Three environment variables
point them at a scratch tree: `KDOS_POWERD_ETC` replaces `/etc` (and then skips `nft` and `iw`),
`KDOS_POWERD_ZONEDIR` replaces `/usr/share/zoneinfo`, and `KDOS_POWERD_SOCKET` moves the socket.

## kdos-energyd

Per-application energy attribution: which application is using the processor's power.

```
KDOS energy  —  2.1 h of samples, RAPL package-0

  firefox-esr (appbox app.firefox-esr)     75.5%  ███████████████   gpu 75.0%
  kdos-comp                                15.4%  ███               gpu 25.0%
  short-lived and exited processes          8.7%

  shares are of ATTRIBUTABLE energy — 57% of the package total; the rest is the idle floor
  idle floor 15.00 W, the lowest average power seen in 3 samples
```

The hard part of this on an ordinary Linux desktop is not the measurement, which is decades old. It
is identity: an application is dozens of processes in scattered groups, and nothing owns enough of
the system to name them. On KDOS every boxed application already runs in its own container with a
known name (the mapping is `/usr/share/kdos/alien-apps`), so that half comes free.

| Verb | Answers |
|---|---|
| `ping` | `ok` |
| `report` | The text report above |
| `report-json` | The same report as JSON |

```
kdos-energy [--json|ping]
```

`kdos-energy` prints the report (or JSON with `--json`) and exits 0; it exits 1 when the daemon is
not running or answered `err`, and 2 on bad usage. The Energy page in `kdos-res` shows the same
answer.

Answers go to root and `wheel` only: on a multi-user machine this list is what everyone else is
running.

**Figures are shares, never watt-hours.** The counter (Intel RAPL, read from
`/sys/class/powercap`) measures the processor package. It cannot see the screen — the largest
single draw on a laptop — nor the radio, the storage, or a discrete graphics card. "This
application was 41% of attributable CPU energy today" is a measurement; "this application used 12%
of your battery" would be a guess wearing a unit.

The daemon samples every ten seconds. The interval is fixed, not configurable, and the six rules
below each change the answer:

- **Nested domains are dropped.** The power interface lists a package beside that package's own
  sub-domains, so summing the list counts the cores twice (on a test fixture, 15 W becomes
  26.25 W). A domain inside another's directory is a sub-domain and is skipped. The platform-wide
  domain goes the other way: it contains the packages, so where it exists it replaces them.
- **The counter wraps**, roughly every half hour at a laptop's power draw. A naive subtraction would
  produce one enormous negative reading with nothing in the output saying so; the daemon handles
  the wrap.
- **The idle floor is subtracted before anything is attributed.** A processor burns power with
  nothing running, and a share model that ignored this would report a machine sitting at a login
  prompt as 90% one process. The floor is the lowest average power seen, and it is printed with the
  answer.
- **The floor is applied when the report is made, not per sample window.** The floor can only fall,
  so charging each window the floor as it stood then would throw away the first window — usually
  the busiest, because something was just launched. Each application carries weighted sums and the
  report subtracts once, with the floor as it finally stands.
- **The denominator is the whole system's energy, not the sum of surviving processes.** A build that
  starts and exits inside one window is gone by the next sample, and dividing by the survivors would
  hand its energy to them. That difference gets its own line: *short-lived and exited processes*.
- **The graphics column is engine time, never energy.** Nothing on the machine says what GPU time
  cost in joules. On integrated graphics it is already inside the package figure; on a discrete
  card it is outside the counter entirely, and the report says so. A driver that publishes no
  statistics gets no column rather than a column of zeroes.

**Why a daemon rather than a one-shot command.** The counter runs freely, so a one-shot command
could only report what happened while it was watching. The counter is also readable only by root,
because a side-channel attack (PLATYPUS) showed that fine-grained unprivileged reads can recover
cryptographic keys. What leaves this daemon is a per-application percentage over minutes; the raw
counter and the sampling interval are never republished, and no client can ask for a shorter
interval. There is no write path into the power interface at all.

**Skipped** when no `/sys/class/powercap/*/energy_uj` is readable — most virtual machines and every
non-x86 processor. The daemon itself also refuses to start there, because a daemon sampling an
unreadable counter would report a machine that uses no energy, which reads as "nothing is draining
the battery".

`kdos-energyd --fixture <dir> [--json]` replays recorded snapshots through the same sampler and
report and prints the result. `KDOS_ENERGY_PROC` and `KDOS_ENERGY_POWERCAP` point the reader at
other `/proc` and powercap trees, and `KDOS_ALIEN_APPS` at another application-name table.

## kdos-oomd

Kills the process that is starving the machine of memory, before the desktop freezes.

The kernel has its own out-of-memory killer, but it fires when an *allocation fails*. On a machine
with swap that is often minutes after the desktop stopped responding — the whole session spent
thrashing while the kernel technically still had pages. The kernel's pressure interface (PSI, in
`/proc/pressure/memory`) says instead that the machine is *stalling* on memory, which is what a
frozen desktop feels like. Boxed applications make this likely: a browser and a 3D slicer on one
modest machine.

How it decides:

- **It waits on the kernel; it does not poll.** It writes the trigger `full 150000 1000000` into the
  pressure file — 150 ms of full stall within a one-second window, meaning every runnable thread was
  stuck on memory for 15% of the last second — and sleeps until the kernel signals it. A sampling
  loop would itself compete for processor time with the stall it is trying to notice.
- **The desktop is never a victim.** Process 1, kernel threads, anything whose `oom_score_adj` is
  -500 or lower (the opt-out other user-space killers honour too), and these by name are protected: `kdos-comp`, `kdos-shell`, `kdos-desk`,
  `kdos-notifyd`, `Xwayland`, `dbus-daemon`, `seatd`, `ksvc`, `kdos-powerd`, `kdos-energyd`,
  `kdos-oomd`, `wireplumber`, and every process whose name starts with `pipewire`. Killing the
  compositor to save the desktop is not a trade, and killing the audio session manager leaves a
  machine whose sound stopped for no visible reason.
- **Boxed processes are preferred victims.** A boxed application is the likely culprit, is
  supervised, and relaunches in seconds, while a host process is more often session state. Among
  the eligible, the largest by resident memory goes.
- **A box over its declared memory budget goes first,** ahead of the general rule. That is what
  makes a box profile's `memory` key mean something, since a rootless container on a machine with
  no cgroup delegation accepts a limit and ignores it. Budgets come from each user's own box
  profiles, `/home/<user>/.config/kdos/boxes/<box>.conf`; `KDOS_BOX_PROFILES`, when set, replaces
  that search with one directory (the test suite uses it).
- **The victim is named by its box.** Identity comes from the same container-supervisor walk that
  `kdos-res` and `kdos-energyd` use.
- **The memory comes back immediately.** The daemon takes a handle on the process first (so the
  next step cannot land on a recycled process id), sends SIGKILL, then calls `process_mrelease` to
  free the victim's memory at once rather than whenever it is reaped. On a kernel without that call
  the release is skipped and the kill stands.
- **At most one kill every ten seconds.**

Nothing in the protocol names a process, so there is nothing to aim. The socket takes no argument
and answers root, `seat` and `wheel`:

| Verb | Answers |
|---|---|
| `ping` | `ok` |
| `status` | `ok trigger 'full 150000 1000000', kills <n>` and, after the first kill, `, last: <name> (pid <pid>, <size> MB)` |

`kdos doctor` reports whether its socket is present.

`kdos-oomd --fixture <dir>` reads a recorded `/proc` tree and prints who would be killed, signalling
nobody.

`testing/oomd-fire.sh` exercises the real path in a booted virtual machine: it allocates memory
until the trigger fires, then asks the socket what happened rather than watching the allocating
process die (the kernel's own killer would also have got it, later). A passing run shows `status`
going from `kills 0` to something like `kills 1, last: python3 (pid 1615, 3644 MB)` while the
script that started the allocation keeps running. That run has one large process to choose; which
process the daemon picks on a busy desktop is what `--fixture` tests.

## kdos-mountd

Removable media, encrypted volumes, SMART health and network shares for a desktop that is not root.
There is no general-purpose disk service (no udisks) on KDOS, so this daemon is the whole of what
stands between the desktop and `mount`.

### Verbs

A **row** below is a number from the daemon's own `list`; a **share row** is a number from
`shares`. `<count>` is the byte length of a second frame, described under [The request
format](#the-request-format).

| Request | Does |
|---|---|
| `list` | The eligible devices, one row each — index, kernel name, label, filesystem, size in GiB, mountpoint, tab-separated, with `-` for an empty label or mountpoint — then `ok` |
| `mount <row>` | Mount the device; answers `ok <mountpoint>` |
| `unmount <row>` | Unmount it |
| `eject <row>` | Power the medium down. An optical disc ejects its own drive; a USB stick ejects its **parent disk**, because a stop command sent to one partition means nothing to the hardware |
| `unlock <row> <count>` | Open a LUKS volume. The passphrase is the second frame |
| `close <row>` | Close the mapping `unlock` made |
| `format <row> <fs> <count>` | Write a filesystem. **Off unless `format = yes`.** The second frame is the device's kernel name, typed by the person |
| `smart <row>` | The drive's model, serial and health, tab-separated |
| `cifs <server> <share> <user> <domain> <count>` | Mount an SMB share. The password is the second frame |
| `krb5 <server> <share> <user\|-> <domain\|->` | Mount an SMB share with the Kerberos ticket `kinit` left in the caller's credential cache. **No second frame** |
| `shares` | The mounted network shares, one row each |
| `browse` | Machines that answered an mDNS and a NetBIOS broadcast just now, as `name<TAB>address` |
| `disconnect <share row>` | Unmount a network share |
| `subscribe` | Answer `ok`, then write `changed` whenever the device list moves, and never close |
| `ping` | `ok` |

Every verb answers root, `seat` and `wheel`: mounting a stick is the desktop user's job whether or
not they administer the machine.

The client never names a path or a mountpoint. It asks for a row out of a list the daemon
published, and the daemon decides the device, the mountpoint and the options. Any design that
takes "a path and a mountpoint" ends with someone mounting a stick over `/etc` from a shell.

### The command-line client

```
kdos-mount list
kdos-mount mount <index>
kdos-mount unmount <index>
kdos-mount smart <index>
kdos-mount shares
kdos-mount browse
kdos-mount krb5 <server> <share> <user|-> <domain|->
kdos-mount ping
kdos-mount subscribe
```

For example:

```
$ kdos-mount list
0	sdb1	KDOS	vfat	28.7G	-
1	sdb2	backup	ext4	120.0G	/media/kdos/backup
ok
$ kdos-mount mount 0
ok /media/kdos/KDOS
```

The client prints the daemon's reply unchanged: fields are separated by tabs, `-` stands for an
empty label or mountpoint, and every answer ends in an `ok` or `err …` line.

| Exit status | Means |
|---|---|
| 0 | The daemon answered — **including when the answer was `err …`**. A script must check standard output for a leading `err` to tell a failed mount |
| 2 | No `kdos-mountd` to ask (`kdos-mount: no kdos-mountd on <socket path> (<reason>)`), or bad usage |

The index is re-rendered as a number before it is sent, whatever the argument held. The verbs that
carry a secret (`unlock`, `format`, `cifs`), and `eject`, `close` and `disconnect`, are reached
from the desktop's surfaces rather than from `kdos-mount`:

| Surface | Uses |
|---|---|
| `kdos-devices` | `list`, `mount`, `unmount` |
| `kdos-disks` | `list`, `mount`, `unmount`, `unlock`, `close`, `format` (always as `ext4`), `smart` |
| `kdos-connect` | `cifs`, `krb5`, `shares`, `browse`, `disconnect` |
| `kdos-mediad` | `subscribe`, `list`, `mount`, `eject` |

### The request format

A request is one line, plus a second frame where a secret is involved.

- **Frame one** is a verb and up to five tokens, separated by spaces, ending in a newline. The line
  may be up to 1,024 bytes. That ceiling is set by `cifs` alone: a DNS name may be 253 bytes, a
  share 80, a username 104 and an NT domain 255, so a legal corporate share spells a request of
  about seven hundred.
- **Frame two** is exactly the number of bytes frame one's last token declared, from 1 to 512. A
  passphrase travels as a frame and not a token because the tokeniser splits on spaces and a
  passphrase may contain them.

Every token is checked before it means anything, and the token count is fixed per verb:

- A row is one to three digits and must be inside the list just published.
- A count is one to four digits and between 1 and 512.
- A trailing token nobody asked for makes the request unknown (`err unknown command`) rather than
  ignored. A dispatcher that read an index and discarded the rest would accept
  `mount 0 rm -rf /` as a well-formed mount.
- A line with eight or more tokens is refused as `err too many arguments`.

The device list is rescanned on every request rather than cached, so a stick pulled out between two
requests is never still offered.

### Which devices are offered

A device appears in `list` only if every one of these holds:

| Rule | Why |
|---|---|
| It is removable, or it is on USB | An internal disk is the administrator's. An external drive in a USB enclosure reports itself as non-removable, so the bus is checked too |
| It carries a filesystem this kernel can mount | Checked against `/proc/filesystems` **before** the mount call, so a kernel without the driver refuses up front instead of failing halfway |
| It is not named in `/etc/fstab` | An entry there is a decision somebody already made |
| It is not the medium this system booted from | Offering to unmount the live medium is offering to kill the session |

The label and filesystem type are read directly from the superblock rather than through a
detection library. The daemon recognises LUKS (checked first, because a LUKS header written over an
old filesystem still carries that filesystem's superblock), ext2/3/4, FAT, NTFS, exFAT, ISO 9660 and
btrfs — the formats sticks, cards and discs actually use.

**Mount options.** `nosuid` and `nodev` always; `noexec` unless `exec = yes` is set in
`/etc/kdos/mountd.conf`. A setuid-root binary on somebody else's stick is a local root hole, so
executing from removable media is something you opt into.

**The mountpoint** is `/media/<user>/<label>`, created mode 0700 and owned by the caller, and
removed on unmount. The label is sanitised to the characters `A–Z a–z 0–9 . _ -` before it becomes
a path component, because it is whatever somebody else's computer wrote into a superblock.

### /etc/kdos/mountd.conf

The image ships no `mountd.conf`; create it to change a default. The daemon reads it on each
request that needs it.

| Line | Default when absent | Effect |
|---|---|---|
| `exec = yes` | `noexec` | Removable media are mounted without `noexec`, so programs on them can run |
| `format = yes` | `format` refused | The `format` verb is allowed |

The daemon searches the whole file for the text `exec = yes` or `exec=yes` (and `format = yes` or
`format=yes`) anywhere, comments included. A commented-out `# exec = yes` therefore still turns the
setting on. To turn a setting off, delete the text rather than commenting it out.

### Encrypted volumes

The passphrase reaches `cryptsetup` on standard input, through `--key-file=-`, and never in an
argument list: `/proc/<pid>/cmdline` is readable by every user for the life of the process. One
buffer holds it, and every way out of the request wipes it.

The mapping name is the daemon's: `kdos-<kname>`, derived from the row. A client cannot ask for a
mapping called anything else, and `close` finds the same name from the same row without being told
it.

After an `unlock`, the opened mapping appears in `list` beside the container it came from, so the
filesystem inside can be mounted like any other row. The daemon finds it by the name it chose
itself rather than by walking `/sys/block/dm-*`; a mapping this daemon did not open — someone's own
`cryptsetup open` of a root volume, for instance — is not offered. The row keeps the container's
physical disk, so every destructive verb is still judged by that disk.

### SMART

`smart` answers for the whole drive, not the partition: SMART is a property of the drive, so a row
per partition would repeat one answer and point a raw-device tool at an offset nothing owns.

`smartctl` needs the raw block device, which no session may open. The alternatives — a setuid
binary or a sudo rule — are both wider holes than one daemon answering one question. The output of
`smartctl -a` (two hundred lines of vendor attributes) is filtered down to the model, the serial
and whether the drive says it is failing. Its exit status is a bitfield rather than a failure flag:
bits 3 to 7 mean the drive is unwell, which is often the answer rather than the absence of one, so
only an empty capture is treated as "nothing learnt".

### Network shares

**`cifs`** mounts an SMB share with a password. `mount(2)` cannot open an SMB session by itself —
dialect negotiation, authentication and the tree connect all happen inside `mount.cifs` — so this
verb runs that helper.

- **Each of the four names is checked against its own character allowlist.** `mount.cifs` builds
  its option string by joining fields and escapes nothing but the password, so a comma in the
  server, share, username or domain would become a new mount option, and a `/` or `\` in a server
  would silently re-aim the mount. What is not on the list is refused rather than quoted.
- **The password reaches the helper on a file descriptor** (`PASSWD_FD=0`, with the bytes on the
  child's standard input). The other ways `mount.cifs` accepts one are worse: an option string is
  visible in the process list, an environment value in `/proc/<pid>/environ`, and a file is a file
  somebody has to delete.
- **The `cifs` module is loaded first.** Nothing else on the image loads it, and
  `/proc/filesystems` lists only what the kernel already has, so checking support before `modprobe`
  would refuse every first connection.
- **The mount belongs to the caller.** `uid=`, `gid=`, `file_mode=` and `dir_mode=` are always
  given, because a server without Unix extensions reports every file as owned by root. `nosuid` and
  `nodev` always; `noexec` unless `exec = yes`.
- **Names musl cannot resolve are resolved here.** `nsswitch.conf` has no effect on musl and there
  is no winbind, so a `.local` name and a bare NetBIOS name are the two shapes `getaddrinfo` never
  answers. The normal resolver is tried first (a name in `/etc/hosts` is one somebody wrote down,
  and a broadcast must not override it); then a `.local` name goes to `avahi-resolve-host-name` and
  a bare name to `nmblookup`. An address or a dotted DNS name never triggers a broadcast. Only the
  address is substituted: the share mounts under the name that was typed, with the address passed
  in `ip=`, so the mountpoint is one a person recognises.

**`shares`** lists what `/proc/mounts` says is connected. No list is held between requests, so a
server that went away, or a share another session mounted, is reported as it is now.

**`browse`** is two broadcasts, not a directory. There is no browse master to ask (samba here is
built without winbind and without a domain controller), so the list is `avahi-browse -ptrk
_smb._tcp` for machines that advertise the service, plus `nmblookup -S -- '*'` for machines that
answer a NetBIOS query. Only a machine with a `<20>` (file server) entry is offered, since one
without it is sharing nothing. A browse row is not an index: `cifs` and `krb5` take a server name.

**`krb5`** mounts with `sec=krb5`, using the ticket already in the caller's credential cache. No
secret crosses the socket: the kernel's cifs module raises a `cifs.spnego` key request,
`request-key` runs `cifs.upcall` against that cache, and the helper hands back the SPNEGO blob. That
is why it is a verb of its own rather than `cifs` with an empty password.

- The mount passes `cruid=<caller>`. Without it the upcall would look in root's credential cache,
  which is empty, and the mount would fail with `Required key not available`, naming no user.
- `-` means "no username" and "no domain", and for Kerberos that is the normal case: the principal
  in the ticket already says who you are.
- `cifs.upcall` and `/etc/request-key.d/cifs.spnego.conf` are checked before the module is loaded
  and before the mountpoint is made. Without them the kernel's error is that same unhelpful
  `Required key not available`, so the daemon names the missing file instead and leaves no empty
  directory under `/media`.

### What a destructive verb refuses

**The boot medium is refused by the disk, not by the partition.** A live USB carries an ISO 9660
partition and a FAT EFI partition beside it. Every per-partition rule would offer the EFI partition
— it is removable, it is FAT, it is unmounted and no fstab names it — and formatting it would
destroy the running session. In a live session any disk carrying an ISO 9660 partition is treated
as the boot disk, whole.

**`format` demands the device's kernel name, typed.** Not a flag and not the word yes: the daemon
compares the second frame with the name it put in the list itself, by exact length and content, so
a client cannot send a confirmation it was never shown.

**`format` is opt-in** (`format = yes` in `/etc/kdos/mountd.conf`), and the filesystem is one of
four, checked against a table:

| Filesystem | Command |
|---|---|
| `ext4` | `/usr/sbin/mkfs.ext4 -F -L <label>` |
| `btrfs` | `/usr/bin/mkfs.btrfs -f -L <label>` |
| `vfat` | `/usr/sbin/mkfs.vfat -I -n <label>` |
| `exfat` | `/usr/sbin/mkfs.exfat -n <label>` |

**The device node is checked again at the moment of use.** Between the scan that built the row and
the call that acts on it, a path could become a symlink or a different device. The daemon opens it
with `O_NOFOLLOW` and requires a block device whose device number matches the one `/sys` recorded.

**Every program it runs is named by absolute path** — `eject`, `cryptsetup`, `mkfs.*`, `modprobe`,
`mount.cifs` — and all go through one function. Otherwise a root process would look programs up
through an inherited `PATH`.

### How the desktop hears about a new device

`kdos-mediad` is the subscriber. It runs in the session beside `kdos-notifyd`:

1. `kdos-mediad` sends `subscribe` and keeps the connection open.
2. The daemon listens to the kernel's own hotplug broadcast (`NETLINK_KOBJECT_UEVENT`), so hotplug
   works with no udev rule file and without udev running. `ACTION=change` is included, because that
   is what a drive reports when a disc goes into a tray that was already there.
3. When the device list moves, the daemon writes `changed` to every subscriber. It cannot say
   *which* device, because a row number is only true of the list it came with, so the subscriber
   asks again with `list` and compares.
4. `kdos-mediad` raises a notification with **Open** and **Eject** buttons, and asks for the list
   again when a button is clicked rather than trusting the row the notification was built from.

The daemon only says that something moved; the session decides what it means. The daemon is root,
starts before anybody logs in, and has no session bus to send a notification on. At most four
subscribers are held at once; a fifth is answered `err too many subscribers`.

The panel does not subscribe. A socket round trip per panel frame is exactly what "nothing blocks
the frame" rules out; `kdos-devices` asks when it is opened.

### Fixtures

```sh
kdos-mountd --fixture <sys> [dev]
kdos-mountd --fixture-serve <sys> [dev]
```

`--fixture` prints what the daemon would offer from a recorded `/sys` tree (and optionally a `/dev`
tree), one row per device and a final `<n> eligible`, and mounts nothing. `--fixture-serve` runs the
real request handling over a real socket (`KDOS_MOUNTD_SOCKET`) with the fixture's trees, admits
any caller, and prints each command it would run instead of running it. That is how a `format`
aimed at the boot medium is proved to be refused without a disk to lose.

Only in these two modes does the daemon read the path overrides `KDOS_MOUNTD_SYS`,
`KDOS_MOUNTD_DEV`, `KDOS_MOUNTD_MOUNTS`, `KDOS_MOUNTD_FSTAB`, `KDOS_MOUNTD_MEDIA`,
`KDOS_MOUNTD_CONF` and `KDOS_MOUNTD_UEVENT` (a FIFO standing in for the kernel's hotplug socket).
The daemon started by the init script reads none of them: an environment variable that moved its
idea of `/dev` would be a way to aim a format at any device on the machine.

The committed fixture is a recorded block-device tree plus two hand-built superblocks: a removable
device that must be offered and an internal one that must not. The internal disk carries a real
superblock so that a broken removable check shows up as an extra row rather than as nothing.

## kdos-packd

The only program on the system that mounts an application pack. A pack is a signed, read-only EROFS
image holding an application, a runtime it depends on, or data; how packs are built, verified and
combined into a box is in [Packs and boxes](../03-architecture/packs-and-boxes.md).

| Verb | Does |
|---|---|
| `list` | Every pack the machine can see: id, version, kind, state (`mounted`, `installed` or `available`), size, and origin (`store` or `medium`), tab-separated |
| `info <id>` | One pack's metadata |
| `mount <id>`, `unmount <id>` | Mount or release a pack |
| `compose <box> <id>…` | Build a box's overlay stack from one or more packs |
| `decompose <box>` | Tear it down |
| `install <file>` | Move a pack from the staging directory into the store |
| `remove <id>` | Take a pack out of the store |
| `rollback <id>` | Return to a retained earlier version |
| `graft <id>`, `ungraft <id>` | Place or remove a data pack's contents |
| `ping` | `ok` |
| `status` | The store, medium and staging directories, the retention count, the mount route, pack counts, and each composed box |

Every verb answers root and `wheel` only. `kdos-appbox` is the client; `kdos app install`,
`remove` and `rollback` go through it.

Paths:

| Path | What |
|---|---|
| `/var/lib/kdos/packs` | The store |
| `/var/lib/kdos/packs/staging` | Where an unprivileged download lands; mode 01777 |
| `/var/lib/kdos/packs/mnt` | Mountpoints |
| `/mnt/iso/packs` | Packs on the boot medium |
| `/var/lib/kdos/pack-manifest` | Every graft made, so an ungraft removes exactly what was added |
| `/etc/kdos/keys/packs` | The keys packs are verified against — separate from `/etc/kdos/keys`, which is `kpkg`'s package-repository ring |
| `/etc/kdos/packd.conf` | Configuration |

Unlike `kdos-mountd`'s overrides, the daemon reads these environment variables in normal operation,
not only in fixture mode:

| Variable | Effect |
|---|---|
| `KDOS_PACK_STORE`, `KDOS_PACK_MEDIUM`, `KDOS_PACK_MANIFEST` | Replace the store, the boot-medium pack directory and the graft record |
| `KDOS_PACK_RETAIN` | Overrides `retain` in `packd.conf` (below) |
| `KDOS_REQUIRE_SIG` | Refuse every unsigned pack (`<id> is unsigned and KDOS_REQUIRE_SIG is set`), for a machine that installs only what it can attribute |

They are also listed in [Filesystem and IPC](../06-reference/filesystem-and-ipc.md#environment-variables).

**The client never names a path, with one exception:** `install` takes a file name inside the
staging directory. A name containing `/`, the name `..`, or an empty name is refused. `status`
publishes the staging directory and the retention count, so a program writing a download into the
store does not have to work either out for itself; a second definition of where an unprivileged
write is allowed is exactly the kind of thing that drifts.

The list is rescanned on every request, so a medium pulled out between two requests is not still
offered.

**An install replaces the old version safely.** If the old version is mounted but not in use, it is
unmounted before the file is swapped, so the next compose reads the new file. If it is composed
into a running box, the install is refused (`<id> is composed into <n> box(es)`), the same rule
`remove` applies. Without this, a box composed after an update would put the new application's
layer over the old runtime's still-mounted files, and the application would fail on a library only
the new runtime carries.

**Retention is what makes rollback possible.** `/etc/kdos/packd.conf` sets how many superseded
versions of each pack the store keeps:

```
retain = 1
```

| Value | Effect |
|---|---|
| `1` (shipped) | The update you just took can be undone |
| `0` | Nothing is kept; `rollback` answers "no earlier version is kept" rather than failing |
| Up to `8` | That many; higher values are treated as 8 |

The sweep that deletes older versions runs after an install and at no other time, so nothing deletes
a rollback while somebody is deciding whether to use it. `KDOS_PACK_RETAIN` in the daemon's
environment overrides the file.

A socket path too long for the socket address structure is refused rather than truncated.
Truncating would bind a socket at a path nobody asked for, and the next start would fail with
"address already in use" for a file that appears not to exist.

`kdos-packd --fixture <store> [medium]` lists the packs in a scratch store with their signature
state, composes each application pack, grafts each data pack, and prints the result, mounting
nothing. `KDOS_PACKD_VERBOSE` shows its log lines; `KDOS_KEYS` points it at another key directory.

The verification rules, the two mount routes, reference counting and adoption of existing mounts at
startup are in [Packs and boxes](../03-architecture/packs-and-boxes.md).

## kdos-boxsock

Not a root daemon and not in `/run`. `kdos-boxsock` (installed in `/usr/bin`) runs as the desktop
user, one process per box, and gives that box its own Wayland socket so the compositor always
knows which box a window came from.

```
kdos-boxsock <box> [instance-id]
```

What it does:

1. Takes a lock on `$XDG_RUNTIME_DIR/kdos-box-<box>@<display>.lock`. If another `kdos-boxsock` for
   the same box and compositor already holds it, this one exits 0 and does nothing.
2. Binds `$XDG_RUNTIME_DIR/kdos-box-<box>@<display>.sock`.
3. Hands it to the compositor through the Wayland security-context protocol, tagged with the engine
   name `io.kdos.appbox`, the box name and the instance id (the box name when no instance is
   given).
4. Stays alive holding the descriptor that keeps the tag valid.

Every client that connects on that socket is tagged by the compositor itself; the client never
sees the tag, so it cannot forge, choose or drop it. That tag is what the compositor's sandbox
filter reads, what the box chip on a title bar shows, and what lets the panel say which box a
window came from.

`<display>` is taken from `$WAYLAND_DISPLAY` (at most twelve characters, `session` when unset). It
is in the path because the listener belongs to the compositor this process connected to; a path
keyed on the box alone would send a later launch through a compositor that may already be gone.
`kdos-appbox` derives the same path from the same variables, so the two agree with nothing passed
between them.

It is a separate program for two reasons:

- The launcher replaces itself with the container command, so it cannot hold anything for the box's
  lifetime, and the sandbox lasts exactly as long as the descriptor stays open. Something has to
  outlive the launch.
- `kdos-appbox` links only `libkbase`, `libktui`, `libkcolor` and `libkxdg`, none of which speaks
  Wayland. Speaking Wayland would add a
  client library and generated protocol code to a program whose short dependency list is a
  deliberate property.

## xdg-desktop-portal-kdos

The portal backend, installed as `/usr/lib/xdg-desktop-portal-kdos` and started on demand over the
session bus. It answers the file chooser, settings and application chooser, and the access
question that Camera, Screenshot and Location ask through `kdos-prompt`. It is covered in
[The session](../03-architecture/session.md#the-kdos-backend), including the two rules that matter
most: every request is answered, and the bus loop never blocks on a dialog.

## kdos-lock

The lock screen, in `/usr/bin`. It is not a root daemon, but it is the other long-lived program
close to privilege. It covers every output with a lock surface and checks the password by running
`kdos-checkpass`.

`kdos-checkpass` is the only setuid piece: `/etc/shadow` is readable only by root, and the lock
screen must not run as root. It takes no arguments, checks the password of the user who ran it (the
real user id, never a name it was given), and reads the password on standard input, because
argument lists are visible to every user.

| `kdos-checkpass` exit | Means |
|---|---|
| 0 | Correct |
| 1 | Wrong |
| 2 | Could not tell: no such user, no readable shadow entry, an account with no password or a locked one (an empty, `!` or `*` hash), or an argument was given |

Each wrong answer takes one second before `kdos-checkpass` exits, which limits how fast a password
can be guessed. An account with no password, or a locked one, cannot unlock the lock screen at all.

Once the compositor confirms the session is locked, `kdos-lock` prints `locked` on standard output;
`kdos-power suspend` waits for that line.

The compositor, not the lock program, owns the locked state. If the lock program crashes, the
screens stay covered and a new lock client may take over. See
[kdos-comp](kdos-comp.md#idle-dim-lock-and-lid).

## Adding a root daemon

A new daemon fits the family when all of these are true:

1. It runs in the foreground and is started by an `/etc/init.d` script under `ksvc`.
2. Its script skips with a printed reason, before supervision, when the machine cannot support it.
3. It owns exactly one socket in `/run`, mode 0666, and refuses to serve that socket unless it is
   root (or, like `kdos-energyd`, cannot start at all without root's access).
4. It authorises on the caller's credentials — root and `wheel`, plus `seat` for what the person at
   the machine needs without administering it — by calling `kb_uid_allowed()`, never a copy of that
   test, and answers `err not permitted` otherwise.
5. No verb takes a path. Identifiers come from a list the daemon published.
6. It has a `--fixture` mode that decides and prints without acting.
7. It links only libraries whose every line you are willing to run as root.
8. Its refusals are documented on this page, including the ones that look like limitations.

## See also

- [Architecture overview](../03-architecture/overview.md) — where these sit
- [The security model](../03-architecture/security-model.md) — the authorisation argument
- [Packs and boxes](../03-architecture/packs-and-boxes.md) — what the pack daemon implements
- [kdos-res](kdos-res.md) — the monitor that asks the energy daemon
- [kinstall](kinstall.md) — which groups the installed account ends up in
- [Filesystem and IPC](../06-reference/filesystem-and-ipc.md) — every socket and verb in full
- [Boot and init](../03-architecture/boot-and-init.md) — how they are started
