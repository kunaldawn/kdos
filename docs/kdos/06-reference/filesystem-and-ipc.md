# Filesystem and IPC

Every path KDOS owns on a running system, every socket and its verbs, and the environment variables
that change behaviour. This is the lookup page: what lives where, and how the parts talk to each
other.

For the source tree rather than the target, see [Repository layout](repository-layout.md).

## The target filesystem

### `/etc/kdos/` — machine configuration

| Path | Holds | Shipped |
|---|---|---|
| `login.conf` | The account tty1 logs in | yes |
| `menu.conf` | The routes — a name a script can hold for every place in the system | yes |
| `packd.conf` | The pack daemon's retention | yes |
| `zram.conf` | Compressed-swap size and algorithm | yes |
| `timers.d/` | The system's periodic jobs | yes |
| `keys/` | Trusted keys for host packages | yes, no keys — a `README` stating the policy |
| `keys/packs/` | Trusted keys for application packs | yes, one key |
| `mountd.conf` | Removable-media options | no — create it |

Every key is documented in [Configuration](configuration.md).

### `/var/lib/kdos/` — state written by the system

| Path | Holds | Written by |
|---|---|---|
| `packs/` | Installed application packs | The pack daemon, as root |
| `packs/mnt/` | Mount points for packs | The pack daemon |
| `packs/staging/` | The one place an unprivileged download may land, mode 01777 | You |
| `pack-manifest` | Every graft made, so removal is exact | The pack daemon |
| `fs-manifest` | Every path the build's static tree provided | The build |
| `march.ledger` | Measured per-machine optimisation results | `kdos march` |
| `apps-pending` | What the installer chose and could not install | `kinstall` |
| `update.json` | What `kdos update check --json` found | The system update timer |

### `/usr/share/kdos/` — generated and shipped data

| Path | Holds |
|---|---|
| `appstore/catalogue` | Every application, as a chain of apt packages |
| `alien-apps` | Application name → the command line that runs it in its box |
| `icons/atlas.kia` | The rasterised icon atlas |
| `icons/art/`, `icons/marks/` | The vendored icon source and the KDOS marks |
| `cursors/art/` | The vendored cursor source |
| `gtk-theme/theme/` | The vendored stylesheet source |
| `secdb.txt` | The vendored security database, read by `kdos cve` |
| `reasons/` | The explanations `kdos why` and `kdos explain` print |
| `doc/` | Shipped documentation |
| `logo.txt` | The banner logo, generated from the mascot |
| `screensaver.txt` | The grid the screensaver's `art` and `bounce` effects move |
| `splash.psf` | The splash font |
| `boot/` | Boot artwork |
| `memtest86plus/` | The memory tester on the medium |

`screensaver.txt` is yours to replace at `~/.config/kdos/screensaver.txt`. It is separate from
`logo.txt` so that changing the screensaver does not change the picture the machine boots with. The
other six screensaver effects draw without a file.

### `/var/lib/kpkg/` — the host package database

| Path | Holds |
|---|---|
| `db/<name>` | One entry per installed package, with its manifest |
| `db/.recipe/<name>` | The recipe hash a package was installed from |

### Per user

| Path | Holds |
|---|---|
| `~/.config/kdos/` | Your desktop configuration |
| `~/.config/kdos-comp/` | The compositor's own configuration |
| `~/.local/share/kdos/alien-apps` | Your own launcher table — user entries win over the system one |
| `~/.local/share/kdos/boxes/<name>/` | A box's writable layer: `upper`, `work` and the merged `root` |
| `~/.local/share/kdos/packs/` | Where a data pack's contents are grafted for boxed applications |
| `~/.local/share/applications/` | Launchers for applications you installed |
| `~/.local/bin/` | Shims for applications you installed |
| `~/.local/state/kdos/appusage` | Launch counts, which order the Start menu's frequent column |
| `~/.local/state/kdos/toggles/` | One empty file per switch that is on: `stay-awake`, `night-light`, `dnd` |
| `~/.local/state/kdos/session` | What was running when the session ended: `app <name>` per boxed application, `native <app_id>` per host toplevel |
| `~/.local/state/kdos/winpos` | Where the compositor last saw each application |
| `~/.local/state/kdos/diskwarn` | `<step> <mountpoint>` per line: which disk-full step the panel has already warned about |
| `~/.local/state/kdos/update.json` | What `kdos update check --json` found, for the panel's badge |
| `~/.cache/kdos/theme` | One word: the accent name. The entire theme state the desktop reads |
| `~/.cache/kdos/wallpaper.png` | The retinted wallpaper the compositor prefers |
| `~/.cache/kdos/plocate.db` | This account's file index, rebuilt nightly from `$HOME` by `kdos-updatedb` |
| `~/.local/share/Trash/` | The freedesktop trash |
| `~/Mail/` | The machine's Maildir |
| `~/Recordings/` | What `kdos-rec` wrote, `YYYY-MM-DD-HHMMSS.wav`, made on demand |
| `~/.local/share/whisper.cpp/models/` | Speech models, `ggml-*.bin`. Searched, never written by the desktop |

`winpos` holds `app_id x y w h workspace shaded` in pixels, most recent first, capped at 200 lines.
It is read at map and written at unmap, gated by `comp.conf`'s `window_memory`.

`~/Mail` is named by `~/.mbsyncrc` and by notmuch's `mail_root`, and is created by
`script/06_packaging/00_user.sh`. Both programs report an error on a directory that is not there
rather than making one.

`~/Recordings` is not an XDG user directory. `RECORDINGS` is not in the freedesktop set, and
`user-dirs.dirs` must not grow an invented key.

### `$XDG_RUNTIME_DIR` — per session

| Path | Is |
|---|---|
| `bus` | The session message bus, at a fixed path because a box shares this directory |
| `.kdos-bus.lock` | Serialises starting that bus |
| `wayland-*` | The compositor's socket |
| `kdos-cmd.sock` | The compositor's query socket |
| `kdos-frames.sock` | Late-frame reports |
| `kdos-notify.sock` | The notification daemon |
| `kdos-clip.sock` | The clipboard history |
| `ssh-agent.socket` | The account's `ssh-agent`, started by the first login shell that finds nothing answering there, and what `SSH_AUTH_SOCK` names |
| `kdos-panel.overflow` | What the panel has hidden behind the chevron |
| `kdos-comp.log` | The compositor's output |
| `kdos-appbox.trace` | Stage timings for the last launches |
| `kdos/timers.pid` | The pids of the session's timer loops, one per line, cleared when the login ends |
| `kdos/nowplaying` | One line naming what is playing, written by `kdos-mpctl watch` |
| `kdos/screencast.pid` | The pid of the running `kdos-record`, while one is running |

## Sockets

Every root daemon takes the same shape: one socket in `/run`, mode 0666, authorised by the peer's
credentials rather than by the socket's mode, one line per connection, and no verb that takes a
path. Replies are `ok` with optional data, or `err <reason>`. An unauthorised caller gets
`err not permitted`.

### `/run/kdos-powerd.sock`

Root, `seat` and `wheel` for the four verbs below. `timezone`, `autologin`, `firewall` and `accent`
answer root and `wheel` only.

| Verb | Argument | Answers |
|---|---|---|
| `suspend` | — | Suspends |
| `poweroff` | — | Signals process 1, then powers off |
| `reboot` | — | Signals process 1, then reboots |
| `ping` | — | Liveness |

### `/run/kdos-energyd.sock`

Root and `wheel`.

| Verb | Argument | Answers |
|---|---|---|
| `report` | — | Per-application shares of attributable energy, the idle floor, the sample count |
| `ping` | — | Liveness |

The raw counter and the sampling interval are never published, and the interval is fixed by the
daemon rather than requested, so this socket cannot be driven toward being a measurement
instrument.

### `/run/kdos-oomd.sock`

Root, `seat` and `wheel`.

| Verb | Argument | Answers |
|---|---|---|
| `status` | — | What it is watching, and what it has killed |
| `ping` | — | Liveness |

Nothing in this protocol names a process, so there is nothing to aim. Killing is the daemon's own
decision or it does not happen.

### `/run/kdos-mountd.sock`

Root, `seat` and `wheel`. Fifteen verbs.

| Verb | Argument | Answers |
|---|---|---|
| `list` | — | The eligible devices, with an index each |
| `mount` | An index | The mountpoint |
| `unmount` | An index | |
| `eject` | An index | |
| `close` | An index | Closes a mapper an `unlock` opened |
| `smart` | An index | The drive's own health line |
| `unlock` | An index and a byte count | The mapper's name; the passphrase is a second frame |
| `format` | An index, a filesystem and a byte count | The device's own name typed back, as a second frame |
| `cifs` | A server, a share, a username, a domain and a byte count | The mountpoint; the password is a second frame |
| `krb5` | A server, a share, a username or `-`, and a domain or `-` | The mountpoint; no second frame at all |
| `shares` | — | The network shares that are mounted, with an index each |
| `browse` | — | The servers that answered an mDNS and a NetBIOS broadcast, as `name<TAB>address` |
| `disconnect` | A share index | Unmounts one |
| `subscribe` | — | Keeps the socket and writes a line per block uevent |
| `ping` | — | Liveness |

The client asks for an index out of a list the daemon published, and the daemon decides the device,
the mountpoint and the options. The list is rescanned on every request.

A share's index counts a different list. `shares` is built from `/proc/mounts` on every request, and
a `disconnect` index is checked against that list and never against the device list. An index is
true only of the list it came with, and the two lists have nothing to do with each other.

A secret is a frame and never a token. `unlock`, `format` and `cifs` declare a byte count on the
request line and send exactly that many bytes after the newline, because the request line is
tokenised on whitespace and a passphrase may contain some. The daemon holds one buffer for it and
wipes it on every path out of the request.

`cifs`'s four names are checked against a character allowlist each, and what is not on the list is
refused rather than quoted. `mount.cifs` assembles its option string by concatenation and escapes
nothing but the password, so a comma in the server, the share, the username or the domain is a new
mount option handed to the kernel's cifs parser; a `/` or a `\` in a server re-aims the mount,
because the helper's own `parse_unc()` splits on exactly those. Quoting them would be a second
implementation of that parser, and two parsers of one string eventually disagree.

The password reaches `mount.cifs` on a file descriptor: `PASSWD_FD=0`, and the bytes on the child's
stdin. An option string is argv, an environment value is `/proc/<pid>/environ`, and a password file
is a file somebody has to delete. A `krb5` mount is given neither the descriptor nor the
environment, because there is nothing on stdin for the helper to read and a helper told otherwise
waits for a descriptor already at end of file.

`krb5` is a verb of its own and not `cifs` with an empty count. A ticket is in the caller's
credential cache and nothing about it crosses this socket, so there is no second frame to read and
nothing to wipe afterwards, and the byte count a secret-carrying verb declares may not be zero. What
`krb5` adds to the option string is `sec=krb5` and `cruid=<the caller>`: the daemon is root and the
mount is the caller's, so without `cruid` the `cifs.upcall` helper reads root's credential cache —
empty on a machine where nobody has any reason to `kinit` as root — and the mount fails with
`Required key not available` naming no user. The helper and the `request-key` rule are both checked
before the module is loaded and before the mountpoint is made, so a machine that cannot do this says
which file is missing.

A server name the C library cannot resolve is resolved before the helper runs. `nsswitch.conf` is
inert on musl and there is no winbind, so a `.local` name goes to `avahi-resolve-host-name` and a
bare NetBIOS label to `nmblookup` — only those two shapes, and only after `getaddrinfo` has already
failed on them, so an address or a DNS name never waits for a broadcast. What the helper is handed
is an `ip=` beside the name that was typed: a mountpoint named after an address is one nobody
recognises, and a lease that moved would leave the old number on the filesystem for ever.

A `browse` row is not an index. `cifs` and `krb5` name a server, so there is no row for a number to
be, and the row carries the name itself with the address beside it — which is what tells two
machines with the same NetBIOS name on different subnets apart. The list is never held between
requests, for the reason the device list is not.

The token count is fixed per verb, and a longer line is refused rather than truncated. The argument
allowlist is what makes an index mean an index: `mount 0 rm -rf /` is not a well-formed `mount 0`,
it is an unknown command.

### `/run/kdos-packd.sock`

Root and `wheel`. Thirteen verbs.

| Verb | Argument | Answers |
|---|---|---|
| `list` | — | Every pack the machine can see, with its state |
| `info` | A pack id | Its metadata |
| `mount` | A pack id | |
| `unmount` | A pack id | |
| `compose` | A box name | The merged root |
| `decompose` | A box name | |
| `install` | A filename in the staging directory | |
| `remove` | A pack id | |
| `rollback` | A pack id | |
| `graft` | A data pack id | |
| `ungraft` | A data pack id | |
| `status` | — | The mount route, the staging directory and the retention count |
| `ping` | — | Liveness |

`status` publishes the staging directory and the retention count, so a client writing a download
does not have to derive either.

Every id comes out of `list`. The single exception is `install`, which names a filename in the
daemon's own staging directory — the one place an unprivileged write is allowed. Relative traversal
and absolute paths are both errors.

### `$XDG_RUNTIME_DIR/kdos-cmd.sock` — the compositor

Your own compositor, so there is no privilege check. Requests and replies are structured objects.

| Verb | Answers |
|---|---|
| `list` | Every window: identifier, title, geometry, state, workspace, box, instance |
| `outputs` | The outputs and their scales |
| `boxes` | The distinct boxes with a window on screen |
| `thumb` | A window's pixels, written to a file |
| `peek` | Fades the windows to reveal the desktop |
| `run` | Executes something |

The reply buffer is per connection and small, so a partial response is truncated rather than allowed
to grow. `kdos hey` is the command-line front end.

### `$XDG_RUNTIME_DIR/kdos-frames.sock` — late frames

Write-only, from the compositor: one structured object per late frame, carrying the output, the
lateness, the compositor's own render cost, and a source field saying whether the measurement is a
presentation gap or a frame-clock gap. A presentation gap is what the user saw; a frame gap is what
the compositor was given.

It is non-blocking at both ends. A consumer that cannot keep up loses lines, and there is no
history: a consumer that connects late has missed what happened, and a ring buffer would hide that.
The frame loop is what this must never slow.

### `$XDG_RUNTIME_DIR/kdos-notify.sock` — notifications

| Verb | Answers |
|---|---|
| `count` | How many, and how many unseen |
| `list` | The history |
| `seen` | Clears the unseen count |
| `open <id>` | Activates an entry |
| `forget <id>` | Drops one entry |
| `clear` | Empties the history |
| `dnd` | Toggles do not disturb; `dnd on` and `dnd off` set it |
| `dismiss` | Puts the newest toast away |
| `dismiss all` | The same for every toast on the screen |
| `raise` | The last one dismissed, back on the screen |

A dismissal reports reason 2, dismissed by the user, so a client waiting on `NotificationClosed` is
told the truth. `raise` takes the entry out of the history, so a notification is on screen or in the
centre and never both, and it comes back without its buttons: the notification it came from is
closed, and its actions belong to the program that sent it.

### `$XDG_RUNTIME_DIR/kdos-clip.sock` — the clipboard

The daemon owns the history and the front end draws it — the same split notifications use.

A socket path that does not fit the address structure is refused rather than truncated. Truncation
binds a socket nobody asked for and answers the next start with an address-in-use error for a file
that appears not to exist, and two different runtime directories can land on one socket.

## Protocol conventions

Shared by every daemon above:

- One line per connection: a short request, a short answer, no session state.
- `ok` or `err <reason>`.
- Authorisation is the peer's real user id, read from the kernel — not anything in the message, and
  not the socket's mode.
- No verb takes a path. Identifiers come from a list the daemon published.
- A fixture mode on every daemon prints what it *would* do and does nothing.

## Files used as an interface

Not configuration, and not storage: these are how one program tells another something.

| File | Written by | Read by |
|---|---|---|
| `~/.cache/kdos/theme` | `kdos theme` | The compositor, the panel, the desktop, notifications — one word |
| `~/.cache/kdos/wallpaper.png` | `kdos theme` | The compositor, preferred over the configured path |
| `$XDG_RUNTIME_DIR/kdos-panel.overflow` | The panel | The overflow popup |
| `/usr/share/kdos/alien-apps` | The build | Every shim, and the launcher dispatcher |
| `~/.local/share/kdos/alien-apps` | `kdos-appbox genlaunchers --user` | The same, winning over the system table |
| `~/.local/share/kdos/observed-app-ids` | The compositor, once per window | `kdos appid` |
| `/var/lib/kdos/pack-manifest` | The pack daemon | Itself, so ungrafting removes exactly what was added |
| `/boot/initramfs.modules` | `01_initramfs.sh`, one module per line | The `linux` postinstall, which carries the new kernel's copies of that set into `/boot/initramfs-kdos.cpio.gz` |
| `/boot/initramfs-kdos.cpio.gz` | The `linux` postinstall: the image's initramfs with the new kernel's modules appended | `kdos-bootctl deploy` and `kinstall`, which take it over the image's `/boot/initramfs.cpio.gz` |
| `EFI/kdos/<slot>/vmlinuz`, `EFI/kdos/<slot>/initramfs.cpio.gz` on the ESP | `kinstall` for slot `a`, `kdos-bootctl deploy` for either | Limine, through the `/KDOS` entries `kdos-bootctl` regenerates on every change of boot state |
| `/run/kdos-svc.<name>.log` | `ksvc supervise`, capped at 64 KiB with one `.old` | You, when syslog is not running; every line also goes to syslog |
| `$XDG_RUNTIME_DIR/kdos-appbox.trace` | The launcher | You |
| `$XDG_RUNTIME_DIR/kdos/screencast.pid` | `kdos-record`, while its pipeline runs | The next `kdos-record`, which stops that one, and the panel, which draws its lamp |

The overflow file is published rather than re-derived: asking the system again at the moment
somebody clicked would be a second implementation of the same reading. Both readers of
`screencast.pid` check the pid is alive, because a marker left by a crash is not a recording.

## Environment variables

The tree reads 113 distinct `KDOS_*` names — 79 through `getenv()` in C and the rest from shell.
The ones below are the ones worth knowing; header guards and internal constants are omitted.

### Debugging

| Variable | Effect |
|---|---|
| `KDOS_COMP_DEBUG=1` | Raise the compositor's log level; every dropped frame is logged |
| `KDOS_PANEL_DEBUG=1` | The panel explains its layout decisions |
| `KDOS_BB_DEBUG=1` | The demo reports how its mixer is fed |
| `KDOS_WHEEL_DEBUG=1` | Trace every axis event, tick and dropped duplicate |
| `KDOS_CRT_DUMP=<prefix>` | Write the phosphor pass's input and output once |
| `KDOS_CRT_DUMP_FRAME=<n>` | Wait until frame *n* before dumping |
| `KDOS_PACKD_VERBOSE=1` | The pack daemon explains itself |
| `KDOS_WHISPER_MODEL=<file>` | One speech model, named exactly. When set, no directory is searched behind it |

### Testing seams

| Variable | Points at |
|---|---|
| `KDOS_DUMP_SIZE=WxH` | The size an offscreen dump renders at |
| `KDOS_GOLDEN_UPDATE=1` | Regenerate reference frames instead of comparing |
| `KDOS_RES_FIXTURE` | A recorded system state for the monitor |
| `KDOS_PRIVACY_PROC` | A recorded process tree for the privacy indicator |
| `KDOS_ENERGY_PROC`, `KDOS_ENERGY_POWERCAP` | Recorded trees for the energy daemon |
| `KDOS_MOUNTD_SYS`, `KDOS_MOUNTD_DEV`, `KDOS_MOUNTD_FSTAB`, `KDOS_MOUNTD_MOUNTS` | Recorded state for the media daemon |
| `KDOS_MOUNTD_CONF`, `KDOS_MOUNTD_MEDIA`, `KDOS_MOUNTD_UEVENT` | Its configuration file, its mount root, and a FIFO standing in for the kernel's uevent socket |
| `KDOS_PACK_STORE`, `KDOS_PACK_MEDIUM`, `KDOS_PACK_MANIFEST` | Alternate pack locations |
| `KDOS_INITRD` | An alternate boot image, for the microcode check |
| `KDOS_BOOTSTATE` | An alternate boot-state file |
| `KDOS_*_SOCKET` | Move a daemon's socket. It grants nothing — authorisation never depended on the path |

Every one of the mountd seams is read only under `--fixture`. A variable inherited from an init
environment names nothing.

### Behaviour

| Variable | Effect |
|---|---|
| `KDOS_A11Y=1` | Enable the accessibility stack for one boxed launch |
| `KDOS_BOX` | Set inside every box: its name. What identifies an X11 window's box |
| `KDOS_REQUIRE_SIG=1` | Refuse anything unsigned |
| `KDOS_ALLOW_UNVERIFIED` | The opposite, and it says so |
| `KDOS_NO_ANIM=1` | Skip the banner animation |
| `KDOS_NO_SFX=1` | Silence sound effects |
| `KDOS_NO_LOCK_ON_SUSPEND` | Do not lock when suspending |
| `KDOS_ASCII=1` | Force the lowest glyph tier |
| `KDOS_MARCH_RUNS` | How many samples the optimisation measurement takes |
| `KDOS_WHEEL_MIN_MS` | The duplicate-wheel gate; `0` disables it |

### Build

| Variable | Effect |
|---|---|
| `KDOS_REPLAY=1` | A step's marker guard stands down — set for steps a plan named |
| `KDOS_ISO_SOURCES=1` | Put the sources on the medium |
| `KDOS_PACK_KDOS=1` | Build this root filesystem as a base pack |
| `KDOS_GIT_COMMIT`, `KDOS_GIT_DIRTY` | Recorded in each phase's snapshot manifest as `git_commit` and `git_dirty`, and shown by the restore picker, which marks a snapshot stale when either disagrees with the tree. Nothing on the image reads them — `/etc/os-release` carries a static version |
| `KDOS_SNAPSHOT_PATHS`, `KDOS_SNAPSHOT_EXCLUDE`, `KDOS_PHASE_TITLE`, `KDOS_PHASE_DESC` | The phase metadata block — parsed, never sourced |

Any variable a chroot step reads must be named on the chroot command line, because the chroot is
entered with a cleared environment. Exactly three are forwarded — `KDOS_REPLAY`,
`KDOS_ISO_SOURCES` and `KDOS_PACK_KDOS`, in `script/chroot_exec.sh`. A fourth added to the makefile
and not there would reach every host step and no chroot one.

## See also

- [Configuration](configuration.md) — every setting, with defaults
- [The daemons](../04-programs/daemons.md) — what each socket's owner does
- [Architecture overview](../03-architecture/overview.md) — where state lives, in summary
- [The security model](../03-architecture/security-model.md) — why authorisation is the credential
- [Repository layout](repository-layout.md) — the source tree, rather than the target
