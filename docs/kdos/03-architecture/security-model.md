# The security model

This chapter describes who may do what on a KDOS machine, which mechanism enforces each rule, and
what is not protected at all. It is written for anyone who administers a KDOS machine, reviews its
security, or changes one of its privileged programs. It assumes the vocabulary of
[Architecture overview](overview.md); a *box* is a rootless container that runs an application,
and a *pack* is the read-only image a box is composed from (both are defined in the
[glossary](../06-reference/glossary.md) and described in [Packs and boxes](packs-and-boxes.md)).

The chapter runs in this order: the threat model, the setuid binaries, the login stacks and how a
password is checked, the system accounts, the root daemons, the firewall, polkit, the containers
and KWin's filter for boxed clients, the kernel, mount options, and signing and trust. Its last
section, [What is not protected](#what-is-not-protected), is part of the specification rather than
a footnote: a model that lists only its defences gets relied on for things it does not do. A
reader with time for one section should read that one.

## What this model is for

KDOS is a single-user workstation. The person who installs it makes one account, that account is
in the `wheel` group, and `wheel` is trusted: it is the administrator. The live medium's account is
`kdos` (uid 1000), in the same groups. The design defends against four things:

- **Outside software misbehaving.** An application from another distribution, run in a box, doing
  what a desktop application has no reason to do.
- **A tampered artefact.** A package, an index, a system image or an application image that is not
  what it claims to be.
- **Privilege escalation** through the small number of programs that need privilege.
- **A catastrophic mistake** by a `wheel` user, made through an interface that should never have
  let it be expressed.

It does not defend against the person at the keyboard, or against software running as that
person. A recurring pattern in what follows is that a mechanism is not made safe where it can be
made unnecessary: KDOS ships no setuid program of its own, `locate` needs no setgid binary, and no
root daemon accepts a path.

## setuid binaries

A *setuid* binary runs as its owner, here root, whoever starts it. The recipes in the ports tree
produce eighteen setuid-root binaries, none of them KDOS's own:

| Binary | From | What it is for |
|---|---|---|
| `sudo` | sudo | Running a command as another user |
| `su` | shadow | Switching to another account. util-linux's `su` is not built (`--disable-su`), so shadow's is the only one |
| `passwd`, `chage`, `gpasswd`, `chfn`, `chsh`, `newgrp` | shadow | Account management |
| `newuidmap`, `newgidmap` | shadow | Writing a rootless container's user-namespace map |
| `pkexec`, `polkit-agent-helper-1` | polkit | polkit's privileged actions, and checking a password typed into the authentication agent |
| `ssh-keysign` | OpenSSH | Host-based authentication |
| `dbus-daemon-launch-helper` | dbus | System-bus activation. Owned `root:messagebus`, mode `4110`, so only the bus daemon can run it |
| `mount.nfs` | nfs-utils | Mounting an NFS share listed in `fstab` as an ordinary user |
| `unix_chkpwd` | pam | Lets `pam_unix` check a password for a caller that is not root. The lock screen depends on it (see [How a password is checked](#how-a-password-is-checked)) |
| `fusermount3` | fuse | Mounting a userspace filesystem from a session with no user namespace: kio-fuse, sshfs, gocryptfs, fuse-overlayfs, the document portal, `rclone mount`, and `restic mount` through the `fusermount` link beside it |
| `crontab` | cronie | Writing the caller's own crontab into `/var/spool/cron`, which is mode 0700 and root's; it drops back to the caller for everything else |

Most of these bits come from the upstream `make install`. Three recipes set a bit themselves:
`shadow` repeats the mode on `newuidmap` and `newgidmap` so that a configure change cannot drop
it, `pam` sets it on `unix_chkpwd`, and `cronie` sets it on `crontab`, which its install leaves
0755. The `dbus` port's `postinstall.sh` restores the helper's group and mode after the package
manager has installed it as `root:root`. shadow's `login` is not installed: the console's `login`
is util-linux's, which needs no setuid bit because `agetty` runs it as root.

`newuidmap` and `newgidmap` are the reason every boxed application starts. The container engine
(podman) runs them to write a process's user-namespace map, which the kernel allows only from a
process that already holds `CAP_SETUID`. Podman checks the binaries first; if they are not setuid
it exits with status 125 and prints `should have setuid or have filecaps setuid`. Without those two
bits no boxed application starts.

These are deliberately not setuid:

- util-linux's `mount` and `umount` (built `--disable-makeinstall-setuid`). Removable media are
  mounted by `udisksd`, which runs as root and asks polkit.
- `kwrited`, which upstream installs setuid root so that it can write its own utmp record. The
  `kwrited` recipe clears the bit: its terminal library writes the record with musl's `utmpx`
  functions, which record nothing, so the bit would buy a setuid-root program and no record.
- `bwrap` (bubblewrap). It sandboxes through an unprivileged user namespace, which is how
  `xdg-desktop-portal` runs its validators.
- `ksu`, Kerberos's `su`. The `krb5` recipe deletes it: accounts are local and privilege
  escalation is `sudo`, so it would be an entry point nobody uses and nobody audits. Kerberos is
  present for `kinit`, an ordinary program.
- `pppd`, which NetworkManager starts as root, and `qemu-bridge-helper`, which serves only root.
- `wireshark`'s `dumpcap`, `bandwhich`, `trippy` and `sniffnet`. Run them as root or grant the
  capability yourself. The Wireshark window started from the menu runs as you, so it lists no
  capture interfaces until one of those is done; it opens a saved capture either way.
- Chromium's `chrome-sandbox`, which the `chromium` recipe does not build. Chromium's renderer
  sandbox, and QtWebEngine's, is built on unprivileged user namespaces, which the kernel has
  (`CONFIG_USER_NS=y`). Turning user namespaces off would leave Chromium needing the setuid helper,
  which would then join the list above.
- `spice-client-glib-usb-acl-helper`, which `spice-gtk` is built without (`-Dpolkit=disabled`). USB
  redirection from a SPICE viewer reaches only the devices whose nodes you can already open.
- LinuxCNC's `rtapi_app` and `linuxcnc_module_helper`, whose bit the recipe removes, so LinuxCNC
  runs its threads without realtime priority, which is its simulator.
- `ping`, iputils', built with neither a setuid bit nor a file capability. `/etc/sysctl.conf` sets
  `net.ipv4.ping_group_range` to `0 2147483647`, so every user may open an ICMP datagram socket and
  no privilege is needed. `arping` sends raw packets and needs root.

The list above is derived from the recipes; the built image is the authority. To check a machine:

```sh
find / -xdev -perm -4000 -type f
```

KDOS uses mode bits rather than file capabilities because every host binary reaches the image
inside a kpkg package, and the package archive is a GNU-format tar written without `--xattrs`. A
mode bit survives that archive; a file capability, which is an extended attribute, does not. The
later copies (the squashfs system image, the installer's `rsync -aHAX`, and a pack built with
`mkfs.erofs` as root) would all carry an extended attribute, but by then it has already been lost.
The same is why KWin, which upstream gives `CAP_SYS_NICE` as a file capability, gets its real-time
priority from a resource limit instead (see [The login stacks](#the-login-stacks)).

Losing a setuid bit is a silent failure: one `chown` or one archive copy without the right flag
is enough, and nothing reports it. `kdos doctor` checks the four that matter most (`unix_chkpwd`,
`newuidmap`, `newgidmap`, and `dbus-daemon-launch-helper` with its `messagebus` group) and prints
the repair when one is wrong: `chown root` and `chmod 4755` for the first three,
`chown root:messagebus` and `chmod 4110` for the bus helper. The same check confirms that
`/etc/subuid` and `/etc/subgid` exist, because without them no rootless box can map a user.

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
| The schedule | `~/.config/kdos/timers.d/20-updatedb.timer`, a row of the user's own `timers` service (seeded from `/etc/skel`; see [The session](session.md#the-users-services)) |
| How `locate` finds it | `$LOCATE_PATH`, exported by `/etc/profile.d/40-plocate.sh` |

An index built this way can contain only paths its owner could already list, so the visibility
check has nothing to guard. `--require-visibility no` is required: with the check on, `updatedb`
insists on the `plocate` group and writes nothing. `--prune-bind-mounts yes` keeps a bind-mounted
tree, such as a box's graft into the home directory, from being indexed twice.

The port carries one behavioural patch, `locate-path-replaces-default.patch`. Upstream searches
its compiled-in `/var/lib` database first and exits on the first database it cannot open, so on a
machine with no shared database every search would fail before reaching the user's own. With the
patch, a set and non-empty `$LOCATE_PATH` replaces the default instead of following it.

## The login stacks

Every login on the machine is a PAM login, and the stacks in `/etc/pam.d` are where an account is
checked, its resource limits are set, and its session is registered. The shared stacks come from
the `pam` port; each login program's own stack comes from that program's port.

| Stack | Used by | What it does |
|---|---|---|
| `system-auth`, `system-account`, `system-password`, `system-session` | Everything below, and `sudo`, `su`, polkit's `polkit-1` | `pam_unix` (password hashes in `/etc/shadow`, SHA-512 for a new password; `nullok`, so an account with an empty password logs in with an empty field) and `pam_limits` |
| `system-login` | Every login that starts a session | The four above, then `pam_turnstile` and `pam_elogind`, in that order, both optional |
| `login` | util-linux's `login`, on tty2, the serial line and the fallback prompt on tty1 | `pam_faildelay` (three seconds after a failure) and `pam_nologin` (`/etc/nologin` refuses everyone but root), then `system-login` |
| `sddm` | A password at the login screen | `system-login`, then `pam_kwallet5` in the auth and session phases |
| `sddm-autologin` | SDDM's automatic login | No password. Refuses every account below uid 1000 except `plasma-setup`, the first-boot wizard's, so a `kde_settings.conf` naming `root` or a service account starts nothing. Then `system-login`, `pam_env` with `/etc/security/sddm-autologin.conf`, and the session half of `pam_kwallet5` |
| `sddm-greeter` | The login screen itself, as the `sddm` account | `pam_permit` for auth, `pam_limits`, `pam_unix`, `pam_elogind`, and `pam_turnstile` as `required`: turnstile is the only thing that makes the greeter's runtime directory, and its KWin will not start without one |
| `kde`, `kde-fingerprint` | The lock screen | `system-auth`; `pam_fprintd` for a fingerprint, used only when it is turned on in the lock screen's settings |
| `turnstiled` | The session `turnstiled` opens for a user's services | turnstile's own: `pam_limits` and `pam_elogind`, so PipeWire gets the audio group's limits and elogind sees the session |
| `other` | Any PAM service with no file | Refuses everything and logs it (`pam_warn`, `pam_deny`) |

`pam_turnstile` comes before `pam_elogind` in `system-login`, the order Chimera Linux uses: turnstile
makes the runtime directory and exports it before elogind registers the session. What the two do
for a session is in [The session](session.md#starting-a-session).

**Resource limits.** `pam_limits` reads `/etc/security/limits.d` at every login. PipeWire's
`25-pw-rlimits.conf` gives the `audio` group real-time priority 95, nice −19 and 4 MiB of locked
memory, which PipeWire's threads need. KDOS's `30-kdos.conf` gives every login real-time priority
1, the lowest, which is what KWin asks for its input and frame-commit threads, and SDDM's
`30-sddm.conf` gives the same to the `sddm` account for the login screen's KWin. A group entry
outranks `*` whatever the file order, so a member of `audio` keeps 95. The kernel's real-time
throttle (`kernel.sched_rt_runtime_us`) keeps a process at priority 1 from taking more than 95% of
a CPU.

### How a password is checked

The login screen, `login`, `sudo`, `su`, polkit's agent and the lock screen all check a password
through `pam_unix`. A program running as root reads `/etc/shadow` itself; one running as the user
(the lock screen, which kscreenlocker runs inside the session) has `pam_unix` call the setuid
`unix_chkpwd`, which checks only the calling user's own password and is the one privileged step.
Without its setuid bit the lock screen refuses every password, and the session can be left only
from another console. kscreenlocker is built with the password service `kde` and the fingerprint
service `kde-fingerprint`; smart-card, face and second-factor unlocking are built off.

An account with an empty password (the live medium's) unlocks the lock screen with an empty field,
because `system-auth` carries `nullok`. The live medium also turns the timed lock off
(`/usr/share/kdos/xdg-live/kscreenlockerrc`), so its screen locks only when asked.

**An automatic login can start behind the lock screen.** `sddm-autologin` runs `pam_env` with
`/etc/security/sddm-autologin.conf`, which puts `/etc/kdos/autologin-xdg` first in
`XDG_CONFIG_DIRS`. The image ships no such directory, so on its own the line changes nothing. The
installer creates it on a single-account machine with an encrypted root that asked for an
automatic login, holding a `kscreenlockerrc` of `[Daemon] LockOnStart=true`: the session then
comes up locked, and the lock screen is that machine's login after the disk passphrase. The
installer's "no password after the disk passphrase" answer leaves the directory out. The setting
lives with the stack rather than in `/etc/xdg` or a home directory because it belongs to the
automatic login, not to the account: a password login never reads the directory, so turning the
automatic login off later does not leave every password login locked as well.

**The wallet.** `pam_kwallet5`, from kwallet-pam, keeps the password typed at the login screen in
the auth phase and, in the session phase, starts the user's secret store, `ksecretd`, with a hash
of it, which unlocks the default wallet without a second prompt. Nothing unlocks it when there was
no password to keep: after an automatic login, or a login that was not a graphical one, the wallet
asks for its own password the first time something opens it. Unlocking the lock screen runs no
session phase, so it never unlocks the wallet. The live medium turns the wallet off
(`/usr/share/kdos/xdg-live/kwalletrc`), since its account has no password.

### The mode of `/etc/shadow`

`/etc/shadow` must be mode 0600, and the build has to set that explicitly. Git records one
permission bit (executable or not), so a file under `fs/` cannot carry a mode narrower than 644,
and the file-system build step gives every non-executable file exactly that. A world-readable
shadow file hands every password hash to every account and makes `unix_chkpwd`'s setuid bit
pointless.

`script/phases/10_bootstrap/000_file_system.sh` therefore carries a table (the `FSMODES` block near its end)
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
| `utmp` | 14:14 | `utmpd` and `wtmpd`, which own `/run/utmps` and `/var/log/wtmp` ([Login records](boot-and-init.md#login-records)) |
| `sddm` | 64:64 | The login screen. Home `/var/lib/sddm`, which `kdos-dm` makes and hands to the account at every start. The id is fixed below 101 because `useradd -r` hands out ids from 999 downwards, and a fixed id in that range could be one a tree that already holds those accounts gave to somebody else |
| `nobody` | 99:99 | `kiwix-serve` and `llama-server`, which only read, and anything that asks for an unprivileged account by that name |

The group `sambashare` (gid 65) has no member until somebody is put in it: it is the group Samba
lets publish a share of their own from Dolphin, and its first member is what makes `smbd` start at
boot.

More are created when their port is installed, by the port's `postinstall.sh` with `groupadd -r` and
`useradd -r`, which pick a free id: `polkitd`, `avahi`, `avahi-autoipd`, `clamav`, `geoclue`, `gnuhealth`, `kolibri`, `libstoragemgmt`, `maddy`,
`minidlna`, `mosquitto`, `mumble-server`, `ngircd`, `nm-openvpn`, `nut`, `pcscd`, `postgres`,
`prosody`, `radicale`, `tcpdump` and `usbmux`. `lsmd`, the storage-management daemon, starts as
root and runs every plugin that does not need root as `libstoragemgmt`. `nut` is also put in
`dialout`, for a UPS on a serial line. `libvirt`'s hook creates the account `qemu`, whose primary
group is the shipped `kvm`, so the guests `libvirtd` starts can open `/dev/kvm`, and the group
`libvirt`, which libvirt's own polkit rule admits and which has no member. `brltty`'s hook creates
the group `brlapi` and adds the desktop account to it; that is the group brltty's own polkit rule
admits to its braille display interface. `/etc/init.d/62_plasma-setup.sh` creates the system
account `plasma-setup` with `useradd --system` on a machine installed for somebody else, as the
account the first-boot wizard runs as. The file-system build step merges these files rather than
overwriting them, so accounts a hook added survive a rebuild of `fs/`; where the repository and the
image disagree about an entry, the repository wins.

Two rules keep the tables consistent:

- Every id a shipped account uses has its own line in both `/etc/passwd` and `/etc/group`. A
  primary gid with no `/etc/group` line looks free to `groupadd -r`, which would hand it to another
  daemon's group.
- Every group a udev rule names must exist. eudev logs `specified group '<x>' unknown`,
  carries on with gid 0, and grants nothing. `testing/preflight.sh` checks the rules under `fs/`
  for this.

The desktop account belongs to `wheel`, `lpadmin`, `audio`, `video`, `render`, `input`, `kvm`,
`cdrom`, `dialout`, `tty` and `users`. An account the installer makes for a non-administrator is
left out of `wheel`, and so of `sudo`, the polkit grants and every root daemon's configuration
verbs. Neither is in `tss`, so the TPM is root's: run the `tpm2_*` tools under `sudo`. Every account
made later joins `audio` whichever tool makes it, because its members carry PipeWire's real-time
limits: `/etc/default/useradd` sets `GROUPS=audio`, which System Settings' Users page and Plasma
Setup reach through `useradd`, and AccountsService is patched to add an administrator's `wheel`
after `useradd` rather than with `-G`, which would replace that default. The person at
the machine needs no group for the display, the input devices or a removable drive: elogind gives
the active session access to the devices udev tags `uaccess`, and takes it away when the session is
switched away from.

## Root daemons

Six KDOS daemons run as root, each started by its own init script under `ksvc` and never by D-Bus
activation:

| Daemon | Answers on | What it does |
|---|---|---|
| `kdos-powerd` | `/run/kdos-powerd.sock`, and `io.kdos.Power1` on the system bus | The firewall's named services, the accent everything drawn before a login wears, and the splash's motion |
| `kdos-energyd` | `/run/kdos-energyd.sock` | Per-application energy use |
| `kdos-oomd` | `/run/kdos-oomd.sock` | Kills a process under memory pressure, sparing the desktop |
| `kdos-packd` | `/run/kdos-packd.sock` | Mounts, verifies, installs and composes application packs |
| `kdos-settingsd` | `org.freedesktop.hostname1`, `timedate1` and `locale1` on the system bus | The machine's name, clock, time zone, locale and keyboard, for System Settings and the first-boot wizard |
| `kdos-sysupdated` | `org.freedesktop.sysupdate1` and `io.kdos.Slots1` on the system bus | System updates for Discover, the A/B slots for System Settings, and the boot confirmation |

Their verbs are in [the daemons](../04-programs/daemons.md). Suspend, power-off and reboot are not
among them: they are elogind's, and polkit decides who may ask.

### Who may talk to them

A socket is checked on who is connecting, not on its permissions. Every socket is mode 0666 and
anyone may connect. The daemon asks the kernel for the connecting process's user id
(`SO_PEERCRED`, which the client cannot forge) and answers `err not permitted` to anyone it does
not admit. A mode that looked like the authorisation would be a mode somebody eventually loosens.
A bus interface asks polkit about the calling process instead, and may prompt through the
session's agent.

| Daemon | Admits |
|---|---|
| `kdos-powerd`, socket | Anyone: `ping`, `firewall list` and `firewall knocks`. Root and `wheel`: the changes (`firewall`, `accent`, `splash`) with no prompt. Anyone else: whatever polkit answers about the same action for that process, asked without a prompt, so the person at the machine may set the accent and the splash and nobody outside `wheel` may open a port |
| `kdos-powerd`, `io.kdos.Power1` | polkit for every caller: `io.kdos.firewall.set` is an administrator's password at the machine and no elsewhere; `io.kdos.accent.set` (the accent and the splash) and `io.kdos.filesharing.start` (starting Samba for shares that already exist) are yes at the machine and no elsewhere |
| `kdos-oomd` | Root, `wheel`, and any account elogind reports as `active` (in the foreground of a seat); its only verbs are `ping` and `status` |
| `kdos-energyd` | Root and `wheel` |
| `kdos-packd` | Root and `wheel` |
| `kdos-settingsd` | polkit, under systemd's own action ids: setting the time zone and switching network time are yes at the machine (System Settings' Date & Time page authorises setting the clock before any change, so it asks for the password even for those); the hostname, the locale, the keyboard and setting the clock are an administrator's password, kept for a few minutes, everywhere |
| `kdos-sysupdated` | polkit: checking for an update is yes at the machine; installing one, trying the other slot, changing the update channel and changing how software is installed (`io.kdos.slots.install-route`, prebuilt or built here) are an administrator's password, kept for a few minutes; installing a version picked by hand asks every time; the boot confirmation (`io.kdos.slots.bless`) is yes for the active local session and no for anything else |

The socket test lives in one function, `kb_uid_allowed()` in `libkbase`, and no daemon keeps a
copy of it. It admits uid 0, refuses a uid with no passwd entry, and otherwise checks the account's
primary and supplementary groups. Several copies would be a rule tightened on one socket and left
loose on the others. To see how `kdos-powerd`'s socket would treat an account:

```sh
kdos-powerd --explain <user>
```

It asks the same function the socket does, so its answer cannot drift from the daemon's.
`kdos-oomd` asks elogind whether an account is active through `libelogind`, opened the first time
it is needed rather than linked: the daemon exists for the minute memory runs out, and a session
library that failed to load must cost the read tier, never the killer.

`kdos-energyd` answering only root and `wheel` leaves the energy sensors of System Monitor empty
for an account outside `wheel`. The daemon exists because the CPU energy counter (RAPL) is
root-only: fine-grained unprivileged reads can recover cryptographic keys through a side channel.
What leaves the daemon is a per-application percentage accumulated over minutes. It samples every
ten seconds, an interval compiled in and not configurable, the raw counter is never republished,
and a client cannot drive the interval towards a fine-grained one. There is no write path into the
power interface at all.

### Clients never name a path

Every configuration verb takes a name from a list compiled into the daemon: a firewall service, an
accent, a splash motion. A daemon that accepted a path would let anyone it admits write wherever
root can. `kdos-settingsd` checks every value before it asks polkit, so a bad value is refused
without a password prompt, and writes only its own files (`/etc/hostname`, `/etc/machine-info`,
`/etc/localtime`, `/etc/locale.conf`, `/etc/keymap`, `/etc/kdos/xkb.conf` and the few beside them);
every program it runs, such as `loadkeys`, is an argument vector, because every argument came over
a bus.

The one exception is installing a pack, which names a file name, not a path, in a staging
directory the daemon owns (`/var/lib/kdos/packs/staging`, mode 01777). The name must match
`[A-Za-z0-9._-]` and contain no slash. That directory is the single place an unprivileged download
may land, and the daemon publishes its location so clients do not derive it.

### What each daemon refuses

| Daemon | Refuses |
|---|---|
| `kdos-powerd` | Anything but `ping`, `firewall list`, `firewall knocks`, and three verbs whose argument is checked before use: `firewall` (a service name from a table compiled into the daemon, then `on` or `off`), `accent` (one of the eight colour schemes in `libkcolor`'s table), `splash` (`phosphor` or `modern`) |
| `kdos-packd` | Paths as arguments; a staged name outside `[A-Za-z0-9._-]`; a pack whose hash or signature fails; replacing or removing a pack that is composed into a running box |
| `kdos-oomd` | Any argument at all: killing is its own decision or it does not happen |
| `kdos-energyd` | Republishing the raw energy counter; a client-chosen sampling interval |
| `kdos-settingsd` | A hostname, zone, locale or keymap that does not parse, before polkit is asked; switching network time on where there is no `chrony` |
| `kdos-sysupdated` | Writing the inactive slot while the running boot is itself a trial; any work of its own: it runs `kdos update apply` and answers only once the child has said `verified` or `refused` |

`kdos-boxsock` is not a root daemon: it is installed in `/usr/bin` and runs as the desktop user, one
process per box launcher. It binds the box's tagged Wayland socket in the box's private runtime
directory and holds that socket's security context open until KWin exits or the process is killed;
see [Sandboxed clients](#sandboxed-clients).

## The firewall

The firewall is nftables, loaded at boot by `/etc/init.d/25_nftables.sh` from `/etc/nftables.conf`.
The script checks the whole file with `nft -c` first and never applies one that fails the check,
because a ruleset applied halfway would leave the input chain's drop policy in place with its
accept rules missing. A failed check keeps a running ruleset. With none running, as at boot, it
loads the file without its drop-ins, or else a minimal drop policy (established and related
traffic, loopback, the necessary ICMP and ICMPv6, the DHCPv6 client), and reports `[FAIL]`: an
empty ruleset would accept everything while sshd and every other listener start.

The shipped `inet filter` table:

| Chain | Policy | Accepted |
|---|---|---|
| `input` | drop | Established and related traffic; loopback; the ICMP and ICMPv6 types that keep a network working (unreachable, too big, time exceeded, parameter problem, echo request, and IPv6 neighbour and router discovery); mDNS (UDP 5353); NetBIOS name service answers (UDP from port 137 to port 137); WS-Discovery answers (UDP from port 3702, from a private or link-local address, to the kernel's ephemeral port range); the DHCPv6 client (UDP 546); DNS and DHCP from `10.42.0.0/16`, the range NetworkManager gives a hotspot's clients |
| `forward` | drop | Established and related traffic, and anything to or from `10.42.0.0/16` |
| `output` | accept | Everything |

The NetBIOS and WS-Discovery rules exist for Dolphin's network view. A NetBIOS name query is a
broadcast and a WS-Discovery probe is multicast, and each host answers from its own address, so
connection tracking holds no entry for the answers and the first rule does not match them. Each is
scoped to the shape of an answer rather than opening the service: nothing on the image listens on
137 or 3702.

Every `*.nft` file in `/etc/nftables.d/` is included after it. `40-podman.nft` lets containers on
a `podman*` bridge reach DNS and be forwarded; `40-libvirt.nft`, from the libvirt package, does the
same for guests on a `virbr*` bridge and lets them reach the network's dnsmasq for DHCP.
`50-kdos-services.nft` is written by `kdos-powerd` and holds the services an administrator has
opened.

Opening a port goes through the same rule as every other daemon verb: the client names a service,
never a port. `kdos-powerd` carries the table of twenty-five names (`ssh`, `http`, `https`, `ipp`,
`smb`, `kiwix`, `mqtt`, `xmpp`, `nfs`, `babel`, `nut`, `snmp`, `mumble`, `caldav`, `mail`, `irc`,
`dlna`, `tryton`, `caddy`, `mosh`, `syncthing`, `kdeconnect`, `vnc`, `rdp`, `xonotic`), each with
the one rule it adds, so a client that can only say `ssh` can open exactly TCP 22 and nothing else.
`vnc` is TCP 5900, where krfb shares the desktop, and `rdp` TCP 3389, where krdp does. `ssh` is also
the switch for the SSH server: turning it on removes `/etc/service.disabled/sshd` and starts `sshd`,
and turning it off puts the marker back and stops it. mDNS has no name in the table: the base input
chain accepts UDP 5353 on its own. The daemon rewrites `50-kdos-services.nft` whole from the names
that are on, checks the full ruleset with `nft --check`, and then reloads it. When the check refuses
the ruleset or the reload fails, the daemon puts the previous file back, so the file on disk is the
ruleset the kernel runs. A rule written by hand into that file is dropped on the next change; a
hand-written rule belongs in a file of its own beside it.

```sh
kdos-power firewall list          # anyone
kdos-power firewall ssh on        # root or wheel
```

System Settings' Firewall page does the same through `io.kdos.Power1`. `kdos-fwask`, started with
each Plasma session, asks every ten seconds how many packets each closed service has turned away,
and when the count rises for a service whose program runs in this session (KDE Connect, say) and
nobody has answered for this network, it asks once whether to open it. "Allow" opens the name for
every network, through the same polkit check; it can open nothing the settings page could not.

Samba's `smbd` runs only once some account can share: the `sambashare` group has a member, whose
shares from Dolphin are usershares in `/var/lib/samba/usershares` (mode 1770, writable by the
group), or a share is already there or written into `smb.conf`. Its service script starts it at
boot only then, and the first login of an account just added to the group starts it through
`kdos-powerd`'s `StartFileSharing`, which takes no argument and runs that script; see
[Starting Samba for a shared folder](../04-programs/daemons.md#starting-samba-for-a-shared-folder).
The `smb` name in the firewall is separate and stays the administrator's choice: TCP 445 is closed
until `smb` is opened, whether `smbd` runs or not, and opened with `smbd` down it reaches nothing.
The firewall file holds what an administrator chose, not what is running, so no rule follows
`smbd`.

## polkit and the authentication agent

*polkit* is the system service that other daemons ask whether a user may perform an action. Each
action's `.policy` file gives three answers, and polkit picks one by asking elogind where the
caller is:

| Caller | Answer read | Who that is |
|---|---|---|
| In the active local session | `allow_active` | The person at the machine, in the foreground session |
| In a local session switched away from | `allow_inactive` | Another account's session on another terminal |
| Anything else | `allow_any` | An SSH login (OpenSSH opens no session), a cron job, a daemon |

An answer of `auth_self` or `auth_admin` is a *challenge*: polkit asks the session's
authentication agent, `polkit-kde-authentication-agent-1`, which Plasma starts from an autostart
entry and which shows a password dialog. `auth_admin` takes the password of a member of `wheel`, and
a `_keep` answer is remembered for a few minutes. A caller with no session has no agent to ask, so
a challenge there is a refusal; in a terminal, `pkttyagent` asks instead.

polkit is built `-Dsession_tracking=elogind` and carries `turnstile-session.patch`. turnstile
registers each user's services as a session of their own with no seat, and the user's session bus
is one of those services, so every program the bus starts (Plasma's `plasma-shutdown`, KRunner and
everything launched from it, `ksystemstats`) would be judged as a seatless caller, get `allow_any`
and find no agent. For a process in a turnstile session the patch takes the user from that session
(`sd_session_get_uid`) and answers for the user's display session (`sd_uid_get_display`), so the
decision and the agent are those of the person at the machine; a user with no display session
leaves the process with no session. It cannot fall back on polkit's own owner-uid lookup instead:
elogind keeps each session's processes in a `/cN/session` cgroup leaf, for which
`sd_pid_get_owner_uid` fails. Without the patch, Plasma's Shut Down and Restart would ask for an
administrator's password and be refused, a program launched from KRunner would get no password
dialog, and the panel's network meter would be refused NetworkManager's statistics.

### Who asks polkit

polkit is installed and `polkitd` is started by the init system. elogind (suspend, power-off,
reboot), NetworkManager, ModemManager, `udisksd`, `accounts-daemon`, `kdos-settingsd`,
`kdos-sysupdated`, `kdos-powerd`'s bus face, `bolt`, `fwupd`, `upower`, `fprintd`, `pcscd`,
`libvirtd` and brltty's braille server ask it, and GeoClue asks it on ModemManager's
behalf. `pkexec` runs GParted, the installer and Discover's ports helper and ports builder as root through it, and
the KAuth helpers of Plasma and KDE applications (Kate's save as root, K3b, kio-admin, KDE
Partition Manager, the process list's kill of another user's process, PowerDevil's charge limits
and the rest) are checked through KAuth's polkit backend.

Most of these need no rule: the shipped defaults already give the person at the machine what a
desktop needs. Mounting a removable drive, joining Wi-Fi and switching it off, a hotspot, a smart
card, a modem, suspend and power-off, brightness, power profiles, the time zone, network time and
the accent are `allow_active` upstream. So are KAuth actions such as reading a drive's SMART data,
the hardware tables Info Center shows and a keyboard's colour profile.

### What `50-kdos.rules` changes

`/etc/polkit-1/rules.d/50-kdos.rules` changes an answer only where KDOS wants another, and names
the action each time. Every grant to `wheel` also tests `subject.local && subject.active`, so it
reaches the administrator in the foreground session and never an SSH login, a background job or a
session switched away from; those get the defaults, and a `wheel` member there has `sudo`, which
asks.

| Action | Answer | For |
|---|---|---|
| `org.freedesktop.NetworkManager.settings.modify.system` | Yes, for `wheel` at the machine | Editing and deleting a system connection, and reading its passphrase back (see [below](#why-settingsmodifysystem-is-granted)) |
| `org.freedesktop.udisks2.filesystem-mount-system`, `…filesystem-unmount-others`, `…filesystem-take-ownership`, `…encrypted-unlock-system`, `…encrypted-lock-others`, `…eject-media-system`, `…loop-delete-others`, `…open-device`, `…open-device-system`, `…modify-device-system` | Yes, for `wheel` at the machine | Dolphin and the device notifier mounting a second internal disk and unlocking its container, unmounting what another login mounted, an image writer or GNOME Disks reaching the raw device, formatting and partitioning an internal drive. Their removable twins are `allow_active` already |
| `org.gnome.gparted`, `org.kde.k3b.updatepermissions`, `org.kde.ktexteditor6.katetextbuffer.savefile`, `org.libvirt.unix.manage`, `net.nokyan.Resources.kill` | An administrator's password, kept, for `wheel` at the machine | GParted, K3b setting the burner's permissions, Kate saving a root-owned file, virt-manager on `qemu:///system`, Resources ending another user's process |
| `org.freedesktop.fwupd.update-internal`, `org.freedesktop.bolt.enroll`, `…authorize`, `…manage`, `org.gtk.vfs.file-operations`, `…file-operations-helper` | An administrator's password, kept, for anybody at the machine | Flashing system firmware, letting a Thunderbolt device onto the PCIe bus, gvfs's `admin://`. Held at a password whatever a later rules file says; away from the machine the defaults apply, and each is stricter there |
| `local.kcron.crontab.save` | No | The system crontab: `crond` is built not to read `/etc/crontab`, so a save there would write a file nothing reads. A user's own crontab needs no polkit |
| `org.kde.powerdevil.chargethresholdhelper.setthreshold`, `…setconservationmode` | No | Battery charge limits are TLP's, which sets them at boot and each time it applies a profile, so a limit PowerDevil wrote would not last |

A `YES` is a grant nobody is asked about, so one exists only for something an administrator at the
keyboard does as a matter of course. An `AUTH_ADMIN_KEEP` written out is a prompt a broader rules
file sorted after this one cannot turn into a silent yes. A `NO` is something this system does not
do, refused where the defaults would have asked. Left at the defaults, and so a password: the
storage layout under a running system (`fstab`, `crypttab`, RAID, LVM, btrfs, swap), the firmware
erases, NetworkManager's hostname and machine-wide DNS, and `pkexec` of an arbitrary command.
`testing/fixtures/polkit` holds the defaults measured from the shipped `.policy` files and the
table of answers this file must give, and `testing/selftest.sh` evaluates it against both.

Two more rules files ship with packages. plasma-setup's
`/usr/share/polkit-1/rules.d/plasma-setup-polkit.rules` lets the `plasma-setup` account, and nobody
else, take the actions the first-boot wizard needs with no password: making the first account,
setting the hostname, locale, keyboard and time zone, and joining a network. On the live medium,
`63_live-installer` installs `/etc/polkit-1/rules.d/49-kdos-installer-live.rules`, which lets the
live session at the machine run the installer (`io.kdos.installer.run`), KDE Partition Manager's
helper, and one `pkexec` command line, `/usr/local/bin/kdos persist create [/dev/<name>] --yes`,
matched by the program's real path (`/usr/sbin/ksvc`) and the whole command line against a pattern
whose device is one word of lower-case letters and digits under `/dev`, so no other `kdos` verb, no
other argument and no other program is covered. `create` only adds a partition in free space, or a
partition table to a blank disk. The live account's password is published, so asking for it would
protect nothing; `kinstall` leaves the file out of every install. libvirt's own rule grants
`org.libvirt.unix.manage` to the `libvirt` group, which has no member, and brltty's grants the
braille display to `brlapi`.

### Why `settings.modify.system` is granted

A *system connection* is a NetworkManager profile every user of the machine may join: what the
network applet makes when "all users may connect" is ticked, what `nmcli` makes, and every profile
the installer wrote. Changing or deleting one, or reading its passphrase back, is
`settings.modify.system`, whose default is a password even at the machine. The rule grants it to
`wheel` at the machine, so the administrator can forget a network or show a saved Wi-Fi passphrase
without one. A profile kept to its own user (`permissions` naming an owner) needs no grant: its
owner may change it, and NetworkManager, built with elogind session tracking, connects it
automatically while that user has a session.

That grant lets anybody in `wheel` at the machine read every stored Wi-Fi passphrase. It is a
shortcut, not a new capability: the shipped sudoers line (`/etc/sudoers.d/00-sudo`) is
`%wheel ALL=(ALL) ALL`, and the passphrases are files under `/etc/NetworkManager` that `sudo cat`
prints.

### How polkit and ModemManager are started

`polkitd` is started by `/etc/init.d/41_polkitd.sh` and ModemManager by
`/etc/init.d/42_modemmanager.sh`, rather than by D-Bus activation. Activation of a root service
goes through `dbus-daemon-launch-helper`, which the bus may run only through the helper's
`messagebus` group; kpkg installs every package as `root:root`, and that group is on the helper
only because `dbus`'s `postinstall.sh` sets it back. Starting these two directly keeps the
machine's network authorisation off that dependency. When activation fails it fails silently: the
service never starts, every call is refused, and the only trace is a `Spawn.ExecFailed` in the
log. `kdos doctor` checks the helper's owner, group and mode for that reason.

A daemon the supervisor runs is never activatable as well. The polkit, ModemManager and elogind
ports delete the activation file their builds install, and NetworkManager, `accounts-daemon`,
`kdos-settingsd` and `kdos-sysupdated` install none. With a file, a caller that asked for the name
before the supervised daemon held it would make the bus start a second copy outside the supervisor,
and whichever lost the name would exit. A supervised daemon that lost would be restarted every five
seconds and fail to take the name each time. NetworkManager asks for ModemManager as it starts and
ModemManager asks for polkit, so the race would be run at every boot. Without the files, a call to
a name nobody owns fails with `ServiceUnknown` and starts nothing. `40_dbus.sh`, `40_elogind.sh`,
`41_polkitd.sh`, `42_modemmanager.sh` and `44_accounts.sh` each wait, through `wait_bus_name` in
`/etc/init.d/service_helper`, until their daemon owns its name before the next script starts, at
most ten seconds, so the order the scripts sort in is the order the names appear.

Left to D-Bus activation, and so dependent on the helper: `wpa_supplicant`, `fprintd`, `fwupd`,
`boltd`, `upower`, `udisksd`, `geoclue`, kio-admin's worker, the KAuth helpers, and
NetworkManager's dispatcher and privileged helper. None of these is also supervised.

### What polkit logs

The list of action ids is the boundary, and the only record of its use is a log line.
NetworkManager is built `-Dlibaudit=no`, so an authorised change leaves no audit record beyond
NetworkManager's own message to syslog.

polkitd itself logs no allow or deny decision. What it does log, such as a rules file that does not
compile, it sends to syslog as `authpriv`, so to `/var/log/auth.log`. A rule that wants a decision
recorded calls `polkit.log()`, which polkitd prints on its standard error; `41_polkitd.sh` has
`ksvc` forward that stream under `authpriv` too, so the line lands in `/var/log/auth.log` once,
tagged `polkitd`, and not in `/var/log/messages`. polkitd's standard output repeats every line it
sends to syslog, and the script points it at `/dev/null` with execline's `redirfd` so that nothing
is logged twice.

## Containers

Boxes are rootless: the container engine runs as you, maps your identity into the container,
and uses the setuid mapping helpers above for the one privileged step. A pack box is created with
`podman create --rootfs` over the overlay `kdos-packd` composed, and a store box from the
catalogue's container image, both by `kdos-appbox`. A box built from somebody else's OCI image (a
container image in the Open Container Initiative format, as a registry such as Docker Hub serves
it) goes through distrobox, the upstream tool that wraps podman to make a container behave like
part of the host; it also shares the host's root at `/run/host`.

- Inside the box you are a mapped non-root user (`--userns keep-id`: uid 1000 inside is uid
  1000 outside). That is because applications refuse to run as root, not because it is a
  boundary: a process in your box is a process running as you.
- Ownership inside a box grants nothing, because packs are mounted `nosuid`.
- No mandatory access control applies. Boxes are created with `label=disable` and
  `apparmor=unconfined`; the host runs neither SELinux nor AppArmor.

What a box shares with the host is set by its profile, `~/.config/kdos/boxes/<name>.conf`. Each
namespace key maps onto one podman flag that the kernel enforces, and applies when the box is
created; `kdos-box profile <name>` prints the flag for each, and says beside a key when that key
enforces nothing for this box.

| Key | Default | Private or `none` means |
|---|---|---|
| `home` | shared: your whole `$HOME` | A home directory of the box's own |
| `network` | `host`: the host's network namespace | `private`: a namespace of its own; `none`: no interface at all |
| `ipc` | shared with the host | A private IPC namespace |
| `processes` | the host's pid namespace | A private pid namespace |
| `devices` | `/dev` and `/sys` bind-mounted from the host | No host `/dev`; `gpu = yes` binds `/dev/dri` back alone |
| `gpu` | `yes` | With `devices = private`, `yes` binds `/dev/dri` back and `no` leaves the box without it; with `devices` shared it follows `devices` |
| `wayland`, `audio`, `x11` | `yes`, `yes`, `yes` | Which sockets the box's private runtime directory holds: the tagged Wayland socket, links to the PipeWire and PulseAudio sockets, the X11 cookie |
| `memory`, `cpus`, `pids` | unlimited | `--memory`, `--cpus`, `--pids-limit` |
| `grant` | none | Named sets of KWin's refused protocols, and the password store on the bus (see [Sandboxed clients](#sandboxed-clients)) |

**The runtime directory is the boundary.** A pack box or a store box sees
`$XDG_RUNTIME_DIR/kdos/box/<box>` as its `/run/user/<uid>`, and what a launch puts there is all of
the session it can reach: its own tagged Wayland sockets, its bus proxy, links to the audio sockets
when `audio` allows them, the accessibility bus only while assistive technology is in use, and
the X11 cookie unless the box says `x11 = no`. The session's own
Wayland socket (`wayland-1`) and `bus` are never in it, and nothing in `kdos-appbox` falls back to
them. A box `distrobox` made from somebody else's image shares the session's runtime directory
instead, and every socket in it is reachable whatever `wayland`, `audio` and `x11` say.

**The session bus is filtered.** A box's `DBUS_SESSION_BUS_ADDRESS` is its keeper's
`xdg-dbus-proxy`, which passes the portals (`org.freedesktop.portal.*`), notifications, the tray's
status-notifier watcher and the screensaver interface, each scoped to its own interface and path,
lets the box own its application's id (so a single-instance application works) and
`org.mpris.MediaPlayer2.*`, adds the accessibility bus while it is in use and the password store
(`org.freedesktop.secrets`) with `grant = secrets`, and hides every other name. Hidden among them:
`org.kde.KWin`, whose `ScreenShot2` answers any caller and whose scripting interface loads code
into the compositor; kglobalaccel; plasmashell, whose `evaluateScript` runs code in the shell;
Klipper; KRunner; and ksmserver. A call scoped to notifications cannot reach plasmashell's other
interfaces through its unique name, because the proxy gives a unique name only the rules of the
well-known names its owner holds.

**The three states.** Which boundary held is recorded when a box is started, in its `.state` file,
and shown on its windows:

| State | When | Engine on its windows | Badge |
|---|---|---|---|
| Confined | The tag, the private runtime directory and the filtered bus all held, and no X11 display was handed over | `io.kdos.appbox+private` | A solid box before the caption |
| Tagged | KWin tags its clients, but something else is in reach: the session's runtime directory (a distrobox box), an unfiltered bus, or an X11 display, which bypasses the tag because Xwayland is one client to KWin | `io.kdos.appbox+tagged` | A dashed box |
| Unconfined | Not even the tag held | — | A warning triangle |

A launch that would run untagged is refused rather than started, so an unconfined window is one
that should not exist. The Task Manager's tooltip names the box as well ("In box: www"), and a
box's colour, when it has one, tints its server-side title bar. A program that draws its own title
bar takes no colour, and the badge is then the only mark. How KWin draws each is in
[KWin](../04-programs/kwin.md#boxes-at-the-compositor).

**Resource limits** (`memory`, `cpus`, `pids`) become real cgroup limits only when the box's cgroup
is a scope in the login's delegated subtree. elogind, with `DelegateSessionCgroups=yes`, runs a
login in `/sys/fs/cgroup/<session>/session` and gives the user `/sys/fs/cgroup/<session>/delegated`,
and `15_cgroups.sh` has enabled the `cpu`, `memory` and `pids` controllers at the root for it to
pass on. `kdos-appbox` creates a pack box under `<session>/delegated/app-kdos-<box>-<n>.scope`, the
name the portal and System Monitor read an application id from, but only when the user may write
the `cgroup.procs` of `/sys/fs/cgroup/<session>` itself: cgroup2 refuses an unprivileged move
between two cgroups unless the mover can write their common ancestor's. elogind leaves that file
root's, so on KDOS the box is created under podman's own default instead, `memory`, `cpus` and
`pids` are accepted and enforced by nothing, `kdos-box profile` says so, and the portal names the
box no application (see
[Known gaps](../06-reference/known-gaps.md#host-applications-and-boxes-reach-the-portals-unidentified)). A store box and a distrobox box are never
placed in a scope. In every case `kdos-oomd` reads the profiles and prefers a box that is over its
declared `memory` budget as a victim.

A pulled image is not verified. `/etc/containers/policy.json`, written by the
`containers-common` port, is upstream's default, `insecureAcceptAnything`, which checks no
signature. `/etc/containers/registries.conf` resolves an unqualified image name against
`docker.io`. A base image is trusted as far as the registry and the TLS connection to it are.

## Sandboxed clients

A client in a box reaches KWin through a socket that `kdos-boxsock` created for its launcher in the
box's private runtime directory; `kdos-appbox` sets `WAYLAND_DISPLAY` to that socket before
anything in the box starts. `kdos-boxsock` registers the socket with KWin through
`wp_security_context_v1`: the engine `io.kdos.appbox+private` or `+tagged`, the launcher's id as
the application id, and the box as the instance id. KWin tags every client that connects on it.
The client cannot see, choose or forge the tag, and KWin gives each of its windows the tag's
application id.

Upstream KWin refuses a tagged client seven globals: `org_kde_plasma_window_management` (the list
of every window, and acting on them), `org_kde_kwin_fake_input` (emulated pointer and key events),
`zkde_screencast_unstable_v1` (streaming outputs and windows), `org_kde_plasma_activation_feedback`,
`kde_lockscreen_overlay_v1`, `ext_data_control_manager_v1` (every clipboard, as a clipboard manager
sees it) and `wp_security_context_manager_v1`. KDOS's `sandbox-policy.patch` refuses it the rest of
the desktop shell as well: `zwlr_layer_shell_v1` (panels and overlays), `kde_output_management_v2`
and `kde_external_brightness_v1` (the displays' modes, layout and brightness),
`wp_drm_lease_device_v1`, `org_kde_kwin_dpms_manager`, `org_kde_plasma_shell` (placing a window
anywhere), `kde_screen_edge_manager_v1`, `org_kde_kwin_keystate` and
`org_kde_plasma_virtual_desktop_management`. Everything a client needs for its own windows stays
offered: surfaces, the XDG shell, the seat, buffers, outputs, decorations, text input, the ordinary
clipboard and drag-and-drop, and the rest.

The input-method interfaces are offered only to the input method KWin itself started, fcitx5, and
to no other client, host or boxed. Text input, the application's half of an input method, is
offered to a box, because denying it would deny input methods to the applications that need one
most.

The capture refusal is what makes the portal the sanctioned route. A boxed screen recorder cannot
bind the capture interfaces through its tagged socket, so it asks the ScreenCast portal, which runs
on the host and asks which screen to share. See [the session](session.md#screen-capture-and-the-clipboard).

A client with no security context, which is every program started on the host, is offered every
global, the capture and fake-input interfaces included. KWin's own check is the tag; KWin is built
without libsystemd, so it does not also judge a client by its cgroup.

The tag holds only for a client that uses the socket it was given. In a confined box that is the
only Wayland socket there is. A box that can reach the session's `wayland-1` (a distrobox box, or
one handed the X11 display, whose Xwayland is a host client) can connect untagged and is offered
everything, which is why such a box is at most `tagged` and its badge says so.

### Granting a box past the allowlist

Without a grant, a screen recorder in a box of its own could never be given the screen, even by the
person who installed it. The `grant` key of the box's profile names sets of the refused globals,
which KWin's patch reads once per client whose engine is `io.kdos.appbox` or starts with
`io.kdos.appbox+`, and for no other client:

```ini
grant = screencast, data-control
```

| Grant | Unlocks | What it puts at risk |
|---|---|---|
| `window-list` | Window management, activation feedback, virtual-desktop management | The title of every window, and closing, moving and raising any of them |
| `fake-input` | `org_kde_kwin_fake_input` | Typing and clicking into every other window |
| `screencast` | `zkde_screencast_unstable_v1` | Recording any screen or window without being asked |
| `data-control` | `ext_data_control_manager_v1` | Reading and setting the clipboard at any time, passwords copied from a password manager included |
| `layer-shell` | `zwlr_layer_shell_v1` | Drawing over everything, a fake lock screen or login prompt included |
| `output-management` | Output management, external brightness, DRM leases | Changing or turning off the displays |
| `dpms` | `org_kde_kwin_dpms_manager` | Turning the displays off |
| `plasma-shell` | The Plasma shell surface, screen edges, the keyboard state | Placing a window anywhere, under the panel or over it |
| `input-method` | Nothing | KWin still offers the input-method interfaces only to its own input method |
| `secrets` | `org.freedesktop.secrets` on the box's bus | Every secret in the unlocked wallet |

A name that is not in the table grants nothing, and `kdos-box profile` says so beside it. The
security-context manager and the lock-screen overlay are never granted, so a box cannot mint a
context of its own or draw over the lock screen. The profile is read once per client, so a changed
grant reaches the box's next client and no running client's rights change under it. The profile
directory, `~/.config/kdos/boxes`, is bound read-only into a pack box or a store box over the
shared home, so such a box cannot grant itself what its profile withholds. The path is
`$HOME/.config`, not `$XDG_CONFIG_HOME`, in KWin's reader and in `kdos-appbox` alike, because the
two resolving it differently is the one failure this design must not have.

## The kernel

The kernel's configuration is `kdos.config` in the `linux` port, and six of its settings are there
to remove a way in rather than to support hardware. Each is set explicitly, even where it matches
the kernel's own default, so the build fails if a later kernel stops honouring it (see
[The kernel configuration](../05-developer/how-kdos-is-built.md#the-kernel-configuration)):

| Setting | Value | What it removes |
|---|---|---|
| `CONFIG_X86_X32_ABI` | off | The x32 system-call ABI: a second entry path into every system call, which no program on KDOS uses |
| `CONFIG_UEVENT_HELPER` | off | A program the kernel forks as root for every device event, named by a writable `/sys/kernel/uevent_helper`; `udevd` reads events from netlink and needs none |
| `CONFIG_PM_AUTOSLEEP` | off | Suspend started by the kernel itself whenever no wakeup source is held; on KDOS suspend is always asked for from user space, by elogind writing `/sys/power/state` when Plasma, the lid or the power key asks |
| `CONFIG_LDISC_AUTOLOAD` | off | Any user loading a line-discipline module by setting it on a terminal they opened. A process with `CAP_SYS_MODULE` still loads one, so `pppd` and `hciattach`, which run as root, are unaffected |
| `CONFIG_PROC_MEM_FORCE_PTRACE` | on | Writing to another process's read-only memory through `/proc/<pid>/mem` merely by having the right to trace it; only a debugger actually attached with `ptrace` may |
| `CONFIG_BUG_ON_DATA_CORRUPTION` | on | Running on after a kernel list or reference-count check has found corrupted memory: the kernel stops the offending task with a `BUG` instead of logging a warning and continuing |

### Security modules

The kernel starts the security modules named in `CONFIG_LSM`, which is
`landlock,lockdown,yama,integrity`; no boot entry passes an `lsm=` that would change the list.

| Module | What it does here |
|---|---|
| Landlock | Lets an unprivileged program restrict its own file and network access. [`kdos sandbox`](../04-programs/kdos-command.md#kdos-sandbox) uses it to run a native program under a profile of readable and writable paths and allowed TCP ports; nothing is run that way unless you ask |
| Lockdown | Built in, in its default mode, `none`: it restricts nothing until root writes `integrity` or `confidentiality` to `/sys/kernel/security/lockdown` or a `lockdown=` boot option names one |
| Yama | Restricts `ptrace`. Nothing sets `kernel.yama.ptrace_scope`, so it keeps the kernel's default of 1: a process may trace only its own descendants, and `gdb -p` or `strace -p` on another process of your own fails unless you run it as root or start the program under the tracer |
| Integrity | The kernel's keyrings for signature checks; it enforces no policy by itself |

SELinux, AppArmor, Smack, TOMOYO, LoadPin, SafeSetID and IPE are compiled in but are not in the
list, so none of them runs.
`CONFIG_SECURITY_DMESG_RESTRICT` is on as well: the kernel log is readable only by root (or a
process with `CAP_SYSLOG`), so `dmesg` as an ordinary user prints `Operation not permitted` and
`sudo dmesg` is the way to read it. The copies `syslogd` writes, `/var/log/kern.log` and
`/var/log/messages`, are mode `0600` for the same reason, set by `22_syslog.sh` before it starts.

### Module signing

Every module is signed when the kernel is built (`CONFIG_MODULE_SIG_ALL`, SHA-512, an ECDSA P-384
key), and the certificate of that key is built into the kernel. `CONFIG_MODULE_SIG_FORCE` is off:
the kernel still loads a module that is unsigned or signed by another key, and marks itself
tainted (`module verification failed` in the kernel log). Signing tells you which modules came from
the build that made the running kernel; it does not stop root from loading another.

The key is `build/keys/linux-module-signing.pem` on the machine that builds KDOS. The first kernel
build that finds no key there writes one, with the subject in the port's `x509.genkey`, and every
later build signs with it, so two builds of the same tree carry the same certificate and the
modules of either load untainted under the other's kernel. `make clean` and `make cleanbuild` both
keep `build/keys`, and git ignores all of `build/`, so the private key never enters the repository.
Losing it costs nothing worse than a new certificate in the next kernel. A kernel built where there
is no `/kdos/build`, as `kdos update apply` builds one on an installed machine, takes the kernel's
own default instead: a fresh key generated for that one build.

## Mount options

| Mounted | Options |
|---|---|
| Application packs | `ro,nosuid,nodev` |
| Data packs | `ro,nosuid,nodev,noexec`, and never composed into a box's root |
| Removable media (`udisksd`) | udisks2's defaults, which always include `nosuid,nodev` |
| `/tmp` | tmpfs, `mode=1777,nosuid,nodev` |
| `/run` | tmpfs, `mode=0755,nosuid,nodev` |
| `/sys/kernel/tracing`, `/sys/kernel/debug` | `nosuid,nodev,noexec` |
| `/sys/fs/bpf` | `nosuid,nodev,noexec,mode=0700` |

`nosuid` is the one that matters: a setuid-root binary on a USB stick from another machine would
give any local user root.

`/tmp` is one directory shared by every user and written by every root job: init scripts, timers
and package hooks. `/etc/sysctl.conf` sets `fs.protected_symlinks`, `fs.protected_hardlinks` and
`fs.protected_fifos` to 1 and `fs.protected_regular` to 2; the kernel's own defaults are 0. At 0, a
user who plants a link, a FIFO or a file at a name a root job is about to write in a sticky
world-writable directory can turn that write onto any file on the machine.

The same file hardens the network stack and the kernel's addresses:

| Setting | Value | Why |
|---|---|---|
| `net.ipv4.conf.{all,default}.accept_redirects`, `secure_redirects`; `net.ipv6.conf.{all,default}.accept_redirects` | 0 | An ICMP redirect rewrites a route on the word of whoever sends it, so on a shared network any host on the link could put itself in the path |
| `net.ipv4.conf.{all,default}.send_redirects` | 0 | The machine routes only for its hotspot and its containers, whose clients have no other route to correct |
| `net.ipv4.conf.{all,default}.rp_filter` | 2 | The loose reverse-path check drops a packet whose source no interface routes back to. The strict check, 1, would break the asymmetric routes a VPN, the hotspot, podman's bridge and a Babel mesh make |
| `kernel.kptr_restrict` | 1 | `/proc/kallsyms`, `/proc/modules` and the kernel log's pointers read as zeroes to an account without `CAP_SYSLOG`, so they do not undo address randomisation; root still sees them |

## Signing and trust

KDOS uses three keyrings, and keeping them apart is structural rather than a convention:

| Directory | Attests | Used for | Shipped with |
|---|---|---|---|
| `/etc/kdos/keys` | Who built a release | The release index (`kdos-update.idx`), the binary-package host (binhost) index, package signature files, and the slot image manifest | No key |
| `/etc/kdos/keys/packs` | That whoever holds the matching secret key signed a pack or a pack index | Pack indexes and packs' own signatures, read by `kdos-packd` and `kdos-pack verify` | `kdos-packs.pub` (see below) |
| `/etc/kdos/keys/tree` | That a published ports tree came from whoever holds the release key the image was built with | `kdos-ports.tar.xz.sig`, read by `kdos update fetch` before the tree is unpacked | `kdos-release.pub`, when the build found `build/keys/kdos-release.pub`; otherwise no key |

The keyring loader reads `*.pub` in one directory and does not descend into subdirectories, so the
three are separate policies. A pack-signing key placed in `/etc/kdos/keys` would silently become a
trusted publisher of host packages too.

The directory is the policy:

- Adding trust is copying a `.pub` file in; removing it is deleting one. There is no
  revocation list and no online check.
- A ring holds at most sixteen keys, read in name order. Each further `*.pub` file is left
  untrusted and never tried, and the loader names it on stderr (`<dir>: a keyring holds at most
  16 keys; <name> is not trusted`).
- A secret key whose file mode grants any group or other permission is refused for signing: a
  key others could read has to be assumed copied. `kpkg` says so (`<key> is readable by other
  users — refusing to sign with it`); `kdos-pack` prints only `cannot read the signing key <key>`.
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

The key that ships in `/etc/kdos/keys/packs`, `kdos-packs.pub`, is a fixed public key committed
under `fs/`. Nothing in the build holds or uses its secret half: the image step bakes no
application packs and writes no pack index (see
[Packs and boxes](packs-and-boxes.md#where-a-pack-comes-from)), and the opt-in KDOS base pack it can
build is unsigned. So the key verifies nothing the build makes; it only means that a pack or index
signed with its secret half, wherever that is, would be accepted. To trust packs you sign yourself,
add your own `.pub` with `kdos-pack keygen`, and delete `kdos-packs.pub` if you want only yours.
The rest of the signing design is in [Packaging](packaging.md#signing).

The key in `/etc/kdos/keys/tree` is the one choice of trust an image makes for you: the recipe tree
is the trust root for everything built on the machine, since every source is checked against a
hash the tree names, so the image trusts the next tree from the same key as the tree it carries.
`ports/publish --release` signs each release's `kdos-ports.tar.xz` and its release index,
`kdos-update.idx`, with `build/keys/kdos-release.key` (or the key `$KDOS_RELEASE_KEY` names), and
the build copies `build/keys/kdos-release.pub` into the
ring; a build that finds no `.pub` there ships an empty ring, and that image refuses every fetched
tree. `kdos update fetch` refuses a tree whose signature does not verify, and a tree whose `TREE`
stamp is older than the installed one, so a replayed old tree cannot pin old versions. Replace the key to follow a tree you publish yourself.

### Signed updates

An update arrives as one of three things, and each is checked before anything is kept. The slot
image and the binhost are found only through the release's
[release index](packaging.md#the-release-index), `kdos-update.idx`, verified against
`/etc/kdos/keys` before a line of it is read: every file is taken from where it says and kept only
with the size and SHA-256 it gives, and an index for another release is refused. It is signed with
the release key and the binhost key, so a machine trusting either reads it.

| Lane | What is signed | What is checked against it |
|---|---|---|
| Slot image | The manifest, `kdos-image.manifest`, with its signature beside it, verified against `/etc/kdos/keys` before a line of it is read | Every file the manifest lists, by size and SHA-256, after it is written into the slot; the bundles that carry the files, full or delta, are unsigned, and a file whose hash differs is thrown away and fetched again from the full image |
| Binhost | The index, `PACKAGES` with `PACKAGES.sig`, verified against `/etc/kdos/keys` | Every package, by the hash the index records; a binhost delta is rebuilt and then checked against the hash of the package it stands for |
| Source (`--source`) | The ports tree, verified against `/etc/kdos/keys/tree` | Every source tarball, by the `sha256` in its recipe |

So a tampered bundle or delta costs a download, never a file. A manifest names its version, and an
image is offered only when it is newer than the running system (by its release time when both have
one, by version otherwise), so an older signed release cannot be offered as an update. Every
download is made with `curl --proto =https,file --proto-redir =https`: HTTPS or a local file, and
no redirect to plain HTTP. `kdos image make` refuses a root that holds a file capability or an
access-control list, which a manifest cannot represent, rather than dropping it silently. The
formats and the update path are in
[Packaging](packaging.md#updating-a-machine).

The image ships with no key in `/etc/kdos/keys`, so a fresh machine verifies no binhost index and
no slot image until a key is put there; until then `kdos update` can only build from source, with
the tree checked against the release key. A machine that should take a release's prebuilt packages
or images needs that release's `.pub` copied into `/etc/kdos/keys`.

### Software a person installs

`kdos update install` adds a port to the running root by one of two routes (see
[Packaging](packaging.md#installing-more-software)), and they carry different trust:

| Route | What runs as root | What it rests on |
|---|---|---|
| Prebuilt | `kpkg binhost`, unpacking a package | The signed binhost index: every package goes in under `KPKG_REQUIRE_SIG`, with the host keyring copied into the cache, by the hash the index records |
| Built here | The port's own `build.sh`, and every dependency's that the install builds, for as long as the build takes | The signed ports tree and the `sha256 =` lines of its recipes; the build is root on the running system, and a recipe can do anything root can |

So a source install is as trusted as the tree it builds from, and runs that tree's code as root.
Discover keeps the two routes under two programs and two polkit actions, each program the one
`org.freedesktop.policykit.exec.path` of its action, so neither grant reaches the other:

| Program | Action | Answer |
|---|---|---|
| `/usr/libexec/kdos-discover/kdos-ports-helper` | `io.kdos.ports.install` | An administrator's password, kept for a few minutes at the machine |
| `/usr/libexec/kdos-discover/kdos-ports-builder` | `io.kdos.ports.build` | An administrator's password every time, everywhere: a password kept from a prebuilt install must not cover running a recipe as root |

Each runs `kdos update install` with its own route flag (`--prebuilt` or `--source`, the builder
also taking `--build-deps=prebuilt|source`) and the port, and nothing else. Which
route the terminal takes by default is `install =` in `/etc/kdos/update.conf`, which System
Settings writes through `kdos-sysupdated` under `io.kdos.slots.install-route`.

### TLS trust anchors

The certificate authorities used for TLS are a separate trust root, kept apart from all three
keyrings.

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
| NSS, for a program that loads its builtin-roots module | `/usr/lib/libnssckbi.so`, the module NSS loads as "Builtin Roots", is a symlink to p11-kit's trust module `/usr/lib/pkcs11/p11-kit-trust.so`, so such a program reads the same anchors as GnuTLS. NSS's own `certdata` roots module is not installed |
| Python code that calls `certifi.where()`, `requests` included | `python3-certifi`'s `certifi/cacert.pem` is a symlink to `/etc/ssl/cert.pem` |

The two sides see a new local root at different times:

- GnuTLS, and NSS through `libnssckbi.so`, see it immediately. p11-kit trusts every certificate in a trust path that is a file,
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
warning, and then only the p11-kit side trusts it. `caddy trust` does the whole job by itself: it
writes Caddy's local root into the anchors directory and runs `trust extract-compat`, which the
`p11-kit` port replaces with a call to `update-ca-certificates`. Firefox, LibreWolf and Thunderbird
compile their builtin roots into the browser itself and keep their own store, so a local root
reaches them only through their own certificate settings.

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
| Signed by | Nothing in `/etc/kdos/keys` | A `kdos-box freeze` pack: its own signature block, when someone ran `kdos-pack sign`. A `.ktar` set: its index (`PACKAGES.sig`), when `kdos app export` had a key; its packs are not signed (see below) |
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
   must match the index entry. A match is accepted; a mismatch is refused. `kdos-appbox import`
   (behind `kdos app import`) copies a set's `PACKAGES` and `PACKAGES.sig` into the staging
   directory as `PACKAGES.<pid>` and its `.sig` when the archive holds both, names that index in
   each `install` it sends and takes it away when the import ends, so the packs of a set whose
   index was signed with a trusted key are checked here. The daemon reads no index a request does
   not name.
   `kdos-box import` copies only the `.kpack` file it installs.
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
it does not sign the packs themselves. `kdos app import` stages the signed index with the packs, so
a pack of a set whose index verifies against `/etc/kdos/keys/packs` is accepted at step 1 on that
signature. A pack of a set exported without a key, or signed with a key this machine does not
trust, reaches `kdos-packd` as an unsigned pack: it is accepted on its payload hash alone, and
refused outright when `KDOS_REQUIRE_SIG` is set.

Refusing unsigned packs is opt-in. With `KDOS_REQUIRE_SIG` set, `kdos-packd` refuses an unsigned
pack at install (a medium pack is not covered), and `kdos-pack verify` reports an unsigned one as a
failure. It does not cover an OCI image pulled from a registry. `kdos-pack` reads the variable from
the caller's environment. `kdos-packd` reads it from its own, and the shipped init script,
`/etc/init.d/59_packd.sh`, sets none, so turning it on for installs means exporting the variable in
that script before its `supervise` line and restarting the daemon.

## What is not protected

This section lists what the mechanisms above do not do. A reader who assumes a protection that is
not there is worse off than one who knows it is missing.

- **There is no mandatory access control.** SELinux and AppArmor are compiled into the kernel but
  not started (see [Security modules](#security-modules)), and no policy ships. A process running
  as you can do anything you can do.
- **There is no verified boot and no signed kernel.** The boot chain is not measured or attested.
  Anyone with physical access can boot a modified kernel or initramfs and take control of the
  machine. The signed slot image protects what an update writes into the inactive slot, not what
  is on the disk afterwards.
- **Module signatures are not enforced.** Modules are signed, but `CONFIG_MODULE_SIG_FORCE` is off
  and lockdown runs in its `none` mode, so root loads any module, signed or not, and the kernel
  only marks itself tainted (see [Module signing](#module-signing)).
- **The live image's serial console is a root shell.** `/etc/inittab` runs
  `agetty --autologin root` on `ttyS0` (through `/etc/init.d/serial-getty`, which skips a machine
  with no serial port), so anyone with a serial connection to a machine running the live medium is
  root, with no password. The installer removes `--autologin root` from the installed copy, where
  `ttyS0` asks for a login.
- **Single-user mode asks for root's password.** The installed menu's `KDOS (single user)` entry
  and `telinit S` reach `sulogin`, which opens a root shell only for root's password. With root
  locked, as the installer leaves it by default, it opens no shell at all and the boot continues to
  the login screen. Anyone who can edit the kernel command line at the boot menu still gets a root
  shell with `rdinit=/bin/sh`, which runs the initramfs's bash: the physical-access gap above.
- **The live image has known passwords and logs in by itself.** Both `root` and `kdos` have the
  password `kdos` (see [Getting started](../02-user-guide/getting-started.md)), and SDDM logs the
  `kdos` account in at boot and again after a Log Out (`/etc/sddm.conf.d/kde_settings.conf`). So
  `sshd` does not start until it is enabled: the image ships the marker
  `/etc/service.disabled/sshd`, which `rcS` obeys. Opening port 22 is what enables it:
  `kdos-power firewall ssh on`, and the SSH row of the Firewall settings page, remove the marker
  and start `sshd`, and turning `ssh` off puts the marker back and stops it, so a closed port never
  hides a listening daemon. Nothing advertises SSH while port 22 is closed: the `avahi` package
  ships no `ssh` or `sftp-ssh` service file, so mDNS never names a server the firewall drops. The
  installer offers the SSH server, off by default, and removes the marker on the installed system
  when it is chosen. It also asks for a new password for the account and, by default, locks root
  (it can set a root password instead), so an installed system does not carry the known passwords.
- **An automatic login is a login nobody typed.** An installed machine logs in by itself only when
  it has an encrypted root, one account and the answer asked for it, and then the disk passphrase
  is what stands in front of the session; with the default "lock at start" the lock screen asks for
  the account's password as well. Either way the lock screen protects a session only once it has
  been locked, and the wallet is not opened by an automatic login.
- **Disk encryption protects data at rest only.** It is a passphrase typed into the initramfs, not
  a TPM-sealed key, and it does nothing once the machine is running.
- **A box is not a jail.** By default it shares your home directory, your network, your process
  table, `/dev`, `/sys` and `/tmp`. A malicious application in a box can read and destroy your
  files exactly as a native one could, write `~/.config/autostart` or `~/.bashrc` and so decide what
  your next login runs outside every box, and use any device node your account can open. The
  private runtime directory and the filtered bus keep it away from the desktop's interfaces; they
  do not keep it away from your data. `home = private` is the key that does.
- **A box from somebody else's image, or one handed X11, reaches past its tag.** A distrobox box
  shares the session's runtime directory, with `wayland-1` and the unfiltered bus in it, and any box
  handed the X11 display can talk to Xwayland, which is one host client to KWin. Such a box is
  `tagged`, not `confined`, and its badge is dashed.
- **A native application is not sandboxed at all.** An application built from a port, a browser
  or an office suite included, runs as you with no box around it and is offered every Wayland
  global, screen capture and fake input among them. Chromium's renderer sandbox and QtWebEngine's
  confine their own web content; nothing confines the application itself unless you start it under
  [`kdos sandbox`](../04-programs/kdos-command.md#kdos-sandbox), which limits its files and its TCP
  traffic through Landlock; Landlock does not police a Unix socket reached by path, so the program
  still reaches KWin and the session bus.
- **The session bus trusts everything on it.** Every program running as you on the host shares one
  session bus, and several services on it answer any caller: KWin's `ScreenShot2` takes a
  screenshot without asking, KWin's scripting interface loads a script into the compositor, which
  then sees and moves every window, kglobalaccel registers and fires global shortcuts, and once the
  wallet is unlocked the Secret Service hands any caller any secret in it. A box's proxy hides all
  of these; nothing hides them from a native program.
- **A granted box is as privileged as its grant.** `grant = fake-input` hands that box every
  window to type into, `data-control` every clipboard and `screencast` every screen, and nothing
  warns at the moment it is used. The profile, and System Settings' Boxes page, are the only
  record. A box whose home is shared cannot change its own grant, because the profile directory is
  bound read-only into it, but a native program running as you can.
- **`wheel` is effectively root.** `sudo` grants it everything with its own password, and every
  root daemon's socket answers it on every configuration verb, so there is no separation between
  "can change the accent colour" and "can open a port". The polkit rules grant it only at the
  machine, and only the actions named in `50-kdos.rules`; anywhere else it gets the defaults, which
  ask for an administrator's password, and an administrator's password is its own.
- **An OCI base pulled from a registry is unsigned content** from somebody else's server. It is an
  online operation, `KDOS_REQUIRE_SIG` does not cover it, and the tool says so before doing
  anything.
- **An unsigned pack is accepted** unless `KDOS_REQUIRE_SIG` is set, and its hash proves only that
  it was not corrupted. A signature that fails, or that no key on this machine can check, is
  refused, so a pack that is signed but uncheckable is treated more harshly than the same pack with
  no signature block at all.
- **Ports built from source are not signed**, and need not be: a port's integrity is the checksum
  in its recipe. That makes the recipe, and so this repository, the trust root for everything on
  the host. A port installed by building it here runs its recipe's `build.sh` as root on the
  running system (see [Software a person installs](#software-a-person-installs)), so a recipe in a
  tree you trust is code you run as root.
- **Updates are never installed automatically, and there is no security-advisory service.** A
  system timer (`/etc/kdos/timers.d/10-update-check.timer`) runs `kdos update check` once a day
  against what was last fetched, and `kdos-sysupdated` fetches the signed tree and release index,
  and through it the binhost index and image manifest, at most once an hour when Discover asks; Discover's unattended updates are off
  (`UseUnattendedUpdates=false`). Writing the update into the inactive slot is somebody's decision,
  in Discover or with `kdos update apply`, and an administrator's password. A fix reaches a machine
  only once a release carries it. `kdos cve` tells you what is behind a known fix.
- **`kdos-oomd`'s choice between several large processes is tested only against recorded
  process tables.** `kdos-oomd --fixture` exercises victim selection offline, and
  `testing/oomd-fire.sh` makes the daemon fire in a virtual machine against a single memory hog;
  which of several real candidates it kills under real pressure has not been measured.

## See also

- [How KDOS differs](../01-philosophy/how-kdos-differs.md#the-security-model) — this model set against
  the ones other distributions use
- [Boot and init](boot-and-init.md) — the display manager, the consoles and the boot confirmation
- [The session](session.md) — the user's services, the portals and what a box receives
- [KWin](../04-programs/kwin.md#boxes-at-the-compositor) — how the compositor tags and badges a box
- [Packaging](packaging.md) — signing, the binhost index, slot images and how ports are verified
- [Packs and boxes](packs-and-boxes.md) — the pack format, verification, mounting and box profiles
- [kdos-appbox](../04-programs/kdos-appbox.md) — box profiles, the launch path and what a box receives
- [The daemons](../04-programs/daemons.md) — each root daemon's verbs, clients and refusals
- [Known gaps](../06-reference/known-gaps.md) — everything else that does not exist

<!-- book-nav -->
---

*Part III — Architecture, chapter 17.* Previous: [16. Packs and boxes](packs-and-boxes.md) · [Contents](../README.md) · Next: [18. The design language](design-language.md)
