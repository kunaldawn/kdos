# kdos-comp

The compositor: a frozen hard fork of labwc 0.20.0 carrying the KDOS additions. This page covers
what is ours and how to find it, the configuration split, the phosphor pass, the sockets other
KDOS programs talk to it over, and how it supervises the desktop's own programs.

Why a fork rather than a compositor of our own is argued in
[Decisions](../01-philosophy/decisions.md).

## What it is

Upstream's source, imported wholesale, rebranded, and **never merged from again**. `KDOS-FORK` at
the root of the port records the upstream tarball and its checksum. Upstream's licence and
copyright headers are kept.

| | |
|---|---|
| Binary | `kdos-comp` |
| Configuration | `~/.config/kdos-comp/` for upstream's own files, `~/.config/kdos/comp.conf` for ours |
| Built with | meson, out of the port directory — there is no source tarball to fetch |
| Log | `$XDG_RUNTIME_DIR/kdos-comp.log` |

Because it is a fork rather than a patch set, **upstream's documentation for `rc.xml` applies
verbatim**. Anything about window management, bindings, mouse behaviour, theming keys or menus is
labwc's and is documented there.

### Finding what is ours

Two conventions make the additions greppable:

- **KDOS code lives in `src/kdos-*.c`** plus one shared header. Sixteen files.
- **Upstream files carry minimal hooks marked `/* KDOS */`.** Grep for that marker to find every
  touch point — they are concentrated in a handful of files, most of them in `main.c`.

| Graft | What it adds |
|---|---|
| `kdos-config.c` | The KDOS configuration file and its reload |
| `kdos-child.c` | Supervised chrome children |
| `kdos-wallpaper.c` | The wallpaper, as a scene node rather than a client |
| `kdos-crt.c` | The phosphor pass |
| `kdos-frames.c` | The late-frame reporting socket |
| `kdos-idle.c` | The dim/lock/off idle policy |
| `kdos-lid.c` | Laptop lid behaviour |
| `kdos-cmd.c` | The command socket other KDOS programs query |
| `kdos-thumb.c` | A window's pixels as a file, for hover previews |
| `kdos-peek.c` | Fading windows to reveal the desktop |
| `kdos-appid.c` | Recording the application identifiers windows actually present |
| `kdos-boxchip.c` | The box colour chip on a title bar |
| `kdos-grant.c` | Per-box grants on the sandbox allowlist |
| `kdos-group.c` | Window grouping support for the panel |
| `kdos-layerfocus.c` | Click-away for on-demand layer surfaces |
| `kdos-winpos.c` | Window placement decisions |

## Configuration

The split matters: **`comp.conf` holds only the KDOS keys**, and everything else is `rc.xml`.

An old-style binding, startup entry, workspace or mouse line in `comp.conf` is ignored **and says
so**, naming `rc.xml` — as is any other unrecognised key, because a setting that quietly does
nothing is indistinguishable from a typo.

### Live keys

Re-read on reload, and applied immediately. `kdos theme` already sends the signal that triggers
this.

| Key | Default | Does |
|---|---|---|
| `wallpaper` | the shipped image | Path, or `none` |
| `crt` | `55` | Phosphor pass strength, per cent. `0` is off |
| `crt_scanlines` | `0` | Scanline depth |
| `crt_curve` | `0` | Barrel distortion |
| `crt_fullscreen` | `on` | Whether the pass runs over a fullscreen window |
| `idle_dim` | `300` | Seconds to dim |
| `idle_lock` | `600` | Seconds to lock |
| `idle_off` | `900` | Seconds to power outputs off |
| `lid_close` | `suspend` | `suspend`, `off`, or ignore |
| `icons` | `yes` | Whether chrome draws pictures at all |
| `panel_opacity`, `panel_margin` | | Panel appearance |

### Startup-only keys

Each is a supervised child's **command line**, so changing one takes effect at the next login —
and the reload **reports that by name** rather than silently keeping the old value. The
configuration structure is restored to the running value too, because a structure that disagrees
with the running chrome is how a later reader concludes the setting works.

| Key | Default | Does |
|---|---|---|
| `panel` | `bottom` | `bottom`, `top` or `off` |
| `panel_cells` | `2` | Panel height in cells |
| `panel_font` | | The panel's font pattern |
| `panel_autohide` | `no` | |
| `desktop_icons` | `yes` | Whether desktop icons run |
| `slit` | `no` | The dockapp column |
| `clipboard` | | The clipboard history daemon |
| `chrome_font` | `Terminus:pixelsize=32` | The font every KDOS surface draws with |
| `clock_format` | `%H:%M` | |
| `window_memory` | | Remember window positions |

### Files whose existence is the setting

Two ship **absent**, and absent is a working default in both:

| File | Enables |
|---|---|
| `~/.config/kdos/session-restore` | Reopening the previous session's windows |
| `~/.config/kdos/a11y` | The accessibility stack inside boxes |

`~/.config/kdos/favorites` is a third file of this shape but **ships populated**, with a handful of
pinned entries. An empty list makes both the quick-launch row and the Start menu's pinned column
look broken on a freshly booted machine; delete every line for an empty one. An identifier with no
matching desktop entry is skipped in silence, so an application this image's catalogue does not
carry leaves no launcher that opens nothing.

## Bindings, and the one line that must not be lost

**`<default />` must be the first child of both `<keyboard>` and `<mouse>` in `rc.xml`.** It is
the most load-bearing line in this distribution's configuration.

The compositor loads its built-in bindings only when your file defines **none** of that kind, so
a file that binds one key throws every default away. What goes with them: click-to-focus, the
title-bar drag, the window buttons, border resize, the root menu, window cycling, close, and the
snap arrows. On a running system the symptom is "the mouse does not work", and it is invisible to
a compile, to the recipe parser and to XML validation.

Overrides go **after** it, because the later of a duplicate pair wins.

`testing/preflight.sh` fails a shipped `rc.xml` that gets this wrong.

The shipped bindings are listed in [The desktop](../02-user-guide/desktop.md).

## Decorations

The frame is generated from the same palette as everything else, into an override file the
compositor reads over its built-in theme — so frames retint live with the panel and the shader.
The full rationale is in [the design language](../03-architecture/design-language.md); the
mechanics:

- **`<cornerRadius>0</cornerRadius>`** and a two-pixel border in `rc.xml`.
- **A custom title-bar texture** rendering the same double rule the cell grid draws with. The
  title-bar fill is one pixel wide and stretched, so anything varying only vertically is free —
  and a double horizontal rule varies only vertically.
- **The title and every button take the plain background instead.** A rule behind a word is a word
  struck through; a rule behind the minimise button, which *is* a horizontal line, is a button
  with no readable state.
- **Button glyphs are small bitmaps enlarged by a whole number with nearest-neighbour filtering.**
  Upstream's resize path only ever *shrinks*, so the glyphs would otherwise be composited at their
  own few pixels in the middle of a large button.
- **The hover plate carries an alpha.** There are no hover icons: the plain image is copied and a
  colour laid over it, so an opaque colour paints the symbol out and leaves every button blank
  under the pointer.

**The title-bar font must name a scalable face.** Pango has not rendered bitmap fonts for many
releases, so naming the bitmap console font resolves and then silently falls back to a generic
sans for every title bar and every menu. The shipped `rc.xml` names the TrueType Terminus at 24
points — 32 pixels at 96 dpi — so a title bar is exactly one cell tall. A machine without that
font falls back to DejaVu Sans.

Telling those two apart takes a measurement rather than an eye: count luminance levels in a
screenshot. Bitmap text has three and no midtones; an antialiased face has well over a hundred.

**The menu width cap** defaults to a value sized for a small font, which at 32 pixels is about
eleven characters — so menu entries came out truncated. The generated theme raises it. A cap only
truncates, so a generous one costs a short menu nothing.

## The prompt command

`<core><promptCommand>` is `kdos-prompt`. The compositor's conditional action spawns the prompt
command and dispatches on its **exit status**: zero takes the affirmative branch, a specific code
means cancelled, anything else takes the negative branch.

Upstream's own prompt program is not built here, so the facility existed with nothing on the
other end of it. `kdos-prompt` is `kdos-shell` under another name and answers with those codes.
It is what lets ending the session, restarting and shutting down ask before they act.

## The phosphor pass

The compositor renders the desktop through a shader: scanlines every third **physical** row, a
three-tap horizontal bleed, a vignette, optional barrel distortion, and a faint phosphor floor so
black is never quite black. It is **on by default**.

**There is no shader API in the underlying library** — the rendering pass offers textures and
rectangles, and the scene graph has three node types and no callback node. What it does have is a
documented seam: the scene's build-state call accepts an options structure with a **custom
swapchain** field. So the scene composites into a buffer of ours, and our own program blits that
buffer into the output's real buffer with the effect applied. Both swapchains come from the
library's own configuration call, so neither needs format or modifier guesswork.

Four things it must get right:

- **Direct scanout is turned off for the whole session while the pass is on.** When the scene
  takes that path it hands the commit a *client's* buffer plus a destination rectangle — not a
  picture of the desktop — and the pass would stretch, say, a panel over the whole screen. The
  library exposes exactly one switch, an environment variable read when the scene is created, so
  the pass sets it **before** the scene exists. A foreign buffer arriving anyway is committed
  unprocessed rather than mangled.
- **The texture is imported per frame and destroyed after the pass.** Caching it per swapchain
  slot is the obvious optimisation and it **deadlocks the swapchain**: importing locks the buffer,
  a slot is only reused once its last lock goes, and a full set of cached textures means no free
  output buffer and a scene that stops rendering.
- **Two fallbacks, and neither can produce a black screen.** A renderer that is not the GL one
  gets no pass at all — software rendering with a fullscreen post-process is a slideshow — and it
  is reported at startup. Anything that fails at run time marks that *output* broken and returns
  to the ordinary commit for good, because sixty identical error lines a second is worse than
  missing scanlines.
- **The magnifier takes the frame instead, whole.** The magnified inset is drawn inside the call
  the pass replaces, so with the pass on a magnified frame lost the inset whenever the scene
  redrew and kept it whenever the scene was static — a flicker between two different pictures. The
  pass declines while the magnifier is enabled, which is also the right answer on its own: an
  accessibility zoom read through scanlines is harder to read, not easier.

The curvature is normalised by the corner displacement so no value crops the desktop.

Colours come from the shared palette, and the accent is re-read on reload, so a theme change
retints the running shader in the same signal that repaints the panel.

**Looking at it without a screen:** `KDOS_CRT_DUMP=<prefix>` writes the composite and the result
once, with `KDOS_CRT_DUMP_FRAME=<n>` to wait past the empty first frame. The input is read back
through the texture the shader sampled rather than the buffer, because that buffer is not always
readable.

One part only hardware can confirm: the composite and the pass share one GL context, so ordering
is free, but the commit relies on implicit buffer fencing. Getting that wrong shows up as a torn
frame, not an error.

## The wallpaper

Drawn by the compositor, not by a client: one scene buffer per output at the bottom of the tree,
rebuilt when the output layout changes.

The usual answer is a layer-shell client, and it is wrong here — the toolkit paints **cells**, so
a wallpaper client would be the one program in this desktop that is not a character grid.

Three things it has to get right: there is no public way to make a buffer from memory, so the file
carries the smallest buffer implementation that works; the PNG decoder hands back one channel
order and the buffer format wants another; and the decoder reports a bad file by jumping back
**after** the allocation, so only an initialised buffer may leave the decode path.

Scaled to cover and centred. `wallpaper = none` is an honest off.

## Idle, dim, lock and lid

One timer, three stages — dim, then lock, then outputs off — each measured from the **last
activity**, never from the previous stage. Activity ends the dim and powers outputs back on; it
never unlocks. An idle inhibitor stops the policy dead.

The hook is the single activity funnel every input path already goes through, plus one call in
each inhibitor handler. The dim is a translucent scene rectangle raised to the top, with lock
surfaces re-raised above it — not a gamma change.

**All three timers default to zero in a virtual machine** unless any `idle_*` key is set, because
a blanked screen over a remote display is indistinguishable from a crashed compositor.

**The compositor owns the locked state, not the lock client.** It stays locked when the lock
program dies without unlocking: the lock surfaces keep covering every screen, and a **new** lock
client may replace the abandoned one — which is exactly the recovery needed after a crash. A lock
screen that unlocks when it crashes is the failure the protocol exists to remove.

## The frames socket

One line of structured data per **late** frame, on a socket in the runtime directory. This is what
makes [`kdos stutter`](kdos-command.md) possible, and nothing else on any desktop has it.

Presentation events are the truth where the backend has them — they carry when content actually
turned into light, plus the refresh interval. Headless and nested backends do not present, so the
fallback is the frame clock, and the two are reported with a **source** field rather than averaged:
a presentation gap is what the user *saw*, a frame gap is what the compositor was *given*.

The threshold is one and a half refresh intervals. Each report includes the compositor's own render
cost, which is the field that separates the two explanations: over a large fraction of the frame
budget means *the desktop itself was late*, which is the one causal claim the tooling makes.

**The socket must never slow the frame loop.** Non-blocking at both ends; a consumer that cannot
keep up loses lines. **No history either** — a consumer that connects late has missed what
happened, and a ring buffer would hide that.

It is deliberately not a Wayland protocol: it is one distribution's channel between two of its own
programs, and the standard presentation protocol already exists for clients wanting their own
numbers.

## The command socket

Other KDOS programs ask the compositor questions over a socket in the runtime directory.
[`kdos hey`](kdos-command.md) is the command-line front end.

| Verb | Answers |
|---|---|
| `list` | Every window: identifier, title, geometry, state, workspace, **box** and instance |
| `outputs` | The outputs and their scales |
| `boxes` | The distinct boxes that currently have a window on screen |
| `thumb` | A window's pixels, written to a file |
| `peek` | Fade the windows to reveal the desktop |
| `run` | Execute something |

This is Haiku's shape: the window manager answers questions from the command line, so a window is
something a script can find and act on. It is also how the box garbage collector asks whether a
box still has a window before stopping it.

## Window thumbnails

A client cannot see another client's buffer, so the compositor renders a window's contents to a
file on request. That is what the panel's hover preview is made of.

The preview is rendered as solid blocks off the palette's brightness ladder rather than through
the shape-matching character renderer — a large window sampled into a small grid is many source
pixels per cell, so there is no shape left to match and every textured cell picks the same
character. The shape matcher keeps its other callers, which run at grids where a cell still holds
a shape.

**The preview is never required.** No compositor, no socket, a client whose pixels are not
readable, a file that does not parse — every one leaves the tooltip the two lines of text it was
always going to be.

## Box identity

The compositor knows which box a window came from, because `kdos-boxsock` tags every client from a
box with a security context naming it.

**For an X11 window it reads the process environment instead.** Such a window's Wayland client is
the X server, on the host, with no context — so the lookup answered nothing for every X11-only
application in the catalogue. The window does carry its client's process id, and every process a
box starts carries an identifying variable, so that is the fallback, cached per window.

The **box chip** is a square of the box's accent colour at the left of the title area. Four rules:

- **A box wearing the session's own accent draws no chip.** The colour comes from the box's
  profile and nothing else, so a default install has none — and the first chip appears exactly
  when somebody gives a box a colour to tell it apart by.
- **Its width is added to the title's left offset**, in the single place both the title's wrapping
  width and its position are computed. A chip drawn at a coordinate of its own is correct until a
  title grows long enough to run under it.
- **It is rebuilt, never patched.** The title update fires on every resize and every title change,
  so a chip that were merely *added* would stack one per frame of a drag.
- **The profile is read once per window**, and dropped on reload — because an accent switch changes
  *which* windows wear a chip.

The colour is scaled down on an inactive window: the chip is an identity, not a focus indicator.

## Supervised children

Five programs, started from a table in `kdos-child.c` and respawned if they die. The table, and
the per-output rule, are in [The session](../03-architecture/session.md).

**A child gets every ignored signal disposition back before it is executed.** Ignored dispositions
survive execution exactly as the signal mask does, so a session started under a wrapper that
ignores a signal would hand that down to every program it starts — and the live retint, which is
delivered as a signal, would never fire.

Upstream's own spawns keep their double fork: a terminal opened by a key binding is not the
desktop's chrome.

## Xwayland

Run **rootlessly** by the compositor, so X11-only applications inside boxes work. It is built
without GLX, because the graphics stack here is built without X11 platform support — so **X clients
get no OpenGL**. See [Principles](../01-philosophy/principles.md).

The compositor exports the display variable only to what *it* spawned, so `kdos-appbox` probes for
the socket and adds the variable to a box's environment itself.

## Shutdown

Termination signals are handled through the event loop. The graft teardown runs in order — the
wallpaper, the frame reporter, the idle policy, then the phosphor pass — before the server is
destroyed. The panel and the notification daemon notice the dead compositor themselves.

## Debugging

| | |
|---|---|
| `KDOS_COMP_DEBUG=1` | Raise the log level; every dropped frame is logged as well |
| `KDOS_CRT_DUMP=<prefix>` | Write the phosphor pass's input and output once |
| `KDOS_CRT_DUMP_FRAME=<n>` | Wait until frame *n* before dumping |

The fork logs at an informative level by default. Upstream's default is errors only, with which
the graft layer's decisions are invisible and an empty session log looks like a hung session.

## See also

- [The session](../03-architecture/session.md) — what starts it and what it starts
- [The desktop](../02-user-guide/desktop.md) — the bindings and the user-facing behaviour
- [The design language](../03-architecture/design-language.md) — why the frame looks like that
- [kdos-shell](kdos-shell.md) — the chrome it supervises
- [The kdos command](kdos-command.md) — `kdos hey` and `kdos stutter`
- [Configuration](../06-reference/configuration.md) — every key above, with defaults
