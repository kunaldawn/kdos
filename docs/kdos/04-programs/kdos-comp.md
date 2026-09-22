# kdos-comp

`kdos-comp` is the KDOS compositor: a frozen hard fork of labwc 0.20.0 carrying sixteen KDOS
grafts. It is the Wayland server for the whole session, it supervises the desktop's own chrome, and
it answers questions from other KDOS programs over two sockets of its own.

The case for forking an existing compositor rather than writing one is set out in
[Decisions](../01-philosophy/decisions.md).

## Overview

| | |
|---|---|
| Binary | `kdos-comp` |
| Upstream configuration | `~/.config/kdos-comp/rc.xml` and `menu.xml`, both shipped from `/etc/skel` |
| Generated theme | `~/.config/kdos-comp/themerc-override`, written by `kdos theme` |
| KDOS configuration | `~/.config/kdos/comp.conf` |
| Built with | meson, out of the port directory — there is no source tarball to fetch |
| Log | `$XDG_RUNTIME_DIR/kdos-comp.log` |

Upstream's source was imported wholesale, rebranded, and is never merged from again. `KDOS-FORK`
at the root of the port records the upstream tarball and its checksum; upstream's licence and
copyright headers are kept in place.

Because this is a fork rather than a patch set, upstream's documentation for `rc.xml` applies
verbatim. Window management, key and mouse bindings, window rules, theming keys and menus are all
labwc's, and labwc's manual pages are the reference for them.

### Finding the KDOS additions

Two conventions keep the additions greppable. KDOS code lives in `src/kdos-*.c` — sixteen files —
plus one shared header, `include/kdos.h`. Upstream files carry minimal hooks marked with the
comment `/* KDOS */`; grep for that marker to find every touch point. There are 21 such files, and
`main.c` holds seventeen of the markers on its own.

| Graft | What it adds |
|---|---|
| `kdos-config.c` | The KDOS configuration file and its reload |
| `kdos-child.c` | Supervised chrome children |
| `kdos-wallpaper.c` | The wallpaper, as a scene node rather than a client |
| `kdos-crt.c` | The phosphor pass |
| `kdos-frames.c` | The late-frame reporting socket |
| `kdos-idle.c` | The dim, lock and power-off idle policy |
| `kdos-lid.c` | Laptop lid behaviour |
| `kdos-cmd.c` | The command socket other KDOS programs query |
| `kdos-thumb.c` | A window's pixels as a file, for hover previews |
| `kdos-peek.c` | Fading windows to reveal the desktop |
| `kdos-appid.c` | Recording the application identifiers windows actually present |
| `kdos-boxchip.c` | The box colour chip on a title bar |
| `kdos-grant.c` | Per-box grants on the sandbox allowlist |
| `kdos-group.c` | Window grouping, for the panel's window list |
| `kdos-layerfocus.c` | Click-away for on-demand layer surfaces |
| `kdos-winpos.c` | Window placement decisions |

### What the compositor does not decide

The compositor owns no window-model arithmetic. Where a new window lands, what a tiled state
becomes and what rectangle it occupies, which edge a moving edge stops against, and how workspace
stepping skips empty workspaces all come from `libkwm`, which is asserted against a fixture with
no compositor running. Every one of those rules can therefore be checked without booting anything.

What stays here is what only a compositor can do: walking its own view list, asking the decoration
how thick it is, working out which edges are actually visible, and deciding how a drag feels as it
crosses one. See [The window model](../03-architecture/window-model.md).

## Configuration

Configuration is split in two, and the split is strict. `comp.conf` holds only the KDOS keys;
everything else belongs in `rc.xml`.

An old-style binding, startup entry, workspace or mouse line written into `comp.conf` is ignored
and logged by name, pointing at `rc.xml`. So is any other unrecognised key, and so is a key with
an empty value. A setting that quietly does nothing cannot be told apart from a typo.

### Keys applied on reload

These are re-read and applied immediately when the compositor is reconfigured. `kdos theme`
already sends the signal that triggers a reload.

| Key | Default | Does |
|---|---|---|
| `wallpaper` | `/usr/share/backgrounds/kdos/default-wallpaper.png` | Path to an image, or `none` |
| `crt` | `55` | Phosphor pass strength, per cent. `0` turns the pass off |
| `crt_scanlines` | `0` | Scanline depth |
| `crt_curve` | `0` | Barrel distortion |
| `crt_fullscreen` | `on` | Whether the pass runs over a fullscreen window |
| `idle_dim` | `300` | Seconds of inactivity before the screen dims |
| `idle_lock` | `600` | Seconds before the session locks |
| `idle_off` | `900` | Seconds before outputs are powered off |
| `lid_close` | `suspend` | `suspend`, `off`, or anything else to ignore the lid |
| `window_memory` | `yes` | Whether an application opens where its window last was |

A path value may be written `~/…` or `$HOME/…`; both are expanded. Boolean values accept
`yes`/`no`, `true`/`false`, `on`/`off` and `1`/`0`, and a value that is none of those is reported
rather than guessed at.

### Keys applied at the next login

Each of these is part of a supervised child's command line, so changing one takes effect when that
child is next started. A reload reports the change by name and then restores the running value,
because a configuration structure that disagrees with the running chrome is how a later reader
concludes the setting does not work.

| Key | Default | Does |
|---|---|---|
| `panel` | `bottom` | `bottom`, `top` or `off` |
| `panel_cells` | `2` | Panel height in cells, 1 to 4 |
| `panel_font` | `Terminus:pixelsize=20` | The bar's own font pattern; empty follows `chrome_font` |
| `panel_autohide` | `no` | Whether the bar hides when the pointer leaves it |
| `panel_margin` | `0` | Gap between the bar and the screen edge, 0 to 64 pixels |
| `panel_opacity` | `80` | Bar opacity, per cent, floored at 20 |
| `desktop_icons` | `yes` | Whether the desktop icon surface runs |
| `slit` | `no` | The dockapp column |
| `clipboard` | `yes` | The clipboard history daemon |
| `icons` | `yes` | Whether chrome draws pictures at all |
| `chrome_font` | `Terminus:pixelsize=32` | The font every KDOS surface draws with |
| `clock_format` | `%H:%M` | The panel clock's `strftime` format |

`panel_opacity` is floored at 20 rather than 0. A bar at zero is not a see-through bar; it is a bar
that is not there, with every control on it invisible and the pointer still hitting it, and no way
back except editing this file from a virtual terminal.

The two font keys are written by `kdos-style` as well as by `kdos-settings` and by hand. The
picker's Font page grafts the chosen family onto each key, keeps the size that key already carried,
and leaves every other line of `comp.conf` verbatim. It changes nothing in the compositor process:
the face it loads is in its own window, and the compositor passes `--font` to a supervised child
when it spawns one, so the desktop wears the new family as each surface is next started.

### Files whose existence is the setting

Two files ship absent, and absent is a working default for both.

| File | Enables |
|---|---|
| `~/.config/kdos/session-restore` | Reopening the previous session's windows |
| `~/.config/kdos/a11y` | The accessibility stack inside boxes |

`~/.config/kdos/favorites` has the same shape but ships populated with a handful of pinned entries.
An empty list makes both the quick-launch row and the Start menu's pinned column look broken on a
freshly booted machine; delete every line if you want an empty one. An identifier with no matching
desktop entry is skipped silently, so an application this image's catalogue does not carry leaves
no launcher that opens nothing.

## Bindings

### The one line that must not be lost

`<default />` must be the first child of both `<keyboard>` and `<mouse>` in `rc.xml`. It is the
most load-bearing line in this distribution's configuration.

The compositor loads its built-in bindings only when your file defines none of that kind, so a
file that binds a single key throws every default away. What goes with them: click-to-focus, the
title-bar drag, the window buttons, border resize, the root menu, window cycling, close, and the
snap arrows. On a running system the symptom is "the mouse does not work", and it is invisible to
a compile, to the recipe parser and to XML validation. Overrides go after `<default />`, because
the later of a duplicate pair wins.

`testing/preflight.sh` fails a shipped `rc.xml` that gets this wrong.

### `--` may not appear inside an XML comment

`rc.xml` documents itself in prose, and prose about a desktop names command arguments. A single
`--app-id` inside a `<!-- -->` makes the whole document ill-formed, and a compositor that cannot
parse its configuration loads none of the bindings in it. Nothing about the running system says
so: the chords are not there, one by one, in whatever order somebody happens to try them.
`testing/preflight.sh` parses the shipped file with a real XML parser for exactly this reason.

The shipped bindings are listed in [The desktop](../02-user-guide/desktop.md).

## Decorations

The window frame is generated from the same palette as everything else, into an override file the
compositor reads over its built-in theme, so frames retint live with the panel and the shader. The
reasoning is in [the design language](../03-architecture/design-language.md); the mechanics are
these.

- `<cornerRadius>0</cornerRadius>` and a two-pixel border in `rc.xml`.
- A custom title-bar texture rendering the same double rule the cell grid draws with. The title-bar
  fill is one pixel wide and stretched, so anything varying only vertically costs nothing — and a
  double horizontal rule varies only vertically.
- The title and every button take the plain background instead. A rule behind a word is a word
  struck through, and a rule behind the minimise button, which is itself a horizontal line, is a
  button with no readable state.
- Button glyphs are small bitmaps enlarged by a whole number with nearest-neighbour filtering.
  Upstream's resize path only ever shrinks, so the glyphs would otherwise be composited at their
  own few pixels in the middle of a large button.
- The hover plate carries an alpha. There are no hover icons: the plain image is copied and a
  colour laid over it, so an opaque colour paints the symbol out and leaves every button blank
  under the pointer.

The title-bar font must name a scalable face. Pango does not render bitmap fonts, so naming the
bitmap console font resolves and then silently falls back to a generic sans for every title bar and
every menu. The shipped `rc.xml` names the TrueType Terminus at 24 points — 32 pixels at 96 dpi —
so a title bar is exactly one cell tall. A machine without that font falls back to Noto Sans, which
`56-noto-preferred.conf` puts at the head of `sans-serif`.

Telling the two apart takes a measurement rather than an eye. Count luminance levels in a
screenshot: bitmap text has three and no midtones, while an antialiased face has well over a
hundred.

The menu width cap defaults to a value sized for a small font, which at 32 pixels is about eleven
characters, so menu entries arrive truncated. The generated theme raises it. A cap only ever
truncates, so a generous one costs a short menu nothing.

## The prompt command

`<core><promptCommand>` is `kdos-prompt`. The compositor's conditional action spawns the prompt
command and dispatches on its exit status: zero takes the affirmative branch, a specific code means
cancelled, and anything else takes the negative branch.

Upstream's own prompt program is not built here (`-Dlabnag=disabled`), so the facility exists with
nothing on the other end of it unless something supplies one. `kdos-prompt` is `kdos-shell` under
another name and answers with those codes. It is what lets ending the session, restarting and
shutting down ask before they act.

## Frame pacing and the output mode

Nothing in the compositor sets a frame rate. The only thing that draws is the output's frame event,
raised by the backend when that output is ready for another frame; the handler composites once and
returns. There is no timer, no sleep and no period anywhere on the path, so the rate is the mode's
rate — a 144 Hz panel gets 144 frames a second for the same reason a 60 Hz one gets 60. An output
with nothing to redraw takes the scene's early-out instead of a frame, which is why an idle desktop
costs nothing. A fixed period would be a floor on that idle cost in one direction and a ceiling on
a fast panel in the other.

Two rate limits do exist and neither binds. The interactive-resize path emits at most one configure
per refresh interval, read from the output's own mode. The shutdown collapse steps at 16 ms, while
the event loop is being pumped by hand after the display has already stopped.

The mode is chosen resolution first, then the highest rate that will commit. The preferred mode —
the panel's EDID-preferred timing — fixes the resolution only; every mode at that resolution is
then tried in descending order of refresh rate, and the first that passes its test commits. Taking
the preferred mode's own rate is the trap, because panels routinely advertise 60 Hz as the
preferred timing and 120 or 144 elsewhere in the same mode list, and the session would then sit at
60 with nothing in the desktop presenting it as a choice. Descending order is what makes this safe:
a rate the link cannot carry fails its test and the next one down is tried, so the preferred mode
remains reachable, and the fallback to a lower resolution when none of them commits is untouched.

Two things outrank this, both deliberately. A mode a client asks for through the
output-management protocol is tested exactly as asked, which is how a user pins a rate. And
`reuseOutputMode` in `rc.xml` keeps a mode that is already set ahead of any of this, which stops a
handover from re-modesetting a working screen.

## The phosphor pass

The compositor renders the desktop through a shader: scanlines every third physical row, a
three-tap horizontal bleed, a vignette, optional barrel distortion, and a faint phosphor floor so
black is never quite black. It is on by default at 55 per cent, with scanlines and curvature off.

Scanlines ship off because they are the one part of the effect that argues with what is underneath
it. The desktop is a grid of 16×32 cells, and a dark line on every third physical row lands across
the glyphs at a period nothing on the screen shares, so text drawn two-colour and crisp arrives
striped. `crt_scanlines = 60` is the strength for anyone who wants them.

### How it is implemented

There is no shader API in the underlying library. The rendering pass offers textures and
rectangles, and the scene graph has three node types and no callback node. What the library does
offer is a documented seam: the scene's build-state call accepts an options structure with a custom
swapchain field. So the scene composites into a buffer of ours, and KDOS code blits that buffer
into the output's real buffer with the effect applied. Both swapchains come from the library's own
configuration call, so neither needs format or modifier guesswork.

Four constraints bind the implementation.

- Direct scanout is turned off for the whole session while the pass is on. When the scene takes
  that path it hands the commit a client's buffer plus a destination rectangle — not a picture of
  the desktop — and the pass would stretch, say, a panel over the whole screen. The library exposes
  exactly one switch, an environment variable read when the scene is created, so the pass sets it
  before the scene exists. A foreign buffer arriving anyway is committed unprocessed rather than
  mangled.
- The texture is imported per frame and destroyed after the pass. Caching it per swapchain slot is
  the obvious optimisation and it deadlocks the swapchain: importing locks the buffer, a slot is
  only reused once its last lock goes, and a full set of cached textures means no free output
  buffer and a scene that stops rendering.
- Two fallbacks exist and neither can produce a black screen. A renderer that is not the GL one
  gets no pass at all — software rendering with a fullscreen post-process is a slideshow — and that
  is reported at startup. Anything that fails at run time puts that output on the ordinary commit
  for a cooldown: five seconds, doubling per consecutive failure to a minute, reset by a pass that
  completes. Sixty identical error lines a second is worse than missing scanlines, and a permanent
  give-up would untheme a screen for the whole session over one hotplug renegotiation.
- The magnifier takes the frame instead, whole. The magnified inset is drawn inside the call the
  pass replaces, so with the pass on a magnified frame loses the inset whenever the scene redraws
  and keeps it whenever the scene is static — a flicker between two different pictures. The pass
  declines while the magnifier is enabled, which is also the right answer on its own: an
  accessibility zoom read through scanlines is harder to read, not easier.

The curvature is normalised by the corner displacement, so no value crops the desktop.

### What it costs

The pass is not what bounds the frame rate. Timed off-screen at the shipped defaults on a GeForce
RTX 4060, one pass costs 0.033 ms at 1920×1080, 0.060 ms at 2560×1440 and 0.138 ms at 3840×2160 —
against 0.018, 0.033 and 0.070 ms for a plain blit of the same buffer. The effect itself is roughly
the cost of moving the pixels again, and even the 4K figure is two per cent of a 144 Hz frame
budget.

The lever the pass gives back is `crt_fullscreen = off`, which is one render instead of two for
video and games. It is a battery setting rather than a frame-rate one.

Colours come from the shared palette and the accent is re-read on reload, so a theme change
retints the running shader in the same signal that repaints the panel.

### Inspecting it without a screen

`KDOS_CRT_DUMP=<prefix>` writes the composite and the result once; `KDOS_CRT_DUMP_FRAME=<n>` waits
past the empty first frame. The input is read back through the texture the shader sampled rather
than the buffer, because that buffer is not always readable.

One property only hardware can confirm. The composite and the pass share one GL context, so
ordering is free, but the commit relies on implicit buffer fencing. Getting that wrong shows up as
a torn frame rather than as an error.

## The wallpaper

The wallpaper is drawn by the compositor, not by a client: one scene buffer per output at the
bottom of the tree, rebuilt when the output layout changes.

The usual answer is a layer-shell client, and it is the wrong one here. The toolkit paints cells,
so a wallpaper client would be the one program in this desktop that is not a character grid.

Three details are load-bearing. There is no public way to make a buffer from memory, so the file
carries the smallest buffer implementation that works. The PNG decoder hands back one channel order
and the buffer format wants another. And the decoder reports a bad file by jumping back after the
allocation, so only an initialised buffer may leave the decode path.

The image is scaled to cover and centred. `wallpaper = none` is an honest off.

## Idle, dim, lock and lid

One timer drives three stages — dim, then lock, then outputs off — each measured from the last
activity rather than from the previous stage. Activity ends the dim and powers outputs back on; it
never unlocks. An idle inhibitor stops the policy dead.

The hook is the single activity funnel every input path already goes through, plus one call in each
inhibitor handler. The dim is a translucent scene rectangle raised to the top, with lock surfaces
re-raised above it. It is not a gamma change.

All three timers default to zero in a virtual machine unless any `idle_*` key is set, because a
blanked screen over a remote display is indistinguishable from a crashed compositor.

The compositor owns the locked state, not the lock client. The session stays locked when the lock
program dies without unlocking: the lock surfaces keep covering every screen, and a new lock client
may replace the abandoned one, which is exactly the recovery needed after a crash. A lock screen
that unlocks when it crashes is the failure the protocol exists to remove.

## The frames socket

The compositor writes one line of structured data per late frame to a socket in the runtime
directory. This is what makes [`kdos stutter`](kdos-command.md) possible.

Presentation events are the truth where the backend has them: they carry when content actually
turned into light, plus the refresh interval. Headless and nested backends do not present, so the
fallback is the frame clock, and the two are reported with a source field rather than averaged. A
presentation gap is what the user saw; a frame gap is what the compositor was given.

The threshold is one and a half refresh intervals. Each report includes the compositor's own render
cost, which is the field that separates the two explanations: a cost that is a large fraction of
the frame budget means the desktop itself was late, which is the one causal claim the tooling
makes.

The socket must never slow the frame loop, so it is non-blocking at both ends and a consumer that
cannot keep up loses lines. There is no history either: a consumer that connects late has missed
what happened, and a ring buffer would hide that.

This is deliberately not a Wayland protocol. It is one distribution's channel between two of its
own programs, and the standard presentation protocol already exists for clients that want their own
numbers.

## The command socket

Other KDOS programs ask the compositor questions over a second socket in the runtime directory.
[`kdos hey`](kdos-command.md) is the command-line front end. Six verbs are answered.

| Verb | Answers |
|---|---|
| `list` | Every window: identifier, title, geometry, state, workspace, box and instance |
| `outputs` | The outputs and their scales |
| `boxes` | The distinct boxes that currently have a window on screen |
| `thumb` | A window's pixels, written to a file |
| `peek` | Fade the windows to reveal the desktop |
| `run` | Execute something |

This is Haiku's shape: the window manager answers questions from the command line, so a window is
something a script can find and act on. It is also how the box garbage collector asks whether a box
still has a window before stopping it.

## Window thumbnails

A client cannot see another client's buffer, so the compositor renders a window's contents to a
file on request. That is what the panel's hover preview is made of.

The preview is rendered as solid blocks off the palette's brightness ladder rather than through the
shape-matching character renderer. A large window sampled into a small grid is many source pixels
per cell, so there is no shape left to match and every textured cell picks the same character. The
shape matcher keeps its other callers, which run at grids where a cell still holds a shape.

The preview is never required. No compositor, no socket, a client whose pixels are not readable, a
file that does not parse — every one of those leaves the tooltip the two lines of text it was
always going to be.

## Box identity

The compositor knows which box a window came from, because `kdos-boxsock` tags every client from a
box with a security context naming it.

For an X11 window it reads the process environment instead. Such a window's Wayland client is the X
server, running on the host with no context, so the context lookup answers nothing for every
X11-only application in the catalogue. The window does carry its client's process id, and every
process a box starts carries an identifying variable, so that is the fallback, cached per window.

The box chip is a square of the box's accent colour at the left of the title area. Four rules
govern it.

- A box wearing the session's own accent draws no chip. The colour comes from the box's profile and
  nothing else, so a default install has none, and the first chip appears exactly when somebody
  gives a box a colour to tell it apart by.
- Its width is added to the title's left offset, in the single place where both the title's
  wrapping width and its position are computed. A chip drawn at a coordinate of its own is correct
  until a title grows long enough to run under it.
- It is rebuilt rather than patched. The title update fires on every resize and every title change,
  so a chip that were merely added would stack one per frame of a drag.
- The profile is read once per window and dropped on reload, because an accent switch changes which
  windows wear a chip.

The colour is scaled down on an inactive window: the chip is an identity, not a focus indicator.

## Supervised children

Seven programs are started from a table in `kdos-child.c` and respawned if they die. Three are
started once per output; four own a single bus name, socket or subscription and are started once
for the session.

| Child | Per output | Started when |
|---|---|---|
| `kdos-shell` | yes | `panel` is not `off` |
| `kdos-desk` | yes | `desktop_icons` |
| `kdos-slit` | yes | `slit` |
| `kdos-notifyd` | no | always |
| `kdos-netagent` | no | always |
| `kdos-mediad` | no | always |
| `kdos-clip` | no | `clipboard` |

The four single-instance children are single for concrete reasons. `kdos-notifyd` owns
`org.freedesktop.Notifications`, which is one bus name, and a second instance would fail to
take it. `kdos-netagent` registers one agent with NetworkManager on the system bus, and a second
would raise a second passphrase box for the same question. `kdos-mediad` holds one subscription to
`kdos-mountd`, and a second would offer every removable device twice. `kdos-clip` owns one socket
in `$XDG_RUNTIME_DIR` and holds the history in memory, so a second instance would be a second
history nobody could reach. The per-output rules and the session bring-up around them are in
[The session](../03-architecture/session.md).

A child gets every ignored signal disposition back before it is executed. Ignored dispositions
survive execution exactly as the signal mask does, so a session started under a wrapper that
ignores a signal would hand that down to every program it starts — and the live retint, which is
delivered as a signal, would never fire.

Upstream's own spawns keep their double fork: a terminal opened by a key binding is not the
desktop's chrome.

## Xwayland

Xwayland is run rootlessly by the compositor, so X11-only applications inside boxes work. It is
built without GLX, because the graphics stack here is built without X11 platform support, so X
clients get no OpenGL. See [Principles](../01-philosophy/principles.md).

The compositor exports the display variable only to what it spawned itself, so `kdos-appbox` probes
for the socket and adds the variable to a box's environment on its own.

## Shutdown

Termination signals are handled through the event loop. The graft teardown runs in order — the
wallpaper, the frame reporter, the idle policy, then the phosphor pass — before the server is
destroyed. The panel and the notification daemon notice the dead compositor themselves.

## Debugging

| Variable | Effect |
|---|---|
| `KDOS_COMP_DEBUG=1` | Raise the log level; every dropped frame is logged as well |
| `KDOS_CRT_DUMP=<prefix>` | Write the phosphor pass's input and output once |
| `KDOS_CRT_DUMP_FRAME=<n>` | Wait until frame *n* before dumping |

The fork logs at an informative level by default. Upstream's default is errors only, with which
the graft layer's decisions are invisible and an empty session log looks like a hung session.

## See also

- [The session](../03-architecture/session.md) — what starts the compositor, and what it starts
- [The desktop](../02-user-guide/desktop.md) — the bindings and the user-facing behaviour
- [The window model](../03-architecture/window-model.md) — the arithmetic `libkwm` owns
- [The design language](../03-architecture/design-language.md) — why the frame looks like that
- [kdos-shell](kdos-shell.md) — the chrome it supervises
- [The kdos command](kdos-command.md) — `kdos hey` and `kdos stutter`
- [Configuration](../06-reference/configuration.md) — every key above, with defaults
