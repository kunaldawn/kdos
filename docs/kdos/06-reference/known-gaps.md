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
it is right on a machine with one screen and wrong on two of different densities. The console's
font chords step every view that has a screen of its own, which keeps the two screens agreeing
rather than letting each be right: a per-output size is a different design, not a missing call.

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
other — so a picture on the console desktop means opening one of those. A `--tty` view running
inside one of the session's own terminal windows detects this and stays on characters: that window
answers the device-attributes probe claiming sixel and then reports no picture geometry, and it is
the second answer that decides.

**A terminal view's cell size is a guess unless the terminal names one.** `kdos-view --tty` asks
`CSI 16t`, which `kdos-term` answers and most terminals do not; without an answer it uses 8x16 and
`KDOS_VIEW_CELL=WxH` is the override. A wrong cell is a correctly encoded picture at the wrong
scale, which does not look like a probe failure.

**The last grid row of a terminal view is never pixels.** A picture at the bottom margin scrolls the
host terminal in every protocol, and a scroll invalidates the frame diff with nothing able to detect
it, so those cells keep the fallback mark.

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

**No process on the live medium can create a user namespace** — not even root with the full
capability set. Measured on the booted ISO: `unshare` succeeds for the mount, UTS, IPC, PID and
network namespaces and fails for `CLONE_NEWUSER` with `EPERM`, for uid 0 and for `kdos` alike,
while the running kernel reports `CONFIG_USER_NS=y`, `user.max_user_namespaces` at 15440, no LSM,
no seccomp filter and no lockdown. Rootless `podman` therefore cannot start there at all: it fails
at `cannot clone: Operation not permitted / cannot re-exec process`, before it reaches any storage
layer. The consequence anything else has to plan around is that a namespace is not available to a
program on this medium — `aerc`'s HTML filter, for one, falls back to an unroutable proxy.

**A live session cannot create a persistent box.** The home directory is on the boot overlay, and
the kernel refuses to stack a container's writable layer on an overlay. `kdos doctor` reports this
as a property of the session rather than as a failure.

**A box is not a security boundary against you.** It shares your home directory in full. It
constrains what an application can do to the **desktop**, not to your data. See
[The security model](../03-architecture/security-model.md#what-is-not-protected).

**An invitation in a message is read, never answered.** `aerc`'s calendar filter prints the event —
summary, times, location, who was asked — and writes nothing anywhere. There is no verb that accepts
or declines one, because a filter runs every time a message scrolls past and one that imported would
accept every meeting it was scrolled over, and nothing on this image sends a reply to an organiser.
**Filing one is now manual and it works**: `:save` the part out of the message and `khal import` it,
and the day carries a mark in the panel's calendar. No `text/calendar` handler is registered for the
same reason the filter writes nothing — opening a file would file it.

**On a bare virtual terminal, a link resolves to a graphical handler.** `Ctrl+Alt+F2` gives a login
shell whose `XDG_CURRENT_DESKTOP` is `KDOS` and which has neither the console session's socket nor a
compositor, so `http` and `https` resolve the way they do under the compositor — to the browser box,
a Wayland client with nothing to connect to. The console's own rows are keyed to the console
session's desktop name, which that shell does not have.

**With no browser pack installed, the compositor has nothing that opens `http`.** The console
answers with `w3m` in a terminal, and a browser installed as a box claims the scheme through the
launcher table the generator writes — but a graphical session with neither falls through to
xdg-utils' own script, whose last resort is to start a **text** browser with no terminal around it,
which means nothing visible happens. The console rows are not copied to the compositor on purpose:
they would outrank the browser box's entry the moment one was installed.

**Nothing on this image has a clipboard a Rust program can reach.** `iamb` and `atuin` both offer
one through `arboard`, which speaks the X11 protocol in pure Rust — it links no C library, so it
costs nothing to carry — but there is no X server here and the Wayland path is not compiled into it,
so a yank inside such a program has nowhere to go. The desktop's own clipboard is `kdos-clip`, and
`kdos-term` puts a selection there.

**A spreadsheet can be read and not written back.** `sc-im` opens an `.xlsx` natively — it links
`libzip` and `libxml2` and carries the reader in C — but its **export** is gated on
`libxlsxwriter`, which is not a port here, so it answers `XLSX export support not compiled in.` and
saves back out as `.sc`, `.csv` or `.ods`. You can open a file somebody sent you and cannot hand it
back in the format they sent. `visidata` reaches the same file only through `openpyxl` and can
write one, so the round trip exists — through the other program.

**A call is voice only, and out of the box it is G.711.** `baresip` is the SIP phone here and its
interface is a terminal menu. The codecs are all built — `opus.so`, `vp8.so`, `vp9.so` and
`avcodec.so` are among the 55 modules installed — but baresip's own generated `config` leaves every
one of them commented out, so a first run reports `Populated 0 video codecs` and negotiates G.711
alone. Uncommenting the module lines in `~/.baresip/config` turns them on.

**Video calling has nowhere to put the picture.** The capture half is there — the built `avformat`
module registers a video source and the shipped ffmpeg carries `video4linux2` — and the codecs
build. What is missing is a **display**: of baresip's video outputs only `fakevideo` (a null sink)
and `vidbridge` (a loopback) were built, because `x11` needs the X headers this image refuses by
rule and `sdl` needs an SDL port that does not exist. A call can send your camera and cannot show
you theirs.

**`mbsync` cannot use XOAUTH2, so mail from such a provider cannot be mirrored locally.** `isync`
reaches XOAUTH2 and OAUTHBEARER only through `cyrus-sasl`, which this tree does not build —
measured: the shipped `mbsync` links `libssl`, `libcrypto`, `libz` and `libc` and carries no XOAUTH2
string at all, and `pizauth` does not lift it because there is nothing to present a token to.

**The rest of the lane does.** Measured on the image: `msmtp --version` reports
`Authentication library: built-in` and lists `oauthbearer` and `xoauth2`, and `aerc` carries
`imaps+oauthbearer`, `smtps+oauthbearer` and its own `xoauth2Client`. So an account whose provider
has withdrawn application passwords is **read in `aerc` directly and sent through `msmtp`** — what
it does not get is a local Maildir kept in step by `mbsync`, and with it `notmuch`'s index and
offline search.

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

**No speech model ships and the desktop cannot fetch one.** `kdos-rec`'s *Transcribe* is therefore
permanently greyed on a fresh image, and the transcribed text has never been read back on this
tree: the model gate, the `whisper-cli` argv, the spawn and the exit status are what is verified.
The way in is upstream's `models/download-ggml-model.sh` writing to
`~/.local/share/whisper.cpp/models`.

**Transcription is batch over a closed file**, not live. The streaming example needs SDL2, which is
not a port here.

**The emulated HDA codec gives the guest no capture signal**, so the rig cannot photograph a
deflecting meter from `--audio` alone. The recording evidence comes from `snd-aloop`, loaded by
hand in a root script.

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
