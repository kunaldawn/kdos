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
- **One line per connection.** A short request, a short answer, no session state.
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
| `ping` | Liveness |

One word per connection, and **nothing in the protocol takes an argument** — there is nothing to
forge and nothing to aim.

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
| `ping` | Liveness |

**The client asks for an index out of a list the daemon published**, and the daemon decides the
device, the mountpoint and the options. Every "just take a path and a mountpoint" design ends at
mounting a stick over `/etc` from any shell in `wheel`.

**A request is one line, and two frames where a secret is involved.** Frame one is a verb and up to
three tokens; frame two is the exact byte count frame one declared. A passphrase is a **frame and
not a token** because a tokeniser splits on spaces and a passphrase may contain them.

**Every token is checked before it means anything, and the token COUNT is fixed per verb.** An
index is one to three digits and inside the published list. A trailing token nobody named makes the
request unknown rather than ignored — a dispatch that read an index and discarded the rest of the
line accepted `mount 0 rm -rf /` as a well-formed mount.

**The passphrase reaches `cryptsetup` on standard input**, through `--key-file=-`, and never in an
argument vector: `/proc/<pid>/cmdline` is world-readable for the life of the process. One buffer
holds it and every exit from the request wipes it.

**The mapper name is the daemon's**: `kdos-<kname>`, derived from the row. A client cannot ask for a
mapping named anything else, and `close` finds the same name from the same row without being told
it. The unlocked volume is not yet enumerated by `list` — mounting it is `kdos-disks`' half.

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

**This daemon spawns children now** — `eject`, `cryptsetup`, `mkfs` — which it never did before; it
mounted with `mount(2)` and unmounted with `umount2()`. Every child is named by an **absolute path**,
because `execvp` would otherwise resolve a program through an inherited `PATH` in a process running
as root, and every one goes through a single function.

**The path overrides are gated on fixture mode.** `KDOS_MOUNTD_SYS`, `_DEV`, `_MOUNTS`, `_FSTAB` and
`_CONF` are read only when `--fixture` or `--fixture-serve` set it. A daemon started by the service
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
