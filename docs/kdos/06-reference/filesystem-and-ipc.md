# Filesystem and IPC

Every path KDOS owns on a running system, every socket and its verbs, and the environment
variables that change behaviour. This is the lookup page: what lives where, and how the parts talk
to each other.

## The target filesystem

### `/etc/kdos/` — machine configuration

| Path | Holds | Survives reinstall |
|---|---|---|
| `packd.conf` | The pack daemon's retention | no |
| `pack-sources` | Where application updates are looked for | no |
| `zram.conf` | Compressed-swap size and algorithm | no |
| `mountd.conf` | Removable-media options. **Not shipped** — create it | no |
| `keys/` | Trusted keys for **host packages** | no |
| `keys/packs/` | Trusted keys for **application packs** | no |

Every key is documented in [Configuration](configuration.md).

### `/var/lib/kdos/` — state written by the system

| Path | Holds | Written by |
|---|---|---|
| `packs/` | Installed application packs | The pack daemon, as root |
| `packs/mnt/` | Mount points for packs | The pack daemon |
| `packs/staging/` | **The one place an unprivileged download may land**, mode 01777 | You |
| `pack-manifest` | Every graft made, so removal is exact | The pack daemon |
| `fs-manifest` | Every path the build's static tree provided | The build |
| `march.ledger` | Measured per-machine optimisation results | `kdos march` |

### `/usr/share/kdos/` — generated and shipped data

| Path | Holds |
|---|---|
| `alien-apps` | Application name → the command line that runs it in its box |
| `icons/atlas.kia` | The rasterised icon atlas |
| `icons/art/`, `icons/marks/` | The vendored icon source and the KDOS marks |
| `cursors/art/` | The vendored cursor source |
| `gtk-theme/theme/` | The vendored stylesheet source |
| `secdb.txt` | The vendored security database |
| `reasons` | The explanations `kdos why` prints |
| `doc/` | Shipped documentation |
| `logo.txt` | The banner logo, generated from the mascot |
| `splash.psf` | The splash font |
| `boot/` | Boot artwork |
| `memtest86plus/` | The memory tester on the medium |

### `/var/lib/kpkg/` — the host package database

| Path | Holds |
|---|---|
| `db/<name>` | One entry per installed package, with its manifest |
| `db/.recipe/<name>` | The recipe hash a package was installed from |

### Per-user

| Path | Holds |
|---|---|
| `~/.config/kdos/` | Your desktop configuration |
| `~/.config/kdos-comp/` | The compositor's own configuration |
| `~/.local/share/kdos/alien-apps` | Your own launcher table — **user entries win** over the system one |
| `~/.local/share/kdos/packs/` | Where a data pack's contents are grafted for boxed applications |
| `~/.local/share/applications/` | Launchers for applications you installed |
| `~/.local/bin/` | Shims for applications you installed |
| `~/.local/state/kdos/appusage` | Launch counts, which order the Start menu's frequent column |
| `~/.local/state/kdos/diskwarn` | `<step> <mountpoint>` per line: which disk-full step the panel has already warned about |
| `~/.cache/kdos/theme` | **One word**: the accent name. The entire theme state the desktop reads |
| `~/.cache/kdos/wallpaper.png` | The retinted wallpaper the compositor prefers |
| `~/.local/share/Trash/` | The freedesktop trash |

### `$XDG_RUNTIME_DIR` — per session

| Path | Is |
|---|---|
| `bus` | The session message bus, at a **fixed** path because a box shares this directory |
| `.kdos-bus.lock` | Serialises starting that bus |
| `wayland-*` | The compositor's socket |
| `kdos-cmd.sock` | The compositor's query socket |
| `kdos-frames.sock` | Late-frame reports |
| `kdos-notify.sock` | The notification daemon |
| `kdos-clip.sock` | The clipboard history |
| `kdos-panel.overflow` | What the panel has hidden behind the chevron |
| `kdos-comp.log` | The compositor's output |
| `kdos-appbox.trace` | Stage timings for the last launches |
| `kdos/` | The console desktop's sockets, mode **0700** |
| `kdos/<name>.sock` | A console session's **surface** socket |
| `kdos/<name>.view` | The same session's **view** socket |
| `kdos/<name>.windows` | What has a window there, one app id per line |
| `kdos-con.log` | The console session's output |

## Sockets

Every root daemon takes the same shape: one socket in `/run`, mode 0666, **authorised by the
peer's credentials** rather than by the socket's mode, one line per connection, and no verb that
takes a path.

Replies are `ok` with optional data, or `err <reason>`. An unauthorised caller gets
`err not permitted`.

### `/run/kdos-powerd.sock`

Root and `wheel`.

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

**The raw counter and the sampling interval are never published**, and the interval is fixed by the
daemon rather than requested — so it cannot be driven toward being a measurement instrument.

### `/run/kdos-oomd.sock`

Root and `wheel`.

| Verb | Argument | Answers |
|---|---|---|
| `status` | — | What it is watching, and what it has killed |
| `ping` | — | Liveness |

**Nothing in this protocol names a process**, so there is nothing to aim. Killing is the daemon's
own decision or it does not happen.

### `/run/kdos-mountd.sock`

Root and `wheel`.

| Verb | Argument | Answers |
|---|---|---|
| `list` | — | The eligible devices, **with an index each** |
| `mount` | An index | The mountpoint |
| `unmount` | An index | |
| `ping` | — | Liveness |

**The client asks for an index out of a list the daemon published**, and the daemon decides the
device, the mountpoint and the options. The list is rescanned on every request.

### `/run/kdos-packd.sock`

Root and `wheel`.

| Verb | Argument | Answers |
|---|---|---|
| `list` | — | Every pack the machine can see, with its state |
| `info` | A pack id | Its metadata |
| `mount` / `unmount` | A pack id | |
| `compose` / `decompose` | A box name | The merged root |
| `install` | **A filename in the staging directory** | |
| `remove` | A pack id | |
| `rollback` | A pack id | |
| `graft` / `ungraft` | A data pack id | |
| `status` | — | The mount route, **the staging directory** and **the retention count** |
| `ping` | — | Liveness |

**`status` publishes the staging directory and the retention count**, so a client writing a
download does not have to derive either.

**Every id comes out of `list`.** The single exception is `install`, which names a filename in the
daemon's own staging directory — the one place an unprivileged write is allowed. Relative
traversal and absolute paths are both errors.

### `$XDG_RUNTIME_DIR/kdos-cmd.sock` — the compositor

The user's own compositor; no privilege check. Requests and replies are structured objects.

| Verb | Answers |
|---|---|
| `list` | Every window: identifier, title, geometry, state, workspace, **box**, instance |
| `outputs` | The outputs and their scales |
| `boxes` | The distinct boxes with a window on screen |
| `thumb` | A window's pixels, written to a file |
| `peek` | Fades the windows to reveal the desktop |
| `run` | Executes something |

The reply buffer is per connection and small, so a partial response is truncated rather than
allowed to grow. `kdos hey` is the command-line front end.

### `$XDG_RUNTIME_DIR/kdos-frames.sock` — late frames

**Write-only, from the compositor.** One structured object per late frame, carrying the output,
the lateness, the compositor's own render cost, and **a source field** saying whether the
measurement is a presentation gap or a frame-clock gap — a presentation gap is what the user saw, a
frame gap is what the compositor was given.

**Non-blocking at both ends.** A consumer that cannot keep up **loses lines**, and there is **no
history** — a consumer that connects late has missed what happened, and a ring buffer would hide
that. The frame loop is what this must never slow.

### `$XDG_RUNTIME_DIR/kdos-notify.sock` — notifications

| Verb | Answers |
|---|---|
| `count` | How many, and how many **unseen** |
| `list` | The history |
| `seen` | Clears the unseen count |
| `open` | Activates an entry |
| `forget` | Drops one entry |
| `clear` | Empties the history |
| `dnd` | Toggles do not disturb |

### `$XDG_RUNTIME_DIR/kdos/<name>.sock` and `.view` — a console session

Not a line protocol: this pair is the console desktop's own framed protocol, and the two sockets
exist so that **only one of them is safe to forward**.

| Socket | Admits | May leave the machine |
|---|---|---|
| `<name>.sock` | Surfaces — programs that place windows | **never** |
| `<name>.view` | Views — a display, which holds no window state | over ssh, with `kdos con forward` |

**Which socket a client reached decides what it is allowed to be.** The kind in its handshake is a
claim and is overridden; a forwarded socket that admitted surfaces would give the far end the right
to place windows in your session, which is a different thing entirely from showing you yours.

`$KDOS_CON` names the **surface** socket, so a program started inside the session inherits an
address that opens a window. A view is told its socket on the command line.

**Three kinds of client, and what each is told.** A *surface* places a window and hears about its
own. A *view* is a display and is sent frames. A *shell* is the panel and its family, and it alone
is sent the **window list** — `TOPLEVEL_ADD`, `TOPLEVEL_STATE`, `TOPLEVEL_REMOVE` and `WORKSPACE`,
which carry on the console what `wlr-foreign-toplevel-management` and `ext-workspace-v1` carry on
Wayland. A program with a window in the session has no business knowing what else is open, and
because the surface socket never leaves the machine a forwarded display cannot ask either.

The two requests back — `ACTIVATE` and `CLOSE_REQUEST` — are **requests**. The session owns the
stack and every window's lifetime, so a panel says which window it means and reads the answer out
of the list it is then sent; a panel that raised a window itself would be a second implementation of
raising.

**There is no "send me the list" message, deliberately.** A client that could ask could ask
repeatedly. The session notices a shell attaching and sends the list again, which is the same shape
as the sprite resend a newly attached view gets.

**A shell may also ask the session to run a graphical application** — the one message on this socket
that is not about a window. The answer is `0` when it became an ordinary window, the terminal's
number when it was pinned to one, and a refusal when it could not be started at all. It is
restricted to shell-kind clients the way "detach every view" is, and for the same reason: not because it is a privilege boundary — a client that reached this socket is
already the session's own user and can fork and exec whatever it likes — but so the message has one
caller and one meaning.

The directory is created `0700` by the session server, and one that already exists with the wrong
mode or owner is **a refusal to start, not a `chmod`** — if it is not ours, quietly taking it over
puts the socket in a path another account chose, after which the peer-credential check is guarding
the wrong door.

**One descriptor crosses one channel, and it is neither of these.** An embedded application's
compositor is a child of the session, and the frames come back over a `socketpair` created before
the fork — never a path anything can connect to. That is what keeps both published protocols
descriptor-free, and descriptor-free is what lets the view socket be forwarded.

**There is no TCP listener.** A remote desktop is a forwarded unix socket and inherits ssh's
authentication, which is why it needs none of its own; the self-test asserts the absence by
grepping these sources for `AF_INET`. `remote = no` in `con.conf` is enforced by `kdos con forward`
refusing to build the tunnel — a complete refusal rather than a check something could connect
around.

### `$XDG_RUNTIME_DIR/kdos-clip.sock` — the clipboard

The daemon owns the history and the front end draws it — the same split notifications use.

**A socket path that does not fit the address structure is refused, not truncated.** Truncation
binds a socket nobody asked for and answers the next start with an address-in-use error for a file
that appears not to exist — and two different runtime directories can land on one socket.

### `$XDG_RUNTIME_DIR/kdos/<name>.windows`

One application id per line, for every mapped, non-minimised window in that console session.
Rewritten only when the set changes — a desktop that rewrote a file every frame would be doing IO
for as long as it is switched on.

It exists for `kdos-box gc`, which must know whether a box still has something on screen before it
stops it. On Wayland that is a question for the compositor's command socket; here it is a file,
because teaching `kdos-appbox` this session's protocol would pull `libkcon` and the whole cell model
into a binary that is on every image.

**"Cannot tell" is not "no window".** `box_has_window` answers 1, 0 or **-1**, and the collector
leaves a box alone on -1. A collector that read the third as the second would stop every warmed box
on a desktop it did not know how to question.

## Files used as an interface

Not configuration, and not storage: these are how one program tells another something.

| File | Written by | Read by |
|---|---|---|
| `~/.cache/kdos/theme` | `kdos theme` | The compositor, the panel, the desktop, notifications — **one word** |
| `~/.cache/kdos/wallpaper.png` | `kdos theme` | The compositor, preferred over the configured path |
| `$XDG_RUNTIME_DIR/kdos-panel.overflow` | The panel | The overflow popup — **published rather than re-derived**, because asking the system again at the moment somebody clicked would be a second implementation of the same reading |
| `/usr/share/kdos/alien-apps` | The build | Every shim, and the launcher dispatcher |
| `~/.local/share/kdos/alien-apps` | `genlaunchers --user` | The same, **winning** over the system table |
| `~/.local/share/kdos/observed-app-ids` | The compositor, once per window | `kdos appid` |
| `/var/lib/kdos/pack-manifest` | The pack daemon | Itself, so ungrafting removes exactly what was added |
| `$XDG_RUNTIME_DIR/kdos-appbox.trace` | The launcher | You |

## Protocol conventions

Shared by every daemon here:

- **One line per connection.** A short request, a short answer, no session state.
- **`ok` or `err <reason>`.**
- **Authorisation is the peer's real user id**, read from the kernel — not anything in the message,
  and not the socket's mode.
- **No verb takes a path.** Identifiers come from a list the daemon published.
- **A fixture mode** on every daemon prints what it *would* do and does nothing.

The console session's protocol shares only the third of these: authorisation is the peer's real
user id. It is framed rather than line-based, it is stateful, and **it passes no file
descriptors** — which is what lets the whole of it survive being forwarded down an ssh channel.

## Environment variables

Around 120 `KDOS_*` variables exist; these are the ones worth knowing. Header guards and internal
constants are omitted.

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

### Testing seams

| Variable | Points at |
|---|---|
| `KDOS_DUMP_SIZE=WxH` | The size an offscreen dump renders at |
| `KDOS_GOLDEN_UPDATE=1` | Regenerate reference frames instead of comparing |
| `KDOS_RES_FIXTURE` | A recorded system state for the monitor |
| `KDOS_PRIVACY_PROC` | A recorded process tree for the privacy indicator |
| `KDOS_ENERGY_PROC`, `KDOS_ENERGY_POWERCAP` | Recorded trees for the energy daemon |
| `KDOS_MOUNTD_SYS`, `KDOS_MOUNTD_DEV`, `KDOS_MOUNTD_FSTAB`, `KDOS_MOUNTD_MOUNTS` | Recorded state for the media daemon |
| `KDOS_PACK_STORE`, `KDOS_PACK_MEDIUM`, `KDOS_PACK_MANIFEST` | Alternate pack locations |
| `KDOS_INITRD` | An alternate boot image, for the microcode check |
| `KDOS_BOOTSTATE` | An alternate boot-state file |
| `KDOS_*_SOCKET` | Move a daemon's socket. **Grants nothing** — authorisation never depended on the path |

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
| `KDOS_GIT_COMMIT`, `KDOS_GIT_DIRTY` | Stamped into the version |
| `KDOS_SNAPSHOT_PATHS`, `KDOS_SNAPSHOT_EXCLUDE`, `KDOS_PHASE_TITLE`, `KDOS_PHASE_DESC` | The phase metadata block — **parsed, never sourced** |

**Any variable a chroot step reads must be named on the chroot command line**, because the chroot
is entered with a cleared environment. Three are forwarded; a fourth added to the makefile and not
here would reach every host step and no chroot one.

## See also

- [Configuration](configuration.md) — every setting, with defaults
- [The daemons](../04-programs/daemons.md) — what each socket's owner does
- [Architecture overview](../03-architecture/overview.md) — where state lives, in summary
- [The security model](../03-architecture/security-model.md) — why authorisation is the credential
- [Repository layout](repository-layout.md) — the source tree, rather than the target
