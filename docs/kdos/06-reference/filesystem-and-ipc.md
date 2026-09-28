# Filesystem and IPC

This chapter is the lookup table for a running KDOS system: every path KDOS owns on the installed
machine, every socket its programs talk over with the requests each one accepts, the files one
program writes for another to read, and the environment variables that change behaviour, including
those `make build` and the source-archive scripts read on the build machine. It is for
administrators tracking down where something lives, for people scripting the desktop, and for
contributors who need to know which program owns which file. Read
[Architecture overview](../03-architecture/overview.md) first if you want the picture these paths
belong to; this chapter assumes it.

*IPC* (inter-process communication) here means the Unix sockets and shared files one KDOS program
uses to tell another something. Terms such as *surface*, *chrome*, *accent*, *box*, *pack*,
*graft*, *binhost*, *shim*, *root slot* and *phosphor pass* are defined in the
[Glossary](glossary.md).

The chapter runs from the filesystem to the protocols to the environment. Look a path up under
[The target filesystem](#the-target-filesystem), which is grouped by where a path lives: the
machine's own directories, the boot medium, the boot files, your home directory and the session's
runtime directory. To talk to a daemon, read [Protocol conventions](#protocol-conventions) and then the socket's own section
under [Sockets](#sockets). What each configuration file's keys mean is in
[Configuration](configuration.md); this chapter says only where the file is and who writes it. For
the source tree rather than the installed system, see [Repository layout](repository-layout.md).

## The target filesystem

Paths below are those of the installed or live system, not of the repository. Most per-user paths
written with `~/.config`, `~/.local/share`, `~/.local/state` or `~/.cache` follow
`$XDG_CONFIG_HOME`, `$XDG_DATA_HOME`, `$XDG_STATE_HOME` or `$XDG_CACHE_HOME` when that variable is
set; the table shows the default. These are always under the home directory, whatever the
variables say: `~/.config/kdos/boxes/`, `~/.config/kdos/a11y`, `~/.config/kdos/displays.conf`,
`~/.local/share/kdos/packs/` and `~/.local/share/kdos/observed-app-ids`. A box's directory under
`~/.local/share/kdos/boxes/` is resolved both ways: the launcher places a private home under
`$XDG_DATA_HOME` when it is set, while the pack daemon's writable layer and `kdos-appbox`'s box
subcommands always use the home directory.

### `/etc/kdos/` — machine configuration

| Path | Holds | Shipped |
|---|---|---|
| `login.conf` | The account tty1 logs in automatically (`autologin = kdos`) | yes |
| `menu.conf` | The routes: a stable `verb.noun` name for every place in the system, and the command that opens it | yes |
| `packd.conf` | How many superseded versions of each pack the store keeps (`retain = 1`) | yes |
| `zram.conf` | Compressed-swap size and algorithm | yes |
| `timers.d/` | The system's periodic jobs, plus a `README` | yes: `10-update-check.timer` and `20-fstrim.timer`; the `fwupd` package adds `30-fwupd-refresh.timer` |
| `keys/` | Trusted keys for host packages and the binhost index | yes, with no keys: only a `README` stating the policy |
| `keys/packs/` | Trusted keys for application packs | yes, one key: `kdos-packs.pub` |
| `mountd.conf` | Removable-media options, including `format = yes` and `write = yes`, without which the media daemon refuses to format a device or write an image over one | no: create it |
| `update.conf` | The binhost `kdos update` installs from | no: create it |
| `accent` | The accent name, so the boot splash can repaint in it before anyone logs in | no: written by `kdos-powerd` when `kdos theme` runs |

The host package manager's own configuration is `/etc/kpkg.conf`; see
[`/var/lib/kpkg/` and `/var/cache/kpkg/`](#varlibkpkg--the-host-package-database).

### `/var/lib/kdos/` — state written by the system

| Path | Holds | Written by |
|---|---|---|
| `packs/` | Installed application packs, `<id>.kpack` | The pack daemon, as root |
| `packs/mnt/` | Mount points for packs | The pack daemon |
| `packs/staging/` | The one place an unprivileged download may land, mode 01777 | Any user, and the application store |
| `pack-manifest` | Every graft made, so removing one is exact | The pack daemon |
| `fs-manifest` | Every path the build's static tree (`fs/`) provided | The build |
| `march.ledger` | Measured per-machine optimisation results | `kdos march` |
| `apps-pending` | Applications chosen in the installer that could not be installed | `kinstall` |
| `update.json` | What `kdos update check --json` found; the panel's update badge reads it | The update-check timer, nightly at 04:17 |

### `/usr/share/kdos/` — shipped data

| Path | Holds |
|---|---|
| `appstore/catalogue` | Every application the store can build, one row per image layer: `<kind> <id> <parent> <apt packages…>` |
| `alien-apps` | Application name to the command line that runs it in its box |
| `icons/atlas.kia` | The pre-rendered icon atlas |
| `icons/art/`, `icons/marks/` | The vendored icon source and the KDOS marks |
| `cursors/art/` | The vendored cursor source |
| `gtk-theme/theme/` | The vendored GTK stylesheet source |
| `charnames.idx` | The Unicode character-name index the character picker searches |
| `secdb.txt` | The vendored security-advisory database, read by `kdos cve` |
| `reasons/` | The explanations `kdos why` and `kdos explain` print |
| `doc/` | The help pages the desktop's surfaces open on F1 |
| `logo.txt` | The login banner's picture, with its terminal colour codes; also drawn by the About surface and the lock screen |
| `kdos.png` | The mascot artwork the marks and logo are cut from |
| `screensaver.txt` | The grid the screensaver's `art` and `bounce` effects move |
| `splash.psf` | The boot splash's font |
| `boot/` | Boot artwork: `kdos-backdrop.png`, `kdos-banner.png`, `os_kdos.png` |
| `memtest86plus/` | The memory tester carried on the boot medium |
| `syncthing-offline.xml` | The first-run configuration `kdos-syncthing` gives a user who has none: syncing between machines on one LAN without reaching outside it. Installed by the `syncthing` package |

`screensaver.txt` can be replaced per user at `~/.config/kdos/screensaver.txt`. It is separate from
`logo.txt` so that changing the screensaver does not change the picture the machine boots with. The
screensaver has eight effect names; `art` and `bounce` read this grid, and the other six (`rain`,
`matrix`, `pipes`, `starfield`, `fire`, `clock`) draw without a file.

### `/usr/local/bin/` — session commands and application shims

Holds `kdos` and the other session-side names of the `ksvc` binary, `kdos-appbox` with its names
`kdos-box` and `xdg-open`, and the session scripts from `fs/`; the
[command index](command-index.md#where-the-commands-live) lists them. It is also where the build puts a
*shim*, a symbolic link to `kdos-appbox` named after a boxed application. Running a shim runs
`kdos-appbox`, which dispatches on the name it was called by. The build writes one shim per
application in a pack the medium carries (`kdos-appbox genlaunchers`, from
`script/06_packaging/00_launchers.sh`); the default medium carries no packs, so it has none, and
the same step removes the links committed under `fs/usr/local/bin/`. An application you install
gets its shim in `~/.local/bin` instead.

### `/var/lib/kpkg/` — the host package database

The state and caches of `kpkg`, the host package manager. Every directory is a default in
`/etc/kpkg.conf`, which is a list of `NAME="${NAME:-default}"` lines.

| Path | Holds |
|---|---|
| `/var/lib/kpkg/db/<name>` | One entry per installed package, with its file list |
| `/var/lib/kpkg/db/.recipe/<name>` | The recipe hash a package was installed from |
| `/var/cache/kpkg/sources/` | Downloaded source files (`SOURCE_DIR`) |
| `/var/cache/kpkg/packages/` | Built package archives (`PACKAGE_DIR`) |
| `/var/cache/kpkg/work/` | Build working directories (`WORK_DIR`) |

The ports tree `kpkg` builds from is `/ports/core` (`PORT_REPO`); every port in it is listed in
[The ports catalogue](ports-catalogue.md). Neither `/ports` nor
`/var/cache` is carried onto the boot medium, so on a fresh machine both are empty until something
is built there; a medium made with `KDOS_ISO_SOURCES=1` carries a partial copy of the tree, with no
ports, under `/mnt/iso/sources`.

### The boot medium

A live session mounts its boot medium at `/mnt/iso`. The installer and the pack daemon read it
there.

| Path | Is |
|---|---|
| `/mnt/iso/system.sfs` | The root filesystem image |
| `/mnt/iso/packs/` | Packs available on the medium; the pack daemon lists them as `available` without copying them |
| `/mnt/iso/packs/PACKAGES` | The pack index the medium carries |
| `/mnt/iso/appstore/catalogue` | The installer's fallback copy of the application catalogue |
| `/mnt/iso/sources/` | The KDOS tree that built the image, present only on a medium built with `KDOS_ISO_SOURCES=1`; `kdos rebuild` reads it |

### `/run/` — system runtime

Emptied at every boot.

| Path | Is |
|---|---|
| `kdos-powerd.sock`, `kdos-energyd.sock`, `kdos-oomd.sock`, `kdos-mountd.sock`, `kdos-packd.sock` | The root daemons' sockets; see [Sockets](#sockets) |
| `<name>.pid` | The process ID of the `ksvc` supervisor running service `<name>`. The supervisor leads its own process group, so stopping the service kills the daemon under it too |
| `kdos-svc.<name>.log` | A supervised service's output, capped at 64 KiB with one `.old`. Every line also goes to syslog |
| `kdos-init.<name>.log` | What a service's start script printed at boot |
| `kdos-getty.<tty>.log` | `kdos-getty`'s messages for that terminal |
| `kdos-fsck.log` | What the boot-time filesystem check reported, written only when it said something |
| `kdos-medium` | Where the installer binds the boot medium while installing |

### Boot files

An installed machine set up for A/B updates has two root filesystems, slots `a` and `b`. A *trial
boot* starts the candidate slot, and the machine keeps it only when `kdos-bootctl mark-good` runs at
the end of a successful boot; otherwise the next boot returns to the active slot. See
[A/B slot selection](../03-architecture/boot-and-init.md#ab-slot-selection).

| Path | Is |
|---|---|
| `/boot/initramfs.cpio.gz` | The image's initramfs |
| `/boot/initramfs.modules` | The kernel modules the initramfs carries, one per line |
| `/boot/initramfs-kdos.cpio.gz` | The image's initramfs with a newer kernel's modules appended, written when the `linux` package is upgraded |
| `/boot/efi/EFI/kdos/<slot>/` | Each A/B root slot's `vmlinuz` and `initramfs.cpio.gz`, on the EFI system partition, for slots `a` and `b` |
| `/boot/efi/EFI/kdos/vmlinuz`, `initramfs.cpio.gz` | A flat pair that boots either slot. Nothing in KDOS writes one (`kinstall` puts slot `a`'s pair in `EFI/kdos/a/`); where one exists, `kdos-bootctl` boots it for a slot with no directory of its own and deletes it once no menu entry names it |
| `/boot/efi/EFI/kdos/bootstate` | Each slot's root UUID and encrypted container, which slot is active, and whether a trial boot is pending with its attempt count; `kdos-bootctl` owns it |
| `/boot/efi/EFI/kdos/trial/` | A trial boot's copy of the loader, present only while a trial is pending |

### Per user

| Path | Holds |
|---|---|
| `~/.config/kdos/` | Your desktop configuration; every file's keys are in [Configuration](configuration.md) |
| `~/.config/kdos-comp/` | The compositor's own configuration (`rc.xml`) |
| `~/.config/kdos/boxes/<name>.conf` | A box's profile |
| `~/.config/kdos/sandbox/<profile>.conf` | A `kdos sandbox` profile |
| `~/.config/kdos/displays.conf` | The screen layout `kdos-display` keeps and replays at login |
| `~/.config/kdos/places` | Extra places, beside the standard folders, in the lists the Start menu, the desktop and the file picker show |
| `~/.config/kdos/favorites` | The pinned applications, one desktop-entry ID per line; the taskbar and the Start menu both read it |
| `~/.config/kdos/timers.d/` | This account's periodic jobs; the default account ships `10-backup.timer` and `20-updatedb.timer` |
| `~/.config/kdos/screensaver.txt` | Your own screensaver grid, in place of the shipped one |
| `~/.config/kdos/session-restore` | Present means the next login relaunches the boxed applications the last session left running |
| `~/.config/kdos/a11y` | Present means boxed applications start with the accessibility stack |
| `~/.local/share/kdos/alien-apps` | Your own launcher table; its entries win over the system one |
| `~/.local/share/kdos/boxes/<name>/` | A box's writable layer (`upper`, `work`), its `snapshots/<tag>/`, and its home when the profile says `home = private` |
| `~/.local/share/kdos/packs/` | Where a data pack's contents are grafted for boxed applications |
| `~/.local/share/kdos/doc/` | Your own help pages, searched beside `/usr/share/kdos/doc` |
| `~/.local/share/kdos/observed-app-ids` | Every application ID the compositor has seen a window present, read by `kdos appid` |
| `~/.local/share/kdos/run-history` | Commands typed into the Run dialog, one per line, newest last, at most 50. A command run again moves to the end |
| `~/.local/share/kdos/scratch.txt` | The scratch note `kdos-note` keeps |
| `~/.local/share/applications/` | Launchers for applications you installed |
| `~/.local/bin/` | Shims for applications you installed |
| `~/.local/state/kdos/appusage` | Launch counts, which order the Start menu's frequent column |
| `~/.local/state/kdos/startcat` | The Start menu category last shown |
| `~/.local/state/kdos/toggles/` | One empty file per switch that is on: `stay-awake`, `night-light`, `dnd` |
| `~/.local/state/kdos/session` | What was running when the session ended: `app <name>` per boxed application, `native <app_id>` per host program. Written by `kdos-session-save` |
| `~/.local/state/kdos/winpos` | Where the compositor last saw each application's window |
| `~/.local/state/kdos/diskwarn` | `<step> <mountpoint>` per line: which disk-full warnings the panel has already shown |
| `~/.local/state/kdos/update.json` | What `kdos update check --json` found, when you ran it yourself |
| `~/.cache/kdos/theme` | One word: the accent name. The whole theme state the desktop reads |
| `~/.cache/kdos/wallpaper.png` | The accent-coloured wallpaper, which the compositor draws in preference to the configured one |
| `~/.cache/kdos/plocate.db` | This account's file index, rebuilt nightly from `$HOME` by `kdos-updatedb`; `LOCATE_PATH` names it |
| `~/.local/share/Trash/` | The freedesktop trash |
| `~/Mail/` | The account's Maildir |
| `~/Recordings/` | What `kdos-rec` recorded, as `YYYY-MM-DD-HHMMSS.wav`, created on first use |
| `~/.local/share/whisper.cpp/models/` | Speech models, `ggml-*.bin`. `kdos speech get` writes here; the desktop only searches it |

Each `winpos` line is `app_id x y w h workspace shaded`, in pixels, most recent first, at most 200
lines. It is read when a window opens and written when it closes, only while `comp.conf`'s
`window_memory` is on. A window with a parent, such as a file chooser, is neither recorded nor
restored.

Only the `app` lines of `session` are relaunched, and only when `session-restore` exists. Where each
window reappears is the business of `winpos`, not of this file.

A speech model is looked for in the order `$KDOS_WHISPER_MODEL`, then
`~/.local/share/whisper.cpp/models/`, then `/usr/share/whisper.cpp/models/`.

`~/Mail` is named by `~/.mbsyncrc` and by notmuch's `mail_root`, and is created for the default
account when the image is built. Both programs report an error on a missing directory rather than
creating one, so a new account needs `mkdir ~/Mail`.

`~/Recordings` is not an XDG user directory: the freedesktop set has no such entry, and
`user-dirs.dirs` does not invent one.

### `$XDG_RUNTIME_DIR` — per session

Normally `/run/user/<uid>`, mode 0700, on a tmpfs that does not survive a reboot. Boxes share this
directory with the session.

| Path | Is |
|---|---|
| `bus` | The session message bus (D-Bus), at a fixed path because boxes share this directory |
| `.kdos-bus.lock` | Serialises starting that bus |
| `wayland-*` | The compositor's Wayland socket |
| `kdos-cmd.sock` | The compositor's command socket |
| `kdos-frames.sock` | Late-frame reports from the compositor |
| `kdos-notify.sock` | The notification daemon |
| `kdos-clip.sock` | The clipboard history |
| `kdos-box-<box>@<display>.sock` | The Wayland socket `kdos-boxsock` serves for one box; the compositor tags every client that connects on it with that box |
| `kdos-box-<box>@<display>.lock` | Held by that `kdos-boxsock` for its whole life, so a second one for the same box exits at once |
| `ssh-agent.socket` | The account's `ssh-agent`, started by the first login shell that finds nothing answering there; `SSH_AUTH_SOCK` names it |
| `podman/podman.sock` | The rootless Podman API, while `podman system service` runs; `kdos-podman-api` starts it on demand |
| `kdos-panel.overflow` | What the panel has placed behind its chevron |
| `kdos-comp.log` | The compositor's output |
| `kdos-thumb.ppm` | The last window picture the compositor's `thumb` request wrote |
| `kdos-osd.lock`, `kdos-osd.what` | The on-screen volume and brightness display: the process ID of the one on screen, and what the next key press asks it to show |
| `kdos-appbox.trace` | Stage timings for the most recent boxed launches |
| `kdos-appbox.warmup.lock`, `kdos-appbox.create.lock` | Serialise the login warm-up and box creation, so two launches do not build one box twice |
| `kdos-sfx.route` | Which player the sound effects use, cached; only `pw-cat` is cached |
| `kdos/boxes/<box>/root` | The merged root of a box composed from packs |
| `kdos/boxes/<box>/upper`, `work` | A composed box's writable layer when `$HOME` cannot hold one (a live session, whose home is itself on an overlay); lost at logout, and the launcher says so |
| `kdos/timers.pid` | The process IDs of this session's timer loops, one per line |
| `kdos/nowplaying` | One line naming what is playing, written by `kdos-mpctl watch` and read by the panel |
| `kdos/screencast.pid` | The process ID of the running `kdos-record`, while it runs |

## Protocol conventions

The five root daemons share one shape:

- **One socket in `/run`, mode 0666.** Anyone can connect; the mode is not the gate.
- **Authorisation is the caller's user ID, read from the kernel** (`SO_PEERCRED`), never anything in
  the message. The allowed users are root plus members of a group; each socket's section says
  which.
- **One request per connection:** a short line in, a reply out, then the socket closes. There is no
  session state. The exceptions are `kdos-mountd`'s `subscribe`, which keeps the connection open,
  and the three mountd verbs that carry a secret as a second frame after the request line.
- **Replies** are `ok`, optionally followed by data, or `err <reason>`. A caller who is not allowed
  gets `err not permitted`, and a verb the daemon does not know gets `err unknown command`.
- **Each verb takes a fixed number of words**, with two exceptions: `compose` on `kdos-packd`
  takes a box name and up to 32 pack IDs, and `firewall` on `kdos-powerd` takes `list` or a
  service and `on`/`off`. `kdos-mountd` refuses a longer line rather than truncating it:
  `mount 0 rm -rf /` is an unknown command, not `mount 0`. The other daemons do not all hold to
  that: `kdos-powerd`'s `firewall` ignores words after its second, and `kdos-packd` stops reading
  a line after 34 words and acts on what it has.
- **No verb takes a path.** Where a request names a thing, it names it by an identifier the daemon
  published in a list. The single exception is the pack daemon's `install`, which names a file in
  its own staging directory.
- **Test modes.** `kdos-energyd`, `kdos-oomd`, `kdos-mountd` and `kdos-packd` accept `--fixture`,
  which reads recorded state instead of the machine's and prints what it would do instead of doing
  it. `kdos-mountd --fixture-serve` serves the socket over that recorded state, so the verbs
  themselves can be exercised. `kdos-powerd --explain USER` prints which verbs that user may use.

You can try any of these by hand with `socat`, which the image ships:

```sh
printf 'ping\n' | socat - UNIX-CONNECT:/run/kdos-oomd.sock
```

The session sockets under `$XDG_RUNTIME_DIR` belong to your own session and use their own shapes,
described with each one.

## Sockets

### `/run/kdos-powerd.sock`

Power and machine settings, served by `kdos-powerd`; the client is `kdos-power`, the same binary
under a second name. Two tiers of caller:

- root, and members of `seat` or `wheel`: `suspend`, `poweroff`, `reboot`, `ping`
- root and members of `wheel` only: `firewall`, `autologin`, `accent`, `timezone`

`seat` is the group that owns the display, so the person at the machine can always suspend and shut
down; changing configuration needs an administrator. The list of seat verbs names what is allowed,
so any verb added later is an administrator's by default.

| Verb | Argument | Does |
|---|---|---|
| `suspend` | none | Answers `ok`, then suspends to RAM |
| `poweroff` | none | Answers `ok`, asks process 1 to power off, and forces it after 60 seconds if init did not |
| `reboot` | none | The same, rebooting |
| `ping` | none | Liveness |
| `firewall` | `list`, or `<service> on` / `<service> off` | Lists the firewall's named services, one row each, or opens or closes one, rewriting `/etc/nftables.d/50-kdos-services.nft` and reloading the ruleset after `nft --check` passes |
| `autologin` | An account name, or `off` | Rewrites `/etc/kdos/login.conf` |
| `accent` | One of the eight accent names: `phosphor`, `amber`, `ice`, `bone`, `norton`, `borland`, `perfect`, `paper` | Writes `/etc/kdos/accent`, then runs `kdos-bootctl theme`, which recolours the boot menu and `/etc/vtrgb`. It does not retint the desktop; `kdos theme` does that |
| `timezone` | `Area/City` | Writes `/etc/localtime`, `/etc/profile.d/20-timezone.sh`, and the Wi-Fi country in `/etc/modprobe.d/kdos-regdom.conf` |

`kdos-power suspend` locks the session before it asks, unless it is given `--no-lock` or
`KDOS_NO_LOCK_ON_SUSPEND` is set. It sends `ping` first, so a caller who may not suspend is refused
before the screen is locked.

The firewall takes names, never ports: a client that can only name `ssh` can open exactly what the
daemon's table says `ssh` is.

### `/run/kdos-energyd.sock`

Per-application energy use, from `kdos-energyd`; the client is `kdos-energy`. Root and members of
`wheel`.

| Verb | Argument | Answers |
|---|---|---|
| `report` | none | Each application's share of attributable energy, the idle floor, and the sample count |
| `report-json` | none | The same, as JSON |
| `ping` | none | Liveness |

The raw energy counter and the sampling interval are never published, and the interval is fixed by
the daemon, so this socket cannot be turned into a fine-grained power measurement instrument (a
known side channel). The daemon refuses to start on a machine whose energy counter it cannot read,
rather than report a machine that uses no energy.

### `/run/kdos-oomd.sock`

The out-of-memory killer, `kdos-oomd`. Root, and members of `seat` or `wheel`. The daemon waits on
a kernel pressure-stall information (PSI) trigger: a threshold on the time tasks spend stalled for
memory.

| Verb | Argument | Answers |
|---|---|---|
| `status` | none | `ok trigger '<psi trigger>', kills <n>`, and the last kill when there has been one |
| `ping` | none | Liveness |

Nothing in this protocol names a process, so it cannot be used to aim a kill. Killing is the
daemon's own decision or it does not happen.

### `/run/kdos-mountd.sock`

Removable media, encrypted volumes, network shares and drive health, from `kdos-mountd`; the client
is `kdos-mount`. Root, and members of `seat` or `wheel`. Sixteen verbs. Removable media are mounted
under `/media`.

Four verbs carry a second frame; see [Secrets](#secrets). One, `write`, also carries an open file
descriptor.

| Verb | Argument | Answers |
|---|---|---|
| `list` | none | The devices it will act on, with an index each |
| `mount` | An index | The mountpoint |
| `unmount` | An index | |
| `eject` | An index | |
| `close` | An index | Closes an encrypted volume that `unlock` opened |
| `smart` | An index | The drive's own health summary |
| `unlock` | An index and a byte count | The name of the unlocked device; the passphrase follows as a second frame |
| `format` | An index, a filesystem and a byte count | The device's kernel name, typed back, follows as a second frame. Refused unless `/etc/kdos/mountd.conf` says `format = yes` |
| `write` | An index and a byte count | Writes an image over the whole disk the row is on and reads it back. The disk's name, typed back, follows as a second frame, and the image is attached to the request as an open descriptor (`SCM_RIGHTS`), never named. The connection stays open for `progress write\|verify <done> <total>` lines and ends `ok <disk> <bytes> verified`. Refused unless `/etc/kdos/mountd.conf` says `write = yes` |
| `cifs` | A server, a share, a username, a domain and a byte count | The mountpoint; the password follows as a second frame |
| `krb5` | A server, a share, a username or `-`, and a domain or `-` | The mountpoint. Uses your Kerberos ticket; no second frame |
| `shares` | none | The mounted network shares, with an index each |
| `browse` | none | The file servers that answered an mDNS and a NetBIOS broadcast, as `name<TAB>address` |
| `disconnect` | A share index | Unmounts one share |
| `subscribe` | none | Keeps the connection open and writes a line for each block-device event |
| `ping` | none | Liveness |

#### Indexes

The client asks for an index from a list the daemon published, and the daemon decides the device,
the mountpoint and the options. The device list is rescanned on every request. A share index
belongs to a different list, `shares`, built from `/proc/mounts` on every request; a `disconnect`
index is checked against that list only. An index is meaningful only for the list it came from.

A verb that writes to a device refuses the medium the session booted from, anything currently
mounted, and any device whose node differs from the one the scan recorded.

#### Secrets

`unlock`, `format`, `write` and `cifs` carry their secret or confirmation as a *second frame*: the
request line ends with a byte count, and exactly that many bytes follow the newline. A secret is never a word on the request
line, because that line is split on whitespace and a passphrase may contain some. The byte count
may not be zero. The daemon holds the secret in one buffer and wipes it on every way out of the
request.

#### Network shares

Each of the four names `cifs` takes must pass a character allowlist; anything else is refused
rather than quoted. `mount.cifs` builds its option string by concatenation and escapes only the
password, so a comma in a server, share, username or domain would become a new mount option, and a
`/` or `\` in a server name would redirect the mount.

The password reaches `mount.cifs` on a file descriptor (`PASSWD_FD=0`, with the bytes on the
helper's standard input): not on its command line, where any process could read it, not in its
environment, and not in a file someone must delete.

`krb5` sends no secret: the Kerberos ticket is in the caller's credential cache. The daemon adds
`sec=krb5` and `cruid=<the caller>` to the options, so the kernel's `cifs.upcall` helper reads the
caller's tickets rather than root's. Without `cruid` the mount fails with `Required key not
available`. The helper and its `request-key` rule are checked first, so a machine that cannot do
Kerberos mounts says which file is missing. No password descriptor is passed, so `mount.cifs` does
not wait for one.

When the C library cannot resolve a server name, the daemon tries once more: a `.local` name
through `avahi-resolve-host-name`, a bare NetBIOS name through `nmblookup`. The mount is given the
address as `ip=` beside the name you typed, so the mountpoint keeps the name.

`browse` rows are not indexes. `cifs` and `krb5` take a server name, so a row carries the name with
its address beside it, which also tells apart two machines with the same NetBIOS name on different
subnets.

### `/run/kdos-packd.sock`

The pack daemon, `kdos-packd`, which installs, mounts and composes application packs. Root and
members of `wheel`. Thirteen verbs.

| Verb | Argument | Answers |
|---|---|---|
| `list` | none | Every pack the machine can see, one row each: ID, version, kind, state (`mounted`, `installed` or `available`), size, and origin (`store` or `medium`) |
| `info` | A pack ID | Its metadata |
| `mount` | A pack ID | |
| `unmount` | A pack ID | |
| `compose` | A box name, then one or more pack IDs | The merged root of those packs, for that box |
| `decompose` | A box name | |
| `install` | A file name in the staging directory | |
| `remove` | A pack ID | |
| `rollback` | A pack ID | |
| `graft` | A data pack ID | |
| `ungraft` | A data pack ID | |
| `status` | none | The store, medium and staging directories, the retention count, how packs are mounted (`file-backed`, `loop device`, or `not yet used` before the first mount), pack and box counts, and each composed box's merged root, marked `ephemeral` or `persistent` |
| `ping` | none | Liveness |

`status` publishes the staging directory and the retention count, so a client writing a download
does not have to work either out.

Every ID comes from `list`. The single exception is `install`, which names a file in the daemon's
own staging directory, `/var/lib/kdos/packs/staging`, the one place an unprivileged user may
write. Relative traversal and absolute paths are both refused. Installing is a rename out of that
directory after the pack is verified. Signatures are checked at install and at mount against
`/etc/kdos/keys/packs/`; see [Packs and boxes](../03-architecture/packs-and-boxes.md).

A composed box's merged root is `$XDG_RUNTIME_DIR/kdos/boxes/<box>/root`, and its writable layer
is `~/.local/share/kdos/boxes/<box>/upper` where the home directory's filesystem can hold an
overlay upper, and a tmpfs directory beside the root where it cannot. A `graft` makes symbolic links
under `/usr/share` for the host and under `~/.local/share/kdos/packs` for boxes, and records each
link in `/var/lib/kdos/pack-manifest`; `ungraft` removes only links, only those the manifest names.

### `$XDG_RUNTIME_DIR/kdos-cmd.sock` — the compositor

The compositor's command socket. One JSON object per line in, one JSON object per line out, then
the connection closes. The caller's user ID must be the compositor's own, because this socket
performs actions; it cannot tell a boxed application from a host one, since both run as you.
`kdos hey` is the command-line front end.

| Request | Does |
|---|---|
| `{"cmd":"list"}` | Every window: `id`, `app_id`, `title`, `workspace`, geometry, `pid`, `box`, `instance`, and whether it is focused, minimised, maximised, fullscreen or shaded |
| `{"cmd":"outputs"}` | The outputs: name, size, scale, position, and whether each is enabled |
| `{"cmd":"boxes"}` | The distinct boxes with a window on screen |
| `{"cmd":"run","action":"Close","id":7}` | Runs a compositor action on a window |
| `{"cmd":"peek","on":true}` | Fades every window to reveal the desktop; `false` (or no `on`) restores them |
| `{"cmd":"thumb","app_id":"foot","w":64,"h":36}` | Writes a picture of that application's front window to `$XDG_RUNTIME_DIR/kdos-thumb.ppm` and answers with the path |

Replies carry `"ok":true`, or `"ok":false` with the reason in `"err"`; an unknown or incomplete
action is always answered, never silently ignored. A window ID is the compositor's creation number,
unique for the session. The `thumb` path is chosen by the compositor, never by the request, and a
window whose pixels live on the GPU has no picture (`ok:false`).

The socket never blocks the compositor. A request longer than 4096 bytes is refused, at most eight
clients are served at once, and a client that sends or reads nothing for five seconds is dropped.
It keeps no history and offers no subscription; for a stream of events use `kdos-frames.sock`.

### `$XDG_RUNTIME_DIR/kdos-frames.sock` — late frames

Written by the compositor, read by `kdos stutter` and the panel's stutter indicator. A reader is
first sent a `hello` line naming the compositor and wlroots versions, then one JSON object per late
frame (`"event":"miss"`) carrying the output, how late it was, the compositor's own render cost,
the refresh rate, and a `source` field saying whether the gap was measured at presentation (what
you saw) or at the frame clock (what the compositor was given). A frame is late when it arrives
more than 1.5 refresh intervals after the one before.

Both ends are non-blocking. A reader that cannot keep up loses lines, and there is no history: a
reader that connects late has missed what happened. Keeping the compositor's frame loop fast comes
first.

### `$XDG_RUNTIME_DIR/kdos-notify.sock` — notifications

The notification daemon, `kdos-notifyd`. One text request per connection. The socket is mode 0600
and has no credential check beyond that and the runtime directory's own mode.

| Verb | Does |
|---|---|
| `count` | One line, `<unseen> <total> <dnd>` |
| `list` | The history, newest first, one tab-separated row per entry (index, time, urgency, application, summary, body), then `ok` |
| `seen` | Clears the unseen count |
| `open <index>` | Activates the entry at the index `list` printed |
| `forget <index>` | Removes the entry at the index `list` printed |
| `clear` | Empties the history |
| `dnd` | Toggles Do Not Disturb; `dnd on`, `dnd off` and `dnd toggle` are explicit. Answers `1` or `0`: the state after the request |
| `dismiss` | Puts the newest notification on screen away |
| `dismiss all` | The same for every notification on screen |
| `raise` | Brings back the most recent notification to leave the screen, whether it was dismissed, expired or closed |

Do Not Disturb is the `dnd` toggle file under `~/.local/state/kdos/toggles/`, so `kdos toggle dnd`
and this verb change the same switch. A dismissal reports reason 2, "dismissed by the user", to the
program that sent it. `raise` takes the entry out of the history, so a notification is on screen or
in the centre, never both, and it comes back without its buttons: the original notification is
closed, and its actions belonged to the program that sent it.

### `$XDG_RUNTIME_DIR/kdos-clip.sock` — the clipboard

The clipboard history daemon, `kdos-clip`; `kdos-clip --pick` is the picker that draws it. The
daemon keeps up to 32 text entries of at most 64 KiB each, in memory only: nothing is written to
disk. The socket is mode 0600. One text request per connection.

| Verb | Does |
|---|---|
| `list` | One line per entry, `index<TAB>length<TAB>preview`, then `ok` |
| `count` | The number of entries |
| `get <n>` | Entry *n*'s full text |
| `set <n>` | Makes entry *n* the current selection |
| `forget <n>` | Removes entry *n* |
| `clear` | Empties the history |

Any other request gets `err unknown command`. A socket path too long for the socket address
structure is refused rather than truncated, since a truncated path could bind a different socket.

## Files used as an interface

Neither configuration nor storage: these files are how one program tells another something.

| File | Written by | Read by |
|---|---|---|
| `~/.cache/kdos/theme` | `kdos theme` | The compositor, the panel and every surface, the terminal, the lock screen and the portal: one word |
| `~/.cache/kdos/wallpaper.png` | `kdos theme` | The compositor, in preference to the configured wallpaper |
| `~/.local/state/kdos/toggles/<name>` | `kdos toggle`, and the notification daemon's `dnd` verb | The compositor's idle policy (`stay-awake`); every surface on the retint signal, the `SIGHUP` `kdos toggle` sends to make surfaces re-read the theme and repaint (`night-light`); the notification daemon (`dnd`) |
| `$XDG_RUNTIME_DIR/kdos-panel.overflow` | The panel | The overflow popup |
| `$XDG_RUNTIME_DIR/kdos/nowplaying` | `kdos-mpctl watch` | The panel |
| `/usr/share/kdos/alien-apps` | The build (`kdos-appbox genlaunchers`) | Every shim, and the launcher dispatcher |
| `/usr/local/bin/<name>`, `~/.local/bin/<name>` | The build, one symbolic link to `kdos-appbox` per application in a pack the medium carries (none on the default medium); `kdos app install` for an application you install | You, as an ordinary command; `kdos-appbox` dispatches on the name it was called by |
| `~/.local/share/kdos/alien-apps` | `kdos-appbox genlaunchers --user` | The same, winning over the system table |
| `~/.local/share/kdos/observed-app-ids` | The compositor, once for each new application ID a window presents | `kdos appid` |
| `/var/lib/kdos/pack-manifest` | The pack daemon | Itself, so ungrafting removes exactly what was added |
| `/var/lib/kdos/update.json` | The update-check timer | The panel's update badge, which falls back to `~/.local/state/kdos/update.json` |
| `/etc/kdos/accent` | `kdos-powerd`'s `accent` verb | The boot scripts, which tell the running splash to repaint in it |
| `/boot/initramfs.modules` | The initramfs build step, one module per line | The `linux` package's post-install, which carries a new kernel's copies of that set into `/boot/initramfs-kdos.cpio.gz` |
| `/boot/initramfs-kdos.cpio.gz` | The `linux` post-install: the image's initramfs with the new kernel's modules appended | `kdos-bootctl deploy` and `kinstall`, which use it in place of `/boot/initramfs.cpio.gz` |
| `EFI/kdos/<slot>/vmlinuz`, `EFI/kdos/<slot>/initramfs.cpio.gz` on the EFI system partition | `kinstall` for slot `a`, `kdos-bootctl deploy` for either | The boot loader (Limine), through the `/KDOS` menu entries `kdos-bootctl` regenerates on every change of boot state |
| `EFI/kdos/trial/` on the EFI system partition: a copy of the loader and a `limine.conf` defaulting to the candidate slot | `kdos-bootctl try` on UEFI; removed by every state that is not a pending trial | The firmware, for the one boot `BootNext` asks for |
| `Boot####` "KDOS update trial" and `BootNext` in `efivarfs` | `kdos-bootctl try` on UEFI; removed by `kdos-bootctl mark-good` | The firmware, once |
| `$XDG_RUNTIME_DIR/kdos/screencast.pid` | `kdos-record`, while its recording runs | The next `kdos-record`, which stops that one |

The overflow file is published rather than recomputed so the popup shows exactly what the panel
decided. `kdos-record` checks that the process `screencast.pid` names is still alive, because a
file left by a crash is not a recording. The panel's screen-recording lamp does not read this file:
it counts PipeWire screen-cast streams, so it lights for any screen share.

## Environment variables

KDOS programs and build scripts read more than a hundred distinct `KDOS_*` variables (counted as
`getenv` calls and shell expansions across `src/`, `fs/`, `script/`, `ports/` and `testing/`). The
tables below list those a person may want to set; header guards, internal constants and variables
set only between KDOS's own programs are left out.

### Debugging

| Variable | Effect |
|---|---|
| `KDOS_COMP_DEBUG=1` | Raise the compositor's log level from INFO to DEBUG |
| `KDOS_PANEL_DEBUG=1` | The panel explains its layout decisions, such as why a tile declined |
| `KDOS_BB_DEBUG=1` | The `kdos-bb` demo reports how its audio mixer is fed |
| `KDOS_WHEEL_DEBUG=1` | Trace every scroll-wheel event, tick and dropped duplicate in a `libkwl` surface |
| `KDOS_CRT_DUMP=<prefix>` | Write the phosphor effect's input and output images once, as `<prefix>-in.ppm` and `<prefix>-out.ppm` |
| `KDOS_CRT_DUMP_FRAME=<n>` | Wait until frame *n* before that dump |
| `KDOS_PACKD_VERBOSE=1` | Under `kdos-packd --fixture`, print every step rather than only the result |

### Behaviour

| Variable | Effect |
|---|---|
| `KDOS_A11Y` | `1` starts one boxed launch with the accessibility stack, `0` starts it without; unset, `~/.config/kdos/a11y` decides |
| `KDOS_BOX` | Set inside every box to its name. It is what identifies which box an X11 window came from |
| `KDOS_REQUIRE_SIG=1` | `kdos-packd` refuses to `install` a pack with no signature, and `kdos-pack verify` fails on one. An unsigned pack already on the boot medium still mounts |
| `KDOS_PACK_KEY=<file>` | The private key the application store signs exported pack indexes with; without it the index is unsigned and says so |
| `KDOS_BINHOST=<dir>` | The binhost `kdos update` installs from, overriding `/etc/kdos/update.conf` |
| `KDOS_SOURCES=<dir>` | The KDOS tree `kdos rebuild` builds from; otherwise the first of `/mnt/iso/sources`, `/kdos` and the current directory that is one |
| `KDOS_WHISPER_MODEL=<file>` | One speech model, named exactly. When set, no directory is searched after it |
| `KDOS_NO_ANIM=1` | Skip the login banner's animation |
| `KDOS_BANNER_DELAY=<seconds>` | The delay between the login banner's raster lines; default `0.012` |
| `KDOS_BANNER_LOGO=<file>` | A picture for the login banner in place of `/usr/share/kdos/logo.txt` |
| `KDOS_NO_SFX=1` | Silence sound effects |
| `KDOS_NO_LOCK_ON_SUSPEND=1` | `kdos-power suspend` does not lock the session first |
| `KDOS_ASCII=1` | Force the plainest, ASCII-only glyph set |
| `KDOS_MARCH_RUNS=<n>` | How many samples `kdos march` takes per measurement: default 5, clamped to 3–31 and made odd |
| `KDOS_MARCH_LEVEL=<level>` | The micro-architecture level `kdos march` measures, in place of the best one the CPU supports |
| `KDOS_WHEEL_MIN_MS=<ms>` | The window within which a duplicate scroll-wheel event is dropped; default 20, and `0` disables the filter |

### Testing seams

These point a program at recorded state instead of the live machine. They are for tests and are
honoured only where noted.

| Variable | Points at |
|---|---|
| `KDOS_DUMP_SIZE=WxH` | The size the test harness's offscreen render (`--dump`) draws a surface at |
| `KDOS_GOLDEN_UPDATE=1` | Regenerate reference frames instead of comparing against them |
| `KDOS_RES_FIXTURE` | A recorded system state for the resource monitor's library tests; the program itself takes `kdos-res --fixture <dir>` |
| `KDOS_PRIVACY_PROC` | A recorded process tree for the panel's privacy indicator and the devices surface |
| `KDOS_PANEL_NOW` | A fixed wall-clock second for every surface that draws the time, so a frame with a clock in it can be compared |
| `KDOS_ETC`, `KDOS_ZONEINFO`, `KDOS_CHRONY`, `KDOS_UPDATE_JSON`, `KDOS_CVE_JSON`, `KDOS_SLOT_TEXT`, `KDOS_FIREWALL_LIST`, `KDOS_DISPLAY_LIST`, `KDOS_CAL_LIST`, `KDOS_CONTACT_LIST`, `KDOS_CHARIDX` | Files standing in for the system files, command output and indexes the panel's surfaces read, so an offscreen render has fixed input |
| `KDOS_ENERGY_PROC`, `KDOS_ENERGY_POWERCAP`, `KDOS_ALIEN_APPS` | Recorded trees and a launcher table for the energy daemon |
| `KDOS_BOX_PROFILES` | A directory of box profiles for the out-of-memory killer |
| `KDOS_MOUNTD_SYS`, `KDOS_MOUNTD_DEV`, `KDOS_MOUNTD_FSTAB`, `KDOS_MOUNTD_MOUNTS` | Recorded state for the media daemon |
| `KDOS_MOUNTD_CONF`, `KDOS_MOUNTD_MEDIA`, `KDOS_MOUNTD_UEVENT` | Its configuration file, its mount root, and a FIFO standing in for the kernel's device-event socket |
| `KDOS_PACK_STORE`, `KDOS_PACK_MEDIUM`, `KDOS_PACK_MANIFEST`, `KDOS_PACK_SHARE`, `KDOS_PACK_HOME` | Alternative pack store, medium, manifest, host graft root and home directory for the pack daemon |
| `KDOS_PACK_RETAIN` | A retention count in place of `packd.conf`'s `retain` |
| `KDOS_KEYS` | An alternative pack key directory for `kdos-pack` and the pack daemon's fixture |
| `KDOS_POWERD_ETC`, `KDOS_POWERD_ZONEDIR` | An alternative `/etc` and time-zone database for `kdos-powerd`'s configuration verbs |
| `KDOS_INITRD` | An alternative boot image, for the microcode check |
| `KDOS_BOOTSTATE` | An alternative boot-state file. With it, no firmware variable is written unless `KDOS_EFIVARS` is also set, and no kernel command line is read unless `KDOS_CMDLINE` is |
| `KDOS_CMDLINE` | A file standing in for `/proc/cmdline`, whose `kdos_slot=` tells `kdos-bootctl mark-good` which slot's kernel is running |
| `KDOS_EFIVARS`, `KDOS_ESP_DISK` | A directory standing in for `efivarfs`, and `<disk image>:<partition number>` standing in for the EFI system partition's disk, for testing the `BootNext` trial |
| `KDOS_LIMINE_CONF`, `KDOS_VTRGB` | Alternative boot-menu configuration and console palette files for `kdos-bootctl` |
| `KDOS_POWERD_SOCKET`, `KDOS_ENERGYD_SOCKET`, `KDOS_OOMD_SOCKET`, `KDOS_MOUNTD_SOCKET`, `KDOS_PACKD_SOCKET` | Move a daemon's socket. This grants nothing, since authorisation never depends on the path |

The media daemon's seams are read only under `--fixture` or `--fixture-serve`, so a variable
inherited from the boot environment has no effect on a running system.

### Build

Read by `make build` and the build scripts on the build machine; see
[The build system](../05-developer/build-system.md), and
[How KDOS is built](../05-developer/how-kdos-is-built.md) for the build these variables steer.

| Variable | Effect |
|---|---|
| `KDOS_ISO_SOURCES=1` | Copy `src/` and `script/` onto the boot medium as `/sources`, with an empty `ports/` directory and a `SOURCES` stamp; a live session sees it at `/mnt/iso/sources`. The `Makefile`, the `Dockerfile` and `fs/` are not on the medium |
| `KDOS_PACK_KDOS=1` | Also pack this root filesystem as a base pack named `kdos`, written to `build/kdos-base` |
| `KDOS_REPLAY=1` | A build step's "already done" guard stands down. Set for steps a build plan named explicitly |
| `KDOS_GIT_COMMIT`, `KDOS_GIT_DIRTY` | Recorded in each phase's snapshot manifest as `git_commit` and `git_dirty`, and shown by the snapshot picker, which marks a snapshot stale when either disagrees with the tree. Nothing on the image reads them; `/etc/os-release` carries a fixed version |
| `KDOS_SNAPSHOT_PATHS`, `KDOS_SNAPSHOT_EXCLUDE`, `KDOS_PHASE_TITLE`, `KDOS_PHASE_DESC` | A phase's metadata block in its `script/<phase>.env.sh` file, parsed by the orchestrator and never sourced |
| `KDOS_RES=WxH` | The virtual screen size for `make run` and its variants; default `1920x1080` |

The build runs its later steps inside a chroot entered with a cleared environment, so a variable a
chroot step reads must also be named in `script/chroot_exec.sh`. Exactly three are forwarded:
`KDOS_REPLAY`, `KDOS_ISO_SOURCES` and `KDOS_PACK_KDOS`. A new one added to the `Makefile` and not
there reaches every host step and no chroot step; see
[Entering the chroot](../05-developer/how-kdos-is-built.md#entering-the-chroot).

### Sources

Read by `ports/fetch` (`make fetch`), `ports/publish` and the pre-push hook, which fetch and archive
the upstream source tarballs. The shared defaults are set in `ports/srclib.sh`. How `make fetch`
resolves a source and when the pre-push hook refuses a push are in
[Writing ports](../05-developer/writing-ports.md) and
[Fetching](../05-developer/how-kdos-is-built.md#fetching-the-only-step-that-uses-the-network).

| Variable | Default | Effect |
|---|---|---|
| `KDOS_SOURCES_REPO` | `kunaldawn/kdos` | The GitHub repository whose releases hold the source archive |
| `KDOS_SOURCES_BASE` | `https://github.com/$KDOS_SOURCES_REPO/releases/download` | Where archived sources are downloaded from, as `$KDOS_SOURCES_BASE/sources-<NNN>/<sha256>`, with `NNN` from the index. Set it empty to fetch from upstream only |
| `KDOS_SRCCACHE` | `ports/.srccache` | The local source cache, one file per hash, stored as `sha256-<first two hex digits>/<hash>`. Point two checkouts at one cache to download each file once |
| `KDOS_SOURCES_INDEX` | `ports/sources.idx` | The index saying which archive release holds each hash |
| `KDOS_RELEASE_CAP` | `1000` | Files per archive release before `ports/publish` opens the next; GitHub's asset limit |
| `KDOS_FETCH_HOST=1` | unset | Run `ports/fetch` entirely on this machine, generating vendor bundles with its own toolchains, instead of handing missing bundles to the fetch container |
| `SOURCE_DATE_EPOCH` | `1735689600` | The timestamp written into generated vendor bundles, so they are reproducible |
| `KDOS_SOURCES_TOKEN` | read from `~/.config/kdos/sources-token` (mode 600) | The GitHub token `ports/publish` uploads with |
| `KDOS_REPO` | `kunaldawn/kdos` | The repository whose release `ports/publish --freeze <tag>` attaches `sources.sha256` to |
| `KDOS_PUBLISH_DELAY` | `8` | Seconds between uploads |
| `KDOS_LFS_STORE` | the clone's `lfs/objects` | Where `ports/publish --history` reads Git LFS (Large File Storage) objects that earlier commits' recipes name, to archive them under their hashes |
| `KDOS_GITHUB_API`, `KDOS_GITHUB_UPLOADS` | `https://api.github.com`, `https://uploads.github.com` | Replace the API and upload endpoints, for testing against a local stand-in |
| `KDOS_ALLOW_UNVERIFIED=1` | unset | Let `kpkg` build from a source file whose recipe has no `sha256`, and quiet `ports/fetch`'s warning about it. Without it such a file is refused. `ports/fetch`, and `kpkg install` or `kpkgbuild` run by hand, read it; a `make build` step in the chroot does not see it (see [Build](#build)) |
| `KDOS_SKIP_PUBLISH_CHECK=1` | unset | Skip the pre-push hook's check that every source a pushed recipe names is archived |

## See also

- [Configuration](configuration.md): every setting, with defaults
- [The daemons](../04-programs/daemons.md): what each socket's owner does
- [Architecture overview](../03-architecture/overview.md): where state lives, in summary
- [The security model](../03-architecture/security-model.md): why authorisation is the caller's
  credential
- [Packs and boxes](../03-architecture/packs-and-boxes.md): the pack store, composition and grafts
- [Repository layout](repository-layout.md): the source tree, rather than the installed system
- [How KDOS is built](../05-developer/how-kdos-is-built.md): the build that the [Build](#build) and
  [Sources](#sources) variables steer
- [The ports catalogue](ports-catalogue.md): every port the package database can hold

<!-- book-nav -->
---

*Part VI — Reference, chapter 41.* Previous: [40. Configuration](configuration.md) · [Contents](../README.md) · Next: [42. Repository layout](repository-layout.md)
