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
a KDOS surface and a boxed application under `kdos-comp`; on the console the session carries a drag
between its own windows, and an embedded graphical application is not one of them — see below.
See [Status](status.md) for what that rests on.

**A drag on the console has no picture under the pointer.** The view draws the pointer and the
session owns the drag, so nothing that knows what is being carried is in a position to hang a
picture off it — the bar says what is being carried instead. A target does not highlight either:
the session sends `ENTER` and `LEAVE`, and `kdos-desk` keeps them rather than drawing on them,
which is what `libkwl` does with the compositor's own.

**The pointer does not change shape, and it steps a cell at a time.** On a screen of its own it is
an arrow in pixels and everywhere else it is the cell under it reversed, but it is one picture
either way: there is no resize double-arrow, no I-beam and no busy pointer, and the window says
what a press would arm instead — see the pointer contract in
[design-language](../03-architecture/design-language.md). It also moves in whole cells, because
`libkkms` reports a cooked motion only when the cell changes and that event is what moves the
drawn pointer; the pixel and the delta beside it go to an embedded guest, which is the one thing
aimed more finely than a cell.

**The console pointer is composited, not a hardware cursor plane.** `libkkms` draws the arrow into
the same framebuffer as the cells, so moving it costs the rows it covers and the rows it left.
`drmModeSetCursor2` on a plane would cost nothing per move and `kkms_drm_fd()` is public, but a
plane has its own size limits, its own format and a per-driver set of refusals, and the
transfer-model drivers the console runs on in a virtual machine have no usable plane at all — so
there would still have to be the composited path underneath it.

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

**One font for every output, size and face alike — everywhere but a terminal window.** The font
every KDOS surface draws with is a single setting, so it is right on a machine with one screen and
wrong on two of different densities. The console's font chords step every view that has a screen of
its own and the font picker sets the face on all of them, which keeps the two screens agreeing
rather than letting each be right: a per-output font is a different design, not a missing call. The
one window with a size of its own is `kdos-term` under `kdos-comp`, and only because it is one
process per window and the face is a process-global: `Ctrl+=`, `Ctrl+-` and `Ctrl+0` move that
process and nothing else. The same chords in a console window have nowhere to go — the cell there
is the view's — and say so. The picker also shows one list where two displays are attached — the
FIRST to answer — because two lists would be one question with two answers and nothing to say
which screen a person meant.

**No console font is loadable from `/usr/share/consolefonts`.** Every one of those is a PSF, and
the cell painter loads a face through fontconfig, which cannot scan a PSF at all: FreeType has no
driver for the format. The console draws in a fontconfig face like every other surface, and the
picker lists what fontconfig offers. The shipped PCF bitmap faces are also invisible to it — the
`70-no-bitmaps-except-emoji` rule rejects them and the rescue rule names `Terminus` where the files
report `xos4 Terminus` — so what is listed is the scalable monospaced families.

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

**The panel's own launch rows and `kdos-menu` split an `Exec` line on whitespace.** The
quick-launch row, the taskbar chip's *New window* and every application row in `kdos-menu` build
their argv with `strtok(" ")`, so an entry whose `Exec` carries a quoted argument starts wrong:
`Exec=foot --title="Install KDOS" -- sudo kinstall` reaches `foot` as `--title="Install` with a
stray `KDOS"` after it. They also fork the child themselves, so a graphical application started
from the panel while it is docked on the console session gets no cage and no display. The six
surfaces that go through `sh_launch()` — the Start menu, the palette, the desktop's icons,
`kdos-find`, *Open With* and the run box — have neither fault; these three are what a `sh_launch()`
call would close.

**`kdos-appbox open` starts a graphical handler outside the console session.** `open` resolves a
file's handler and `execvp`s it in the process it was given, so on the console a handler that is not
a terminal program gets no cage and no display and exits at once, with nothing on the screen to say
why. `terminal_first()` hides this wherever a terminal handler exists — it is put at the head of the
chain — so what is left exposed is a type whose only handler is graphical. The same function splits
the handler's `Exec` line on whitespace, so a quoted argument reaches the program in pieces. The
launch surfaces in `kdos-shell` do neither: they go through `sh_launch()`, which reads the quoting
and hands a graphical program to the session.

**A recording of a screen that does not change is an empty file.** `wlr-screencopy` hands over a
frame when the output is damaged and at no other time, so a session left alone produces no buffers,
the muxer writes no header, and `~/Videos/<name>.mkv` ends at zero bytes with nothing in the
recorder's log to say why. It is the shape of the protocol rather than a defect — a desk somebody
is sitting at always changes — but a recording taken to prove the pipeline needs something moving
on the screen while it runs.

**No input method in the console session.** fcitx5 is a Wayland client and speaks
`input-method-v2` to the compositor; there is no compositor on that path. The candidate *window* is
drawn there — `kdos-ime` is a cell surface on both desktops — but the engine that would fill it is
not running.

**A terminal application opens nothing from a launch surface on the console.** `Super+space`, `ma`,
`Enter` on **Mail** leaves the desktop as it was, and so does any other row whose desktop entry says
`Terminal=true` — from the palette, the Start menu, a desktop icon or `kdos-find` alike, because all
four make the same call. The wrapper is not the problem: typed by hand, `kdos-term --title mc
--app-id mc -e mc` opens the window and the taskbar names it.

**The two kinds of row do not end up in the same place**, which is where to look. On the console a
row that is *not* a terminal entry is handed to the session through `kcon_run()`, and the session
owns what it started; a `Terminal=true` row is double-forked by the surface, which then closes.
*Resources* opens because it is the first kind. The chord route is unaffected, so `Super+Shift+e`
still opens mail.

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

**A drag onto an embedded graphical application reads as cancelled.** `KEMBED_DRAG_OFFER`,
`KEMBED_DRAG_ENTER`, `KEMBED_DRAG_MOTION`, `KEMBED_DRAG_LEAVE` and `KEMBED_DROP` are declared in
`kembed.h` and implemented by neither `kdos-cage` nor `kdos-con`, so a drag released over an
embedded window is one the guest never hears of and the session ends as cancelled, and a drag begun
inside one never leaves it. The clipboard is the way across and it is built: `KEMBED_CLIP_OFFER`
puts a guest's copy on the session's own clipboard and `KEMBED_CLIP_SET` mirrors that clipboard and
the primary onto every cage's seat, both over a sealed `memfd`. What the clipboard cannot carry is
what a drag would have: it is text of at most `KEMBED_CLIP_MAX`, so a copied image leaves the
selection as it was, and a file goes across as a path and not as its contents.

**No real boxed application has pasted across the cage boundary.** Both ends of `KEMBED_CLIP_*` are
built and the crossing is proved on a host with stand-in cages — one guest's offer reaching another
guest, and a copy made in the session reaching both — but the guest in that proof is a stub that
writes a fixed string, not a toolkit negotiating mime types on a real `wl_data_device`. What is
untested is the negotiation against a real client, not the carrier.

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

**A touch screen points at an embedded application a cell at a time.** A view with a real pointer
or keyboard carries the raw device stream beside the cell one, so a guest is aimed in pixels and
typed at through the person's own layout; a touch event is synthesised from the gesture recogniser
and has no raw partner, so it arrives at the grid's resolution. A finger is wider than a cell, which
is why this has not been worth a second synthesis path.

**The console's tabbed windows stack and do not tile.** `Super+Shift+s` folds one window into
another as a tab and that is the whole of it: there are no **tile groups** — two windows put side
by side that move, size and minimise together — because nothing in the window model can hold one.
`tiled` is a per-window bitmask resolved against the work area and never against a neighbour, and
the arrangements clear it afterwards precisely so that an arrangement is not a state, so a group
would reuse none of the machinery a stack reuses and is a much larger change.

**A stack is made by a chord and never by a drag.** A title-bar drag on the console is a pure
translation with no drop target and no hit test against another window, and the tab strip answers no
press at all — so there is no dropping one window onto another's title bar and no clicking a tab to
bring it up. The four chords are what exist.

**Tabs cannot be reordered, and a stack does not survive a session restore.** The strip is ordered
by window id, which is the only order that does not reshuffle when a tab is brought up; there is no
verb that moves one along it. And the saved-session record is a rectangle and a flags column per
window, with no word for a relation between two of them, so a session saved with a stack open comes
back as that many ordinary windows. Both are in
[kdos-con](../04-programs/kdos-con.md#tabbed-windows).

**A picture needs `kdos-term`, not `kdos-con`'s own terminal windows.** The session links no pixel
code by design, so a terminal window it opens itself shows the fallback shade where a picture is.
`kdos-term` is the terminal that joins the parser to the decoder, and it is a surface like any
other — so a picture on the console desktop means opening one of those. A `--tty` view running
inside one of the session's own terminal windows detects this and stays on characters: that window
answers the device-attributes probe claiming sixel and then reports no picture geometry, and it is
the second answer that decides.

**Italic is upright wherever the loaded font has no italic companion at the same cell size.** The
painter asks fontconfig for the face with `:slant=italic` and keeps it only if its advance and
height match — and fontconfig never fails a match, so an italic Terminus comes back as a different
family at a different size and is refused. The attribute still travels: the cell carries it, a
terminal view emits `SGR 3` and the host terminal draws it. It is the KMS and Wayland painters,
drawing with the console's own bitmap face, that show the words upright.

**Nothing reads the GRAPHICAL desktop.** The console session is read by `kdos-a11y` over its third
socket, because it holds the literal text of every cell and every widget announces itself.
`kdos-comp` draws pixels and has no such buffer, so a reader there would need the tree of accessible
objects this project does not build. What exists for a boxed application is that box's own registry,
opted into with `~/.config/kdos/a11y`.

**Braille is the `brltty` route and not a library this tree links.** `a11y = yes` keeps the kernel's
text plane so `brltty` reads it over `/dev/vcsa`; BrlAPI is not linked by anything here, and a
display driven that way is driven by `brltty` rather than by the desktop.

**A recording is not an asciicast, and there is no player port.** `kdos con record` writes the
session's own messages — cell frames, sprites, window chrome — so no asciinema player will open one,
and a view that draws cells is already the player. An `asciinema` port to replay a format this tree
writes would be weight with no user on it.

**A terminal view's cell size is a guess unless the terminal names one.** `kdos-view --tty` asks
`CSI 16t`, which `kdos-term` answers and most terminals do not; without an answer it uses 8x16 and
`KDOS_VIEW_CELL=WxH` is the override. A wrong cell is a correctly encoded picture at the wrong
scale, which does not look like a probe failure.

**The last grid row of a terminal view is never pixels.** A picture at the bottom margin scrolls the
host terminal in every protocol, and a scroll invalidates the frame diff with nothing able to detect
it, so those cells keep the fallback mark.

**No ReGIS and no Tektronix.** They are vector graphics protocols from DEC hardware, and nothing in
the catalogue emits either. The three raster protocols are what a modern program reaches for.

**A console screen can be given a mode but not turned off, scaled or rotated.** The session lights
every connected connector into one grid, and `kdos-display` lists them and sets a mode on one; the
other three verbs are Wayland's, because a text grid has no scale factor, a rotation would give the
cells a different shape on one screen than on the next, and a dark connector would leave a hole in
the middle of a grid that windows are already placed across. The buttons for them are drawn
disabled on the console rather than hidden, so the surface is the same surface in both sessions.

**A mode chosen on the console does not survive the session.** `kdos-display`'s keep sends the
mode with its `keep` flag set and the view acts on the mode and drops the flag: there is nowhere on
this desktop a mode is written down, so the next login comes up at whichever mode `con.conf`'s
`refresh` names — the monitor's preferred one, or the fastest at that size. The
flag is on the wire because the countdown is the only thing that distinguishes an applied mode from
a kept one, and a keep that could not be expressed would make the countdown a lie.

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

**Hardware video decode reaches a box only after the packs are re-baked.** `ports/appbox/packs.conf`
carries `libva` and the VA-API driver set in the base row, but a pack is a built artefact: the
`.kpack` files on the medium are whatever the last `make fetch-packs` produced, and a row edited
since then changes nothing a running system can see. A boxed browser on packs baked before that
row reports no hardware decoder and decodes every frame on the CPU. The bake needs network and
about an hour.

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

**The initramfs must carry util-linux's `switch_root` and not toybox's, and the difference is
every container on the machine.** toybox's applet chroot()s into the new root and never moves that
root onto the root of the mount namespace, so every process on the booted system is chrooted for
ever — and `create_user_ns()` refuses a chrooted caller outright. The symptom is `EPERM` from
`CLONE_NEWUSER` for uid 0 with the full capability set as readily as for anybody, on a kernel
reporting `CONFIG_USER_NS=y`, 15440 namespaces available, no LSM, no seccomp filter, no lockdown
and nothing on the command line; `/proc/self/mountinfo` gives it away, with the root mount present
on the right device and a **parent id that is not in the table**. toybox owns the name
`/usr/sbin/switch_root` on the finished image and is installed after util-linux, so the copy has to
name util-linux's own file, and the packaging step refuses to build an initramfs whose
`switch_root` is toybox's.

**A live session cannot create a persistent box.** The home directory is on the boot overlay, and
the kernel refuses to stack a container's writable layer on an overlay. A pack is mounted from the
medium and is gone when the session ends; `kdos doctor` reports this as a property of the session
rather than as a failure.

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

**A network share is reached by an address or a DNS name, never by a workgroup name.** musl
resolves through `/etc/hosts` and `/etc/resolv.conf`; `nsswitch.conf` is inert on this C library,
there is no winbind and there is no mDNS responder, so a name only a NetBIOS or a Bonjour
broadcast could answer fails inside `mount.cifs` with a message nobody can act on.
`kdos-mountd`'s `cifs` verb refuses such a name up front rather than passing it on, and there is
no browse list: a server has to be named.

**Kerberos is not reachable from this image, and `CONFIG_CIFS_UPCALL=y` is not evidence that it
is.** `cifs.upcall` is disabled in the `cifs-utils` recipe and is absent from the image, and there
is no krb5 port, so `sec=krb5` has nothing to call out to. A server that will accept only a ticket
cannot be mounted from here; the verb offers username, domain and password and nothing else.

**Neither synchroniser in the mail and calendar lanes has ever run against a server.** The rig has
no account, no network and no IMAP or CalDAV server on the image, so what is measured of `mbsync`
and `vdirsyncer` is that each runs, reports its version, and does nothing and exits cleanly with
nothing configured. That a password account synchronises is unproven here and can only be proven
against a real account.

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
