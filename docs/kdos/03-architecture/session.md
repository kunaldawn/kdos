# The session

This page explains what happens between logging in and seeing the desktop, and
what keeps running while you use it: the per-user message bus, the audio
graph, the portals, the compositor and the desktop programs it supervises,
screen capture, the clipboard, input methods, and the environment a
containerised application receives. It is for people who administer or debug
a KDOS desktop, and for anyone changing the session scripts or a program that
runs inside a session.

The session is where the host and a *box* (a containerised application; see
the [glossary](../06-reference/glossary.md)) meet, and most of this page is
about making that meeting work. If *box* and *pack* are new words, read the
[architecture overview](overview.md) first. For using the desktop rather than
understanding it, see [the desktop](../02-user-guide/desktop.md).

## Starting a session

On a default install, switching the machine on reaches the desktop without
anyone typing a command:

1. `kdos-getty` runs on tty1 (from `/etc/inittab`). It loads the console font
   and palette, sets the real-time limits the audio stack inherits, and, for an
   autologin account, moves itself into that user's delegated cgroup (below).
2. It runs `kdos-login tty1`, which reads `autologin = <user>` from
   `/etc/kdos/login.conf` (the shipped file says `autologin = kdos`) and starts
   `agetty --autologin <user>`. With no `autologin` line, or a missing or
   unreadable file, you get a login prompt instead.
3. The login shell reads `~/.bash_profile` (or `~/.zprofile` for an account
   whose shell is zsh, after `/etc/zsh/zprofile` has read `/etc/profile`). On
   tty1, when no Wayland display is set and `kdos-desktop` exists, it runs
   `kdos-desktop`.

Logging in on any other terminal, over a serial line or over ssh gives you a
shell and no desktop.

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

**Resource limits for boxes depend on how the session started.** `kdos-getty`
places an autologin session in `/sys/fs/cgroup/user.slice/user-<uid>/session`,
the cgroup delegated to that user, so the `memory =` and `cpus =` keys of a
box profile become real limits. A session started from a password prompt or
over ssh stays in the root cgroup, where podman accepts those limits and does
not enforce them.

### The shared bring-up: `session-common.sh`

Both start scripts source `/usr/local/lib/kdos/session-common.sh`. It is
**sourced, never executed**, and it defines one function per job:

| Function | Called by | Sets up |
|---|---|---|
| `kdos_session_open` | `kdos-desktop-start` | `$BROWSER` (default `xdg-open`), for a start that read no profile |
| `kdos_session_runtime` | `kdos-desktop` | `XDG_RUNTIME_DIR` (default `/run/user/<uid>`), before anything uses it |
| `kdos_session_keymap` | `kdos-desktop` | The console keymap from `/etc/keymap`, translated into `XKB_DEFAULT_LAYOUT` / `XKB_DEFAULT_VARIANT` |
| `kdos_session_boxes` | `kdos-desktop` | Starts `kdos-appbox warmup` at `nice 10`, and runs `kdos-box gc` every ten minutes to stop idle boxes |
| `kdos_session_bus` | `kdos-desktop` | One session bus per user, at a fixed path |
| `kdos_session_audio` | `kdos-desktop-start` | PipeWire, once per user rather than once per session |
| `kdos_session_once` | `kdos-desktop-start` | Everything done once the display exists (below) |

Each block carries a constraint that is invisible from where it is called, so
the file exists once and both scripts source it. A second copy would be a
second place for one of those constraints to go missing.

The keymap is translated because the console and XKB name layouts
differently:

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

When `/usr/share/X11/xkb/symbols` is present, a derived layout is exported only
if `/usr/share/X11/xkb/symbols/<layout>` exists: a layout xkbcommon cannot
compile makes the compositor fall back to US for the whole session,
lock-screen password prompt included. Where that directory is absent the
layout is exported unchecked.

Both start scripts are plain shell in `/usr/local/bin`; reading them shows
exactly what a login does.

### What `kdos-desktop` does

1. Refuses at once if `/usr/bin/kdos-comp` is missing, naming the build phase
   (`05_desktop`) it comes from.
2. On a virtio GPU, sets `WLR_NO_HARDWARE_CURSORS=1` (the virtual cursor plane
   misreports what it supports) and, when the GPU has no 3D acceleration
   (virgl), `WLR_RENDERER=pixman`. It reads the virgl feature bit from
   `/sys/bus/virtio/devices/*/features`, not from the kernel log: the log is
   root-only under `dmesg_restrict`, so reading it would force software
   rendering for every ordinary user.
3. Runs `kdos_session_runtime`, `kdos_session_keymap`, `kdos_session_boxes` and
   `kdos_session_bus`, then executes `kdos-desktop-start`.

### What `kdos-desktop-start` does

1. Exports `XDG_CURRENT_DESKTOP=KDOS`, over whatever the login shell set.
2. Runs `kdos_session_open` and `kdos_session_audio`.
3. Starts the portal stack and the input method in background subshells that
   first wait (up to 30 seconds) for the compositor's socket to appear.
4. Starts `kdos_session_once` in the background.
5. Runs `kdos-comp` in the foreground, with its standard error written to
   `$XDG_RUNTIME_DIR/kdos-comp.log` (the previous session's log is kept as
   `kdos-comp.log.old`) and shown on the terminal as well.

When the compositor exits:

- **Status 0 is a logout** — the menu's Log Out, which runs the compositor's
  `Exit` action, ends this way. The script exits and you are left at a shell
  prompt on `tty1`, still logged in: `~/.bash_profile` (or `~/.zprofile`) runs
  `kdos-desktop` as an ordinary command, so a session that fails to come up
  also leaves you a working shell. Typing `exit` there ends the login; `tty1`
  then gets a fresh login, and with autologin on that logs straight back in
  and starts the desktop again.
- **Any other status is a crash.** The script prints the last 20 lines of the
  log and asks `r=restart session, anything else=exit to tty`. Answering `r`
  re-runs the script, which starts the portal stack again (the screen-capture
  backend died with the compositor) but not a second PipeWire.
- **Three crashes within 60 seconds** stop the offer: a crash loop faster than
  a person can read the log is not one to keep feeding.

The log goes to a file followed by `tail -f`, never through a pipe. Every
descendant of the compositor inherits its standard error, and a pipe's reader
sees end-of-file only when the last writer closes — so one long-lived
grandchild, such as the `conmon` that lives as long as a boxed application,
would hold the pipe open forever. The terminal would then sit there with no
desktop, no error and nothing in a log.

### What runs once, after the display exists

`kdos_session_once` takes a readiness test as its argument — a command that
blocks until the display exists and exports whatever the children need to
reach it (for `kdos-desktop-start`, waiting for the compositor's socket and
exporting `WAYLAND_DISPLAY`). Then, in one background subshell so that none of
it delays the desktop:

| Step | Condition | Notes |
|---|---|---|
| The login sound (`kdos-sfx login`) | `kdos-sfx` installed | Silent on failure |
| The keybinding card (`kdos-keys --first-run`) | `~/.config/kdos/first-run` does not exist | `kdos-keys` writes the marker once you have seen it; delete it to see the card again |
| `mpd --no-daemon` and `kdos-mpctl watch` | `mpd` installed and a config in `~/.config/mpd/mpd.conf`, `~/.mpdconf`, `~/.mpd/mpd.conf` or `/etc/mpd.conf` | The image ships no mpd config, so this is opt-in. mpd ends with the login |
| The pending-applications offer | See [below](#applications-chosen-during-installation) | |
| The per-user timers | `~/.config/kdos/timers.d/*.timer` and `snooze` | See below |
| Session restore | `~/.config/kdos/session-restore` exists | Relaunches the `app` lines of `${XDG_STATE_HOME:-~/.local/state}/kdos/session`, two seconds apart |

**Per-user timers.** Each non-comment line of a `*.timer` file is
`NAME TIMESPEC... -- COMMAND...`, the same format as the system table in
`/etc/kdos/timers.d`. The command is an argument vector, not a shell line. A
line whose schedule `snooze -n` rejects, or whose command is not installed, is
skipped. Each line runs in a loop that waits for its slot with `snooze`, runs
the command, and waits again, so it repeats for as long as the session lives.
The loops' process ids are written to `$XDG_RUNTIME_DIR/kdos/timers.pid`, and a
restarted session stops the previous set before starting its own. The timers
die with the session: a job that writes into your home has no business
outliving the login that started it. The shipped `20-updatedb.timer` builds
your file-search index.

**Session restore** only relaunches boxed applications. `kdos-session-save`
writes the file when you log out, restart or shut down from the menu or from
the session menu. Where each window reappears is the compositor's window
memory, not this file.

### Applications chosen during installation

Applications ticked in the installer are *offered* at first login, never built
in the background. When `kinstall` can neither import an exported set nor
build the applications over the network (or a network build fails), it writes
the chosen ids to `/var/lib/kdos/apps-pending`. The first session then sends a
notification — "N chosen during installation are ready to install — open the
store" — and starts nothing; the store is the panel's application window,
[`kdos-store`](../04-programs/kdos-shell.md#kdos-store). Building means podman and apt, which can take
twenty minutes or more on a machine somebody has just booted, and a job that
long with nothing on screen is a machine busy for reasons nobody can see.
The panel's update window, `kdos-update` (see
[kdos-shell](../04-programs/kdos-shell.md#the-system-surfaces)), follows the
same rule for `kdos update apply`: it says what to type and never starts the
build itself.

| File | Owner | Meaning |
|---|---|---|
| `/var/lib/kdos/apps-pending` | root | What is still wanted. Removed only when `kdos app install --pending` has built all of it, so a partial run forgets nothing |
| `~/.config/kdos/apps-offered` | you | The offer has been made. Delete it and the offer comes back at the next login |

The notification is sent only once `org.freedesktop.Notifications` has an
owner (the script waits up to 20 seconds), and the marker is written only
after the send. Nothing activates that bus name on demand, so a call made the
moment the display appears would reach a notifier that is still starting and
be lost; a session that never gets a notifier keeps the offer for the next
login.

## The session message bus

One `dbus-daemon` per user, listening at a fixed path:
`unix:path=$XDG_RUNTIME_DIR/bus`. A session restart reuses the running daemon.

It is not started with `dbus-run-session`, which listens on a socket in the
host's `/tmp`. Boxes see the runtime directory at the same path as the host,
so one fixed address in `$XDG_RUNTIME_DIR` is valid on both sides. Without a
reachable bus, a boxed application's single-instance negotiation fails (every
impatient re-click starts another full instance), settings and accessibility
probes stall, and notifications go nowhere.

Three details of the startup matter:

- **The address carries no GUID.** A later session rebinding the socket would
  otherwise abort every client using a strict D-Bus implementation with a
  server-identity mismatch.
- **The start is serialised with a lock** (`$XDG_RUNTIME_DIR/.kdos-bus.lock`),
  so two sessions starting at once cannot both decide the daemon is down, with
  the loser removing the winner's socket.
- **The lock is closed for the daemon** (`7>&-`). `dbus-daemon` inherits open
  descriptors and outlives the subshell, so without that it would hold the lock
  until logout, and the *second* login of a boot would block forever before
  printing anything — a terminal with no desktop, no error and nothing in a
  log.

## Audio

PipeWire runs on the host, started by `kdos_session_audio` from
`kdos-desktop-start`, once per user (a restart finds it running and starts
nothing). Three programs, in this order:

1. `pipewire`, the media graph;
2. `wireplumber`, its session manager — it connects to the daemon's socket;
3. `pipewire-pulse`, the PulseAudio compatibility layer, which announces sinks
   that exist only once WirePlumber has created them.

PipeWire is built with no session manager of its own (`-Dsession-managers=[]`).
The daemon routes nothing by itself: device discovery, which sink a stream
lands on and a Bluetooth headset's profile are all policy, and policy is
WirePlumber's. `kdos-oomd` names `wireplumber` explicitly in the processes it
protects, because the prefix that covers `pipewire` and `pipewire-pulse` does
not match it, and killing it leaves a running graph with nothing connected.

**Bluetooth codecs are enabled by name**, not left to auto-detection: aptX,
LDAC, AAC, LC3, Opus and G.722, linked from `libfreeaptx`, `ldacbt`, `fdk-aac`
and `liblc3` (all in the port's `depends`). SBC is mandatory in the profile and
always present. Each codec is a meson feature that silently disables itself
when its library is missing, and a headset then falls back to SBC with nothing
saying why.

**LE Audio needs the Bluetooth daemon's half too.** bluez offers the LC3 (BAP)
endpoints only when its experimental D-Bus interfaces and the kernel's ISO
socket feature are both on, so `/etc/init.d/60_bluetooth.sh` starts
`bluetoothd -n -E --kernel=6fbaf188-05e0-496a-9885-d6ddfdb4e03e`. Without them
an LE Audio headset or hearing aid falls back to A2DP and HFP, or does not
connect. They are command-line flags rather than a `main.conf`, so the file
the bluez package owns is not replaced.

### Reaching the graph from inside a box

A boxed application reaches the host's PipeWire through the shared
`$XDG_RUNTIME_DIR`: a program that speaks PulseAudio finds the
`pipewire-pulse` socket there through the base pack's `libpulse0`.

A plain ALSA program in a box needs one more file. `kdos-boxinit`, the box's
first process, writes `/etc/asound.conf` pointing ALSA's default PCM and mixer
at PulseAudio (`pcm.!default { type pulse }`, `ctl.!default { type pulse }`),
using `libasound2-plugins` from the Debian base. It writes the file only when
that plugin is present, so a base without it (Alpine) keeps ALSA's own
default.

Without the redirect, `default` inside the box is alsa-lib's built-in
`plug → softvol → dmix` chain. Because `/dev` is shared and the host `audio`
group survives the box's user namespace, that open **succeeds and takes the
sound card**: the box has sound and the rest of the machine has none until the
box stops. The redirect moves `default` only; `hw:0` and `sysdefault` still
name the hardware.

### Reaching the graph from a plain ALSA program on the host

On the host this takes exactly one file, `/etc/alsa/conf.d/99-kdos-pipewire.conf`.
`alsa.conf`'s `@hooks` read `/var/lib/alsa/conf.d`, `/usr/etc/alsa/conf.d`,
`/etc/alsa/conf.d`, `/etc/asound.conf` and `~/.asoundrc` — **not**
`/usr/share/alsa/alsa.conf.d`, where PipeWire installs its own drop-in.
Without a file in a directory that is actually read, every ALSA program talks
to the card directly, the first one to open it owns it, and PipeWire runs with
no device. The resulting stutter is hard to diagnose: a dmix client's underrun
happens in userspace, so it shows no xrun on the card and nothing in `dmesg`.

The file defines two devices and a default:

| Name | Is | Shown as |
|---|---|---|
| `kdos_pipewire` | PipeWire | "Default Audio Device (PipeWire)" |
| `kdos_card` | `plug` onto `sysdefault`, the card directly | "Sound Card (no sound server)" |
| `pcm.!default` | Whichever of the two `$KDOS_ALSA_DEFAULT` names; `kdos_pipewire` when unset | |

A login that runs no session script has no PipeWire: a tty2 login, a serial
console and an ssh login all lack one. There, run a program with
`KDOS_ALSA_DEFAULT=kdos_card` to play straight to the card. The mixer control
(`ctl.default`) is left on the hardware, so the panel's volume control keeps
working when PipeWire is what went wrong.

Every redefinition in that file carries its `!`. The hooks run after the rest
of `alsa.conf` is parsed, so a plain `pcm.default { … }` aborts the whole
configuration load and leaves every ALSA program with no configuration at all.
Check it with `aplay -L`.

### The quantum, and why a virtual machine gets a bigger one

The *quantum* is how many audio frames PipeWire processes per cycle; it sets
the latency. Under a hypervisor the card gets a deeper buffer, because an
emulated card has no clock of its own: the emulated codec advances its DMA
position from the emulator's main loop, the same loop that updates the
display, so the position moves in bursts. PipeWire keeps about one quantum in
the device and times its wake-ups from that position, so a wake-up later than
one quantum is silence. The underrun belongs to the *player*, not the card,
and nothing appears in `dmesg`.

`/etc/pipewire/pipewire.conf.d/99-kdos-vm.conf` applies only when
`cpu.vm.name` is set (a virtual machine):

| Setting | Value |
|---|---|
| `default.clock.quantum` | 4096 (85 ms at 48 kHz) |
| `default.clock.min-quantum` | 4096 |
| `default.clock.max-quantum` | 8192 |

The quantum floor is 85 ms, and measured end-to-end latency is typically
127 ms, against a 42 ms floor and 64 ms typical at PipeWire's own 1024-frame
quantum. A machine on real hardware
keeps PipeWire's defaults and the 21 ms latency that comes with them.
`pw-metadata -n settings` shows which is in force, and `pw-top`'s `ERR` column
counts underruns.

The quantum is also the granularity of every music clock on the machine: a
program that follows a song position sees it advance in quantum-sized steps,
not smoothly, and has to tolerate one quantum of flat step rather than chase
it. [kdos-bb](../04-programs/kdos-bb.md#the-servo) has the measured staircase
and the correction that copes with it.

## Portals

A *portal* is how a sandboxed application asks the host to do something it
cannot do itself: open a file chooser, share the screen, open a link. The
front-end daemon, `/usr/lib/xdg-desktop-portal`, owns the public D-Bus
interfaces; *backends* implement them.

`/usr/share/xdg-desktop-portal/kdos-portals.conf` chooses the backends. The
front end looks for `<desktop>-portals.conf` using the lowercased first name in
`XDG_CURRENT_DESKTOP`, so `KDOS` gives `kdos-portals.conf`:

```ini
[preferred]
default=none
org.freedesktop.impl.portal.ScreenCast=wlr
org.freedesktop.impl.portal.Screenshot=wlr
org.freedesktop.impl.portal.FileChooser=kdos
org.freedesktop.impl.portal.Settings=kdos
org.freedesktop.impl.portal.AppChooser=kdos
org.freedesktop.impl.portal.Access=kdos
```

| Portal | Backend |
|---|---|
| ScreenCast, Screenshot | `xdg-desktop-portal-wlr` |
| FileChooser, Settings, AppChooser, Access | `xdg-desktop-portal-kdos` |
| OpenURI | The front end itself, using AppChooser |
| Camera, Location | The front end itself, gated by Access |
| Everything else (Print, Email, Wallpaper, …) | None |

Without this file the front end would fall back on each backend's own
`UseIn=` line. The KDOS backend's names `KDOS`, but `xdg-desktop-portal-wlr`'s
names other compositors, so screen capture would have no backend.

`XDG_CURRENT_DESKTOP` is set by `kdos-desktop-start`, the program that starts
the display, over whatever the login shell left. `/etc/profile.d/10-wayland.sh`
fills it in (`KDOS`) only when it is unset, which is what a serial line and an
ssh login get. The variable is a list, most specific first.

Each interface names exactly one backend. A list such as `wlr;kdos` would not
fall through: a backend that D-Bus can activate always counts as available, so
the front end would pick `wlr` and fail.

`default=none` is deliberate. The backend other desktops fall back to is a GTK
program, and there is no GTK on this host, so Print, Email, Wallpaper and the
rest genuinely have nobody to serve them. A portal that could only ever fail
is not advertised. Printing from a box still works through the application's
own print dialog and the shared CUPS socket (see
[the environment a box receives](#the-environment-a-box-receives)).

`OpenURI` — "open this on the host for me" — is what a boxed application asks
when a link or a downloaded file is clicked. The front end resolves the host's
handlers itself and launches the chosen one from the host's own launchers, so
a link in a boxed application opens in the browser this desktop generated an
entry for. It exports the interface **only** when a backend answers
`AppChooser`, which is why the KDOS backend implements AppChooser rather than
an `impl.portal.OpenURI` interface the specification does not have.

### Startup ordering

The front end reads its list of backends once, when it starts, and
`xdg-desktop-portal-wlr` is a Wayland client that needs the compositor. So a
background subshell of `kdos-desktop-start` does five things in order:

1. Waits for the compositor's socket to exist.
2. Pushes `WAYLAND_DISPLAY`, `XDG_CURRENT_DESKTOP`, `XDG_SESSION_TYPE`,
   `XCURSOR_THEME`, `XCURSOR_SIZE` and `BROWSER` into the bus's activation
   environment with `dbus-update-activation-environment`.
3. Starts the capture backend, `/usr/lib/xdg-desktop-portal-wlr`.
4. Waits (up to 20 seconds) for it to own
   `org.freedesktop.impl.portal.desktop.wlr`.
5. Stops any front end that is already running, and starts
   `/usr/lib/xdg-desktop-portal`.

Get that order wrong and screen capture stays unavailable for the whole
session (OBS reports "No capture sources available").

### The KDOS backend

`xdg-desktop-portal-kdos` (source `src/desktop/xdg-desktop-portal-kdos`,
installed at `/usr/lib/xdg-desktop-portal-kdos`, started by D-Bus as
`org.freedesktop.impl.portal.desktop.kdos`) serves FileChooser, Settings,
AppChooser and Access. It does not implement ScreenCast.

It is a bus adapter and nothing more. For a file or application choice it runs
`kdos-pick` and reads its output; for an Access question it runs `kdos-prompt`
and reads its exit status. The chooser stays an ordinary program you can run
by hand, script or replace. The backend does no permission checking of its
own; that is the front end's job.

Four properties of that backend, each of which is a common way for a portal to
break:

**Every request is answered.** Response 0 is success, 1 is cancelled, 2 is an
error. A request that is never answered leaves the application blocked with a
half-drawn window.

**The bus loop never blocks on a dialog.** The handler starts the chooser,
keeps the request, returns without replying, and adds the chooser's output
pipe to its main loop; the reply is built when the chooser exits. Otherwise a
second application's Open would queue behind the first, and a boxed
application asking Settings for the colour scheme — which happens on every
launch — would hang until the dialog was dismissed.

**The dialog is placed over its application.** An application sends
`parent_window` as `wayland:<xdg-foreign handle>`; the backend strips the
scheme and passes the handle to `kdos-pick --parent`, which asks the
compositor to centre the dialog on its parent. An `x11:` handle, or anything
with no scheme, is dropped. The chooser is always run from an argument vector,
because filter patterns and file names arrive from other applications.

**Settings answers two namespaces, and a boxed application needs both.**

| Namespace | Key | Value |
|---|---|---|
| `org.freedesktop.appearance` | `color-scheme` | 1, "prefer dark", whatever the accent, including the light `paper` scheme |
| | `accent-color` | The current accent, as RGB; libadwaita reads it |
| `org.gnome.desktop.interface` | `gtk-theme-name` | `KDOS-<accent>` |
| | `icon-theme-name` | `KDOS` |
| | `cursor-theme` | `KDOS-cursors` |
| | `color-scheme` | `prefer-dark`, whatever the accent |

Because both `color-scheme` answers are dark under every accent, a boxed GTK
or libadwaita application draws dark even on the light `paper` desktop.

The GNOME namespace is the only thing that restyles a GTK3 application that is
already running. A user stylesheet is loaded once at startup, so a palette
written into it reaches the next launch and never this one, while a theme
*name* that changes makes GTK rebuild its whole style.

**The backend watches the accent and emits `SettingChanged`.** A toolkit reads
the settings once and then waits for that signal; without it, an accent
switched while a boxed editor is open would reach only its next launch. The
watch is on the *directory*, because the file is replaced rather than edited.
The signal is held back until `~/.themes/KDOS-<accent>` exists: `kdos theme
--preview` writes the setting without generating a theme, and an application
told the name of a theme that is not there falls back to its own default.

### Access, and what it unlocks

The front end exports the Camera, Screenshot and Location portals only when a
backend answers `org.freedesktop.impl.portal.Access`, the grant-or-deny
question each asks before handing an application what it guards. The KDOS
backend answers with `kdos-prompt`:

- the title and subtitle the front end sends become the question;
- its grant and deny labels become the two buttons;
- the dialog opens with Deny selected, which is also the answer when nobody
  answers.

| `kdos-prompt` result | Portal response |
|---|---|
| Grant | 0 |
| Deny, or Escape | 1 (refused) |
| The prompt could not run | 2 (error) |

The front end's body text is not shown, because it points at a privacy page in
a settings application this desktop does not have.

Answers are kept in the front end's permission store, under
`~/.local/share/flatpak/db/`, keyed by application id, so each question is
asked once. The front end recognises Flatpak, Snap and Linyaps sandboxes; a
KDOS box is none of them, so to the front end every box is the same host
program and **one answer covers every box**. To be asked again, delete the
portal's file in that directory.

Location comes from the system GeoClue service, `geoclue`. D-Bus starts it on
the system bus, as its own account, when the first client asks, and it exits
after a minute with no clients. Its Wi-Fi and 3G sources ask beaconDB for a
position from what `wpa_supplicant` and ModemManager see; the modem-GPS,
NMEA-over-Avahi and compass sources read their devices directly. It runs with
no consent agent: `/etc/geoclue/conf.d/90-kdos.conf` empties the agent
whitelist, and the port's `no-agent.patch` treats an empty whitelist as leave
to answer every client at once (upstream would hold every client until an
agent registered, which here would be never). So GeoClue answers whoever asks
on the system bus. The front end asks Access before Location only for a
sandbox it recognises, which means a boxed application's position is given
without a question; see
[known gaps](../06-reference/known-gaps.md#applications-and-boxes).

An icon or a sound an application hands a portal is decoded by
`xdg-desktop-portal-validate-icon` or `xdg-desktop-portal-validate-sound`, each
of which re-runs itself under `bwrap` with no network, no home directory and a
read-only `/usr`. `bwrap` is not setuid; it sandboxes through an unprivileged
user namespace.

### Recording

`kdos-record` records the screen to a file through the ScreenCast portal:

```sh
kdos-record                    # start: ~/Videos/YYYY-MM-DD-HHMMSS.mkv
kdos-record ~/clip.mkv         # start, to a file you name
kdos-record                    # again, while recording: stop
```

It asks the portal for a screen, reads the PipeWire node it is given, and runs
`gst-launch-1.0 -e pipewiresrc ! videoconvert ! <encoder> ! matroskamux !
filesink`, with `x264enc` (H.264) when that element exists and `vp8enc`
otherwise. It records video only.

**Choosing the screen is done by a person.** On this compositor the ScreenCast
backend is `xdg-desktop-portal-wlr`, whose chooser is `slurp` (configured in
`~/.config/xdg-desktop-portal-wlr/config` as `chooser_cmd=slurp -f %o -or`): it
covers the screen and waits for you to pick an output. The three portal calls
therefore have different deadlines:

| Call | Deadline | Why |
|---|---|---|
| `CreateSession` | 5 s | A local bus answering in milliseconds |
| `SelectSources` | 120 s | Waiting for somebody to decide; seconds would cancel every recording before the question is answered |
| `Start` | 30 s | The backend binds the capture interface, takes a first frame to learn the format and registers a PipeWire node before it answers |

**Running it again stops it.** There is one screen and one portal session, so a
second recording is not something to want, and a key binding or menu entry
that started a recording must be able to end it. While recording, the
program's process id is in `$XDG_RUNTIME_DIR/kdos/screencast.pid`. A second
invocation reads it, checks the process still exists (a file left by a crash
is not a recording), and sends it **`SIGINT`**, which it forwards to the
pipeline. `gst-launch-1.0 -e` turns an interrupt into end-of-stream so the
muxer writes its index; a `SIGTERM` straight to the pipeline would leave a
file without one. The handlers are installed without `SA_RESTART`, so the
`waitpid` holding the recording open returns with `EINTR` and the forward
happens.

**A still screen still produces frames.** A recording that received no frames
would be a file with no header: `pipewiresrc` drops a chunk of size zero, so
the muxer would never be handed anything and the file would end empty with no
error. The frames come from `ext-image-copy-capture`, which asks the output for
a frame whether or not anything changed. `max_fps=30` in the same
`xdg-desktop-portal-wlr` config bounds what that costs; without the line, a
still screen is captured and encoded at the output's own refresh rate for as
long as the recording runs.

**A portal session belongs to one connection**, which is why `kdos-record` is a
program rather than a script. The portal ties a session to the unique bus name
that created it and closes it when that name leaves the bus, so a shell script
making three `gdbus call` invocations makes three connections: the second is
answered `Invalid session`, and the first session is already gone. One
connection must stay open for the whole recording.

**A portal reply is a signal, not a return value.** Every call returns a
Request object path and answers later with a `Response` signal on it, so the
signal match is installed *before* the call; a signal that arrives with no
match is gone, and nothing replays it.

An application in a box that records sound as well (OBS, say) takes its video
through the same ScreenCast portal and its audio from PipeWire through the
PulseAudio socket in the shared runtime directory.

## Supervised chrome

The compositor starts and supervises the desktop's own programs (its
*chrome*) from a table in its source. Seven entries:

| Program | One per output? | Runs when (`~/.config/kdos/comp.conf`) |
|---|---|---|
| `kdos-shell` (the panel) | yes | `panel` is not `off` (default `bottom`) |
| `kdos-desk` (desktop icons) | yes | `desktop_icons = yes` (default) |
| `kdos-slit` (the dockapp column) | yes | `slit = yes` (default `no`) |
| `kdos-notifyd` (notifications) | no | always |
| `kdos-netagent` (Wi-Fi passphrase prompts) | no | always |
| `kdos-mediad` (removable-media offers) | no | always |
| `kdos-clip` (clipboard history) | no | `clipboard = yes` (default) |

These switches are read once at startup: a program switched off is not
started by a reconfigure, only by the next login.

**Three are per output**, each started with the output's name as an argument.
A layer surface (a panel-like surface the compositor anchors to a screen
edge) is placed on one screen, and `libktui`, the toolkit every KDOS surface
is drawn with (see [the design language](design-language.md)), keeps a single
buffer per process, so a second monitor cannot be a
second surface; it has to be a second process.

Each panel lists its own screen's windows. The window-management protocol
reports which outputs a window is on, and a panel filters its taskbar to the
screen its surface actually entered — not the one `--output` asked for, since a
screen unplugged between the request and the mapping leaves the panel wherever
the compositor put it. Two bars listing the same windows would put the same
buttons on both screens. A server that gives no per-screen answer gets the
unfiltered list, because filtering on an answer nobody gave would empty the
bar. Two things are not filtered:

- **the window menu**, because it is how a window on the other screen is
  reached;
- **the workspace pager**, because a workspace spans every screen, so
  occupancy is counted over every window. Otherwise a workspace would read as
  empty on the left screen while its windows were on the right.

**Four are single-instance**, because each owns something unique. The
notification daemon owns a bus name a second instance could not take. The
secret agent registers one agent with NetworkManager, and a second would raise
a second passphrase box for the same question. The media handler holds one
subscription to `kdos-mountd`, and a second would offer every stick twice. The
clipboard owns a socket and keeps its history in memory, so a second instance
would be a second history nobody could reach.

How the supervision behaves:

- **Children are reaped through the compositor's existing child-signal
  handling**; a second signal watcher would race the first. Because that signal
  coalesces, the handler also checks each supervised child by its own process
  id, so two chrome programs dying together do not leave one a zombie that is
  never restarted.
- **More than five deaths within thirty seconds stops the restarts**, because a
  crash loop buries the log line that explains it. The compositor then shows a
  `kdos-prompt` naming the program it gave up on — the log is invisible from
  inside the session — and Reconfigure (in the session menu) resets the
  counters and gives it another run of attempts.
- **Every child gets default signal handling back before it starts.** Ignored
  signals and the blocked-signal mask survive `exec`, so a session started
  under a wrapper that ignores a signal would pass that on to every program —
  and the live retint — every surface repainting in a new accent when
  `kdos theme` sends it a signal — would never arrive.
- **When an output goes away**, its chrome is signalled and the slot marked as
  stopping, so the reap frees it instead of restarting a program whose screen
  is gone.

## Screen capture and the clipboard

The compositor offers both generations of each capture protocol — the older
wlr screencopy and export-dmabuf interfaces, and the newer
`ext-image-copy-capture` with output and window sources, which the capture
portal backend prefers — and both generations of the data-control protocol
that clipboard tools use. Implementing only one generation would strand either
the tools shipped here or everything written after them.

A boxed application is kept off all of them. The compositor tags every client
from a box with a security context and offers it a fixed allowlist: surfaces,
the seat, shared memory and dmabuf buffers, text input, the primary selection
and the ordinary clipboard, and the usual shell and decoration protocols — and
none of the capture, data-control, input-method, layer-shell or
output-management interfaces. The full table is in
[the security model](security-model.md#sandboxed-clients).

That is what makes the portal the sanctioned route rather than a convenience:
a boxed screen recorder **cannot** bind the capture interfaces at all, so it
must ask the portal, which runs on the host and asks you which output to
share.

A screenshot of the desktop with the phosphor (CRT) effect on looks like what
you see. Output capture copies the output's final buffer, which is the
processed one. Per-window capture renders the window's own contents, without
the effect. Each is the honest answer to what was asked.

### Granting a box more than the allowlist

A box can be given specific protocols beyond the allowlist with a `grant`
line in its profile, `~/.config/kdos/boxes/<name>.conf`:

```ini
grant = screencopy, data-control
```

The eight grantable names are `screencopy`, `toplevel-capture`,
`export-dmabuf`, `data-control`, `foreign-toplevel`, `layer-shell`,
`input-method` and `output-power`; any other name grants nothing. Where a
protocol has two generations — screen copy, data control and the toplevel
list — the short name unlocks both. Grants are read once per box, at the first
bind that reaches the filter, and cached against the box name, since reading a
profile on every bind would put a file read in the compositor's hot path.
Reconfiguring the compositor (`SIGHUP`) drops the cache, so an edited profile
applies to the next client; a running one keeps what it already bound.

`input-method` hands the box every keystroke on the seat. What each name
unlocks, and why each is dangerous, is in
[the security model](security-model.md#granting-a-box-past-the-allowlist).

## Input methods

An *input method* turns keystrokes into text that a keyboard cannot type
directly — Chinese, Japanese or Korean, for example. Three parties are
involved, and they never speak to each other directly; the compositor is the
wire between them:

```
fcitx5  ──input-method──▶  kdos-comp  ──text-input──▶  the application
      ◀──virtual-keyboard──                             (host or boxed)
```

The application's half of that — the text-input protocol — is **inside** the
box allowlist, because denying it would deny input methods to exactly the
applications that need one. The engine's half is outside it, because a client
that can be an input method receives every keystroke on the seat.

The engine is `fcitx5`, with the Chinese, Anthy (Japanese) and Hangul (Korean)
add-ons. It is built Wayland-only and started by `kdos-desktop-start` as
`fcitx5 -d`, not by an autostart entry, because KDOS runs no autostart agent
(the port is built `ENABLE_XDGAUTOSTART=Off`). If fcitx5 is not installed the
session starts without it and prints nothing. The engines you can switch
between are set in `~/.config/fcitx5/profile`, which the image seeds.

A boxed application reaches the engine through the compositor, never
directly. Inside a box `QT_IM_MODULE=wayland` is set; the GTK equivalent is
deliberately **not set at all**, because GTK on Wayland chooses the right
route by itself when it is unset, and setting it is how a working GTK
application stops accepting input. Neither is ever set to `fcitx`: that is the
X11-era route where each toolkit talks to the engine directly, and inside a
container that engine does not exist.

**An X11 application has no input method**, on the host or in a box. Xwayland
passes no text input to its X clients, and the only route an X client has to
an engine is XIM, which fcitx5 provides only when built with X11 support. The
port is built `ENABLE_X11=Off`, because that would need the X client libraries
the host does not carry.

**The candidate window is drawn by KDOS.** `kdos-ime` owns the
`org.kde.impanel` bus name, and fcitx5's kimpanel module has a higher priority
than fcitx5's own interface and takes over as soon as that name appears. So
starting `kdos-ime` is all it takes to select it, and it starts before fcitx5
so the engine sees it from the start rather than switching after the first
keystroke. Only one process can own that name, and the session bus outlives a
session restart, so `kdos-desktop-start` stops any running `kdos-ime` before
starting its own (and again when the compositor exits). See
[the candidate window](../04-programs/kdos-shell.md#the-candidate-window).

## The environment a box receives

A command executed inside a container inherits **nothing** — not the
container's own init environment and not the caller's. So `kdos-appbox` states
every variable a launch needs:

| Variable | Value, and why |
|---|---|
| `PATH` | `/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin:/usr/games:/usr/local/games`. Without `/usr/games`, Debian's games are all "not found" |
| `HOME`, `USER`, `LOGNAME` | Yours. Some toolkits crash with no user name |
| `LANG` | The host's when it is UTF-8, otherwise `C.UTF-8`. GTK and others refuse a non-UTF-8 locale |
| `XDG_RUNTIME_DIR` | `/run/user/<uid>`, shared with the host |
| `XDG_SESSION_TYPE` | `wayland` |
| `WAYLAND_DISPLAY` | The box's own tagged socket from `kdos-boxsock` |
| `DBUS_SESSION_BUS_ADDRESS` | `unix:path=/run/user/<uid>/bus`. Without it a single-instance application blocks with no window and no message |
| `XDG_CURRENT_DESKTOP` | `KDOS`, for portal and theme selection |
| `GTK_USE_PORTAL` | `1`. See below |
| *(no `GTK_THEME`)* | Deliberately absent: it overrides the theme setting for the life of the process, so an accent change could never reach a running GTK application. The theme name arrives through the Settings portal and the seeded `gtk-3.0/settings.ini` instead |
| `GSETTINGS_BACKEND` | `keyfile`, because no settings daemon is reachable |
| `QT_IM_MODULE` | `wayland`: input methods through the compositor |
| `QT_QPA_PLATFORMTHEME` | For a pack-based box, whatever the pack's runtime declares in its metadata; for a store-built box, whatever its catalogue entry declares; otherwise `kde` where the image carries the KDE platform theme, or `gtk3` (with `QT_STYLE_OVERRIDE=Fusion` where the image asks for it) |
| `CUPS_SERVER` | `/run/cups/cups.sock`, when that socket exists on the host |
| `NO_AT_BRIDGE=1`, `GTK_A11Y=none` | Accessibility off by default; see below |
| `DISPLAY` | The host's, or the first Xwayland socket in `/tmp/.X11-unix`, for X11-only applications |
| `LIBGL_ALWAYS_SOFTWARE` | `1` only when the box profile's `render` key resolves to software: a profile that refuses the GPU, a machine with no render node, or a box with no `/dev/dri` because `devices = private` meets `gpu = no`. Nothing is exported for hardware rendering, because Mesa already picks the right driver and falls back on its own |

**`GTK_USE_PORTAL=1` is what makes the KDOS portal reachable at all.** A GTK
application routes through the portal only when it believes it is sandboxed,
which it decides from a marker file or from this variable, and a container has
neither. Without it, FileChooser and Settings exist, answer, and are never
called: every boxed application draws its own GTK file dialog instead of
`kdos-pick`. Firefox's default file-picker setting consults the same variable.
The cost is that GTK's Print dialog also goes to the portal, which has no Print
backend; the application's own print dialog, through `CUPS_SERVER`, still
works.

**Accessibility is a default, not a policy.** The host runs no accessibility
registry, so every boxed GTK application would spend its startup waiting for
one; that justifies turning the probe off. It does not justify hard-disabling
accessibility in every application, since a screen reader running *inside*
the box can reach the box's own registry. To turn it back on:

- for every launch, create `~/.config/kdos/a11y` (an empty file is enough);
- for one launch, set `KDOS_A11Y=1` (`KDOS_A11Y=0` forces it off).

## What is shared into a box

For a box built from packs (every catalogue application):

| Host path | Inside the box |
|---|---|
| `$HOME` | Read-write, at the same path. With `home = private` in the profile the host home is not shared; `${XDG_DATA_HOME:-~/.local/share}/kdos/boxes/<box>` is mounted at that same path instead, and `HOME` inside the box is still your host home path |
| `/tmp` | Read-write |
| `/run/user/<uid>` | Read-write: the session bus, the compositor socket, PipeWire |
| `/dev`, `/sys` | Read-write, unless the profile says `devices = private`; then only `/dev/dri`, and only with `gpu = yes` |
| `/dev/shm` | Read-write |
| `/run/cups` | Read-write, when the print service is running at creation time |
| `/var/lib/kdos/packs/mnt` | **Read-only**, so links into mounted data packs resolve |
| `/usr/libexec/kdos/kdos-boxinit` | Read-only, as `/usr/libexec/kdos-boxinit`, the box's first process |
| `/run`, `/run/lock` | Private temporary filesystems |

**Sharing is fixed when the box is created.** A box created before the print
service was running does not have the CUPS socket until the box is created
again, while `CUPS_SERVER` is decided at every launch. The two halves are
asymmetric on purpose: the variable probes again each time, and a volume can
only be added when the box is created. The same holds for every profile key
that changes a namespace or a volume (`home`, `devices`, `network`, `ipc`,
`gpu`, and the rest).

To apply a changed key, remove the box with `kdos-box remove <box>`; the next
launch (or `kdos-box create <box>`) creates it again with the current profile
and host state. Removing a box keeps its profile and its writable layer under
`~/.local/share/kdos/boxes/<box>`, and never touches your home directory.

## Ending a session

Logging out ends the compositor, and with it the session. The compositor
handles a termination signal through its event loop and tears down in order:

1. closes the command socket behind
   [`kdos hey`](../04-programs/kdos-command.md#kdos-hey), so no command acts on a session that
   is already over;
2. plays the CRT power-down animation (bounded by a deadline);
3. stops the wallpaper, the frame reporter, window-position memory, window
   groups, the [box chips](../04-programs/kdos-comp.md#box-identity), the lid handler, the peek view and the idle policy;
4. removes the phosphor pass;
5. destroys the server.

The panel and the notification daemon notice the closed compositor socket
themselves, because the toolkit's event loop uses Wayland's documented
prepare-read sequence and sees the socket close rather than spinning on it.
`kdos-desktop-start` then stops `kdos-ime` and, for a clean exit, leaves you
at a shell prompt on `tty1` in the logged-in account. The per-user timers stop with the session.

## See also

- [Architecture overview](overview.md) — where the session sits in the whole system
- [kdos-comp](../04-programs/kdos-comp.md) — the compositor and its configuration
- [kdos-appbox](../04-programs/kdos-appbox.md) — launching boxes and box profiles
- [Packs and boxes](packs-and-boxes.md) — how a box is built and started
- [The security model](security-model.md) — the sandbox allowlist and what it denies
- [Boot and init](boot-and-init.md) — everything before the login prompt
- [The desktop](../02-user-guide/desktop.md) — using what this page starts
