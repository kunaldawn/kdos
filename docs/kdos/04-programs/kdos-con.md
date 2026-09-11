# kdos-con

The console desktop: a compositor whose framebuffer is a character grid. It is **the session a
login on tty1 reaches**, it needs no Wayland, and it comes up on a machine whose GPU driver does
not.

## The split that everything else falls out of

Two programs, and the line between them is the whole design.

| | Holds | Links |
|---|---|---|
| **`kdos-con`** | Every window, the focus, the workspaces, the clipboard, the lock | libkbase, libkcolor, libktui, libkdisp, libkcon, libkvt, libkwm, libkxdg |
| **`kdos-view`** | A display, and nothing else | the above, plus libkkms in the KMS build |

**The session rasterises no glyph and opens no device.** That is not tidiness: it is why a broken
GPU driver costs you a display and not your session, and it is why `kdos-con` compiles and its
goldens check on a bare host with no Wayland, no fcft and no pixman anywhere.

The view is dumb by design. It holds no window list, no focus, no workspace state and no
clipboard — cells arrive and input leaves. Four things follow from that without being built:

- **Detach and reattach.** A view that exits cleanly leaves the session running with every window
  where it was.
- **A view that crashes loses nothing.** `kdos-con-start` supervises the view and not the session,
  because the view is the half that holds a DRM device and a seat and can lose them. It supervises
  with **`--kms-only`**, which makes a screen it cannot take a non-zero exit carrying the step that
  failed: the terminal fallback is right for somebody running `kdos-view --kms` by hand and wrong
  here, where there is no terminal to fall back into and the fallback would draw a desktop into a
  log file and exit *0* in milliseconds. **A detach is told from a failure by how long the view
  lived, not by its status alone** — a view can still exit 0 quickly for a reason that is not a
  detach, so a clean exit under two seconds is a failure. Its stderr is kept in
  `$XDG_RUNTIME_DIR/kdos-view.log` and the tail is printed when the supervisor gives up — a view
  that fails silently is a desktop that is simply absent with no way to ask why. `kkms_reason()`
  names which of the eight steps failed, so the log says *no connector is connected* rather than
  repeating that there is no screen.
- **A desktop over ssh.** The view socket is forwardable and the view is trusted with nothing.
- **Screenshots, as cells or as a picture.** `kdos-view --dump` is a view like any other and
  `--shot FILE.png` is the same frame rasterised. The rasterising is the **view's**, not
  `kdos-shot`'s: turning cells into pixels needs a font, fcft and pixman, and `kdos-tools` is on
  every image and links none of them — while a view already loads a font to put the same cells on a
  screen. It settles the same way `--dump` does and takes the same frame, and it declares
  `KCON_VIEW_PIXELS`, so the pictures a program drew are in the photograph rather than missing
  from it.

## Four names, one binary

Dispatched on `argv[0]`, as `ksvc` and `kdos-appbox` are.

| Name | Is |
|---|---|
| `kdos-con` | The session server. `--serve`, `--new`, `--ls`, `--attach`, `--detach`, `--kill` |
| `kdos-grid` | A session **and** a view in one command, for a terminal or an ssh line |
| `kdos-con-login` | What `/etc/inittab` reaches through `kdos-getty` on tty1 |
| `kdos-view` | A separate binary — the display half |

## No window-model arithmetic lives here

Placement, tiling, the edge search and the ring walks are **libkwm's**, shared with `kdos-comp`. A
window landing in a different place on the two desktops would be two implementations of one idea,
and a defect in either would be two fixes. The console and the compositor call the same functions
against the same 124-row contract fixture.

## The two sockets

`$XDG_RUNTIME_DIR/kdos/<name>.sock` and `<name>.view`, in a directory the session creates `0700`.

| Socket | Admits | May leave the machine |
|---|---|---|
| `.sock` | Surfaces — programs that place windows | **never** |
| `.view` | Views — a display | over ssh, with `kdos con forward` |

**Which socket a client reached decides what it is allowed to be.** The kind in a client's
handshake is a claim and is overridden by the listener's. That is the entire reason there are two:
forwarding a socket that admitted surfaces would hand the far end the right to place windows in
your session, which is a different thing from showing you yours.

`$KDOS_CON` names the **surface** socket, so a program started inside the session inherits an
address that opens a window — typing `kdos-res` in a terminal here opens a window, exactly as it
does in `foot` under Wayland.

A directory that already exists with the wrong mode or owner is **a refusal to start, not a
`chmod`**: if it is not ours, quietly taking it over puts the socket in a path another account
chose, and the peer-credential check is then guarding the wrong door.

**Ending a session is a message, not a missing socket.** `kdos con kill` connects to the surface
socket and asks; the session sets its quit flag, stops its listeners — which takes both socket
files with them, so nothing new attaches and `kdos con ls` stops reporting it — and drains its
clients on the way out. Unlinking the files instead would leave the loop running on listeners it
still holds, so every attached view keeps its display and the session is unreachable and alive; and
there is no pid in a socket path, so looking one up by name would end whichever process matched.
Only a shell surface may ask, the same rule `detach` keeps.

### Managing somebody else's windows

**A surface asks for the privilege when it attaches, and is a plain surface if it does not.** A
panel, a task switcher and a window menu set `manage` on the display configuration; the client
declares itself a **shell** in its opening message, and the kind is fixed there and cannot be
raised afterwards. Everything a surface may do to another program's window is gated on it: being
sent the window list at all, and `ACTIVATE`, `CLOSE_REQUEST` and `MINIMISE`. A launcher or a
calculator never asks, so neither can close somebody's editor.

**The list is pushed and the surface re-reads it.** The session sends `TOPLEVEL_ADD`,
`TOPLEVEL_STATE`, `TOPLEVEL_REMOVE` and `WORKSPACE` when its own state changes, and sends the whole
list to a shell that has just attached. A surface re-reads the list on each turn rather than being
called back: both consumers already have a poll loop, and the only pump that reads this socket is
the one that also delivers key events, so a pump added for a callback would swallow the panel's
input.

**Every verb is a request and none of them reports.** The session owns the stack and the lifetime;
what happened arrives as a new list, which is the only account either display server can give.
`MINIMISE` carries the state asked for rather than a toggle — a panel draws the rows it was sent
and a person clicks the one they can see, so a toggle would act on whatever the session believed a
round trip later.

**A surface reaches all of this through `libkdisp`, never through either protocol.** The same six
calls are answered on the console from these messages and under a compositor from
`wlr-foreign-toplevel-management`, so a panel is written once. Neither list carries a window
handle across the interface: a caller holds an **id** and asks again, because a window can close
between the frame that drew the row and the click on it. The Wayland side reports no workspace,
because that protocol does not carry one.

**There is no TCP listener anywhere.** A remote desktop is a forwarded unix socket and inherits
ssh's authentication, which is why it needs none of its own. `remote = no` is enforced where the
tunnel is built — `kdos con forward` refuses — rather than where a connection arrives, because a
forwarded socket's peer is the local ssh process running as the same user and cannot be told from
a local view by credentials. The self-test asserts the absence by grepping the sources for
`AF_INET`.

## Native surfaces are first class

`kdos-shell`, `kdos-res` and `kdos-lock` each name `{ &kcon_impl, &kwl_impl }` and libkdisp picks
the first whose probe succeeds — console first, so a surface started *from* this desktop attaches
to it even on a machine that is also running a compositor.

**A panel docks.** A panel-role surface names an edge and a **thickness** and attaches with **no
size**: the extent along the edge is the screen's, which no client can know, so the session answers
with the configure that allocates the cells. It carries no frame and no shadow, and when it
reserves an exclusive zone it genuinely shrinks the work area rather than covering it.

**A panel is re-docked when the grid changes**, before the work area is taken and before any window
is fitted against it: a rectangle measured from an edge is wrong at a new size, and the exclusive
zone it reserves is what every window below is moved by. The thickness is kept, the extent is the
new screen's, and the configure tells the client.

**The session's bar has two shapes, and `taskbar` in `con.conf` picks.** `windows` is a row of the
open windows. `fkeys` is Norton Commander's row — `F1` to `F10` and what each does, which a person's
hands learn in a week.

**The function-key row names the chord and does not bind the key.** A bare `F7` belongs to whatever
is running in the focused window, which is the whole reason a terminal desktop can carry a
function-key row at all. Every cell is a **pointer target** that fires `Super+F<n>`, and the row
says `Super+` once — ten copies of the word do not fit beside ten labels in eighty columns. A click
is turned back into the chord and fed to the same handler a key press reaches, so the row cannot
teach a chord that does something else.

**A cell is drawn whole or not at all**, and none is drawn into the clock's width: half a label is
a target that names the wrong chord, and a cell the clock overwrites is one a person aims at and
misses. A row too narrow for the tenth ends at the ninth.

**Eight of the ten are bound on both desktops; tile and rearrange are the console's alone.** labwc
has no tile-all and its `MoveResize` is not the same interaction, so the row names them as this
desktop's own rather than pretending they are shared. `selftest` fails the build if any cell names
a chord nothing is bound to — the row is drawn from one table and bound from another, and nothing
else makes the two agree.

**One bar, and the session's is the fallback.** `kdos-con` draws its taskbar until a
panel whose app id is `kdos-shell` docks — a second menu that appeared only when the real one was
missing would be a second menu to keep in step. Every other docked surface is a panel too:
`kdos-slit` is a column of gadgets, and standing down for it would leave a desktop with no Start,
no window list, no clock and no pager.

**A saver covers.** A saver-role surface attaches with **no size** for the same reason a panel does
— the session owns the answer and sends it in the first configure — and is then
drawn *instead of* the desktop rather than over it: nothing else is painted, taskbar included, and a
cell it did not write shows the theme background rather than the window that was there. It is in no
taskbar, no cycle order and no hit test, so it can never take the focus and never swallows a click.

**Closing a surface asks; it does not remove.** The entry stays until the client has actually
disconnected — the same rule a terminal window, a guest on its own VT and an embedded application
all follow, and here it is load-bearing rather than merely honest: a surface stays in the server's
list until its client goes, so an entry removed at the request is one the session builds again on
the very next pass — as a fresh window, and for a role it configures on adopt as a fresh lock or a
fresh saver.

## Roles, and the three layers

A surface says what it is when it attaches, and the session answers with a layer rather than a
window. Five roles get a branch of their own:

| Role | Drawn | Frame | Taskbar | Alt-Tab | Keyboard |
|---|---|---|---|---|---|
| `TOPLEVEL` | with the windows | yes | yes | yes | on focus |
| `PANEL` | docked, with an exclusive zone | no | no | no | never on attach |
| `OVERLAY` | above every window | no | no | no | takes it |
| `BACKGROUND` | below every window | no | no | no | never |
| `LOCK` / `SAVER` | instead of everything | no | no | no | lock only |

**Three passes, because there are three layers.** Stacking inside a layer is the window list's own;
between them it is fixed and has to be — a menu a window could be raised above is a menu that
disappears behind the thing it was opened from, and desktop icons drawn last would cover every
window on the screen.

**An overlay takes the keyboard and a background does not.** The Start menu, the launcher and the
run box are overlays and are answered by typing. The icon layer covers the whole grid and sits
behind everything; focusing it would take the keyboard from the window a person is working in every
time the desktop redrew.

**An overlay is placed where it asked to be, not where there was room.** A surface names a corner
and its margins from that corner's two edges — the same `corner`, `margin_x` and `margin_y` a
Wayland client passes, because layer-shell has no coordinates either and a corner plus a margin is
how it says "at x". The unit is a cell here and a pixel there, and a caller's number is the same on
both: `kdisp_cell_w()` answers 1 on this transport. So a menu opened by a click on `Start` lands
above the Start button, a toast lands in the top right, and one opened by a chord names no position
and is centred. Placing a layer with the minimal-overlap search that windows use put the Start menu
a third of the way across the top of the screen, which for a menu is nowhere.

Margins are clamped to the work area rather than honoured off it: a bottom-anchored menu taller
than the space above the taskbar is drawn from the top of the work area, never from a negative row.

**A layer belongs to no workspace.** A toast that belonged to the workspace it was raised on would
be invisible to somebody who had just switched away from it.

**A background is not hit-tested.** It covers the grid, so testing it before the windows would take
every click on the desktop. An overlay *is*: a menu is there to be clicked.

Without this, every one of those surfaces arrived framed, shadowed, listed in the taskbar and in the
Alt-Tab ring — which for a toast is worse than no toast at all.

## The lock

`ext-session-lock-v1` is Wayland. Here `kdos-con` **is** the server, so it owns the lock:

- Input goes only to the lock surface — not to a window and not to the session's own chords, or a
  locked machine would still open a terminal on `Super+Return`.
- **Only the lock surface is drawn.** A lock composited over the desktop shows the desktop wherever
  the lock has a cell it did not write, and every cell it did not write is one.
- **The state survives the lock client dying.** A crashed lock program leaves a locked screen that
  says so. That is the entire reason a lock screen is not a fullscreen window.
- **Unlock is its own message**, never an inference from the client exiting. Closing is what a
  client does when it exits for *any* reason, including an error it did not expect.
- **The grant is the session's to give, and it is told.** A lock client refuses every keystroke
  until the session confirms the lock, because until then the desktop is still on screen and a
  password typed at it goes to whatever has the focus. Attaching is a request; `KCON_OP_LOCK_STATE`
  is the answer, and it is flushed rather than queued — a byte sitting in a send buffer is a lock
  screen nobody can type into.
- **One lock at a time.** A second client is refused with the same message, carrying the refusal bit
  instead of the grant, and closed. It is not allowed to replace the first: the person typing would
  then be answering a different program from the one that locked the session. A refused client
  exits non-zero, so a caller cannot read *0* as "the session is locked".

## Idle

Three steps — `idle_saver`, `idle_lock`, `idle_off` — each measured from the last input rather than
from the step before it, so they read as they look and want to be written in increasing order.

At `idle_saver` the session starts the program named by `saver`, which attaches with the saver role
and covers the grid. **`Super+Shift+l` starts the same program**, so blanking the screen before
walking away is a chord rather than a wait — the shifted form of the one that locks, because the
two are the same gesture with and without the password on the way back. **It takes no keyboard and claims no pointer region**, so every keystroke and
every click reaches what is underneath — which is what lets the idle policy see the activity that
takes it away. It is *asked* to close, never killed: the session double-forks everything it starts
and does not know the process, and asking is what lets a saver put its own affairs in order.

**There is no `idle_dim`.** `comp.conf` has one because a compositor can dim a framebuffer; a dim is
a brightness, and this desktop's colours are eight palette slots with no brightness between them, so
a "dimmed" grid would be a different picture rather than a darker one. The saver is not a dim under
another name — it is a picture, which a grid draws exactly.

The lock happens before the blank and never the other way, or a screen would come back on showing
what was on it, and the saver goes when the lock arrives rather than animating underneath it.
Blanking is `drmModeSetCrtc` with no framebuffer on the view's device — **not** a DPMS property
write: DPMS is a connector property that legacy and atomic drivers expose differently and some
virtual drivers do not expose at all, while detaching the CRTC is the one operation every KMS driver
implements. In a virtual machine all three default to `0`, because a blanked screen over VNC cannot
be told from a crashed session.

## The login

`/etc/inittab` gives tty1 to `kdos-getty tty1 /usr/local/sbin/kdos-con-login tty1`. The getty stays
first and stays in the chain: it loads the console font and palette after fbcon's deferred
take-over, and it **falls back to the plain autologin getty** when the program named there cannot
be executed, so an image built without this desktop still gives a console.

`greet = no` hands the tty to `agetty --autologin`, which keeps utmp, lastlog and the shell profile
on the path they take everywhere else. `greet = yes` — what the installer writes — draws the login
surface on the **tty backend**, not a modeset: a greeter that opened a DRM device would make this
binary depend on libdrm, fcft and pixman, which is the one thing the session/view split exists to
survive.

**The privileged half never handles a hash.** On submit the greeter forks, the child drops to the
candidate account, and it executes `kdos-checkpass` with the password on stdin — the same setuid
helper the lock screen uses, which takes no arguments and checks the caller's own real uid, so
nothing on this path can be aimed at root. Its three exit codes stay apart all the way to the
message: reporting "wrong password" for an unreadable `/etc/shadow` locks a user out of a working
account while looking in the wrong place.

## Configuration

`/etc/kdos/con.conf`, overridden by `~/.config/kdos-con/con.conf`; chords in
`~/.config/kdos-con/keys.conf`, which binds **the same chords `rc.xml` binds**. Every key is in
[Configuration](../06-reference/configuration.md).

Which key runs the launcher is a keyboard question and which program *is* the launcher is not, which
is why the two live in different files.

## Terminals

`libkvt` is a hard fork of libtsm 4.7.1, kmscon's VT100–VT520 state machine, rebranded `tsm_` →
`kvt_`. A terminal window is a `kvt_term` and a pty; `vim`, `htop`, `mc`, `less` and `tmux` are what
it is for. Scrollback is set by the caller — a library that read a program's configuration file
would answer differently in every consumer.

**A hyperlink works the same in a session terminal as in `kdos-term`.** `OSC 8` interns the address
on the terminal's own cell, hover underlines the whole run and `Ctrl`+click opens it through
`kdos-appbox open` — one implementation in `libkvt`, so a link cannot behave differently depending
on which desktop a person is sitting at. Four schemes and printable ASCII only; the refusals are in
[the security model](../03-architecture/security-model.md#a-uri-a-terminal-was-told-about).

**A view can write what it is sent to a file, and a view can draw one back.**
`kdos-view --record FILE` records while it draws; `--replay FILE` draws a recording and attaches to
no session at all — a player that connected would be a second view on somebody's desktop, resizing
it to whatever the recording was made at. `kdos con record FILE` and `kdos con replay FILE` are the
same two through the `kdos` command.

**What is recorded is the session's own messages**, one ndjson line each with a timestamp and the
payload, the whole stream under `zstd`. Not the cells and not the pixels: a recorder that re-encoded
cells would be a second definition of a frame, and it would lose an op added to the protocol after
it was written. The header names the protocol version and **a replay refuses a version it does not
speak** rather than reading the bytes as something else. The self-test records a live view and
replays it with no session anywhere: the frame that comes out must be the frame that went in.

**It is KDOS's format and it is not an asciicast.** An asciicast is a terminal's byte stream; this
is a desktop's cell frames, its sprites and its window chrome, so no asciinema player will open it
and calling it asciicast would buy a broken expectation. Nothing needs a player port either — a view
that draws cells is already the player.

**A session can be started again with what it had open.** With `restore = yes`, a session started
under the same name reads `$XDG_STATE_HOME/kdos/con/<name>.session` and reopens the windows it
records — kind, size, place and workspace. **The list comes back; the processes do not.** A terminal
is reopened running `con.conf`'s own `terminal` and an application is started through its desktop
entry by app id, so **nothing in the file is ever run as a command**: it is written by a program and
read by a program, and a file that named an argv would be a file that decides what somebody's
session starts.

**It is saved when the session ends, and a signal is how a session ends.** A login ending sends
`TERM`; with the default disposition the process would die where it stands, so `TERM` and `INT` set
the same flag the quit verb does and the session leaves through the same door — closing its guests,
releasing their terminals and writing the list. A session killed outright keeps the list it had.

**`restore_scrollback` puts the last session's output back**, above the fresh prompt and under a
line saying whose it is. Off by default and a separate key, because this is the half that can
mislead: unmarked old output reads as live, and somebody scrolling up finds a build that never ran
here. An application's window is placed when it attaches — a restored client's window does not exist
until then — and only the first window of each app id takes a remembered place.

**A running session can be photographed as text**, with `kdos con capture` — the whole grid, or one
window's content with `--window N`. It goes to the **surface** socket and only a shell surface is
answered: reading a session back is a management right, and a display is trusted with the cells it
was handed and the events it reports, not with what every other window is showing. The session
pumps its terminals once and composes before answering, so the text is what has already been
written rather than a frame from before the last command's output arrived.

**The prompt marks work the same way**, from the same `libkvt`: `Ctrl+Shift+Up` and
`Ctrl+Shift+Down` jump between the prompts a shell marked with `OSC 133`, and the dot goes on the
window frame's left border — the one column the window manager owns, since every column inside
belongs to the child. The chord is claimed whether or not it moves, so a shell that emits no marks
does not send the arrow to the child on some screens and not on others.

**A pointer event reaches a terminal window in that window's own grid**, with the window's origin
already subtracted, because the hit test that found the window used the frame rect. A press on the
border is therefore outside the content and is dropped rather than clamped onto the first row —
and a terminal handed screen coordinates would select text, and follow a link, as far from the
pointer as the window is from the corner.

**A bell is two halves, because a bell is two things.** `BEL` from a child flashes that window's
frame and title in the accent slot for 120 ms — the chrome and not the content, so the line that
rang is still readable — and the session sends the `bell` verb to every attached view. A view in
somebody's terminal writes `BEL` on and lets *that* terminal do whatever it is configured to do; a
view with a screen of its own has no sound to make and does nothing, the flash having already
happened where the cells are. A window with no frame — fullscreen, a panel, a layer — shows no
flash, and the ring is the whole of what it gets.

## Pictures

Three protocols carry an image into a terminal, and they differ only in how they are delimited —
sixel is a `DCS`, iTerm2's is an `OSC`, kitty's is an `APC`. `libkvt` delimits all three through one
collector and **decodes none of them**: it hands the payload to a callback, which is what keeps it
free of image decoders, and it has to stay free of them because `kdos-con` links no pixel code at
all.

A payload past the cap is **dropped entirely, not truncated**. Half an image is not a smaller image;
it is a malformed file, and handing one to a decoder is handing it exactly what an attacker would
have sent on purpose.

`libkimg` is where the decoding happens, and it is one entry point for that reason: there is one
place to audit, one place the budget is enforced, and one place a fifth format would be added. The
budget is checked against the size the **format itself declares**, before a decoder allocates — a
PNG saying 65535x65535 is eight bytes on the wire and sixteen gigabytes in memory, and refusing
after decoding is not refusing.

**A picture is a sprite.** Nothing new is invented for the drawing: `ktui_sprite_put` already takes
a picture occupying whole cells with a fallback codepoint for backends that cannot show one, and a
picture larger than sixteen cells is a grid of sprites sharing a key prefix so it evicts and
re-registers as a unit.

**A surface's slot numbers are its own.** Two windows both using slot 0 is the normal case, so the
session assigns a slot when a picture first arrives and rewrites the cell as it composites — which
is what stops one window's picture appearing in another's. A slot a surface never sent draws the
fallback rather than somebody else's image.

**The pixels cross the wire through an accessor the consumer registers.** `libkcon` must not know
what a pixman image is, so a consumer that has one supplies the bytes and `libkcon` sends the blob
it is handed. A consumer with no pixel library sends metadata only and the display draws the
fallback — which is what a text backend does anyway, and is why a picture over a plain terminal
degrades to characters rather than to nothing.

**The pixel size travels with the picture, and the view scales it.** A client cannot know how many
pixels a cell is: a cell client has none of its own, two views of one session can be running
different fonts, and a view forwarded over ssh is a third answer. So a picture is sent at whatever
size its sender had, and the display — the only thing that knows — resamples it to its own cells.

## A Wayland application on the console

This desktop composites **character cells** and a Wayland client's surface is **pixels**. There is
no way to put one inside the other, so a graphical application on the console gets a **VT of its
own** and the console desktop stays where it is.

[`kdos-cage`](kdos-cage.md) is what runs on that VT: a hard fork of cage 0.3.1, MIT, on the same
`wlroots 0.20` the compositor fork pins. It runs one application full screen and refuses every
attempt to do anything else, which is precisely the job — so it was taken rather than written.

### How a guest gets its terminal

**seatd binds a client to whatever VT is active when it asks**, and there is no protocol by which a
client requests a particular one. That single fact fixes the order:

1. `VT_OPENQRY` for a free terminal. Activating one allocates the console, so the next query steps
   past it and no bookkeeping is needed here.
2. `VT_ACTIVATE` — asked for by the session, **waited for by the child**. A session that blocked on
   `VT_WAITACTIVE` would stop drawing until the view had acknowledged the switch, and the view is
   what draws it.
3. The child execs `kdos-cage -s -- <the application>`. **`-s` is not optional**: without it the
   kiosk swallows the VT-switch chords, and a full-screen application nobody can leave, on a machine
   whose desktop is on another terminal, is a wedged machine.

Nothing here opens `/dev/ttyN`. The session is an ordinary user and a spare terminal belongs to root
until something privileged opens it — which seatd does, on the guest's behalf. Finding and taking
one needs neither.

The guest's **stdin and stdout go to `/dev/null` and its stderr is kept**, so a compositor that
failed to start says why in the session's own log. A log on a terminal nobody switched to is a log
nobody reads.

### A graphical application is a window

`kdos-cage --embed` composites it in a **process of its own** and hands the frames back over a
private descriptor channel; the session cuts each frame into sprites and writes the sprite
codepoints into the window's cells. `kdos-con` still links no wlroots, no mesa and no pixel library
— it moves bytes it never looks at.

**Sprites, not a pixel rectangle painted beside the grid.** A sprite lives *in a cell*, so a window
in front of an embedded one simply overwrites those cells and the occlusion is the z-ordered copy
that was already there. A rectangle drawn over the grid would cover whatever was above it, and
every window-model question — stacking, snapping, workspaces — would need a second answer for one
kind of window.

A picture is at most sixteen cells square, which is what the cell's sprite encoding carries, so a
window is a grid of blocks that size and damage is rounded out to the blocks it touches.

Everything else about it is an ordinary window: chrome from the same code, a title bar, a close
button, `kwm_snap`, a taskbar entry, a workspace. Minimising tells the guest, which stops
rendering — a guest drawing frames nobody composites is a guest spending a core on nothing. The
guest exiting closes the window; closing the window asks the guest to go and leaves the entry until
it actually has, the same rule a terminal window follows.

**Input arrives as a cell and a position inside it.** A view that knows its own pixel geometry says
where in the cell the pointer was, in 1/256ths; one that does not means the centre of the cell. A
key arrives as a *character*, because that is what a view resolved the person's layout to, and the
session maps it back to the key that produces that character on a **US keymap** — which is the
keymap the guest is started with. An application reading raw scancodes therefore sees US positions.

### The guest on a terminal of its own

`--vt` remains for an application that needs acceleration a software renderer cannot give it: an
embedded guest is composited by pixman on the CPU, which is fine for an editor and is not a way to
play a game. It is selected per application by `display = vt` in its box profile, and
`embed = false` in `con.conf` turns embedding off for everything.

Such a guest is a window with **no cells**: in the list so that it is in the taskbar and the
Alt-Tab ring, drawn nowhere, and **selecting it is a VT switch** rather than a raise. The taskbar
marks it with the terminal it is on — `[vt3]` — because that number is how a person reaches it when
the taskbar is on another screen.

**Alt-Tab crosses in one direction only.** Once the screen is showing such a guest, this desktop
receives no input at all — `Ctrl+Alt+F<n>`, or closing the guest, is how you come back. That is not
a defect in the taskbar; it is what "on a terminal of its own" means.

### What a view is told, and what it does with pixels

A view says in its hello how many pixels one of its cells is, whether it can put a sprite's bytes on
a screen, whether it rasterises its own glyphs, and **whether it can draw a colour outside the
theme's eight slots**. The session uses the first for sizing an embedded guest — the primary view's, because
that is the display the person is looking at — and the second to decide how often it may send a
frame: at the session's own redraw rate when something can show pixels, and once every 250 ms when
nothing can, because a window of pixels at a compositor's frame rate down an `ssh` link is a link
that does nothing else.

**A third socket is the reader's**, beside the surface socket and the view socket in the same
private directory. Its clients are displays that **cannot drive** — the socket decides that, not the
client — and they are the only ones sent `KCON_OP_ANNOUNCE`: the role, name, value and position of
whatever the focused widget just said, plus the focused window when it changes. A reader that
attaches late is told the window again, for the same reason a view that attaches is sent the whole
frame. `speak = yes` starts `kdos-a11y` with the session; see
[Accessibility](../02-user-guide/accessibility.md).

**A finger crosses as `KCON_OP_TOUCH`, carrying the verdict and not the geometry.** There is one
gesture recogniser and it lives where the touch device is — `libinput` under a KMS view — so what
the session is told is what that recogniser made of the finger. A second one here would be fed a
message rather than a device and would disagree the first time a link was slow. It is a **display
only**, the rule the pointer keeps: a window that could report a finger could put a long press on
somebody else's surface. The mouse event the recogniser synthesises beside the touch travels as an
ordinary `KCON_OP_PTR`, so a surface that has never heard of touch still gets its click, and the
session draws the pointer cell from that arm alone — drawing from both would put the cursor at the
finger and then at the pointer within one frame.

**The view is told where the caret is, because it cannot work it out.** A display holds no window
state, so it cannot know where the text cursor of the focused window sits — and a display that does
not know cannot put a real cursor there. On a `--tty` view that costs a person the one thing their
own terminal could have shown them: a blinking cursor at the place they are typing, drawn by the
terminal they are sitting at rather than by a cell this desktop painted.

**A terminal and a surface both report one, and both report it in their own cells.** A window knows
where its caret is and nothing else knows where the window is, so the session adds the offset and is
the only thing that does. A surface's half is `KCON_OP_CARET`, sent whenever the caret moves and
once with a negative x when the field it belonged to has gone — which the session reads as *no
caret* rather than as a position off the left edge, and which is also the answer when nothing has
the focus. The toolkit routes it: `ktui_term_caret()` writes the terminal escape only when the
backend has no `caret` entry of its own, so every surface that already placed a caret reports one
here without being changed.

**Every connected screen is lit, and the grid is all of them.** A KMS view takes every connector
that reports itself connected with a mode and a CRTC it can have, in DRM connector order, and lays
their modes end to end into one virtual box — one dumb buffer per screen, each with its own row
diff, because two screens comparing against one previous frame would each find the other's paint
already done and neither would redraw. `--card PATH` names a device for a machine with more than
one; without it the first card with a connected output wins.

**A screen plugged in after login is a resize.** The view holds a `udev` monitor of its own for the
`drm` subsystem — libinput's context watches `input` and nothing else — and a hotplug re-probes the
connectors, re-cuts the buffers and announces the new grid as an ordinary `KCON_OP_VIEW_SIZE`,
which is the same event the session already handles for a font step. Every picture goes with it:
each was cut for a grid that has changed shape.

**A view may watch without typing.** `kdos con attach --observe` says so in its hello, and the
session drops that view's keys and pointer where they arrive — the refusal is on the server's side,
because a promise a client keeps by itself is decorative. It sends none either, which is what makes
it honest rather than merely safe, and it **draws no pointer**: each view draws its own locally over
the shared frame, so a pointer on a view that cannot click would be a lie about what it is. `views`
in `con.conf` caps how many displays may attach at once.

**A colour a program named exactly is sent only to a view that asked for it**, in a run of its own
beside the cells. A view in somebody's sixteen-colour terminal would have to reduce every literal
back to the slot it was already sent, having paid for it on the link — and a link slow enough for
that to matter is the link a remote view is on. Every cell carries the slot either way, so a view
that declined draws exactly what it drew before.

**Every view is sent the same thing otherwise**, and what becomes of a picture is decided at the far
end. A
view that cannot show pixels turns each cell of the picture into the character whose shape covers
the same part of a cell — the matcher behind `kdos-ascii` — and colours it with the nearest palette
slot to that cell's average. That happens **in the view**, which is the only end that knows whether
this build has a font at all; the session holds no font and no pixel code and must not gain one.

**A terminal view asks the terminal it is running in.** `--tty` writes ANSI to its own stdout, so
the only way a picture can be pixels there is for that terminal to draw them. Before the first
frame it sends one burst — the kitty graphics query, `CSI ?2;1S` for a sixel geometry, `CSI 16t` for
the cell size — and closes it with `CSI c`, which every terminal answers and which is therefore the
fence that says the picture questions have been answered or never will be. **Every byte of every
reply is consumed**: one left behind is not silence, it is decoded as keystrokes and typed into the
session being viewed.

The ladder is:

| Answer | Tier |
|---|---|
| `\033_Gi=…;OK\033\` | kitty — it said it will draw one |
| `\033_Gi=…;E…\033\` | none — it said no on purpose, and guessing past a "no" is how a terminal with pictures turned off gets pictures emitted into it |
| DA1 parameter 4 **and** `CSI ?2;0;w;hS` | sixel |
| nothing at all, not even DA1 | `$TERM` / `$KITTY_WINDOW_ID` |
| anything else | none — characters |

**DA1's sixel attribute is a claim about a parser, not about a decoder**, which is why it is never
believed alone. This tree's own terminal answers it with pictures compiled out, and `kdos-con`'s
terminal windows answer it while registering no decoder at all; `CSI ?2;3;S` — no geometry — is
what those then say to the second question, and that is the answer that decides.

**The picture is put back after the text, every frame the text painted over it.** The emitter is a
backend wrapper rather than a step after the frame, because the one thing it has to know is which
cells were just repainted — and that is knowable only while the previous frame is still around to
diff against. The text layer is also the **eraser**: every sprite cell flushes as a space or as the
fallback mark, so a picture whose window closed, moved or scrolled is removed by the same pass that
would have redrawn it, and nothing has to track disappearance separately.

**An animation is invisible in the cells**, which name a slot and not a picture, so the emitter
compares the sprite's generation counter instead and rate-limits itself to one re-encode per
picture per `KDOS_VIEW_PIX_MS` (50 ms). It does **not** declare `KCON_VIEW_PIXELS`: that would stop
the session rate-limiting every guest for every view, and a terminal that blocks on a write is a
view that is not reading its session socket — which a session ends. The throttle stays; the emitter
bounds itself.

**A failure falls back to characters, never to blank cells.** A picture too broken up by windows
over it, one the sprite table refused, and one whose encode would exceed what the terminal will
accept all keep the marks the text layer wrote. `KDOS_VIEW_PIX=off` turns the whole thing off and
`KDOS_VIEW_CELL=WxH` overrides a cell size the terminal would not name.

**The last grid row is never pixels.** A picture at the bottom margin scrolls the host terminal in
every protocol, and a scroll invalidates the whole frame diff with nothing able to detect it. Those
cells keep the fallback mark.

So an embedded application over `ssh` is characters, by shape, where the terminal cannot draw and
pixels where it can, and both fall out of the negotiation rather than being special cases.

### Recording it

`kdos-view --cast` is a view nobody looks at. It rasterises through `libkcell` exactly as the KMS
view does — the same cell painter, the same glyph cache, the same row diff — and writes the result
into a **PipeWire stream** instead of onto a screen. There is no capture path beside the drawing
path, because a view is already what turns cells into pixels.

It imposes no grid size, so starting a recording does not resize the desktop being recorded, and it
sends no input: a recording is not a seat. Damage drives the frames — a still desktop rasterises
nothing and the stream's cycles carry an empty chunk — with one exception: a consumer that has just
connected is fed one frame anyway, because otherwise a recording of an idle console is a recording
of nothing.

It prints the node id and the stream's pixel size on one line, which is what
`xdg-desktop-portal-kdos` hands to the application that asked. See
[the session](../03-architecture/session.md) for why that backend serves the console and
`xdg-desktop-portal-wlr` serves the compositor.

### Reaching it

| From | What runs |
|---|---|
| The Start menu, any graphical application | `kdos-shell` asks the session, which starts `kdos-cage --embed` and makes it a window |
| The Start menu, **Desktop** | The full `kdos-desktop` session, on a terminal of its own and **not** in a cage — it is its own compositor |
| A terminal entry (`Terminal=true`) | `kdos-term`, which is a cell surface and opens as a window here |
| A command line | `kdos con run [--] CMD [ARG…]`, which prints the terminal the guest was given or **0** when it became a window |

## Two kinds of terminal, and why

`Super+Return` opens what `con.conf`'s `terminal` names — `sh` by default — as a window the session
runs **itself**, with `libkvt` driving a pty and no separate process. `kdos-term` is the other kind:
a full terminal that attaches as a client over `libkcon`.

They differ in one visible way. **`kdos-con` links no `libkimg`**, so a picture arriving in a
session-run terminal draws as a fallback shade, while the same picture in `kdos-term` is shown. That
is a decision, not an oversight: the session links no decoder and no pixel code at all, which is
exactly what lets it come up on a machine whose GPU driver does not. Putting an image decoder in the
process that holds every window would trade that property for a picture in the one terminal that
does not need to be the one showing pictures.

So: the session's own terminal is the one that always works, and `kdos-term` is the one that shows
everything. A person who wants pictures in the terminal `Super+Return` opens sets `terminal` to a
command that runs `kdos-term`, or opens it from the menu.

## Copy and paste

**The session owns the selection**, because nothing else can. On Wayland a compositor arbitrates
between data devices and a source stays alive to serve its own bytes — which is why a copy there
vanishes when the program that made it exits. Here `kdos-con` *is* the server, so a copy is a client
handing over bytes and a paste is a client asking for them back, and the selection outlives the
program that made it. That is what a person means by copying.

One CLIPBOARD and one PRIMARY, both text and only text. A picture on the clipboard would be a
payload this desktop has nowhere to put.

| Doing | Reaches |
|---|---|
| A drag in any terminal | the PRIMARY selection, on release |
| Middle-click | pastes the PRIMARY |
| A paste in the terminal a view runs in | the focused window, and the CLIPBOARD |
| `Ctrl+V` | asks for the CLIPBOARD; the text arrives as a message, not a reply |
| `Shift+PageUp` / `PageDown` | ten lines of scrollback; any other key ends the view |

**Capped at 64 KB.** `libkcon` refuses a payload above a megabyte and drops a client whose queue
passes four, so a selection that reached either limit would be a copy that killed the window it came
from.

**A request is answered even when the selection is empty.** A client that asked and heard nothing
cannot tell a slow session from an empty clipboard, and would wait for a paste that is never coming.

**A child can copy, through OSC 52.** `52;c;<base64>` from any program running in a terminal
reaches the session's selection, so a `vim` yank is pastable into every other window. The decoded
selection then goes **out to every view that is a terminal**, which writes OSC 52 to the desktop it
is running on: a view inside `foot` or at the far end of `ssh` is a window on somebody's own
desktop, and a copy that did not reach their clipboard is one they cannot paste into their own
editor. It buys nothing on `tty1`, where `ktui_clip_copy()` is a deliberate no-op because a Linux
console has no clipboard — there the session's own selection is the whole answer.

Only the CLIPBOARD goes out. The primary changes on every drag, and a desktop whose clipboard was
rewritten by every mouse gesture in a remote session is one nobody would leave attached.

**And a paste comes back in.** A view in somebody's terminal asks that terminal for bracketed paste
(`CSI ?2004h`), so text pasted there arrives between `CSI 200~` and `CSI 201~` as text rather than
as the keys it spells — a pasted line beginning with `Ctrl+A` would otherwise arm the leader key
and swallow the character after it. The view forwards it whole on the `paste` verb, and the session
puts it into the **focused window**: a terminal window gets it through `kvt_term_paste`, bracketed
again when its own child asked for that, and a surface gets it as clipboard data, which is the only
text channel a surface has. It also lands on the session's CLIPBOARD, so the next window can have
it without the far-end terminal being asked twice for one gesture.

**Only a display may paste.** The verb is refused from a surface. A client that could paste could
type into whatever has the focus without the person touching a key, which is the one thing nothing
on this socket is allowed to do.

**`52;c;?` is refused, and there is no key to turn it on.** It asks the terminal to hand the
clipboard back to the program running inside it, which would let anything that can write to a
terminal read whatever was last copied anywhere on the desktop. It is refused in the state machine,
so there is no callback to enable it by mistake.

**No `Ctrl+Shift+C` is bound in `keys.c`.** Selection is a property of the focused surface, and a
session chord would take that key from every guest terminal and from `kdos-term` itself.

**What a drag means is `libkvt`'s, not this program's.** `kvt_ui_mouse` decides when the wheel
belongs to the child rather than to the scrollback, when a drag is a selection rather than a mouse
report, and that a press and a release in one cell is a click and selects nothing. `kdos-term` calls
the same function: written twice, the two would drift, and the difference would be a terminal that
behaves differently depending on which desktop it is on.

## What the pointer does

The panel row and every window frame answer a click, and both answer it from a **hit map recorded
as they were drawn** — never re-derived from geometry afterwards. A title is truncated to what fits
and the clock is right-aligned, so a second calculation of where an element ended up is a second
thing to get wrong, and a click that lands one entry off is worse than one that lands nowhere.

| Where | Press | Does |
|---|---|---|
| `Start` | left | opens `con.conf`'s `menu` |
| A taskbar row | left | raises it, or **restores it** when it is minimised — the row is the way back |
| A pager cell | left | switches to that workspace |
| The clock | left | opens `kdos-cal` |
| `_` `■` `X` on a frame | left | minimise, maximise / restore, close |
| A title row | left drag | moves the window |
| Anywhere in a window | Super + left drag | moves it, so a window that is all content is still movable |
| Anywhere in a window | right drag | resizes it from the **nearest** edge or corner |

**A minimised window keeps its taskbar row**, because the row is how it comes back: it is drawn
nowhere, cycled past and not hit-testable on the desktop, so a bar that dropped it would leave
`Super+Shift+n` as the only route to it.

**The keyboard reaches a window by pointing at it.** `Super+Shift+`arrow moves the focus to the
nearest window that starts past the focused one and shares rows or columns with it, and
`Super+Alt+`arrow trades the two rectangles — maximise and tile state with them, so neither window
ends up at a size nothing asked for — with the focus following the window rather than the place.
When nothing overlaps, the focus stays put: the ring is the way to a window the arrows cannot see.

**Every dragged rectangle goes through `kwm_fit`**, exactly as a snap does — and so does every
rectangle `win_resized()` is about to tell a window about, which is what makes it the one place
that applies a surface's minimum. A dragged window and a snapped one obeying different work-area
rules would be two answers to one question, and the panel's exclusive zone is in that answer.

**A surface reports the smallest grid it can compose on at ATTACH, and the minimum wins over the
work area.** A program given fewer cells than it needs draws nothing at all, and the cells under it
keep whatever the last program left there — a hole in the desktop that no repaint fixes, because
nothing owns those cells any more. Told the minimum, the session hands the window that size and
lets it hang off the edge: a window with a corner off the screen is visibly a window. Zero is no
minimum, and it is what a terminal reports — a terminal reflows to anything, and the program inside
it decides for itself.

**The panel is not draggable and neither is a fullscreen window.** The panel's rectangle *is* the
exclusive zone, so moving it would move the work area out from under every other window.

## The chords, and the leader that reaches them

Every window-management chord is on Super, so none of them can collide with what a program inside a
window wants. A view with a screen of its own reads Super from `libinput` and they all work.

**A chord means the same thing on both desktops, and the suite is what keeps it true.** The defaults
are written twice in two syntaxes — `Super+Shift+t` in `keys.conf`, `W-S-t` in `rc.xml` — so a chord
added to one file and forgotten in the other was a key that worked on one machine and did nothing on
the next, with neither file able to say so. `selftest.sh` normalises both into one spelling and
fails on any chord bound on one desktop only. **`rc.xml` is not one table but two**: `<default />`
is the first child of `<keyboard>`, so labwc's own sixteen binds load beside the file's, and a check
that read only the `<keybind>` tags would call `Alt+Tab` console-only and miss `Alt+F4` entirely.

**Twenty-six chords are one-sided on purpose**, each with the reason in the check's own table: the
leader, because it is the one chord not on Super and exists for the views where Super never arrives;
the mark and the paste, because the session holds the text of every cell and a compositor holds
pixels; the font steps, because the screen's font is the console's and under the compositor the font
is each client's; and labwc's grow, shade and always-on-top, because the console has no such state.
**The key card prints the table of the desktop it is on and not both** — `rc.xml` is the
compositor's file and is never parsed on the console — so the cross-check is what makes the two
tables one table rather than the card showing them side by side.

**Eleven of them start a surface, and nine are the chords `rc.xml` already binds.** The panel that
hangs these off its applets is the graphical desktop's and does not run here, so without a chord
they were reachable only from the Start menu. Taking the compositor's own chords rather than
inventing new ones means a person who learns one desktop knows the other, and that there is one key
card rather than two.

| Chord | Surface | `con.conf` key |
|---|---|---|
| `Super+F1` | the keybinding card | `keys` |
| `Super+F3` | audio devices | `audio` |
| `Super+F4` | networking | `net` |
| `Super+F5` | Bluetooth | `bluetooth` |
| `Super+F6` | cameras, microphones and removable media | `devices` |
| `Super+i` | settings | `settings` |
| `Super+c` | the calendar | `calendar` |
| `Super+/` | the documentation viewer | `docs` |
| `Super+p` | screen configuration | `displays` |
| `Super+Ctrl+p` | power and battery | `power` |
| `Super+Ctrl+t` | the system monitor | `monitor` |
| `Super+Ctrl+q` | the calculator | `calculator` |
| `Super+Ctrl+n` | the scratch pad | `notes` |
| `Super+Ctrl+v` | the clipboard history | `clipboard` |
| `Super+Ctrl+e` | the character map | `characters` |

The last two are new on both desktops: neither had a chord anywhere, and a power page and a system
monitor are what a person reaches for while something is going wrong.

### One key per program

Seven more chords **raise the window running a program, or start it** — and a second press while
that window has the focus **cycles** to the next window of the same program, so three terminals
under one chord are all reachable.

| Chord | Role | `con.conf` key | Default |
|---|---|---|---|
| `Super+e` | files | `files` | `mc` |
| `Super+Shift+e` | mail | `mail` | `aerc` |
| `Super+Shift+b` | the web | `browser` | `lynx` |
| `Super+Shift+u` | music | `music` | `rmpc` |
| `Super+Shift+c` | the diary | `agenda` | `ikhal` |
| `Super+Shift+g` | chat | `chat` | `iamb` |
| `Super+Shift+w` | writing | `writing` | `micro` |

**The chord names a role and `con.conf` names the program.** Which key opens the mail is a keyboard
question and which program *is* the mail is not, which is the same split every chord above keeps.
The diary is `agenda` because `calendar`, `find` and `notes` already name surfaces of this
desktop's own.

**Every one of them runs in a terminal, and that is what makes one action enough.** The window is
the session's own — `term_open()`, on this grid — with no display-mode decision to make; a
graphical program named in one of these keys would open in a terminal and draw nothing. The browser
here is a text browser for the same reason.

**A role whose program is not installed keeps its chord and opens nothing**, and the key card drops
its row rather than teaching a key that does nothing. `kdos-con --keys` prints a third field for
these rows — the program — and that is what the card reads; the session prints every row it binds,
because hiding one there would make the table disagree with the chords.

**Matching is on a field the guest cannot rewrite.** A window's `title` follows whatever the program
emits, and its `app_id` says what *kind* of window it is — every terminal window is `terminal` and
every caged guest is `kdos-cage` — so the session records the program a window was opened for once,
at open, and matches on that. A terminal entry started from the menu declares it through
`kdos-term --app-id`.

**The workspace switch comes before the raise, never after.** Going to a workspace clears the focus
and cycles to whatever the ring lands on, so a raise before it is undone; and a minimised match is
un-minimised where it is rather than through the restore path, which *moves* a window to the
current workspace — the opposite of going to it.

The compositor binds the same seven, as `rc.xml` `ForEach` blocks whose `<query identifier>` is the
program's `app_id`. `W-grave` carries the [scratchpad](#the-scratchpad) on both, which is a
`ForEach` of the same shape over a marker rather than a program.

**`Super+p` reaches a surface that cannot configure a console screen.** `libkkms` takes the first
connected output at its preferred mode and has no mode selection, so `kdos-display` says so and
exits. The chord is bound because a stated limit is better than a missing key.

**Which program each chord runs is `con.conf`'s, and the table above is the whole list.** They are
keys rather than literals in the chord table for the same reason `menu` and `lock` are: one place
says what a chord starts, and `kdos-con --keys` prints it, so the card cannot name a program the
session does not start.

**A view that is a terminal is the harder half, and it is the one the two-socket split exists for.**
A terminal reports modifiers as escape sequences, and Super arrives only in the kitty keyboard
protocol's `CSI <codepoint> ; <modifiers> u` form. `libktui` asks for it with `CSI ? u` on entering
the screen and pushes `CSI > 1 u` only if the terminal answers — there is no capability database
entry for this and no `TERM` value that implies it, so asking is the only way to know. The push is
popped on every path that restores the terminal, including the signal routes: a view that died with
the mode pushed would leave a terminal its own shell does not understand.

**A terminal that never answers gets the leader instead.** xterm, most VTEs and the Linux VT — which
is what a `--tty` view on `tty1` runs in — report no Super at all, so without a fallback the remote
desktop would be `Alt+Tab` and nothing else. The leader, `Ctrl+A` by default and rebindable in
`keys.conf` like any other chord, makes the next key run as though Super were held: `Ctrl+A` then
`Return` opens a terminal, `Ctrl+A` then `Space` the Start menu.

Three rules it keeps, each with what breaks without it:

- **Pressing it twice sends the literal.** `Ctrl+A` is the start of the line in every shell on this
  image, and a desktop that took it outright is one people turn off.
- **One key deep and no timeout.** A prefix that expired would fire a chord or a literal depending
  on how fast somebody typed.
- **A second key that names no chord is swallowed, not typed.** A prefix that leaked would put a
  stray character in a document every time somebody mistyped a chord.

## The keyboard's own window management

Every one of these was a menu item on the text desks this desktop descends from, and each is a rule
over state the session already holds.

| Chord | Does |
|---|---|
| `Super+grave` | show the scratchpad over everything; the same key hides it |
| `Super+Alt+grave` | make the focused window the scratchpad |
| `Super+Alt+1`…`9` | raise the window that number names |
| `Super+F2` | the window list |
| `Super+Shift+t`, `Super+F8` | tile this workspace |
| `Super+Alt+t` | cascade this workspace |
| `Super+r`, `Super+F9` | move and size the focused window from the keyboard |
| `Super+Shift+d` | hide every window; the same chord brings them back |
| `Super+Shift+m` | mark a rectangle of the screen |
| `Super+Shift+v` | paste what was marked into the focused window |
| `Super+Shift+p` | capture a rectangle: its text to the clipboard, its picture to a file |
| `Super+Ctrl+c` | the capture group, for the verbs that have no chord of their own |
| `Print` | the whole screen to a file — **on a KMS view only** |
| `Shift+Print` | the same rectangle `Super+Shift+p` marks — **on a KMS view only** |
| `Alt+Print` | start the screen recording, and stop it — **on a KMS view only** |
| `Super+Ctrl+Alt+t` | the time and date, as a notice |
| `Super+Ctrl+Alt+b` | what the battery says, as a notice |
| `Super+Ctrl+r` | remind me — one row, and the line is `in 20m tea` |
| `Super+Ctrl+Alt+r` | the reminders still waiting |
| `Super+Ctrl+Shift+r` | forget every reminder |
| `Super+x` | put the newest notice away |
| `Super+Shift+x` | put every notice away |
| `Super+Ctrl+x` | hold notices back, and let them through |
| `Super+Alt+x` | bring the last one back, without its buttons |
| `Super+Ctrl+i` | never lock or blank on idle, and again to let it |
| `Super+Ctrl+Shift+n` | warm the palette, and cool it |
| `Super+Shift+Space` | put the bar away, and bring it back — **this desktop's alone** |
| `Super+Shift+r` | record the keys you type; the same chord stops and asks for a letter |
| `Super+Alt+r` then a letter | type that script back into the focused window |
| `Super+Ctrl+Shift+Space` | the accent picker, previewing live as the highlight moves |
| `Super+Ctrl+Space` | the next character-art background, and `none` is in the ring |
| `Super+=`, `Super+-` | bigger and smaller text on the screen — **on a KMS view only** |
| `Super+Ctrl+0` | the text size back to what the configuration names |
| media keys | louder, quieter, mute, play, stop, next, previous — **on a KMS view only** |

**The font belongs to the view and the chord to the session**, which is the split every other
device verb here keeps: the view is the half holding a rasteriser, the session the half holding a
keyboard table. `KCON_OP_VIEW_FONT` carries a signed step and never a font name — a session naming
fontconfig syntax would be a session deciding what a display it has never seen can render — and the
new grid comes back as an ordinary `KCON_OP_VIEW_SIZE` with the new cell beside it, because a screen
divided by a different cell is a resize and the session already knows how to handle one.

**A view inside somebody else's terminal is not sent a step.** It says so in its hello, and the
session answers on the bar instead: that terminal owns the font, and no message from here can change
it. Every view with a screen of its own is stepped, not the primary alone — two screens showing one
session must not end up at two cell sizes because the chord reached whichever attached first.

**The stepped size is an override under `~/.local/state/kdos/con-font`**, written only after the
font has loaded and removed by the reset. A name fcft would refuse is therefore never the name the
next login starts with, and `--font` and `$KDOS_CON_FONT` still win: a state file that beat them
would be a chord that had quietly switched the configuration off.

**The FACE is picked the same way round, and by the same rule.** `kdos-style --page font` — the
`style.font` route — lists what the display offers, and the display is what gathered it: a view is
the only end with a font stack, and a view forwarded over `ssh` has its own machine's fonts. The
list crosses on `KCON_OP_VIEW_FONTS`, asked by the session and answered by the view; what goes back
on `KCON_OP_VIEW_SETFONT` is an **index into that answer** and never a name, which is the rule the
step keeps said about a list. A `--tty` view claims no font capability, so the list is empty and the
picker draws the sentence the chord puts on the bar.

**An arrow is a real font on a real screen and writes nothing.** Only `Enter` writes
`~/.local/state/kdos/con-font`; leaving any other way puts back the face the window opened on. A
preview that persisted would make the last face a highlight passed over the one the next login comes
up in, whether or not anybody chose it.

**`con.conf`'s `font` is what the screen starts at.** `kdos-con-start` passes it to the view, which
is the half that rasterises; a `--tty` view is given nothing, because the terminal it runs in owns
its font.

Every picture the view is holding was cut for the old cell, so it drops them and the session sends
them again when it sees the grid move — an embedded guest is sized in pixels from that cell, so its
blocks are re-cut in the same breath. A view that stretched what it had would draw one sharp screen
and one blurred one on a machine with two.

**A window's number is its position in the Alt-Tab ring**, drawn in its title bar and in its taskbar
row. It is the ring's own index rather than an identity: it renumbers when a window closes, which is
what cycling does too, so the two cannot disagree about what "the second window" means. Windows past
nine have no digit and none is drawn.

**Tile and cascade are arrangements, not tile states.** Both clear the snapped mask, so a
`Super+`arrow afterwards snaps from the rectangle the arrangement made rather than from a half the
window is not in. Tile fills the work area with a grid of `ceil(sqrt(n))` columns and spreads the
last row across the width; cascade offsets each window by a row and two columns at two thirds of the
work area, wrapping when the screen runs out. **A fullscreen window is left out of both** — it was
put there on purpose, and folding it into a grid would undo a request nobody withdrew.

**Rearrange owns the keyboard while it is on**, ahead of the chord table and ahead of the window:
arrows move by a cell, `Shift+`arrows size from the bottom-right corner, `Ctrl+`arrows move by
eight, `Return` keeps and `Escape` puts back the rectangle from before. The taskbar names those keys
for as long as the mode is up, because a mode that took every key and said nothing would be one
nobody could get out of. **Pointer resistance is not applied**: a drag has resist and attract zones
because a hand is imprecise, and a key that moved a window by one cell except near an edge would be
a key that lies. Every intermediate rectangle still goes through `kwm_fit`.

### The scratchpad

**One window a session may keep over every other window.** `Super+grave` shows it on whatever
workspace is being looked at, focused, spanning the work area's full width across the top half of
its height —
the drop-down shape every terminal on that key has had since Quake put a console there. The same
key hides it again. When no window has the role, the first press opens the terminal `con.conf`'s
`terminal` key names and gives it the role.

**It is two flags on an ordinary window and not a fourth kind of one.** `sticky` says the window is
on no workspace and therefore on every one — `workspace` is not asked about it anywhere, and it is
counted in no workspace's occupancy, because a pager dot under every number would say the desk was
full when one window was open. `hidden` says it is drawn nowhere, listed nowhere and under the
pointer nowhere. Because they are ordinary fields, `Super+Alt+grave` can hand the role to something
already running: the window that had it comes back onto the workspace being looked at as an
ordinary window, since one left sticky and hidden with no chord naming it is one nothing can reach.

**Hidden is not minimised, and the difference is the taskbar row.** A minimised window keeps its
row because the row is the way back — it is drawn nowhere else and cycled past, so dropping the row
would leave the chord as its only route. The scratchpad already has a chord, so a row as well would
be a second way back, drawn on every workspace since it is on none. Both states answer the show:
minimise the scratchpad from its own frame and `Super+grave` still brings it back, rather than
hiding an already invisible window and needing a second press.

**The shape is applied on every show rather than remembered.** The grid can be resized while the
scratchpad is away, and a remembered rectangle would bring it back partly off the screen — or, on a
screen that had shrunk, not onto it at all. While it is *shown* it is an ordinary window and a grid
resize refits it like one; the shape comes back on the next show.

**A saved session remembers the role.** The record's flag column carries what a rectangle cannot
say, so a scratchpad comes back as the scratchpad and a fullscreen window as fullscreen — both are
states that *replace* a rectangle, and a row of four numbers restores neither. A minimise is not in
the column: that is where one window was put for a minute, not part of how a screen was arranged.

**It is left out of tile and cascade, as a fullscreen window is.** Both were put where they are on
purpose, and folding either into a grid would undo a request nobody withdrew. It is also not sent
anywhere by `Super+Shift+`*N*: a window on no workspace cannot be moved to one.

**On the compositor it is the same flag and the same key.** `W-grave` runs a `ForEach` over the
`kdos-scratchpad` app id: `ToggleOmnipresent` on the view it finds, and `foot --app-id
kdos-scratchpad` when it finds none. Omnipresence is labwc's word for what `sticky` says here — the
view belongs to no workspace, so it is on the one you are looking at — and pressing the key again
takes it off, which puts the view back on the workspace it was opened on. The compositor has no
state for a window that is drawn nowhere without a taskbar row, so that half of the console's rule
has no equivalent there and the flag is where the two desktops meet.

### Where a window opens

**A window opens where that program's window last was.** The rectangle is written when the window
goes and used when one running the same program next appears, so an application stops opening in
the middle of the screen at the size its author picked. `con.conf`'s `remember` turns it off, in
both directions: a person who does not want the behaviour does not want the file written either.

**The key is the program, not the app id.** Every terminal's app id is `terminal` and every caged
guest's is `kdos-cage` — an app id says what *kind* of window this is — so a table keyed on one
would give the whole desk a single shared rectangle. `prog` is what the window was opened for,
written once and never rewritten by the guest, and for a native surface it is the client's own
name. A window that named no program is remembered for nothing.

**A record is per program *and* per workspace.** The same editor on workspace 1 and on workspace 3
is two windows a person arranged separately, and one line for both would make each opening move
the other.

**Which windows are remembered is decided by the function a caller reaches for**, not by a flag.
The lookup happens inside `win_place()` — the placement every ordinary window goes through — so an
overlay, which is placed by `win_place_corner()`, cannot inherit a terminal's rectangle, and a
restored session, which is placed by `win_place_at()`, still wins. Chrome is excluded by role on
top of that: a docked panel, a layer, the lock, the saver, a guest on another terminal and the
scratchpad are all put where they are by something other than a person.

**A tiled window is remembered by what an untile returns to**, never by the half of the screen it
is currently filling, and the tile itself comes back from the last field. **A remembered rectangle
is fitted, not trusted**: it goes through `kwm_fit()` into the work area, so one kept on a wide
screen still comes back onto a narrow one. And **a second window of the same program does not land
on the first** — one record per program means every instance would take the same corner, so a
record whose origin is already occupied is declined and the placement search does its job.

**It is not the compositor's file, and it cannot be.** `kdos-comp` keeps the same idea in
`~/.local/state/kdos/winpos` under `comp.conf`'s `window_memory`, but its rectangles are **pixels**
and these are **cells**: one file with both writers would restore every window at a size taken from
the other desktop's units, an eighty-column terminal coming back eighty pixels wide. What the two
desktops share is the rule, not the row.

### Arrangements, with a name

**A layout is the session record with a name.** `kdos con layout save <name>` writes what is open to
`~/.config/kdos-con/layouts/<name>`; `kdos con layout load <name>` opens every entry that is not
already open and closes nothing. Same rows and same reader as the file a clean exit leaves behind,
because two formats for one idea are two things to keep in step.

**A row names what to open and never how.** `term` is `con.conf`'s `terminal`; a role — `files`,
`mail`, `writing` — is that `con.conf` key opened in a terminal; a command key such as `monitor` or
`notes` is one of this desktop's own surfaces; anything else is an app id for the pack store. One
resolver answers for both files, so a restored session and a loaded layout cannot disagree about
what a row means.

**A terminal's row names the role it was filling.** Every terminal window's app id is the literal
`terminal`, so a row carrying that says a window *was* a terminal and not which program was in it —
and an arrangement saved and reloaded would come back as a screen of bare shells. The row says
`files` and `con.conf` says what fills it, which is the indirection the chord that opened it used
and is not a command line, which the file must never hold.

**A load adds and never takes away.** A row whose program is not installed opens nothing and is not
an error; a row already open opens nothing either, so a layout is safe to ask for twice; and a
`term` row always opens, because no name separates one plain shell from another.

**Three ship** — `work`, `write`, `talk` — under `/usr/share/kdos/layouts/`, each with a
`layout.<name>` route in the palette. A person's own file of the same name replaces the shipped one
rather than merging with it: an arrangement is a whole statement about a screen, and half of one is
not an arrangement.

## Mark and transfer

**The session composes every window into one grid, so the text on the screen is text.** Marking a
rectangle of it is a read of a buffer that already exists — no protocol between two programs, no
selection ownership, and no cooperation from the program being marked. It works over a session
terminal, a `libkcon` surface and an embedded graphical application rendered as characters, because
all three are cells by the time they are here. DESQview had this in 1985 and it is cheaper on this
desktop than it was there.

`Super+Shift+m` starts it over the focused window's first content cell. Arrows place the corner,
`Shift+`arrows drag the other one out of it, `Ctrl+`arrows move by eight, `Home` and `End` reach the
edges, `Return` copies and `Escape` leaves. The taskbar names those keys while the mode is up.

**The mark owns the pointer as well as the keyboard.** A press anchors the rectangle, a drag draws
it and the button coming up finishes it, the same as `Return`. Nothing under it is raised or
clicked while the mode is on: a press that fell through would put a window over the region being
marked, and a capture would then photograph that.

**The rectangle is the screen's, not a window's.** Somebody marking a column of numbers out of two
windows side by side means the column, and a mark that stopped at a frame would be a mark that knew
better than they did.

**The mark is drawn by reversing the cells under it, never by repainting them.** Those cells belong
to whatever program drew them and are what is being copied; a mark in a colour of its own would hide
the text a person is trying to see the extent of. A picture's cells mark as spaces.

**`Super+Shift+p` is a mark that also files a picture.** The same rubber band, the text of the
cells on the clipboard, and then `con.conf`'s `capture` program — `kdos-shot` by default — asked for
the same rectangle as `kdos-shot region --geom X,Y,W,H`, in cells. It attaches a second view to
rasterise them, so the picture is what a screen would show rather than a second drawing of the same
cells. **The mark is taken down before the picture is asked for**, or the rectangle would be
reverse video in the file.

**The colour picker is a LOOKUP, not a probe.** Every cell the session composed carries the slot it
was drawn in, so `kdos-shot colour` — the `capture.colour` route — asks the session with
`kdos-con --pick-colour`, the bar says *click a cell for its colour*, and the click is answered from
the frame that is already in memory. There is no screen to grab and nothing to sample. The answer
is the **ink** where the cell holds a character and the **ground** where it does not, because a
picker that always read the foreground would answer with the colour of a glyph nobody can see; it
goes on the clipboard as `accent #39ff14`, or as a bare `#rrggbb` for a cell a program painted in
truecolor — that literal is what the program chose, so it is what the person is told, with no slot
name in front of it. **The picker owns the pointer while it is on**, for the reason the mark does: a
press that fell through would raise a window over the cell being read. `Esc` leaves.

**It is the console's alone.** Under the compositor no protocol says where the pointer is — a client
is told when one enters its own surface and nothing more — so a picker there would have to *be* the
compositor. `kdos-shot colour` says so rather than doing nothing.

**A drag crosses the session, and the session is the only half that can carry it.** A surface says
it has picked something up with `KCON_OP_DRAG_START` — `kdos-desk` handing over an icon's URI — and
hears nothing more until the drag is over it. The session then sends all four verbs: `ENTER` with
the MIME type when the pointer arrives over a window, `MOTION` while it stays, `LEAVE` when it goes,
and `DROP` with the payload on release. Only two types are accepted, `text/plain` and
`text/uri-list`: anything else is a payload nothing here can act on, and taking it would mean
highlighting targets that would refuse the drop.

**The payload waits for the release.** A drag crossing six windows would otherwise hand its bytes to
all six, and five of those are windows somebody was only passing over.

**A drop reaches the icon layer, which a click does not.** `win_at()` skips the background
deliberately — it covers the whole grid, so hit-testing it before the windows would take every click
on the desktop — so a drag asks it **last**, after every window has declined. Without that the whole
path is dead where it matters: a release over the desktop would find nothing, be treated as a
cancel, and the trash would never see it.

**Three ways out, because the drag was started by another program and cannot be asked to stop.**
`Esc` gives it back; a source that dies mid-drag ends it, or a dead program's payload would land on
whatever was under the pointer at the next release; and locking the screen ends it, because the
release that would have finished it goes to the lock surface instead. The bar says what is being
carried and names the way out, as it does for the mark.

**A dropped session leaves you at a working prompt, and `kdos con attach` is the one thing to
type.** A `--tty` view leaves through one exit whatever ended it, and that exit puts the host
terminal back: the alternate screen closed, the palette restored, the modes off, the cursor
visible. Two things end it. The **socket** dying is a session that has gone — `kcon_conn_dead()`,
which a write returning `EPIPE` sets as surely as a read returning end of file. The **terminal**
dying is an `ssh` connection that dropped while the session carried on, and that one is a write to
the pty failing: `ktui_term_hungup()`. A view that watched only the socket would paint frames into
a hung-up descriptor for as long as the session ran, and the terminal it was given would never be
handed back.

**Not on `SIGHUP`.** `kdos theme` sends that signal to every view to retint it, so tearing down on
it would make an accent change end every `--tty` view on the machine — and a kernel hangup is the
same signal, so the two cannot be told apart. The write failing is the fact; the signal is
ambiguous.

**A terminal window outlives its program, and the modes go with the program.** The window stays
showing how the command finished until somebody dismisses it — so the first time the death is seen
the terminal's modes are put back: bracketed paste, mouse reporting, focus reporting, synchronized
output, and the alternate screen. Once, not on every pump. What was printed stays; see
[kdos-term](kdos-term.md#what-it-has-been-run-against).

**A program a chord starts gets no console.** This session's own stdout is the tty the composited
grid is drawn on, so a child that inherited it would write over the desktop — and a program deciding
whether it has somewhere to print would be told yes, on a terminal nobody can read. `/dev/null` in
and out; stderr is left alone, because the session's is already the log and that is where a
diagnostic belongs. It is what lets `kdos toggle` and `kdos remind ls` tell a chord from a prompt.

**The three `Print` chords reach a KMS view and nothing else**, for the reason the media keys do:
`Print` produces no character, so no terminal reports one and a view reading a terminal never sees
it. `Print` is the whole screen, `Shift+Print` is the rectangle — the bare key cannot be the
rectangle here, because a region is drawn with the session's own rubber band and a bare `Print` has
nothing to draw with — and `Alt+Print` starts the screen recording and stops it. `rc.xml` binds the
same three the same way round, so there is one key card. The `Super` chords are the way to the same
verbs over ssh and in a `--tty` view.

**`Super+Ctrl+c` opens the palette at the capture group** — `con.conf`'s `capture_menu`, which is
`kdos-palette --route capture` — because the group has more verbs than a keyboard has chords worth
spending. `capture.qr` photographs a region, reads a QR code out of it with `zbarimg`, puts the
text on the clipboard and **removes the picture before it returns**: a QR on a screen is a wifi
password or a pairing token more often than it is a URL, and one left in `~/Pictures` is that
secret kept where nobody meant to keep it. Nothing is written under `~/Pictures` on that path at
all — the picture goes to the runtime directory, which is the session's own and mode 0700.

**`Super+Ctrl+h` opens the palette at the setting-up group** — `con.conf`'s `setup_menu`, which is
`kdos-palette --route setup`. Eleven panels have a chord of their own and the printers, the users
and the clock do not; a chord each would be eleven more keys to learn, and a person looking for the
printers is looking to **set something up** rather than for a program name. It is the capture
group's neighbour on the keyboard because it is the same shape of answer: a group reached by its
route rather than a second menu written for it.

## Scripts

**A script is the keys somebody typed, played back.** `Super+Shift+r` starts recording every key
the session routes to the focused window and the same chord stops it, asking on the taskbar for a
letter to keep it under; `Super+Alt+r` then that letter types it into whatever has the focus now.
Files live at `~/.config/kdos-con/scripts/<letter>`, one line per key.

**A script is keys into a window and never a command.** The file holds a chord's name, its
modifiers, its key number and the gap before it, and there is no field that could name a program —
so a file somebody plants in that directory types into a window and cannot start anything. This is
the whole of the format, and it is deliberately too small to hold a command.

**The name column is for the eye and the numbers are what is replayed.** The name is the chord
spelled the way `keys.conf` spells it, written through the same table the session binds chords with,
so a person can read a script without decoding key numbers. It is not parsed back; a file whose name
and numbers disagree replays the numbers.

**Nothing is recorded and nothing is played while the screen is locked.** A lock is typed into with
a password, so a recorder underneath one would write it to a file and a replay into one would be a
guess at it. The refusal is checked when a recording starts, again for every key, when a replay
starts and again on every turn of it — a screen can lock on its own timer in the middle of either.
The greeter needs no rule of its own: `kdos-con-login` is a separate process and no session, and so
no recorder, exists while it is up.

**A replay leaves on the session's tick, one key at a time.** The gap between two keys is the gap
they were typed with, capped at twenty milliseconds, and the keys are delivered from the same loop
that polls everything else. A replay that slept between keys instead would hold the whole desktop:
nothing would repaint, and a person would watch a frozen screen produce a finished paragraph.
`Escape` stops one.

**A replayed key goes into the window and never through the chord table.** What was recorded reached
a window, and that is where it goes back — a replay routed through the session's own keys would fire
whatever a file happened to contain, a workspace switch or a quit, from a file.

**The directory is 0700 and the files 0600**, created with the mode rather than chmod'd afterwards:
between the two there is a file somebody else can read, and what is in it is whatever was typed.

**`kdos con` gained no `script` verb, and neither did the protocol.** A recording starts and a
script plays from the chord table and from nowhere else, so a client on the surface socket can do
neither: a verb that could would be a way to type into the session, and to read back what somebody
had typed, for anything that can reach the socket. The only client that reaches this at all is a
driving view, which is the keyboard by definition — and an observing view is refused every key it
sends, so it cannot press the chord either.

**The key card lists the letters that have a script and the first ten keys of each**, read out of
the directory by a program running as the same person rather than asked for over the socket. A
letter with no script is a chord that does nothing and a letter with the wrong script types into the
wrong window, so which letters are taken has to be visible somewhere.

## The media keys

**They run a program, and that is the whole of them.** What "louder" means belongs to the mixer and
what "next" means belongs to the player; a window manager that decided either would be a second
answer to a question something else already owns. `con.conf` names the seven programs —
`kdos-osd volume +5` and its peers — and the chord table names the seven keys.

**A KMS VIEW ONLY, and this is a limit of the wire rather than of the code.** A media key produces
no character. A backend reading a terminal is shown characters and modifiers and nothing else, so
no terminal reports one and none ever will; the keys reach a session through `libkkms`, which reads
the evdev keysym directly. On `tty1` they work and over `ssh` they are absent — and the chord table
still carries them, because a stated limit is better than a missing key.

**The key codes are appended to `libktui`'s enum, never inserted.** It is positional from
`KT_K_SPECIAL` and **the number is on the wire** — a session writes it into `KCON_OP_KEY` — so a key
added in the middle renumbers every key after it and a surface built before the change reads `Home`
where the session sent `End`. `keys.conf` is safe either way: it stores names.

**What is playing sits left of the pager**, and the session does not know what it means. `kdos-mpctl
watch` asks mpd over its unix socket, writes one line to `$XDG_RUNTIME_DIR/kdos/nowplaying` and then
sleeps inside mpd's own `idle` — polling a daemon for a title that changes every few minutes is a
wakeup a battery pays for. The bar reads that line at most once a second, because `panel_draw()` runs
on a 20 ms poll and an unthrottled read would be fifty opens a second.

**This bar is the one drawn when `kdos-shell`'s panel is NOT up**, which on a booted machine it is —
so the same line is read by [that panel's `mpris` widget](kdos-shell.md#the-notification-area),
which falls back to the file when nothing on the bus speaks MPRIS. One line, two readers: two cells
that disagreed about what is playing would be worse than one that is sometimes empty.

**A stopped player writes an empty line and the field disappears**, rather than the bar saying
"stopped": a row with nothing playing should look like a row with nothing playing. The field is
drawn whole or not at all, inside a third of the bar, because the window list is what the bar is for
and a long track title that pushed it off an eighty-column screen would be a music player eating a
task switcher. It is not drawn on the function-key row at all — ten labels and the word `Super`
already end two columns from the clock. `nowplaying = no` in `con.conf` turns it off.

**A screen recording lights a `•REC` lamp between the pager and the clock**, in `KT_ERR` — the
urgent slot, because a recording somebody has forgotten is running is a recording of whatever they
do next. `kdos-record` writes its pid to `$XDG_RUNTIME_DIR/kdos/screencast.pid` while its pipeline
runs; the bar reads it once a second and **checks the process is still there**, so a marker left by
a crash is not a lamp nothing can put out. The bullet comes from the glyph table rather than being
written into the source: the console font is 512 glyphs and carries no `●`, so `KT_G_BULLET` is `•`
where UTF-8 reaches and `*` where it does not, and the lamp is four columns on every view. It is
**frozen off under `KDOS_CON_DUMP`**, like the clock and the now-playing line — a golden made on a
machine that happened to be recording would fail everywhere else.

**That lamp is not the row-wide `RECORDING` banner.** The banner is the keystroke recorder
(`Super+Shift+r`) and it takes the whole bar, so the two are never on screen at once and cannot be
read as one thing.

`Super+Shift+v` puts the session clipboard into the focused window — `kvt_term_paste` for a
terminal, `KCON_OP_CLIP_DATA` for a surface. A view can already hand the session a paste, but a
`--kms` view has no host terminal to take one from, so on `tty1` this chord is the whole of it.
There is no `Super+Shift+c`: the terminal's own copy is `Ctrl+Shift+C` inside the window and the
mark is the session's, and a third copy chord would be a third thing to explain.

**Show desktop remembers the set it hid.** A window already minimised when the chord was pressed was
minimised on purpose and is not brought back, and a window opened while the desktop is showing is
left alone.

**The session's own window list is the fallback, not the desktop's.** Arrows and digits pick,
`Enter` raises, `Delete` closes, `m` minimises, `Escape` leaves. It is what a session with no shell
running has, and it is drawn from the list the session already holds. `kdos-teams` shows the same
windows through `libkdisp`, which is the list every other surface reads.

**Tile, cascade and rearrange are the console's alone.** labwc has no tile-all or cascade action and
its `MoveResize` is not the same interaction, so binding the nearest thing there would make one
chord mean two different things on the two desktops — which is the one rule `keys.conf` and
`rc.xml` exist to keep.

## Reaching another terminal

**Ctrl+Alt+F1 to F12 switch virtual terminals, and `libkkms` acts on them.** `libseat` putting this
VT into graphics mode is what stops the kernel answering the chord, so a desktop that did not offer
the switch itself would take away the tty2 recovery console `/etc/inittab` exists to guarantee.

**It is a keysym, not a chord.** xkb resolves Ctrl+Alt+F2 to `XF86Switch_VT_2` before any modifier
reaches a caller, so code looking for F2 with two modifiers held finds neither and the switch
silently does nothing. It is answered in `kkms_input.c`, which is the only place the keysym exists —
and that is also the half holding the seat, so the session keeps its property of linking no device
code and coming up on a machine whose GPU driver does not.

The chord is not a session binding and is not in `keys.conf`: it is not rebindable here for the
same reason it is not rebindable on a bare console. Coming back finds every window where it was —
the session never stopped, and a view that lost its devices to the switch gets them back and
repaints in full.

## Known limits

- **A picture needs a `kdos-term` window.** The session's own terminal windows link no pixel code —
  that is what keeps `kdos-con` free of a font renderer — so they show the fallback shade where a
  picture is. [`kdos-term`](kdos-term.md) is the terminal that decodes one, and it reaches this
  desktop as an ordinary cell surface.
- **The session keeps no copy of a picture, and asks for it again instead.** A sprite is forwarded
  to whatever is attached when it arrives; a session that cached every one would be holding
  megabytes of pixels it is otherwise built never to touch. So when a display attaches, the session
  tells every surface to start again: each forgets what it has sent, and its next flush puts every
  picture back on the wire before any cell. A reattached display therefore fills in, at the cost of
  re-sending — which is the same cost an animation already pays per frame.
- **The pointer moves a cell at a time.** A press and a release carry where in the cell they landed,
  so a small button is clickable; a drag that stays inside one cell moves the guest's pointer
  nowhere, because the input stream reports a move when the cell changes.
- **No VT has ever been allocated.** The `--vt` path compiles and links and has never been run: it
  needs an ISO with `kdos-cage` in it and a machine with real terminals. Embedding, which is the
  default, has been run end to end.
- **No input method.** fcitx5 is a Wayland client.
- **Single output.** `libkkms` takes the first card with a connected output and its preferred mode.

## See also

- [The session](../03-architecture/session.md) — two sessions, one bring-up
- [Boot and init](../03-architecture/boot-and-init.md) — the tty1 chain
- [kdos-comp](kdos-comp.md) — the other desktop, and libkwm's other caller
- [Filesystem and IPC](../06-reference/filesystem-and-ipc.md) — both sockets and every verb
