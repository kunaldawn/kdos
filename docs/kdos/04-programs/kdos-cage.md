# kdos-cage

One application, full screen, on a virtual terminal of its own. A hard fork of
[cage](https://github.com/cage-kiosk/cage) 0.3.1, MIT, built on the same `wlroots 0.20` the
compositor fork pins.

It is **not the compositor**. It has no window management, no workspaces, no panel feed and no
phosphor pass, and it is not meant to grow any: it holds one client, across a whole terminal or —
under `--embed` — as a picture per toplevel for a parent that owns the window model.
Why that is a second program rather than a mode of `kdos-comp` is in
[decisions](../01-philosophy/decisions.md#a-second-fork-for-the-kiosk-not-a-mode-of-the-first).

## What it is for

The console desktop composites **character cells**; a Wayland client's surface is **pixels**. There
is no way to put one inside the other. So a graphical application launched from the console gets a
VT to itself, this holds the display there, and the console desktop stays exactly where it was.

`kdos-con` is what allocates the terminal and starts this on it — see
[kdos-con](kdos-con.md#a-wayland-application-on-the-console) for the ordering and why it is that
ordering.

## What the fork changed

Only what a person sees is rebranded: the binary, the usage text and the version line, which prints
both names because "a fork of cage 0.3.1" is what somebody chasing a wlroots incompatibility needs
to read. **Upstream's internal names are left alone** — `cg_server`, `CAGE_HAS_XWAYLAND` and the
rest — because a fork whose identifiers stop matching upstream's is a fork nobody can read a
security fix against.

Three additions and two removals:

| Change | Why |
|---|---|
| `security-context-v1` is advertised | `kdos-boxsock` tags a box's socket against it, so a boxed application reaches this desktop through the same `kdos-appbox` launch and not a second one. No policy hangs off the tag: one client, nothing to protect it from |
| A background in the palette's deep colour | From `libkcolor`, the only KDOS library it links and linked statically. Unpainted scene is black, and a black rectangle in the middle of a phosphor screen reads as a dead display rather than as a program starting |
| The background is resized on every layout change | One sized at creation stops covering the screen the moment a monitor is plugged in |
| The man page and its `scdoc` dependency are gone | This system documents itself here, and a second description of the same flags is a second thing to keep true |
| The wlroots subproject fallback is gone | This tree builds against its own pinned wlroots; a silent fallback download is not something a reproducible build may do |

**XWayland stays.** A boxed application may be an X11 client, and the rootless X server is this
distribution's one carve-out.

## `--embed`: the guest's pixels, without a screen

A graphical application on the console can be a **window** rather than a whole terminal, and this is
how. The mechanism is two wlroots choices and nothing else:

- the **headless backend**, whose outputs are buffers in memory rather than screens, and
- a **renderer whose frames this process can read**, because bytes are all the parent gets.

**Which renderer that is, the machine decides.** `WLR_RENDERER` is left unset, so
`wlr_renderer_autocreate()` tries **gles2**, then **vulkan**, then **pixman**, and skips each
hardware attempt where no DRM render node can be opened — which is exactly the "is there a usable
card" question, asked by the code that has to answer it anyway. virtio-gpu publishes a render node
only where the host offered virgl, so the plain `make run` lands on pixman with nothing decided
here and nothing to fail.

**The renderer is the guest's graphics stack, which is why the card is the default.** A pixman cage
advertises neither `linux-dmabuf` nor `wl_drm`; Mesa inside the box then finds `wl_shm` and nothing
else, and answers that by loading llvmpipe — no hardware GL, no hardware video decode and every
frame drawn on the CPU, on a machine whose render nodes, DRI drivers and `libva` are all present.
Software is the right answer only where the hardware road is not there: it draws into memory a
pointer reaches directly, needs no device, no driver and nothing negotiated, and works on a machine
whose graphics stack is broken.

**`KDOS_EMBED_GPU` overrides in both directions**, carried in by the session from the box profile's
`render` key; the cage reads the variable and never a profile, and the session forwards the key's
value unread so that both directions cross — see
[kdos-con](kdos-con.md#the-guest-on-a-terminal-of-its-own). `software` —
and `pixman`, `no`, `off`, `0`, `false` — pins the software renderer for a guest that should not
touch the card; `gpu`, `auto` and an absent key are the default above. **A value nobody recognises
means the default and not the software renderer**, because a typo that silently costs a guest its
hardware GL is a fault nothing reports. It is its own key and not the profile's `gpu`, which says
the render nodes are bound into the box and which every box carries.

**The hardware renderer needs an allocator of its own**, and that is what makes it possible rather
than merely faster. `wlr_allocator_autocreate()` picks gbm the moment the renderer has a DRM descriptor,
and a gbm buffer can be neither mapped nor turned into a shared-memory handle — every frame would
render and none could be published, leaving a permanently black window with each part reporting
success. So `embed_allocator()` takes **udmabuf**: a buffer that is a DMA-BUF the card draws into
*and* a file the kernel backs with ordinary memory, which is the only allocation that serves both
ends. wlroots' own udmabuf branch cannot be reached here, because it is guarded on there being no
DRM descriptor at all.

**Every step of that falls back to software rather than failing**, because a half-succeeding
hardware path is a black window and a software path is a working one. No render node, a missing
`/dev/udmabuf` — its rule is `fs/etc/udev/rules.d/70-kdos-udmabuf.rules`, group `render`, the same
group that owns the render node — or an allocator that cannot produce a readable buffer each cost
the guest the card and nothing else.

**And the frame is read by whichever road the allocator left open.** A direct pointer is asked for
first because it is free; a udmabuf buffer implements `get_shm` and `get_dmabuf` and *not* data-ptr
access, so the shm handle is mapped for the frame instead. A cage that knew only the first road
would publish nothing at all the moment the renderer stopped being the software one.

**The hardware road reads memory the card may still be writing.** A gles2 pass on a headless output
ends in a bare `glFlush()` — wlroots allocates a signal timeline only for a backend that has a DRM
descriptor and the headless backend has none — so the copy has nothing of its own to wait on. The
DMA-BUF descriptor beside the memfd is the same memory and the only handle the kernel synchronises
on: the publish polls it for `POLLIN`, which is its implicit write fence, and brackets the copy with
`DMA_BUF_IOCTL_SYNC` START/END, the cache maintenance a CPU mapping of memory a device wrote needs.
Without the pair, a block tears against the frame still being drawn, worst exactly when the card is
busiest, and the fault reads as a flaky compositor rather than as a race. An shm buffer has no
DMA-BUF handle and no GPU writer, and takes neither step.

**The fence wait is bounded at 100 ms and never infinite**, because it happens inside the cage's
event loop: a card that never signals would otherwise stop the guest's input and every message to
the parent for as long as it stays wedged. A frame whose fence misses that deadline is skipped
rather than published half-drawn — the slot is not flipped, so the parent goes on showing the frame
before it.

**Skipping needs a frame to fall back to, and into a fresh mapping there is none.** A resize remaps,
and so does start-up — the mapping is made before the guest exists, so the first frame it ever
publishes lands in one too. Either way the parent holds zero-filled pages and is showing none of
them until something is copied in, so a missed deadline there copies the frame unwaited instead: the
guest's next draw corrects a torn frame and nothing corrects a skipped one. The scene has already
subtracted that damage, so a guest with nothing further to redraw never asks for another frame, and
the window would stay blank for as long as it is open.

What the wait costs is latency: on the hardware road a publish is as late as the card is slow, and
the frame reaches the parent after the readback rather than before it.

The backend is chosen through the environment wlroots already reads, and the renderer by leaving
that environment alone unless a profile overrides it, so the code path is upstream's own. `WLR_HEADLESS_OUTPUTS=0` goes with them: autocreate adds headless
outputs of its own accord and at a size of its choosing, and this mode makes every output it wants
by hand. The allocator is the exception and is built by hand for the reason above.

## One window, one output, one mapping

**One mapped toplevel is one headless output, one `memfd` and one window on the parent's desktop.**
An application is as many windows as it maps toplevels — GIMP is a toolbox, an image window, two
docks and a file dialog — and each of them has to be a KDOS window a person can move, raise, close
and find in the taskbar.

**A scene output renders only what is inside its own layout box**, and that is the whole mechanism:
toplevels placed in disjoint boxes reach the parent as separate pictures. Sharing one output
composites them into one framebuffer *before* the parent sees a byte, and nothing downstream can
take them apart again — the information is gone at `wlr_scene_output_build_state()`.

**The outputs are placed by hand, on a grid, and never `auto`.** wlroots re-packs auto-configured
outputs left to right on every add, remove and resize, so opening or closing one window would move
every other window's origin — every scene position, every pointer coordinate and every popup's
unconstrain box — underneath the guest, mid-gesture. Each window takes the lowest free span of
`CG_EMBED_SPAN` pixels, which is also the largest window this mode will make: a window that could
grow past its span would reach into the box beside it, and two overlapping boxes put one window's
pixels into another window's framebuffer. A size past the span is refused rather than clamped, so
the guest is never told one size while the parent scales its frames as another. **Four spans across
and four down, so the grid holds sixteen windows**, because an X11 client is placed by its *origin*
in *signed 16-bit* root coordinates: the sixteenth span starts at 24576 and the seventeenth would
start past 32767, where no Xwayland guest can be told where its window is. The scene background is
sized to the whole grid once and never resized, so a span past the last one renders against nothing
for a Wayland guest too. **A seventeenth toplevel is refused an output rather than placed outside
the grid**: it gets no `KEMBED_OPEN`, so it is not a window on the parent's desktop at all, and its
scene node is disabled so that it renders nowhere rather than into the box the first window owns. An
output made anyway would render, copy a whole framebuffer and publish for the life of the process
for a window the parent has already refused and nothing can close. `kdos-con`'s own ceiling on
windows per channel is the same sixteen, and the two must agree.

**The first ordinary window takes the output this cage started with, and that output is never
destroyed.** `--embed WxH` makes one headless output before the backend starts; it is maximised into
by the first toplevel that has **no owner and no role bit** — an xdg toplevel claims it at its
*initial commit*, an XWayland one at its *map*, and whichever gets there first takes it. So a guest
that shows one window allocates nothing, is configured at the size the parent forked it with, and
behaves exactly as a single-output cage does. **Ordinary**, because the parent hands its remembered
rectangle for that application to the first such window too: a GIMP splash maps before anything else
and, taking the anchor, would be sized to the application's remembered window while the parent framed
the image window behind it at that same rectangle, with neither end telling the other. A cage that
made its first output lazily would sit with none at all for as long as the guest takes to start — a
client that waits for a `wl_output` before mapping never maps, Xwayland's root screen stays 0x0 where
no X client can map either, and `output_destroy()` terminates a server whose output list empties.
Outputs for every other window are created at the map from the toplevel's own size and destroyed at
the unmap; the anchor is released back to the next ordinary window instead.

**A second toplevel is configured 0x0 at its initial commit — "you choose".** It has no output yet,
and a size named there is a size the client takes for the parent's window. The size it picks instead
is its **natural size**, read at the map where it has committed a buffer and its geometry is real,
and carried out in `KEMBED_OPEN`. That is what lets the console open a dialog at dialog size instead
of full screen. Its own geometry first, the committed surface size where the client set no window
geometry, and zero — "none" — where it has neither.

**This end owns no window model.** Where a window sits, how large it is, what is above what, what
has the keyboard and what a role means are all the parent's. Each toplevel fills its own output and
nothing here centres, cascades or maximises across a union — the union box maximises every parentless
toplevel onto one rectangle, which is five GIMP windows in one place pixel for pixel, and with one
output per window it would maximise each of them across all of them. The one shape decision left is
that a toplevel naming an **owner** is sized and not maximised: maximised is what strips a toolkit's
shadow and rounded corners from a window that fills its frame, and on a dialog it adds a restore
button to something no desktop shows one on.

**Popups, tooltips and override-redirect surfaces are not windows.** An `xdg_popup` is built into
its toplevel's scene tree, so it moves with it, renders into that window's framebuffer and is
published inside its frames — no `win`, no output, no taskbar entry. Its unconstrain box is **its
toplevel's own output** and never the output under a point: a popup whose origin falls outside every
output resolves to `NULL`, and `wlr_output_layout_get_box(NULL)` is the *whole* layout, so a menu
would be unconstrained across every window this guest has and composited into the one beside it. An
X11 override-redirect surface — a menu, a tooltip, a splash, a drag icon — names its own place in
root coordinates, which **are** the output layout, so honouring them lands it inside the box of the
window that raised it; it gets no output and no `win` either. Get that wrong and every Java splash
and every GTK-under-X tooltip becomes a taskbar entry.

**A window that nobody can see stops rendering, and that is per window.** `KEMBED_SLEEP` names one,
so a guest whose docks are on another workspace stops drawing them and goes on drawing the one in
front of the person. One shared output draws every toplevel whenever any one of them draws.

**A resize is an output resize**, so the guest reconfigures the way it would on any compositor.
**The parent gates that channel**: a guest is told at most one size per window per frame period, and
only once a new mapping for that window has come back — whatever size that mapping carries, because
a size the cage folded into an earlier one is a size no mapping will ever report. A mode change here
is a swapchain, a fresh `memfd` and a relayout inside the application, and a drag would otherwise
emit one per pointer motion. A wait that reaches a second stops holding the next size back as well;
a guest that never remaps at all would otherwise hold the channel shut for the life of the window.

**The output is negotiated and the surface is reported, and they are two different sizes.** A Wayland
client is free to commit a window smaller than the output it was given — a dialog that will not be
stretched — or larger — a window with a minimum of its own. The first is composited at the output's
top left over this process's background, so the parent is handed a frame the size it asked for with
a band of the scheme's darkest slot down two sides of it; the second is cut off at the output's
edge. The framebuffer is the output's either way, so `KEMBED_BUF` can only echo the size the parent
chose and no correction derived from a frame can ever see the gap. `KEMBED_SURFACE` is what closes
it: the guest's own window geometry, which the parent rounds **up** to whole cells, gives to the
window and asserts back as a `KEMBED_SIZE` — so the two ends agree to within the cell the rounding
added, and for a Wayland guest neither the band nor the crop outlives one round trip.

**The output carries a scale, and it is the only number on this channel that is not pixels.**
`KEMBED_SCALE` names how many real pixels the desktop spends on one of the guest's logical ones, per
window because a window is an output. It is committed with `wlr_output_state_set_scale()` and the
MODE is left alone, so the framebuffer, the `memfd`, the blocks the parent cuts and the cell
rectangle the window sits in are all exactly what they were — what changes is the logical size
wlroots derives from the mode, which is what the toolkit lays its window out in and multiplies its
own drawing by. At 2 a guest draws everything twice as large into the frame it was already filling.
Without the number every boxed application would render at 1 on a console whose own text is twice
that, which is unreadable chrome beside legible text.

**So every size that crosses the channel is converted here, and a place is a size.** The natural
size in `KEMBED_OPEN`, the report in `KEMBED_SURFACE` and the pointer hint in `KEMBED_GRAB` are
multiplied out of logical units on the way to the parent; `KEMBED_MOTION`, `KEMBED_BUTTON`,
`KEMBED_REL` and `KEMBED_AXIS` are divided into them on the way in. `KEMBED_SIZE` is the exception
and is *not* converted: it names the output's mode, which is pixels on both sides. A position left
unconverted at scale 2 lands at half the distance from the window's corner that the person pointed
at, and a size left unconverted is a window the session makes half the size the guest asked for.

**The scale is a whole number that divides the console's cell.** wlroots divides the mode by the
scale and TRUNCATES, so an output whose pixel width is not a whole multiple leaves the guest
rendering a column short of its own output — a stripe of the cage's background down the edge of the
window for as long as the window lives. The session picks the number (see
[kdos-con](kdos-con.md)) and steps it down until it divides; this end refuses anything below 1 or
above `EMBED_SCALE_MAX`, which is 4. **A fractional density is therefore never asked for**: a
console one and a half times the reference steps down to 1.

**On the shipped console the number IS 1, and every conversion here is an identity.** The session
measures its view's cell against a reference of eight by sixteen, and `con.conf`'s
`font = monospace:size=12` is that reference cell; 2 wants a cell thirty-two pixels tall whose width
is even as well, which text stepped far up with `Super+=` or a much larger `font =` produces and no
shipped configuration does. Both ends are built and the multiplications above run on every message;
what is missing is a font that crosses the mark — see
[known-gaps](../06-reference/known-gaps.md).

**An X11 guest is upscaled rather than re-laid-out.** X has no scale factor: an Xwayland client
draws in logical pixels at one pixel each, and the scene magnifies its buffer onto the output. Its
chrome comes out the right SIZE and soft at the edges, which is legible where scale 1 is not.

**An X11 guest can neither band nor crop, and cannot be measured off a frame at all.** The cage is
the window manager, so a managed toplevel's geometry is whatever `wlr_xwayland_surface_configure()`
last set on it and Xwayland attaches a buffer of exactly that geometry: the committed surface is the
parent's own number coming back, on every frame the client will ever publish. What an X11 client has
instead is the `ConfigureRequest`, which wlroots delivers as the surface's `request_configure` signal
and drops if nothing is listening. The cage listens, and does two things with it. It answers the
request the way ICCCM 4.1.5 requires — `wlr_xwayland_surface_configure()` with the geometry the
window already has, which emits the synthetic `ConfigureNotify` that a redirected request never gets
from the X server, because a client left waiting on a notify that is not coming sits unresized and
unpainted for good. And it reports the size that was asked for as `KEMBED_SURFACE`, which is the
only way a dialog with a size of its own gets that size from the session.

**It is sent on a change, not on a frame.** Because the parent rounds up, a window whose pixels do
not land on a cell boundary is permanently a few pixels short of its output — reporting per frame
would be a message per frame for the life of nearly every window — so the cage records what it has
said and repeats itself only when the guest picks a different size or the parent moves the output.

**A Wayland report waits for the guest to have answered the last size it was given; an X11 one has
nothing to wait for.** A guest rendered between being told a size and answering it still measures the
size it is about to stop being, and honouring that would put the window back where it was for the
length of a drag. xdg-shell answers by acknowledging a configure, so an empty configure list — with
no configure still waiting on the idle that sends it — is the client having caught up, and its
committed geometry is then a size it chose. A `ConfigureRequest` has nothing in flight behind it, so
it is reported as it arrives; it is also reported straight out of the signal rather than at the next
frame, because a client asking to shrink has nothing new to draw and that frame is never coming.

**A guest sees as many `wl_output`s as it has windows open**, and a toolkit that enumerates them
reads them as monitors. That is the cost of the mechanism and there is no way to hide it from a
client that asks; nothing in a normal application's path is affected, because each toplevel is on
exactly one of them and is told the size of that one.

**A drag from one of its windows into another is one seat's drag inside one compositor**, and it
works with no help from this channel at all — the toolbox and the image window are two windows on
the parent's desktop and one client on one `wl_display`.

**Two frames in one mapping.** The child renders into the half the parent is not reading and then
flips. Single buffering tears on every commit, and on a photograph that reads as the compositor
being broken rather than as the timing artefact it is.

**A frame is published only when the scene says there is one, and an EMPTY damage region is
nothing rather than everything.** The two branches of the frame handler are not symmetrical and
both halves of that cost a screen. `wlr_scene_output_commit()` — what the non-embed branch calls —
returns early on `!wlr_scene_output_needs_frame()`; `wlr_scene_output_build_state()`, which the
embed branch calls to reach the buffer, has no such guard, so the embed branch tests it itself or
renders, copies a whole framebuffer and publishes on every tick of the headless output whether or
not a client has committed anything. And `wlr_scene_output_build_state()` ALWAYS sets the damage
field, with the scene subtracting afterwards what it committed — so an idle frame arrives as a
region that is present and empty. Reading either as "no damage information" and falling through to
the whole-window box tells the parent that every pixel has changed, fifty times a second, and the
parent then re-cuts and re-sends every block — about two megabytes a frame for a half-screen guest.

**And that gate is what makes a published frame mean the guest is alive.** The parent clears its
close deadline on one, because an application that answers "save your work?" inside the window it
was asked to close maps nothing else the parent can see. A cage that published on its own tick
would clear that deadline for a guest whose event loop has stopped, and the window nothing can
close is back.

**The damage that is sent is the region's own boxes, not the one box that contains them.**
`KEMBED_FRAME` announces the flip and the first box; every further box follows immediately in a
`KEMBED_DAMAGE`. The parent rounds each box out to the blocks it touches — at most sixteen cells
square, and smaller where a block that size would not fit one message — and re-cuts every one of
those, so the difference between the boxes and their bounding box is the difference between sending
what changed and sending the window: a page scrolled with a clock ticking in the title bar is two
small rectangles at opposite corners whose extents are everything. A region cut into more than
sixteen pieces is collapsed to its extents instead, because past that the messages cost more than
the blocks they save — most of the pieces land in blocks another piece already named.

**And nothing is published at all until a client has mapped a window.** The scene's background
rectangle is created with the server, so the first headless frame is a whole window of the scheme's
darkest slot and it goes out within milliseconds of the fork — long before a container has come up
and the guest exists. The parent cannot tell that black from a black an application drew, so the
window would read as a program that started and then did nothing for the half-minute the box takes.
With no frame at all the parent knows it is still waiting and says so; see
[`kdos-con`](kdos-con.md).

**The shared-memory descriptor is the one thing this channel has that the published protocols must
not.** It is passed child to parent over a socketpair inherited across the fork — never over a path
anything can connect to. That is what keeps the surface and view protocols forwardable over `ssh`.
See `kembed.h`, which is the whole protocol: a struct, and no serialisation, because both ends are
one build on one machine and the socket is `SOCK_SEQPACKET`.

**Every op names a window, and zero is the channel.** A guest is as many toplevels as it maps and
each one is a window on the parent's desktop, so `win` says which. Zero belongs to the channel
itself — the hello, the exit status, the keymap, the selection — and on an op that addresses a window
it names the front one. A mapping is one window's and never the channel's: it is announced under the
`win` it belongs to and every flip of it names that window, so a window that redrew costs one message
about itself and a window that did not costs none. Each end answers an op on the window `win` names
and **drops** one naming a window it does not have, closing any descriptor that op carried, because
a window can go while a message about it is in flight and a descriptor left open is one leaked per
frame.

**`KEMBED_OPEN` is what makes a toplevel a window**, and it precedes every op naming it. It carries
the natural size, the owner's `win` — zero for a window of its own — the role bits and the title, at
the map and never before. Ids come from this end, start at 1 and are **never reused**: the parent
keys a window on the pair (channel, `win`), and an id handed out twice is a frame drawn into
whichever window claimed it first. `KEMBED_CLOSE_WIN` retires one and is the last op that may name it.

**The roles are what a placement and a stacking order are made from**, and only one of them crosses
from a Wayland client. xdg-shell's toplevel state carries maximised, fullscreen, resizing, activated,
suspended, tiled, constrained and sizes — there is no modal, no utility and no splash in the protocol
to report — so a Wayland toplevel is a `DIALOG` when it names an owner and nothing when it does not.
X11 has all three, in `_NET_WM_STATE` and `_NET_WM_WINDOW_TYPE`, so `MODAL`, `UTILITY` and `SPLASH`
arrive from an Xwayland guest alone and the parent's rules degrade to "owned or not" without them.

**`KEMBED_SURFACE` is the guest's own idea of how large its window is**, in pixels, sent only when
that is not the size of its output. Nothing else on this channel can carry it — see the resize rule
above — and the parent answers it with a `KEMBED_SIZE` of the cell rectangle it rounded up to.

**`KEMBED_CLOSE` naming a window asks that toplevel and only it.** A person clicking the X on an
export dialog has not asked the application to quit. Zero asks every toplevel the guest has.

**A message may be longer than the struct**, and that is how a name travels. The kernel frames every
datagram, so a string after the struct needs no length field and no codec — the boundary is its end.
`KEMBED_TITLE` carries the guest's window name that way, cut on a UTF-8 sequence boundary because a
byte cut is a lead byte the parent's grid can only draw as a replacement mark. Both ends receive
into a buffer the size of the struct plus `KEMBED_TAIL_MAX` for the same reason the receive is a
`recvmsg`: a datagram that does not fit the buffer is truncated by the kernel with no error
anywhere, and a receive of exactly the struct would take a truncated tail for a whole message and
would drop the descriptor the kernel had attached. **A descriptor that arrives is closed whatever
the op did with it** — one left open per message is a cage that runs out of them.

**`KEMBED_CLOSE` asks the toplevel; it does not end the display.** Terminating the display takes the
application down with it, so a guest with unsaved work loses it to a close button it was never
shown. With nothing mapped there is nobody to ask and the display ends at once. The deadline and the
escalation are the parent's, because the parent owns the process and this end owns only the
protocol — and they are per *process*, so a dialog that ignores a close is never what signals the
application.

**What the guest does with the ask is visible to the parent in three ways, and drawing is one of
them.** The toplevel unmaps and `KEMBED_CLOSE_WIN` goes out; or a question opens in a toplevel of
its own and `KEMBED_OPEN` does; or the guest draws the question inside the window it was asked
about and only a `KEMBED_FRAME` does. The third is what a libadwaita `AdwDialog` is, and what every
Electron application does, and it maps nothing at all — so a parent that watched for new toplevels
alone would signal such a guest while the person was still reading the question. What is left when
none of the three arrives is a guest whose event loop is not running, and that is what the deadline
reaps.

**So an application that goes on drawing and never honours a close keeps its window through the
ask.** It is alive and it is being asked, and a desktop that took a live program's unsaved work away
to satisfy a click would lose more than it saved. **What ends such a window is the person asking a
second time**: ten seconds after an ask the parent offers the force on its bar, and a close taken up
on that offer drops the window and signals this process — `SIGTERM`, so this end unwinds its own
client the way it does for any other shutdown, and `SIGKILL` after it. A guest that answered once
and wedged afterwards is reached by the next click, which arms a fresh ask against a guest that has
stopped answering. The ladder is the parent's and is written out in
[kdos-con](kdos-con.md#a-graphical-application-is-windows).

**A guest whose last window closes does not end.** A `GApplication` holding the session bus name with
no window open is exactly what the shared bus is for: the next launch hands off into it and opens a
window there, which arrives as another `KEMBED_OPEN` on the channel that is already up. Nothing is on
screen and nothing is in the taskbar, which is what every other desktop does, and the backstop
against a forgotten process is the box collector.

**And the wait for the guest is bounded.** The event loop blocked SIGINT and SIGTERM to make its
signalfds, so a cage sitting in an unbounded `waitpid()` cannot be signalled out of it — it would be
a window on the parent's desktop that nothing can close and a box that nothing can collect. The
guest gets three seconds, then SIGTERM, then two more, then SIGKILL.

**A fullscreen request is the parent's to answer.** `set_fullscreen()` sizes the toplevel to *its own
output*, and in this mode that output is the parent's *window* — so a request honoured here alone
hands a video player back the rectangle it already had. `KEMBED_FULLSCREEN` names the window and
carries the state out; the parent makes that KDOS window fullscreen, the output follows the window,
and the client is configured again at the size of the screen. A request that arrives before the
toplevel maps has no `win` to travel under, so it is held on the window and goes out immediately
behind its `KEMBED_OPEN` — a player launched fullscreen would otherwise come up windowed.

**And fullscreen comes back the other way, because the parent decides it too.** `Super+f` is the
parent's chord and the output follows the window either way, but a guest that is never told keeps
the layout it drew for a window: Firefox holds its toolbars across the whole screen, and a player
that fullscreened itself and was put back draws its fullscreen chrome inside a small one.
`KEMBED_FULLSCREEN_SET` is that direction, and it names the window: a fullscreen asked of the image
window does not reshape the dialog in front of it. **It always names a window**, because a box
launched into a window the console already has fullscreen would otherwise be told before the guest
has execed, connected and mapped anything. The parent holds that state and re-offers it every turn
until the window it is for has an id, so the message crosses the moment the toplevel maps and
nothing is held on this end — a guest that came up windowed inside a fullscreen window would draw
its small-window chrome across the screen for the life of that window.

### Input with no input device

A headless backend has no keyboard and no pointer. Input arrives from the parent and is injected
through the paths a real device already takes — the pointer finds the surface under it the same way
a real motion does, and the keyboard group runs the xkb state machine, so a key resolves against the
layout and a modifier held down is held down.

**A key is a switch and not a character.** The parent sends a press and a release as two messages
carrying an **evdev** code, so a guest holds `W` until the release arrives, repeats it from its own
keymap, and reads a modifier whose key produces no character at all. Anything resolved to a
codepoint on the way — which is what a cell desktop wants and what a text field is typed into — can
express none of that: no key is ever down, `Ctrl`+click is a click with nothing held, and a game
cannot be walked forward. **The code is not shifted by eight.** That offset is xkb's own and wlroots
adds it where it runs the state machine; a code shifted here would be shifted twice and every letter
the guest typed — Xwayland's included, because the X server reads the same keymap this seat sends —
would be the wrong one.

**A press with no release is a key held for ever**, so losing the keyboard releases every key the
guest still holds. The parent stops sending keys to a window it has taken the keyboard from, which
makes this end the only one that can end them.

**The keymap is the person's, not an assumption.** `KEMBED_KEYMAP` carries the compiled xkb text the
view's own keyboard is running, in a sealed descriptor because a keymap is tens of kilobytes and
this channel already passes one. A guest whose view has no keyboard of its own — a view inside
somebody's terminal at the far end of `ssh` has libinput nowhere — is never sent one and reads the
layout xkb builds from the environment, which is the US positions and is why an accented letter
typed there arrives as the letter printed on an American keyboard. A text that does not compile
leaves the layout already set: a wrong layout is a guest that types the wrong letters, and a cleared
one is a guest that ignores every key.

**The locks and the layout group arrive as a resync and never per key.** No key stream can establish
them — Caps Lock was pressed before this process existed — so `KEMBED_MODS` sets them by mask at
focus-in, on a keymap change, and **behind** a key that left the locks or the layout group somewhere
other than where that window was last told they were. Behind such a key and never ahead of one:
ahead, the guest applies the key on top of the mask and toggles the lock straight back off. A session
that holds no mask of its own sends none. xkb's own rule is that a state driven by keys must not also
be set by mask: the two then disagree about which keys are down, and the next key resolves under the
wrong one. It is applied to the keyboard **group's** own object rather than to the seat, or this
keyboard's state disagrees with what the client was told.

**No virtual-keyboard or virtual-pointer protocol.** Those exist so a client can inject into a
compositor it does not own; this one is being driven by its own parent over a channel nothing else
can reach.

**A keyboard object is still created**, and that is not a contradiction: without one the seat has no
keymap to send, and a client with no keymap ignores every key. It is a keyboard group with no
keyboards in it — a real `wlr_keyboard` whose implementation drives nothing.

**And the seat announces a pointer it does not have.** A client binds a pointer only if the seat
says there is one, so a seat whose input is injected by its parent would advertise nothing and every
click would be delivered to a client that never asked to receive any. There is no pointer *device*
to make and none is needed: the cursor is warped and the ordinary motion path runs.

**`WLR_NO_HARDWARE_CURSORS=1` is set with the backend, and it is what makes the cursor visible at
all.** A headless output answers `set_cursor` and `move_cursor` with `true` and stores nothing, so
wlroots records a hardware cursor plane that does not exist and leaves the cursor out of the render
pass — and the parent, which reads the rendered bytes and has no plane of its own, gets a frame
with no pointer in it. The guest's cursor *shape* goes with it: an I-beam over a text field and a
hand over a link are how an application says what is under the pointer, and both arrive only as
pixels in the frame. This is the one of the four environment variables that is set rather than
defaulted — there is no headless cursor plane to prefer, so a person who set it to `0` set it for
some other compositor.

**And because this cursor is in the frame, it is the ONLY pointer inside the window.** The parent's
cell pointer is the cell under it reversed, and a reversed cell over an opaque guest frame is a
second pointer a cell from the first; the view therefore draws none over a cell whose picture keeps
being rewritten under it, which is what a cursor composited into every frame makes of the block it
is on — see the pointer contract in
[design-language](../03-architecture/design-language.md). A guest that hides its cursor is honoured
rather than overridden, which is what a full-screen player and a game ask for, and the cell pointer
comes back on the frame, one cell out, which is where the window is grabbed and resized anyway.

**Two things follow from a software cursor, and neither is a cost worth trading back.**
`wlr_output_is_direct_scanout_allowed()` returns false on this output for the life of the process,
which this mode requires rather than merely tolerates: the publish can read a data-ptr or an shm
handle and nothing else, so a client buffer handed straight to scan-out would publish nothing and
the window would be black with every part reporting success — "Direct scan-out disabled by software
cursor" in the log is the mode working. And every frame then goes through a full scene render, so
the cursor and the frame rate sit on one code path and have to be measured together.

**The pointer arrives in pixels, with the button evdev gave it and an axis that has two directions.**
A position is where the window was aimed and not which cell it fell in, which is the difference
between hitting a two-pixel scrollbar and not; the full button code is what drives Back and Forward
in a browser; and `KEMBED_AXIS` carries a continuous value, the high-resolution `value120` a modern
toolkit steps by, an axis index — horizontal exists — an axis source and whether the device reversed
the direction itself. Both numbers travel because a client reads one or the other and never both,
and a value of zero on both is the end of a gesture, delivered rather than dropped because it is
what stops a guest's kinetic scrolling. **An axis carries no position**: a scroll is not a place,
and the parent sends the motion first whenever the pointer moved, which is what keeps a detent over
a frame button a scroll rather than a click on it.

**A position cannot say how far the device moved**, so `KEMBED_REL` says it. The difference between
two positions is the pointer's speed after acceleration, clamping and the desktop's own cell grid,
and a guest reading relative motion asked what the mouse did. Outside a grab the delta arrives
first and the motion that follows spends it; there is no second motion event and nothing doubles.
Under a grab of either kind the parent sends no position at all, so the delta is spent where it is
received — a delta stashed there would wait for a motion that never comes, and the next one would
overwrite it.

**`KEMBED_LEAVE` is how the guest learns the pointer left**, and it is a cleared pointer focus and a
frame — never a motion to somewhere outside. The cursor warp clamps to the layout and the view
covers it, so an out-of-range position lands on a view edge and is delivered as an *enter*. A guest
never told keeps the link it last crossed lit and a button looking pressed for as long as it is
open.

### The keyboard, and the pointer the guest takes

**`KEMBED_FOCUS` is what activates and deactivates the guest.** Without it an unfocused boxed GIMP
keeps a blinking caret and activated chrome while the person types in a terminal, and a toolkit that
throttles when it is blurred never throttles. `KEMBED_FOCUS` **names the window**, and losing it
deactivates that view, releases every key still held and clears the seat's keyboard focus.

**The parent is the only source of keyboard focus in this mode, and this end never chooses one.**
Not on a map, not on a click, not on a destroy, and not on a guest asking its own
`wlr_foreign_toplevel` handle to activate. A dialog that took the keyboard here would take it from a
terminal the person is typing in, while the parent went on sending that terminal's keys and
releasing them against a window that never saw the presses. So `view_map()` does not focus what it
maps, `press_cursor_button()` does not focus what is clicked, `view_destroy()` does not hand the
keyboard to whatever is left and a `request_activate` raises the window without focusing it — the
parent decides where it goes and says so. A managed XWayland toplevel the parent names is focused
whatever window type it carries: wlroots' override-redirect guess answers no for a `UTILITY`, a
`SPLASH` and a `MENU`, and a GIMP dock under X11 is exactly `_NET_WM_WINDOW_TYPE_UTILITY` — dropping
the parent's instruction there leaves the guest with no keyboard focus at all while the parent
believes a window has it. A focus naming a window
this end does not have is **dropped** like any other op that names one: the two ends are out of step,
and the keyboard is left where it is rather than taken from a window that still has keys down.

**A guest may take the pointer, and the way out is the keyboard.** `wlr_pointer_constraints_v1` is
what a game and a three-dimensional editor lock or confine the pointer with, and it is advertised
**only in this mode**: this end can stop its own cursor and tell the parent to stop drawing its
arrow, while a cage on a terminal of its own has a real device whose motion it does not intercept —
a lock announced there would be a client told its grab took while the pointer kept moving. A
constraint activates only for the surface that has pointer focus *and* only while the parent says
this window has the keyboard, so moving the focus ends the grab. There is no break chord and no
escape verb, because a machine whose pointer can be captured with no way out is the failure that
this avoids.

**A lock does not move the pointer and a confine does.** `KEMBED_GRAB` tells the parent, which stops
moving its own arrow, stops changing which window is hovered and sends `KEMBED_REL` alone; both
kinds of grab get the delta through `wlr_relative_pointer_v1`. Under a **lock** that is all they
get: no enter and no motion, because the whole point is that the pointer did not move, and a warp
here would fight the lock and let the guest read the difference as motion of its own making. Under a
**confine** the guest asked only that the pointer not leave the region it named, so this end walks
the cursor by the delta itself — `wlr_region_confine()` against the region, offset by the view's own
position because the region is the guest's coordinates and the cursor is the layout's. A delta that
would carry the pointer out of the region is cut at the boundary, so motion along an edge does not
stop dead. **A pointer that is not inside the region has no boundary to cut against**, and nothing
else could put it back — the parent sends no position while a grab is held — so the cursor is moved
inside *before* the client is told the constraint took: to the hint where the guest named one, and to
the centre of the region otherwise. Without that a game confining to its viewport while the pointer
is over its own chrome would see no motion at all until the person moved the keyboard focus away.
Nothing else moves it: a walk navigation or a toolkit stopping a drag from leaving the window would
otherwise see no motion at all for as long as it held the confinement. The cursor hint the guest
sets — where it wants the pointer left when the grab ends — travels with the grab, in **that
window's** own pixels. **The window a constraint belongs to is resolved through the surface's root**:
the protocol matches a constraint against the surface the pointer is over, a toolkit that draws its
content on a subsurface hands that subsurface, and only a toplevel's root surface carries the view.
Resolved any other way the origin answers zero and the region is evaluated about the first span of
the grid — a guest in the second window confined to a rectangle a whole screen away.

**And for the duration, where the pointer is belongs to this end.** A `KEMBED_MOTION` is ignored
while a grab is held, and so is the position `KEMBED_BUTTON` carries, which is the place the guest
last saw rather than anywhere either kind of grab has since put the pointer. A delta stashed before
a grab began is dropped when it activates, or it is spent as a jump on the first motion after the
grab ends.

**Every coordinate on this channel is window-relative**, in both directions, and the conversion
happens once at each end. The top-left of window `win` is (0, 0); this end adds that window's output
origin on receipt, in `seat_embed_motion()` and nowhere else, and subtracts nothing on send because
the one outbound coordinate — the grab's cursor hint — is already the guest's own surface
coordinate. An origin added on the way out is one the parent would take off again, and the moment a
window's output sits anywhere but the layout origin it is a hint a whole screen away, with nothing
reporting an error: a game's pointer is simply confined to a rectangle somewhere else.

### The selection crosses, in a sealed descriptor

**The session owns the clipboard and this cage holds a copy of it.** On the cell desktop every boxed
application is a compositor of its own with a seat of its own, so a selection left where a guest put
it is one clipboard per *application* — a copy in the browser the terminal beside it cannot paste,
and a copy in the terminal the browser cannot see. Every window of one guest shares this seat and so
shares this clipboard, which is why a copy in a GIMP dock pastes into its image window with no help
from this channel. Both directions of the channel are here: `KEMBED_CLIP_OFFER` carries what the
guest copied up, and `KEMBED_CLIP_SET` installs what the session holds on this seat as a source of
this compositor's own, so a paste inside the guest is answered here with no round trip that could
hang the window doing it.

**The session's end answers both.** `kdos-con` reads `KEMBED_CLIP_OFFER` into the one clipboard the
console terminal's own selection writes, and sends `KEMBED_CLIP_SET` to every cage that is behind
the session's selection generation — so a copy made in one guest pastes in the terminal beside it
and in the next box, and a copy made on the console pastes inside a guest. `KEMBED_DRAG_*` and
`KEMBED_DROP` are the ops neither end implements; see
[Known gaps](../06-reference/known-gaps.md).

**Both carry a sealed `memfd`, for the reason the keymap does.** A selection chunked through a
32-byte message would be thousands of datagrams through the loop that also pumps frames, and the
receive buffer is sized for a name and not for a page of text. Sealed because the receiving end maps
it at the length it was told: a descriptor that could still be shrunk is one whose reader can be
faulted after the fact. Both ends check what arrives — a regular file of the length it claims, sealed
against shrinking, and within `KEMBED_CLIP_MAX`, which is the session's own bound and the same
constant `kdos-con` truncates at rather than a copy of it.

**Text, and the two names for it.** The session's clipboard holds bytes with no type beside them, so
the negotiation is the first of `text/plain;charset=utf-8` and `text/plain` the guest offers — which
is also what XWayland's own bridge maps `UTF8_STRING` and `STRING` to. A guest that offers an image
and nothing else has copied something this desktop has nowhere to put: the selection is left as it
was rather than replaced with nothing.

**A `KEMBED_CLIP_SET` carrying the bytes this cage last offered changes nothing here.** Every offer
is remembered and compared, because a session that mirrored a guest's own copy back at it would
destroy that guest's source and replace it a moment after the copy — which for a toolkit is its copy
being cancelled. And when a guest's own source dies — its document window closes, the toolkit clears
the clipboard, an Xwayland owner disowns `CLIPBOARD` — the seat is left offering nothing, so the
session's copy goes back on. Otherwise the guest could paste nothing at all until somebody copied
again somewhere else in the session.

**Nothing is transferred while this process waits for it.** Both halves are descriptors on the event
loop: a guest that offers a mime type and never writes, or asks for the selection and never reads,
would otherwise stop the loop that also pumps frames — the window freezing because somebody pressed
Ctrl+C in it. A pipe whose reader has left kills the writer and a pipe has no `MSG_NOSIGNAL`, so
`SIGPIPE` is ignored **process-wide**. An ignored disposition is inherited across an `exec`, so it is
set after the guest is forked and no earlier; every process this compositor execs from there on
carries it, Xwayland included — and Xwayland installs the same disposition itself, so nothing it runs
is changed by it.

### A video must not be covered by the screensaver

The guest's `zwp_idle_inhibit_manager_v1` inhibitor reaches this process's own idle notifier, and
this process has no idle policy: the saver, the lock and the DPMS clock all belong to the desktop
this window sits on. `KEMBED_INHIBIT` forwards it, so a video in a boxed player holds the screen
awake exactly as one in the graphical session does. Without it the saver arrives five minutes in
with nothing the application can do about it.

**It names the window the inhibitor is for**, because the parent's saver is per screen and its sleep
is per window — a player told to stop rendering has to be the one that asked the screen to stay
awake. A toolkit hangs its inhibitor on the video widget rather than on the toplevel as often as
not, so the surface is resolved to its root; where even that is not a window the front one answers
for it, because an inhibitor held against no window at all holds the screen awake nowhere.

### A window nobody can see stops rendering

`KEMBED_SLEEP` skips the render and the copy for **that window** while keeping frame-done flowing. A
client that never gets frame-done stops drawing and then never redraws when the window comes back; a
client that renders into a window nobody is compositing is spending a core on nothing. Because each
window has a scene output of its own, a dock behind another workspace produces no frames at all
while the image window in front of the person goes on producing them; a shared output would draw
every toplevel whenever any one of them drew.

## Options

```
kdos-cage [OPTIONS] [--] [APPLICATION...]
```

| Flag | Means |
|---|---|
| `-d` | Do not draw client-side decorations where that can be asked for |
| `-D` | Debug logging |
| `-m extend` / `-m last` | Extend across every output, or use only the last connected one. **Ignored under `--embed`**, where every output is one window: `last` disables all but the newest, which would blank every window but the one that opened last |
| `-s` | **Allow VT switching** |
| `-v` | The version, and what it is a fork of |
| `--embed WxH` | Render into a shared buffer instead of onto a screen, and take input from the parent. `WxH` is the **first** window's size, not a limit: it is the anchor output's, and further windows are made at their own. Started by `kdos-con`; the channel is fd 3 |

**`-s` is what `kdos-con` always passes**, and it is not optional there. Without it the kiosk
swallows the VT-switch chords, and a full-screen application nobody can leave — on a machine whose
desktop is on another terminal — is a wedged machine.

## What has been run

It builds through its own recipe and installs one binary. `ldd` shows wlroots, wayland-server,
xkbcommon and XWayland's xcb libraries, and **none of ours**: `libkcolor` and `libkbase` are static.
The self-test compiles every file in the port wherever wlroots exists, which is the only automated
check a fork of a library that breaks API every release can have.

**A guest HAS run in it, embedded, and reached a cell desktop.**
`testing/fixtures/embed/embedcheck.c` is the parent half as a test: it forks `kdos-cage --embed`,
takes the mapping over `SCM_RIGHTS`, waits for a frame with something in it and writes a PPM. A
keycode injected into a drawn frame changes a later one — which is the only thing a parent holding
pixels can observe about input having arrived, and it is enough, because nothing else moves in a
still frame.

It is a second process for the reason `decocheck` is: a headless output, a software renderer, a
memfd and `SCM_RIGHTS` are real kernel and library behaviours, and a mock would only assert about
itself.

Beyond it, `kdos-con` has run a guest as a window: the frames arrive as sprites and a view with no
pixels of its own prints them as characters, a click reaches the guest at the pixel inside the cell
it landed on, and snapping, workspaces and closing behave as they do for any other window.

`embedcheck` receives into a buffer the size of the struct, so the kernel discards the tail of a
`KEMBED_OPEN` and the op falls through the arms it does not know. What it observes is one window's
frames, which is what a single-toplevel guest has.

**No guest has run in it on a VT.** That path needs an ISO carrying this and a machine with real
terminals.

**And no MULTI-TOPLEVEL guest has been photographed.** A guest that maps five toplevels reaching five
KDOS windows needs a session driving the channel and a picture of what the person sees; the compile
gate is what stands behind it until then, and it is the same gate the rest of this fork has. The
same is true of a guest holding a key, reading its own layout or locking the pointer.

## See also

- [kdos-con](kdos-con.md) — the desktop that starts it, and how the terminal is allocated
- [kdos-comp](kdos-comp.md) — the compositor, and what this deliberately is not
- [Decisions](../01-philosophy/decisions.md#a-second-fork-for-the-kiosk-not-a-mode-of-the-first) — why a second fork
- [Known gaps](../06-reference/known-gaps.md) — what has and has not been run
