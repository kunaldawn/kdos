# The daemons

The privileged and long-lived services KDOS ships: five root daemons on sockets in `/run`, the
per-box sandbox helper, and the portal backend. They share one shape, and the differences between
them are all in what they own.

For who is allowed to talk to them and why, see
[The security model](../03-architecture/security-model.md).

## The shape they share

Every root daemon in this system is built the same way. A new one that is not is the odd one out.

- **Foreground, under `ksvc`.** No daemonising, no forking into the background. The supervisor
  owns the process and writes its pid file.
- **One socket in `/run`**, named after the daemon.
- **Mode 0666, with the credentials as the real gate.** Anyone may connect; the daemon reads the
  connecting process's real user id from the kernel and answers `err not permitted` to anyone who
  is not root or in `wheel`. A mode that *looked* like the authorisation is a mode somebody
  eventually loosens.
- **One line per connection.** A short request, a short answer, no session state. The one exception
  is a **subscription**, which keeps its socket and is written to when something changes; it still
  holds no state — the daemon remembers a file descriptor and nothing else — and it is why a daemon
  with one polls rather than blocking in `accept()`, since otherwise a subscriber would leave every
  other client queued behind it forever.
- **The client never names a path.** Every verb takes an identifier out of a list the daemon
  itself published.
- **A fixture seam.** Each daemon can be pointed at a recorded system state and made to print what
  it *would* do without doing it. That is the only way selection logic this consequential gets
  tested.
- **A skip check in the init script, before supervision.** A daemon that cannot do its job on this
  machine is skipped and says why — because a refusing daemon under a respawn loop is a boot that
  never settles.

| Daemon | Socket | Skipped when |
|---|---|---|
| `kdos-powerd` | `/run/kdos-powerd.sock` | The binary is missing |
| `kdos-energyd` | `/run/kdos-energyd.sock` | There is no readable energy counter |
| `kdos-oomd` | `/run/kdos-oomd.sock` | The memory pressure interface is not writable |
| `kdos-mountd` | `/run/kdos-mountd.sock` | The binary is missing |
| `kdos-packd` | `/run/kdos-packd.sock` | The kernel cannot mount the pack filesystem |

## kdos-powerd

Suspend, poweroff and reboot for a desktop that is not root.

| Verb | Does |
|---|---|
| `suspend` | Suspend to RAM |
| `poweroff` | Power off |
| `reboot` | Reboot |
| `timezone <Area/City>` | Point `/etc/localtime` and `TZ` at a zone |
| `autologin <user>\|off` | Which account tty1 logs in without asking |
| `firewall list\|<service> on\|off` | Which named services answer the network |
| `ping` | Liveness |

One line per connection. Four verbs are a bare word; `timezone` is **the only one that takes an
argument**, and it is the only place in this protocol where anything has to be validated.

**The timezone is here rather than in a second daemon** because it is the same question: writing
`/etc/localtime` and `/etc/profile.d/20-timezone.sh` is root's, the person doing it is the one
administering the machine, and `wheel` is already the answer to who that is. A second socket with
a second authorisation rule would be a second answer to one question.

**A zone name is validated as a character set first and as a file second.** A zone is `Area/City`
or `Area/Sub/City`, so a slash is legal — which makes `../../etc/shadow` legal-looking, and the
character rule is what stops it: letters, digits, `+`, `-`, `_` and `/`, no dot at all, no leading
or doubled slash. Then the file must exist under `/usr/share/zoneinfo`, so a name that passes the
first check and names nothing is refused rather than symlinked to.

**Both halves are written or neither is.** `/etc/localtime` is what a program reading the zoneinfo
tree follows; `TZ` is what musl reads, and it **wins where it is set** — which it is, on every KDOS
login. Writing only the symlink leaves `date` reporting the old zone in every shell that had
already sourced the profile, which reads as the setting having done nothing. `TZ` is written as
`:/etc/localtime`, the colon form that points musl at the same file, because that is the only value
that cannot name different rules from the symlink beside it.

**`autologin` is here for the same reason the timezone is**: `/etc/kdos/con.conf` is root's and the
choice is an administrator's, which is what `wheel` already answers. **The account must be one a
greeter would offer** — `kb_users()` is the one place that decides who may log in, and pointing
autologin at a name it would not list is a machine that boots to a login nobody can complete.
**Both keys are rewritten together**, because `greet` and `autologin` are one setting seen twice:
`greet = no` with no autologin logs in whatever the default happens to be, and an autologin under
`greet = yes` is a line that does nothing and reads as though it does. A file that would not fit
the rewrite buffer is refused rather than truncated — half a config is a machine whose login
settings are whatever survived.

**`firewall` names SERVICES and never ports, and the table is the daemon's.** A client that could
name a port could open any port; a client that can only name `ssh` opens exactly what the daemon's
table says `ssh` is. `kdos-firewall` asks for the list rather than carrying a copy, so there is one
answer to what a name means. The file is **rewritten whole** from the names that are on — merging
would mean parsing nftables syntax to find what to remove, and a parser that got it wrong would
leave a port open that the surface showed as closed. Anything hand-written belongs in another file
under `/etc/nftables.d`, which the daemon never reads or touches.

**The ruleset is checked before it is applied.** `/etc/nftables.conf` begins with `flush ruleset`,
so a bad file half-applied is a machine with no firewall at all; `nft --check` first means a bad
ruleset is refused and the previous one stays in the kernel.

**`--set-timezone`, `--set-autologin` and `--firewall` exist for the same reason `--explain` does.** The gate is
SO_PEERCRED on a connection and cannot be exercised without two uids, so each verb's own rules
would otherwise be asserted by nothing. None of them grants anything: it is the binary writing to
an `/etc` the caller could already write to, which on the real path is root's.

Poweroff and reboot **signal process 1 first** and only then call the kernel directly, so the init
system gets its chance to run its shutdown entries.

```sh
kdos-powerd --explain <user>
```

answers "would this user be allowed, and why", which is what a dead power key gets diagnosed with.

The socket path can be moved for testing, and **moving it grants nothing** — authorisation never
depended on the path.

`kdos-power` is the client.

## kdos-energyd

Per-application energy attribution. Windows, macOS and Android all ship this; no other Linux
desktop does — and the reason is not the measurement, which is decades old. It is **identity**: an
application is dozens of processes in scattered groups, and nothing on an ordinary desktop owns
enough of the system to name them. Here the container boundary already exists and its supervisor
already knows the name, so the expensive half is free.

```
KDOS energy  —  2.1 h of samples, RAPL package-0

  firefox-esr (appbox app.firefox-esr)     75.5%  ███████████████   gpu 75.0%
  kdos-comp                                15.4%  ███               gpu 25.0%
  short-lived and exited processes          8.7%

  shares are of ATTRIBUTABLE energy — 57% of the package total; the rest is the idle floor
  idle floor 15.00 W, the lowest average power seen in 3 samples
```

**Relative, never watt-hours.** The counter measures the processor package. It cannot see the
panel — the largest single draw on a laptop — nor the radio, the storage, or a discrete graphics
card. "This application was 41% of attributable CPU energy today" is a measurement; "this
application used 12% of your battery" is a guess wearing a unit.

Six decisions, each of which changes the answer:

- **Nested domains are dropped.** The power interface lists a package flat beside that package's
  own sub-domain, so summing the listing counts the cores twice — measured on a fixture, 15 W
  becomes 26.25 W. A domain is a sub-domain exactly when it appears *inside* another's directory.
  The platform-wide domain goes the other way: it contains the packages, so where it exists it
  replaces them.
- **The counter wraps**, roughly every half hour at typical power, and a naive subtraction produces
  one enormous negative reading with nothing in the output saying so.
- **The idle floor is subtracted before anything is attributed.** A package burns power with
  nothing running, and a share model that skips this reports a machine at a login prompt as 90% one
  process. The floor is the lowest average power seen — a measurement, printed with the answer.
- **The floor is applied at report time, not per window.** It can only fall, so charging each
  window the floor as it stood then throws away the first window entirely — which is usually the
  busiest, because something was just launched. Each application carries weighted sums and the
  report computes the subtraction once, with the floor as it finally stands.
- **The denominator is the system's aggregate, not the sum of surviving processes.** A build that
  starts and exits inside one window is gone by the next sample, and dividing by the survivors
  would hand its energy to them. That difference is a real quantity and gets its own line.
- **The graphics column is engine *time*, never energy.** Nothing on the machine says what that
  time cost in joules. On integrated graphics it is already inside the package number; on a
  discrete card it is outside the counter entirely, and the report says so. A driver publishing no
  statistics gets **no column**, not a column of zeroes.

**Why a daemon, and why the socket is not an oracle.** The counter is free-running, so a one-shot
tool could only report what happened while it was watching. And it has been root-only since a
side-channel attack showed fine-grained unprivileged reads can recover cryptographic keys. What
leaves this process is a per-application percentage over minutes; the raw counter and the interval
are never republished, and **the interval is fixed by the daemon rather than requested by a
client**, so it cannot be driven toward being one. There is no write path into the power interface
at all.

Answers go to root and `wheel` and nobody else — on a multi-user machine this list is what everyone
else is running.

`kdos-energy` is the client. `kdos-res`'s Energy page is the same answer, asked for.

## kdos-oomd

Killing something before memory pressure wedges the desktop.

**The kernel's own killer is the wrong signal, not a redundant one.** It fires when an
*allocation* fails, which on a machine with swap is minutes after the desktop stopped answering —
the whole session spent thrashing while the kernel technically still had pages. The pressure
interface says the machine is **stalling** on memory, which is what a wedged desktop feels like.
Containerised applications make that likely here: a browser and a slicer in one modest machine.

Five rules:

- **It blocks; it does not poll.** The threshold is *written into* the pressure file and the daemon
  waits on it. That is the kernel's own trigger mechanism; a sampling loop would be the thing
  competing for processor time with the stall it is trying to notice.
- **The desktop is not eligible.** The compositor, the panel, the desktop, the notification daemon,
  process 1 and kernel threads are protected. **Boxed processes are preferred victims**: a
  containerised application is the likely culprit, is supervised, and relaunches in seconds, while
  a host process is more often session state.
- **A box over its declared memory budget is preferred**, ahead of the general rule — which is what
  makes the profile's memory key honest, since a rootless container on a machine with no cgroup
  delegation accepts a limit and ignores it.
- **Identity is the conmon walk**, the same one the monitor and the energy daemon use. The message
  names the box.
- **The pages are released immediately after the kill**, through a handle taken **first** so the
  release cannot land on a recycled process id. Under a stall, getting the pages back now rather
  than whenever the process is reaped is the whole point. A kernel without that call skips the
  release; the kill stands.

**Nothing in the protocol names a process, so there is nothing to aim.** The socket answers `ping`
and `status` and takes no argument; killing is the daemon's own decision or it does not happen. At
most one kill per ten seconds.

`--fixture <dir>` prints who **would** be killed and signals nobody.

**It has never fired for real.** A genuine pressure stall is the test that matters and has not been
run.

## kdos-mountd

Removable media. Plugging a stick into this machine did nothing at all before it existed: there is
no general-purpose disk service here, mounting is root's, and the desktop is not root.

| Verb | Does |
|---|---|
| `list` | The eligible devices, with an index each |
| `mount` | Mount the device at an index |
| `unmount` | Unmount it |
| `eject` | Power the medium down. Optical media eject their own node; a stick ejects the **parent disk**, because a start-stop on one partition means nothing to the hardware |
| `unlock` | Open a LUKS volume. The passphrase is a second frame, never a token |
| `close` | Close the mapping `unlock` made |
| `format` | Write a filesystem. **Off unless `format = yes`** |
| `smart` | The drive's model, serial and health, tab-separated |
| `cifs` | Mount an SMB share. The password is a second frame, never a token |
| `shares` | The network shares that are mounted, with an index each |
| `disconnect` | Unmount the share at a share index |
| `ping` | Liveness |

**The client asks for an index out of a list the daemon published**, and the daemon decides the
device, the mountpoint and the options. Every "just take a path and a mountpoint" design ends at
mounting a stick over `/etc` from any shell in `wheel`.

**A request is one line, and two frames where a secret is involved.** Frame one is a verb and up to
five tokens; frame two is the exact byte count frame one declared. A passphrase is a **frame and
not a token** because a tokeniser splits on spaces and a passphrase may contain them. The line's
ceiling is `cifs`'s and nothing else's: a DNS name may be 253 bytes, a share 80, a username 104 and
an NT domain 255, so a legal corporate share spells a request of about seven hundred.

**Every token is checked before it means anything, and the token COUNT is fixed per verb.** An
index is one to three digits and inside the published list. A trailing token nobody named makes the
request unknown rather than ignored — a dispatch that read an index and discarded the rest of the
line accepted `mount 0 rm -rf /` as a well-formed mount.

**The passphrase reaches `cryptsetup` on standard input**, through `--key-file=-`, and never in an
argument vector: `/proc/<pid>/cmdline` is world-readable for the life of the process. One buffer
holds it and every exit from the request wipes it.

**The mapper name is the daemon's**: `kdos-<kname>`, derived from the row. A client cannot ask for a
mapping named anything else, and `close` finds the same name from the same row without being told
it.

**The mapper an `unlock` opened is listed beside the container it came from**, found by the name
the daemon itself chose rather than by walking `/sys/block/dm-*`. Without it an unlock is a dead
end: the container's row goes on saying `crypto_LUKS`, nothing on the list can be mounted, and the
filesystem inside — the only reason anybody unlocked it — is reachable from no verb at all. A
mapper this daemon did not open is not this daemon's to offer, which is the right answer for
somebody's own `cryptsetup open` of a root volume. The row carries the CONTAINER'S disk, so every
destructive verb is still refused by the physical drive.

**`smart` answers for the DISK and not the partition.** SMART is a property of the drive, so a row
per partition would print the same answer four times and would point a raw-device tool at an offset
nothing owns. `smartctl` needs the raw block device, which nothing in a session may open — the
alternative to a verb is a setuid binary or a sudo rule, and both are a wider hole than one daemon
answering one question. Its output is **captured and filtered, never forwarded whole**: `smartctl
-a` is two hundred lines of vendor attributes, and what a person opening a disks window wants is
whether the drive says it is failing and which drive that is. Its **exit status is a bitfield and
not a failure** — bits 3 to 7 mean the drive is unwell, which is frequently the answer rather than
the absence of one — so only an empty capture is treated as nothing learnt.

### A share on another machine

**`mount(2)` cannot raise a cifs session**, so `cifs` is the second verb that spawns a child: the
dialect negotiation, the authentication and the tree connect all happen inside `mount.cifs` before
the syscall it eventually makes.

**Each of the four names is checked against a character allowlist of its own.** `mount.cifs`
assembles its option string by concatenation and escapes nothing but the password, so a comma in
the server, the share, the username or the domain is a **new mount option** handed to the kernel's
cifs parser, and a `/` or a `\` in a server silently re-aims the mount — the helper's own
`parse_unc()` splits on exactly those. What is not on the list is refused rather than quoted:
quoting is a second implementation of that parser.

**The password reaches the helper on a descriptor.** `PASSWD_FD=0`, with the bytes on the child's
stdin. `mount.cifs` will also take one from `$PASSWD`, from a file named by `$PASSWD_FILE` or from
`pass=` in the option string — an option string is argv, an environment value is
`/proc/<pid>/environ`, and a file is a file somebody has to delete.

**The module is loaded before the question is asked.** `cifs` is a module here and nothing else on
the image loads it, and `/proc/filesystems` lists only what is already in the kernel — a support
check in front of `modprobe` would refuse every first connection on a machine that can do this
perfectly well.

**The mount is the caller's.** `uid=`, `gid=`, `file_mode=` and `dir_mode=` are always given,
because a server that speaks no unix extensions reports every file as owned by root and a share
only root can read has not mounted as far as the person who asked is concerned. `nosuid` and
`nodev` always, and `noexec` unless `exec = yes` — the same argument the removable rules keep.

**What is connected is what `/proc/mounts` says is connected.** No list is held between requests:
a server that went away, or a share a second session mounted, must not be answered for out of this
daemon's memory.

### What a destructive verb refuses

**The boot medium is refused by the DISK, not by the partition.** A live USB carries an iso9660
partition *and* a vfat ESP beside it. Every per-partition rule offers the ESP — it is removable, it
probes as vfat, it is unmounted and no fstab claims it — so a format there destroys the running
session, and a typed confirmation does not help because the person genuinely typed the name of the
row they meant. Any disk carrying an iso9660 partition is the boot disk, whole, in a live session.

**`format` demands the device's own kernel name, typed.** Not a flag, not a hash, not the word yes:
the daemon compares what it was sent against the string it put in the list itself, by exact length
and `memcmp`. A client cannot send a confirmation it was not shown.

**`format` is opt-in**, `format = yes` in `/etc/kdos/mountd.conf`, the same argument `noexec` won.
The filesystem is one of four — ext4, btrfs, vfat, exfat — checked against a table and nothing else.

**The node is re-derived at the moment of use.** Between the scan that built the row and the syscall
that acts on it, a path can become a symlink or a different device: the daemon opens it `O_NOFOLLOW`
and requires a block device whose `st_rdev` matches the one `/sys` recorded.

**Every child this daemon spawns** — `eject`, `cryptsetup`, `mkfs`, `modprobe`, `mount.cifs` — is
named by an **absolute path**,
because `execvp` would otherwise resolve a program through an inherited `PATH` in a process running
as root, and every one goes through a single function.

**The path overrides are gated on fixture mode.** `KDOS_MOUNTD_SYS`, `_DEV`, `_MOUNTS`, `_FSTAB`,
`_MEDIA` and `_CONF` are read only when `--fixture` or `--fixture-serve` set it. A daemon started by the service
script reads none of them — an environment variable that moved its idea of `/dev` would be a way to
point a format at any node on the machine.

**The list is rescanned on every request** rather than cached — a stick pulled out between two
requests must not still be offered.

**Eligibility is four refusals**, and each is the point:

| Refusal | Why |
|---|---|
| Not removable and not on USB | An internal disk is the administrator's. An external drive in an enclosure reports itself non-removable, so the bus is checked too |
| A filesystem this kernel cannot mount | Checked by name **before** the mount call, not after |
| Anything named in `/etc/fstab` | An entry there is a decision somebody already made |
| The medium this system booted from | Offering to unmount the live medium is offering to kill the session |

**`nosuid,nodev` always, `noexec` by default.** A setuid root binary on a stick is a local root
hole that predates every other consideration; `exec = yes` in the configuration is how somebody
says they meant it.

The mountpoint is `/media/<user>/<label>`, and **the label is sanitised to a safe character set
before it becomes a path component** — it is whatever was written into a superblock by somebody
else's computer.

**There is no separate identification library.** The label and type come from reading the
superblock directly, for the handful of formats a stick is actually formatted with. A seventh
format would be a library.

`--fixture <sys> [dev]` prints what it **would** offer and mounts nothing. `--fixture-serve` runs
the real dispatch over a real socket with the fixture's roots and **prints each argument vector
instead of running it**, which is how a `format` aimed at the boot medium is proved to be refused
without a disk to lose. The committed fixture is
a recorded block-device tree plus two hand-built superblocks — a removable one that must be
offered, and an **internal** one that must not. The internal disk carries a real superblock
precisely so a broken removable check shows up as an extra row rather than as nothing.

**The front end is `kdos-devices`, not the panel.** A short connection per request from a surface
that is already waiting for a keystroke is fine; a socket round trip per panel tick is exactly what
"nothing blocks the frame" is about.

**`subscribe` is the daemon's only long-lived verb, and it names nothing.** It writes `changed` when
the device list moves and never exits. It cannot say *which* device, because an index is only true
of the list it came with and `scan()` rebuilds that on every request — so a subscriber asks again
with `list` and diffs. The events come from the kernel's own `NETLINK_KOBJECT_UEVENT` broadcast
rather than from a udev rule, so hotplug works with no rule file and no dependency on udev running;
`ACTION=change` is in the filter because that is what a drive reports when a disc goes into a tray
that was already there.

**`kdos-mediad` is the subscriber, and it is the session's.** The daemon is root, starts before
anybody logs in, and has no session bus — `Notify` lives at `$XDG_RUNTIME_DIR/bus`, which belongs to
a login that may not exist yet. So the daemon says only that something moved, and `kdos-mediad`,
which runs in the session beside `kdos-notifyd`, decides what it means and raises the toast with its
**Open** and **Eject** buttons. It re-reads the list at the click rather than trusting the row the
toast was built with: a button pressed a minute later would otherwise act on whatever had arrived
since.

## kdos-packd

The only thing on the system that mounts an application pack.

| Verb | Does |
|---|---|
| `list` | Every pack the machine can see, with its state |
| `info` | One pack's metadata |
| `mount`, `unmount` | Mount or release a pack |
| `compose`, `decompose` | Build or tear down a box's overlay stack |
| `install`, `remove` | Copy a pack into the store, or take it out |
| `rollback` | Return to a retained earlier version |
| `graft`, `ungraft` | Place or remove a data pack's contents |
| `ping`, `status` | Liveness, and the daemon's own configuration |

**`status` publishes the staging directory and the retention count**, so a client writing a
download into the store does not have to derive either — a second definition of where an
unprivileged write is allowed is exactly the kind of thing that drifts.

**The client never names a path**, with one deliberate exception: `install` takes a **filename** in
a staging directory the daemon owns, mode 01777, the one place an unprivileged download may land.
Relative traversal and absolute paths are both errors.

The verification rules, the two mount routes, reference counting and adoption at startup are in
[Packs and boxes](../03-architecture/packs-and-boxes.md). Two behaviours belong here:

- **An install drops the old version's idle mount, and refuses one that is composed.** The mount
  table is keyed by identifier and survives the rescan an install triggers, so without this every
  compose after an update puts the **new** application's layer over the **old** runtime's mounted
  bytes — measured: an application dying on a library the new runtime carries and the mounted one
  did not. Idle, the old mount goes before the file swap; in use, the install is refused by the
  rule removal already applies.
- **Retention is what makes rollback possible.** The default keeps one previous version. A store
  keeping none could not roll anything back; one keeping every version an application ever had
  would fill a disk with copies nobody will launch again. **The sweep runs after an install and at
  no other time** — a sweep on a timer would be a background job deleting somebody's rollback while
  they were deciding whether to use it. `retain = 0` is an honest off that makes rollback answer
  "no earlier version is kept" rather than fail at a rename.

**A socket path that does not fit the address structure is refused, not truncated.** Truncation
binds a socket nobody asked for and answers the next start with "address already in use" for a file
that appears not to exist — and two different runtime directories can land on one socket.

`--fixture <store> [medium]` prints what it **would** mount and mounts nothing.

## kdos-boxsock

Not a `/run` daemon: **one tagged Wayland socket per box**.

It binds a socket for one box, hands it to the compositor tagged with the box's name and instance,
and then **stays alive holding the descriptor that keeps the tag valid**. Every client connecting
on that socket is tagged **by the compositor itself** — the client never sees the tag and so
cannot forge, choose or drop it.

That tag is what the compositor's sandbox filter reads, what the box chip on a title bar resolves,
and what lets the panel say which box a window came from.

**It is a separate program for two structural reasons.** The launcher *replaces itself* with the
container command, so it cannot hold anything for the box's lifetime — and the sandbox lives
exactly as long as that descriptor stays open, so somebody has to outlive the launch. And the
launcher links a deliberately small set of libraries; speaking a Wayland protocol would mean adding
a client library and generated protocol code to a program whose dependency list is a documented
property rather than an accident.

## xdg-desktop-portal-kdos

The portal backend: the file chooser, settings, and the application chooser. Covered in
[The session](../03-architecture/session.md#the-kdos-backend), including the two rules that matter
most — every request is answered, and **the bus loop does not block on the dialog**.

## kdos-lock

Not a root daemon, but the other long-lived privileged-adjacent piece. It covers every output with
a lock surface and asks `kdos-checkpass` — which takes **no arguments** and reads the password on
standard input — to check the password.

**The compositor owns the locked state**, so a crash in the lock program leaves the screens covered
and allows a **new** lock client to replace the abandoned one. See
[kdos-comp](kdos-comp.md#idle-dim-lock-and-lid).

## Adding a root daemon

A new one matches the family when all of these are true:

1. It runs in the foreground and is started by an `init.d` script under `ksvc`.
2. Its script **skips with a reason** when the machine cannot support it, before supervision.
3. It owns exactly one socket in `/run`, mode 0666.
4. It authorises on the peer's credentials — root and `wheel` — and answers `err not permitted`
   otherwise.
5. No verb takes a path. Identifiers come from a list the daemon published.
6. It has a `--fixture` mode that decides and prints without acting.
7. It links only libraries whose every line you are willing to run as root.
8. Its refusals are documented, including the ones that look like limitations.

## See also

- [Architecture overview](../03-architecture/overview.md) — where these sit
- [The security model](../03-architecture/security-model.md) — the authorisation argument
- [Packs and boxes](../03-architecture/packs-and-boxes.md) — what the pack daemon implements
- [kdos-res](kdos-res.md) — the monitor that asks the energy daemon
- [Filesystem and IPC](../06-reference/filesystem-and-ipc.md) — every socket and verb in full
- [Boot and init](../03-architecture/boot-and-init.md) — how they are started
