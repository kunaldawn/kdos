# kdos-cage

One application, full screen, on a virtual terminal of its own. A hard fork of
[cage](https://github.com/cage-kiosk/cage) 0.3.1, MIT, built on the same `wlroots 0.20` the
compositor fork pins.

It is **not the compositor**. It has no window management, no workspaces, no panel feed and no
phosphor pass, and it is not meant to grow any: it holds one client and shows it the whole screen.
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
- the **pixman renderer**, the software one, which can hand back a pointer to those bytes.

Both are selected through the environment wlroots already reads, so the code path is upstream's own.
`WLR_HEADLESS_OUTPUTS=0` goes with them: autocreate adds headless outputs of its own accord, and a
second output beside the one this mode wants makes the layout twice as wide as the window — which
is exactly what the frames then are.

**One window, one output.** A resize is an output resize, so the guest reconfigures the way it would
on any compositor; there is no second notion of "the window is smaller than the output".

**Two frames in one mapping.** The child renders into the half the parent is not reading and then
flips. Single buffering tears on every commit, and on a photograph that reads as the compositor
being broken rather than as the timing artefact it is.

**The shared-memory descriptor is the one thing this channel has that the published protocols must
not.** It is passed child to parent over a socketpair inherited across the fork — never over a path
anything can connect to. That is what keeps the surface and view protocols forwardable over `ssh`.
See `kembed.h`, which is the whole protocol: a struct, and no serialisation, because both ends are
one build on one machine and the socket is `SOCK_SEQPACKET`.

### Input with no input device

A headless backend has no keyboard and no pointer. Input arrives from the parent and is injected
through the paths a real device already takes — the pointer finds the surface under it the same way
a real motion does, and the keyboard runs the xkb state machine so a client sees modifiers.

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

### Minimised means stop rendering

`KEMBED_SLEEP` skips the render and the copy while keeping frame-done flowing. A client that never
gets frame-done stops drawing and then never redraws when the window comes back; a client that
renders into a window nobody is compositing is spending a core on nothing.

## Options

```
kdos-cage [OPTIONS] [--] [APPLICATION...]
```

| Flag | Means |
|---|---|
| `-d` | Do not draw client-side decorations where that can be asked for |
| `-D` | Debug logging |
| `-m extend` / `-m last` | Extend across every output, or use only the last connected one |
| `-s` | **Allow VT switching** |
| `-v` | The version, and what it is a fork of |
| `--embed WxH` | Render into a shared buffer at this pixel size instead of onto a screen, and take input from the parent. Started by `kdos-con`; the channel is fd 3 |

**`-s` is what `kdos-con` always passes**, and it is not optional there. Without it the kiosk
swallows the VT-switch chords, and a full-screen application nobody can leave — on a machine whose
desktop is on another terminal — is a wedged machine.

## What has been run

It builds through its own recipe and installs one binary. `ldd` shows wlroots, wayland-server,
xkbcommon and XWayland's xcb libraries, and **none of ours**: `libkcolor` and `libkbase` are static.
The self-test compiles all seven files wherever wlroots exists, which is the only automated check a
fork of a library that breaks API every release can have.

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

**No guest has run in it on a VT.** That path needs an ISO carrying this and a machine with real
terminals.

## See also

- [kdos-con](kdos-con.md) — the desktop that starts it, and how the terminal is allocated
- [kdos-comp](kdos-comp.md) — the compositor, and what this deliberately is not
- [Decisions](../01-philosophy/decisions.md#a-second-fork-for-the-kiosk-not-a-mode-of-the-first) — why a second fork
- [Known gaps](../06-reference/known-gaps.md) — what has and has not been run
