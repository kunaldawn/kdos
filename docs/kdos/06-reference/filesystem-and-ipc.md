# Filesystem and IPC

This page is the lookup table for a running KDOS system: every path KDOS owns, every socket its
programs talk over and the requests each one accepts, and the environment variables that change
behaviour. It is for administrators tracking down where something lives, for people scripting the
desktop, and for contributors who need to know which program owns which file.

*IPC* (inter-process communication) here means the Unix sockets and shared files one KDOS program
uses to tell another something.

**How to use this page.** Look a path up under [The target filesystem](#the-target-filesystem),
grouped by top-level directory. To talk to a daemon, read [Protocol conventions](#protocol-conventions)
and then the socket's own section under [Sockets](#sockets). What each *configuration* file's keys
mean is on [Configuration](configuration.md); this page says only where the file is. For the source
tree rather than the installed system, see [Repository layout](repository-layout.md).
Terms such as *surface*, *chrome*, *accent*, *box* and *pack* are defined in the
[Glossary](glossary.md).

## The target filesystem

### `/etc/kdos/` — machine configuration

| Path | Holds | Shipped |
|---|---|---|
| `login.conf` | The account tty1 logs in automatically | yes |
| `menu.conf` | The routes — a stable name a script can use for every place in the system | yes |
| `packd.conf` | How many superseded pack versions to keep | yes |
| `zram.conf` | Compressed-swap size and algorithm | yes |
| `timers.d/` | The system's periodic jobs, plus a `README` | yes: the update check and `fstrim`; `fwupd` adds its refresh |
| `keys/` | Trusted keys for host packages | yes, with no keys — only a `README` stating the policy |
| `keys/packs/` | Trusted keys for application packs | yes, one key |
| `mountd.conf` | Removable-media options | no — create it |
| `update.conf` | The binhost `kdos update` installs from | no — create it |
| `accent` | The accent name, so the boot splash can repaint in it before anyone logs in | no — written by `kdos-powerd` when `kdos theme` runs |

Every key is documented in [Configuration](configuration.md).

### `/var/lib/kdos/` — state written by the system

| Path | Holds | Written by |
|---|---|---|
| `packs/` | Installed application packs, `<id>.kpack` | The pack daemon, as root |
| `packs/mnt/` | Mount points for packs | The pack daemon |
| `packs/staging/` | The one place an unprivileged download may land, mode 01777 | Any user, and the application store |
| `pack-manifest` | Every graft made, so removing one is exact | The pack daemon |
| `fs-manifest` | Every path the build's static tree provided | The build |
| `march.ledger` | Measured per-machine optimisation results | `kdos march` |
| `apps-pending` | Applications chosen in the installer that could not be installed | `kinstall` |
| `update.json` | What `kdos update check --json` found; the panel's update badge reads it | The system update-check timer |

Packs on the boot medium are read from `/mnt/iso/packs`.

### `/usr/share/kdos/` — shipped data

| Path | Holds |
|---|---|
| `appstore/catalogue` | Every application the store can build, as a chain of apt packages |
| `alien-apps` | Application name → the command line that runs it in its box |
| `icons/atlas.kia` | The pre-rendered icon atlas |
| `icons/art/`, `icons/marks/` | The vendored icon source and the KDOS marks |
| `cursors/art/` | The vendored cursor source |
| `gtk-theme/theme/` | The vendored GTK stylesheet source |
| `charnames.idx` | The Unicode character-name index the character picker searches |
| `secdb.txt` | The vendored security-advisory database, read by `kdos cve` |
| `reasons/` | The explanations `kdos why` and `kdos explain` print |
| `doc/` | The help pages the desktop's surfaces open |
| `logo.txt` | The login banner's picture, generated from the mascot |
| `kdos.png` | The mascot artwork the marks and logo are cut from |
| `screensaver.txt` | The grid the screensaver's `art` and `bounce` effects move |
| `splash.psf` | The boot splash's font |
| `boot/` | Boot artwork |
| `memtest86plus/` | The memory tester carried on the boot medium |
| `syncthing-offline.xml` | The first-run configuration `kdos-syncthing` gives a user who has none: syncing between machines on one LAN without reaching outside it. From `syncthing` |

`screensaver.txt` can be replaced per user at `~/.config/kdos/screensaver.txt`. It is separate from
`logo.txt` so that changing the screensaver does not change the picture the machine boots with. The
other six screensaver effects draw without a file.

### `/var/lib/kpkg/` — the host package database

The database of `kpkg`, the host package manager.

| Path | Holds |
|---|---|
| `db/<name>` | One entry per installed package, with its file list |
| `db/.recipe/<name>` | The recipe hash a package was installed from |

### `/run/` — system runtime

Emptied at every boot.

| Path | Is |
|---|---|
| `kdos-powerd.sock`, `kdos-energyd.sock`, `kdos-oomd.sock`, `kdos-mountd.sock`, `kdos-packd.sock` | The root daemons' sockets; see [Sockets](#sockets) |
| `kdos-svc.<name>.log` | A supervised service's output, capped at 64 KiB with one `.old`. Every line also goes to syslog |
| `kdos-init.<name>.log` | What a service's start script printed at boot |
| `kdos-getty.<tty>.log` | `kdos-getty`'s messages for that terminal |
| `kdos-fsck.log` | What the boot-time filesystem check reported, written only when it said something |
| `kdos-medium` | Where the installer binds the boot medium while installing |

### Boot files

| Path | Is |
|---|---|
| `/boot/initramfs.cpio.gz` | The image's initramfs |
| `/boot/initramfs.modules` | The kernel modules the initramfs carries, one per line |
| `/boot/initramfs-kdos.cpio.gz` | The image's initramfs with a newer kernel's modules appended, written when the `linux` package is upgraded |
| `/boot/efi/EFI/kdos/<slot>/` | Each A/B root slot's `vmlinuz` and `initramfs.cpio.gz`, on the EFI system partition |
| `/boot/efi/EFI/kdos/bootstate` | Which slot is current and whether a trial boot is pending; `kdos-bootctl` owns it |
| `/boot/efi/EFI/kdos/trial/` | A trial boot's copy of the loader, present only while a trial is pending |

### Per user

| Path | Holds |
|---|---|
| `~/.config/kdos/` | Your desktop configuration |
| `~/.config/kdos-comp/` | The compositor's own configuration |
| `~/.config/kdos/boxes/<name>.conf` | A box's profile |
| `~/.config/kdos/sandbox/<profile>.conf` | A `kdos sandbox` profile |
| `~/.config/kdos/displays.conf` | The screen layout `kdos-display` keeps and replays at login |
| `~/.config/kdos/places` | Extra rows in the places column |
| `~/.local/share/kdos/alien-apps` | Your own launcher table; its entries win over the system one |
| `~/.local/share/kdos/boxes/<name>/` | A box's writable layer (`upper`, `work` and the merged root), and its home when the profile says `home = private` |
| `~/.local/share/kdos/packs/` | Where a data pack's contents are grafted for boxed applications |
| `~/.local/share/kdos/observed-app-ids` | Every application ID the compositor has seen, read by `kdos appid` |
| `~/.local/share/kdos/run-history` | Commands typed into the Run dialog, one per line, newest last, at most 50. A command run again moves to the end |
| `~/.local/share/kdos/scratch.txt` | The scratch note `kdos-note` keeps |
| `~/.local/share/applications/` | Launchers for applications you installed |
| `~/.local/bin/` | Shims for applications you installed |
| `~/.local/state/kdos/appusage` | Launch counts, which order the Start menu's frequent column |
| `~/.local/state/kdos/startcat` | The Start menu category last shown |
| `~/.local/state/kdos/toggles/` | One empty file per switch that is on: `stay-awake`, `night-light`, `dnd` |
| `~/.local/state/kdos/session` | What was running when the session ended: `app <name>` per boxed application, `native <app_id>` per host program |
| `~/.local/state/kdos/winpos` | Where the compositor last saw each application's window |
| `~/.local/state/kdos/diskwarn` | `<step> <mountpoint>` per line: which disk-full warnings the panel has already shown |
| `~/.local/state/kdos/update.json` | What `kdos update check --json` found, when you ran it yourself |
| `~/.cache/kdos/theme` | One word: the accent name. The whole theme state the desktop reads |
| `~/.cache/kdos/wallpaper.png` | The accent-coloured wallpaper, which the compositor draws in preference to the configured one |
| `~/.cache/kdos/plocate.db` | This account's file index, rebuilt nightly from `$HOME` by `kdos-updatedb` |
| `~/.local/share/Trash/` | The freedesktop trash |
| `~/Mail/` | The account's Maildir |
| `~/Recordings/` | What `kdos-rec` recorded, as `YYYY-MM-DD-HHMMSS.wav`, created on first use |
| `~/.local/share/whisper.cpp/models/` | Speech models, `ggml-*.bin`. `kdos speech get` writes here; the desktop only searches it |

Each `winpos` line is `app_id x y w h workspace shaded`, in pixels, most recent first, at most 200
lines. It is read when a window opens and written when it closes, only while `comp.conf`'s
`window_memory` is on.

`~/Mail` is named by `~/.mbsyncrc` and by notmuch's `mail_root`, and is created for the default
account when the image is built. Both programs report an error on a missing directory rather than
creating one, so a new account needs `mkdir ~/Mail`.

`~/Recordings` is not an XDG user directory: the freedesktop set has no such entry, and
`user-dirs.dirs` does not invent one.

### `$XDG_RUNTIME_DIR` — per session

Normally `/run/user/<uid>`. Boxes share this directory with the session.

| Path | Is |
|---|---|
| `bus` | The session message bus (D-Bus), at a fixed path because boxes share this directory |
| `.kdos-bus.lock` | Serialises starting that bus |
| `wayland-*` | The compositor's Wayland socket |
| `kdos-cmd.sock` | The compositor's command socket |
| `kdos-frames.sock` | Late-frame reports from the compositor |
| `kdos-notify.sock` | The notification daemon |
| `kdos-clip.sock` | The clipboard history |
| `ssh-agent.socket` | The account's `ssh-agent`, started by the first login shell that finds nothing answering there; `SSH_AUTH_SOCK` names it |
| `podman/podman.sock` | The rootless Podman API, while `podman system service` runs |
| `kdos-panel.overflow` | What the panel has placed behind its chevron |
| `kdos-comp.log` | The compositor's output |
| `kdos-thumb.ppm` | The last window picture the compositor's `thumb` request wrote |
| `kdos-appbox.trace` | Stage timings for the most recent boxed launches |
| `kdos-appbox.warmup.lock`, `kdos-appbox.create.lock` | Serialise the login warm-up and box creation, so two launches do not build one box twice |
| `kdos-sfx.route` | Which player the sound effects use, cached |
| `kdos/timers.pid` | The process IDs of this session's timer loops, one per line |
| `kdos/nowplaying` | One line naming what is playing, written by `kdos-mpctl watch` |
| `kdos/screencast.pid` | The process ID of the running `kdos-record`, while it runs |

## Protocol conventions

The five root daemons share one shape:

- **One socket in `/run`, mode 0666.** Anyone can connect; the mode is not the gate.
- **Authorisation is the caller's real user ID, read from the kernel** (`SO_PEERCRED`) — never
  anything in the message. The allowed users are root plus members of a group; each socket's section
  says which.
- **One request per connection:** a short line in, a reply out, then the socket closes. There is no
  session state. The exceptions are `kdos-mountd`'s `subscribe`, which keeps the connection open,
  and the three mountd verbs that carry a secret as a second frame after the request line.
- **Replies** are `ok`, optionally followed by data, or `err <reason>`. A caller who is not allowed
  gets `err not permitted`.
- **Each verb takes a fixed number of words** (except `compose` on `kdos-packd`, which takes one or
  more pack IDs), and a longer line is refused rather than truncated: `mount 0 rm -rf /` is an
  unknown command, not `mount 0`.
- **No verb takes a path.** Where a request names a thing, it names it by an identifier the daemon
  published in a list. The single exception is the pack daemon's `install`, which names a file in
  its own staging directory.
- **Test modes.** `kdos-energyd`, `kdos-oomd`, `kdos-mountd` and `kdos-packd` accept `--fixture`,
  which reads recorded state instead of the machine's and prints what it would do instead of doing
  it. `kdos-powerd --explain USER` prints which verbs that user may use.

You can try any of these by hand, for example:

```sh
printf 'ping\n' | socat - UNIX-CONNECT:/run/kdos-oomd.sock
```

The session sockets under `$XDG_RUNTIME_DIR` belong to your own session and use their own shapes,
described with each one.

## Sockets

### `/run/kdos-powerd.sock`

Power and machine settings, served by `kdos-powerd`; the client is `kdos-power`. Two tiers of
caller:

- root, and members of `seat` or `wheel`: `suspend`, `poweroff`, `reboot`, `ping`
- root and members of `wheel` only: `firewall`, `autologin`, `accent`, `timezone`

`seat` is the group that owns the display, so the person at the machine can always suspend and shut
down; changing configuration needs an administrator.

| Verb | Argument | Does |
|---|---|---|
| `suspend` | — | Suspends |
| `poweroff` | — | Signals process 1, then powers off |
| `reboot` | — | Signals process 1, then reboots |
| `ping` | — | Liveness |
| `firewall` | `list`, or `<service> on` / `<service> off` | Lists the firewall's services, one row each, or opens or closes one, rewriting `/etc/nftables.d/50-kdos-services.nft` |
| `autologin` | An account name, or `off` | Rewrites `/etc/kdos/login.conf` |
| `accent` | An accent name | Writes the root-owned copies of the accent: `/etc/kdos/accent`, `/etc/vtrgb` and the boot menu's theme. It does not retint the desktop; `kdos theme` does that |
| `timezone` | `Area/City` | Writes `/etc/localtime`, `/etc/profile.d/20-timezone.sh`, and the Wi-Fi country in `/etc/modprobe.d/kdos-regdom.conf` |

### `/run/kdos-energyd.sock`

Per-application energy use, from `kdos-energyd`; the client is `kdos-energy`. Root and members of
`wheel`.

| Verb | Argument | Answers |
|---|---|---|
| `report` | — | Each application's share of attributable energy, the idle floor, and the sample count |
| `report-json` | — | The same, as JSON |
| `ping` | — | Liveness |

The raw energy counter and the sampling interval are never published, and the interval is fixed by
the daemon, so this socket cannot be turned into a fine-grained power measurement instrument (a
known side channel).

### `/run/kdos-oomd.sock`

The out-of-memory killer, `kdos-oomd`. Root, and members of `seat` or `wheel`.

| Verb | Argument | Answers |
|---|---|---|
| `status` | — | What it is watching, and what it has killed |
| `ping` | — | Liveness |

Nothing in this protocol names a process, so it cannot be used to aim a kill. Killing is the
daemon's own decision or it does not happen.

### `/run/kdos-mountd.sock`

Removable media, encrypted volumes, network shares and drive health, from `kdos-mountd`; the client
is `kdos-mount`. Root, and members of `seat` or `wheel`. Fifteen verbs.

Three verbs carry a secret as a *second frame*: the request line ends with a byte count, and exactly
that many bytes follow the newline (see below the table).

| Verb | Argument | Answers |
|---|---|---|
| `list` | — | The devices it will act on, with an index each |
| `mount` | An index | The mountpoint |
| `unmount` | An index | |
| `eject` | An index | |
| `close` | An index | Closes an encrypted volume that `unlock` opened |
| `smart` | An index | The drive's own health summary |
| `unlock` | An index and a byte count | The name of the unlocked device; the passphrase follows as a second frame |
| `format` | An index, a filesystem and a byte count | The device's kernel name typed back follows as a second frame |
| `cifs` | A server, a share, a username, a domain and a byte count | The mountpoint; the password follows as a second frame |
| `krb5` | A server, a share, a username or `-`, and a domain or `-` | The mountpoint. Uses your Kerberos ticket; no second frame |
| `shares` | — | The mounted network shares, with an index each |
| `browse` | — | The file servers that answered an mDNS and a NetBIOS broadcast, as `name<TAB>address` |
| `disconnect` | A share index | Unmounts one share |
| `subscribe` | — | Keeps the connection open and writes a line for each block-device event |
| `ping` | — | Liveness |

**Indexes.** The client asks for an index from a list the daemon published, and the daemon decides
the device, the mountpoint and the options. The device list is rescanned on every request. A share
index belongs to a different list, `shares`, built from `/proc/mounts` on every request; a
`disconnect` index is checked against that list only. An index is meaningful only for the list it
came from.

**Secrets travel as a second frame, never as a word.** `unlock`, `format` and `cifs` declare a byte
count on the request line and send exactly that many bytes after the newline, because the request
line is split on whitespace and a passphrase may contain some. The daemon holds the secret in one
buffer and wipes it on every way out of the request. The byte count may not be zero.

**Share names are checked, not quoted.** Each of the four names `cifs` takes must pass a character
allowlist; anything else is refused. `mount.cifs` builds its option string by concatenation and
escapes only the password, so a comma in a server, share, username or domain would become a new
mount option, and a `/` or `\` in a server name would redirect the mount.

**The password reaches `mount.cifs` on a file descriptor** (`PASSWD_FD=0`, with the bytes on the
helper's standard input) — not on its command line, where any process could read it, not in its
environment, and not in a file someone must delete.

**`krb5`** sends no secret: the Kerberos ticket is in the caller's credential cache. The daemon adds
`sec=krb5` and `cruid=<the caller>` to the options, so the kernel's `cifs.upcall` helper reads the
caller's tickets rather than root's. Without `cruid` the mount fails with `Required key not
available`. The helper and its `request-key` rule are checked first, so a machine that cannot do
Kerberos mounts says which file is missing. No password descriptor is passed, so `mount.cifs` does
not wait for one.

**Name resolution.** When the C library cannot resolve a server name, the daemon tries once more:
a `.local` name through `avahi-resolve-host-name`, a bare NetBIOS name through `nmblookup`. The
mount is given the address as `ip=` beside the name you typed, so the mountpoint keeps the name.

**`browse` rows are not indexes.** `cifs` and `krb5` take a server name, so a row carries the name
with its address beside it, which also tells apart two machines with the same NetBIOS name on
different subnets.

### `/run/kdos-packd.sock`

The pack daemon, `kdos-packd`, which installs, mounts and composes application packs. Root and
members of `wheel`. Thirteen verbs.

| Verb | Argument | Answers |
|---|---|---|
| `list` | — | Every pack the machine can see, with its state |
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
| `status` | — | How packs are mounted, the staging directory and the retention count |
| `ping` | — | Liveness |

`status` publishes the staging directory and the retention count, so a client writing a download
does not have to work either out.

Every ID comes from `list`. The single exception is `install`, which names a file in the daemon's
own staging directory, `/var/lib/kdos/packs/staging` — the one place an unprivileged user may
write. Relative traversal and absolute paths are both refused. Signatures are checked at install and
at mount against `/etc/kdos/keys/packs/`; see
[Packs and boxes](../03-architecture/packs-and-boxes.md).

### `$XDG_RUNTIME_DIR/kdos-cmd.sock` — the compositor

The compositor's command socket. One JSON object per line in, one JSON object per line out, then
the connection closes. The caller's user ID is checked, because this socket performs actions; it
cannot tell a boxed application from a host one, since both run as you. `kdos hey` is the
command-line front end.

| Request | Does |
|---|---|
| `{"cmd":"list"}` | Every window: ID, title, geometry, state, workspace, box, instance |
| `{"cmd":"outputs"}` | The outputs and their scales |
| `{"cmd":"boxes"}` | The distinct boxes with a window on screen |
| `{"cmd":"run","action":"Close","id":7}` | Runs a compositor action on a window |
| `{"cmd":"peek","on":true}` | Fades every window to reveal the desktop; `false` (or no `on`) restores them |
| `{"cmd":"thumb","app_id":"foot","w":64,"h":36}` | Writes a picture of that application's front window to `$XDG_RUNTIME_DIR/kdos-thumb.ppm` and answers with the path |

Replies carry `"ok":true` or `"ok":false` with a reason; an unknown or incomplete action is always
answered, never silently ignored. A window ID is the compositor's creation number, unique for the
session. The `thumb` path is chosen by the compositor, never by the request, and a window whose
pixels live on the GPU has no picture (`ok:false`).

The socket never blocks the compositor: requests are small and bounded, an oversized request is
refused, and a client that stalls is dropped on a timer. It keeps no history and offers no
subscription; for a stream of events use `kdos-frames.sock`.

### `$XDG_RUNTIME_DIR/kdos-frames.sock` — late frames

Written by the compositor, read by `kdos stutter` and the panel's stutter indicator: one JSON object
per late frame, carrying the output, how late it was, the compositor's own render cost, and a
source field saying whether the gap was measured at presentation (what you saw) or at the frame
clock (what the compositor was given).

Both ends are non-blocking. A reader that cannot keep up loses lines, and there is no history: a
reader that connects late has missed what happened. Keeping the compositor's frame loop fast comes
first.

### `$XDG_RUNTIME_DIR/kdos-notify.sock` — notifications

The notification daemon, `kdos-notifyd`. One text request per connection.

| Verb | Does |
|---|---|
| `count` | How many notifications, and how many unseen |
| `list` | The history |
| `seen` | Clears the unseen count |
| `open <id>` | Activates an entry |
| `forget <id>` | Removes one entry |
| `clear` | Empties the history |
| `dnd` | Toggles Do Not Disturb; `dnd on` and `dnd off` set it |
| `dismiss` | Puts the newest notification on screen away |
| `dismiss all` | The same for every notification on screen |
| `raise` | Brings the last dismissed notification back on screen |

A dismissal reports reason 2, "dismissed by the user", to the program that sent it. `raise` takes
the entry out of the history, so a notification is on screen or in the centre, never both, and it
comes back without its buttons: the original notification is closed, and its actions belonged to
the program that sent it.

### `$XDG_RUNTIME_DIR/kdos-clip.sock` — the clipboard

The clipboard history daemon, `kdos-clip`. The daemon owns the history and the front end draws it.
One text request per connection.

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
| `~/.cache/kdos/theme` | `kdos theme` | The compositor, the panel, the desktop, notifications — one word |
| `~/.cache/kdos/wallpaper.png` | `kdos theme` | The compositor, in preference to the configured wallpaper |
| `$XDG_RUNTIME_DIR/kdos-panel.overflow` | The panel | The overflow popup |
| `/usr/share/kdos/alien-apps` | The build | Every shim, and the launcher dispatcher |
| `~/.local/share/kdos/alien-apps` | `kdos-appbox genlaunchers --user` | The same, winning over the system table |
| `~/.local/share/kdos/observed-app-ids` | The compositor, once per window | `kdos appid` |
| `/var/lib/kdos/pack-manifest` | The pack daemon | Itself, so ungrafting removes exactly what was added |
| `/var/lib/kdos/update.json` | The update-check timer | The panel's update badge |
| `/etc/kdos/accent` | `kdos-powerd`'s `accent` verb | The boot scripts, which repaint the splash in it |
| `/boot/initramfs.modules` | The initramfs build step, one module per line | The `linux` package's post-install, which carries a new kernel's copies of that set into `/boot/initramfs-kdos.cpio.gz` |
| `/boot/initramfs-kdos.cpio.gz` | The `linux` post-install: the image's initramfs with the new kernel's modules appended | `kdos-bootctl deploy` and `kinstall`, which use it in place of `/boot/initramfs.cpio.gz` |
| `EFI/kdos/<slot>/vmlinuz`, `EFI/kdos/<slot>/initramfs.cpio.gz` on the EFI system partition | `kinstall` for slot `a`, `kdos-bootctl deploy` for either | The boot loader (Limine), through the `/KDOS` menu entries `kdos-bootctl` regenerates on every change of boot state |
| `EFI/kdos/trial/` on the EFI system partition: a copy of the loader and a `limine.conf` defaulting to the candidate slot | `kdos-bootctl try` on UEFI; removed by every state that is not a pending trial | The firmware, for the one boot `BootNext` asks for |
| `Boot####` "KDOS update trial" and `BootNext` in `efivarfs` | `kdos-bootctl try` on UEFI; removed by `kdos-bootctl mark-good` | The firmware, once |
| `/run/kdos-svc.<name>.log` | `ksvc supervise`, capped at 64 KiB with one `.old` | You, when syslog is not running; every line also goes to syslog |
| `$XDG_RUNTIME_DIR/kdos-appbox.trace` | The launcher | You |
| `$XDG_RUNTIME_DIR/kdos/screencast.pid` | `kdos-record`, while its recording runs | The next `kdos-record`, which stops that one, and the panel, which shows a recording indicator |

The overflow file is published rather than recomputed so the popup shows exactly what the panel
decided. Both readers of `screencast.pid` check that the process is still alive, because a file
left by a crash is not a recording.

## Environment variables

KDOS programs read well over a hundred `KDOS_*` variables. The ones below are those a person may
want to set; header guards, internal constants and variables set only between KDOS's own programs
are left out.

### Debugging

| Variable | Effect |
|---|---|
| `KDOS_COMP_DEBUG=1` | Raise the compositor's log level; every dropped frame is logged |
| `KDOS_PANEL_DEBUG=1` | The panel explains its layout decisions |
| `KDOS_BB_DEBUG=1` | The `kdos-bb` demo reports how its audio mixer is fed |
| `KDOS_WHEEL_DEBUG=1` | Trace every scroll-wheel event, tick and dropped duplicate |
| `KDOS_CRT_DUMP=<prefix>` | Write the phosphor effect's input and output images once |
| `KDOS_CRT_DUMP_FRAME=<n>` | Wait until frame *n* before that dump |
| `KDOS_PACKD_VERBOSE=1` | The pack daemon explains itself |

### Behaviour

| Variable | Effect |
|---|---|
| `KDOS_A11Y=1` | Enable the accessibility stack for one boxed launch |
| `KDOS_BOX` | Set inside every box to its name. It is what identifies which box an X11 window came from |
| `KDOS_REQUIRE_SIG=1` | `kdos-packd` and `kdos-pack` refuse any pack with no signature |
| `KDOS_PACK_KEY=<file>` | The private key the application store signs exported pack indexes with; without it the index is unsigned and says so |
| `KDOS_BINHOST=<dir>` | The binhost `kdos update` installs from, overriding `/etc/kdos/update.conf` |
| `KDOS_WHISPER_MODEL=<file>` | One speech model, named exactly. When set, no directory is searched after it |
| `KDOS_NO_ANIM=1` | Skip the login banner's animation |
| `KDOS_NO_SFX=1` | Silence sound effects |
| `KDOS_NO_LOCK_ON_SUSPEND=1` | Do not lock the session when suspending |
| `KDOS_ASCII=1` | Force the plainest, ASCII-only glyph set |
| `KDOS_MARCH_RUNS=<n>` | How many samples `kdos march` takes per measurement |
| `KDOS_WHEEL_MIN_MS=<ms>` | The window within which a duplicate scroll-wheel event is dropped; `0` disables the filter |

### Testing seams

These point a program at recorded state instead of the live machine. They are for tests and are
honoured only where noted.

| Variable | Points at |
|---|---|
| `KDOS_DUMP_SIZE=WxH` | The size an offscreen render (`--dump`) draws at |
| `KDOS_GOLDEN_UPDATE=1` | Regenerate reference frames instead of comparing against them |
| `KDOS_RES_FIXTURE` | A recorded system state for the resource monitor |
| `KDOS_PRIVACY_PROC` | A recorded process tree for the panel's privacy indicator |
| `KDOS_ENERGY_PROC`, `KDOS_ENERGY_POWERCAP` | Recorded trees for the energy daemon |
| `KDOS_MOUNTD_SYS`, `KDOS_MOUNTD_DEV`, `KDOS_MOUNTD_FSTAB`, `KDOS_MOUNTD_MOUNTS` | Recorded state for the media daemon |
| `KDOS_MOUNTD_CONF`, `KDOS_MOUNTD_MEDIA`, `KDOS_MOUNTD_UEVENT` | Its configuration file, its mount root, and a FIFO standing in for the kernel's device-event socket |
| `KDOS_PACK_STORE`, `KDOS_PACK_MEDIUM`, `KDOS_PACK_MANIFEST` | Alternative pack locations |
| `KDOS_INITRD` | An alternative boot image, for the microcode check |
| `KDOS_BOOTSTATE` | An alternative boot-state file. With it, no firmware variable is written unless `KDOS_EFIVARS` is also set, and no kernel command line is read unless `KDOS_CMDLINE` is |
| `KDOS_CMDLINE` | A file standing in for `/proc/cmdline`, whose `kdos_slot=` tells `kdos-bootctl mark-good` which slot's kernel is running |
| `KDOS_EFIVARS`, `KDOS_ESP_DISK` | A directory standing in for `efivarfs`, and `<disk image>:<partition number>` standing in for the EFI system partition's disk, for testing the `BootNext` trial |
| `KDOS_POWERD_SOCKET`, `KDOS_ENERGYD_SOCKET`, `KDOS_OOMD_SOCKET`, `KDOS_MOUNTD_SOCKET`, `KDOS_PACKD_SOCKET` | Move a daemon's socket. This grants nothing, since authorisation never depends on the path |

The media daemon's seams are read only under `--fixture`, so a variable inherited from the boot
environment has no effect on a running system.

### Build

Read by `make build` and the build scripts on the build machine; see
[The build system](../05-developer/build-system.md).

| Variable | Effect |
|---|---|
| `KDOS_ISO_SOURCES=1` | Put the source tarballs on the boot medium |
| `KDOS_PACK_KDOS=1` | Also pack this root filesystem as a base pack named `kdos`, written to `build/kdos-base` |
| `KDOS_REPLAY=1` | A build step's "already done" guard stands down. Set for steps a build plan named explicitly |
| `KDOS_GIT_COMMIT`, `KDOS_GIT_DIRTY` | Recorded in each phase's snapshot manifest as `git_commit` and `git_dirty`, and shown by the snapshot picker, which marks a snapshot stale when either disagrees with the tree. Nothing on the image reads them; `/etc/os-release` carries a fixed version |
| `KDOS_SNAPSHOT_PATHS`, `KDOS_SNAPSHOT_EXCLUDE`, `KDOS_PHASE_TITLE`, `KDOS_PHASE_DESC` | A phase's metadata block in its `*.env.sh` file — parsed, never sourced |
| `KDOS_RES=WxH` | The virtual screen size for `make run` and its variants; default `1920x1080` |

The build runs its later steps inside a chroot entered with a cleared environment, so a variable a
chroot step reads must also be named in `script/chroot_exec.sh`. Exactly three are forwarded:
`KDOS_REPLAY`, `KDOS_ISO_SOURCES` and `KDOS_PACK_KDOS`. A new one added to the `Makefile` and not
there reaches every host step and no chroot step.

### Sources

Read by `ports/fetch` (`make fetch`), `ports/publish` and the pre-push hook, which fetch and archive
the upstream source tarballs; see [Writing ports](../05-developer/writing-ports.md).

`make fetch` resolves every recipe's source from the local cache, then the archive, then upstream,
and is the only build step that uses the network; `make fetch-check` reports what is missing without
downloading. The pre-push hook runs only after `git config core.hooksPath script/hooks`, and then
refuses a push whose recipes name a source not yet in the archive.

| Variable | Default | Effect |
|---|---|---|
| `KDOS_SOURCES_REPO` | `kunaldawn/kdos` | The GitHub repository whose releases hold the source archive |
| `KDOS_SOURCES_BASE` | `https://github.com/$KDOS_SOURCES_REPO/releases/download` | Where archived sources are downloaded from, as `$KDOS_SOURCES_BASE/sources-<NNN>/<sha256>`, with `NNN` from the index. Set it empty to fetch from upstream only |
| `KDOS_SRCCACHE` | `ports/.srccache` | The local source cache, one file per hash. Point two checkouts at one cache to download each file once |
| `KDOS_SOURCES_INDEX` | `ports/sources.idx` | The index saying which archive release holds each hash |
| `KDOS_RELEASE_CAP` | `1000` | Files per archive release before `ports/publish` opens the next |
| `KDOS_FETCH_HOST=1` | unset | Run `ports/fetch` entirely on this machine, generating vendor bundles with its own toolchains, instead of handing missing bundles to the fetch container |
| `SOURCE_DATE_EPOCH` | `1735689600` | The timestamp written into generated vendor bundles, so they are reproducible |
| `KDOS_SOURCES_TOKEN` | read from `~/.config/kdos/sources-token` (mode 600) | The GitHub token `ports/publish` uploads with |
| `KDOS_REPO` | `kunaldawn/kdos` | The repository whose release `ports/publish --freeze <tag>` attaches `sources.sha256` to |
| `KDOS_PUBLISH_DELAY` | `8` | Seconds between uploads |
| `KDOS_LFS_STORE` | the clone's `lfs/objects` | Where `ports/publish --history` looks for old LFS objects |
| `KDOS_GITHUB_API`, `KDOS_GITHUB_UPLOADS` | GitHub's endpoints | Replace the API and upload endpoints, for testing against a local stand-in |
| `KDOS_ALLOW_UNVERIFIED=1` | unset | Let `kpkg` build from a source file whose recipe has no `sha256`, and quiet `ports/fetch`'s warning about it. Without it such a file is refused. `ports/fetch` and a `kpkg build` run by hand read it; a `make build` step in the chroot does not see it, because the chroot forwards only `KDOS_REPLAY`, `KDOS_ISO_SOURCES` and `KDOS_PACK_KDOS` |
| `KDOS_SKIP_PUBLISH_CHECK=1` | unset | Skip the pre-push hook's check that every source a pushed recipe names is archived |

## See also

- [Configuration](configuration.md) — every setting, with defaults
- [The daemons](../04-programs/daemons.md) — what each socket's owner does
- [Architecture overview](../03-architecture/overview.md) — where state lives, in summary
- [The security model](../03-architecture/security-model.md) — why authorisation is the caller's credential
- [Repository layout](repository-layout.md) — the source tree, rather than the installed system
