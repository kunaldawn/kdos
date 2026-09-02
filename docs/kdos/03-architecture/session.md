# The session

What happens between a login and having a desktop: the session message bus, audio, the portal
stack, the supervised chrome, screen capture and the clipboard, input methods, and the environment
a containerised application receives. This is the layer where the host and a box meet, so most of
it exists to make that meeting work.

**There are two sessions and they share this layer.** The console desktop (`kdos-con`) is the
default — a login on tty1 reaches it — and the graphical one (`kdos-comp`) is started with
`kdos-desktop`. Where a page below says "the compositor" without qualification it means whichever
one is running.

## Starting a session

**Two sessions, one bring-up.** The console desktop is what a login on tty1 reaches without anyone
typing a command; the graphical one is started by hand with `kdos-desktop`. What they share is in
`/usr/local/lib/kdos/session-common.sh`, which is **sourced, never executed**:

| Function | Does |
|---|---|
| `kdos_session_runtime` | `XDG_RUNTIME_DIR`, before anything uses it |
| `kdos_session_keymap` | The console keymap as XKB variables |
| `kdos_session_boxes` | The appbox warmup, and giving idle ones back |
| `kdos_session_bus` | One session bus per user, at a fixed path |
| `kdos_session_audio` | PipeWire, once per user rather than once per session |
| `kdos_session_once` | The login sound, the first-run card, the session restore |

It is one file rather than two copies because each of those blocks carries a trap that cost a
debugging session to find — the `flock 7>&-` in the bus function above all — and a second copy is a
second place to lose one. **The keymap function especially:** `libkkms` reads the same
`XKB_DEFAULT_LAYOUT` that every Wayland client does, so leaving that table in the graphical script
gave the console desktop US QWERTY on a machine whose owner does not type it.

`kdos_session_once` takes its **readiness test as a command**, because what "up" means differs: a
Wayland socket for one session, a session socket for the other. The command must block until the
display is there and export whatever the children need to reach it.

Both start scripts are deliberately still shell. Every line in them is a fix for something
specific, they take no untrusted input, and rewriting them would buy nothing but risk.

**`kdos-desktop`** prepares the environment and hands over:

1. Refuses immediately if the compositor is not installed, naming the build phase it comes from.
2. **Detects a virtual GPU** and decides the renderer. It reads the feature bits from `sysfs`, not
   from the kernel log — the log is root-only under `dmesg_restrict`, so reading it there would
   force software rendering for every ordinary user and silently disable the CRT pass. It also
   disables hardware cursors on virtio, where the cursor plane misreports what it supports.
3. The shared bring-up above, then executes `kdos-desktop-start`.

**`kdos-desktop-start`** brings up what must exist before or beside the compositor: the compositor
itself with its log kept and a restart offer if it dies, and — in subshells that wait for the
compositor's socket — the portal stack, the input method, and the once-per-session block.

**`kdos-con-start`** is the console session's equivalent, and **nothing on its path opens a Wayland
socket** — which is what makes it the session that still comes up on a machine whose GPU driver
does not. After the shared bring-up it:

1. Creates the session socket at a **fixed** path, `$XDG_RUNTIME_DIR/kdos/con.sock`. Fixed, because
   `kdos con attach` and every surface started later find the session by that name, and a name that
   changed every login could only be discovered by guessing.
2. Starts **only** the KDOS portal backend. `xdg-desktop-portal-wlr` is a Wayland client and there
   is no compositor here for it to connect to, so starting it would leave a backend that dies at
   once and a front-end that has already snapshotted it.
3. **Supervises the view, not the session.** A view holds a DRM device and a seat and is the half
   that can lose them; the session holds every window and must outlive that. A view that exits
   cleanly is a detach and is not restarted; one that crashes is, three times in sixty seconds.
4. Runs `kdos-con --serve` under the same crash harness `kdos-desktop-start` uses.

**Both harnesses write stderr to a file and follow it with `tail -f`, never a pipe.** Every
descendant inherits fd 2, and a pipe's reader sees EOF only when the last write end closes — so one
surviving grandchild, such as the `conmon` that lives for the whole life of a boxed application,
would hold the reader open forever. The symptom is a terminal that sits there with no desktop, no
error and nothing in a log.

## The session message bus

**One daemon per user, at a fixed path**: `$XDG_RUNTIME_DIR/bus`.

It is deliberately not started with the usual session-launcher wrapper, which listens on a socket
in the host's `/tmp` — a directory a box does not share in the way that matters. Every application
inside a box then sees a bus address pointing at nothing: single-instance negotiation breaks so
every impatient re-click spawns another full instance, settings and accessibility probes stall,
and notifications go nowhere. `$XDG_RUNTIME_DIR` **is** shared with a box, so one fixed address is
valid on both sides.

Three details in that startup are each load-bearing:

- **The address carries no GUID.** A later session rebinding the socket would otherwise abort
  every client using a strict D-Bus implementation, with a server-identity mismatch.
- **The start is serialised with a lock**, or two sessions racing could both decide the daemon is
  down and the loser would remove the winner's socket.
- **The lock descriptor is closed for the daemon.** The bus daemon inherits open descriptors, and
  it outlives the subshell — so without closing it, the daemon holds the lock until logout and the
  *second* login of a boot blocks forever before printing anything at all. The symptom is a
  terminal that just sits there: no compositor, no error, nothing in a log.

## Audio

PipeWire runs on the **host**, started by `kdos-desktop-start`: the daemon, a session manager and
the PulseAudio compatibility layer. Boxed applications reach it through the shared
`$XDG_RUNTIME_DIR`.

That is also the route screen-capture audio takes. Capture goes portal → ScreenCast → PipeWire,
with the sockets crossing into the box the same way.

## Portals

A portal is how a sandboxed application asks the host to do something it cannot do itself. There
is a front-end daemon that owns the interfaces, and **backends** that implement them.

`fs/usr/share/xdg-desktop-portal/kdos-portals.conf` selects them, and both its name and its
contents are load-bearing. The front end looks for `<desktop>-portals.conf` with the desktop name
lowercased, so `KDOS` gives `kdos`. Without the file, the only thing selecting a backend is a
`UseIn=` line inside another backend's own description, which lists other desktops and has never
heard of this one.

```ini
[preferred]
default=none
org.freedesktop.impl.portal.ScreenCast=wlr
org.freedesktop.impl.portal.Screenshot=wlr
org.freedesktop.impl.portal.FileChooser=kdos
org.freedesktop.impl.portal.Settings=kdos
org.freedesktop.impl.portal.AppChooser=kdos
```

**TWO SESSIONS, TWO BACKENDS, ONE INTERFACE.** `XDG_CURRENT_DESKTOP` is a list, most specific
first, and the front end reads the first configuration file it finds for any name in it. The
console session sets `KDOS-Console:KDOS` and gets `kdos-console-portals.conf`; the compositor keeps
`KDOS` and gets the file above. The one line that differs is `ScreenCast`, which on the console is
`kdos` — `xdg-desktop-portal-wlr` is a Wayland client and there is no compositor on that path, so it
would start, fail to connect, and leave the interface with a backend that is not there.

A semicolon-separated list would not do it. A backend that is D-Bus activatable is always
"available", so `wlr;kdos` starts `wlr` and fails rather than falling through.

**`default=none` is honest rather than lazy.** The backend every other desktop falls back to is a
GTK program, and there is no GTK on this host — so Print, Email, Wallpaper and the rest genuinely
have nobody to serve them. A portal that would only ever fail is not advertised.

**`OpenURI` is the front end's own portal, and what it needs from a backend is `AppChooser`.**
"Open this on the host for me" is what every containerised application asks when a link or a
downloaded file is clicked. The front end resolves the host's handlers itself and launches the
chosen one from the host's own launchers — so a link in a boxed application opens in the browser
this desktop generated an entry for. It exports the interface **only** when a backend answers
`AppChooser`. A backend implementing `impl.portal.OpenURI`, an interface the specification does
not have, leaves every boxed `xdg-open` answering "no such interface".

### The startup ordering

**The front-end daemon snapshots its backends when it starts.** So `kdos-desktop-start` must, in
this order: wait for the compositor's socket to exist, push the display variable into the bus
activation environment, start the capture backend, **wait for it to own its bus name**, and only
then restart the front end. Get the order wrong and ScreenCast stays empty for the whole session.

### The KDOS backend

`src/desktop/xdg-desktop-portal-kdos` serves FileChooser, Settings, AppChooser and — on the console
— ScreenCast. It is a bus adapter and nothing more: it spawns `kdos-pick` and reads its output, so
the file chooser stays an ordinary program you can run by hand, script or replace, and it spawns
`kdos-view --cast` and reads the node id that view registered.

**A recording is a view nobody looks at.** The session holds cells and a view is what turns them
into pixels, so ScreenCast on the console is a second `kdos-view` rasterising into a PipeWire
stream. The backend opens no device, renders nothing and holds no frame; the view prints the node
and the stream's pixel size on one line, because the backend never rasterises and cannot work the
size out.

**One source, the whole desktop.** There is one grid and no notion of a monitor inside it, and a
"window" there is a rectangle of cells with nothing behind it to capture separately — so
`AvailableSourceTypes` is MONITOR and nothing else. The cursor mode is EMBEDDED for the same kind of
reason: the caret is a cell the toolkit draws in reverse video, so it is already in the frame and
there is no separate image to send alongside.

Three rules it exists to keep:

- **Every request is answered** — success, cancelled, or error. An unanswered request leaves the
  asking application blocked forever.
- **The parent-window hint is ignored, and it says so.** Positioning a dialog over the window that
  asked for it needs cross-process window referencing that is not wired up, so dialogs open
  centred.
- **The chooser is executed with an argument vector, never a command string.**

**And a cast completes on a LINE, not on end-of-file.** The chooser exits and its output ends; the
cast view does not exit — it *is* the stream — so the reply is built when its first line arrives and
the process is left running until the session is closed.

**And the bus loop does not block on the dialog.** The fork happens in the handler, the request
message is retained, the handler returns "handled" *without* replying, the pipe joins the main
loop, and the reply is built when the chooser exits. A portal backend is a server, and a server
that stops serving while it thinks is a server that is down — without this, a second application's
Open queues behind the first, and a boxed application asking Settings for the colour scheme, which
happens on every launch, hangs until the dialog is dismissed.

## Supervised chrome

The compositor starts and supervises the desktop's own programs from a table in its source. Five
entries:

| Program | Per output? | Gated on |
|---|---|---|
| `kdos-shell` | yes | The panel being enabled |
| `kdos-desk` | yes | `desktop_icons` |
| `kdos-slit` | yes | `slit`, off by default |
| `kdos-notifyd` | **no** | always |
| `kdos-clip` | **no** | `clipboard` |

**Three are per output** because a layer surface is placed by the compositor on one screen, and
our toolkit has a single cell buffer per process — so a second monitor cannot be a second surface,
it has to be a second process. Each takes the output name as an argument.

**Two are single-instance** because each owns something unique: the notification daemon owns a bus
name that a second instance would simply fail to take, and the clipboard owns a socket and holds
its history in memory, so a second instance would be a second history nobody could reach.

Three properties of the supervision:

- **One fork, and the reap goes through the compositor's existing child-signal handling.** A
  second signal watcher would race the first. Because that signal coalesces, the handler also
  polls each supervised child specifically — otherwise two chrome programs dying together leave
  one a zombie that never respawns.
- **Five deaths in thirty seconds stops the respawn**, because a crash loop buries the log line
  that explains it.
- **A child gets every ignored signal disposition back before it is executed.** Ignored
  dispositions survive execution exactly as the signal mask does, so a session started under a
  wrapper that ignores a signal would hand that down to every program it starts — and the live
  retint, which is delivered as a signal, would never fire.

When an output goes away its chrome is signalled and the slot marked stopping, so the reap frees
it instead of respawning a program whose screen is gone.

## Screen capture and the clipboard

The compositor offers **both generations** of each protocol: the older screen-copy and
buffer-export interfaces, and the newer image-capture interface with output and window sources
that the capture portal backend prefers. Likewise both generations of the data-control interface
that clipboard tools use. Implementing only one strands either the tools shipped here or
everything written after them.

**A boxed application is kept off all of them.** A client from a box carries a security context,
and the compositor's filter gives such clients a fixed allowlist — surfaces, the seat, buffer
sharing, text input, the primary selection — and none of the capture, data-control or
input-method interfaces.

That is what makes the portal the sanctioned route rather than a convenience: a boxed screen
recorder **cannot** bind the capture interface at all, so it must ask the portal, which runs on
the host and asks you which output to share.

**A screenshot of a phosphor desktop looks like the desktop.** Output capture copies the output's
committed buffer, which under the CRT pass is the processed one. Per-window capture renders the
window's own contents and is untinted. Both are the honest answer to what was asked.

## Input methods

Three parties that never speak to each other directly. The compositor is the wire between them:

```
fcitx5  ──input-method──▶  kdos-comp  ──text-input──▶  the application
      ◀──virtual-keyboard──                             (host or boxed)
```

The application half of that — the text-input interface — is **inside** the sandbox allowlist,
deliberately: denying it would deny input methods to exactly the applications that need one most.
The engine half is outside it, because a client that can be an input method receives every
keystroke on the seat, which is a keylogger by design.

The engine is `fcitx5`, built Wayland-only. A boxed application reaches it **through the
compositor and never directly**: the Qt input-module variable is set to the Wayland route, and
the GTK one is deliberately **not set at all**, because GTK on Wayland picks the right route by
itself when the variable is unset and setting it is how a working GTK application stops accepting
input. Neither is ever set to the engine's own name — that is the X11-era route, where each
toolkit talks to the engine directly, and inside a container that engine does not exist.

## The environment a box receives

Executing a command inside a container inherits **nothing** — not the container's own init
environment, not the caller's. So every variable a launch needs is stated explicitly:

| Variable | Why |
|---|---|
| `PATH` | Including `/usr/games`, or Debian's games are all "not found" |
| `HOME`, `USER`, `LOGNAME` | A missing user name is a crash in some toolkits |
| `LANG` | `C.UTF-8` unless the host's is already UTF-8; a non-UTF-8 locale breaks GTK |
| `XDG_RUNTIME_DIR`, `XDG_SESSION_TYPE` | The shared runtime directory, and Wayland |
| `WAYLAND_DISPLAY` | The compositor socket |
| `DBUS_SESSION_BUS_ADDRESS` | Or a single-instance application blocks with no window |
| `XDG_CURRENT_DESKTOP=KDOS` | Portal and theme selection |
| `GTK_USE_PORTAL=1` | See below |
| `GTK_THEME=KDOS` | The recoloured theme in `$HOME` |
| `GSETTINGS_BACKEND=keyfile` | No settings daemon is reachable |
| `QT_IM_MODULE=wayland` | Input methods through the compositor |
| `CUPS_SERVER` | The host's print socket, when it exists |
| `NO_AT_BRIDGE`, `GTK_A11Y` | A default, not a policy — see below |
| `DISPLAY` | Added when Xwayland is running, for X11-only applications |

Measured across the catalogue when this was incomplete: most graphical applications failed to map
a window, only a few of them saying why, every game died on a missing executable, and applications
using the single-instance mechanism blocked in negotiation with no window and no message.

**`GTK_USE_PORTAL=1` is what makes the KDOS portal reachable at all.** A GTK application routes
through the portal only when it believes it is sandboxed, which it decides from a marker file or
from that variable — and a container is neither. Without it, FileChooser and Settings all existed,
all answered, and **nothing ever called them**: every boxed application went on drawing its own
file dialog, which is exactly the rounded antialiased window `kdos-pick` was written to replace.

**The accessibility variables are a default, not a policy.** The host runs no accessibility
registry, so every boxed GTK application spent a startup probe timing out — which justified
disabling the probe and did **not** justify hard-disabling the accessibility stack of a hundred
applications on the grounds that the host had none. A screen reader running *inside* the box can
reach the box's own registry, so you opt in with `~/.config/kdos/a11y` — an empty file is enough —
or per launch with `KDOS_A11Y=1`.

## What is shared into a box

| Path | Mode |
|---|---|
| `$HOME` | read-write |
| `/tmp` | read-write |
| `/run/user/<uid>` | read-write — the bus, the compositor socket, PipeWire |
| `/dev`, `/sys` | read-write, when the profile allows devices |
| `/dev/shm` | read-write |
| `/run/cups` | read-write, when the print service is running |
| The pack mount directory | **read-only** |
| `kdos-boxinit` | read-only, as the container's init |
| `/run/lock` | a private temporary filesystem |

Sharing is fixed when the container is **created**. A box created before the print service was
running does not have that socket until it is recreated, which is why the variable is set
per-launch and probes again: the two halves are deliberately asymmetric.

## Ending a session

The compositor handles termination signals through its event loop and tears down in order — the
wallpaper, the frame reporter, the idle policy, then the CRT pass — before the server itself is
destroyed. The panel and the notification daemon notice the dead compositor themselves, because
the toolkit's event pump uses the documented prepare-read sequence and so sees a closed socket
rather than spinning on it.

## See also

- [Architecture overview](overview.md) — where the session sits in the whole system
- [kdos-comp](../04-programs/kdos-comp.md) — the compositor and its configuration
- [Packs and boxes](packs-and-boxes.md) — how a box is built and started
- [The security model](security-model.md) — the sandbox allowlist and what it denies
- [Boot and init](boot-and-init.md) — everything before the login prompt
- [The desktop](../02-user-guide/desktop.md) — using what this page starts
