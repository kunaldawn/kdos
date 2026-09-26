# The security model

This page describes who may do what on a KDOS machine, which mechanisms
enforce it, and — just as importantly — what is **not** protected. It is for
anyone who administers a KDOS machine, reviews its security, or changes one of
the privileged programs.

If you read only one section, read [What is not protected](#what-is-not-protected)
at the end. A security model that lists only its defences gets relied on for
things it does not do, so that section is part of the specification, not a
footnote.

After reading this page you will know:

- which programs run with privilege, and why each one needs it;
- how the root daemons decide whom to obey;
- why the desktop has no password prompt for administrative actions;
- what a containerised application (a *box*, built from read-only *pack*
  images; see the [glossary](../06-reference/glossary.md)) can and cannot
  reach;
- which keys and certificates the machine trusts, and how to change them.

## What this model is for

KDOS is a single-user workstation. One human account ships (`kdos`, uid 1000),
that account is in the `wheel` group, and `wheel` is trusted. The design
defends against four things:

- **Outside software misbehaving.** A browser, an office suite or an
  application from another distribution doing things a desktop application has
  no business doing.
- **A tampered artefact.** A package, an index or an application image that is
  not what it claims to be.
- **Privilege escalation** through the small number of programs that genuinely
  need privilege.
- **A catastrophic mistake** by a `wheel` user, made through an interface that
  should never have let it be expressed.

It does not defend against the person at the keyboard, or against software
running as that person; see [What is not protected](#what-is-not-protected).

## setuid binaries

A *setuid* binary runs as its owner (here, root) no matter who starts it. The
shipped system carries nineteen setuid-root binaries. Two are KDOS's own.

| Binary | From | What it is for |
|---|---|---|
| `kdos-checkpass` | **KDOS** | Checking the caller's own password against `/etc/shadow` (the lock screen) |
| `kdos-resctl` | **KDOS** | Signalling and renicing a process, and reading the hardware table, for the resource monitor |
| `sudo` | sudo | Running a command as another user |
| `su` | shadow | Switching to another account. util-linux's `su` is not built (`--disable-su`) |
| `passwd`, `chage`, `gpasswd`, `chfn`, `chsh`, `newgrp` | shadow | Account management |
| `pkexec`, `polkit-agent-helper-1` | polkit | polkit's privileged actions |
| `ssh-keysign` | OpenSSH | Host-based authentication |
| `dbus-daemon-launch-helper` | dbus | System-bus activation. Owned `root:messagebus`, mode `4110`, so only the bus daemon can run it |
| `mount.nfs` | nfs-utils | Mounting an NFS share listed in `fstab` as an ordinary user |
| `unix_chkpwd` | pam | Lets `pam_unix` check a password for a caller that is not root. Without it every unprivileged PAM check fails, `wayvnc`'s included |
| `fusermount3` | fuse | Mounting a userspace filesystem from a session with no user namespace: sshfs, gocryptfs, fuse-overlayfs, the document portal, `rclone mount`, and `restic mount` through the `fusermount` link beside it |
| `newuidmap`, `newgidmap` | shadow | **Rootless containers** |

`newuidmap` and `newgidmap` are why every application on the machine starts.
The container engine (podman) runs them to write a process's user-namespace
map, which the kernel allows only from a process that already holds the
capability. Podman checks the binaries first; if they are not setuid it exits
with status 125 and prints nothing else. Lose those two bits and no graphical
application starts, with nothing saying why.

Deliberately **not** setuid:

- util-linux's `mount` and `umount` (built `--disable-makeinstall-setuid`);
  removable media go through `kdos-mountd` instead.
- `bwrap` (bubblewrap). It sandboxes through an unprivileged user namespace.
- `ksu`, Kerberos's `su`. The `krb5` recipe deletes it: accounts here are
  local and privilege escalation is `sudo`, so it would be an entry nobody uses
  and nobody audits. Kerberos is here for `kinit`, an ordinary program.
- `wireshark`'s `dumpcap`, `bandwhich` and `trippy`. Run them as root or grant
  the capability yourself.

**The table is a guide; the built image is the authority.** To check a machine:

```sh
find / -xdev -perm -4000 -type f
```

Mode bits are used rather than file capabilities because mode bits survive all
three copies the system makes — the compressed system image, the installer's
copy to disk, and the pack image format — and no extended attribute does.

Losing a setuid bit is the worst *silent* failure on the system: one archive
copy without the right flag is enough. `kdos doctor` checks the five that
matter most — `kdos-checkpass`, `kdos-resctl`, `newuidmap`, `newgidmap`, and
`dbus-daemon-launch-helper` with its `messagebus` group — and prints the fix
when one is wrong: `chown root` and `chmod 4755` for the first four,
`chown root:messagebus` and `chmod 4110` for the bus helper. The same check
confirms that `/etc/subuid` and `/etc/subgid` exist, because without them no
rootless box can map a user.

### And no setgid ones, which is why `plocate`'s index is per user

`plocate` (the `locate` command) ships upstream as **setgid** to a `plocate`
group, with one shared database in `/var/lib` at mode 0640. That database
names every path on the machine; the binary reads it on the user's behalf and
filters out, per result, any path the caller could not have reached.

KDOS ships none of that: no setgid bit, no `plocate` group, no shared
database. Each user's index is built **by that user**, from their own home
directory, into their own cache:

| Piece | Where |
|---|---|
| The builder | `/usr/local/bin/kdos-updatedb`, which runs `/usr/sbin/updatedb --require-visibility no --prune-bind-mounts yes --database-root "$HOME"` |
| The index | `${XDG_CACHE_HOME:-~/.cache}/kdos/plocate.db` |
| The schedule | `~/.config/kdos/timers.d/20-updatedb.timer`, one of the per-user timers the session runs |
| How `locate` finds it | `$LOCATE_PATH`, set by `/etc/profile.d/40-plocate.sh` |

An index built this way can only contain paths its owner could already list,
so the visibility check has nothing to guard. `--require-visibility no` is
required, not optional: with the check on, `updatedb` insists on the `plocate`
group and writes nothing. The port carries one behavioural patch
(`locate-path-replaces-default.patch`): upstream searches its compiled-in
`/var/lib` database first and stops at the first one it cannot open, so on a
machine with no shared database every search would fail before reaching the
user's own. With the patch, a set `$LOCATE_PATH` replaces the default rather
than following it.

That is the pattern the rest of this page follows: where possible a mechanism
is not made safe, it is made unnecessary.

## kdos-checkpass

The lock screen's password check. If this program is wrong, you are locked out
of your own session, so here is exactly what it does and does not do:

```sh
printf '%s' "$password" | kdos-checkpass    # exit 0 = correct
```

- **It takes no arguments**, not even a user name. It always checks the account
  of the caller's **real user id**, so it cannot be aimed at root.
- **The password arrives on standard input**, never on the command line, because
  a process's command line is readable by every user for as long as it runs.
- **It drops root as soon as it has read the hash.** The hash is computed
  unprivileged.
- **It reads `/etc/shadow` directly**, not through the name-service switch, so no
  loadable module runs as root.
- **The comparison is constant time** over the whole hash.
- **A locked (`!`, `*`) or empty hash is refused with exit 2** before any
  password is read, so an account with no password cannot be unlocked by
  pressing Enter.
- **A wrong password costs one second**, spent inside the setuid binary, so a
  loop that drives `kdos-checkpass` directly instead of through the lock
  screen is rate-limited too.

| Exit code | Meaning |
|---|---|
| 0 | Correct |
| 1 | Wrong |
| 2 | Could not tell: no such user or shadow entry, an unreadable shadow file, a locked (`!`, `*`) or empty hash, an argument given, or an internal failure (reading standard input, dropping privilege, or `crypt()` refusing the hash) |

A caller must treat 2 as a failure *and say why*: reporting "wrong password"
for a machine with a broken shadow file sends the user looking in the wrong
place.

Without its setuid bit it cannot read the shadow file, refuses every password,
and locks the user out.

### The mode of `/etc/shadow`

`/etc/shadow` must be mode 0600, and the build has to set that explicitly. Git
records only one permission bit (executable or not), so a file under `fs/`
cannot carry a mode narrower than 644, and the file-system build step gives
every non-executable file exactly that. A world-readable shadow file hands
every password hash to every account and makes `kdos-checkpass`'s setuid bit
pointless.

`script/01_phase1/00_file_system.sh` therefore carries a table of the paths
whose mode or owner git cannot express:

| Path | Mode | Owner |
|---|---|---|
| `etc/shadow` | 600 | root |
| `etc/polkit-1/rules.d` | 755 | root |
| `etc/polkit-1/rules.d/50-kdos.rules` | 644 | root |
| `etc/udev/rules.d/` and everything under it | 644 (directories 755) | root |

`testing/preflight.sh` checks the result on the **built** tree, because the
source tree cannot express it. The polkit and udev rows are there because both
daemons read every rule they find with no ownership check: a rules directory
the desktop user could write would let that user grant themselves anything
(polkit) or run any command as root on the next device event (udev).

## kdos-resctl

The second KDOS setuid binary, used by the resource monitor `kdos-res`. Its
security argument is that there is nothing to aim: three verbs, no paths, no
options.

```
kdos-resctl dmi
kdos-resctl signal <pid> <TERM|KILL|STOP|CONT>
kdos-resctl renice <pid> <-20..19>
```

- **Nothing in the argument vector is ever opened.** The hardware-table path
  (`/sys/firmware/dmi/tables/DMI`) is compiled in.
- **The caller must be in `wheel`, by real user id.** The group is fixed at build
  time, never read from the environment.
- **It signals through a process handle (a pidfd)** taken before the signal
  is sent, so the process it opened is the one it signals, even if the number
  is reused in between. On a kernel without pidfd support it falls back to
  `kill(2)`, which does not have that guarantee.
- **Process ids 1 and below are refused.** That covers init, and 0 and
  negative ids, which `kill(2)` reads as process groups.
- **Root is dropped before the hardware table is parsed**, so the parser never
  runs as root.
- **It is never on the sampling path.** `kdos-res` calls it only when you act.
  A setuid program run once a second would be an attack surface on a schedule.

| Exit code | Meaning |
|---|---|
| 0 | Done |
| 1 | Refused |
| 2 | Usage error (any other argument vector) |
| 3 | The operation itself failed |

## System accounts

Every daemon that drops privilege drops to an account of its own. None of
these accounts can log in: each has `/sbin/nologin` as its shell and `!` as its
password. The image ships these in `/etc/passwd`, `/etc/group` and
`/etc/shadow`:

| Account | uid:gid | Used by |
|---|---|---|
| `dhcpcd` | 999:999 | `dhcpcd`'s privilege-separated children |
| `messagebus` | 997:997 | `dbus-daemon`, the system bus |
| `sshd` | 996:996 | `sshd`'s privilege separation |
| `tss` | 993:993 | The TPM: tpm2-tss's udev rules give `/dev/tpm*` to the user and `/dev/tpmrm*` to the group |
| `lp` | 10:10 | CUPS |
| `nobody` | 99:99 | Anything that asks for an unprivileged account by that name |

These accounts are created when their port is installed, by the port's
`postinstall.sh` with `groupadd -r` and `useradd -r`, which pick a free id:
`polkitd`, `avahi`, `avahi-autoipd`, `geoclue`, `mosquitto`, `nm-openvpn`,
`pcscd`, `postgres`, `prosody` and `tcpdump`. `brltty`'s creates the group
`brlapi` and adds the desktop account to it; that is the group brltty's own
polkit rule admits to its braille display interface.

Two rules keep this consistent:

- **Every id a shipped account uses has its own line in both `/etc/passwd` and
  `/etc/group`.** A primary gid with no `/etc/group` line looks free to
  `groupadd -r`, which would hand it to another daemon's group.
- **Every group a udev rule names must exist.** eudev logs
  `specified group '<x>' unknown`, carries on with gid 0, and grants nothing.

The desktop account belongs to `wheel`, `seat`, `lpadmin`, `audio`, `video`,
`render`, `input`, `kvm`, `cdrom`, `dialout`, `tty` and `users`. It is not in
`tss`, so the TPM is root's: run the `tpm2_*` tools under `sudo`.

## Root daemons

Five KDOS daemons run as root and answer a socket in `/run`:

| Daemon | Socket | What it does |
|---|---|---|
| `kdos-powerd` | `/run/kdos-powerd.sock` | Suspend, power-off, reboot, and a few system settings |
| `kdos-energyd` | `/run/kdos-energyd.sock` | Per-application energy use |
| `kdos-oomd` | `/run/kdos-oomd.sock` | Kills a process under memory pressure, sparing the desktop |
| `kdos-mountd` | `/run/kdos-mountd.sock` | Removable media, encrypted volumes, network shares, disk health |
| `kdos-packd` | `/run/kdos-packd.sock` | Mounts and verifies application packs |

Their verbs are in [the daemons](../04-programs/daemons.md). They share one
authorisation design.

### Who may talk to them

**The check is on who is connecting, not on the socket's permissions.** Every
socket is mode 0666 and anyone may connect. The daemon asks the kernel for the
connecting process's user id (`SO_PEERCRED`, which the client cannot forge)
and answers `err not permitted` to anyone it does not admit.

| Daemon | Admits |
|---|---|
| `kdos-powerd` | root and `wheel`: every verb. `seat`: `ping`, `suspend`, `poweroff`, `reboot` only |
| `kdos-mountd` | root, `seat` and `wheel`: every verb |
| `kdos-oomd` | root, `seat` and `wheel` (its only verbs are `ping` and `status`) |
| `kdos-energyd` | root and `wheel` |
| `kdos-packd` | root and `wheel` |

`seat` is the group seatd gives the display to, meaning the person at the
machine. The installer keeps the desktop account in `seat` and, for a
non-administrator, takes it out of `wheel`. Such an account keeps the lid and
the power keys, and loses `sudo`, the polkit grants and every configuration
verb.

The test lives in one function, `kb_uid_allowed()` in `libkbase`, and no
daemon keeps a copy. This is the authorisation boundary of the whole system;
five copies would be a rule tightened on one socket and left loose on the
others. To see how `kdos-powerd` would treat an account, run:

```sh
kdos-powerd --explain <user>
```

It asks the same function the socket does, so its answer cannot drift from the
daemon's.

`kdos-mountd --fixture-serve` is a test mode that admits anybody and grants
nothing: its paths are a scratch directory, and every command it would run is
printed instead. The init script starts the daemon with no arguments, so a
running system never reaches it.

### Clients never name a path

Every verb takes an identifier from a list the daemon itself published a
moment earlier, or from a list compiled into it. A daemon that accepted a
device and a mount point would let anyone at the seat mount a USB stick over
`/etc`. `kdos-powerd accent` is the tightest case: its argument must name one
of the eight colour schemes in `libkcolor`'s own table.

The one exception is installing a pack, which names a **file name** — not a
path — in a staging directory the daemon owns (`/var/lib/kdos/packs/staging`,
mode 01777). That directory is the single place an unprivileged download may
land, and the daemon publishes its location so clients do not derive it.

### What each daemon refuses

| Daemon | Refuses |
|---|---|
| `kdos-mountd` | Removable devices only, with the checks listed below the table |
| `kdos-packd` | Paths as arguments; a staged name outside `[A-Za-z0-9._-]`; a pack whose hash or signature fails; removing a pack that is in use |
| `kdos-oomd` | Any argument at all: killing is its own decision or it does not happen |
| `kdos-energyd` | Republishing the raw energy counter; a client-chosen sampling interval |
| `kdos-powerd` | Anything but `ping`, `suspend`, `poweroff`, `reboot`, and four verbs whose argument is checked against a table compiled into it: `firewall`, `autologin`, `accent`, `timezone` |

`kdos-mountd` refuses:

- internal disks, filesystems the kernel cannot mount, and anything in
  `fstab`;
- **every partition of the disk the system booted from**;
- a verb carrying a token nobody issued, and a device index that is not a
  number;
- a device node that differs from the one the scan recorded;
- any format request unless `format = yes` is set in
  `/etc/kdos/mountd.conf`. With it set, a format request must repeat the
  device's kernel name (for example `sdb1`) exactly as confirmation, and only
  `ext4`, `btrfs`, `vfat` and `exfat` are written.

A note on `kdos-energyd`: the CPU energy counter (RAPL) is root-only because
fine-grained unprivileged reads can recover cryptographic keys through a side
channel. What leaves the daemon is a per-application percentage over minutes.
The raw counter is never republished and the interval is fixed by the daemon,
so a client cannot drive it towards a fine-grained one. There is no write path
into the power interface at all.

`kdos-boxsock` is sometimes counted among these daemons and is not one. It is
installed in `/usr/bin`, runs as the desktop user, binds one tagged Wayland
socket per box, and holds that box's security context open for the box's
lifetime.

## polkit, and why the desktop has no authentication agent

*polkit* is the system service that other daemons ask "may this user do this?".
On a mainstream desktop it pops up a password prompt through an
*authentication agent*. KDOS has no agent, and no prompt, for reasons that
follow from having no session manager.

### Who asks polkit

polkit is installed and `polkitd` is started by the init system. NetworkManager,
ModemManager, `bolt`, `fwupd`, `upower`, `fprintd`, `pcscd` and brltty's
braille server ask it, and GeoClue asks it on ModemManager's behalf.
NetworkManager's actions are the only ones a KDOS surface calls.

### Why a password prompt cannot work here

polkit can never see an *active* session on this machine. It finds a
process's session by calling `org.freedesktop.ConsoleKit` on the system bus;
neither ConsoleKit nor elogind is installed (see
[principles](../01-philosophy/principles.md#no-systemd)), so the lookup fails
and every check falls to the `allow_any` column of the action's `.policy`
file. The `allow_active` and `allow_inactive` columns are never read.

An action with no `<allow_any>` element is then a **flat refusal**. polkit
consults an agent only when the result is a *challenge*, and a flat refusal is
not one. NetworkManager's policy has no `<allow_any>` for
`enable-disable-wifi`, `enable-disable-network` or either `wifi.share` action,
so no agent — one written here or the `pkttyagent` polkit ships — could ever be
asked about the Wi-Fi switch. An agent would also have nothing to register as:
with no session it can register only for one exact process (matched by pid and
start time), so a session-long agent would never be found for a program it did
not start itself.

The same applies to `bolt`, `fwupd` and `upower`. Their actions are
`auth_admin`, a challenge, and a challenge with no agent is a refusal. `bolt`
and `fwupd` ship their own rules granting `wheel`, but both test
`subject.active` and `subject.local`, which are never true here, so neither
fires. `fprintd`'s enrol and verify actions are `allow_active` only. A
Thunderbolt enrolment, a firmware update or a fingerprint enrolment is
therefore done with `sudo`; polkit authorises root for every action.

### What is granted instead: `50-kdos.rules`

`/etc/polkit-1/rules.d/50-kdos.rules` grants a short, named list of actions to
members of `wheel`, and nothing to anyone else:

| Action | Used by |
|---|---|
| `org.freedesktop.NetworkManager.network-control` | `kdos-net`: connecting and disconnecting |
| `org.freedesktop.NetworkManager.wifi.scan` | `kdos-net`: the network list |
| `org.freedesktop.NetworkManager.enable-disable-wifi` | The Wi-Fi switch |
| `org.freedesktop.NetworkManager.enable-disable-network` | The networking switch |
| `org.freedesktop.NetworkManager.settings.modify.own` | Profiles another tool created with an owner |
| `org.freedesktop.NetworkManager.settings.modify.system` | Joining, forgetting, and reading a saved passphrase back (see below) |
| `org.freedesktop.NetworkManager.wifi.share.open`, `…wifi.share.protected` | A hotspot |
| `org.freedesktop.ModemManager1.Device.Control`, `…Messaging` | `mmcli`: unlocking a SIM, enabling a modem, reading its text messages |
| `org.debian.pcsc-lite.access_pcsc`, `…access_card` | Smart cards (gpg's scdaemon, opensc, ykman, openconnect's certificate login) |

Deliberately **not** granted, and so left to `sudo`:

| Action | Why not |
|---|---|
| `settings.modify.hostname` | The hostname belongs to the installer and `/etc/hostname` |
| `settings.modify.global-dns` | A machine-wide resolver is not a per-user decision |
| `checkpoint-rollback` | Nothing here creates a checkpoint |
| `sleep-wake` | `kdos-powerd` owns suspend, with its own check |
| `reload` | Re-reading NetworkManager's configuration is administration |

Two further consumers work without a KDOS rule. brltty's own rule grants
`org.a11y.brlapi.write-display` to the `brlapi` group with no session test.
GeoClue's rule grants ModemManager's `Device.Control` and `Location` to the
`geoclue` account alone, so the location service can switch on a modem's GPS;
nobody logs in as that account.

The grant is a rules file of named actions, not a D-Bus policy that lets
`wheel` onto NetworkManager's write interfaces, because a bus policy cannot scope a
`Properties.Set` to one property, and cannot leave out the hostname or the
machine-wide resolver. If a session provider were ever added, the explicit
grants would stay exactly as narrow as they are written.

### Why `settings.modify.system` is granted

On this build it is the only permission a saved network can be under.
NetworkManager is compiled `-Dsession_tracking=no`, so its check "does this
user have a session?" always answers no. A connection profile that names an
owner in its `permissions` list is therefore permanently invisible, and an
invisible profile never connects automatically — a Wi-Fi network you joined
would not come back after a reboot. So `kdos-net` writes no owner: everything
it creates is a system connection, and forgetting one or reading its
passphrase back needs `settings.modify.system`.

That grant lets anybody in `wheel` read every stored Wi-Fi passphrase. It is a
shortcut, not a new capability: the shipped sudoers line is
`%wheel ALL=(ALL) ALL`, and the passphrases are files under
`/etc/NetworkManager` that `sudo cat` prints.

### How polkit and ModemManager are started

`polkitd` is started by `/etc/init.d/41_polkitd.sh` and ModemManager by
`/etc/init.d/42_modemmanager.sh`, rather than by D-Bus activation. Activation
of a root service goes through `dbus-daemon-launch-helper`, which the bus may
run only through the helper's `messagebus` group; kpkg installs every package
as `root:root`, and that group exists only because `dbus`'s `postinstall.sh`
sets it back. Starting these two directly keeps the machine's network
authorisation off that dependency. When activation fails it fails silently:
the service never starts, every call is refused, and the only trace is a
`Spawn.ExecFailed` in the log. `kdos doctor` checks the helper's owner, group
and mode for that reason.

Left to D-Bus activation, and so dependent on the helper: `wpa_supplicant`,
`fprintd`, `fwupd`, `boltd`, `upower`, `geoclue` and NetworkManager's
dispatcher.

### What this grants, stated plainly

Anybody in `wheel` reconfigures networking with no prompt. That is the same
group that can already power the machine off through `kdos-powerd` and write a
filesystem through `kdos-mountd`, so it is the existing boundary applied to
one more thing, not a new one. It is not a password prompt, because this
system cannot produce one; a design that pretended otherwise would be a
control that fails silently.

The list of action ids is the entire boundary, and the only record of its use
is a log line. NetworkManager is built `-Dlibaudit=no`, so an authorised
change leaves no audit record beyond NetworkManager's own message to syslog.

## Sandboxed clients

A client from a box connects to the compositor through a socket that
`kdos-boxsock` created for that box. The compositor tags every client on that
socket with a *security context* naming the box (engine `io.kdos.appbox`, the
box name as the app id); the client never sees the tag and cannot choose,
forge or drop it. The compositor's global filter then offers such clients a
**fixed allowlist** of Wayland protocols.

| Allowed | Denied |
|---|---|
| Surfaces, subsurfaces, shared memory, `wl_fixes` and the xdg shell | Screen capture, both generations (wlr screencopy; ext image-copy-capture and its output and toplevel sources) |
| The seat, relative pointer, pointer gestures, pointer constraints, cursor shape | Buffer export (wlr export-dmabuf) |
| Outputs and xdg-output, dmabuf and `wl_drm`, viewporter, presentation time, fractional scale | Data control (clipboard manipulation), both generations |
| Both decoration managers, activation, tablet, toplevel icon, dialog | Foreign-toplevel management and the ext toplevel list |
| **Text input** (text-input-v3) | **Input method and virtual keyboard** |
| The primary selection, and the ordinary data device (the clipboard and drag-and-drop) | Output management, output power, gamma control |
| Colour management, colour representation, alpha modifier, syncobj, single-pixel buffer | The layer shell, session lock, virtual pointer, idle notification |
| Idle inhibit, tearing control, xdg-foreign both ways | The security-context manager itself |

Three entries matter most:

- **Text input is allowed.** It is the *application* half of the input-method
  protocol; denying it would deny input methods to exactly the applications
  that need one most.
- **Input method and virtual keyboard are denied**, because a client that can be
  an input method receives every keystroke on the seat. That is a keylogger by
  design.
- **The security-context manager is denied** to anyone already carrying a
  context, so a box cannot mint a context of its own.

The capture denial is what makes the portal the sanctioned route. A boxed
screen recorder cannot bind the capture interfaces at all, so it must ask the
screen-cast portal, which runs on the host and asks you which output to share.
See [the session](session.md#portals).

### Granting a box past the allowlist

The fixed list is the right default but not the only answer: a screen
recorder in a box of its own could otherwise never be given the screen,
however deliberately. Add a `grant` line to the box's profile,
`~/.config/kdos/boxes/<name>.conf`:

```ini
grant = screencopy, data-control
```

Eight names can be granted. A name that is not in this map cannot be granted
at all; the map is the whole policy.

| Grant | Unlocks |
|---|---|
| `screencopy` | wlr screencopy, plus the ext image-copy-capture manager and its output source |
| `toplevel-capture` | The ext foreign-toplevel image-capture source |
| `export-dmabuf` | wlr export-dmabuf |
| `data-control` | Both generations of data control |
| `foreign-toplevel` | wlr foreign-toplevel management and the ext toplevel list |
| `layer-shell` | wlr layer shell |
| `input-method` | The input-method and virtual-keyboard managers |
| `output-power` | wlr output power management |

Where a protocol has two generations — screen copy, data control and the
toplevel list — the short name unlocks both, because granting the old
screen-copy protocol and not the new one would strand every client written
after 2024. `input-method` unlocks two related protocols, the input-method
and virtual-keyboard managers.

`input-method` must be spelled out in full and is kept conspicuous: granting
it hands that box every keystroke on the seat.

Grants are read once per box, at the first bind that reaches the filter, and
cached against the box name, because the filter runs for every global for
every client. `SIGHUP` to the compositor (its Reconfigure) drops the cache with
the rest of the configuration, so an edited profile takes effect for the next
client; a running one keeps what it already bound.

## Containers

Boxes are **rootless**: the container engine runs as you, maps your identity
into the container, and uses the setuid mapping helpers above for the one
privileged step.

- **Ownership inside a box grants nothing**, because packs are mounted `nosuid`.
- **Inside the box you are a mapped non-root user**, not mapped root. That is
  because applications refuse to run as root, not because it is a boundary: a
  process in your box is a process running as you.
- **A box is not a security boundary against you.** By default it shares your
  home directory in full (`home = private` in the profile gives it a home of
  its own). What it is, is a boundary against the *desktop's* interfaces — the
  compositor globals above — and a way of packaging software.

Box profiles are honest about what they enforce. Every key maps onto something
that is actually enforced, and the profile says what it cannot enforce: there
is no engine flag that grants a box a speaker and denies it a camera, so the
profile does not pretend to have one.

Resource limits (`memory =`, `cpus =`) become real cgroup limits when the
desktop was started by autologin: `kdos-getty` places that session in the
user's delegated cgroup (`/sys/fs/cgroup/user.slice/user-<uid>/session`), and
each box gets a sibling cgroup with `memory.max` set. A session started any
other way — a password login on a tty, an ssh login — stays in the root
cgroup, where podman accepts the limit and ignores it. In either case
`kdos-oomd` reads the profiles and prefers a box that is over its declared
`memory` budget as a victim.

**A pulled image is not verified.** `/etc/containers/policy.json` is
upstream's default, `insecureAcceptAnything`, which checks no signature. A base
image is trusted as far as the registry and the TLS connection to it are.

## Mount options

| Mounted | Options |
|---|---|
| Application packs | `ro,nosuid,nodev` |
| Data packs | `ro,nosuid,nodev,`**`noexec`**, and never composed into a box's root |
| Removable media (`kdos-mountd`) | `nosuid,nodev`, and `noexec` unless `exec = yes` is set in `/etc/kdos/mountd.conf` |
| `/tmp` | tmpfs, `mode=1777,nosuid,nodev` |
| `/run` | tmpfs, `mode=0755,nosuid,nodev` |

`exec = yes` is how you say you meant it. A setuid-root binary on a USB stick
from another machine is a local root hole that predates every other
consideration on this page, which is why `nosuid` is not configurable.

`/tmp` is one directory shared by every user and written by every root job —
init scripts, timers, package hooks. `/etc/sysctl.conf` sets
`fs.protected_symlinks`, `fs.protected_hardlinks` and `fs.protected_fifos` to 1
and `fs.protected_regular` to 2; the kernel's defaults are 0. At 0, a user who
plants a link, a FIFO or a file at a name a root job is about to write in a
sticky world-writable directory can turn that write onto any file on the
machine.

## Signing and trust

KDOS uses two keyrings, and keeping them apart is structural rather than a
convention:

| Directory | Attests | Used for | Shipped with |
|---|---|---|---|
| `/etc/kdos/keys` | Who built a host package | The binary-package host (binhost) index and package signature files | No key |
| `/etc/kdos/keys/packs` | Who exported an application pack | Pack indexes and packs' own signatures | `kdos-packs.pub` |

The keyring loader reads `*.pub` in one directory and does **not** descend into
subdirectories, so the two really are separate policies. A pack-signing key
placed in `/etc/kdos/keys` would silently become a trusted publisher of *host
packages* too.

The directory is the policy:

- **Adding trust** is copying a `.pub` file in; **removing it** is deleting one.
  There is no revocation list and no online check.
- **A key id is a label, not a credential.** Verification tries every key in the
  directory and nothing outside it, so a tool reports the key that actually
  verified, not the id the signature claimed.

No key ships for host packages, deliberately: a distribution that shipped its
own trusted key would be asking you to trust whoever built the image. Ports
are built from source and their integrity is the `sha256` in each recipe. To
trust a binhost you choose:

```sh
kpkg keygen builder                    # on the machine that builds
sudo cp builder.pub /etc/kdos/keys/    # on every machine that should trust it
```

`kpkg keygen builder` writes two files: `builder.key`, the secret half, which
stays on the build machine and must not be shared, and `builder.pub`, the
public half you copy.

The rest of the signing design is in [Packaging](packaging.md).

### TLS trust anchors

A third trust root, and not a keyring: the certificate authorities used for
TLS.

| File | What it is |
|---|---|
| `/usr/share/ca-certificates/mozilla.pem` | The Mozilla CA bundle, installed by `ca-certificates` |
| `/etc/ca-certificates/trust-source/anchors/` | Your own local roots |
| `/etc/ssl/cert.pem` | Generated: the Mozilla bundle followed by every local root |
| `/etc/ssl/certs/ca-certificates.crt`, `/etc/ssl/ca-bundle.crt` | Symlinks to `/etc/ssl/cert.pem` |

A program configured against any of the last three reads the same set.

The bundle is built, not downloaded ready-made. The `ca-certificates` port
pins `certdata.txt` at an NSS release tag (the port's version is that release,
3.130) and converts it with curl's `mk-ca-bundle.pl` from a pinned curl
release (8.22.0), taking the roots NSS trusts to issue server certificates:
121 in this `certdata.txt`. The converter drops any root already expired when
it runs, so a rebuild after a root expires ships one fewer.

| Library | How it finds the anchors |
|---|---|
| OpenSSL, and everything linked against it | Built `--openssldir=/etc/ssl`, so it reads `/etc/ssl/cert.pem` |
| GnuTLS, and everything linked against it | Through p11-kit's trust module, built `-D trust_paths=/usr/share/ca-certificates/mozilla.pem:/etc/ca-certificates/trust-source`. GnuTLS is built `--with-default-trust-store-pkcs11="pkcs11:"`, so p11-kit is its only source |
| Python code that calls `certifi.where()`, `requests` included | `python3-certifi`'s `certifi/cacert.pem` is a symlink to `/etc/ssl/cert.pem` |

The two sides see a new local root at different times:

- **GnuTLS sees it immediately.** p11-kit trusts every certificate in a trust
  path that is a file, and, for a trust path that is a directory, what is in
  its `anchors/` subdirectory.
- **OpenSSL sees it after `update-ca-certificates`.** That command rewrites
  `/etc/ssl/cert.pem` — the Mozilla bundle, then the certificate blocks of
  every PEM file in the anchors directory — and renames it into place so no
  reader sees half a file. `ca-certificates` runs it on every install and
  upgrade, so local roots survive an upgrade.

To trust a local certificate authority:

```sh
sudo cp root.crt /etc/ca-certificates/trust-source/anchors/   # must be PEM
sudo update-ca-certificates
```

A DER file in the anchors directory has no PEM block: `update-ca-certificates`
skips it with a warning, and GnuTLS alone then trusts it. `caddy trust` does
the whole job by itself: it writes Caddy's local root into the anchors
directory and runs `trust extract-compat`, which p11-kit hands to
`update-ca-certificates`. Firefox and other NSS programs keep their own store
and are not covered by any of this.

GnuTLS reads a system-wide priority policy from `/etc/gnutls/config` (the path
is compiled in from `--sysconfdir=/etc`; a policy anywhere else is ignored
without a warning). The image ships none, so every consumer uses the library's
`NORMAL` priorities; a file written there restricts or widens them for every
GnuTLS program at once.

The two halves fail independently, and the failure looks like a problem at the
other end. If the p11-kit trust path pointed at something the image does not
have, p11-kit would load nothing without a word (an absent path is not an
error to it), `gnutls_certificate_set_x509_system_trust()` would return zero
anchors, and every GnuTLS program would reject every server, while `curl` and
the rest of the OpenSSL side kept working. `msmtp`, `openconnect`, `weechat`
and chrony's NTS are the first to show it.

### An application is verified or it is not, and you can tell which

Applications arrive in two ways, with different guarantees, and neither is
hidden.

| | Built from the store | Imported |
|---|---|---|
| Bytes come from | Debian's archive (or another registry), over the network | A `.ktar` set somebody handed you |
| Signed by | Nothing in `/etc/kdos/keys` | A key in `/etc/kdos/keys/packs`, when the exporter had one |
| Checked by | Nothing KDOS controls | `kdos-packd`, when it takes the pack in: the signed index or the pack's signature, and the payload hash |
| Needs a network | Yes | No |

**A store build fetches content nobody here signed.** `kdos-box create` prints
exactly that before it pulls an OCI image, and it is true of every application
built this way. What apt itself verifies still holds — the archive's own GPG
signature over a pinned snapshot — but nothing this system controls attests to
the result, and the result is not signed afterwards.

**An imported pack is checked by the daemon that installs it**, not by the tool
that chose it. A client that verified a pack and then asked for an install
would have verified nothing. `kdos-packd` checks the pack against a signed
index in the staging directory when there is one, or else its own signature,
and hashes the payload, as it moves the pack into its store
(`/var/lib/kdos/packs`); only root can write the store afterwards. That is why import is safe over a channel that is not. A pack mounted
straight off an installation medium is checked when it is mounted, against its
own signature and payload hash only: a bad hash, a bad signature or a key not
in `/etc/kdos/keys` is refused, and an unsigned medium pack mounts.

**An unsigned export is still hash-checked and says it is unsigned.** The index
records a payload hash per pack whether or not a key was available, so a
tampered pack fails either way. What a signature adds is *who*, and an export
with no key prints that it added nothing. `kdos-box freeze` prints how to sign
its output (`kdos-pack sign <pack> <key.sec>`); `kdos-pack keygen <name>`
makes the key pair, as [Packs and boxes](packs-and-boxes.md#building-a-pack)
describes.

**Refusing unsigned packs is opt-in.** With `KDOS_REQUIRE_SIG` set in its
environment, `kdos-packd` refuses an unsigned pack at install (a medium pack is not
covered), and `kdos-pack` reports an
unsigned one as a failure. It does not cover an OCI image pulled from a
registry.

## Untrusted image bytes

Anything that can write to a terminal can reach an image decoder: a shell
script, a program inside a box, `cat` on a file somebody sent you. Three
escape sequences carry a picture — sixel (a DCS sequence), iTerm2's OSC 1337
and kitty's graphics protocol (an APC sequence) — and behind them are five
formats, each decoded by a large C library with a long memory-safety history:
sixel (libsixel), PNG (libpng), JPEG (libjpeg), WebP (libwebp) and GIF
(libnsgif). HEIF and AVIF are not accepted, because both would put a video
decoder on that path.

That is why `libkimg` is the only place KDOS decodes untrusted image bytes.
`kimg_decode_all()` is the one decode path (`kimg_decode()` is the same call
for a single frame), so there is one place to audit, one place the budget is
enforced, and one place a new format would be added. Its rules:

- **The budget is enforced before any allocation**, from the size the format
  itself declares. A length field is an allocation request from an untrusted
  peer: a decompression bomb is four lines of sixel, and a PNG claiming
  65535×65535 is eight bytes on the wire and sixteen gigabytes in memory.
  Refusing after decoding is not refusing.
- **A declared type that disagrees with the bytes is refused**, not re-sniffed.
- **Both chatty decoders are silenced.** libpng and libjpeg write to standard
  error by default, which would let untrusted bytes decide what appears on a
  terminal.
- **Every failure returns the same nothing.** The caller cannot tell a truncated
  file from an unsupported format from a budget refusal, on purpose: there is
  nothing useful to do differently, and a reason string in a log is a string an
  attacker chose.
- **It is tested under mutation.** `testing/selftest.sh` runs every fixture
  through the decoders with each byte changed in turn and truncated at every
  length; run it as `CC="cc -fsanitize=address,undefined -g" testing/selftest.sh`
  to do so under the address and undefined-behaviour sanitizers.

The terminal library `libkvt` adds its own limit before `libkimg` sees
anything:

- **The payload is capped, and a payload over the cap is dropped entirely**
  rather than truncated. Half an image is not a smaller image; it is a
  malformed file, which is exactly what an attacker would send. The cap is
  checked on every byte.
- **A consumer that registers no image callback leaves all three protocols
  off**: sixel goes to the ordinary DCS handling and an APC is ignored up to
  its terminator.

In `kdos-term` the relevant settings go in `~/.config/kdos/term.conf` (see
[kdos-term](../04-programs/kdos-term.md)):

| Key | Default | Range | Meaning |
|---|---|---|---|
| `images` | `yes` | `yes`, `no` | `no` decodes nothing and discards every payload (the parser's cap drops to 4 KiB); a kitty query is answered with a refusal so the program falls back at once |
| `image_max` | `1024` | 4–65536 | Largest single payload, in KiB |
| `image_cells` | `200` | 1–1000 | Largest picture, in cells along either side |

## A URI a terminal was told about

`OSC 8` marks a run of terminal text as a hyperlink. Following one is the one
place a terminal hands an address it was given by a child process to a program
that opens things. The reach is the same as an image payload: a shell script,
a program in a box, `cat` on a file somebody sent you. `libkvt` and `kdos-term`
apply these rules:

- **Four schemes and nothing else**: `http://`, `https://`, `file://`,
  `mailto:`. These are the ones whose worst case is a window appearing. A scheme
  handler is chosen by the `x-scheme-handler/<scheme>` MIME type, so an
  unlisted scheme would be a program of the attacker's choosing asked to start.
- **Every byte must be printable ASCII** (32–126), and an address is at most
  2048 bytes. A control byte would reach an argument vector, and a byte above
  126 lets the same address read two ways depending on who decodes it — which
  is how an allowlist gets walked around.
- **The address is refused when it is parsed**, not when it is clicked. What is
  not in the table cannot be followed by any path, including one written later.
- **The table holds at most 128 addresses per terminal.** A child emitting a new
  URI for every cell is the shape of the attack; past the cap, text is just
  text.
- **Only Ctrl+click follows a link**, so selecting a word never opens anything.
- **It runs as an argument vector, never a command line**:
  `kdos-appbox open <uri>`, with the URI as one element. There is no shell
  anywhere on the path.
- **A refused link is silent.** The characters draw normally, and nothing says
  "refused", because a message naming the address would put the attacker's
  string on screen.

## What is not protected

Stated plainly, because a reader who assumes otherwise is worse off than one
who knows.

- **There is no mandatory access control.** No SELinux, no AppArmor. A process
  running as you can do anything you can do.
- **There is no verified boot and no signed kernel.** The boot chain is not
  measured or attested. Someone with physical access and a moment alone with
  the machine owns it.
- **Disk encryption protects data at rest only.** It is a passphrase typed into
  the initramfs, not a TPM-sealed key, and it does nothing once the machine is
  running.
- **A box is not a jail.** By default it shares your home directory. A
  malicious application in a box can read and destroy your files exactly as a
  native one could. The sandbox limits what it can do to the *desktop*, not to
  your data.
- **`wheel` is effectively root.** `sudo` and polkit both grant it — polkit
  through `/etc/polkit-1/rules.d/50-kdos.rules`, unconditionally, because this
  system cannot ask for a password. With no session provider,
  `subject.active` and `subject.local` are always false, so a rule granting
  `wheel` covers a member logged in over SSH, or a background process running
  as them, exactly as it covers somebody at the console. Adding a session
  provider would not narrow it; the rule would have to be rewritten. Every root
  daemon answers `wheel` on every verb, so there is no separation between "can
  change the accent colour" and "can format a disk".
- **An OCI base pulled from a registry is unsigned content** from somebody else's
  server. It is an online operation, `KDOS_REQUIRE_SIG` does not cover it, and
  the tool says so before doing anything.
- **An unsigned pack is accepted** unless `KDOS_REQUIRE_SIG` is set. Only a
  signature that fails, or that names a key this machine does not have, is
  refused — so a pack that is signed but uncheckable is treated more harshly
  than the same pack with no signature block at all.
- **Ports built from source are not signed**, and need not be: a port's
  integrity is the checksum in its recipe. That makes the recipe — and so this
  repository — the trust root for everything on the host.
- **A granted box is as privileged as its grant.** `grant = input-method` hands
  that box every keystroke on the seat, and nothing warns at the moment a key
  is pressed. The profile is the only record.
- **Updates are never installed automatically, and there is no
  security-advisory service.** A system timer runs `kdos update check` once a
  day and records the result in `/var/lib/kdos/update.json`; newer versions
  arrive only with a newer ports tree, and installing them is
  `kdos update apply`, which you run. `kdos cve` tells you what is behind a
  known fix.
- **`kdos-oomd`'s choice between several large processes is untested.**
  `testing/oomd-fire.sh` exercises a single memory hog in a virtual machine;
  with several candidates, which one it kills has not been verified under
  real pressure.

## See also

- [Packaging](packaging.md) — signing, the index, and the three equality tests
- [Packs and boxes](packs-and-boxes.md) — verification, mount options, box profiles
- [The daemons](../04-programs/daemons.md) — each daemon's verbs and refusals
- [The session](session.md) — the portal route and what a box receives
- [Known gaps](../06-reference/known-gaps.md) — everything else that does not exist
