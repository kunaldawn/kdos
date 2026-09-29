# The session

This chapter explains what happens between logging in and seeing the desktop, and what keeps
running while the desktop is in use: the per-user message bus, the audio graph, the portals, the
compositor and the desktop programs it supervises, screen capture, the clipboard, input methods,
and the environment a containerised application receives. It is written for people who
administer or debug a KDOS desktop, and for anyone changing the session scripts or a program that
runs inside a session. If *box* and *pack* are new words, read the
[architecture overview](overview.md) first; for everything before the login prompt, read
[boot and init](boot-and-init.md). For using the desktop rather than understanding it, see
[the desktop](../02-user-guide/desktop.md).

A *session*, in this chapter, is the set of processes one user's graphical login starts and owns:
a D-Bus session bus, PipeWire, the portal services, the input method, the compositor, and the
desktop programs the compositor starts. None of it runs as root, and none of it is started by the
init system. The session is also where the host and a *box* (a containerised application; see the
[glossary](../06-reference/glossary.md)) meet, and much of the design below exists to make that
meeting work: one bus address, one runtime directory and one audio socket that are valid on both
sides of the container boundary.

Two shell scripts start a session. `kdos-desktop` prepares the environment and the session bus;
`kdos-desktop-start` starts audio, the portals and the input method, then runs the compositor.
Both are plain shell in `/usr/local/bin`; reading them shows exactly what a login does. The
sections that follow take the pieces in that order: the start itself, then
[the session message bus](#the-session-message-bus), [audio](#audio), [portals](#portals), the
[supervised chrome](#supervised-chrome), [screen capture and the
clipboard](#screen-capture-and-the-clipboard), [input methods](#input-methods), what a box
receives, and [how a session ends](#ending-a-session).

## Starting a session

On a default install, switching the machine on reaches the desktop without anyone typing a
command:

1. `kdos-getty` runs on tty1 (from `/etc/inittab`). It loads the console font and palette, raises
   the real-time limits the audio stack inherits, and, for an autologin account, moves itself into
   that user's delegated cgroup (a control-group subtree the user may manage; below).
2. It runs `kdos-login tty1`, which reads `autologin = <user>` from `/etc/kdos/login.conf` (the
   shipped file says `autologin = kdos`) and starts `agetty --autologin <user>`. With no
   `autologin` line, or a missing or unreadable file, `agetty` shows a login prompt instead.
3. The login shell reads `~/.bash_profile` (or `~/.zprofile` for an account whose shell is zsh,
   after the zsh port's `/etc/zsh/zprofile` has read `/etc/profile`). On tty1, when no Wayland
   display is set and `kdos-desktop` exists, it runs `kdos-desktop`.

Logging in on any other terminal, over a serial line or over ssh gives a shell and no desktop.
tty2 is kept as a recovery console for that reason: its inittab line runs a plain `getty` behind
`kdos-getty`, and the profile starts nothing there.

```
login shell (tty1)
  └─ kdos-desktop          checks the compositor exists, picks the renderer,
       │                   runtime dir, keymap, box warmup, session bus
       └─ kdos-desktop-start
            ├─ audio: pipewire, wireplumber, pipewire-pulse
            ├─ (background) wait for the Wayland socket → portal stack
            ├─ (background) wait for the Wayland socket → kdos-ime, fcitx5
            ├─ (background) kdos_session_once → login sound, first-run card,
            │                 mpd, pending-apps offer, timers, session restore
            └─ kdos-comp          in the FOREGROUND, so its exit is caught
```

**The real-time limits are set before the login, not by it.** Resource limits survive `setuid()`
and `execve()`, so `kdos-getty`, which starts the login as root on tty1 and tty2, raises them for
everything below it: `RLIMIT_RTPRIO` to 95, `RLIMIT_NICE` to 39 (a nice value of −19) and
`RLIMIT_MEMLOCK` to 4 MiB. Without them PipeWire's real-time module cannot promote its threads,
they stay at an ordinary priority, and a missed audio cycle is heard as a gap. Nothing later on
the path lowers them, and nothing would raise them instead: shadow is built without PAM, so no
`limits.d` file is ever read, and the image ships no RTKit. A login on the serial console does
not pass through `kdos-getty` and does not get these limits. A limit the kernel refuses is
reported and the login continues.

**Resource limits for boxes depend on how the session started.** `kdos-getty` places an
autologin session in `/sys/fs/cgroup/user.slice/user-<uid>/session`, the cgroup delegated to
that user, so the `memory =` and `cpus =` keys of a pack box's profile become real limits:
rootless podman creates each container's cgroup as a sibling of the caller's. A session started
from a password prompt or over ssh stays in the root cgroup, where podman accepts those limits and
does not enforce them. A box built from an OCI image is given neither limit, whichever way the
session started ([Containers](security-model.md#containers)).

**Native applications take their toolkit settings from the login shell.**
`/etc/profile.d/10-wayland.sh` is read by that shell, so everything the session starts inherits what
it exports:

| Variable | Value, and what it does |
|---|---|
| `QT_QPA_PLATFORM` | `wayland;xcb`: Qt uses Wayland and falls back to Xwayland only when its Wayland plugin cannot start |
| `GDK_BACKEND` | Not set. GDK tries Wayland first on its own, and an X11-only application that sets `GDK_BACKEND=x11` for itself is not overridden |
| `GTK_USE_PORTAL` | `1`: a GTK application opens `kdos-pick` through the FileChooser portal and prints through the Print portal ([Portals](#portals)) |
| `QT_QPA_PLATFORMTHEME` | `kde`: plasma-integration's Qt 6 platform theme, which reads the `~/.config/kdeglobals` that `kdos theme` writes. Qt reads one name here, and a Qt 5 application finds the `qt5ct` port's plugin under it, which also answers `kde` and reads the qt5ct files `kdos theme` writes |
| `_JAVA_AWT_WM_NONREPARENTING` | `1`: Swing and AWT run under Xwayland and otherwise draw blank or misplaced windows. `kdos-comp` sets the same default for what it starts |
| `MOZ_ENABLE_WAYLAND` | `1`: Firefox ESR, LibreWolf and Thunderbird use their Wayland backend |
| `SDL_VIDEODRIVER` | `wayland`: SDL applications open a Wayland window rather than an X11 one |
| `CLUTTER_BACKEND` | `wayland`: Clutter-based applications use Wayland |
| `XCURSOR_THEME`, `XCURSOR_SIZE` | `KDOS-cursors` at `24`: the pointer an application draws over its own window matches the compositor's |

A box receives none of these; `kdos-appbox` states a box's environment itself ([the environment a
box receives](#the-environment-a-box-receives)).

### The shared bring-up: `session-common.sh`

Both start scripts source `/usr/local/lib/kdos/session-common.sh`. It is sourced, never
executed, and it defines one function per job:

| Function | Called by | Sets up |
|---|---|---|
| `kdos_session_open` | `kdos-desktop-start` | `$BROWSER` (default `xdg-open`), for a start that read no profile |
| `kdos_session_runtime` | `kdos-desktop` | `XDG_RUNTIME_DIR` (default `/run/user/<uid>`), before anything uses it |
| `kdos_session_keymap` | `kdos-desktop` | The console keymap from `/etc/keymap`, translated into `XKB_DEFAULT_LAYOUT` / `XKB_DEFAULT_VARIANT` |
| `kdos_session_boxes` | `kdos-desktop` | Starts `kdos-appbox warmup` at `nice 10`, and runs `kdos-box gc` every ten minutes to stop boxes that have run longer than their profile's `autostop` and have no window open |
| `kdos_session_bus` | `kdos-desktop` | One [session bus](#the-session-message-bus) per user, at a fixed path |
| `kdos_session_audio` | `kdos-desktop-start` | [PipeWire](#audio), once per user rather than once per session |
| `kdos_session_once` | `kdos-desktop-start` | [Everything done once the display exists](#what-runs-once-after-the-display-exists) |

Each block carries a constraint that is invisible from where it is called, so the file exists
once and both scripts source it. A second copy would be a second place for one of those
constraints to go missing.

`$BROWSER` is set here as well as in `/etc/profile.d/30-open.sh` because the portal startup below
pushes it into the bus's activation environment, and pushing an unset name pushes nothing: a
session started without a profile would leave every link clicked in a box with no handler. A
value already exported is kept.

The warmup runs at `nice 10` rather than 19: at 19 it loses every CPU slice to the starting
desktop and is still mid-initialisation minutes later, and a launch that lands in that window
waits for it.

The keymap is translated because the console and XKB name layouts differently:

| Console keymap in `/etc/keymap` | XKB layout (variant) |
|---|---|
| `us` | `us` |
| `uk` | `gb` |
| `de*`, `fr*`, `es*`, `it*`, `br*`, `ru*` | `de`, `fr`, `es`, `it`, `br`, `ru` |
| `sg*` | `ch` |
| `slovene` | `si` |
| `croat` | `hr` |
| `la-latin1` | `latam` |
| `dvorak*` | `us` (variant `dvorak`) |
| anything else | its first two letters |

When `/usr/share/X11/xkb/symbols` is present, a derived layout is exported only if
`/usr/share/X11/xkb/symbols/<layout>` exists; otherwise the script prints a warning and leaves the
default. A layout xkbcommon cannot compile makes the compositor fall back to US for the whole
session, [lock-screen](../04-programs/kdos-comp.md#idle-dim-lock-and-lid) password prompt
included. Where that directory is absent the layout is exported unchecked.

### What `kdos-desktop` does

1. Refuses at once if `/usr/bin/kdos-comp` is missing, naming the build phase (`05_desktop`) it
   comes from.
2. On a virtio GPU (a `card0` whose modalias is `virtio:*` or PCI vendor `1AF4`), sets
   `WLR_NO_HARDWARE_CURSORS=1`, because the virtual cursor plane misreports what it supports and
   the pointer would be invisible or leave trails. When no virtio GPU device has the feature bit
   for virgl (the virtio GPU's 3D acceleration), it also sets `WLR_RENDERER=pixman`, since
   without virgl there is no GL at all. It reads the feature bit from
   `/sys/bus/virtio/devices/*/features` rather than from the kernel log: the log is root-only
   under `dmesg_restrict`, so reading it would force software rendering for every ordinary user.
3. Runs `kdos_session_runtime`, `kdos_session_keymap`, `kdos_session_boxes` and
   `kdos_session_bus`, then executes `kdos-desktop-start`.

The compositor talks to `seatd` through libseat. `seatd` is a small daemon that hands the
compositor access to the display and input devices, the role logind plays on other distributions;
there is no logind or elogind on the host. How this compares with a systemd-based desktop is in
[how KDOS differs](../01-philosophy/how-kdos-differs.md#init-and-service-supervision).

### What `kdos-desktop-start` does

1. Exports `XDG_CURRENT_DESKTOP=KDOS`, over whatever the login shell set (the variable is a list,
   most specific first). The portal front end and the `mimeapps.list` search both key on the
   first name in that variable, so it has to be the session's own answer.
2. Runs `kdos_session_open` and `kdos_session_audio`.
3. Starts the portal stack and the input method in background subshells that first wait (up to
   30 seconds) for a `wayland-*` socket to appear in `$XDG_RUNTIME_DIR`.
4. Starts `kdos_session_once` in the background.
5. Runs `kdos-comp` in the foreground, with its standard error written to
   `$XDG_RUNTIME_DIR/kdos-comp.log` (the previous session's log is kept as `kdos-comp.log.old`)
   and shown on the terminal as well.

The compositor has start-up hooks of its own, inherited from labwc, the compositor `kdos-comp`
is forked from: it reads an optional `environment` file and runs optional `autostart` and
`shutdown` scripts from `~/.config/kdos-comp/` (falling back to `/etc/xdg/kdos-comp/`). When it
drives the display hardware directly (DRM, rather than running nested in another display), it
also pushes `WAYLAND_DISPLAY`, `XDG_CURRENT_DESKTOP`, `XDG_SESSION_TYPE`, the cursor variables
and `DISPLAY` into the bus's activation environment. That push is the compositor's own and is
separate from the one the start script makes (see [startup ordering](#startup-ordering)); both
happen. The image ships none of those three files; see [kdos-comp](../04-programs/kdos-comp.md).

When the compositor exits:

- **Status 0 is a logout.** The menu's Log Out, which runs the compositor's `Exit` action, ends
  this way. The script exits and leaves a shell prompt on tty1, still logged in:
  `~/.bash_profile` (or `~/.zprofile`) runs `kdos-desktop` as an ordinary command rather than
  with `exec`, so a session that fails to come up also leaves a working shell. Typing `exit` there
  ends the login; tty1 then gets a fresh login, and with autologin on that logs straight back in
  and starts the desktop again.
- **Any other status is a crash.** The script prints the last 20 lines of the log and asks
  `r=restart session, anything else=exit to tty`. Answering `r` re-executes the script, which
  starts the portal stack again (the screen-capture backend died with the compositor) but not a
  second PipeWire.
- **Three crashes within 60 seconds** stop the offer, because a crash loop that fast scrolls the
  log away before it can be read. The count is carried across the re-exec in `KDOS_COMP_FAIL_T0`
  and `KDOS_COMP_FAIL_N`.

The log goes to a file followed by `tail -f`, never through a pipe. Every descendant of the
compositor inherits its standard error, and a pipe's reader sees end-of-file only when the last
writer closes, so one long-lived grandchild, such as the `conmon` that lives as long as a boxed
application, would hold the pipe open indefinitely, and tty1 would stay blank with no error
printed and nothing logged.

### What runs once, after the display exists

`kdos_session_once` takes a readiness test as its argument: a command that blocks until the
display exists and exports whatever the children need to reach it (for `kdos-desktop-start`,
waiting for the compositor's socket and exporting `WAYLAND_DISPLAY`). The test belongs to the
caller because the file is sourced before anything has a display to test. Then, in one
background subshell so that none of it delays the desktop:

| Step | Condition | Notes |
|---|---|---|
| The login sound (`kdos-sfx login`) | `kdos-sfx` installed | Silent on failure |
| The keybinding card (`kdos-keys --first-run`) | `~/.config/kdos/first-run` does not exist | `kdos-keys` writes the marker once the card has been seen; delete it to see the card again |
| `mpd --no-daemon` and `kdos-mpctl watch` | `mpd` installed and a readable config in `~/.config/mpd/mpd.conf`, `~/.mpdconf`, `~/.mpd/mpd.conf` or `/etc/mpd.conf` | The image ships no mpd config, so this is opt-in. mpd stays a child of the login and ends with it; one already running is left alone. The watcher feeds the panel's media cell |
| The pending-applications offer | See [below](#applications-chosen-during-installation) | |
| The per-user timers | `~/.config/kdos/timers.d/*.timer` and `snooze` | See below |
| Session restore | `~/.config/kdos/session-restore` exists | Relaunches the `app` lines of `${XDG_STATE_HOME:-~/.local/state}/kdos/session` with `kdos-appbox run`, two seconds apart |

**Per-user timers.** Each non-comment line of a `*.timer` file is `NAME TIMESPEC... -- COMMAND...`,
the same format as the system table in `/etc/kdos/timers.d`. `snooze` is a small scheduler that
waits until a time matching its options and then runs a command; `TIMESPEC` is those options,
and the command is an argument vector, not a shell line, so `$HOME` written on the line is four
literal characters. A line whose schedule `snooze -n` rejects, or whose command is not
installed, is skipped. Each line runs in a loop that waits for its slot with `snooze`, runs the
command, and waits again, so it repeats until the loop is stopped; a one-second floor under the
loop stops a command that fails instantly from spinning it. The loops are children of this
login, not of a root service. Their process ids are written to
`$XDG_RUNTIME_DIR/kdos/timers.pid`, and the next session start on the same boot stops the
previous set (each loop's `snooze` child first) before starting its own. Nothing stops them when
the compositor exits, so they keep running at the tty1 prompt a logout leaves.

Two tables are shipped in `/etc/skel`: `20-updatedb.timer` rebuilds the `plocate` index of the
home directory nightly at 03:05, and `10-backup.timer` holds a nightly `kdos-backup --once` line
that is commented out until a backup repository is configured.

**Session restore** relaunches only boxed applications; a host program costs no container start.
`kdos-session-save` writes the file when the session is ended from the compositor's root menu (Log
Out, Restart, Shut Down) or with Super+Escape, before the confirmation prompt, because once the
compositor has exited there is nobody left to ask. The panel's own Log Out, Restart and Shut Down
rows do not run it. Where each window reappears is the compositor's window memory, not this file.

### Applications chosen during installation

Applications ticked in the installer are *offered* at first login, never built in the
background. When `kinstall` can neither import an exported set nor build the applications over
the network (or a network build fails), it writes the chosen group ids to
`/var/lib/kdos/apps-pending`. The first session then sends a notification ("N chosen during
installation are ready to install — open the store") and starts nothing; the store is the
panel's application window, [`kdos-store`](../04-programs/kdos-shell.md#kdos-store). Building
means podman and apt, which can take twenty minutes or more on a machine booted for the first
time, and a build that long with nothing on screen leaves the machine busy with no visible cause.
The panel's update window, `kdos-update` (see
[kdos-shell](../04-programs/kdos-shell.md#the-system-surfaces)), follows the same rule for
`kdos update apply`: it says what to type and never starts the build itself.

| File | Owner | Meaning |
|---|---|---|
| `/var/lib/kdos/apps-pending` | root | What is still wanted. `kdos app install --pending` tries to delete it after a clean run, but the command runs as the user and the file is root's in a root-owned directory, so the deletion fails and the file stays until root removes it; a partial run forgets nothing either way |
| `~/.config/kdos/apps-offered` | the user | The offer has been made. Delete it and the offer comes back at the next login |

The notification is sent only once `org.freedesktop.Notifications` has an owner (the script
waits up to 20 seconds), and the marker is written only after the send. Nothing activates that
bus name on demand, so a call made the moment the display appears would reach a notifier that is
still starting and be lost; a session that never gets a notifier keeps the offer for the next
login. The wait runs in its own subshell, so session restore does not wait behind it.

## The session message bus

One `dbus-daemon` per user, listening at a fixed path: `unix:path=$XDG_RUNTIME_DIR/bus`. A
session restart reuses the running daemon, found by a `Ping` to the bus.

It is not started with `dbus-run-session`, which listens on a socket in the host's `/tmp`. Boxes
see the runtime directory at the same path as the host, so one fixed address in
`$XDG_RUNTIME_DIR` is valid on both sides. Without a reachable bus, a boxed application's
single-instance negotiation fails (every impatient re-click starts another full instance),
settings and accessibility probes stall, and notifications go nowhere.
`/etc/profile.d/10-wayland.sh` exports the same address for any login shell when the socket
exists, so a shell on tty2 or over ssh reaches the running session's bus.

Three details of the startup matter:

- **The address carries no GUID.** A later session rebinding the socket would otherwise abort
  every client of a strict D-Bus implementation (zbus, for one) with a server-identity mismatch.
- **The start is serialised with a lock** (`$XDG_RUNTIME_DIR/.kdos-bus.lock`, taken with
  `flock`), so two sessions starting at once cannot both decide the daemon is down, with the
  loser removing the winner's socket.
- **The lock is closed for the daemon** (`7>&-`). `dbus-daemon --fork` inherits open descriptors
  and outlives the subshell, so without that it would hold the lock until logout, and the
  *second* login of a boot would block before starting the compositor.

## Audio

PipeWire runs on the host, started by `kdos_session_audio` from `kdos-desktop-start`, once per
user: a restart finds it running and starts nothing, because two PipeWire daemons would fight
over the same devices. Three programs, in this order:

1. `pipewire`, the media graph;
2. `wireplumber`, its session manager, which connects to the daemon's socket;
3. `pipewire-pulse`, the PulseAudio compatibility layer, which announces sinks that exist only
   once WirePlumber has created them.

The compositor plays no part in audio: a login sound and a boxed application's audio go through
the same PipeWire and neither goes through `kdos-comp`.

PipeWire is built with no session manager of its own (`-Dsession-managers=[]`). The daemon
routes nothing by itself: device discovery, which sink a stream lands on and a Bluetooth
headset's profile are all policy, and policy is WirePlumber's. `kdos-oomd` names `wireplumber`
explicitly in the processes it protects, because the `pipewire` prefix that covers `pipewire` and
`pipewire-pulse` does not match it, and killing it leaves a running graph with nothing
connected.

**Bluetooth codecs are enabled by name**, not left to auto-detection: aptX, LDAC, AAC, LC3, Opus
and G.722, linked from `libfreeaptx`, `ldacbt`, `fdk-aac`, `liblc3` and `opus` (all in the port's
`depends`). SBC is mandatory in the profile and always present. Each codec is a meson feature
that silently disables itself when its library is missing, and a headset then falls back to SBC
with nothing saying why.

**LE Audio needs the Bluetooth daemon's half too.** bluez offers the LC3 endpoints of the Basic
Audio Profile (BAP, LE Audio's streaming profile) only when its experimental D-Bus interfaces and
the kernel's ISO socket feature (the isochronous channels LE Audio streams over) are both on, so
`/etc/init.d/60_bluetooth.sh` starts
`bluetoothd -n -E --kernel=6fbaf188-05e0-496a-9885-d6ddfdb4e03e`. Without them an LE Audio
headset or hearing aid falls back to A2DP (classic stereo playback) and HFP (classic headset
calls), or does not connect. They are command-line flags rather than a `main.conf`, so the file
the bluez package owns is not replaced.

### Reaching the graph from inside a box

A boxed application reaches the host's PipeWire through the shared `$XDG_RUNTIME_DIR`: a program
that speaks PulseAudio finds the `pipewire-pulse` socket at `/run/user/<uid>/pulse` through the
base pack's `libpulse0`.

A plain ALSA program in a pack box (a box composed from packs; see
[packs and boxes](packs-and-boxes.md)) needs one more file. `kdos-boxinit`, a pack box's first
process, writes `/etc/asound.conf` pointing ALSA's default PCM and mixer at PulseAudio
(`pcm.!default { type pulse }`, `ctl.!default { type pulse }`), using `libasound2-plugins` from
the Debian base. It writes the file only when that plugin is present, so a base without it
(Alpine) keeps ALSA's own default. A store-lane box, created from a container image by
`distrobox`, has the same base packages but no `kdos-boxinit`, and so no redirect file.

Without the redirect, `default` inside the box is alsa-lib's built-in `plug → softvol → dmix`
chain, where dmix is alsa-lib's software mixer. Because `/dev` is shared and the host `audio`
group survives the box's user namespace, that open succeeds and takes the sound card: the box has
sound and the rest of the machine has none until the box stops. The redirect moves `default`
only; `hw:0` and `sysdefault` still name the hardware.

### Reaching the graph from a plain ALSA program on the host

A plain ALSA program on the host reaches PipeWire through one file,
`/etc/alsa/conf.d/99-kdos-pipewire.conf`.
`alsa.conf`'s `@hooks` read `/var/lib/alsa/conf.d`, `/usr/etc/alsa/conf.d`, `/etc/alsa/conf.d`,
`/etc/asound.conf` and `~/.asoundrc`, and not `/usr/share/alsa/alsa.conf.d`, where PipeWire
installs its own drop-ins. Without a file in a directory that is actually read, every ALSA
program talks to the card directly, the first one to open it owns it, and PipeWire runs with no
device. The resulting stutter is hard to diagnose: a dmix client's underrun happens in userspace,
so it shows no xrun on the card and nothing in `dmesg`.

The file defines two devices and a default:

| Name | Is | Shown as |
|---|---|---|
| `kdos_pipewire` | PipeWire | "Default Audio Device (PipeWire)" |
| `kdos_card` | `plug` onto `sysdefault`, the card directly | "Sound Card (no sound server)" |
| `pcm.!default` | Whichever PCM `$KDOS_ALSA_DEFAULT` names; `kdos_pipewire` when unset | |

A login that runs no session script has no PipeWire: a tty2 login, a serial console and an ssh
login all lack one, and nothing starts one for them. There, run a program with
`KDOS_ALSA_DEFAULT=kdos_card` to play straight to the card:

```sh
KDOS_ALSA_DEFAULT=kdos_card kdos-bb
```

An undefined name fails with "Unknown PCM", and an absent PipeWire with "Host is down"; neither
failure is silent. `kdos_card` is exclusive in both directions: while it holds the card PipeWire
cannot open it, and the other way round. The mixer control (`ctl.default`) is left on the
hardware, so the panel's volume control keeps working when PipeWire is what went wrong.

Every redefinition in that file carries its `!`. The hooks run after the rest of `alsa.conf` is
parsed, so a plain `pcm.default { … }` aborts the whole configuration load and leaves every ALSA
program with no configuration at all. Check any edit with `aplay -L`.

### The quantum, and why a virtual machine gets a bigger one

The *quantum* is how many audio frames PipeWire processes per cycle; it sets the latency. Under a
hypervisor the card gets a deeper buffer, because an emulated card has no clock of its own: the
emulated codec advances its DMA position from the emulator's main loop, the same loop that
updates the display, so the position moves in bursts. PipeWire keeps about one quantum in the
device and times its wake-ups from that position, so a wake-up later than one quantum is silence.
The underrun belongs to the *player*, not the card, and nothing appears in `dmesg`.

`/etc/pipewire/pipewire.conf.d/99-kdos-vm.conf` applies only when `cpu.vm.name` is set, which
PipeWire does only under a hypervisor:

| Setting | Value |
|---|---|
| `default.clock.quantum` | 4096 (85 ms at 48 kHz) |
| `default.clock.min-quantum` | 4096 |
| `default.clock.max-quantum` | 8192 |

PipeWire's own configuration already raises the floor to 1024 frames in a virtual machine, which
suffices for a device whose clock is steady. Measured on this image against a 1920×1080
accelerated display, the card held at least 42 ms of audio and typically 64 ms at that floor, and
at least 85 ms and typically 127 ms at 4096 frames. A host that is still late needs 8192 frames,
170 ms, the ceiling the rule allows. A machine on real hardware does not match the rule and keeps
PipeWire's defaults: a 1024-frame quantum, about 21 ms at 48 kHz. `pw-metadata -n settings`
shows which is in force, and `pw-top`'s `ERR` column counts underruns.

The quantum is also the granularity of every music clock on the machine: a program that follows
a song position sees it advance in quantum-sized steps, not smoothly, and has to tolerate one
quantum of flat step rather than chase it. [kdos-bb](../04-programs/kdos-bb.md#the-servo) has the
measured staircase and the correction that copes with it.

## Portals

A *portal* is how a sandboxed application asks the host to do something it cannot do itself:
open a file chooser, share the screen, open a link. The front-end daemon,
`/usr/lib/xdg-desktop-portal` (port `xdg-desktop-portal`, 1.22.1), owns the public D-Bus
interfaces; *backends* implement them.

`/usr/share/xdg-desktop-portal/kdos-portals.conf` chooses the backends. The front end looks for
`<desktop>-portals.conf` using the lowercased first name in `XDG_CURRENT_DESKTOP`, so `KDOS`
gives `kdos-portals.conf`:

```ini
[preferred]
default=none
org.freedesktop.impl.portal.ScreenCast=wlr
org.freedesktop.impl.portal.Screenshot=wlr
org.freedesktop.impl.portal.FileChooser=kdos
org.freedesktop.impl.portal.Settings=kdos
org.freedesktop.impl.portal.Print=gtk
org.freedesktop.impl.portal.Email=gtk
org.freedesktop.impl.portal.AppChooser=kdos
org.freedesktop.impl.portal.Access=kdos
```

| Portal | Backend |
|---|---|
| ScreenCast, Screenshot | `xdg-desktop-portal-wlr` (0.8.4) |
| FileChooser, Settings, AppChooser, Access | `xdg-desktop-portal-kdos` |
| OpenURI | The front end itself, using AppChooser |
| Camera, Location | The front end itself, gated by Access |
| Print, Email | `xdg-desktop-portal-gtk` (1.15.3) |
| Everything else (Wallpaper, Inhibit, Notification, …) | None |

Without this file the front end would fall back on each backend's own `UseIn=` line. The KDOS
backend's names `KDOS`, but `xdg-desktop-portal-wlr`'s names other compositors, so screen capture
would have no backend.

Each interface names exactly one backend. A list such as `wlr;kdos` would not fall through: a
backend that D-Bus can activate always counts as available, so the front end would pick `wlr` and
fail.

`default=none` is deliberate: an interface the file does not name has no backend, and a portal
that could only ever fail is not advertised. `xdg-desktop-portal-gtk` is on the host, but it is
the backend other desktops fall back to, and as the default it would put GTK's own file chooser
and access dialog in front of the KDOS ones. It is named for two interfaces only.
**Print** is the print dialog an application with `GTK_USE_PORTAL=1` routes through the portal,
drawn by the GTK backend and sent to CUPS. **Email** is the "compose a mail with this attached"
request; the backend hands it to the `x-scheme-handler/mailto` default in
[`mimeapps.list`](../06-reference/configuration.md). Wallpaper, Inhibit, Notification and the
rest have no backend. A boxed application can also print through its own print dialog and the
shared CUPS socket (see [the environment a box receives](#the-environment-a-box-receives)).

`kdos-desktop-start` sets `XDG_CURRENT_DESKTOP` for a graphical session (see [what
`kdos-desktop-start` does](#what-kdos-desktop-start-does)). `/etc/profile.d/10-wayland.sh` fills
it in (`KDOS`) only when it is unset, which is what a login on tty2, a serial line or over ssh
gets.

`OpenURI` ("open this on the host for me") is what a boxed application asks when a link or a
downloaded file is clicked. The front end resolves the host's handlers itself and launches the
chosen one from the host's own desktop entries, so a link in a boxed application opens in the
browser this desktop generated an entry for. It exports the interface only when a backend
answers `AppChooser`, which is why the KDOS backend implements AppChooser rather than an
`impl.portal.OpenURI` interface the specification does not have. Inside a pack box (a box
composed from packs; see the [glossary](../06-reference/glossary.md)),
`kdos-boxinit` writes `/usr/local/bin/xdg-open` (with `x-www-browser` and `sensible-browser`
linked to it), a script that sends the URI to that portal with `gdbus`; a relative or absolute
path is turned into a `file://` URI first, which resolves on the host because the home directory
and `/tmp` are the same directories on both sides.

### Startup ordering

The front end reads its list of backends once, when it starts, and `xdg-desktop-portal-wlr` is a
Wayland client that needs the compositor. The bus's activation environment is set before the
compositor starts and has no `WAYLAND_DISPLAY`, so a backend started by D-Bus activation would
die at once. A background subshell of `kdos-desktop-start` therefore does five things in order:

1. Waits for the compositor's socket to exist.
2. Pushes `WAYLAND_DISPLAY`, `XDG_CURRENT_DESKTOP`, `XDG_SESSION_TYPE`, `XCURSOR_THEME`,
   `XCURSOR_SIZE` and `BROWSER` into the bus's activation environment with
   `dbus-update-activation-environment`.
3. Starts the capture backend, `/usr/lib/xdg-desktop-portal-wlr`.
4. Waits (up to 20 seconds) for it to own `org.freedesktop.impl.portal.desktop.wlr`.
5. Stops any front end that is already running, and starts `/usr/lib/xdg-desktop-portal`.

If the front end starts before the capture backend owns its name, screen capture is unavailable
for the whole session (OBS reports "No capture sources available").

### The KDOS backend

`xdg-desktop-portal-kdos` (source `src/desktop/xdg-desktop-portal-kdos`, installed at
`/usr/lib/xdg-desktop-portal-kdos`, started by D-Bus as `org.freedesktop.impl.portal.desktop.kdos`)
serves FileChooser, Settings, AppChooser and Access. It does not implement ScreenCast.

It is a bus adapter and nothing more. For a file choice it runs `kdos-pick` and reads its output;
for an Access question it runs `kdos-prompt` and reads its exit status. The choosers stay
ordinary programs that can be run by hand, scripted or replaced. The backend does no permission
checking of its own; that is the front end's job.

**AppChooser draws no dialog.** The front end passes the handlers it found for the file or URI,
best first, and the previous choice when there is one; the backend answers with the previous
choice if it is still in the list, and otherwise with the first. Choosing among handlers
interactively is `kdos-openwith`, which the desktop's "open with" already runs. When the front
end found no handler at all, the backend opens the item itself, with `kdos-appbox open` for a
local file and `xdg-open` for any other URI, and answers "cancelled", since nothing was chosen.

Five further properties of the backend, each of which is a common way for a portal to break:

**Every request is answered.** Response 0 is success, 1 is cancelled, 2 is an error. A request
that is never answered leaves the application blocked with a half-drawn window.

**The bus loop never blocks on a dialog.** The handler starts the chooser, keeps the request,
returns without replying, and adds the chooser's output pipe to its main loop; the reply is built
when the chooser exits. Otherwise a second application's Open would queue behind the first, and a
boxed application asking Settings for the colour scheme, which happens on every launch, would
hang until the dialog was dismissed.

**The dialog is placed over its application.** An application sends `parent_window` as
`wayland:<handle>`, where the handle comes from xdg-foreign, the Wayland protocol that lets one
client name another's window; the backend strips the scheme and passes the handle to
`kdos-pick --parent`, which asks the compositor to centre the dialog on its parent. An `x11:`
handle, or anything with no scheme, is dropped. The chooser is always run from an argument
vector, because filter patterns and file names arrive from other applications. Only the first
filter a request carries is applied, because `kdos-pick` takes one pattern list.

**Settings answers two namespaces, and a boxed application needs both.**

| Namespace | Key | Value |
|---|---|---|
| `org.freedesktop.appearance` | `color-scheme` | 1, "prefer dark", whatever the accent, including the light `paper` scheme |
| | `accent-color` | The current accent's primary colour, as three doubles; libadwaita reads it |
| | `contrast` | 0, "no preference": KDOS has no high-contrast switch, and every accent already holds the contrast floors in [Accessibility](../02-user-guide/accessibility.md#colour-and-contrast) |
| `org.gnome.desktop.interface` | `gtk-theme` | `KDOS-<accent>` |
| | `icon-theme` | `KDOS` |
| | `cursor-theme` | `KDOS-cursors` |
| | `cursor-size` | 24, the size `/etc/profile.d/10-wayland.sh` exports as `XCURSOR_SIZE` |
| | `color-scheme` | `prefer-dark`, whatever the accent |
| | `font-name` | `Noto Sans 10` |
| | `monospace-font-name` | `Noto Sans Mono 10` |
| | `gtk-im-module` | Empty: GTK 4 picks its Wayland text-input context, the one fcitx5 and the on-screen keyboard reach. Left out, GTK 4 falls back to `simple`, which reaches no input method |

The `org.gnome.desktop.interface` keys are GSettings key names, which GTK maps onto its own
`gtk-theme-name` and `gtk-icon-theme-name` properties. For any key the portal does not send, GTK
applies its built-in default (Adwaita, and a 24 or 32 pixel cursor) over `settings.ini`, so the
portal sends every key the KDOS theme depends on. The two fonts are the faces fontconfig already
puts first for `sans-serif` and `monospace`, so naming them changes no glyph; left unnamed, GTK asks
for its schema default, Cantarell, which KDOS does not ship. `kdos theme` writes the same two names
into `kdeglobals` and the qt5ct and qt6ct files, so GTK and Qt draw in the same faces.

A GTK application that does not read the portal reads GSettings instead, and once
`gsettings-desktop-schemas` is installed an unset key there is the schema's default, Adwaita and
Cantarell. `/usr/share/glib-2.0/schemas/90_kdos.gschema.override` sets the defaults to the
portal's answers for the same keys, with the fixed theme name `KDOS`, the link `kdos theme` points
at the accent in force. A value set with `gsettings set` is stored in the user's dconf database and
wins over it. An application reading GSettings does not restyle on an accent switch, which only the
portal announces. The override reaches GTK once `glib-compile-schemas` has read it into
`gschemas.compiled`: kpkg rebuilds that index whenever a package installs into the directory, and
`06_packaging/00_theme.sh` rebuilds it again for the image.

The accent is read from `${XDG_CACHE_HOME:-~/.cache}/kdos/theme`, the same one-word file the
panel and the compositor read. A missing file or an unknown name means the first scheme in the
table, `phosphor`, which is not the desktop's default accent, `bone`. The image ships no such
file, so on the live system the backend tells boxed applications `KDOS-phosphor` and phosphor's
colour while the panel and the compositor draw bone, until `kdos theme` writes the file. The
installer runs `kdos theme` for the new user, so an installed system starts with the file in place.

The colour table is compiled from libkcolor's scheme list, so for any accent the file names, the
backend and the desktop agree on what that accent looks like. Because both `color-scheme` answers
are dark under every accent, a boxed GTK or libadwaita application draws dark even on the light
`paper` desktop.

The GNOME namespace is the only thing that restyles a GTK3 application that is already running. A
user stylesheet is loaded once at startup, so a palette written into it reaches the next launch
and never this one, while a theme *name* that changes makes GTK rebuild its whole style. That is
why `kdos theme` writes each accent's stylesheet to its own `~/.themes/KDOS-<accent>`.

**The backend watches the accent and emits `SettingChanged`.** A toolkit reads the settings once
and then waits for that signal; without it, an accent switched while a boxed editor is open would
reach only its next launch. The watch is on the *directory*, because the file is replaced rather
than edited. The signal is held back until `~/.themes/KDOS-<accent>/index.theme` exists:
`kdos theme --preview` writes the setting without generating a theme, and an application told the
name of a theme that is not there falls back to its own default.

### Access, and what it unlocks

The front end exports the Camera, Screenshot and Location portals only when a backend answers
`org.freedesktop.impl.portal.Access`, the grant-or-deny question each asks before handing an
application what it guards. The KDOS backend answers with `kdos-prompt`:

- the title and subtitle the front end sends become the question;
- its grant and deny labels (default "Grant Access" and "Deny Access") become the two buttons;
- the dialog opens with Deny selected, which is also the answer when nobody answers.

| `kdos-prompt` result | Portal response |
|---|---|
| Grant | 0 |
| Deny, or Escape | 1 (refused) |
| The prompt could not run | 2 (error) |

The front end's body text is not shown, because it points at a privacy page in a settings
application this desktop does not have.

Answers are kept in the front end's permission store, under `~/.local/share/flatpak/db/`, keyed
by application id, so each question is asked once. The front end recognises Flatpak, Snap and
Linyaps sandboxes; a KDOS box is none of them, so to the front end every box is the same host
program and one answer covers every box. To be asked again, delete the portal's file in that
directory.

Location comes from the system GeoClue service, `geoclue` (port `geoclue`). D-Bus starts it on the
system bus, as its own `geoclue` account, when the first client asks, and it exits after a minute
with no clients. Every source is built in: the Wi-Fi and 3G sources ask beaconDB, a public
database of Wi-Fi and cell-tower locations, for a position from what `wpa_supplicant` and
ModemManager see, the modem sources read ModemManager, the NMEA
source finds a phone's GPS feed through Avahi, the compass comes from `iio-sensor-proxy`, and
there are IP-address and static sources. It runs with no consent agent:
`/etc/geoclue/conf.d/90-kdos.conf` empties the agent whitelist and marks the Location portal a
system component, and the port's `no-agent.patch` treats an empty whitelist as leave to answer
every client at once. Upstream would hold every client until an agent registered, which on this
image would be never. So GeoClue answers whoever asks on the system bus. The front end asks Access
before Location only for a sandbox it recognises, which means a boxed application's position is
given without a question; see [known gaps](../06-reference/known-gaps.md#applications-and-boxes).

An icon or a sound an application hands a portal is decoded by
`xdg-desktop-portal-validate-icon` or `xdg-desktop-portal-validate-sound`, each of which re-runs
itself under `bwrap` with no network, no home directory and a read-only `/usr`, so an exploit in
an image or sound decoder lands in an empty namespace. `bwrap` is not setuid; it sandboxes
through an unprivileged user namespace.

### Recording

`kdos-record` records the screen to a file through the ScreenCast portal:

```sh
kdos-record                    # start: ~/Videos/YYYY-MM-DD-HHMMSS.mkv
kdos-record ~/clip.mkv         # start, to a named file
kdos-record                    # again, while recording: stop
```

It asks the portal for one monitor (`types` 1, `multiple` false), reads the PipeWire node it is
given, and runs `gst-launch-1.0 -e pipewiresrc ! videoconvert ! <encoder> ! matroskamux !
filesink`, with `x264enc` (H.264, `tune=zerolatency speed-preset=veryfast`) when that element
exists and `vp8enc` otherwise. It records video only. The `pipewiresrc` element exists because
the PipeWire port is built with `-Dgstreamer=enabled`.

**Choosing the screen is done by a person.** On this compositor the ScreenCast backend is
`xdg-desktop-portal-wlr`, whose chooser is `fuzzel`, configured in
`~/.config/xdg-desktop-portal-wlr/config` (seeded from `/etc/skel`) with `chooser_type=dmenu`: it
lists the screens and, when the request allows windows, the open windows, and waits for one to be
picked. The three portal calls therefore have different deadlines:

| Call | Deadline | Why |
|---|---|---|
| `CreateSession` | 5 s | A local bus answering in milliseconds |
| `SelectSources` | 120 s | Waiting for somebody to decide; seconds would cancel every recording before the question is answered |
| `Start` | 30 s | The backend binds the capture interface, takes a first frame to learn the format and registers a PipeWire node before it answers |

**Running it again stops it.** There is one screen and one portal session, so a second
invocation stops the first rather than starting another recording, and a key binding or menu
entry that started a recording must be able to end it. While recording, the program's process id
is in `$XDG_RUNTIME_DIR/kdos/screencast.pid`. A second invocation reads it, checks the process
still exists (a file left by a crash is not a recording), and sends it `SIGINT`, which it
forwards to the pipeline. `gst-launch-1.0 -e` turns an interrupt into end-of-stream so the
muxer writes its index; a `SIGTERM` straight to the pipeline would leave a file without one. The
handlers are installed without `SA_RESTART`, so the `waitpid` holding the recording open returns
with `EINTR` and the forward happens. The panel's recording lamp does not read this file: it
counts PipeWire ScreenCast nodes, so it lights for any screen share.

**A still screen still produces frames.** A recording that received no frames would be a file
with no header: `pipewiresrc` drops a chunk of size zero, so the muxer would never be handed
anything and the file would end empty with no error. The frames come from
`ext-image-copy-capture`, which asks the output for a frame whether or not anything changed.
`max_fps=30` in the same `xdg-desktop-portal-wlr` config bounds what that costs; without the
line, a still screen is captured and encoded at the output's own refresh rate for as long as the
recording runs.

**A portal session belongs to one connection**, which is why `kdos-record` is a program rather
than a script. The portal ties a session to the unique bus name that created it and closes it
when that name leaves the bus, so a shell script making three `gdbus call` invocations makes
three connections: the second is answered `Invalid session`, and the first session is already
gone. One connection must stay open for the whole recording.

**A portal reply is a signal, not a return value.** Every call returns a Request object path and
answers later with a `Response` signal on it, so the signal match is installed *before* the call;
a signal that arrives with no match is gone, and nothing replays it.

An application in a box that records sound as well (OBS, for example) takes its video through the
same ScreenCast portal and its audio from PipeWire through the PulseAudio socket in the shared
runtime directory.

## Supervised chrome

The compositor starts and supervises the desktop's own programs (its *chrome*) from a table in
`src/desktop/kdos-comp/src/kdos-child.c`, and restarts one when it exits. The mechanism (the
reasons each program is single-instance or per-output, the crash limit, the signal reset before
`exec`, what happens to a removed output's children and the table's capacity) is described once,
in [kdos-comp](../04-programs/kdos-comp.md#supervised-children). This section covers what the
session sees of it.

Which chrome a session gets is decided by `~/.config/kdos/comp.conf`:

| Program | One per output? | Runs when (`~/.config/kdos/comp.conf`) |
|---|---|---|
| `kdos-shell` (the panel) | yes | `panel` is not `off` (default `bottom`) |
| `kdos-desk` (desktop icons) | yes | `desktop_icons = yes` (default) |
| `kdos-slit` (the dockapp column) | yes | `slit = yes` (default `no`) |
| `kdos-notifyd` (notifications) | no | always |
| `kdos-netagent` (Wi-Fi passphrase prompts) | no | always |
| `kdos-mediad` (removable-media offers) | no | always |
| `kdos-clip` (clipboard history) | no | `clipboard = yes` (default) |
| `wvkbd-deskintl` (the on-screen keyboard, started hidden) | no | `osk = manual` or `auto` (default `off`) |

These switches are read once, when the compositor starts: a program switched off is not started by
a reconfigure, only by the next login. The compositor also passes the panel its edge, font, cell
height, clock format, margin, opacity and autohide setting from the same file, and `--no-icons` to
the panel and the desktop when `icons = no`, and `--hidden` to the on-screen keyboard, which it
then shows and hides by signal; the keys are listed in
[kdos-comp](../04-programs/kdos-comp.md).

The per-output programs are started with `--output <name>`, one process per screen, because a
layer surface (a panel-like surface the compositor anchors to a screen edge) is placed on one
screen and `libktui`, the toolkit every KDOS surface is drawn with (see
[the design language](design-language.md)), keeps a single buffer per process. When an output
appears, the compositor also runs `kdos-display --apply`, debounced by one second, to reassert the
saved layout in `~/.config/kdos/displays.conf`.

Each panel lists its own screen's windows. The window-management protocol reports which outputs a
window is on, and a panel filters its taskbar to the screen its surface actually entered, not the
one `--output` asked for, since a screen unplugged between the request and the mapping leaves the
panel wherever the compositor put it. Two bars listing the same windows would put the same
buttons on both screens. A server that gives no per-screen answer gets the unfiltered list,
because filtering on an answer nobody gave would empty the bar. Two things are not filtered:

- **the window menu**, because it is how a window on the other screen is reached;
- **the workspace pager**, because a workspace spans every screen, so occupancy is counted over
  every window. Otherwise a workspace would read as empty on the left screen while its windows
  were on the right.

A chrome program that keeps crashing is given up on and named in a dialog, because the log is
invisible from inside the session; the limit and how to reset it are in
[When a child keeps crashing](../04-programs/kdos-comp.md#when-a-child-keeps-crashing). Children
are reaped through the compositor's existing child-signal handling; because that signal
coalesces, the handler also checks each supervised child by its own process id, so two chrome
programs dying together do not leave one a zombie that is never restarted.

## Screen capture and the clipboard

The compositor offers both generations of each capture protocol (the older wlr screencopy and
export-dmabuf interfaces, and the newer `ext-image-copy-capture` with output and window sources,
which the capture portal backend prefers) and both generations of the data-control protocol that
clipboard tools use. Implementing only one generation would strand either the tools shipped here
or everything written after them.

A boxed application is kept off all of them. The compositor tags every client from a box with a
security context (through the per-box socket `kdos-boxsock` creates) and offers it a fixed
allowlist: surfaces, the seat, shared memory and dmabuf buffers, text input, the primary selection
and the ordinary clipboard, and the usual shell and decoration protocols. None of the capture,
data-control, input-method, layer-shell or output-management interfaces are on it. The full table
is in [the security model](security-model.md#sandboxed-clients).

That is what makes the portal the sanctioned route rather than a convenience: a boxed screen
recorder cannot bind the capture interfaces at all, so it must ask the portal, which runs on
the host and asks which output to share.

A screenshot of the desktop with the phosphor (CRT) effect on looks like what is on the screen.
Output capture copies the output's final buffer, which is the processed one. Per-window capture
renders the window's own contents, without the effect.

### Granting a box more than the allowlist

A box can be given specific protocols beyond the allowlist with a `grant` line in its profile,
`~/.config/kdos/boxes/<name>.conf`:

```ini
grant = screencopy, data-control
```

The eight grantable names, and the globals each unlocks, are:

| Name | Unlocks |
|---|---|
| `screencopy` | `zwlr_screencopy_manager_v1`, `ext_image_copy_capture_manager_v1`, `ext_output_image_capture_source_manager_v1` |
| `toplevel-capture` | `ext_foreign_toplevel_image_capture_source_manager_v1` |
| `export-dmabuf` | `zwlr_export_dmabuf_manager_v1` |
| `data-control` | `zwlr_data_control_manager_v1`, `ext_data_control_manager_v1` |
| `foreign-toplevel` | `zwlr_foreign_toplevel_manager_v1`, `ext_foreign_toplevel_list_v1` |
| `layer-shell` | `zwlr_layer_shell_v1` |
| `input-method` | `zwp_input_method_manager_v2`, `zwp_virtual_keyboard_manager_v1` |
| `output-power` | `zwlr_output_power_manager_v1` |

Any other name grants nothing. Where a protocol has two generations, the short name unlocks both.
Grants are read once per box, at the first bind that reaches the filter, and cached against the
box name (for up to eight boxes at once), since reading a profile on every bind would put a file
read in the compositor's hot path. Reconfiguring the compositor (`SIGHUP`) drops the cache, so an
edited profile applies to the next client; a running one keeps what it already bound.

`input-method` hands the box every keystroke on the seat. Why each name is dangerous is in
[the security model](security-model.md#granting-a-box-past-the-allowlist).

## Input methods

An *input method* turns keystrokes into text that a keyboard cannot type directly: Chinese,
Japanese or Korean, for example. Three parties are involved, and they never speak to each other
directly; the compositor is the wire between them:

```
fcitx5  ──input-method──▶  kdos-comp  ──text-input──▶  the application
      ◀──virtual-keyboard──                             (host or boxed)
```

The application's half of that, the text-input protocol, is inside the box allowlist, because
denying it would deny input methods to the applications that need one most. The engine's half is
outside it, because a client that can be an input method receives every keystroke on the seat.

The engine is `fcitx5`, with the Chinese (`fcitx5-chinese-addons`), Anthy (Japanese,
`fcitx5-anthy`) and Hangul (Korean, `fcitx5-hangul`) add-ons. It is built Wayland-only and
started by `kdos-desktop-start` as `fcitx5 -d`, not by an autostart entry, because KDOS runs no
autostart agent (the port is built `ENABLE_XDGAUTOSTART=Off`). If fcitx5 is not installed the
session starts without it and prints nothing. `kdos-desktop-start` starts Déjà Dup's backup
scheduler, `deja-dup-monitor`, the same way in place of its autostart entry, when Déjà Dup is
installed. The engines available for switching are set in
`~/.config/fcitx5/profile`, which the image seeds from `/etc/skel`.

The same text-input activation drives the on-screen keyboard: with `osk = auto`, the compositor
shows `wvkbd` while an application's text input is active and hides it when none is (see
[kdos-comp](../04-programs/kdos-comp.md#the-on-screen-keyboard)). An application is told about text
input only while an engine is connected, so without fcitx5 the keyboard is never raised on its own.

A boxed application reaches the engine through the compositor, never directly. Inside a box
`QT_IM_MODULE=wayland` is set; the GTK equivalent is deliberately not set at all, because GTK
on Wayland chooses the right route by itself when it is unset, and setting it is how a working
GTK application stops accepting input. Neither is ever set to `fcitx`: that is the X11-era route
where each toolkit talks to the engine directly, and inside a container that engine does not
exist.

**An X11 application has no input method**, on the host or in a box. Xwayland passes no text
input to its X clients, and the only route an X client has to an engine is XIM, which fcitx5
provides only when built with X11 support. The port is built `ENABLE_X11=Off`, which keeps
`xcb-imdkit`, `cairo-xcb`, `xkbfile` and seven further xcb components out of fcitx5's
dependencies; the cost is this missing route for X11 applications. The compositor starts
Xwayland rootless, when the first X11 client connects; see
[kdos-comp](../04-programs/kdos-comp.md#xwayland).

**Neither has a Qt 5 application.** Qt 5's Wayland plugin speaks text-input-unstable-v2 and the
compositor serves text-input-v3, so the route above covers GTK 3, GTK 4 and Qt 6 clients.
`QT_IM_MODULE` is never set to `fcitx` on the host, because that would take Qt 6 off
text-input-v3.

**The candidate window is drawn by KDOS.** `kdos-ime` owns the `org.kde.impanel` bus name, and
fcitx5's kimpanel module has a higher priority than fcitx5's own interface and takes over as soon
as that name appears. Starting `kdos-ime` is therefore all it takes to select it, and it starts
before fcitx5 so the engine sees it from the start rather than switching after the first
keystroke. Only one process can own that name, and the session bus outlives a session restart,
so `kdos-desktop-start` stops any running `kdos-ime` before starting its own, and again when the
compositor exits. See [kdos-ime](../04-programs/kdos-shell.md#kdos-ime).

## The environment a box receives

A command run in a container with `podman exec` inherits nothing: not the container's own
init environment and not the caller's. So `kdos-appbox` states every variable a launch needs:

| Variable | Value, and why |
|---|---|
| `PATH` | `/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin:/usr/games:/usr/local/games`. Without `/usr/games`, Debian's games are all "not found" |
| `HOME`, `USER`, `LOGNAME` | The user's own. Some toolkits crash with no user name |
| `LANG` | The host's when it is UTF-8, otherwise `C.UTF-8`. GTK and others refuse a non-UTF-8 locale |
| `XDG_RUNTIME_DIR` | `/run/user/<uid>`, shared with the host |
| `XDG_SESSION_TYPE` | `wayland` |
| `WAYLAND_DISPLAY` | The box's own tagged socket from `kdos-boxsock`, `$XDG_RUNTIME_DIR/kdos-box-<box>@<display>.sock`, as an absolute path. When the socket does not appear within a second the variable is not set |
| `DBUS_SESSION_BUS_ADDRESS` | `unix:path=/run/user/<uid>/bus`. Without it a single-instance application blocks with no window and no message |
| `XDG_CURRENT_DESKTOP` | `KDOS`, for portal and theme selection |
| `GTK_USE_PORTAL` | `1`. See below |
| *(no `GTK_THEME`)* | Deliberately absent: it overrides the theme setting for the life of the process, so an accent change could never reach a running GTK application. The theme name arrives through the Settings portal and the seeded `gtk-3.0/settings.ini` instead |
| `GSETTINGS_BACKEND` | `keyfile`, because no settings daemon is reachable |
| `QT_IM_MODULE` | `wayland`: input methods through the compositor |
| `QT_QPA_PLATFORMTHEME` and other per-runtime variables | Only the `env =` lines in the metadata of the pack named by the box's `pack:` base (or, for any other box, the installed pack whose id is the box's name) and of the packs it requires; see [packs and boxes](packs-and-boxes.md). A store box is normally named after an application no installed pack carries, so it gets none, and a Qt application in it runs with no platform theme |
| `CUPS_SERVER` | `/run/cups/cups.sock`, when that socket exists on the host |
| `NO_AT_BRIDGE=1`, `GTK_A11Y=none` | Accessibility off by default; see below |
| `DISPLAY` | The host's, or the first Xwayland socket in `/tmp/.X11-unix`, for X11-only applications |
| `LIBGL_ALWAYS_SOFTWARE` | `1` only when the box profile's `render` key resolves to software: a profile that refuses the GPU, a machine with no render node, or a box with no `/dev/dri` because `devices = private` meets `gpu = no`. Nothing is exported for hardware rendering, because Mesa already picks the right driver and falls back on its own |

**`GTK_USE_PORTAL=1` is what makes the KDOS portal reachable at all.** A GTK application routes
through the portal only when it believes it is sandboxed, which it decides from a marker file
(`/.flatpak-info`) or from this variable, and a container has neither. Without it, FileChooser
and Settings exist, answer, and are never called: every boxed application draws its own GTK file
dialog instead of `kdos-pick`. Firefox's default file-picker setting consults the same variable.
GTK's Print dialog also goes to the portal, which `xdg-desktop-portal-gtk` answers on the host;
the application's own print dialog, through `CUPS_SERVER`, works as well.

**Accessibility is a default, not a policy.** On the host, at-spi2-core's launcher starts the
accessibility bus on demand, by D-Bus activation of `org.a11y.Bus`, for the native applications and
Orca. `kdos-appbox` turns a boxed GTK application's accessibility bridge off by default, so the
application does not look for that bus when it starts. It is only a default, because a screen
reader running *inside* the box can reach the box's own registry once the bridge is on. To turn it
back on:

- for every launch, create `~/.config/kdos/a11y` (an empty file is enough);
- for one launch, set `KDOS_A11Y=1` (`KDOS_A11Y=0` forces it off).

## What is shared into a box

A box composed from packs (the import lane; see
[packs and boxes](packs-and-boxes.md#two-lanes-one-box)) receives the mounts in the table below. A
store-lane box is created by `distrobox create` and shares what distrobox shares, plus
`/run/cups` and, with `devices = private` and `gpu = yes`, `/dev/dri`.

| Host path | Inside the box |
|---|---|
| `$HOME` | Read-write, at the same path. With `home = private` in the profile the host home is not shared; `${XDG_DATA_HOME:-~/.local/share}/kdos/boxes/<box>` is bound at its own path instead, and `HOME` inside the box is still the host home path |
| `/tmp` | Read-write |
| `/run/user/<uid>` | Read-write: the session bus, the compositor socket, PipeWire |
| `/dev`, `/sys` | Read-write, unless the profile says `devices = private`; then only `/dev/dri`, and only with `gpu = yes` |
| `/dev/shm` | Read-write |
| `/run/cups` | Read-write, when `/run/cups/cups.sock` exists at creation time |
| `/var/lib/kdos/packs/mnt` | Read-only, when it exists, so links into mounted data packs resolve |
| `/usr/libexec/kdos/kdos-boxinit` | Read-only, as `/usr/libexec/kdos-boxinit`, the box's first process |
| `/run`, `/run/lock` | Private temporary filesystems |

**Sharing is fixed when the box is created.** A box created before the print service was running
does not have the CUPS socket until the box is created again, while `CUPS_SERVER` is decided at
every launch. The two halves are asymmetric on purpose: the variable probes again each time, and
a volume can only be added when the box is created. The same holds for every profile key that
changes a namespace or a volume (`home`, `devices`, `network`, `ipc`, `gpu`, and the rest).

To apply a changed key, remove the box with `kdos-box remove <box>`; the next launch (or
`kdos-box create <box>`) creates it again with the current profile and host state. Removing a box
keeps its profile, `~/.config/kdos/boxes/<box>.conf`, and its writable layer,
`~/.local/share/kdos/boxes/<box>/upper`, and never touches the home directory. On a live session,
whose home is on the boot overlay, the writable layer is in `$XDG_RUNTIME_DIR/kdos/boxes/<box>/`
instead and does not survive the session, because the kernel refuses an overlay upper directory
on overlayfs.

## Ending a session

Logging out ends the compositor, and with it the session. The compositor handles a termination
signal through its event loop and tears down in order:

1. closes the command socket behind [`kdos hey`](../04-programs/kdos-command.md#kdos-hey), so no
   command acts on a session that is already over;
2. cancels any [render-late](../04-programs/kdos-comp.md#render-late-scheduling) frame timer, then
   plays the CRT power-down animation (bounded by a deadline; skipped with `motion = no` in
   `comp.conf`);
3. stops the wallpaper, the [frames socket](../04-programs/kdos-comp.md#the-frames-socket),
   window-position memory, window groups, the
   [box chips](../04-programs/kdos-comp.md#box-identity), the lid handler, the peek view (which
   fades every window to show the desktop), the
   [fades](../04-programs/kdos-comp.md#motion) and the
   [idle policy](../04-programs/kdos-comp.md#idle-dim-lock-and-lid);
4. removes the phosphor pass;
5. runs the optional `shutdown` script and clears the variables it pushed into the bus's
   activation environment;
6. destroys the server.

The panel and the notification daemon notice the closed compositor socket themselves, because the
toolkit's event loop uses Wayland's documented prepare-read sequence and sees the socket close
rather than spinning on it. `kdos-desktop-start` then stops `kdos-ime` and, for a clean exit,
leaves a shell prompt on tty1 in the logged-in account. The session bus and PipeWire are left
running for the next session of the same user on this boot, which reuses them.

## See also

- [Architecture overview](overview.md) — where the session sits in the whole system
- [Boot and init](boot-and-init.md) — everything before the login prompt
- [kdos-comp](../04-programs/kdos-comp.md) — the compositor, its configuration and its start-up files
- [kdos-appbox](../04-programs/kdos-appbox.md) — launching boxes and box profiles
- [Packs and boxes](packs-and-boxes.md) — how a box is built and started
- [The security model](security-model.md) — the sandbox allowlist and what it denies
- [The desktop](../02-user-guide/desktop.md) — using what this chapter starts
- [How KDOS differs](../01-philosophy/how-kdos-differs.md#the-desktop) — how this session compares
  with the desktops of other distributions

<!-- book-nav -->
---

*Part III — Architecture, chapter 14.* Previous: [13. Boot and init](boot-and-init.md) · [Contents](../README.md) · Next: [15. Packaging](packaging.md)
