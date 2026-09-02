# Known gaps

What KDOS does not do. This page exists so you stop looking for something that is not there, and
so that a limitation is never discovered by assuming it is a bug.

Everything here is present tense. A gap that has been closed is not recorded — see
[Principles](../01-philosophy/principles.md#documentation-describes-the-present).

Where something is deliberately absent rather than merely missing, the reason is in
[Decisions](../01-philosophy/decisions.md); where it is planned, it is in [Roadmap](roadmap.md).

## Desktop

**Drag and drop carries text and files, and nothing else.** `text/plain` and `text/uri-list` are
offered and accepted; there is no MIME negotiation, no deferred transfer and no image payload.
Only the trash accepts a drop on the desktop — dropping onto a folder would be a move, and a move
that half-succeeds across filesystems is worse than not offering it. Both directions work between
a KDOS surface and a boxed application. See [Status](status.md) for what that rests on.

**The console desktop does not come up on its own.** `kdos-con` starts and is healthy — it binds
`con.sock`, `con.view` and `con.windows` and logs nothing — but no view ever attaches, so the screen
keeps the last thing the framebuffer console drew and the desktop is invisible. Every `kdos-view`
run against that socket exits inside a second having written a screen's worth of glyphs to **stdout**
and nothing to stderr, including the supervised one. `kkms_init()` is not the failure: a run against
a socket that does not exist prints only `cannot attach`, and the screen is taken before the attach
is tried. A view started by hand with `--tty` draws the desktop correctly, so the session, the
protocol and the drawing all work — what does not work is the view keeping the screen it took.
Reproduce with `testing/vnc-shot.py --no-session`, then `pgrep -c kdos-view`.

**A picture's default handler is a Wayland viewer, on both desktops.** `mimeapps.list` sends every
`image/*` to `imv`, which needs a compositor, so opening a picture from the console desktop's file
manager does nothing. `timg` is installed and is a terminal entry, so *Open With* reaches it and a
launcher row starts it; what is missing is a default that follows the desktop, and one
`mimeapps.list` for one user cannot express two.

**The key card never reads the console keymap.** `kdos-keys` parses `~/.config/kdos-comp/rc.xml`,
which is the compositor's file. On a console session the chords come from
`~/.config/kdos-con/keys.conf`, so a chord rebound there is shown as its default. The two files ship
the same defaults, which is why this is a gap and not a wrong answer. The **programs** the card
names are corrected for the desktop it is read on — a row that said `foot` on the console named a
Wayland client that cannot start there — but the chords themselves still come from the wrong file.

**No multi-seat.** `seat0` only. The session and view split makes a second seat
reachable — a second session with a second view — and nothing implements it, so
the claim is not made.

**A console session ends with the login that started it.** `kdos con new` makes a session the
service supervisor owns for the length of that login, so logging out takes every session with it
and there is nothing to attach to on the next one. Sessions that outlive a logout need a lingering
policy, a per-user enable and an answer for what the greeter does when you log back in; none of
the three is decided, so the mechanism is not built.

**No fractional scaling.** The toolkit adopts an output's **integer** scale and renders glyphs at
that scale, so a high-density display gets a sharp grid rather than a stretched one. Fractional
scale is not negotiated.

**One font size for every output.** The font every KDOS surface draws with is a single setting, so
it is right on a machine with one screen and wrong on two of different densities.

**A per-output panel shows every window, not that output's.** The window-management protocol
reports which output a window is on and the panel ignores it, so on two screens both taskbars list
the same windows. That is a well-established behaviour rather than obviously wrong; filtering is a
decision, not a fix, and it is not made.

**Tray menus published over the menu protocol are not rendered.** An item that expects the host to
draw its menu does nothing when clicked. It is a second protocol with a nested layout tree, and
drawing it as cells is its own piece of work. Such an item is hidden by default, listed in the
overflow popup where the row can say what it is, and its tooltip says why it cannot be clicked.

**Workspace occupancy is derived, not reported.** The protocol has active, urgent and hidden but
no "there are windows here", so the panel marks a workspace occupied when a window on it is not
minimised. That is right for every workspace you have visited and silent about the rest.

**Six surfaces have no offscreen dump and therefore no reference frame**: the panel itself, the run
box, the prompt, the notification daemon, the on-screen display and the desktop.

**Screen layout is an order, not a geometry.** Screens are placed edge to edge from the left in
list order; a vertical arrangement, an overlap or a deliberate gap cannot be expressed. That is a
deliberate narrowing — what people usually want is an order.

**The file dialog opens centred.** The portal's parent-window hint is ignored, because positioning
a dialog over the window that asked for it needs cross-process window referencing that is not
wired up.

**No input-method configuration tool.** The one upstream ships is built on a toolkit this host does
not have. Configuration is text files.

**No input method in the console session.** fcitx5 is a Wayland client and speaks
`input-method-v2` to the compositor; there is no compositor on that path. The candidate *window* is
drawn there — `kdos-ime` is a cell surface on both desktops — but the engine that would fill it is
not running.

**No VT has ever been allocated.** Embedding is what a graphical application gets and it has been
run end to end; `--vt` is the exception for something that needs acceleration, and that path — the
session allocates a terminal, activates it, starts `kdos-cage` there and supervises it, and the
guest appears in the taskbar marked with its terminal — has **never been run**. It needs an ISO
carrying `kdos-cage` and a machine with real terminals. What exists is the mechanism and the
reasoning behind its ordering; what is missing is the evidence that a guest ever appeared on a
screen that way.

**An embedded application is pointed at a cell at a time.** A press and a release carry where in the
cell they landed, so a small button is clickable; a drag that stays inside one cell moves the
guest's pointer nowhere, because a view reports a move when the cell changes. Nothing on this
desktop that is drawn in cells needs finer, and the one thing that is not is the thing that has to
live with it.

**An embedded application is typed at through a US keymap.** A view resolves the person's own layout
to a character before the session sees it, and the session maps that character back to the key that
produces it on a US keyboard — which is the keymap the guest is given. An application reading raw
scancodes therefore sees US positions.

**A picture needs `kdos-term`, not `kdos-con`'s own terminal windows.** The session links no pixel
code by design, so a terminal window it opens itself shows the fallback shade where a picture is.
`kdos-term` is the terminal that joins the parser to the decoder, and it is a surface like any
other — so a picture on the console desktop means opening one of those.

**No ReGIS and no Tektronix.** They are vector graphics protocols from DEC hardware, and nothing in
the catalogue emits either. The three raster protocols are what a modern program reaches for.

**The console screenshot is cells, not an image.** `kdos-shot` writes the grid as text. Rendering
cells to a picture is `libkcell`'s, which needs fcft and pixman, and `kdos-tools` is on every image
and links neither.

**One output on the console.** `libkkms` takes the first card with a connected output and its
preferred mode. A second screen is not composited onto.

## Applications and boxes

**No per-box protocol grants beyond the profile's list.** The compositor's sandbox filter is a
fixed allowlist: a client is sandboxed or it is not. A profile can open named globals; teaching the
filter to consult a box's profile for anything finer is deliberate work that is not done.

**Applications that need raw block devices are not in the catalogue** and get no launcher —
partitioners, drive-health tools, recovery tools. A rootless container cannot do anything useful
with them, and a launcher that opens onto a permission error teaches somebody that the machine is
broken. Those jobs are native tools on the host, which is where privilege is.

**Applications requiring a specific compositor's private protocols are out.** One catalogue
screenshot tool asks a particular compositor's interface and opens an error dialog on any other.
Screenshots are the host's own tool.

**No fonts that must be downloaded.** A Windows program wanting a specific proprietary font gets a
substitute, because fetching them happens at run time over the network and nothing in the image may
depend on that.

**X11 clients get no OpenGL.** The X server is built without the GL extension, because the graphics
stack is built without X11 platform support. Enabling it means rebuilding the graphics stack and
adding several X libraries. Wayland-native applications are unaffected.

**A live session cannot create a persistent box.** The home directory is on the boot overlay, and
the kernel refuses to stack a container's writable layer on an overlay. `kdos doctor` reports this
as a property of the session rather than as a failure.

**A box is not a security boundary against you.** It shares your home directory in full. It
constrains what an application can do to the **desktop**, not to your data. See
[The security model](../03-architecture/security-model.md#what-is-not-protected).

## Hardware and platform

**x86-64 only.** There is no other build target.

**UEFI only.** There is no BIOS boot path and no bootable-CD boot entry for one.

**The shipped image carries no partition table.** A raw copy of it nevertheless boots, verified
under one firmware implementation whose partition driver recognises the CD boot record on any block
device. That is behaviour of that firmware rather than anything the specification requires of
removable media, so it is recorded as measured rather than as a general claim. A recipe change
producing a real partition table is tested and not applied, because it changes the boot path.

**Broad hardware enablement is not a goal.** The firmware tree ships whole and unpruned, which
covers a great deal — but nothing here is tested against a wide device matrix.

**Much of `kdos doctor` cannot answer in a virtual machine**, which is why it has a *skip with a
reason* level rather than reporting those as passing.

## Security

The full statement is [What is not protected](../03-architecture/security-model.md#what-is-not-protected).
In brief: no mandatory access control, no verified boot, no measured boot, disk encryption protects
data at rest only, `wheel` is effectively root, a registry base fetches unsigned content, an
unsigned pack mounts while a failed signature does not, and there is no automatic update path.

## Build and packaging

**A/B slots and encryption are not wired together.** Slot selection yields a filesystem identifier,
and an encrypted slot's filesystem lives inside a container — combining them needs a per-slot
container identifier on the kernel command line.

**Filling the second root slot is an updater's job**, and there is no updater. What exists is the
complete state machine: the installer writes the initial state, the initramfs counts attempts, and
a boot that reaches the end of initialisation confirms the slot.

**There is no public binary host.** The mechanism is complete — a signed index, three equality
tests, deltas — but it is one you run yourself.

**Editing a library rebuilds every port of ours**, not only its consumers, because a recipe names
which libraries it compiles and parsing that would be a shell parser inside the package manager.

**Vendoring for one language is declared by no port**, though the fetch mechanism supports it.

## Testing

**The memory-pressure daemon has never fired for real.** Its victim selection is exercised against
recorded system state; a genuine stall on a machine under real memory pressure is the test that
matters and has not been run.

**The compositor and the shell are not compiled by the self-test on a bare host**, because their
Wayland dependencies are not there. Those blocks report as skipped on most machines.

**Nothing tests the build.** A package manager can only really be tested by building the
distribution with it, which takes hours and a container.

**The phosphor shader is not in any rig photograph.** The rig's virtual display puts the compositor
on software rendering, where the pass declines. What is photographed is the cell grid underneath
it.

## Documentation

**Some measurements in this book are quoted rather than re-derived.** Contrast ratios, launch
timings, freeze ratios and transport throughput figures were measured once and are repeated here.
Where a number is a measurement, the conditions are stated; where it was not re-taken for this
documentation, that is the honest caveat.

## See also

- [Decisions](../01-philosophy/decisions.md) — what is absent on purpose, and why
- [Roadmap](roadmap.md) — what is intended
- [Status](status.md) — maturity per subsystem, and the evidence behind each verdict
- [The security model](../03-architecture/security-model.md) — the full statement of what is not protected
