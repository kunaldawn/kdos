# The security model

This chapter describes who may do what on a KDOS machine, which mechanism enforces each rule, and
what is not protected at all. It is written for anyone who administers a KDOS machine, reviews its
security, or changes one of its privileged programs. It assumes the vocabulary of
[Architecture overview](overview.md); a *box* is a rootless container that runs an application,
and a *pack* is the read-only image a box is composed from (both are defined in the
[glossary](../06-reference/glossary.md) and described in [Packs and boxes](packs-and-boxes.md)).

The chapter runs in this order: the threat model, the setuid binaries and the two KDOS wrote,
the system accounts, the root daemons, the firewall, polkit, the containers and the compositor's
filter for boxed clients, mount options, signing and trust, and the two paths by which untrusted
bytes reach a terminal. Its last section, [What is not protected](#what-is-not-protected), is part
of the specification rather than a footnote: a model that lists only its defences gets relied on
for things it does not do. A reader with time for one section should read that one.

## What this model is for

KDOS is a single-user workstation. One human account ships (`kdos`, uid 1000), that account is in
the `wheel` group, and `wheel` is trusted. The design defends against four things:

- **Outside software misbehaving.** A browser, an office suite or an application from another
  distribution doing what a desktop application has no reason to do.
- **A tampered artefact.** A package, an index or an application image that is not what it claims
  to be.
- **Privilege escalation** through the small number of programs that need privilege.
- **A catastrophic mistake** by a `wheel` user, made through an interface that should never have
  let it be expressed.

It does not defend against the person at the keyboard, or against software running as that
person. A recurring pattern in what follows is that a mechanism is not made safe where it can be
made unnecessary: the lock screen needs no privileged daemon, `locate` needs no setgid binary, and
no root daemon accepts a path.

## setuid binaries

A *setuid* binary runs as its owner, here root, whoever starts it. The recipes in the ports tree
produce nineteen setuid-root binaries, two of them KDOS's own:

| Binary | From | What it is for |
|---|---|---|
| `kdos-checkpass` | KDOS | Checking the caller's own password against `/etc/shadow` for the lock screen |
| `kdos-resctl` | KDOS | Signalling and renicing another user's process for the resource monitor |
| `sudo` | sudo | Running a command as another user |
| `su` | shadow | Switching to another account. util-linux's `su` is not built (`--disable-su`), and toybox's is configured out |
| `passwd`, `chage`, `gpasswd`, `chfn`, `chsh`, `newgrp` | shadow | Account management |
| `newuidmap`, `newgidmap` | shadow | Writing a rootless container's user-namespace map |
| `pkexec`, `polkit-agent-helper-1` | polkit | polkit's privileged actions |
| `ssh-keysign` | OpenSSH | Host-based authentication |
| `dbus-daemon-launch-helper` | dbus | System-bus activation. Owned `root:messagebus`, mode `4110`, so only the bus daemon can run it |
| `mount.nfs` | nfs-utils | Mounting an NFS share listed in `fstab` as an ordinary user |
| `unix_chkpwd` | pam | Lets `pam_unix` check a password for a caller that is not root; without it every unprivileged PAM check fails, `vlock` and `wayvnc` included |
| `fusermount3` | fuse | Mounting a userspace filesystem from a session with no user namespace: sshfs, gocryptfs, fuse-overlayfs, the document portal, `rclone mount`, and `restic mount` through the `fusermount` link beside it |

Most of these bits come from the upstream `make install`. Four recipes set a bit themselves:
`kdos-lock` and `kdos-res` install their helpers with mode 4755, `shadow` repeats the mode on
`newuidmap` and `newgidmap` so that a configure change cannot drop it, and `pam` sets it on
`unix_chkpwd`. The `dbus` port's `postinstall.sh` restores the helper's group and mode after the
package manager has installed it as `root:root`.

`newuidmap` and `newgidmap` are the reason every boxed application starts. The container engine
(podman) runs them to write a process's user-namespace map, which the kernel allows only from a
process that already holds `CAP_SETUID`. Podman checks the binaries first; if they are not setuid
it exits with status 125 and prints `should have setuid or have filecaps setuid`. Without those two
bits no boxed application starts.

These are deliberately not setuid:

- util-linux's `mount` and `umount` (built `--disable-makeinstall-setuid`). Removable media go
  through `kdos-mountd` instead.
- `bwrap` (bubblewrap). It sandboxes through an unprivileged user namespace, which is how
  `xdg-desktop-portal` runs its validators.
- `ksu`, Kerberos's `su`. The `krb5` recipe deletes it: accounts are local and privilege
  escalation is `sudo`, so it would be an entry point nobody uses and nobody audits. Kerberos is
  present for `kinit`, an ordinary program.
- `pppd`, which NetworkManager starts as root, and `qemu-bridge-helper`, which serves only root.
- `wireshark`'s `dumpcap`, `bandwhich` and `trippy`. Run them as root or grant the capability
  yourself.
- `ping`. `/etc/sysctl.conf` sets `net.ipv4.ping_group_range` to `0 2147483647`, so every user may
  open an ICMP datagram socket and no privilege is needed.

The list above is derived from the recipes; the built image is the authority. To check a machine:

```sh
find / -xdev -perm -4000 -type f
```

KDOS uses mode bits rather than file capabilities because every host binary reaches the image
inside a kpkg package, and the package archive is a GNU-format tar written without `--xattrs`. A
mode bit survives that archive; a file capability, which is an extended attribute, does not. The
later copies (the squashfs system image, the installer's `rsync -aHAX`, and a pack built with
`mkfs.erofs` as root) would all carry an extended attribute, but by then it has already been lost.

Losing a setuid bit is a silent failure: one `chown` or one archive copy without the right flag
is enough, and nothing reports it. `kdos doctor` checks the five that matter most
(`kdos-checkpass`, `kdos-resctl`, `newuidmap`, `newgidmap`, and `dbus-daemon-launch-helper` with
its `messagebus` group) and prints the repair when one is wrong:
`chown root` and `chmod 4755` for the first four, `chown root:messagebus` and `chmod 4110` for the
bus helper. The same check confirms that `/etc/subuid` and `/etc/subgid` exist, because without
them no rootless box can map a user.

### No setgid binaries, and a per-user locate index

No recipe installs a setgid binary. util-linux is built `--disable-makeinstall-chown`, so `wall`
and `write` are not setgid `tty`; the games (`nethack`, `moon-buggy`, `bsd-games`) keep their
scores without a shared setgid directory. The check is the same `find` with `-perm -2000`.

`plocate` (the `locate` command) is the one program whose upstream design depends on setgid.
Upstream installs it setgid to a `plocate` group, with one shared database in `/var/lib` at mode
0640. That database names every path on the machine; the binary reads it on the caller's behalf
and filters out, per result, any path the caller could not have reached.

KDOS ships none of that: the recipe resets the binary to 0755 and removes `/var/lib/plocate`, and
there is no `plocate` group. Each user's index is built by that user, from their own home
directory, into their own cache:

| Piece | Where |
|---|---|
| The builder | `/usr/local/bin/kdos-updatedb`, which runs `/usr/sbin/updatedb --require-visibility no --prune-bind-mounts yes --database-root "$HOME" --output <index>` |
| The index | `${XDG_CACHE_HOME:-$HOME/.cache}/kdos/plocate.db` |
| The schedule | `~/.config/kdos/timers.d/20-updatedb.timer`, one of the per-user timers the session runs (seeded from `/etc/skel`) |
| How `locate` finds it | `$LOCATE_PATH`, exported by `/etc/profile.d/40-plocate.sh` |

An index built this way can contain only paths its owner could already list, so the visibility
check has nothing to guard. `--require-visibility no` is required: with the check on, `updatedb`
insists on the `plocate` group and writes nothing. `--prune-bind-mounts yes` keeps a bind-mounted
tree, such as a box's graft into the home directory, from being indexed twice.

The port carries one behavioural patch, `locate-path-replaces-default.patch`. Upstream searches
its compiled-in `/var/lib` database first and exits on the first database it cannot open, so on a
machine with no shared database every search would fail before reaching the user's own. With the
patch, a set and non-empty `$LOCATE_PATH` replaces the default instead of following it.

## kdos-checkpass

`kdos-checkpass` is the lock screen's password check, and the only privileged part of the lock
screen. If it is wrong, the user is locked out of their own session, so its behaviour is specified
exactly:

```sh
printf '%s' "$password" | kdos-checkpass    # exit 0 = correct
```

- It takes no arguments, not even a user name. It always checks the account of the caller's
  real user id, so it cannot be aimed at root or used as an oracle for another account.
- The password arrives on standard input, never on the command line, because a process's
  command line is readable by every user through `/proc/<pid>/cmdline` for as long as it runs. At
  most 511 bytes are read, and one trailing newline is stripped, so `printf '%s\n'` and
  `printf '%s'` mean the same thing.
- It reads `/etc/shadow` directly, with a plain file parse rather than `getspnam()`, so no
  name-service module runs as root.
- A locked (`!`, `*`) or empty hash is refused with exit 2 before any password is read, so an
  account with no password cannot be unlocked by pressing Enter.
- It drops root before hashing. Once the hash and the password are in memory it calls
  `setuid()` to the caller, and `crypt()` runs unprivileged.
- The comparison is constant time over the whole hash, and both buffers are cleared after use.
- A wrong password costs one second, spent inside `kdos-checkpass` itself, so a loop that drives
  it directly instead of through the lock screen is rate-limited too.

| Exit code | Meaning |
|---|---|
| 0 | Correct |
| 1 | Wrong |
| 2 | Could not tell: an argument was given, no passwd or shadow entry for the caller, an unreadable shadow file, a locked or empty hash, or an internal failure (reading standard input, dropping privilege, or `crypt()` refusing the hash) |

A caller must treat 2 as a failure and say why: reporting "wrong password" on a machine with a
broken shadow file sends the user looking in the wrong place. Without its setuid bit the program
cannot read the shadow file, answers 2 to every attempt, and locks the user out.

### The mode of `/etc/shadow`

`/etc/shadow` must be mode 0600, and the build has to set that explicitly. Git records one
permission bit (executable or not), so a file under `fs/` cannot carry a mode narrower than 644,
and the file-system build step gives every non-executable file exactly that. A world-readable
shadow file hands every password hash to every account and makes `kdos-checkpass`'s setuid bit
pointless.

`script/01_phase1/00_file_system.sh` therefore carries a table (the `FSMODES` block near its end)
of the paths whose mode or owner git cannot express:

| Path | Mode | Owner |
|---|---|---|
| `etc/shadow` | 600 | root |
| `etc/polkit-1/rules.d` | 755 | root |
| `etc/polkit-1/rules.d/50-kdos.rules` | 644 | root |
| `etc/udev/rules.d/` and everything under it | 644 (directories 755) | root |

A path in the table that is missing from the image is reported on standard error and skipped.
`testing/preflight.sh` checks the result on the built tree (`build/fs`), because the source tree
cannot express it: it accepts `/etc/shadow` at 600 or 640, and it requires the polkit and udev
rules and their directories to be owned by root. Both daemons read every rule they find with no
ownership check, so a rules directory the desktop user could write would let that user grant
themselves any polkit action, or run any command as root on the next device event through a udev
`RUN+=`.

## kdos-resctl

`kdos-resctl` is the second KDOS setuid binary, used by the resource monitor `kdos-res`. Its
security argument is that there is nothing to aim: three verbs, no paths and no options.

```
kdos-resctl dmi
kdos-resctl signal <pid> <TERM|KILL|STOP|CONT>
kdos-resctl renice <pid> <-20..19>
```

- Nothing in the argument vector is ever opened. The hardware-table path
  (`/sys/firmware/dmi/tables/DMI`) is compiled in.
- The caller must be in `wheel`, by real user id. The group is fixed at build time, never read
  from the environment. The effective user id is root by construction, so testing it would
  authorise everybody.
- Standard input, output and error are opened on `/dev/null` if they arrive closed, so output
  cannot land in a file the program opens later.
- It signals through a process handle (a pidfd) taken before the signal is sent, so the
  process it opened is the one it signals even if the number is reused in between. On a kernel
  without pidfd support it falls back to `kill(2)`, which has no such guarantee.
- Process ids 1 and below are refused. That covers init, and 0 and negative ids, which
  `kill(2)` reads as process groups.
- Root is dropped before the hardware table is parsed. The table is opened as root, then
  `setresuid()` returns to the caller, and the SMBIOS parser never runs privileged.
- It is never on the sampling path. `kdos-res` calls it only when you act on a process, and
  only for a process you do not own or a renice below zero; your own processes are signalled and
  reniced directly. When the helper is missing or has lost its setuid bit, `kdos-res` disables the
  action and puts the reason on the button.

The `dmi` verb prints the populated memory slots (size and speed, from SMBIOS type 17). No shipped
program calls it; the Memory page of `kdos-res` says that memory device details need
`kdos-resctl`.

| Exit code | Meaning |
|---|---|
| 0 | Done |
| 1 | Refused |
| 2 | Usage error (any other argument vector) |
| 3 | The operation itself failed |

## System accounts

Every daemon that drops privilege drops to an account of its own. None of these accounts can log
in: each has `/sbin/nologin` as its shell and `!` as its password. The image ships these in
`/etc/passwd`, `/etc/group` and `/etc/shadow`:

| Account | uid:gid | Used by |
|---|---|---|
| `dhcpcd` | 999:999 | `dhcpcd`'s privilege-separated children |
| `messagebus` | 997:997 | `dbus-daemon`, the system bus |
| `sshd` | 996:996 | `sshd`'s privilege separation |
| `tss` | 993:993 | The TPM: tpm2-tss's udev rules give `/dev/tpm*` to the user and `/dev/tpmrm*` to the group |
| `lp` | 10:10 | CUPS |
| `nobody` | 99:99 | Anything that asks for an unprivileged account by that name |

Ten more are created when their port is installed, by the port's `postinstall.sh` with
`groupadd -r` and `useradd -r`, which pick a free id: `polkitd`, `avahi`, `avahi-autoipd`,
`geoclue`, `mosquitto`, `nm-openvpn`, `pcscd`, `postgres`, `prosody` and `tcpdump`. `brltty`'s
hook creates the group `brlapi` and adds the desktop account to it; that is the group brltty's own
polkit rule admits to its braille display interface. The file-system build step merges these
files rather than overwriting them, so accounts a hook added survive a rebuild of `fs/`; where the
repository and the image disagree about an entry, the repository wins.

Two rules keep the tables consistent:

- Every id a shipped account uses has its own line in both `/etc/passwd` and `/etc/group`. A
  primary gid with no `/etc/group` line looks free to `groupadd -r`, which would hand it to another
  daemon's group.
- Every group a udev rule names must exist. eudev logs `specified group '<x>' unknown`,
  carries on with gid 0, and grants nothing. `testing/preflight.sh` checks the rules under `fs/`
  for this.

The desktop account belongs to `wheel`, `seat`, `lpadmin`, `audio`, `video`, `render`, `input`,
`kvm`, `cdrom`, `dialout`, `tty` and `users`. It is not in `tss`, so the TPM is root's: run the
`tpm2_*` tools under `sudo`.

## Root daemons

Five KDOS daemons run as root, each started by its own init script, and each answers a socket in
`/run`:

| Daemon | Socket | What it does |
|---|---|---|
| `kdos-powerd` | `/run/kdos-powerd.sock` | Suspend, power-off, reboot, and four system settings: the firewall, the autologin account, the accent, the time zone |
| `kdos-energyd` | `/run/kdos-energyd.sock` | Per-application energy use |
| `kdos-oomd` | `/run/kdos-oomd.sock` | Kills a process under memory pressure, sparing the desktop |
| `kdos-mountd` | `/run/kdos-mountd.sock` | Removable media, encrypted volumes, network shares, disk health |
| `kdos-packd` | `/run/kdos-packd.sock` | Mounts, verifies, installs and composes application packs |

Their verbs are in [the daemons](../04-programs/daemons.md). They share one authorisation design.

### Who may talk to them

The check is on who is connecting, not on the socket's permissions. Every socket is mode 0666 and
anyone may connect. The daemon asks the kernel for the connecting process's user id
(`SO_PEERCRED`, which the client cannot forge) and answers `err not permitted` to anyone it does
not admit. A mode that looked like the authorisation would be a mode somebody eventually loosens.

| Daemon | Admits |
|---|---|
| `kdos-powerd` | root and `wheel`: every verb. `seat`: `ping`, `suspend`, `poweroff` and `reboot` only |
| `kdos-mountd` | root, `seat` and `wheel`: every verb |
| `kdos-oomd` | root, `seat` and `wheel` (its only verbs are `ping` and `status`) |
| `kdos-energyd` | root and `wheel` |
| `kdos-packd` | root and `wheel` |

`seat` is the group to which seatd, the seat daemon, hands the display and input devices: it
means the person at the machine. The installer keeps the desktop account in `seat` and, for a
non-administrator, takes it out of `wheel`. Such an account keeps the lid and the power keys and
every `kdos-mountd` verb, and loses `sudo`, the polkit grants and every `kdos-powerd`
configuration verb. `kdos-powerd`'s `seat` list names what is allowed, so a configuration verb
added later belongs to administrators without anyone having to remember to say so.

The test lives in one function, `kb_uid_allowed()` in `libkbase`, and no daemon keeps a copy of
it. It admits uid 0, refuses a uid with no passwd entry, and otherwise checks the account's
primary and supplementary groups. This is the authorisation boundary of the whole system; five
copies would be a rule tightened on one socket and left loose on the others. To see how
`kdos-powerd` would treat an account:

```sh
kdos-powerd --explain <user>
```

It asks the same function the socket does, so its answer cannot drift from the daemon's.

`kdos-mountd --fixture-serve` is a test mode that admits anybody and grants nothing: its paths are
a scratch directory, and every command it would run is printed instead. The init script starts
the daemon with no arguments, so a running system never reaches it.

### Clients never name a path

Every block-device verb takes an index into a list the daemon published a moment earlier, and
every configuration verb takes a name from a list compiled into it. A daemon that accepted a device
and a mount point would let anyone at the seat mount a USB stick over `/etc`.

There are two exceptions. Installing a pack names a file name, not a path, in a staging directory
the daemon owns (`/var/lib/kdos/packs/staging`, mode 01777). The name must match `[A-Za-z0-9._-]`
and contain no slash. That directory is the single place an unprivileged download may land, and
the daemon publishes its location so clients do not derive it. `kdos-mountd`'s `cifs` and `krb5`
verbs name a server, a share, a user and a domain, because a network share is not a row the daemon
can list in advance. The daemon still chooses the mount point (under `/media/<user>/`) and the
options, and it checks each field's characters, and resolves the server, before anything is
created or mounted.

### What each daemon refuses

| Daemon | Refuses |
|---|---|
| `kdos-powerd` | Anything but `ping`, `suspend`, `poweroff`, `reboot`, and four verbs whose argument is checked before use: `firewall` (a service name from a table compiled into the daemon), `autologin` (an existing human account, or `off`), `accent` (one of the eight colour schemes in `libkcolor`'s table), `timezone` (a zone name of at most 64 characters from `[A-Za-z0-9+_/-]`, with no leading, trailing or doubled slash) |
| `kdos-mountd` | A block device that is not removable, with the checks listed below the table; a network share is mounted only through `cifs` or `krb5`, at a mount point the daemon chooses |
| `kdos-packd` | Paths as arguments; a staged name outside `[A-Za-z0-9._-]`; a pack whose hash or signature fails; replacing or removing a pack that is composed into a running box |
| `kdos-oomd` | Any argument at all: killing is its own decision or it does not happen |
| `kdos-energyd` | Republishing the raw energy counter; a client-chosen sampling interval |

`kdos-mountd` refuses:

- internal disks, loop, RAM, zram and device-mapper nodes, filesystems it does not recognise, and
  anything listed in `fstab`;
- every partition of the disk the system booted from;
- anything currently mounted, for a verb that writes;
- a verb carrying a token nobody issued, and a device index that is not a number;
- a device node that differs from the one the scan recorded;
- any format request unless `format = yes` is set in `/etc/kdos/mountd.conf`. With it set, a
  format request must repeat the device's kernel name (for example `sdb1`) exactly as
  confirmation, and only `ext4`, `btrfs`, `vfat` and `exfat` are written.

The image does not ship `/etc/kdos/mountd.conf`, so formatting is off, and removable media mount
`noexec`, until an administrator writes the file.

`kdos-energyd` exists because the CPU energy counter (RAPL) is root-only: fine-grained
unprivileged reads can recover cryptographic keys through a side channel. What leaves the daemon
is a per-application percentage accumulated over minutes. It samples every ten seconds, an
interval compiled in and not configurable, the raw counter is never republished, and a client
cannot drive the interval towards a fine-grained one. There is no write path into the power
interface at all.

`kdos-boxsock` is not a root daemon: it is installed in `/usr/bin`, runs as the desktop user,
binds one tagged Wayland socket per box, and holds that box's security context open for the box's
lifetime.

## The firewall

The firewall is nftables, loaded at boot by `/etc/init.d/25_nftables.sh` from `/etc/nftables.conf`.
The script checks the whole file with `nft -c` first and keeps the running ruleset if the check
fails, because a ruleset applied halfway would leave the input chain's drop policy in place with
its accept rules missing.

The shipped `inet filter` table:

| Chain | Policy | Accepted |
|---|---|---|
| `input` | drop | Established and related traffic; loopback; the ICMP and ICMPv6 types that keep a network working (unreachable, too big, time exceeded, parameter problem, echo request, and IPv6 neighbour and router discovery); mDNS (UDP 5353); NetBIOS name service (UDP from port 137 to port 137); the DHCPv6 client (UDP 546); DNS and DHCP from `10.42.0.0/16`, the range NetworkManager gives a hotspot's clients |
| `forward` | drop | Established and related traffic, and anything to or from `10.42.0.0/16` |
| `output` | accept | Everything |

Every `*.nft` file in `/etc/nftables.d/` is included after it. `40-podman.nft` lets containers on
a `podman*` bridge reach DNS and be forwarded. `50-kdos-services.nft` is written by `kdos-powerd`
and holds the services an administrator has opened.

Opening a port goes through the same rule as every other daemon verb: the client names a service,
never a port. `kdos-powerd` carries the table of thirteen names (`ssh`, `http`, `https`, `ipp`,
`smb`, `kiwix`, `mdns`, `mqtt`, `xmpp`, `nfs`, `caddy`, `mosh`, `syncthing`), each with the one
rule it adds, so a client that can only say `ssh` can open exactly TCP 22 and nothing else. The
daemon rewrites `50-kdos-services.nft` whole from the names that are on, checks the full ruleset
with `nft --check`, and then reloads it. A rule written by hand into that file is dropped on the
next change; a hand-written rule belongs in a file of its own beside it.

```sh
kdos-power firewall list          # root or wheel
kdos-power firewall ssh on
```

## polkit, and why the desktop has no authentication agent

*polkit* is the system service that other daemons ask whether a user may perform an action. On a
mainstream desktop it raises a password prompt through an *authentication agent*. KDOS has no
agent and no prompt, for reasons that follow from having no session manager.

### Who asks polkit

polkit is installed and `polkitd` is started by the init system. NetworkManager, ModemManager,
`bolt`, `fwupd`, `upower`, `fprintd`, `pcscd` and brltty's braille server ask it, and GeoClue asks
it on ModemManager's behalf. NetworkManager's actions are the only ones a KDOS program calls.

### Why a password prompt cannot work here

polkit never sees an *active* session on this machine. It finds a process's session by calling
`org.freedesktop.ConsoleKit` on the system bus; neither ConsoleKit nor elogind is installed (see
[principles](../01-philosophy/principles.md#no-systemd)), so the lookup fails and every check falls
to the `allow_any` column of the action's `.policy` file. The `allow_active` and `allow_inactive`
columns are never read.

An action with no `<allow_any>` element is then a flat refusal. polkit consults an agent only
when the result is a *challenge*, and a flat refusal is not one. NetworkManager's policy has no
`<allow_any>` for `enable-disable-wifi`, `enable-disable-network` or either `wifi.share` action, so
no agent (one written for KDOS, or the `pkttyagent` polkit ships) could ever be asked about the
Wi-Fi switch. An agent would also have nothing to register as: with no session it can register
only for one exact process, matched by pid and start time, so a session-long agent would never be
found for a program it did not start itself.

The same applies to `bolt`, `fwupd` and `upower`. Their actions are `auth_admin`, a challenge, and
a challenge with no agent is a refusal. `bolt` and `fwupd` ship their own rules granting `wheel`,
but both test `subject.active` and `subject.local`, which are never true here, so neither fires.
`fprintd`'s enrol and verify actions are `allow_active` only. A Thunderbolt enrolment, a firmware
update or a fingerprint enrolment is therefore done with `sudo`; polkit authorises root for every
action without reading a rule.

### What is granted instead: `50-kdos.rules`

`/etc/polkit-1/rules.d/50-kdos.rules` grants a short, named list of actions to members of
`wheel`, and nothing to anyone else:

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

Deliberately not granted, and so left to `sudo`:

| Action | Why not |
|---|---|
| `settings.modify.hostname` | The hostname belongs to the installer and `/etc/hostname` |
| `settings.modify.global-dns` | A machine-wide resolver is not a per-user decision |
| `checkpoint-rollback` | Nothing here creates a checkpoint, so a rollback could only undo somebody else's |
| `sleep-wake` | `kdos-powerd` owns suspend, with its own check |
| `reload` | Re-reading NetworkManager's configuration is administration |

Two further consumers work without a KDOS rule. brltty's own rule grants
`org.a11y.brlapi.write-display` to the `brlapi` group with no session test. GeoClue's rule grants
ModemManager's `Device.Control` and `Location` to the `geoclue` account alone, so the location
service can switch on a modem's GPS; nobody logs in as that account.

The grant is a rules file of named actions, not a D-Bus policy that lets `wheel` onto
NetworkManager's write interfaces, because a bus policy cannot scope a `Properties.Set` to one
property and cannot leave out the hostname or the machine-wide resolver. The rules file and an
agent are alternatives: if an agent were ever added, this file is what would have to go.

### Why `settings.modify.system` is granted

On this build it is the only permission a saved network can be under. NetworkManager is compiled
`-Dsession_tracking=no`, so its check "does this user have a session?" always answers no. A
connection profile that names an owner in its `permissions` list is therefore permanently
invisible, and an invisible profile never connects automatically: a Wi-Fi network you joined would
not come back after a reboot. So `kdos-net` writes no owner, everything it creates is a system
connection, and forgetting one or reading its passphrase back needs `settings.modify.system`.

That grant lets anybody in `wheel` read every stored Wi-Fi passphrase. It is a shortcut, not a new
capability: the shipped sudoers line (`/etc/sudoers.d/00-sudo`) is `%wheel ALL=(ALL) ALL`, and the
passphrases are files under `/etc/NetworkManager` that `sudo cat` prints.

### How polkit and ModemManager are started

`polkitd` is started by `/etc/init.d/41_polkitd.sh` and ModemManager by
`/etc/init.d/42_modemmanager.sh`, rather than by D-Bus activation. Activation of a root service
goes through `dbus-daemon-launch-helper`, which the bus may run only through the helper's
`messagebus` group; kpkg installs every package as `root:root`, and that group is on the helper
only because `dbus`'s `postinstall.sh` sets it back. Starting these two directly keeps the
machine's network authorisation off that dependency. When activation fails it fails silently: the
service never starts, every call is refused, and the only trace is a `Spawn.ExecFailed` in the
log. `kdos doctor` checks the helper's owner, group and mode for that reason.

Left to D-Bus activation, and so dependent on the helper: `wpa_supplicant`, `fprintd`, `fwupd`,
`boltd`, `upower`, `geoclue` and NetworkManager's dispatcher.

### What the grant allows

Anybody in `wheel` reconfigures networking with no prompt. That is the same group that can already
change the firewall through `kdos-powerd`, so it is the existing boundary applied to one more thing.
It is not a password prompt, because this system cannot produce one; a design that pretended
otherwise would be a control that fails silently.

The list of action ids is the entire boundary, and the only record of its use is a log line.
NetworkManager is built `-Dlibaudit=no`, so an authorised change leaves no audit record beyond
NetworkManager's own message to syslog.

## Containers

Boxes are rootless: the container engine runs as you, maps your identity into the container,
and uses the setuid mapping helpers above for the one privileged step. A pack box is created with
`podman create --rootfs` over the overlay `kdos-packd` composed, with the flags distrobox uses for
the same job. A box built from an OCI image (a container image in the Open Container Initiative
format, as a registry such as Docker Hub serves it) goes through distrobox itself, the upstream
tool that wraps podman to make a container behave like part of the host; it also shares the host's
root at `/run/host`.

- Inside the box you are a mapped non-root user (`--userns keep-id`: uid 1000 inside is uid
  1000 outside). That is because applications refuse to run as root, not because it is a
  boundary: a process in your box is a process running as you.
- Ownership inside a box grants nothing, because packs are mounted `nosuid`.
- No mandatory access control applies. Boxes are created with `label=disable` and
  `apparmor=unconfined`; the host runs neither SELinux nor AppArmor.

What a box shares with the host is set by its profile, `~/.config/kdos/boxes/<name>.conf`. For a
pack box each key below maps onto one podman flag that the kernel enforces, and applies when the
box is created. For a box built from an OCI image, `memory`, `cpus` and `pids` are not applied,
and `network = none` gives a private namespace with an interface, as `private` does.
`kdos-box profile` prints the podman flag for those keys whichever kind of box it is.

| Key | Default | Private or `none` means |
|---|---|---|
| `home` | shared: your whole `$HOME` | A home directory of the box's own |
| `network` | `host`: the host's network namespace | `private`: a namespace of its own; `none`: no interface at all |
| `ipc` | shared with the host | A private IPC namespace |
| `processes` | the host's pid namespace | A private pid namespace |
| `devices` | `/dev` and `/sys` bind-mounted from the host | No host `/dev`; `gpu = yes` binds `/dev/dri` back alone |
| `gpu` | `yes` | With `devices = private`, `yes` binds `/dev/dri` back and `no` leaves the box without it; with `devices` shared it follows `devices` |
| `audio` | `yes` | Not enforced: sound reaches the box through the PipeWire socket in `/run/user/<uid>`, which every box shares |
| `memory`, `cpus`, `pids` | unlimited | `--memory`, `--cpus`, `--pids-limit` |

Whatever the profile says, every box shares `/tmp`, `/dev/shm`, `/run/user/<uid>` (the session
bus, the PipeWire socket and the session's Wayland socket live there) and, when it exists,
`/run/cups`. A box is therefore not a security boundary against you. It is a boundary against
some of the desktop's interfaces (the compositor globals in
[Sandboxed clients](#sandboxed-clients)) and a way of packaging software. There is no engine flag
that grants a box a speaker and denies it a camera, so `gpu = no` cannot be enforced while
`devices` is shared, and `audio = no` is never enforced, because the PipeWire socket is in the
shared runtime directory. `kdos-box profile <name>` and `kdos-box create` print a warning beside
`audio` and `gpu` only while `devices` is shared; with `devices = private`, `audio = no` is
accepted without a warning and changes nothing.

Resource limits (`memory =`, `cpus =`) become real cgroup limits when the desktop was started by
autologin: `kdos-getty` places that session in the user's delegated cgroup
(`/sys/fs/cgroup/user.slice/user-<uid>/session`), and each pack box gets a sibling cgroup with
`memory.max` set. A session started any other way (a password login on a tty, an ssh login) stays
in the root cgroup, where podman accepts the limit and ignores it. In either case `kdos-oomd`
reads the profiles and prefers a box that is over its declared `memory` budget as a victim.

A pulled image is not verified. `/etc/containers/policy.json`, written by the
`containers-common` port, is upstream's default, `insecureAcceptAnything`, which checks no
signature. `/etc/containers/registries.conf` resolves an unqualified image name against
`docker.io`. A base image is trusted as far as the registry and the TLS connection to it are.

## Sandboxed clients

A client in a box is meant to reach the compositor through a socket that `kdos-boxsock` created
for that box, `$XDG_RUNTIME_DIR/kdos-box-<box>@<display>.sock`; `kdos-appbox` sets
`WAYLAND_DISPLAY` to it before anything in the box starts. The compositor tags every client on
that socket with a *security context* naming the box (engine `io.kdos.appbox`, the box name as the
app id). The client cannot see, choose or forge the tag. The compositor's global filter then
offers such clients a fixed allowlist of Wayland protocols:

| Allowed | Denied |
|---|---|
| Surfaces, subsurfaces, shared memory, `wl_fixes` and the xdg shell | Screen capture, both generations (wlr screencopy; ext image-copy-capture and its output and toplevel sources) |
| The seat, relative pointer, pointer gestures, pointer constraints, cursor shape | Buffer export (wlr export-dmabuf) |
| Outputs and xdg-output, dmabuf and `wl_drm`, viewporter, presentation time, fractional scale | Data control (clipboard manipulation), both generations |
| Both decoration managers, activation, tablet, toplevel icon, dialog | Foreign-toplevel management and the ext toplevel list |
| Text input (text-input-v3) | Input method and virtual keyboard |
| The primary selection, and the ordinary data device (the clipboard and drag-and-drop) | Output management, output power, gamma control, DRM leasing |
| Colour management, colour representation, alpha modifier, syncobj, single-pixel buffer | The layer shell, session lock, virtual pointer, idle notification, workspaces |
| Idle inhibit, tearing control, xdg-foreign both ways | The security-context manager itself |

The list is an allowlist, so a protocol the compositor gains later is denied to boxes until
somebody adds it. Three entries matter most:

- Text input is allowed. It is the application half of the input-method protocol; denying it
  would deny input methods to exactly the applications that need one most.
- Input method and virtual keyboard are denied, because a client that can be an input method
  receives every keystroke on the seat.
- The security-context manager is denied to any client already carrying a context, and no
  grant unlocks it, so a box cannot mint a context of its own.

The capture denial is what makes the portal the sanctioned route. A boxed screen recorder cannot
bind the capture interfaces through its tagged socket, so it asks the screen-cast portal, which
runs on the host and asks you which output to share. See [the session](session.md#portals).

A client with no security context, which is every program started on the host, is offered every
global. The compositor also supports the `<privilegedInterfaces>` list of labwc (the compositor
kdos-comp is built from) in `rc.xml`, which would restrict the privileged protocols for all
clients, but the shipped configuration does not set it.

The tag holds only for a client that uses the socket it was given. A box shares
`$XDG_RUNTIME_DIR` with the session (see [Containers](#containers) above), and the session's own
`wayland-0` socket is in it, so a program in a box that opens `wayland-0` by name connects
untagged and is offered everything. The profile key `wayland = no` cannot take the display away
for the same reason, and `kdos-box profile` prints that it is not enforced. The filter stops a
well-behaved application from being handed capture, clipboard or input-method globals it did not
ask for; it does not contain a hostile one.

### Granting a box past the allowlist

The fixed list is the default, not the only answer: without a grant, a screen recorder in a box of
its own could never be given the screen, even by the person who installed it. Add a `grant` line to
the box's profile, `~/.config/kdos/boxes/<name>.conf`:

```ini
grant = screencopy, data-control
```

Eight names can be granted. A name that is not in this map grants nothing; the map is the whole
policy.

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

Where a protocol has two generations (screen copy, data control and the toplevel list) the short
name unlocks both, because granting the older protocol and not the newer one would strand every
client written against the newer.

Names are separated by commas or spaces; the first `grant` line in the file is read and at most
sixteen names are taken from it. The compositor reads the file from `$HOME/.config`, not from
`$XDG_CONFIG_HOME`, because `kdos-appbox` resolves the profile the same way and the two must not
disagree. Grants are read at the first bind that reaches the filter and cached against the box
name, because the filter runs for every global for every client. The cache holds eight boxes;
when a ninth arrives the whole cache is emptied, and every box's profile is read again at its next
bind. `SIGHUP` to the compositor (its Reconfigure) drops the cache with the rest of the
configuration, so an edited profile takes effect for the next client; a running one keeps what it
already bound.

## Mount options

| Mounted | Options |
|---|---|
| Application packs | `ro,nosuid,nodev` |
| Data packs | `ro,nosuid,nodev,noexec`, and never composed into a box's root |
| Removable media (`kdos-mountd`) | `nosuid,nodev`, and `noexec` unless `exec = yes` is set in `/etc/kdos/mountd.conf` |
| `/tmp` | tmpfs, `mode=1777,nosuid,nodev` |
| `/run` | tmpfs, `mode=0755,nosuid,nodev` |
| `/sys/kernel/tracing`, `/sys/kernel/debug` | `nosuid,nodev,noexec` |
| `/sys/fs/bpf` | `nosuid,nodev,noexec,mode=0700` |

`exec = yes` is how an administrator says executables on removable media were meant to run.
`nosuid` is not configurable: a setuid-root binary on a USB stick from another machine would give
any local user root.

`/tmp` is one directory shared by every user and written by every root job: init scripts, timers
and package hooks. `/etc/sysctl.conf` sets `fs.protected_symlinks`, `fs.protected_hardlinks` and
`fs.protected_fifos` to 1 and `fs.protected_regular` to 2; the kernel's own defaults are 0. At 0, a
user who plants a link, a FIFO or a file at a name a root job is about to write in a sticky
world-writable directory can turn that write onto any file on the machine.

## Signing and trust

KDOS uses two keyrings, and keeping them apart is structural rather than a convention:

| Directory | Attests | Used for | Shipped with |
|---|---|---|---|
| `/etc/kdos/keys` | Who built a host package | The binary-package host (binhost) index and package signature files | No key |
| `/etc/kdos/keys/packs` | That packs came from the same bake as the medium (the build run that produced them) | Pack indexes and packs' own signatures, read by `kdos-packd` and `kdos-pack verify` | `kdos-packs.pub` |

The keyring loader reads `*.pub` in one directory and does not descend into subdirectories, so the
two are separate policies. A pack-signing key placed in `/etc/kdos/keys` would silently become a
trusted publisher of host packages too.

The directory is the policy:

- Adding trust is copying a `.pub` file in; removing it is deleting one. There is no
  revocation list and no online check.
- A key id is a label, not a credential. Verification tries every key in the directory and
  nothing outside it, so a tool reports the key that actually verified, not the id the signature
  claimed.

No key ships for host packages, deliberately: a distribution that shipped its own trusted key
would be asking you to trust whoever built the image. Ports are built from source and their
integrity is the `sha256` in each recipe, checked when the build fetches it (see
[How KDOS is built](../05-developer/how-kdos-is-built.md#fetching-the-only-step-that-uses-the-network)).
To trust a binhost of your choosing:

```sh
kpkg keygen builder                    # on the machine that builds
sudo cp builder.pub /etc/kdos/keys/    # on every machine that should trust it
```

`kpkg keygen builder` writes an Ed25519 key pair as two files: `builder.key`, the secret half,
which stays on the build machine, and `builder.pub`, the public half you copy.

The key that ships in `/etc/kdos/keys/packs` asserts only that the packs beside it on a medium
came from the same bake, the build run that produced the medium (see
[Packs and boxes](packs-and-boxes.md)). To make it say something about you, replace it and
re-sign. The rest of the signing design is in [Packaging](packaging.md).

### TLS trust anchors

The certificate authorities used for TLS are a third trust root, kept apart from both keyrings.

| File | What it is |
|---|---|
| `/usr/share/ca-certificates/mozilla.pem` | The Mozilla CA bundle, installed by `ca-certificates` |
| `/etc/ca-certificates/trust-source/anchors/` | Your own local roots |
| `/etc/ssl/cert.pem` | Generated: the Mozilla bundle followed by every local root |
| `/etc/ssl/certs/ca-certificates.crt`, `/etc/ssl/ca-bundle.crt` | Symlinks to `/etc/ssl/cert.pem` |

A program configured against any of the last three reads the same set.

The bundle is built, not downloaded ready-made. The `ca-certificates` port pins `certdata.txt` at
an NSS release tag (the port's version is that release, 3.130) and converts it with curl's
`mk-ca-bundle.pl` from a pinned curl release (8.22.0), taking the roots NSS trusts to issue server
certificates: 121 in this `certdata.txt`. The converter drops any root already expired when it
runs, so a rebuild after a root expires ships one fewer.

| Library | How it finds the anchors |
|---|---|
| OpenSSL, and everything linked against it | Built `--openssldir=/etc/ssl`, so it reads `/etc/ssl/cert.pem` |
| GnuTLS, and everything linked against it | Through p11-kit's trust module, built `-D trust_paths=/usr/share/ca-certificates/mozilla.pem:/etc/ca-certificates/trust-source`. GnuTLS is built `--with-default-trust-store-pkcs11="pkcs11:"`, so p11-kit is its only source |
| Python code that calls `certifi.where()`, `requests` included | `python3-certifi`'s `certifi/cacert.pem` is a symlink to `/etc/ssl/cert.pem` |

The two sides see a new local root at different times:

- GnuTLS sees it immediately. p11-kit trusts every certificate in a trust path that is a file,
  and, for a trust path that is a directory, what is in its `anchors/` subdirectory.
- OpenSSL sees it after `update-ca-certificates`. That command rewrites `/etc/ssl/cert.pem`
  (the Mozilla bundle, then the certificate blocks of every PEM file in the anchors directory) and
  renames it into place so no reader sees half a file. The `ca-certificates` port runs it on every
  install and upgrade, so local roots survive an upgrade.

To trust a local certificate authority:

```sh
sudo cp root.crt /etc/ca-certificates/trust-source/anchors/   # must be PEM
sudo update-ca-certificates
```

A DER file in the anchors directory has no PEM block: `update-ca-certificates` skips it with a
warning, and then GnuTLS alone trusts it. `caddy trust` does the whole job by itself: it writes
Caddy's local root into the anchors directory and runs `trust extract-compat`, which the `p11-kit`
port replaces with a call to `update-ca-certificates`. Firefox and other NSS programs keep their
own store and are not covered by any of this.

GnuTLS reads a system-wide priority policy from `/etc/gnutls/config` (the path is compiled in from
`--sysconfdir=/etc`; a policy anywhere else is ignored without a warning). The image ships none,
so every consumer uses the library's `NORMAL` priorities; a file written there restricts or widens
them for every GnuTLS program at once.

The two halves fail independently, and the failure looks like a problem at the other end. If the
p11-kit trust path pointed at something the image does not have, p11-kit would load nothing without
a word (an absent path is not an error to it), `gnutls_certificate_set_x509_system_trust()` would
return zero anchors, and every GnuTLS program would reject every server while `curl` and the rest of
the OpenSSL side kept working. `msmtp`, `openconnect`, `weechat` and chrony's NTS are the first to
show it.

### How an application is verified

Applications arrive in two ways, with different guarantees, and neither is hidden. The *store
lane* builds a container image from the catalogue on this machine, over the network; the *import
lane* installs a pack somebody else exported (see *lane* in the
[glossary](../06-reference/glossary.md), and [Packs and boxes](packs-and-boxes.md#two-lanes-one-box)).

| | Built by the store lane | Imported |
|---|---|---|
| Bytes come from | Debian's archive (or another registry), over the network | A `.ktar` set made by `kdos app export`, or a pack from `kdos-box freeze` |
| Signed by | Nothing in `/etc/kdos/keys` | A `kdos-box freeze` pack: its own signature block, when someone ran `kdos-pack sign`. A `.ktar` set: nothing the import checks (see below) |
| Checked by | Nothing KDOS controls | `kdos-packd`, when it takes the pack in |
| Needs a network | Yes | No |

A store-lane build fetches content nobody here signed. `kdos-box create` says so before it pulls an
OCI image, and it is true of every application built this way. What apt itself verifies still
holds (the archive's own GPG signature over a pinned snapshot) but nothing this system controls
attests to the result, and the result is not signed afterwards.

An imported pack is checked by the daemon that installs it, not by the tool that chose it. A
client that verified a pack and then asked for an install would have verified nothing.
`kdos-packd` works through these checks when it moves a staged pack into its pack directory
(`/var/lib/kdos/packs`):

1. If the staging directory (`/var/lib/kdos/packs/staging`) holds an index (`PACKAGES`) whose
   signature verifies against `/etc/kdos/keys/packs` and which lists the pack, the pack's SHA-256
   must match the index entry. A match is accepted; a mismatch is refused. Neither
   `kdos-appbox import` nor `kdos-box import` puts an index there: each copies only the `.kpack`
   file it installs, so this check applies only when an index was placed in the staging directory
   by other means.
2. Otherwise the pack's own footer is checked: the payload must hash to the value the footer
   records, and then the pack's signature block, if it has one, must verify against the same ring.
   A bad hash, a bad signature, or a signature with no key on this machine to check it is refused.
3. A pack with no signature block and a correct payload hash is accepted and logged as unsigned,
   unless `KDOS_REQUIRE_SIG` is set in the daemon's environment.

Only root can write the pack directory afterwards, so an installed pack is not re-hashed on every
mount. A pack mounted straight off an installation medium has never been copied into the pack
directory, so it is checked when it is first mounted, against its own payload hash and signature
only; a bad hash, a bad signature or a signing key not in `/etc/kdos/keys/packs` is refused, and an
unsigned medium pack mounts.

An unsigned pack is protected against corruption, not against tampering. Its payload hash is
recorded in its own footer, which whoever alters the payload can rewrite. An unsigned index does
not help: `kdos-packd` uses an index only when its signature verifies. What a signature adds is
who made the pack. `kdos-box freeze` prints how to sign its output (`kdos-pack sign <pack> <key>`),
and `kdos-pack keygen <name>` makes the key pair (`<name>.key` and `<name>.pub`), as
[Packs and boxes](packs-and-boxes.md#building-a-pack) describes; a signed pack's block is checked
at install by step 2. `kdos app export` signs the set's index (`PACKAGES.sig` beside `PACKAGES`)
when `KDOS_PACK_KEY` names a readable secret key and otherwise says that the index is unsigned, but
it does not sign the packs themselves, and `kdos app import` stages each pack without the index.
Every pack of an exported set therefore reaches `kdos-packd` as an unsigned pack: it is accepted
on its payload hash alone, and refused outright when `KDOS_REQUIRE_SIG` is set, whether or not the
exporter had a key.

Refusing unsigned packs is opt-in. With `KDOS_REQUIRE_SIG` set, `kdos-packd` refuses an unsigned
pack at install (a medium pack is not covered), and `kdos-pack verify` reports an unsigned one as a
failure. It does not cover an OCI image pulled from a registry. `kdos-pack` reads the variable from
the caller's environment. `kdos-packd` reads it from its own, and the shipped init script,
`/etc/init.d/59_packd.sh`, sets none, so turning it on for installs means exporting the variable in
that script before its `supervise` line and restarting the daemon.

## Untrusted image bytes

Anything that can write to a terminal can reach an image decoder: a shell script, a program inside
a box, `cat` on a file somebody sent you. Three escape sequences carry a picture (sixel, a DCS
sequence; iTerm2's OSC 1337; and kitty's graphics protocol, an APC sequence), and behind them are
five formats, each decoded by a large C library with a long memory-safety history: sixel
(libsixel), PNG (libpng), JPEG (libjpeg), WebP (libwebp) and GIF (libnsgif). HEIF and AVIF are not
accepted, because both are containers around a video codec and would put a video decoder on that
path.

`libkimg` is therefore the only place KDOS decodes untrusted image bytes. `kimg_decode_all()` is
the one decode path (`kimg_decode()` is the same call for a single frame), so the budget, the audit
and any new format all live in one function. Its rules:

- The budget is enforced before any allocation, from the size the format itself declares. A
  length field is an allocation request from an untrusted peer: a decompression bomb is four lines
  of sixel, and a PNG claiming 65535×65535 is eight bytes on the wire and sixteen gigabytes in
  memory. The budget bounds width, height and decoded bytes.
- A declared type that disagrees with the bytes is refused, not re-sniffed. Sixel, which has
  no magic number, is the one exception.
- libpng's and libjpeg's diagnostics are suppressed. Both write to standard error by default,
  which would let untrusted bytes decide what appears on a terminal.
- Every failure returns the same nothing. The caller cannot tell a truncated file from an
  unsupported format from a budget refusal, on purpose: there is nothing useful to do differently,
  and a reason string in a log is a string an attacker chose.
- It is tested under mutation. Wherever pixman and the decoder libraries are installed,
  `testing/selftest.sh` checks a fixture corpus (`testing/fixtures/img/`) for the right answers,
  then builds `testing/fixtures/img/fuzz.c` and runs every fixture truncated at every length and
  with every byte changed three ways. Run it as
  `CC="cc -fsanitize=address,undefined -g" testing/selftest.sh` to do so under the address and
  undefined-behaviour sanitizers.

The terminal library `libkvt` adds its own limit before `libkimg` sees anything:

- The payload is capped, and a payload over the cap is dropped entirely rather than truncated.
  A truncated payload is a malformed file, the input a decoder is least likely to survive.
- A consumer that registers no image callback leaves all three protocols off: sixel goes to
  the ordinary DCS handling and an APC is ignored up to its terminator.

In `kdos-term` the relevant settings go in `~/.config/kdos/term.conf` (see
[kdos-term](../04-programs/kdos-term.md)). A value outside the range is clamped to it:

| Key | Default | Range | Meaning |
|---|---|---|---|
| `images` | `yes` | `yes`, `no` | `no` decodes nothing and discards every payload (the parser's cap drops to 4 KiB); a kitty query is answered with a refusal so the program falls back at once |
| `image_max` | `1024` | 4–65536 | Largest single payload, in KiB |
| `image_cells` | `200` | 1–1000 | Largest picture, in cells along either side; the decode budget is 64 pixels per cell |

## Hyperlinks in terminal output

`OSC 8` marks a run of terminal text as a hyperlink. Following one is the one place a terminal
hands an address it was given by a child process to a program that opens things, and its reach is
the same as an image payload's: a shell script, a program in a box, `cat` on a file somebody sent
you. `libkvt` and `kdos-term` apply these rules:

- Four schemes and nothing else: `http://`, `https://`, `file://` and `mailto:`, matched
  without regard to case. These are the ones whose worst case is a window appearing. A scheme
  handler is chosen by the `x-scheme-handler/<scheme>` MIME type, so an unlisted scheme would be a
  program of the attacker's choosing asked to start.
- Every byte must be printable ASCII (32–126), and an address is shorter than 2048 bytes. A
  control byte would reach an argument vector, and a byte above 126 lets the same address read two
  ways depending on who decodes it, which is how an allowlist gets walked around.
- The address is refused when it is parsed, not when it is clicked. What is not in the table
  cannot be followed by any path, including one written later.
- The table holds at most 128 distinct addresses per terminal. A child emitting a new address
  for every cell is the shape of the attack; past the cap, text is plain text.
- Only Ctrl+click follows a link, so selecting a word never opens anything.
- It runs as an argument vector, never a command line: `kdos-appbox open <uri>`, with the URI
  as one element. There is no shell anywhere on the path.
- A refused link is silent. The characters draw normally and nothing says "refused", because a
  message naming the address would put the attacker's string on screen.

## What is not protected

This section lists what the mechanisms above do not do. A reader who assumes a protection that is
not there is worse off than one who knows it is missing.

- **There is no mandatory access control.** No SELinux and no AppArmor. A process running as you
  can do anything you can do.
- **There is no verified boot and no signed kernel.** The boot chain is not measured or attested.
  Anyone with physical access can boot a modified kernel or initramfs and take control of the
  machine.
- **The serial console is a root shell.** `/etc/inittab` runs `/bin/bash -l` on `ttyS0` with
  `askfirst`, on the live image and on an installed system alike: anyone with a serial connection
  presses Enter and is root, with no password.
- **The live image has known passwords.** Both `root` and `kdos` have the password `kdos` (see
  [Getting started](../02-user-guide/getting-started.md)), and `sshd` starts at boot
  (`/etc/init.d/70_sshd.sh`). Only the firewall's `input` drop policy keeps it off the network, and
  `kdos-power firewall ssh on` removes that for port 22. The installer asks for a new password for
  the account and, by default, locks root (it can set a root password instead), so an installed
  system does not carry them.
- **The live image logs in automatically.** Its `/etc/kdos/login.conf` names `kdos` for
  autologin, so whoever boots it reaches the desktop as that account. An installed system asks for
  a password at `tty1` unless its answer file set `autologin = yes` or an administrator later ran
  `kdos-power autologin <user>`. Either way, the lock screen protects a session only once it has
  been locked.
- **Disk encryption protects data at rest only.** It is a passphrase typed into the initramfs, not
  a TPM-sealed key, and it does nothing once the machine is running.
- **A box is not a jail.** By default it shares your home directory, your network, your process
  table, `/dev`, `/sys`, `/tmp` and your runtime directory. A malicious application in a box can
  read and destroy your files exactly as a native one could, reach the session bus, and connect to
  the compositor untagged through `wayland-0`. The sandbox limits what a cooperating application
  is offered by the desktop, not what a hostile one can reach.
- **A box can widen its own grant.** The profile that holds `grant =` lives under the home
  directory a box shares by default, so a program in the box can add `input-method` to its own
  profile, and the compositor honours it the next time it reads that profile: after a `SIGHUP`, a
  restart, the box's first bind of a session, or whenever the eight-entry grant cache fills and
  is emptied. `home = private` closes this for that box.
- **A granted box is as privileged as its grant.** `grant = input-method` hands that box every
  keystroke on the seat, and nothing warns at the moment a key is pressed. The profile is the only
  record.
- **`wheel` is effectively root.** `sudo` and polkit both grant it, polkit through
  `/etc/polkit-1/rules.d/50-kdos.rules`, unconditionally, because this system cannot ask for a
  password. With no session provider, `subject.active` and `subject.local` are always false, so a
  rule granting `wheel` covers a member logged in over SSH, or a background process running as
  them, exactly as it covers somebody at the console. Adding a session provider would not narrow
  it; the rule would have to be rewritten. Every root daemon answers `wheel` on every verb, so
  there is no separation between "can change the accent colour" and "can format a disk".
- **An OCI base pulled from a registry is unsigned content** from somebody else's server. It is an
  online operation, `KDOS_REQUIRE_SIG` does not cover it, and the tool says so before doing
  anything.
- **An unsigned pack is accepted** unless `KDOS_REQUIRE_SIG` is set, and its hash proves only that
  it was not corrupted. A signature that fails, or that no key on this machine can check, is
  refused, so a pack that is signed but uncheckable is treated more harshly than the same pack with
  no signature block at all.
- **Ports built from source are not signed**, and need not be: a port's integrity is the checksum
  in its recipe. That makes the recipe, and so this repository, the trust root for everything on
  the host.
- **Updates are never installed automatically, and there is no security-advisory service.** A
  system timer (`/etc/kdos/timers.d/10-update-check.timer`) runs `kdos update check` once a day and
  records the result in `/var/lib/kdos/update.json`; newer versions arrive only with a newer ports
  tree, and installing them is `kdos update apply`, which you run. `kdos cve` tells you what is
  behind a known fix.
- **`kdos-oomd`'s choice between several large processes is tested only against recorded
  process tables.** `kdos-oomd --fixture` exercises victim selection offline, and
  `testing/oomd-fire.sh` makes the daemon fire in a virtual machine against a single memory hog;
  which of several real candidates it kills under real pressure has not been measured.

## See also

- [How KDOS differs](../01-philosophy/how-kdos-differs.md#the-security-model) — this model set against
  the ones other distributions use
- [Packaging](packaging.md) — signing, the binhost index, and how ports are verified
- [Packs and boxes](packs-and-boxes.md) — the pack format, verification, mounting and box profiles
- [kdos-appbox](../04-programs/kdos-appbox.md) — box profiles, the launch path and what a box receives
- [The daemons](../04-programs/daemons.md) — each root daemon's verbs, clients and refusals
- [The session](session.md) — the portal route and the session bus a box shares
- [Known gaps](../06-reference/known-gaps.md) — everything else that does not exist

<!-- book-nav -->
---

*Part III — Architecture, chapter 17.* Previous: [16. Packs and boxes](packs-and-boxes.md) · [Contents](../README.md) · Next: [18. The design language](design-language.md)
