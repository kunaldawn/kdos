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
| `kdos_session_open` | `$BROWSER`, for the login path that reads no profile |
| `kdos_session_runtime` | `XDG_RUNTIME_DIR`, before anything uses it |
| `kdos_session_keymap` | The console keymap as XKB variables |
| `kdos_session_boxes` | The appbox warmup, and giving idle ones back |
| `kdos_session_bus` | One session bus per user, at a fixed path |
| `kdos_session_audio` | PipeWire, once per user rather than once per session |
| `kdos_session_once` | The login sound, the first-run card, the pending-applications offer, the session restore |

It is one file rather than two copies because each of those blocks carries a trap that cost a
debugging session to find — the `flock 7>&-` in the bus function above all — and a second copy is a
second place to lose one. **The keymap function especially:** `libkkms` reads the same
`XKB_DEFAULT_LAYOUT` that every Wayland client does, so leaving that table in the graphical script
gave the console desktop US QWERTY on a machine whose owner does not type it.

`kdos_session_once` takes its **readiness test as a command**, because what "up" means differs: a
Wayland socket for one session, a session socket for the other. The command must block until the
display is there and export whatever the children need to reach it.

**Applications the installer chose are offered, never built in the background.** When `kinstall`
could neither import a set nor reach a network it writes the ticked ids to
`/var/lib/kdos/apps-pending`, and the first session notifies rather than starting anything:
building them is podman and apt, twenty minutes on a machine somebody has just booted, and a job
that long behind no surface at all is a machine busy for reasons nobody can see. `kdos-update`
keeps the same rule about `kdos update apply`.

The marker is `~/.config/kdos/apps-offered` and is the **user's** — delete it and the offer comes
back. `/var/lib/kdos/apps-pending` is root's and survives until `kdos app install --pending`
actually completes, so a partial run does not forget what was wanted.

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
5. Starts the shell's **layer** surfaces — `kdos-notifyd`, `kdos-netagent`, `kdos-mediad`,
   `kdos-desk` and `kdos-slit`. None needs a
   window list, and all three name the console display implementation first, so they run here
   exactly as they do under the compositor. They are **not** supervised: unlike a view they hold
   nothing a restart would recover, and a crash loop on a toast daemon is a desktop that spends its
   life restarting something nobody is looking at. `kdos-slit` exits silently when no slit is
   configured, which is why it is started unconditionally.

   **The panel is not among them.** It needs a foreign-toplevel manager, and the console has no
   protocol for one yet; the session's own one-row taskbar is the bar until it does.

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

PipeWire runs on the **host**, started by `kdos-desktop-start` *and* `kdos-con-start`: the daemon,
a session manager and the PulseAudio compatibility layer. **A box reaches it because the base pack
carries an audio client and its init writes the ALSA default.** `libpulse0` is what a program that
opens PulseAudio finds on the shared `$XDG_RUNTIME_DIR`; `libasound2-plugins` plus the
`/etc/asound.conf` `kdos-boxinit` writes (`pcm.!default { type pulse }`) is what a plain ALSA
program in a box follows to the same place. Without both, `default` falls through to alsa-lib's
built-in `plug → softvol → dmix` card chain — and since `/dev` is shared and the host `audio` group
survives the keep-id user namespace, that open **succeeds and takes the card**, so the box has
sound and the rest of the machine has none until it stops. It moves `default` only: `hw:0` and
`sysdefault` still name the hardware, which the shared `/dev` still reaches.

**And plain ALSA programs reach it too, which takes one file to arrange.** `alsa.conf`'s `@hooks`
list reads `/var/lib/alsa/conf.d`, `/usr/etc/alsa/conf.d`, `/etc/alsa/conf.d`, `/etc/asound.conf`
and `~/.asoundrc` — and **not** `/usr/share/alsa/alsa.conf.d`, which is where PipeWire installs its
own drop-in. Without a file in a directory that is actually read, `default` falls through to
alsa-lib's built-in card chain, `plug → softvol → dmix`: every ALSA program talks to the card
directly, the first one to open it owns it, and the daemon runs with no device at all. A dmix
client's underrun is raised in **userspace**, so the stutter that follows shows no xrun on the card
and nothing in `dmesg` — which is what makes it hard to see.

`fs/etc/alsa/conf.d/99-kdos-pipewire.conf` is that file. It names two routes — `kdos_pipewire` and
`kdos_card`, the card chain — and points `pcm.!default` at whichever `$KDOS_ALSA_DEFAULT` names,
PipeWire when it names nothing. **The variable exists because a login that runs no session script
has no daemon**: `kdos_session_audio()` is called from the two session starts only, so a `tty2`
getty, a serial console and an `ssh` login have none, and `KDOS_ALSA_DEFAULT=kdos_card` is how a
program plays there. **`ctl.default` is deliberately left on the hardware**, so the panel's volume
applet keeps working when PipeWire is the thing that went wrong.

Every redefinition in that file carries its `!`. The hooks run after the rest of `alsa.conf` is
parsed, so a plain `pcm.default { … }` does not lose a race — it aborts the whole config load and
every ALSA program on the machine is left with no configuration at all. `aplay -L` is the check.

**Under a hypervisor the card is given a deeper buffer, because an emulated card has no clock of
its own.** The emulated codec advances its DMA position from the emulator's main loop — the same
loop that uploads the display — so the position moves in bursts rather than at a steady rate.
PipeWire computes its own wakeups from that position and keeps about one quantum in the device, so
a wakeup later than the quantum is silence, and the underrun is the *player's* rather than the
card's: nothing appears in `dmesg`. `fs/etc/pipewire/pipewire.conf.d/99-kdos-vm.conf` raises the
quantum floor to 4096 frames — 85 ms at 48 kHz — for a machine whose `cpu.vm.name` is set, which
measures as 85 ms held at worst and 127 ms typically against 42 ms and 64 ms at PipeWire's own
1024-frame floor. **A machine booted off metal keeps that floor** and the 21 ms of latency with it.
`pw-metadata -n settings` says which one is in force, and `pw-top`'s `ERR` column counts the
underruns.

**The quantum is also the granularity of every music clock on the machine.** libmikmod's ALSA
driver asks for 50 ms periods — upstream's own request; the port's two patches guard a `NULL`
close and keep the stream non-blocking, and neither touches it — and renders exactly one period
per `VC_WriteBytes()` call, which is the same call that advances the player's song position.
`pcm_pipewire` grants that period verbatim instead of rounding it to a card's, and the plugin's
`hw_ptr` then moves once per graph cycle: 4096 frames, 85.3 ms. So the driver hands over about two
periods at a time and a song position is a **staircase** — a 90.0 ms riser on a 97.7 ms tread,
with 88.3 % of a 10 ms poll seeing no advance at all. **A program that reads a song position must
tolerate one quantum of tread rather than chase it:** a loop that steers its own clock at every
disagreement with the music steers on quantisation and not on drift, and a step sized for the
whole disagreement lands inside a single frame. Shrinking the period does not smooth it — the
sub-steps still all land in the one call the quantum wakes, so the tread is unchanged and the only
thing bought is a wakeup per period on the machine where wakeups are already late.

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
console session sets `KDOS-Console:KDOS` and gets `kdos-console-portals.conf`; the compositor sets
`KDOS` and gets the file above. The one line that differs is `ScreenCast`, which on the console is
`kdos` — `xdg-desktop-portal-wlr` is a Wayland client and there is no compositor on that path, so it
would start, fail to connect, and leave the interface with a backend that is not there.

**EACH SESSION SETS THE NAME ITSELF, in the program that starts its display**: `kdos-con-start` and
`kdos-desktop-start`, both before anything else runs and both over whatever a login shell left
behind. `/etc/profile.d/10-wayland.sh` fills the variable in for a shell no session started, and it
has only `WAYLAND_DISPLAY` to guess from — which is empty in the login shell a graphical session
begins in. A session that lets that guess stand advertises itself as the console, and the front end
then sends its `ScreenCast` to a backend that records a console session by `$KDOS_CON`, which a
compositor session does not have: every recording is refused in twenty milliseconds with the wlr
backend running and never asked.

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

**The backend spawns the view on the VIEW socket**, which it derives from `$KDOS_CON` by changing
the suffix. `$KDOS_CON` is the *surface* socket — what a client attaches to — and the two are one
session's pair; a view handed the surface socket is never told a size and gives up before it has
a node to name.

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

**`kdos-record` is the host's own caller of all this.** Three calls — `CreateSession`,
`SelectSources`, `Start` — and the node the last one answers with goes to
`gst-launch-1.0 pipewiresrc`, which is why `pipewire` is built `-Dgstreamer=enabled`: without it the
`pipewiresrc` element does not exist and nothing can read a node from a pipeline. It goes through
the portal rather than round it even though the backend is ours, because `Start` is what makes the
view exist and the answer it gives is the one a boxed application would get.

**A STILL SCREEN STILL PRODUCES FRAMES, and it takes a different answer on each desktop.** A
recording that carried no buffer at all would be a file with no header in it — `pipewiresrc` drops
a chunk of size zero, so the muxer downstream is never handed a frame and `~/Videos/<name>.mkv`
ends at nothing with no error anywhere to say why. On the console the view re-sends its last frame
every 500 ms when nothing has changed, which is two copies a second for a desktop nobody is
touching. Under the compositor the frames come from `ext-image-copy-capture`, which asks the
output for a frame whether or not anything is damaged — `max_fps` in
`~/.config/xdg-desktop-portal-wlr/config` is what bounds the commits that costs, and removing that
line lets a still screen be committed at the output's own rate for as long as the recording runs.

**AND `SelectSources` IS ANSWERED BY A PERSON.** On the compositor the ScreenCast backend is the
wlr one, whose chooser is `slurp` — it covers the screen and waits for the pointer to pick an
output. So the three calls do not share one deadline: `CreateSession` is a program on a local bus
answering in milliseconds and gets five seconds, while `SelectSources` and `Start` are waiting for
somebody to decide and get two minutes and thirty. A deadline of seconds on the middle call
cancels every recording before the question on screen has been answered, and reports it as a
portal that did not reply.

**`kdos-record` AGAIN STOPS IT.** There is one screen and one portal session, so a second
recording is not something to want, and a chord or a menu row that started one has to be able to
end it from where it was started. While the pipeline runs, the program's pid is in
`$XDG_RUNTIME_DIR/kdos/screencast.pid`; a second invocation reads it, checks the process is still
there — a marker left by a crash is not a recording — and sends **`SIGINT`**, which the running
process forwards to the pipeline. `gst-launch-1.0 -e` turns an INT into an end-of-stream so the
muxer writes its index; a `SIGTERM` straight at the pipeline would leave a file without one. The
handlers are installed with `sigaction` and **no `SA_RESTART`**, because the `waitpid` holding the
recording open has to come back with `EINTR` for that forward to happen at all.

**A session belongs to a CONNECTION, and that is why `kdos-record` is a program.** The portal keys
a session by the unique name that created it and closes the session when that name leaves the bus,
so a shell script making three `gdbus call` invocations makes three connections: the second is
answered `Invalid session` and the first session is already gone. One connection has to stay open
for the whole recording, which is a program holding an `sd-bus` — the same basu the portal backend
links.

**A portal reply is a SIGNAL, not a return value.** Every call returns a Request object path and
answers later with `Response` on it, so the match is installed *before* the call — a signal that
arrives with no match is gone, and nothing replays it. The signal is *directed* at the caller, so
it reaches the connection that asked and no other; a separate monitoring process is not an
alternative to holding the connection, it is a different thing entirely.

**And a cast completes on a LINE, not on end-of-file.** The chooser exits and its output ends; the
cast view does not exit — it *is* the stream — so the reply is built when its first line arrives and
the process is left running until the session is closed.

**Settings answers two namespaces, and a boxed application needs both.**
`org.freedesktop.appearance` carries `color-scheme` — always "prefer dark", because KDOS has no
light palette and a toolkit told "no preference" picks its own light theme — and `accent-color`,
which libadwaita reads. `org.gnome.desktop.interface` carries `gtk-theme-name`, `icon-theme-name`
and `cursor-theme`, and it is the **only thing that retints a GTK3 application already running**:
the user stylesheet is loaded once at startup, so a palette written into it reaches the next launch
and never this one, while a theme name that moves makes GTK rebuild the whole cascade.

**And the backend watches the accent file and emits `SettingChanged`.** A toolkit reads the
settings once and then waits for that signal; without it the portal is a lookup nobody repeats, and
an accent switched while a boxed editor is open reaches its next launch. The watch is on the
directory, because the file is replaced rather than edited, and the signal is withheld until
`~/.themes/KDOS-<accent>` actually exists — `kdos theme --preview` writes the file and generates
nothing, and an application sent the name of a theme that is not there falls back to its own
default.

**And the bus loop does not block on the dialog.** The fork happens in the handler, the request
message is retained, the handler returns "handled" *without* replying, the pipe joins the main
loop, and the reply is built when the chooser exits. A portal backend is a server, and a server
that stops serving while it thinks is a server that is down — without this, a second application's
Open queues behind the first, and a boxed application asking Settings for the colour scheme, which
happens on every launch, hangs until the dialog is dismissed.

## Supervised chrome

The compositor starts and supervises the desktop's own programs from a table in its source. Seven
entries:

| Program | Per output? | Gated on |
|---|---|---|
| `kdos-shell` | yes | The panel being enabled |
| `kdos-desk` | yes | `desktop_icons` |
| `kdos-slit` | yes | `slit`, off by default |
| `kdos-notifyd` | **no** | always |
| `kdos-netagent` | **no** | always |
| `kdos-mediad` | **no** | always |
| `kdos-clip` | **no** | `clipboard` |

**Three are per output** because a layer surface is placed by the compositor on one screen, and
our toolkit has a single cell buffer per process — so a second monitor cannot be a second surface,
it has to be a second process. Each takes the output name as an argument.

**Four are single-instance** because each owns something unique: the notification daemon owns a bus
name that a second instance would simply fail to take; the secret agent registers one agent with
NetworkManager, and a second would be a second passphrase box for the same question; the media
handler holds one subscription to the mount daemon, and a second would offer every stick twice; and
the clipboard owns a socket and holds its history in memory, so a second instance would be a second
history nobody could reach.

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

### A boxed application's clipboard on the console

**The console runs one compositor per embedded APPLICATION.** A boxed application there is a guest
inside a `kdos-cage` of its own, so the seat it copies onto is that cage's and nobody else's: one
clipboard for every window that application maps, invisible to the terminal beside it and to the
next box. A copy in a GIMP dock therefore pastes into its image window with nothing crossing this
channel at all — the two windows are one client on one `wl_display`. The session is the only
clipboard the console and a box can both see, which is why the cage mirrors the selection rather
than arbitrating it — `kdos-con` owns the bytes exactly as `kdos-comp` owns them in the other
session.

**Both halves are descriptors on the cage's event loop.** A guest's copy is read out of a pipe as
the guest writes it, and a paste inside the guest is answered from the copy the cage is already
holding. A transfer that waited for a client, in either direction, would stop the loop that also
pumps frames — the window would freeze because somebody pressed Ctrl+C in it.

**Text, and at most sixty-four kilobytes of it.** What crosses is the first of
`text/plain;charset=utf-8` and `text/plain` the guest offers, which is also what XWayland maps
`UTF8_STRING` and `STRING` to; a guest offering only an image has copied something this desktop
has nowhere to put, and the selection is left as it was rather than emptied by a copy nobody could
paste. The bound is `KEMBED_CLIP_MAX`, which **is** the constant the session's own clipboard
truncates at rather than a copy of it, so a selection is cut once or not at all. It travels in a
sealed descriptor, and one that is not a file of the length it claims, or that can still be shrunk
under the mapping, is dropped rather than mapped.

**The session's end reads the offer and pushes the mirror.** `take_clip()` answers a
`KEMBED_CLIP_OFFER` by measuring the descriptor and handing the bytes to the session's own
clipboard — the same store the console terminal's selection writes. `send_clip()` goes the other
way: it seals the clipboard and the primary once per change and sends both as `KEMBED_CLIP_SET` to
every cage that is behind, once per channel per pump and after the drain, so a copy one guest made
this turn reaches the others in the same turn. A cage is brought level again when a window is
adopted, because a guest advertises what it can paste the moment its first window takes the
keyboard and a cage that has not been told is an application whose Paste item is grey.

**The comparison is the selection's generation and not its bytes.** A guest that has just offered
its own copy is one generation behind by construction, so the mirror hands that copy straight back
and the cage recognises what it last offered and does nothing with it — which is what keeps a copy
from cancelling itself a moment after it was made. Comparing bytes instead would cost a full
compare per channel per pump to answer the same question.

**A drag crosses on the same channel, and the session owns it in both directions.** The five
`KEMBED_DRAG_*` and `KEMBED_DROP` ops carry the half a toolkit needs: `ENTER` with the type when the
pointer arrives over an embedded window, `MOTION` while it stays, `LEAVE` when it goes and `DROP`
with the payload on release — converted to the guest's own pixels like every other position here.
The cage turns them back into `wl_data_device` events by creating a real `wlr_drag` and driving its
pointer grab, which is the only way the guest's own refusal of a type still means something; see
[`kdos-cage`](../04-programs/kdos-cage.md).

**The other direction is read at the START of the guest's drag and not at the release.**
`KEMBED_DRAG_OFFER` carries the type and a sealed descriptor the moment a guest picks something up,
because the session has to be carrying the payload before the pointer leaves that window — fetching
it at the drop would need a round trip through another process with a person's hand already off the
button. It is the channel's and not a window's: one cage is one seat, so a drag begun in a dock and
one begun in an image window arrive under `win` 0.

**Two types and no others**, `text/plain` and `text/uri-list` — the same pair the session accepts
from its own surfaces, refused in the one place that decides what a drag on this desktop may be. A
guest dragging an image out has begun something this desktop has nowhere to put, and no offer is
made rather than one whose bytes nothing can read.

### A boxed application's scale on the console

**The console tells a boxed application how dense it is, and the number is the cell.** The cage
gives each window a headless output and `KEMBED_SCALE` gives that output a scale factor, so a
toolkit told 2 lays its window out in half as many logical pixels and draws each of them twice as
large. Nothing else moves: the output mode, the shared framebuffer and the cell rectangle the window
occupies are all the same numbers at any scale, which is why the scale can be raised on a live
window at all.

**The cell is where the density is written**, because it is the only place that is true of every
display a session can have. A text grid has no physical size to ask about; what it has is the font
the view picked for the screen it is on. So the session divides the primary view's cell height by
the reference cell — eight by sixteen — and a console whose characters are twice that asks its
guests for 2, which is also what makes a person who chose larger text get larger windows.

**It is a whole number, and one that divides BOTH halves of the cell.** A logical size is the
output's pixel size divided and truncated, so a scale that does not divide the cell leaves the guest
a column short of its own output — the cage's background down the edge of the window for as long as
it lives. The session steps the number down until it divides, so a cell one and a half reference
cells tall is a console at 1 and so is a thirty-two-pixel cell of odd width. **Fractional scale is
never offered to a guest.**

**On every shipped configuration that number is 1.** `con.conf` names `font = monospace:size=12`,
whose cell is the reference cell itself, and 2 wants a cell thirty-two pixels tall and even in
width — text stepped up with `Super+=` until the characters are twice their shipped size, or a
`font =` written that large. Both ends of the mechanism are built and every size that crosses the
channel is converted for it; on the shipped font that conversion is an identity, and no shipped font
crosses the mark. [known-gaps](../06-reference/known-gaps.md) records it.

The box environment carries nothing about this: the scale reaches the application through its
compositor, which is what every toolkit already reads it from.

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

**What the engine can be switched to is a shipped file**, `~/.config/fcitx5/profile`, and **which
screen its candidates land on is a shipped decision**: one kimpanel owns `org.kde.impanel`, the two
desktops share one session bus, and the graphical session takes the name from the console's panel
while it runs. Both are in
[the candidate window](../04-programs/kdos-shell.md#the-candidate-window).

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
| *(no `GTK_THEME`)* | Deliberately absent: that variable overrides the theme setting for the life of the process, so an accent switch could never reach a running GTK application. The name arrives through the settings portal and through the seeded `gtk-3.0/settings.ini` instead |
| `GSETTINGS_BACKEND=keyfile` | No settings daemon is reachable |
| `QT_IM_MODULE=wayland` | Input methods through the compositor |
| `CUPS_SERVER` | The host's print socket, when it exists |
| `NO_AT_BRIDGE`, `GTK_A11Y` | A default, not a policy — see below |
| `DISPLAY` | Added when Xwayland is running, for X11-only applications |
| `LIBGL_ALWAYS_SOFTWARE=1` | Added only where the box profile's `render` key resolves to software — a profile that refuses the card, a machine with no render node, or a box with no `/dev/dri` in it because `devices = private` meets `gpu = no`. The hardware answer exports nothing: Mesa asks the right question already and falls back on its own |

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
