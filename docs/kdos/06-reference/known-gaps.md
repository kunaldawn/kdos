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
Only the trash accepts a drop on the desktop, which is a narrowing rather than a gap — see
[Decisions](../01-philosophy/decisions.md#narrowings). Both directions work between
a KDOS surface and a boxed application under `kdos-comp`; on the console the session carries a drag
between its own windows and across the cage boundary in both directions, which is built and
unphotographed — see below. See [Status](status.md) for what that rests on.

**A drag on the console has no picture under the pointer.** The view draws the pointer and the
session owns the drag, so nothing that knows what is being carried is in a position to hang a
picture off it — the bar says what is being carried instead. A target does not highlight either:
the session sends `ENTER` and `LEAVE`, and `kdos-desk` keeps them rather than drawing on them,
which is what `libkwl` does with the compositor's own.

**The pointer changes shape only where the session knows what is under it, and only on a view with
pixels.** Seven shapes are drawn on a screen of its own — resize arrows for every edge and corner,
a move handle on a title row, an I-beam over a terminal's content — and everywhere else the
pointer is the cell under it reversed, whatever the shape says. **There is no busy pointer**:
nothing here tracks a window as not-answering in a way a pointer could report. **An I-beam over a text
field in a SURFACE is the one that is missing**: a surface is another process's cells and the
session cannot tell a text field from a list row, so the shape there would be a guess, and a guess
is worse than an arrow because an I-beam that is wrong says a press will do something it will not.
Nothing may therefore say what a control does through the pointer alone — see the pointer contract
in [design-language](../03-architecture/design-language.md).

**The pointer also moves in whole cells.** `libkkms` reports a cooked motion only when the cell
changes and that event is what moves the drawn pointer; the pixel and the delta beside it go to an
embedded guest, which is the one thing aimed more finely than a cell.

**And no shape has been photographed.** The mapping is asserted at ten places on one window by the
self-test, which reads the shape the session decided; the masks themselves are drawn by `libkkms`
into a real framebuffer, and the rig's virtual display does not run that backend.

**No multi-seat.** `seat0` only. The session and view split makes a second seat
reachable — a second session with a second view — and nothing implements it, so
the claim is not made.

**A console session's PROCESSES end with the login that started it; its window list can come
back.** `kdos con new` makes a session the service supervisor owns for the length of that login, so
logging out takes every running program with it. Sessions that outlive a logout need a lingering
policy, a per-user enable and an answer for what the greeter does when you log back in; none of the
three is decided, so that mechanism is not built.

What is built is the list. With `restore = yes` a session started under the same name reopens the
windows it had — kind, size, place and workspace — with terminals running `con.conf`'s own
`terminal` and applications started through their desktop entry by app id. **Nothing in the state
file is ever run as a command**: it is written by a program and read by a program, and a file that
named an argv would be a file that chooses what somebody's session starts. `restore_scrollback`
puts the last session's output back above the fresh prompt, marked in the terminal as the previous
session's, and is off by default because old output that is not marked reads as live.

**No fractional scaling.** The toolkit adopts an output's **integer** scale and renders glyphs at
that scale, so a high-density display gets a sharp grid rather than a stretched one. Fractional
scale is not negotiated.

**No console font is loadable from `/usr/share/consolefonts`.** Every one of those is a PSF, and
the cell painter loads a face through fontconfig, which cannot scan a PSF at all: FreeType has no
driver for the format. The console draws in a fontconfig face like every other surface, and the
picker lists what fontconfig offers. The shipped PCF bitmap faces are also invisible to it — the
`70-no-bitmaps-except-emoji` rule rejects them and the rescue rule names `Terminus` where the files
report `xos4 Terminus` — so what is listed is the scalable monospaced families.

**A tray menu does not update while it is open.** The tree is read when the menu opens and
`LayoutUpdated` is not watched, which is a narrowing rather than a gap: a menu whose rows move
under the hand activates the wrong row. `AboutToShow` is sent first and its answer is the one
update taken. The rows, their submenus, their toggles, their icons and their chords are all drawn
— see [kdos-shell](../04-programs/kdos-shell.md). An item that publishes no menu path is sent
`ContextMenu` and opens whatever window it has of its own.

**Workspace occupancy is derived, not reported.** The protocol has active, urgent and hidden but
no "there are windows here", so the panel marks a workspace occupied when a window on it is not
minimised. That is right for every workspace you have visited and silent about the rest.

**A file dialog opened for a CONSOLE cage's guest is centred.** Under `kdos-comp` the portal's
`parent_window` reaches the chooser: the handle is imported through `xdg-foreign` and the
compositor centres the dialog on the window that asked for it. A guest under `kdos-cage` exports
its handle from the cage's own compositor, which is not the one the chooser connects to, so that
handle resolves to nothing and the dialog is centred on the screen. An `x11:` handle is dropped
for the same reason it always was: there is no X server here.

**No input-method configuration tool.** The one upstream ships is built on a toolkit this host does
not have. Configuration is text files.

**No input method in the console session.** fcitx5 is a Wayland client and speaks
`input-method-v2` to the compositor; there is no compositor on that path. The candidate *window* is
drawn there — `kdos-ime` is a cell surface on both desktops — but the engine that would fill it is
not running.

**A terminal framed by the session loses its prompt marks.** `kdos-term` draws the `OSC 133` dots on
the one column that is its own — the left border of the box it draws when nothing else drew one —
and a window the session frames has no such column. It is the trade the compositor's side already
makes, and it is the price of the frame not being drawn twice; carrying the marks would mean a
per-row status run beside the cell commit, which nothing sends yet. The chords still jump.

**An embedded graphical application is composited on the CPU, a block at a time, paced by the
display's own queue.** `kdos-con --run` gives a guest a window whose pixels cross a shared mapping
as sprite blocks, and a block at an 8x15 cell is a hundred and twenty kilobytes: a maximised window
is dozens of them. A turn offers a display blocks until its queue reaches half of `KCON_VIEW_HIGH`,
which is the room that display drained since the last turn, so a guest that repaints everything
continuously arrives a turn or two behind rather than at the rate a constant would allow. A large
one costs the session real processor time — this is software compositing of somebody else's pixels
and there is no path where it is not. What it does not cost is the desktop: the mark is under the
one where a display stops counting as ready, so the panel, the pointer and every other window are
still composed while a guest draws, and a display that is behind is skipped, never dropped.

That is the *session's* half of the crossing, and the guest's half is the card's only where the
card's frames can be read back. A profile that names no `render` key leaves both ends to ask the
machine and the machine answers with the card — but the cage keeps that answer only if a frame it
painted comes back out of the buffer it drew into, and on a host whose driver imports the buffer,
satisfies every call against it and then writes the pixels into memory of its own, it does not.
**A machine on that path gets llvmpipe for the whole cage, the guest's Mesa included**: no hardware
GL and no hardware video decode in any box, which for a browser is a video that stutters rather
than plays. **The gap is the readback and not the renderer** — nothing in this tree makes those
frames reachable, so the choice is between a software picture and a window that stays the colour
of the desk, and the cage takes the picture. It is measured on an NVIDIA host under `make run-hw`
and is invisible under `make run`, which lands on pixman with no render node to keep or fails the
import half of the probe before the readback half is asked. The frame reaches the screen as blocks
over the same socket either way, because the console's own display path is a CPU-mapped dumb buffer
with no GPU in it.
**So an embedded window cannot carry a game or 1080p60 video**, whichever renderer drew it: every
frame is read back, cut into sprite blocks, sent over a socket and written into a dumb buffer, and
that crossing is the ceiling rather than the drawing. `display = vt` is the path that can, and it
has never been run on real hardware — see below. See
[`kdos-cage`](../04-programs/kdos-cage.md) and [`kdos-con`](../04-programs/kdos-con.md).

**A boxed application on the console renders at scale 1 on every shipped font.** The session offers
a guest `KEMBED_SCALE`, the cage commits it to that window's output and every position and size that
crosses the channel is converted for it. The number is the primary view's cell height divided by a
reference cell of eight by sixteen, stepped down until it divides the cell's width as well — and
`con.conf` ships `font = monospace:size=12`, whose cell is that reference cell, so the division
gives 1 and the mechanism never engages. A 2 wants a cell thirty-two pixels tall with an even width,
which text stepped far up with `Super+=` or a `font =` written that large produces and nothing
shipped does. **Fractional scale is never offered to a guest**: a cell one and a half reference
cells tall gets 1, because a number that does not divide the cell leaves the guest rendering a
stripe short of its own output. What is missing is a font that crosses the mark, not the code — see
[`kdos-cage`](../04-programs/kdos-cage.md) and [`kdos-con`](../04-programs/kdos-con.md).

**A menu bigger than the window it opens in is cut off at that window's edge.** A popup is not a
window: it renders into its toplevel's framebuffer and cannot leave it. The cage keeps the head of
such a menu on screen rather than let wlroots shrink it to a band, and the scene clips the tail at
the edge — so the items past the edge cannot be reached. There is no scroll, and the toolkit is told
it was given the size it asked for, so it does not paginate either. Make the console window big
enough for the menu and the whole of it appears. See
[`kdos-cage`](../04-programs/kdos-cage.md).

**A drag across the cage boundary has never been seen on a screen.** Both halves are built — the
session sends `KEMBED_DRAG_ENTER` and its three over the channel and `kdos-cage` replays them as a
real `wlr_drag` with a pointer grab, and a guest's own drag is read out as `KEMBED_DRAG_OFFER` — and
what stands behind them is the compile gate and the reasoning, not a photograph. The negotiation
against a real `wl_data_device` is the untested half — the same half the clipboard's own gap below
names: a stand-in guest accepts a type because it was written to accept it, and a toolkit decides.

**And what crosses is text, which is what the session carries.** `text/plain` and `text/uri-list`
are the two types `kdos-con` accepts at all, so a guest dragging an image out has begun a drag this
desktop has nowhere to put and no offer is made; a file crosses as a path and not as its contents,
and the payload is capped at `KEMBED_CLIP_MAX`.

**No real boxed application has pasted or dropped across the cage boundary.** Both ends of
`KEMBED_CLIP_*` are built and the crossing is proved on a host with stand-in cages — one guest's
offer reaching another guest, and a copy made in the session reaching both — but the guest in that
proof is a stub that writes a fixed string, not a toolkit negotiating mime types on a real
`wl_data_device`. What is untested is the negotiation against a real client, not the carrier.

**The drag is one step further back, because the negotiation is what the drop is gated on.**
wlroots sends `wl_data_device.drop` only where the target both accepted a type and set an action,
and a stub accepts because it was written to. So a stand-in guest can prove that the five ops cross
and cannot prove that a toolkit takes what they carry.

**A guest given the keyboard while Caps Lock is on resolves one key before it is told.** The
modifier mask travels on the raw stream and nowhere else, and that stream runs only while an
embedded window holds the keyboard — so a lock toggled while a terminal had the focus reaches the
session as nothing, and the session refuses to assert a state it does not know. The first raw key
carries the mask behind it, which is the only key that guest resolves under the wrong lock. The
alternative is sending a guess at focus-in, which puts every letter of a whole application in the
wrong case whenever the guess is wrong.

**No multi-window guest has been photographed.** One cage is as many KDOS windows as its guest maps
toplevels — a headless output, a scene output and a mapping each, with the roles that decide
placement, stacking and the taskbar row. What stands behind that is the compile gate and host runs
against a stand-in cage; no real multi-window application has been seen on a screen on the shipped
image. The same is true of a guest holding a key, reading its own layout or locking the pointer:
the mechanism is here and the picture of it is not.

**No VT has ever been allocated.** Embedding is what a graphical application gets and it has been
run end to end; `--vt` is the exception for something that needs acceleration, and that path — the
session allocates a terminal, activates it, starts `kdos-cage` there and supervises it, and the
guest appears in the taskbar marked with its terminal — has **never been run**. It needs an ISO
carrying `kdos-cage` and a machine with real terminals. What exists is the mechanism and the
reasoning behind its ordering; what is missing is the evidence that a guest ever appeared on a
screen that way.

**Nothing reads the GRAPHICAL desktop.** The console session is read by `kdos-a11y` over its third
socket, because it holds the literal text of every cell and every widget announces itself.
`kdos-comp` draws pixels and has no such buffer, so a reader there would need the tree of accessible
objects this project does not build. What exists for a boxed application is that box's own registry,
opted into with `~/.config/kdos/a11y`.

**A terminal view's cell size is a guess unless the terminal names one.** `kdos-view --tty` asks
`CSI 16t`, which `kdos-term` answers and most terminals do not; without an answer it uses 8x16 and
`KDOS_VIEW_CELL=WxH` is the override. A wrong cell is a correctly encoded picture at the wrong
scale, which does not look like a probe failure.

**The last grid row of a terminal view is never pixels.** A picture at the bottom margin scrolls the
host terminal in every protocol, and a scroll invalidates the frame diff with nothing able to detect
it, so those cells keep the fallback mark.

**Nerd Font icons are blank on `tty1`, and the shipped configurations turn them off.** They are
private-use codepoints and the console font is 512 glyphs, which is a kernel limit: a glyph the font
does not carry renders as a blank cell, so an icon in front of a filename is a hole rather than a
picture. `yazi`'s generated theme empties all five `[icon]` tables, `starship`'s format uses box
drawing only, the `eza` aliases say `--icons=never` rather than relying on a default, and `lazygit`
0.61 already ships `showIcons: false`. **No shipped program has been found that draws them with no
way to be told** — and a scan of the built binaries is not evidence either way, because a
private-use codepoint in compiled data is a coincidence far more often than it is a glyph. The
answer for a person who wants icons is a Nerd Font in `~/.local/share/fonts` and a `kdos-term`
window at the TTF, which draws what fontconfig can find; there is no Nerd Font port and no
console-font patching.

## Applications and boxes

**An accent switch reaches a running GTK3 application and not a running libadwaita or Qt one.**
GTK rebuilds its style cascade when `gtk-theme-name` moves, so the settings portal's change signal
plus a stylesheet directory named after the accent is enough for GTK3 — which is most of the
catalogue. libadwaita ignores GTK themes entirely and reads `~/.config/gtk-4.0/gtk.css`, which GTK
loads once at startup; Qt under the KDE platform theme reads `~/.config/kdeglobals`, which KDE
re-reads only on its own global-settings signal and there is no KDE daemon here to raise it. Both
wear the new accent when they are next started. libadwaita 1.6 follows the portal's `accent-color`
on its own, which moves the accent and not the rest of the palette.

**No per-box protocol grants beyond the profile's list.** The compositor's sandbox filter is a
fixed allowlist: a client is sandboxed or it is not. A profile can open named globals; teaching the
filter to consult a box's profile for anything finer is deliberate work that is not done.

**A catalogue edit reaches a box only after the image is rebuilt.** The base row carries `libva`
and the VA-API driver set, but an installed application is a built image: editing the row changes
nothing a running system can see until `kdos app install <id>` builds it again. A boxed browser
built before that row reports no hardware decoder and decodes every frame on the CPU. There is no
notification that a row moved — the catalogue has no version per row, so an image is current by
definition until somebody removes it.

**X11 clients get no OpenGL.** The X server is built without the GL extension, because the graphics
stack is built without X11 platform support. Enabling it means rebuilding the graphics stack and
adding several X libraries. Wayland-native applications are unaffected.

**A live session cannot create a persistent box.** The home directory is on the boot overlay, and
the kernel refuses to stack a container's writable layer on an overlay. A pack is mounted from the
medium and is gone when the session ends; `kdos doctor` reports this as a property of the session
rather than as a failure.

**A box is not a security boundary against you.** It shares your home directory in full. It
constrains what an application can do to the **desktop**, not to your data. See
[The security model](../03-architecture/security-model.md#what-is-not-protected).

**A Rust program's clipboard reaches `kdos-comp` and not the console session.** `iamb` and `atuin`
both offer one through `arboard`, and both ask it for `wayland-data-control` on Linux — which is
wl-clipboard-rs speaking `zwlr_data_control_v1`, in pure Rust, so the binaries stay static-pie with
no `NEEDED` at all. `kdos-comp` creates both data-control managers, so a yank under that session
reaches `kdos-clip`. Measured on both built binaries: 214 `zwlr_data_control` symbols each and no
`NEEDED` entry at all. **Under the console session there is no Wayland socket**: arboard falls
back to its X11 path, there is no X server on this image, and a yank has nowhere to go —
`kdos-term`'s own selection is the answer there. **And no yank has been photographed reaching
`kdos-clip`**: what is measured is the path in the binary and the manager in the compositor, not
the two meeting.

**A video call has never been placed.** `baresip` is the SIP phone here and its interface is a
terminal menu; the far end's picture goes in an `sdl.so` window, and sending your own means turning
on `avformat.so`, which the generated config leaves commented because which camera to send is a
choice. The rig has no second endpoint and no camera, so what is measured is that the modules
load.

**`mbsync` reaches XOAUTH2 and not OAUTHBEARER.** `cyrus-sasl` is the mechanism loader and ships no
XOAUTH2 of its own, so the mechanism comes from `cyrus-sasl-xoauth2` beside it and that plugin
implements the one. A server offering only OAUTHBEARER cannot be mirrored into a local Maildir, and
with it goes `notmuch`'s index and offline search for that account. **`aerc` reads it and `msmtp`
sends it**: measured on the image, `msmtp --version` reports `Authentication library: built-in` and
lists `oauthbearer` and `xoauth2`, and `aerc` carries `imaps+oauthbearer`, `smtps+oauthbearer` and
its own `xoauth2Client`.

**A browse list is what answered a broadcast, not a directory.** Samba here is built without
winbind and without a domain controller, so there is no browse master to ask: `kdos-mount browse`
is an mDNS query and a NetBIOS one, and a machine asleep, on another subnet, or behind a router
that does not forward broadcasts is absent from it. It can still be reached by name.

**Kerberos is built and has never been given a ticket.** `kinit`, `cifs.upcall` and the
`request-key` rule that joins them are on the image and `kdos-mount krb5` mounts with `sec=krb5`,
but there is no KDC here and no realm to join — the server half of krb5 is not installed — so what
is measured is the request this daemon makes and not that a domain controller accepts it. **No
share has ever been mounted with a ticket on this image.** `cifs.idmap` is not built either: it
needs winbind's client library, and this desktop does not need it, because every share is mounted
with an explicit `uid=` and `gid=`.

**Neither synchroniser in the mail and calendar lanes has ever run against a server.** The rig has
no account, no network and no IMAP or CalDAV server on the image, so what is measured of `mbsync`
and `vdirsyncer` is that each runs, reports its version, and does nothing and exits cleanly with
nothing configured. That a password account synchronises is unproven here and can only be proven
against a real account — and **the XOAUTH2 lane is unproven in the same way and one step further
back**: that `libxoauth2.so` is in `/usr/lib/sasl2` and that `mbsync` links `libsasl2` can be
measured on the image, and that a provider accepts the token `pizauth` mints cannot.

## Hardware and platform

**x86-64 only.** There is no other build target.

**Secure Boot is not supported.** Limine's EFI binary is unsigned and KDOS enrols no keys, so a
machine with Secure Boot enabled refuses to load it. The installer reports the firmware state on
its first page; turning Secure Boot off in firmware setup is the only route.

**32-bit UEFI is built and has never been booted.** `BOOTIA32.EFI` is on the ISO's ESP tree, on an
installed machine's, and in the El Torito UEFI record, and the installer writes an NVRAM entry
naming it where `/sys/firmware/efi/fw_platform_size` says 32 — but the machine that needs it is an
early Atom tablet and nothing in this tree has one. What is verified is that the binary is built and
placed; BIOS and 64-bit UEFI are the two that have been booted.

**Broad hardware enablement is not a goal.** The firmware tree ships whole and unpruned, which
covers a great deal — but nothing here is tested against a wide device matrix.

**Much of `kdos doctor` cannot answer in a virtual machine**, which is why it has a *skip with a
reason* level rather than reporting those as passing.

**No speech model ships, and the transcribed text has never been read back on this tree.** What is
verified is the model gate, the `whisper-cli` argv, the spawn and the exit status. A model is
fetched rather than packaged — `kdos speech get` names one out of a table, checks its sha256 and
writes it to `~/.local/share/whisper.cpp/models`, and `kdos-rec`'s third button is *Get model*
until one is there — so what is missing is a machine with a model on it and somebody reading the
result, not a way in.

**Live transcription is a terminal program and not a desktop verb.** `whisper-stream` is built and
transcribes a microphone, and nothing on either desktop starts it: `kdos-rec`'s *Transcribe* is
`whisper-cli` over the file it has just recorded. Both want the same model, and no model ships.

**The emulated HDA codec gives the guest no capture signal**, so the rig cannot photograph a
deflecting meter from `--audio` alone. The recording evidence comes from `snd-aloop`, loaded by
hand in a root script.

## Security

The full statement is [What is not protected](../03-architecture/security-model.md#what-is-not-protected).
In brief: no mandatory access control, no verified boot, no measured boot, disk encryption protects
data at rest only, `wheel` is effectively root, a registry base fetches unsigned content, an
unsigned pack mounts while a failed signature does not, and there is no automatic update path.

## Build and packaging

**Filling the second root slot is an updater's job**, and there is no updater. What exists is the
complete state machine: the installer writes the initial state and each slot's LUKS container,
the initramfs selects a slot, unlocks that slot's container and counts attempts, and a boot that
reaches the end of initialisation confirms the slot. **So a second ENCRYPTED slot has never been
booted** — nothing fills one. What is measured is on the host: `select` rolling from B back to A
hands back A's container and not B's, which is the failure the mechanism exists to prevent.

**There is no public binary host.** The mechanism is complete — a signed index, three equality
tests, deltas — but it is one you run yourself.

**No port declares `vendoring = node`**, though `ports/fetch` implements it beside the three that
are used — 57 recipes declare `rust`, 32 `go` and 25 `python`. The npm path has never been run, so
what stands behind it is the code and not a tarball it produced.

## Testing

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
