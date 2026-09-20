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
- **The session's own death is written to its log, not only to the screen.** A restart clears the
  screen within a second, so the line naming the exit status is otherwise unreadable by the time
  anybody looks — and every window going away is then indistinguishable from a logout.
  `$XDG_RUNTIME_DIR/kdos-con.log` is appended to across restarts, so it is the one place that says
  whether the session ended or crashed, and a status of 139 is a segmentation fault.
- **A desktop over ssh.** The view socket is forwardable and the view is trusted with nothing. The
  server accepts from a view only what a display has to say — its hello, the input it carries, the
  frame it painted, what it was pasted, what it can show, and that it is leaving — and drops every
  other client verb on arrival, the clipboard, drags and the session's picture slots included. It is
  a list of what is allowed rather than of what is refused, so a verb added later does not reach the
  far end of an ssh link by default.
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

**And `ACTION`, which names one of the SESSION's own verbs rather than a window.** `tile`,
`cascade`, `show-desktop`, `windows`, `lock` — the things that act on the whole desktop, which were
bound to chords and to nothing else, so a pointer could not ask for any of them. The desktop's root
menu is where they belong and `kdos-desk` is the surface that draws it, which is why the icon layer
asks for `manage`: it is the session's own chrome, started beside the panel by the session script,
and that is the line the privilege is drawn on.

**A NAME AND NEVER A CHORD.** A surface that could send a chord could send *any* chord, including
whichever one the machine's `keys.conf` has a shell command on. The name is looked up in the bind
table and **turned into its chord**, which is then fed to the one key handler — a second dispatcher
keyed on names would be a second copy of the mapping from verb to effect, and the day the two
disagreed the menu row would do something other than the chord printed beside it. A name this build
does not have is dropped, which is what a surface newer than the session it is talking to must get.

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
| `OVERLAY` | above every window | no | no | no | if it asked |
| `BACKGROUND` | below every window | no | no | no | never |
| `LOCK` / `SAVER` | instead of everything | no | no | no | lock only |

**Three passes, because there are three layers.** Stacking inside a layer is the window list's own;
between them it is fixed and has to be — a menu a window could be raised above is a menu that
disappears behind the thing it was opened from, and desktop icons drawn last would cover every
window on the screen.

**A drop shadow is clipped to the work area.** It hangs one cell below and one right of the frame,
so a window sitting against a docked edge casts it onto the panel there — and a panel is a window
in the same layer, drawn before whatever is above it, so nothing repaints those cells afterwards.
The work area is exactly the rectangle a window may cover, which makes it the clip.

**And a window can be seen through.** `window_opacity` and `panel_opacity` in `con.conf` are per
cent; below 100 the rectangle is drawn opaque and then mixed back towards whatever it covered, so a
terminal shows the desktop or the window under it. The ink is never mixed — text, rules, chips and
widget fills keep the colour they were drawn in — and an embedded application's pixels are its own
and are never touched. A display that cannot carry a colour outside the eight slots shows every
window opaque, which is the honest answer where there is nothing to mix with.

**One window may disagree with the file.** `Super+Ctrl+=`, `Super+Ctrl+-` and `Super+Ctrl+Alt+0`
step the FOCUSED window's own transparency and give it back — the font family with Ctrl added,
because both step something about what is in front of you. Which window should be seen through is
decided while looking at it: a reference under an editor, a terminal over a picture. The step starts
from what is on the screen, so the first press moves one notch from what you are looking at rather
than jumping to a fixed number, and it stops at 20 per cent, because there is no chord that brings
back a window nobody can find.

**The shadow darkens and does not erase.** It hangs a column right of the frame and a row below it,
and every cell it covers keeps its glyph while both halves are mixed towards `KT_BG`. A shadow that
wrote a blank cut a rectangular bite out of the window underneath — and out of an embedded
application's picture, where it read as a compositing defect rather than as depth. A picture is not
shadowed at all: a sprite cell is somebody else's pixels edge to edge, with no background of ours
behind them to darken.

**An overlay takes the keyboard if it asked for it, and a background never does.** The Start menu,
the launcher and the run box are overlays and are answered by typing, so one that did not focus
would be a menu nobody could drive. The icon layer covers the whole grid and sits behind
everything; focusing it would take the keyboard from the window a person is working in every time
the desktop redrew.

**And the ones that said no must not.** A tooltip, a toast and the candidate window set
`KDispConfig.keyboard` to 0 — they are drawn over somebody's work and have no business taking the
keys. The attach carries that bit, and a session that focused them anyway unfocused whatever they
appeared beside: every menu on this desktop closes when it loses the focus, so hovering the `Start`
button killed the menu that button had just opened. An attach that says nothing about the keyboard
is taken to want it, which is what every surface did before the field existed.

**The session says who drew the frame, and the client never guesses.** `kdisp_decorated()` means
"somebody else drew my furniture, so I must not": `kdos-term`, `kdos-res` and every `kdos-shell`
window ask it and draw their own box when the answer is no. libkcon answered a flat **no**, so on
this desktop every one of them drew a second box inside the session's frame with the title written
twice — a terminal running `btop` showed three nested borders. It is `KCON_OP_DECORATED` now, sent
when the answer changes, because the window model is the only thing that knows it: a panel, a
layer, a background and a **fullscreen** window are all drawn bare, and a client deriving it from
its own role would be wrong the moment a window went fullscreen and right again when it came back.
It is not a field on the configure — a configure that does not change the size is dropped on
purpose, and a decoration that changed without one would be dropped with it.

**And a window that renames itself is noticed by diff.** A shell writes `OSC 2` on every command.
`kdisp_set_title()` carries it — the session's own `KCON_OP_TITLE` here, `xdg_toplevel_set_title`
under the compositor — and the session compares each surface's title against the one it is drawing,
in the same walk that publishes the decoration. Hooked at the message it would be a second place to
be wrong; `mgmt.c` already republishes the taskbar from a diff for that reason.

**A surface says which of its cells answer the pointer.** `kdisp_input_cells()` is
`wl_surface.set_input_region` on the other transport and means the same thing here: all of the
surface by default, and a count of zero for one that takes nothing at all. Four surfaces declare an
empty region — the tooltip, the toast stack, the candidate window and the saver — because the thing
*under* them is what a click is aimed at. A hit test that ignored it handed the topmost rectangle
every click, so the tooltip describing the `Start` button swallowed the click on it. A region longer
than sixteen rectangles is refused whole, back to all of the surface: a list cut short would leave
the rest of it taking clicks the client said it would not.

**The pointer leaving a surface is reported**, as the off-grid position `(-1, -1)` libkwl sends for
`wl_pointer.leave`. Every consumer already maps a coordinate to "which control is this" and that is
none of them. Without it the last thing the pointer crossed stayed hovered for the rest of the
session — which left the `Start` button drawn in its opened colours, and on a display with no pixel
plate that is a button with no label at all.

**A background is the work area, and never a size its client named.** The icon layer *is* the
desktop — everything the bars left — which is what a layer surface anchored on four edges with no
exclusive zone of its own gets under the compositor. It attaches with no size, the way a saver
does, and the configure answers. One that took the client's own number was placed by the
minimal-overlap window search at the 80x24 a client fills in when it has nothing better to say:
icons in a corner of the screen, the desktop's hint row and its context menu stranded in the middle
of it, and every click outside that rectangle reaching nothing at all.

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

**And a grid that changes size RE-ANCHORS every overlay rather than fitting it.** The session
starts at a fallback 80x24 and becomes the view's size the moment a display attaches, so every
session resizes once, at boot, before anybody looks. `kwm_fit` moves a rectangle the least it can
to get it inside the new area, which for a corner surface throws the corner away — the welcome
card, correctly centred in that fallback grid, stayed in the top-left corner of the real one on top
of the desktop icons. The same walk leaves docked panels alone and assigns the background the work
area, for the same reason in both cases: a rectangle that is derived from the grid has to be
derived again, not nudged.

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

**Two things suppress all three steps.** The `stay-awake` toggle is the person's own, set by
`Super+Ctrl+i` and held until they clear it. An embedded guest playing something is the other: it
sends `KEMBED_INHIBIT` and the session suppresses the saver, the lock and the blank for as long as
it holds — but only while the guest's window is one somebody can see, because an inhibitor honoured
for a minimised window is a battery spent on bookkeeping.

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

**`kdos-con --greet --dump COLSxROWS` composites the login surface and returns.** It asks for no
password and starts no session, and the accounts and sessions come from the file
`$KDOS_GREET_FIXTURE` names rather than from `/etc/passwd` and the installed programs — a golden
drawn from the machine would change the day somebody added an account. The frame is the **ascii
tier**, like every other golden in `testing/goldens/`; on the real tty the same layout is drawn in
the vt tier, because `kdos-getty` has loaded the 512-glyph console font before the greeter runs.

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

**A frame a program brackets is composed whole, in a session window as in `kdos-term`.** A curses
program writes one screen row per `write()` and a pty holds 12 KiB, so a full-screen frame does not
cross in one piece: at 236x63 one compose in thirteen would otherwise show the top of the new frame
over the bottom of the old. Synchronized output (`DECSET 2026`) is the program saying where its
frame begins and ends, and the session honours it — **one implementation of the rule in `libkvt`**,
so a program need not know which of the two terminals it is talking to.

**A held window is composed from its last whole frame, not skipped, and the hold reaches no other
window.** Both follow from the compose: the session clears the grid and repaints every window on one
composed frame — paced by the attached screens, with 16 ms as the widest that period gets — so a
window that drew nothing would be a hole showing the backdrop and a window that delayed the frame
would stop the desktop for one program's frame. Every other window composes on that same frame while
one is held. **Under `libkvt`'s 150 ms watchdog** — a child that sets the mode and
then dies, blocks or is stopped is composed live again on the next tick, which is what makes a hold
safe to honour at all. The buffer is taken when the first bracket opens, while the grid still holds
a whole frame, and only a terminal that has opened one carries it: a program that never brackets
anything composes exactly as it would with none of this here.

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
**innermost column of the frame's left band** — the border is the window manager's and every column
inside it belongs to the child, and a mark on the band's outer column would sit two cells from the
line it marks and break the frame's own rule to do it. The chord is claimed whether or not it moves, so a shell that emits no marks
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

**A display keeps a block's buffer and resamples the next frame into it.** The resample is `SRC`
over the whole image, so every pixel of the destination is replaced and nothing of the previous
frame can show through; a new buffer is taken only when the block's pixel size moves, which is what
a resized block, a font step and a mode change each look like from the display. An embedded guest
republishes every block of its window on every frame and a block at an 8x16 cell is 128 KiB, so a
buffer per block per frame is a mapping the kernel faults in, zeroes and hands straight back —
measured at 2,475 minor faults and 5.08 ms for a 1080p window of blocks, against none and 0.45 ms
when the buffer is kept. **The picture belongs to the sprite table the moment it is registered**:
exactly one reference to it exists, the table's evictor is the only place it is dropped, and the
display forgets its own pointer there — so an eviction, a refused registration and a table cleared
by a font change all leave the display with no way to write into freed pixels.

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

### A graphical application is windows

`kdos-cage --embed` composites it in a **process of its own** and hands the frames back over a
private descriptor channel; the session cuts each frame into sprites and writes the sprite
codepoints into the window's cells. `kdos-con` still links no wlroots, no mesa and no pixel library
— it moves bytes it never looks at.

**One cage is one application, and an application is as many windows as it maps.** A toolbox, an
image window, two docks and a modal file chooser are five windows on this desktop: five rectangles,
five titles, five places in the stack, five mappings. They cannot be one window — five toplevels
composited into one framebuffer are five pictures nothing downstream can separate — and they cannot
be one cage each, because the toplevels belong to one client on one display and a mapped surface
cannot be moved to another compositor. So every message on the channel names the window it is about,
the cage gives each toplevel an output and a mapping of its own, and the only thing a fork still
means here is a new application.

**A window is created without a fork and retired without one.** `KEMBED_OPEN` carries the toplevel's
natural size, its owner, what kind of window it is and its name; the session builds a window around
it, places it and tells it what size it actually got. `KEMBED_CLOSE_WIN` takes one out — and touches
the process not at all, because an application whose last window closed is an application with no
window, which is what a shared session bus is for.

**A launch puts a startup card on the desktop and the first ordinary toplevel claims it.** The card
is a small window — thirty cells by three, `CON_CARD_W` and `CON_CARD_H` — carrying the
application's name, a progress bar and the stage the launch is on, and it stands from the fork until
the first frame. Not the first toplevel of *any* kind: an application whose splash maps first would
give the splash the rectangle the person keeps the document at. An owned window never claims it
either, because a dialog is a question about a window that has to exist first.

**The card and the guest's output are two rectangles, and that is the whole of the design.** The
output is the rectangle the window is going to *be* — what the geometry memory chose for this
program, or half the work area when nothing is remembered — and it is cut, allocated and named in
the cage's `--embed` at the fork, because it is also the size the toolkit lays itself out at. The
card is only what the *window* is. So the card opens out at the first frame into an output that is
already the right size: nothing is reallocated, no `KEMBED_SIZE` goes out and the application is
never laid out twice. Shrink the output to the card instead and the application comes up the size of
the card, which is the failure the split exists to avoid. `Win.starting` is the flag, and everything
that reflows a guest — `embed_resized()`, a size report — reads the output's rectangle while it is
set.

**A card's rectangle is remembered nowhere.** `geo_worth()` refuses it exactly as it refuses a
splash: a launch that dies before its first frame would otherwise write thirty by three to the
geometry table as the size this program opens at next time, and every launch after that would open
as a card and record one.

**The claim is provisional until the toplevel draws.** xdg-shell has no splash role, so a Wayland
startup window arrives naming no owner and no kind — indistinguishable here from the document window
— and the one thing that separates the two is that a startup window paints nothing and retires. A
claim retired before the first frame therefore gives the card back, and the next ordinary toplevel
takes it, at the same rectangle, over the same output. Without that, the document window is a
*second* toplevel with a window and an output of its own, and the person watches a large window open
and be replaced by a smaller one.

**A guess is replaced by the toplevel's natural size and a remembered rectangle is not.** The launch
is forked before there is anything to measure, so when nothing is remembered the output is half the
work area — and a guest whose first window is a welcome card or a small tool then renders that
window at its own size inside a frame twice as wide, over the cage's background. The natural size
`KEMBED_OPEN` carries is therefore taken as the output's the moment it arrives, rounded up to whole
cells, but **only while the placement was the guess**: a rectangle out of the geometry memory is the
one the person last left this program at, and replacing it with the toolkit's default is the window
forgetting where it was kept. Measured once, at the placement, because a card that is claimed,
released and claimed again would otherwise refuse the second claimer the size the first was granted.
A guest that reports no size keeps the guess.

**What kind of window it is decides where it goes, and only a dialog crosses from Wayland.**
xdg-shell has no way to say modal, utility or splash — a Wayland toplevel is a dialog when it names
an owner and nothing when it does not — so those three reach the session from an Xwayland guest
alone and every rule below degrades to *owned or not* without them.

| Kind | Where it opens | Taskbar | Ring | Frame | Stacking |
|---|---|---|---|---|---|
| Ordinary | the minimal-overlap search, or where it was left | a row | yes | yes | its own |
| Dialog (names an owner) | centred on its owner, at its own size | none | yes | yes | above its owner |
| Modal dialog | centred on its owner, at its own size | none | yes | yes | above its owner, which is blocked |
| Utility (a dock, a toolbox) | where the eye is, at its own size | none | yes | yes | its own |
| Splash | centred on the work area | none | no | none | above everything |
| Any of them while a modal is over it | — | unchanged | **no** | yes | under its modal |

**One application is one row.** A dialog, a dock and a splash belong to a window that already has
one, so an editor with four docks and a file chooser open is one entry in the taskbar and not six —
which is what every other desktop does and the whole difference between a bar a person aims at and a
bar that grows a button every time a question is asked. They are all in the Alt-Tab ring except the
splash, because a dialog is exactly what a person is switching to and a splash is something they
could get stuck on. **A window with no row of its own cannot be minimised on its own**, and its
frame carries no minimise button: the row is the way back from a minimise, so putting one away by
itself would leave it drawn nowhere, cycled past and in no bar. A question is answered or closed.
Minimise the OWNER and they go with it, and the owner's row brings the whole family back.

**A window that belongs to another rides its raises, travels with it and is remembered nowhere.** A
raise starts at the head of the family whichever member was named, so raising the owner brings its
dialogs up on top of it and raising a dialog brings its owner up underneath — a dialog on either
side of the window it is asking about is an application that looks frozen. Sending a window to
another workspace sends what it owns, and minimising one minimises them; naming a dialog resolves up
to its owner and moves that. And neither a dialog nor a dock writes the geometry table: every window
of one guest carries the same program name, so a file chooser that was remembered would write *its*
rectangle as the one the document window opens at next time, to disk, for every session after this
one.

**A modal blocks its owner and nothing else.** While it is up the owner cannot be raised, cannot
take the keyboard and cannot be closed; a raise aimed at it — from the directional search, from a
number chord, from a click — lands on the modal and flashes it, which is the honest answer to a
click on a window whose application has stopped answering about it. **The blocked owner is out of
the Alt-Tab ring for as long as the modal is up**, and so out of `Super+Alt+`*n*: a ring entry whose
every step is redirected back to the question is one the ring can never advance past, so it would be
Alt-Tab itself that stopped working rather than one window. The modal is in the ring in its place.
**The window list keeps the owner's row**, because a row is not a step — `Enter` on it is redirected
like any other raise — and the modal, being a dialog, has no row of its own to take its place. A
modal that is minimised, or off its owner's desk, blocks nothing: a
question the person cannot see blocking a window they can is a window that has stopped answering
with nothing on the screen to say why, and the flash would land where they are not looking.
Everything else on the desktop carries on, because a modal is modal to its application and not to
the machine: a dialog that stopped the session is a dialog a crashed application takes the machine
down with.

**An owner that closes leaves one window carrying its row.** The orphan nearest the front takes the
owner's place — its taskbar row, its place in the ring — and the rest are re-parented onto it,
so an application that outlives the window a person opened is still one entry and not one per dock.
Nothing stays modal, because there is nothing left for it to block.

**Closing one window is not closing the application.** `KEMBED_CLOSE` names that toplevel, the
application decides what it means — a save prompt is a window being used, not a close ignored — and
the window goes when the guest actually unmaps it. The deadline that escalates to a signal belongs
to the PROCESS and is armed only when the ask covers the last window a person can reach: a guest
signalled because one dialog of five ignored a close is every other window's unsaved work gone. A
guest that answers — by retiring the window, by opening a question on it, or by drawing one inside
it — is on no deadline at all, and closing such a window a second time after the bar has offered it
is what takes it. The ladder is [further down this page](#a-graphical-application-is-windows).

**Sprites, not a pixel rectangle painted beside the grid.** A sprite lives *in a cell*, so a window
in front of an embedded one simply overwrites those cells and the occlusion is the z-ordered copy
that was already there. A rectangle drawn over the grid would cover whatever was above it, and
every window-model question — stacking, snapping, workspaces — would need a second answer for one
kind of window.

A picture is at most sixteen cells square, which is what the cell's sprite encoding carries, so a
window is a grid of blocks of at most that size and damage is rounded out to the blocks it touches.

**The block is cut smaller wherever sixteen cells would not fit one message.** A block is
`tile × tile × cell_w × cell_h × 4` bytes and a message caps at `KCON_MAX_PAYLOAD`, so the window's
tile is the largest that still fits: a 32-pixel cell is cut fifteen cells square and a 64-pixel one
seven. The side that chooses the pixel size is the side that has to keep inside the cap — a block
above it is declined by the encoder exactly as a block to a full queue is, so it stays owed for the
life of the window and is a permanent hole in it, re-cut and re-refused on every turn. A
smaller tile is more blocks and nothing else, and the ceiling on how many blocks a window may have
still holds — the cell that forces the smallest tile is also the cell that makes the grid
smallest.

**A guest that resizes itself moves its window, once per rectangle.** A frame from a cage is always
the size of the guest's *output*, which is the size this session asked for, so a client that commits
a smaller window inside it — GIMP's welcome card settling once its content loads — publishes a frame
that agrees with the session perfectly and has the cage's own background down two sides of it, and
one that commits a larger window publishes a frame cut off at the output's edge. `KEMBED_SURFACE`
carries what the guest thinks its window is; the session rounds it **up** to whole cells, resizes the
window through the ordinary window-model resize and so tells the guest the rounded rectangle back.
**Three rules stop the loop**, because a guest may answer the size it was just given with another
demand and nothing in the arithmetic separates a toolkit that is settling from one that is chasing.
A report the window already satisfies changes nothing — the cage repeats itself whenever the output
moves, and rounding up leaves nearly every guest permanently a few pixels short of its own output,
so without this the settled state would be a resize per frame. Any rectangle the window model
chooses — a drag, a tile, a font step — starts the count again, so a window whose guest has a
minimum of its own comes back to that minimum after every attempt to shrink it below. And a run of
fits nothing but the guest drove is **capped at four inside a second**: a toolkit that reflows two or
three times as its content loads is followed, and one that answers every size with a different size
is followed four times and then left with what it renders. A tiled or fullscreen rectangle is the
window model's and is never given away: a tile a guest could shrink is a hole in the layout.

**A disagreement lasts as long as the guest keeps it, and the two shells say so differently.** A
Wayland guest is measured off its committed geometry, which is only a size it chose once it has
answered the last one it was given: an empty configure list with no configure still waiting on the
idle that sends it is a client that has caught up, and one still loading, one frozen or one whose
toolkit is chasing past the cap of four reports nothing until it does. An **X11 guest is measured
off its `ConfigureRequest`** and nothing else — the request is the only size such a client can
state, it has nothing in flight behind it, and so it needs no settling and is reported the moment
it arrives. Every such request is answered where it lands, per ICCCM 4.1.5: the cage re-asserts the
geometry the window already has, which makes Xwayland emit the synthetic `ConfigureNotify` the
client is waiting on, because a redirected request that is neither granted nor acknowledged leaves
the client unresized and unpainted for good. While a guest of either shell states nothing the
session can act on, the band of the cage's background down its sides or the crop at the output's
edge stays. Nothing in a frame can close that gap instead: the framebuffer is the output the
session chose, so `KEMBED_BUF` can only echo the size the session already knows.

**A window with no frame yet says what it is doing.** The cage publishes nothing until a client has
mapped a window, so until the first frame arrives there is no picture and the session draws the
card: the application's name, a bar, and the stage below it. Sprite cells naming slots no display
has a picture for come out as the fallback mark — a window full of shade blocks, which reads as a
broken application rather than as one that has not started drawing, and a container takes the better
part of a minute to come up. A rectangle too small for three rows falls back to `starting…` on its
middle row.

**The bar counts stages that happened and never a clock.** There are four this end can see, and
`EM_CARD_STAGES` is them:

| Stage | What it is | What the card says |
|---|---|---|
| 1 | the cage is forked | `starting the box` |
| 2 | `KEMBED_HELLO` — backend, sockets and Xwayland up, guest forked | `starting the application` |
| 3 | `KEMBED_OPEN` — an ordinary toplevel claimed the card | `opening its window` |
| 4 | the first frame | — the card is gone |

So the bar stands at three quarters when the window it precedes appears and is never seen full. A
bar driven by a timer instead would run to the end while a container was still unpacking and stand
still while the application was drawing, and a person reads a full bar that is not finished as a
launch that has failed. A fifth stage would cost a protocol op and is not worth one: kembed carries
no version, only `KEMBED_MAGIC`, so a new child-to-parent op takes a free number in 15..63 and an
older cage answers it with a silence nothing can tell from a slow launch — and what it could add,
the box composed and the container started, happens in `kdos-appbox` below the cage, which does not
read its own child's stages either.

**Closing the card cancels the launch.** While it is the only handle the guest has put on the
desktop, its close is asked of every toplevel the guest has and escalates like any other last
window — and that includes a guest showing nothing but a splash, which carries no frame, no taskbar
row and no place in the ring, so there is nothing else on the desktop to close it by. Once the guest
has a window of its own that a person can close, the card is a leftover no toplevel claimed
and dismissing it takes only the card: asking the application to quit because somebody
dismissed it would take the windows it did open with it.

**A guest that goes without ever having opened a window says why — unless it exited cleanly.** The
card exists from the moment the cage is forked, before anything is known about whether the
program behind it can start, so a pack that will not mount, a box that will not compose or a binary
that is not there all present as a window that opened and then closed itself, with the sentence
explaining it nowhere the person was looking. The cage's standard error is therefore a pipe rather
than the session's own descriptor: every line still reaches `$XDG_RUNTIME_DIR/kdos-con.log`, and the
last one is kept beside the process. When the cage exits having opened no window, that line is
raised as a notification named after the program, falling back to the exit status — which at least
tells a program that is not on the machine (127) from one that ran and refused.

**A clean exit with no window is the bus handoff, and it is silent.** Every boxed application shares
one session bus address, which is exactly what makes a second launch of an editor open a second
document in the cage that is already running. That second cage maps nothing and exits 0; its
card goes without a word, and the window the person asked for arrives in the first cage as
another `KEMBED_OPEN`. A notification there would fire every time somebody opened a second file.

**Which blocks are owed is kept per block and per display.** At an 8x15 cell a block is a hundred
and twenty kilobytes and a maximised guest is dozens of them; a bounding box cannot say *these four
went and those six did not*, so a repaint that did not finish would leave stale squares nothing
repaints. Blocks that did not go stay owed, the cursor carries on where the last turn stopped so no
corner is starved, and each block is flushed to the display as it goes rather than piling up in the
queue the watermark is measured against.

**Each display's own queue is that display's pace, and no display's is anybody else's.** A turn
offers every display blocks until *its* queue reaches half of `KCON_VIEW_HIGH` and then leaves that
display out of the rest of the turn; the others carry on. A refusal is never a reason to stop the
walk — a recorder attached at a tenth of the rate, or a terminal view over `ssh`, would otherwise
hold the local screen to its rate, and the block it refused would have been cut, copied and thrown
away on every turn until it caught up. A turn ends early in exactly one case: no display has room at
all.

**The queue test sits above the copy.** A block is cut into the scratch buffer only for displays
that have room, so a display that fills up costs nothing for the rest of the turn instead of one
wasted full-block copy per remaining block.

**It stops below the refusal mark rather than at it.** `KCON_VIEW_HIGH` is where a display declines
a sprite, and it is also where a display stops counting as *ready*; a session whose displays are
none of them ready composes no frame of its own. A turn that filled a queue to the refusal mark
would therefore buy one window's pixels with the panel, the clock, the pointer and every other
window on the screen. Nothing else paces it: the queue is the whole of the answer, and a constant
on top of it would be a second and blinder one.

**A display that drains wakes the turn that refills it.** The session polls every client's socket
for writability whenever there is a backlog on it, so room a display frees is used in the turn that
notices it rather than at the next tick — a display that empties its socket buffer in well under a
millisecond would otherwise leave most of every twenty-millisecond window unused. Writability is
asked for only where there is a backlog: a socket with nothing to send is writable at once, and
polling it would turn the wait into a spin.

**One walk of the window is one guest frame.** The cage publishes into two halves of a shared
mapping and flips between them as it renders; a walk pins the half it reads from its first block to
its last, however many turns that takes, so what a display assembles is one moment rather than a
band of squares from several. Damage that arrives mid-walk is held and owed when the walk wraps,
which is also when the newer half is adopted — and holding it is what lets the owed count reach zero
under continuous damage instead of standing at the whole grid for ever.

**Two halves narrow the tear; they do not close it.** The cage flips per rendered frame with nothing
to ask permission of, so a guest that renders twice during one walk overwrites the half being read.
A walk that finishes inside one turn is coherent; a walk across several turns of a fast guest is
coherent only as far as the guest's own rate allows. Closing it needs a third half and a
parent-to-child release the cage waits on.

**A block with no pixels is not a refusal.** A block past the edge of the guest's current buffer —
which is every block on the far side of a resize the guest has not caught up with — stops being owed
rather than being read as a display falling behind; reading it as one would stop the whole window
publishing until the new buffer arrived, which is a window frozen mid-resize rather than one whose
edge arrives late. The mapping that gives those blocks pixels damages the whole window when it
arrives, so forgetting them costs nothing.

**A display that cannot show pixels is rate-limited on its own, not by holding the turn back**:
every 250 ms for that display, because a window of pixels at a compositor's frame rate down an `ssh`
link is a link that does nothing else — while the screen beside it keeps the session's own rate.

**A display's sprite table has a byte budget of its own, so pixels that crossed the wire are not
necessarily on the screen.** The session cleared what it owed when the block reached the socket: a
display that could not keep the picture reports the slots it dropped, once per painted frame as one
message, and the session owes those blocks to that display again. **The repair is bounded by a
rate — one window's worth of blocks per display per second.** A display whose table is simply too
small for the window refuses the replacement exactly as it refused the picture, so an unbounded
re-owe would be the same megabytes for ever: the hole, plus the queue the rest of the desktop needs.
A display that lost a few pictures has them all back on the next walk; one that can keep none of
them costs a window's worth of bytes a second and no more.

**The interval is what a settled window has instead of damage, and that is why it is an interval.**
A toolbox, a dialog and a browser with nothing animating draw once and then never again. An
allowance restored only when the guest next damages the window is not a bound on such a window but
an expiry date: once it is spent, the next picture the display loses is a hole nothing fills for as
long as the window is open, which is a boxed application that comes up blank or half drawn and stays
that way. Guest damage still restores it early — a window the guest is redrawing is resending those
blocks by the ordinary path, so a loss reported then costs nothing.

**A session slot that goes back to the rotation is a slot every display is told to forget.** The
rotation hands the number out again when its search comes round to it, and until it does nobody owns
the number and nothing sends a picture under it — while a display keys its sprite table on that
number alone. A display that was never told holds those pixels in its own byte budget with nothing
that will ever replace them, and the eviction that eventually takes them is reported as a loss of a
slot that by then belongs to a live window, which spends that window's repair allowance on a picture
it never lost. A window whose grid *shrinks* hands back every slot above the new grid in one go, so
every resize downward is a batch of numbers the displays have to be told about — and because those
are the highest numbers the window held, they are the ones the rotation reaches last: freeing the
top three of eight allocated slots hands the next caller slot 8, and the first of the three does not
come round again for 4088 more allocations.

**A window nobody can see is asleep, and *nobody can see* is more than minimised.** Another
workspace, hidden, behind the lock, under the saver: each is a window the draw loop paints nowhere,
and a guest kept rendering for one of them spends the display queue the window being looked at is
waiting for. The session has one answer to the question and both the draw loop and the guests read
it, because two answers drift.

**The window with the focus is served first, and the rotation is over WINDOWS.** A turn's room is
finite and the window served first takes it, so a fixed order would let an older window lock out the
one being looked at for as long as both are drawing — and a rotation over processes would let one
application with five windows spend the whole turn before the application beside it was reached even
once. With no embedded window focused the start of the turn rotates, so every window reaches the
front within as many turns as there are windows.

**And a window nobody is looking at produces no frames at all.** Each toplevel has an output of its
own, so a dock that is not animating is asleep on its own while the image window beside it draws —
which is most of the bandwidth an application with four docks would otherwise spend.

**The sprite slots are the session's and every window draws from them.** There are 4096 for the
whole desktop and a window claims one per block, so a maximised window on a 240×67 grid takes 75 and
a dialog takes a couple of dozen: about fifty maximised windows before the rotation is empty, shared
with every `libkcon` surface's own pictures. A window takes what it can get — a block with no slot
is drawn as the fallback shade and owed to no display, so a shortfall is a shaded patch in the
newest window rather than a hole onto the window underneath. Slots above a window's grid go back
when it shrinks, not when it closes. One guest may put at most sixteen windows on the desktop; a
toplevel past that is dropped with a line in the log, because an application mapping without bound
would take the pictures away from every other window.

**A resize is an output resize, and NOTHING ACROSS A SOCKET IS TOLD WHILE THE HAND IS MOVING.**
The window's cells *are* the guest's output, so dragging an edge changes its mode: the cage
reallocates its swapchain, both processes map a new buffer and the application relayouts — far
heavier than the configure a toolkit answers on any other compositor. No client can answer at the
rate a pointer moves, so one told per motion is one permanently an answer behind, and the strip
between the size it has drawn and the size the frame shows jitters along the edge for the whole
gesture.

**An embedded guest is worse, and no rate fixes it.** The block grid is cut for one rectangle and
the cut takes a session sprite slot per block; a slot taken mid-drag names a picture no display has
ever been sent, and a sprite cell whose slot a display does not hold is painted as flat backdrop. So
every re-cut punches holes along the edge being dragged and fills them in a frame later. `layout()`
also hands the slots above the old grid back, wipes the owed set and is followed by damaging the
whole window — which re-sent every block of a guest that had not repainted one pixel, put the view
over the watermark that stops the SESSION composing, and churned the slot rotation until numbers
came round to windows still showing the old picture.

**So the cut stands still for the length of the gesture.** `con_sizing_id()` names the window a
pointer drag on an edge or the keyboard `rearrange` mode is holding; while it does, the size is not
sent and the grid is not re-cut. The frame is the size the pointer says and the guest's pixels are
the size it last rendered; `embed_draw()` clamps to both, so a window that has grown shows its own
fill where the guest has not reached and one that has shrunk draws what fits.

**A terminal the session renders itself is the exception and follows the pointer exactly.** Its
cells are drawn at the window's size on every composed frame, so a reflow costs one call and there
is never a rectangle of the window with nothing in it. The line is whether the content crosses a
socket, not what kind of window it is.

**Every path that ends a gesture asserts the size once more**: the release, `rearrange`'s Return and
Escape, and the cancel a chord does when it takes the pointer away. A size that reached nothing is
the resize that "did not take".

**And a move is not a resize.** A drag that only moved the window tells nobody a size they already
have: the rectangle still goes through the one fit every placement ends in, and the reflow, the
configure and the re-cut are all skipped. The pointer drag and the keyboard `rearrange` keep the
same rule.

**A drag is measured in PIXELS and lands on cells.** A window sits on cell boundaries because it is
made of cells, but the distance the hand travelled is not a whole number of them: measured in cells
alone, a drag begun near the right-hand edge of a cell jumps a whole character on the first pixel of
movement and then runs ahead of the hand for the rest of the gesture. The sub-cell offsets the view
already sends are what make the window move exactly as far as the pointer did.

**A font step is a resize for the guest even when the window keeps its rectangle**, because that
output is measured in pixels. The cell size is read on every resize and a change in it alone
re-lays the grid and tells the guest; a window left at the old pixels-per-cell would render at the
wrong resolution for the rest of its life, with no error anywhere, because the blocks are cut from
the same stale cell they are drawn with.

Everything else about it is an ordinary window: chrome from the same code, a title bar, a close
button, `kwm_snap`, a workspace, and a taskbar entry for whatever is not a dialog, a dock or a
splash. Going off a screen tells the guest, which stops rendering — a guest drawing frames nobody
composites is a guest spending a core on nothing. **A window under an outstanding close is the one
exception and is kept awake**, because a frame is one of the three answers the ask waits for and a
guest told to stop rendering cannot give one: an application closed from the taskbar while it sat on
another workspace would be silent by this session's own instruction and reaped for it. It sleeps
again the moment it answers. The guest exiting closes every window it had, in one pass, because a
window with no process behind it has no pixels, no input and no way to be closed; closing a window
asks that toplevel to go and leaves the entry until it actually has, the same rule a terminal window
follows.

**The ask has three answers, and each of them is the guest's own event loop having run.** Closing
asks the application, which is what gives it the chance to offer a save dialog. The toplevel
retired, a question mapped on the asked window, or a frame drawn in that window more than a quarter
of a second after the ask: any of the three stops both clocks. The third is the one most
applications take — a libadwaita dialog and every Electron prompt are drawn INSIDE the toplevel that
was asked and map nothing at all — and **a frame is evidence because the cage publishes nothing of
its own**, its render being gated on the scene needing one. So "Save changes?" may stand on the
screen for as long as the person wants: nothing is signalled while the question is up. The quarter
of a second is what separates an answer from a frame the guest committed before it was asked and
that was still in the socket. **An answer is remembered for as long as the ask stands**, because a
further close starts the clocks again and a question already drawn has nothing new to commit.

**Silence is what the first rung measures, and it is measured from the LATEST ask.** A window whose
guest has answered nothing at all for ten seconds leaves the desktop, because the toplevel behind it
answers nothing and there is nobody left to hold it for. Every ask starts those ten seconds again:
an ask is a question, and the guest is owed the whole of them to answer the one it has just been
handed. The PROCESS is signalled when the ask covered the last window a person can reach and that
window has answered nothing — or when the last of them had to be taken from it, which is what stops
a guest that ignored two closes at once from running on with nothing on screen and no taskbar chip
to reach it by. Ten seconds after that it is sent `SIGTERM`, once, and eight seconds later
`SIGKILL`. That signal schedule is armed once and a further ask does not restart it, because held
keys repeat and a guest that has answered nothing must not be kept running by a close chord somebody
is leaning on.

**CLOSE IT AGAIN IS THE SECOND RUNG, AND THAT IS HOW A WINDOW IS FORCED SHUT.** A guest that has
answered is reaped on no timer at all, so nothing takes a video player, a game, an animating toolkit
or a question the application redraws and will not act on. Ten seconds after the latest ask, a
window whose guest answered puts a line on the bar — *`<name>` has not closed — close it again to
quit it* — and a close within the next twenty seconds takes the window off the desktop and puts the
guest on a signal schedule with no grace left: `SIGTERM` at once, so the cage can unwind its own
client, and `SIGKILL` eight seconds after that. Nothing the guest draws clears that schedule and
nothing catches `SIGKILL`, which is what guarantees the ladder ends.

**Which rung a close takes is whether the guest ANSWERED, never whether a clock is running.** A
second close before the offer re-sends the ask and starts its ten seconds again, and it takes
nothing: the dialog the first close raised is already on the screen, so the guest has nothing new to
commit and the second ask is met with silence — and reading that silence as a wedged event loop
would lose the very window the question was about, with the unsaved work in it. So an impatient
double-click on a healthy save prompt asks twice, and the bar offers the force ten seconds after the
second of them.

**The offer lapses thirty seconds after the ask that raised it**, which is twenty seconds after the
bar publishes it, so a close a minute later — the question read, cancelled, the work taken up again
— starts at the first rung and is offered the second one afresh ten seconds later. What does NOT
lapse is that the guest answered: that is what this end knows about the program rather than about
one ask, so a re-ask met with silence — the dialog is already on the screen and there is nothing new
to draw — is never read as a wedged event loop, and the window and the work in it stand. The process is signalled only when the forced window was the last one a person could reach,
the same rule the ask keeps: a signal takes every other window's unsaved work with it.

**The name on the frame is the guest's, and it is that WINDOW'S.** An embedded window opens under
the name its launcher was started for and takes the application's own the moment it sets one, which
arrives as `KEMBED_TITLE` — the bytes after the message, since the channel is a datagram socket and
its boundary is the string's end. The toplevel that spoke names itself in the message, so an export
dialog cannot rename the image window it opened over. The session re-announces the window to the
panel on a change, so the taskbar entry and the Alt-Tab ring follow without asking.

**And it may ask for the screen.** A guest's own fullscreen request arrives as `KEMBED_FULLSCREEN`
naming one toplevel and puts THAT window fullscreen, because the toplevel's output *is* that window and a request honoured
inside the cage alone would hand a video player back the rectangle it already had. The traffic runs
both ways: `Super+f` and the frame's own button make a window fullscreen without the guest having
asked, and `KEMBED_FULLSCREEN_SET` tells it so — a guest never told draws the chrome of a windowed
application across a screen that has none.

**And it may ask for the screen to stay on.** A guest playing something sends `KEMBED_INHIBIT`, and
while it holds, the session's three idle steps are all suppressed — the same effect the `stay-awake`
toggle has, from a different source. An inhibitor is honoured only while its window is one somebody
can see: minimised, on another workspace or under the lock it counts for nothing, because a machine
kept awake by bookkeeping is one whose battery goes while it sits closed. It is cleared with the
window as well as by the guest, so a guest that exits without clearing it holds nothing.

### What a guest is typed at and pointed at with

**There are two input streams and a guest is fed by exactly one of them.** Everything drawn in cells
reads the cooked one — a resolved character and a cell — and that is the whole vocabulary a view
inside somebody else's terminal has, because a character and a cell is all its terminal gave it. A
view with a real keyboard and a real pointing device sends the raw stream **beside** it: the evdev
code with its press and its release as two separate events, the full xkb modifier mask, the position
in the view's own pixels with the deltas the device reported, the full evdev button code, a scroll
with a real value and a second axis, and the compiled keymap the person is actually typing on. The
session asks for that stream with `KCON_OP_VIEW_RAW` and asks **only while an embedded window has
the focus**, because it is one message per device event and everything else here is cells.

**The cooked message for one physical input is routed first, and that ordering is the protocol.**
By the time the raw message is delivered the session has already decided whether a chord ate the
key, which window the pointer is over and where its own cursor is — so the raw arm routes nothing,
chords nothing and moves no cursor. It delivers. There is one answer to where a click landed and it
is not made on the raw path.

**A key is a switch.** `KEMBED_KEY` carries an evdev code and a press or a release, so a guest holds
W until the release arrives and repeats from its own keymap. A press whose release was delivered
somewhere else is a key held for the life of the application, so **the session releases every key a
window is holding before the keyboard leaves it** — the cage's own sweep on `KEMBED_FOCUS a=0` then
finds nothing left and no key is released twice.

**A release travels on the raw stream and on nothing else, so losing the stream releases
everything.** The chord that moved the focus off the guest, the lock, the saver, a view that went
away and the guest's own exit all happen while the person is still holding the key that caused them,
and the release they make a moment later is sent by nobody. The gate that asks a view for raw input
is the one place either edge exists, so it is where every guest is released and where the session
forgets which presses a chord ate: a suppressed press remembered past the stream that would have
matched its release swallows that key's next press for the rest of the session.

**A chord that the session kept takes the release with it.** A press the guest never saw must not be
followed by a release it did. A modifier is never kept, because a modifier produces no character and
so has no cooked message any chord could consume — the raw arm knows one by its evdev code rather
than by what came before it. A view delivers the cooked message for one physical input immediately
before that input's raw partner, and every raw event carries the count of cooked events that must be
taken first, so a verdict belongs to the one cooked key it was reached on and to no other. `Ctrl+A` reaches the session as the leader *and* `Ctrl` itself reaches the guest as a
held key. What a guest never receives is
[the chord table](#the-chords-and-the-leader-that-reaches-them) and nothing else.

**The keymap is the person's.** A view that reports a real keyboard sends the compiled xkb text of
the layout it is running as `KCON_OP_KEYMAP` — bytes, never a descriptor, which is what keeps the
view socket forwardable. The session holds one (the last view to speak wins, because one session is
one keyboard), seals it into a memfd once and hands it to every guest with `KEMBED_KEYMAP`.
`KEMBED_MODS` carries the locks and the layout group, which no key stream can establish, and xkb's
own rule is that a state driven by keys must not also be set by mask — so it goes at focus-in, on a
keymap change, and behind a key that left the locks or the group somewhere other than where that
window was last told they were. It goes **behind** such a key and never ahead of one, or the guest
applies the key on top of the mask and toggles the lock straight back off.

**The session knows the mask only while the raw stream runs.** It arrives on that stream and nowhere
else, so Caps Lock pressed into a terminal while no guest held the keyboard reaches the session as
nothing at all. The session sends no mask it does not hold — `KEMBED_MODS` is acted on, and a
session asserting the locks are clear when it does not know puts every letter in the wrong case — so
a guest given the keyboard under a lock nobody told it about is told by the first key it receives,
and that key is the only one it resolves without the lock.

**The pointer is in pixels, and the delta travels with it.** `KEMBED_MOTION` carries the position
inside the window, converted from the view's pixels and the cell size those pixels were measured in
— exact at both window edges and across a font step the session has not seen yet. `KEMBED_REL`
carries how far the device actually moved, accelerated and unaccelerated, which is the only thing a
guest that grabbed the pointer can read. `KEMBED_BUTTON` carries the evdev button, so a mouse with
side buttons drives Back and Forward. `KEMBED_AXIS` carries a continuous value, a `value120` and
**which axis**, so a guest scrolls sideways as well as up, and reads a detent at the resolution the
device reported it at rather than as one whole step.

**The guest is the content and not the frame.** `win_at()` finds a window by its frame rect, which
is the content inflated by `CON_FRAME_X` and `CON_FRAME_Y`, so a cooked event on the band of border
round an embedded window arrives naming that window. It is not delivered: the position would be clamped back
into the content, and the cage would go on drawing its arrow against the inside edge a cell from the
session's own pointer while the hand is on the border. The border is where a window is grabbed,
moved and resized, and the session's own pointer is the only one that may be on it. The guest is told it
**left** — its protocol has a leave and a motion to somewhere outside would be clamped into an
arrival — and `ptr_route_id` is cleared with it, so the raw arm stops aiming the guest in pixels
from a border the session is about to be asked to drag. **A held pointer is clamped, not dropped**:
the press is what put the button here and the release is the only thing that ends what the guest
started.

**And inside the content the guest's cursor is the only one.** `kdos-cage` composites its own cursor
into the frames this session pastes in, so the session marks a guest's content cells `KT_A_GUEST`
and the view draws no pointer of its own on them — see the pointer contract in
[design-language](../03-architecture/design-language.md). Two pointers a cell apart is what the two
halves would otherwise look like, and the one a person aims a two-pixel scrollbar with is the
guest's. The mark goes on content cells alone: a block with no picture yet is a shade mark without
it, the chrome round the window is cells and carries none, and an icon or an image in a terminal is
nobody's guest and keeps the pointer.

**A guest may take the pointer.** A game or a three-dimensional editor asks the cage for a pointer
constraint, which arrives as `KEMBED_GRAB`. While one is held the session routes every pointer event
to that window and to nothing else: nothing is hovered, nothing is raised, no frame button answers,
and the delta goes out with no position behind it. **Moving the keyboard focus is the way out** —
the constraint is active only for the surface that has the keyboard, so every chord that moves focus
drops it. There is deliberately no chord and no break message of its own, because a pointer that can
be captured with no way out is the failure this avoids. **The session's own pointer stops where the
guest is** for as long as the constraint is held: the hovered window is that window and the implicit
grab a button press armed is dropped as the constraint takes over, because a guest normally asks for
the pointer *on* a press and the release that would have ended the grab is spent on the constraint
instead. The desktop's own pointer is drawn by the view from its own cooked stream, so it goes on
following the device across a screen where nothing else answers it — and is held wherever that
lands on a block the guest is still rewriting, which under a constraint is wherever the guest has
put its cursor.

**A finger reaches a guest a cell at a time.** A pointing device reports every motion twice — the
cell the desktop reads and the pixel a guest is aimed at — so a guest is given the pixel and the
cooked event is dropped for it. Nothing reports a finger twice: the recogniser synthesises the
press, the drag and the release from the touch stream and no device event stands behind them. The
session tells them apart by where they arrive — the recogniser pushes what it synthesised
immediately behind the touch it read, and the button it opened stands for the drag in between — and
delivers them to a guest as the cooked events they are, at the grid's resolution, aimed at the centre
of the cell — a synthesised pointer carries no sub-cell offset, and zero is the centre. The gesture
itself reaches only a window drawn in cells.

**A view with no device is driven the one way it can be**, and that is every view inside somebody
else's terminal, which is what a session reached over `ssh` is driven by. Such a view never claims
`KCON_VIEW_RAW`, is never asked for raw input and never sends a keymap; its input reaches a guest as
the character mapped back to the key that produces it on a **US keymap**, which is the keymap such a
guest is started with, pressed and released in the same breath, aimed at the middle of a cell with
the 1/256th offset the view supplied. Such a view loses held keys, the person's own layout inside a
guest, sub-cell aiming, a modifier on a click, a horizontal axis and pointer lock; it keeps every key
and click it could send, the drawn pointer, the picture, the clipboard and the drop.

### The guest on a terminal of its own

`--vt` is for an application the card cannot be given to through a window: one that sets its own
full-screen mode, one whose driver will not run against a headless output, and one that needs the
frame rate the window path cannot reach. An embedded guest is composited **on the card wherever one
works**, and working is measured rather than inferred: the cage leaves the renderer to
`wlr_renderer_autocreate`, which takes gles2 or vulkan where a DRM render node opens and pixman
where none does, and **then renders one frame through that answer** — the renderer, the allocator
and a throwaway output — before anything is built on it. The node opening is not the question that
decides it. On virtio-gpu a render node opens and gles2 is chosen; on a software virgl the driver
then refuses to import the udmabuf buffers an embedded cage has to allocate to be able to read its
own frames back, answering `eglCreateImageKHR` with `EGL_BAD_ALLOC`, and on a virgl backed by a real
host card it accepts the import, satisfies every call and puts the pixels in memory of its own.
Every layer below reports success either way: the refusal surfaces as a failed output commit, which
is a guest waiting for a `wl_output` that never appears, and the acceptance surfaces as nothing at
all — a full frame of blocks a second, every one of them the zeroed page the buffer was allocated
as. So the cage paints a second frame of known pixels and reads it back the way it publishes one,
and drops the card for pixman where the pixels do not come back. A machine with no render node, no
driver for it, no `/dev/udmabuf`, no importable buffer or no readable frame gets the software
renderer and a working window rather than a blank one — at the cost of llvmpipe inside the box. The
session forwards the box profile's `render` key to the cage as `KDOS_EMBED_GPU`,
**the value unread and in both directions**: `software` pins the cage on pixman as well, so a box
that refuses the card is neither drawn nor composited on it, and every other spelling leaves the
cage's own choice alone. The same key reaches the box's own Mesa as `LIBGL_ALWAYS_SOFTWARE` in the
launch environment, so one line answers for both halves. `render` is its own key and not the profile's `gpu`, which says the
render nodes are bound into the box. `--vt` is selected per application by `display = vt` in the
same profile, and `embed = false` in `con.conf` turns embedding off for everything.

**Either renderer, an embedded frame crosses the CPU and a guest on its own terminal does not.**
The window path publishes into shared memory this session reads, cuts it into blocks and sends them
to every display, so the cost is the guest's whole picture copied several times per frame — which is
what puts a ceiling on it well under a card's own rate, and which the hardware renderer moves rather
than removes, because the card's output is read back before anything is sent. On its own terminal
the cage holds the card directly: the guest's buffer reaches a scanout plane with no readback, no
blocks and no socket, at whatever rate the screen runs. What that costs is exclusivity — a terminal
switch is the whole screen, so the desktop, the panel and every other window are gone for the
duration, and a view over `ssh`, a cast and a recording see nothing of it.

**The profile is the box's, and the launcher names the box.** A generated launcher for an
application that belongs to a pack runs `kdos-appbox -b <pack> run <exec>`, and `<pack>` is the
string `~/.config/kdos/boxes/<pack>.conf` is filed under — so the session reads the same file
`kdos-box profile` writes and the settings surface edits, without repeating appbox's
command-to-pack matching. That matching is not a basename: it skips `env` and `VAR=value` prefixes
and matches the whole command, so a second copy of it here would answer differently for exactly the
applications that need it. `--box` is the same option, and `kdos box export` writes the other
generated shape — `kdos-box run <box> <app>` — which the session reads the same way. An entry with
none of them names no box and therefore no profile: it is embedded on whichever renderer the machine
affords, which is what an absent `render` key means anyway.

**`display` is the session's key, not the container's, and `render` is read on both sides.** The
session reads both straight out of the profile file; `kdos-box profile` carries `display` through a
rewrite without interpreting it — a profile writer that knows only its own keys deletes everybody
else's — and resolves `render` against the machine, which is also what puts
`LIBGL_ALWAYS_SOFTWARE` into a software box's launch environment.

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
frame: as fast as the display's own queue drains when something can show pixels, and once every
250 ms when nothing can, because a window of pixels at a compositor's frame rate down an `ssh` link
is a link that does nothing else.

**A view that says it answers frames is what paces the session.** It reports `KCON_VIEW_FRAME` in
its hello, the session closes every frame it sends with `KCON_OP_FRAME`, and the view answers with
one of its own once it has painted. The session composes the next frame when a display says it is
ready rather than when a timer expires — so an animation runs at the refresh rate of the screen
showing it, and a display that is still painting is never sent a frame it would throw away. The
compose floor stays as a cap — the fastest attached screen's period, 16 ms where none reports one;
the answer is the gate under it. A view built without the capability, or one that stops answering,
is paced by that clock alone after 100 ms.

**Every attached display paces itself, not just the quickest.** A view that still owes the frame it
was last sent is sent none, exactly as a view over the backlog mark is. Without that rule a session
with two displays composes as soon as the faster one answers and pushes another frame at the slower
one on each of those composites, resetting the answer it was waiting for and growing its queue
until whole frames are lost — so the contract would pace a single-display session and nothing
else.

**And a surface holds its next commit until the frame carrying its last one is composed.** The
session sends `KCON_OP_FRAME` to every surface whose cells or pictures went into the frame it just
drew — a picture counts, because an animation changes pixels and no cells and a leg answered only
for cells would run at the 100 ms stall timeout instead. A program drawing faster than the desktop
can show — a terminal running a full-screen animation — then puts one commit on the wire per
composed frame, carrying the newest cells rather than every intermediate one, which is the contract
a Wayland frame callback gives a client.

**A view's `cols`/`rows` say how big the frame it holds IS; `view_cols`/`view_rows` are what it
asked for.** The size message a display sends changes the second pair only: nothing on that path
allocates a frame, so writing the request into the first pair would tell the sender that a buffer of
the new size already existed, and the first frame at a bigger grid would be copied into the smaller
allocation — a heap overflow of exactly the difference, which ends the session on the first window
that grows. The allocation is also what emits the configure naming the new grid, so a frame at a
size the display has not been given a buffer for cannot happen quietly.

**A frame is composed for a display, not for a program's output.** The loop turns as fast as the
things it reads produce, and a full-screen animation in a terminal produces without pause — measured
at **1380 turns a second** against a screen that shows sixty. Composing and serialising on every one
of them spent two thirds of the session's core on frames nothing would ever see, and took that core
away from reading the program's output, so the animation ran slower the harder the session worked at
showing it. `frame_floor_ms()` paces the picture to the attached screens — the period of the
fastest mode any attached display is wearing, taken from the millihertz it reports and **rounded up
to the millisecond the loop measures in**, with `CON_FRAME_MS` as the floor where no display
reports a refresh; input is still read every turn, so the pacing costs latency of at most one frame
and nothing else.

**Rounded up, so the floor is never shorter than the screen's period.** 144 Hz is 6.944 ms:
rounding down asks 166.7 composes a second from a screen that shows 144 — measured at 166.2 against
a 144 Hz view, one compose in seven serialised for a frame nothing can display, which is the waste
this floor exists to prevent. Rounding up asks 142.9 and measures 142.3. The cost of rounding up
grows as the rate does, because a millisecond is a larger share of a shorter period: at
`CON_RATE_MAX_MHZ` it is 5 ms for 4.167, 198.9 composes measured against 240 shown.

**The rate is the maximum across every attached display, not whichever answered last.** A display
answers `KCON_OP_VIEW_OUTPUTS` with its own monitors, and that list is one display's — it is what
the Display picker lists and what the seam snap measures, so it is REPLACED by each answer and
never merged. The pacing number is a different question and is kept in a different field: it is
dropped to zero whenever the screens are asked for and RAISED by every answer to that round, so
reply order decides nothing. Without that separation, a 144 Hz view and a 59.94 Hz view attached
together and started the same way three times measured 144.2, 144.1 and 76.2 frames a second at the
144 Hz one — the rate is whichever display answered last, and nothing decides that. Kept apart, the
same three runs measure 142.1, 141.7 and 141.9, with the 59.94 Hz view taking 60.1 throughout: a
slower screen beside a faster one takes its own rate, because a display that has not painted the
last frame is not asked for another.

**The screens are asked for at every attach, at every detach, at every mode this session sets, and
where a grid resize says a monitor moved; the Display picker's request is a fifth caller.** Nothing
else publishes a mode, so a session that waited to be asked by the picker would pace itself at
`CON_FRAME_MS` for its whole life — 62.5 frames a second against a 1920x1080@74.998 screen that
takes 71.4 and against a 144 Hz panel that takes 142.9. The resize keys on the **cell grid changing
size** and on nothing else, so it catches a monitor whose arrival, departure or new resolution
re-cut the grid and misses one that stepped between two modes of the same size in cells: the
session asks where it sets a mode itself, and a hotplug that leaves the grid alone is paced at the
old screen's period until something else asks. Asking on the compose path instead would put a round
trip per view on every frame.

**A refresh a view reports is bounded at both ends before it paces anything.** The view socket is
the one that may be forwarded, so the far end of it is not this machine and may not be this person,
and the number is a claim rather than a measurement: unbounded, a peer answering 4 000 Hz has the
session composing 957.6 frames a second, spending on them the core the terminals are read with.
`CON_RATE_MAX_MHZ` caps the rate at 240 Hz — 197.0 frames a second measured against that same peer
— and `CON_FRAME_MS` caps the period at 16 ms, because a mode below 60 Hz would widen the floor and
compose less often than a display that is ready.

**A display that is behind is sent nothing, and is never dropped for it.** A view is the one peer
whose messages are a stream of pictures — the newest frame makes every older one pointless — so
above `KCON_VIEW_HIGH` (1 MiB queued) the session stops sending it cells and sprites until it
drains. A skipped frame also leaves the view's own copy of the previous frame alone, and because the
diff is taken against that copy, everything a skipped frame would have carried goes out with the
next one the display can take. Without it the ordinary load of a desktop kills its own screen: one
maximised guest repainting its window is over twenty megabytes of blocks, a full-screen animation in
a terminal is most of a megabyte a frame, and either reaches `KCON_MAX_QUEUE`, which means *the peer
stopped reading* and drops the connection — the only display and the only source of input the
session has.

**`KCON_MAX_QUEUE` therefore means only that a peer has stopped reading altogether.** The
watermark is far enough below it that one more whole frame on top cannot reach the cap.

**A frame goes whole or the view's copy is disowned.** A frame is cut into messages of a quarter of
a megabyte — the grid may be far larger than the megabyte a single message is capped at — and the
previous-frame copy the diff is taken against is updated run by run as the runs are encoded. If any
of those messages fails to encode or fails to send, the copy is dropped and the next frame is sent
whole: a copy claiming cells the display never received would make the next diff skip exactly the
cells that are wrong, and the failed frame would stay on screen until something else happened to
overwrite it.

**A surface that is behind skips its own frame, by the same rule read from the other end.** A
terminal window running a full-screen animation produces several megabytes of output a second, and
its cells are a stream of pictures exactly as a display's are. The client leaves its previous-frame
copy alone when it skips, so the next diff carries everything the skipped frame would have; a queue
allowed to grow instead reaches the cap, the connection is marked dead, and the window is gone with
no signal, no exit status worth reading and no line in any log — which is what a terminal that
"just closed" during an animation is.

**A view that the session dropped exits non-zero.** A supervisor reads a clean exit as *a person
asked for the screen back* and stops supervising, so a display that ended because its connection
died and said so with a zero would never come back: the screen would keep the last frame it flipped
for the rest of the login, with nothing able to type at it. Only `KCON_OP_BYE` — and, for a `--tty`
view, its host terminal hanging up — ends the process successfully.

**A third socket is the reader's**, beside the surface socket and the view socket in the same
private directory. Its clients are displays that **cannot drive** — the socket decides that, not the
client — and they are the only ones sent `KCON_OP_ANNOUNCE`: the role, name, value and position of
whatever the focused widget just said, plus the focused window when it changes. A reader that
attaches late is told the window again, for the same reason a view that attaches is sent the whole
frame. **A reader is sent no cells at all** — it is a view kind so that it is counted, gated and
detached like one, but it draws nothing, so handing it the frame's runs would push a full-screen
animation's whole diff down a second socket to be discarded, and it does not count towards whether
a display is ready for the next frame. `speak = yes` starts `kdos-a11y` with the session; see
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
their modes end to end into one virtual box — each screen with its own row diff, because two
screens comparing against one previous frame would each find the other's paint already done and
neither would redraw.

**A screen is up to three scanout buffers and one more the painter owns.** The cells are composited
into the painter's own buffer in ordinary memory, the rows that changed are copied into a scanout
buffer that is neither being shown nor waiting on a flip, and the CRTC is pointed at that one at
the next vblank. Two things follow: no pixel is rewritten while the raster is inside it, so an
animation does not tear; and no glyph is composited into a write-combined mapping, where every
`OVER` reads the destination back at a few bytes a cycle.

**The third buffer is what stops the painter waiting for the vblank.** With two, the only buffer it
may touch is the one the flip is waiting on, so a compose that overruns a refresh period costs a
whole further period — 60 frames a second becomes 30 at the first overrun. With three the next
frame is composed while the flip is in flight and the completion presents it, and exactly one frame
waits, which is what keeps the presentation order the paint order. `con.conf`'s `buffers` is the
ceiling, 1 to 3, and a driver with no memory for the third gets two — 8 MB a screen at 1080p is a
real cost on a machine driving several.

**Which mode a screen wears, and when a frame reaches it, are both `con.conf`'s.** `refresh =
fastest` takes the highest refresh at the size the monitor asked for — 144 rather than 60 on a
panel that publishes both at its native size — and never a different resolution; `preferred` is the
default and takes the monitor's own EDID choice, because a higher refresh is a different link rate
and a screen is the one thing a person cannot work around from somewhere else. `tearing = yes`
points the CRTC at the finished frame immediately instead of at the vblank, which removes up to a
refresh period of latency and cuts a moving edge across the screen; it is silently off on a device
that does not publish `DRM_CAP_ASYNC_PAGE_FLIP`.

**A transfer-model driver gets one buffer, and gets it on purpose.** `virtio_gpu`, `qxl` and
`vmwgfx` keep the displayed image on the host and read the guest's buffer only at the copy
`drmModeDirtyFB` asks for, so painting in place cannot tear and a legacy page flip — which carries
no damage rectangle — would upload the whole plane, 8 MB a frame at 1080p to deliver the few
kilobytes a clock tick changed. Every other driver keeps the extra buffers, virtual ones that scan
guest memory continuously included. A driver that refuses a flip outright falls back to the single
buffer, with the frame copied across before the CRTC is pointed at it; a refusal that is about the
moment rather than the driver — a CRTC detached by the screen blank, a VT switch still settling —
costs one repainted frame and changes nothing. `--card PATH` names a device for a machine with more
than one; without it the first card with a connected output wins.

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
sends no input: a recording is not a seat. Damage drives the frames and a **keep-alive floors
them**: a still desktop rasterises nothing, so a cycle with nothing new re-sends the last frame
once every 500 ms and carries an empty chunk in between. That floor is not a nicety —
`pipewiresrc` drops a chunk of size zero, so a stream that only ever said "no new data" would hand
the pipeline downstream nothing at all and the recording would end with no header in it. A
consumer that has just connected is fed a frame for the same reason, immediately rather than at
the floor.

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

**Routing dispatches on the button, never on the press kind.** A wheel detent is delivered as a
press carrying `KT_MB_WHEEL_UP` or `KT_MB_WHEEL_DOWN` and no release ever follows it, so a test on
`press` alone answers a scroll: over `↓ ■ X` a tick would close the window, over `Start` it would
open the menu, over a title row it would arm a move, and under a mark or the colour picker it would
re-anchor them. Everything that means a detent tests for one **by name**, which is the session
bar's workspace step and nothing else. A scroll reaches the window under the pointer and does
nothing else to it — it does not raise it, does not take the keyboard and does not arm a drag.

| Where | Press | Does |
|---|---|---|
| `Start` | left | opens `con.conf`'s `menu` |
| A taskbar row | left | raises it, or **restores it** when it is minimised — the row is the way back |
| A taskbar row | middle | **minimises it**, or restores it when it is already away. The row is where a minimised window comes back from, so it is where one goes: putting a window away was a chord and the frame's own chip, and neither is reachable while the window is behind whatever is being read |
| A pager cell | left | switches to that workspace |
| Anywhere on the session's own bar | wheel | steps workspace — up is `workspace-prev`, down is `workspace-next`, both landing on the next **occupied** one. `con.conf`'s `panel_wheel`. Asked before the bar's hit map, so a tick over `Start` or over a window row means this too: neither has a use for a detent, and a wheel that did one thing on part of a bar and nothing on the rest reads as broken |
| The clock | left | opens `kdos-cal` |
| `↓` `■` `X` on a frame | left press, then release on the same chip | minimise, maximise / restore, close — each is a **three-cell chip**, the mark centred with the chip's own fill either side, and all three cells answer. The press only **arms** it and the release decides, so moving off the chip before letting go takes the press back |
| A title row, clear of its ends | left drag | moves the window — the **frame's** row, the one the box and the buttons are drawn on. A **tab strip** is drawn on that row and answers no press: the tabs are on chords, and the drag is what the row has always meant |
| A title row, chips included | left, twice inside `dblclick_ms` | `con.conf`'s `title_dblclick` — **maximise** and back by default, or `lower`, or nothing. The pair is spent when it fires, so a hand resting on the button does not flicker the window between states, and no third grab is armed: one holding a window that has just been resized under it would measure every later motion against a rectangle that is gone |
| A title row or a border | left drag **ending against an edge of the work area** | snaps the window there when the button comes up — left and right give that half, the **top maximises**, a corner gives the quarter. `con.conf`'s `edge_snap` and `snap_zone`. A click that never travelled does not snap, or a window whose title row already sits at the top of the work area would maximise itself the first time anybody clicked it |
| A title row, chips included | right | opens the **window menu** at the pointer, raising the window first |
| A title row, chips included | middle | **lowers** the window |
| Any part of a frame | long press | opens the window menu there — a finger's way to the verbs, and the only one it has |
| Any border cell that is not the title row — the band is two columns at the sides, one row top and bottom | left **or** right drag | resizes it from that side |
| The last `CON_GRAB_CORNER` cells of the side and bottom borders | left **or** right drag | resizes it from that **corner**, both axes at once |
| The last `CON_GRAB_CORNER` cells of the title row | left drag | resizes it from that top corner, both axes at once |
| Anywhere in a window | Super + left drag | moves it, so a window that is all content is still movable |
| Anywhere in a window | Super + wheel | steps that window's own **transparency**, by the same ten points `Super+Ctrl+=` and `Super+Ctrl+-` do. Taken before the window is told anything, because `Super` is the session's modifier and a detent carrying it is never the guest's — a terminal would scroll its history and a boxed application would zoom, under a chord meant for the frame. A background layer is never mixed: there is nothing behind it |
| Anywhere in a window | Super + middle or right drag | resizes it from the **nearest** edge or corner |
| Inside a window whose content the session draws | right drag | resizes it from the **nearest** edge or corner |
| A desktop icon | left | selects it; a second press opens it |
| A desktop icon | left drag | carries it — onto the trash, or into a window that takes a drop |

**One function answers all of that, and the router answers none of it.** `win_grab_at()` is asked
what a press on a window arms and which edges a resize is to move; the router owns the grab and the
window model owns the geometry. A second reading of where a border is, of which windows have a
frame at all, or of which button means what on each would be a second thing to get wrong on the
frames nobody tests.

**The title row is the left button's alone, and that is what keeps the grip honest.** Left on the
row moves the window and left on its corner arms resizes from that corner; the right button opens
the window menu and the middle button lowers the window, and neither of them arms a drag anywhere on
the row. The grip below is lit by asking `win_grab_at()` *with the left button standing in for a
press that has not happened*, so a right press that resized from the top edge would be a resize the
window never said was there — one answer between the light and the press is the rule the frame
keeps. `Super` is still the way in from anywhere and is asked first: `Super`+right resizes from the
nearest edge wherever the pointer is, the title row included.

**The window menu is every verb that acts on one window, in one list, each row printing the chord
that does it — and that is the whole of why it exists.** A chord with no row here is something this
desktop can do and a pointer cannot ask for, and the window in question is very often a BOXED
APPLICATION, whose own menus live inside its pixels and know nothing about snapping, tabs or
transparency. The rows are Restore, Move or size, Minimise, Maximise, Fullscreen, Lower, then Snap,
Tabs and Transparency — which open panes of their own — then Scratchpad, Send to workspace and
Close. Snap's pane carries the four half-screen tiles and, under a rule, the four SWAPS: a snap
tiles this window against an edge and a swap exchanges it with the neighbour that way, and both are
"what about the window in that direction", so one is found beside the other rather than in a third
level of menu.

It is the FRAME's menu and is per window; the taskbar row's menu in
[`kdos-shell`](kdos-shell.md) is per application and carries the verbs for a whole group. It opens
on a right press on a title row, on a right press on the session bar's **window row** — which is the
only thing a MINIMISED window has on the screen, so without it that window's verbs are reachable by
the keyboard and by nothing else — on `Alt+Space` over the focused window, and on a long press
anywhere on a frame, which is the only way a finger reaches those verbs at all and is answered above
the surface test because a terminal and an embedded guest have no surface to deliver a touch to. **The chords come
out of the bind table**, never written at the menu, so a `keys.conf` that moves one moves what the
menu teaches; a row that does not apply to this window is **greyed rather than hidden**, because a
menu whose rows moved with the window's state would put `Close` where `Fullscreen` was between one
press and the next. Four rows open a **pane of their own** at the same cell, because each needs an argument a row cannot
ask for: Send to workspace a number, Snap an edge or a neighbour to swap with, Tabs one of four
steps, and Transparency a rung of the ladder — 100 down to 20 per cent, with the rung the window is already on greyed, which is how
every other menu here says "you are here" without inventing a second kind of mark. It stops at 20
for `win_opacity_step`'s reason: there is no pointer gesture that brings back a window nobody can
find. It is drawn with `KtuiMenu` — the same
widget every surface pops with `Shift+F10` — as a popup with no bar, because a bar across the top of
the desktop would be a menu belonging to no window.

**The menu owns the keyboard and the pointer while it is up**, ahead of the chord table, the panel
row and every frame, for the window list's reason: its arrows move the caret and a chord firing
underneath would snap a window while somebody was choosing a verb for it. A row runs on the **press**
and the menu answers that press's **release** as well, or a button-up would land on whatever the pane
was covering as the end of a press that window never heard. Every button but the left one puts the
menu away rather than picking — a wheel detent is a press with no release, and a widget that picked
on the press kind would run whichever row a scroll passed over. A window that closes under its own
menu takes the menu with it.

**And the window says what a drag would take before the button goes down.** The pointer does not
change shape — the view draws it as an arrow where it has pixels and as the cell under it reversed
everywhere else, and a shape published per motion would be a commit per motion — so the *window*
carries the state instead. `win_grab_at()` is
asked the same question with the left button standing in for the press that has not happened, and
what it answers is lit on the frame:

| What a press would arm | What lights | Unicode / ASCII tier |
|---|---|---|
| A move | the frame's four corners, as studs — the whole window travels | `■` / `#` |
| A resize taking the left or right edge | that border's **outermost column**, along its length — the band behind it grabs just the same | `◀` `▶` / `<` `>` |
| A resize taking the bottom edge | the bottom row, between the corner arms | `↓` / `v` |
| A resize taking the top edge alone | the two top corners, so the title keeps its row — and only the left button takes it | `↑` / `^` |
| A resize taking a corner | the two arms that take both axes, and the cell they share | `↓`/`◀`/`▶` on the arms, `■` / `#` on the shared cell |
| Nothing | nothing | |

**What lights is what a press arms, cell for cell.** `win_grab_arms()` hands the grip the same arm
lengths `win_grab_at()` measures a press against, so a single-axis run stops where the corner arms
begin rather than promising a one-axis resize over cells that take two. The shared corner cell
carries `■` and not an arrow, because at that cell a press takes both axes and an arrow there names
one; the console font carries no diagonal at all, so the corner cannot be marked with the direction
it takes. A move lights four studs and no runs, a corner resize lights one stud and two arms, and
the arms are what tell them apart. The glyphs come from `ktui_glyph`, so both
tiers are covered by the table that already chooses them, and the ink is `KT_ACCENT` read against
the frame's own background, or `KT_SURFACE` on a window rung for attention, which is filled in
`KT_ACCENT` and would otherwise swallow the grip whole. The modifiers travel with a motion, so
holding Super lights the move studs from anywhere inside a window, which is where that grab is.

**A drag in progress outranks the pointer.** The window follows the hand, so the pointer is off the
border from the first cell of the drag; while a grab is held it is the *grab* that is lit, or the
affordance would go out at the instant it began to mean something. Under a lock, a saver, a mark, a
pick, a window menu or a guest that has taken the pointer nothing is lit, because no press reaches a
frame in those modes and a lit edge would promise a drag that cannot start. It is recomputed for every cooked
pointer event and repaints the screen only when it *changes*, which on a still desktop is the only
thing that would.

**The grip is drawn on its own frame and never on what is in front of it.** It is painted after the
whole back-to-front window walk, where nothing about the stacking is left in the composed frame, and
the window it belongs to is the one under the *pointer* — topmost there and nowhere else along a
border that may run under the window beside it, under a menu or under a toast. Every cell is
therefore tested against the windows drawn after its own, by the **draw order and not the hit
order**: a tooltip, a toast and the candidate list declare an empty input region and the hit test
walks past them, and all three are drawn and do cover cells. The window switcher is laid over the
walk as well, so nothing is lit while it is up.

**The icon layer is asked LAST and never first.** It covers the work area, so hit-testing it before
the windows would take every click on the desktop; a press no window claimed is the one that belongs
to it. That order is what a drop keeps too, and the background is neither raised nor focused by a
press — it is under everything by definition.

**A surface that declares an empty input region is asked at all.** A tooltip, a toast, the
candidate window and the saver are drawn over the desktop and take nothing: the hit test walks past
them to whatever is underneath. Without that the topmost rectangle wins every time, and the
tooltip describing the `Start` button took the click aimed at it.

**A grab owns the pointer before anything else is asked, and ends on anything that is not a drag.**
Both halves are load-bearing. Hit-testing the bar's own row first eats the button-up of a window
dragged downwards and released over it, and a grab cleared only by a release that reaches the
router is then a grab nothing can end — every later event falls into it, a press is neither a drag
nor a release, so it does nothing and returns, and the pointer is dead for the rest of the session.
One lost button-up must cost a drag, never the pointer.

**A press on the desktop gives it the keyboard, where the desktop asked for one.** The icon layer
is a `BACKGROUND` and is never *raised* — it is under everything by definition — but it implements
arrows, `Enter`, `Delete`-to-trash and an inline name editor, and a session that focused it never
left all of them unreachable while the surface went on advertising them in its own hint row, and
left anything the pointer opened there a state nothing could type into or leave. The next press on a
real window takes the keyboard back.

**A press latches the pointer to the window it landed on until the button comes back up.** Events
are otherwise delivered by position, so a drag leaving the window takes the release with it and the
window that heard the press never hears the end of it: an embedded application holds the button for
the rest of its life and a terminal keeps extending a selection nothing will finish. While the
latch holds, a position outside the window is **clamped to its nearest cell** rather than dropped —
a negative position is this protocol's leave, and a surface told the pointer left never acts on the
release meant for it. A window that closes under a held button releases the latch with it, and so
does a guest that takes the pointer under one — the release that would have ended the latch is spent
on the constraint instead, and a latch nothing can end is a pointer that answers one window and no
other. The frame's own move and resize drag is a separate grab and is answered first.

**The border is `CON_FRAME_X` columns and `CON_FRAME_Y` rows thick, and the two are different
numbers because a cell is.** A cell is twice as tall as it is wide — 8x16 in the console font, and
near enough the same proportion in every monospace face the desktop view loads — so the same *cell*
count on all four sides is a border twice as thick in *pixels* at the top and bottom as at the
sides: heavier to look at than the sides it dwarfs, and no
easier to aim at than they are. **Two columns and one row** is the square border, sixteen pixels of
edge to take hold of on every side of the window. Rows are also the scarce axis: the shipped grid is
240x67 — 1920x1080 over an 8x16 cell — so a row is three and a half times the share of its own axis
that a column is of its. A second row top and bottom takes 3% of every window's height where a
second column each side takes 0.8% of its width, and what the frame is spent on is the axis that is
cheap. **Which side pays depends on what fixes the window's outer rectangle.** A window placed freely — by
the search, by a drag, or at a rectangle a layout or a saved session names — spends the border
outside its content: it takes those columns and rows of the desk, and its program is told the size
it asked for. A window whose outer rectangle is fixed pays out of its own content instead:
maximised, snapped, tiled and the scratchpad's drop-down all take the work area, or a division of
it, as the FRAME, so the program gets `2 * CON_FRAME_X` columns and `2 * CON_FRAME_Y` rows less than
that. A window too big for the grid is the same case, because there the frame is the thing that has
to stay on it. Raising either number takes cells from every one of those at once.

**Every cell of the band answers, not its outermost one.** A press anywhere in those two columns or
that one row is a resize from that side, so the thickness a hand can see is the thickness it can
grab; a band that only acted on its outer cell would be a wider picture of the same unhittable
target. **Thickness is never spent inwards.** Everything inside a frame belongs to a program — a
terminal's text, an embedded application's picture, a surface's own widgets — so a band reaching
past the content rect would swallow that program's first and last column on every press.

**The corner arms are the vertical tolerance.** The last `CON_GRAB_CORNER` cells of every side are a
**corner** and take both axes; the block where the two bands actually cross is two cells, which is a
target a mouse has to be aimed at and a finger cannot land on at all. Four cells and not two,
because the top and bottom bands are one row each and cannot be more without costing every window a
row: the four rows of arm running down each side column are where a hand that wants a bottom corner
finds one, and along the top and bottom rows the same four cells are surplus that costs nothing. An
arm is **floored by the thickness of the band it crosses** — an arm shorter than that leaves cells
inside the corner block taking a single axis, which is a press one cell in from the corner that
changes the width and not the height — and clamped to a third of the side above that, or a window
would be all corner and could not be resized in one axis at all.

**Either button resizes from the border.** The right button is not the one a hand reaches for on a
border, and a left drag along a frame's own rule that does nothing reads as a window that cannot be
resized at all; inside the border there is nothing else a left press can mean, because every cell of
it belongs to the window manager and not to what is in the window. The exception is the title row,
which is the top border and is also the only handle the window has: left along it moves, right along
it takes the top edge, and its two ends resize like any other corner.

**One function answers where a press landed, for the drag and for the light.** `win_grab_at()` is
asked by the router when a button goes down and by the edge grip on every motion, so the arrows lit
on a border and the drag that follows a press there cannot disagree — a grip that worked its own
edges out is a light promising a resize the router does not perform, on exactly the corners nobody
tests. **A window with no frame refuses it**, and the list is the one `win_draw_all()` draws by: a
panel, a fullscreen window, a layer and the icon layer. Their frame rectangle is still inflated by
the border, so a grab that did not refuse them would resize a popup menu by a border nobody can
see.

**A bare right button inside a window belongs to whatever owns the cells.** Resizing from anywhere
inside is right for a window whose content the session draws and wrong for one that is a program's:
a right press the session takes inside a terminal or an embedded application is a button the guest
never sees, and a right press is what opens a context menu in every graphical application there is.
A terminal and an embedded window therefore ask for `Super` before a press inside them is a resize;
every border, theirs included, resizes under either button without it.

**A frame button is a chip: three cells the chip paints itself, the mark centred and its own fill
either side.** Nothing of the border survives inside one. Give the three the slot `ktui_draw_box`
is handed in the same call and the group reads as a run of border rather than as three things to
press — the border's rule shows through the gap between each pair and joins them — and on an
unfocused frame that slot is `KT_DIM`, which measures **1.45:1** against `KT_SURFACE` across the
seven schemes. The plate cells go the same way: a chip that kept the characters under it carries a
length of the title row's rule, which on a focused frame is the double rule and is most of the ink
the chip has. Spaces in the chip's own fill are what make the three cells one plate with one mark
on it.

**A chip carries its meaning in its fill**: `KT_ERR` under the one that destroys the window,
`KT_MID` under the two that do not, `KT_DIM` with `KT_TEXT` on it while the frame is unfocused, and
`KT_ACCENT` under the pointer — 10:1 or better in every scheme, and the one step that is
unmistakable on a chip that is already red. What a button does is taught by its resting colour; the
highlight has one job, which is to say the press will land here.

**Three resting fills are not available in this palette, and the mark carries the difference
instead.** Fill against fill, worst case across the seven schemes, `KT_ERR`/`KT_MID` measures
1.11:1 and `KT_ERR`/`KT_WARN` 1.28:1 — in two schemes a third bright fill would be the same plate
as the one beside it — and `KT_ACCENT` against `KT_WARN` measures 1.19:1, which would cost the
hover step on that chip the one job it has. So minimise and maximise share `KT_MID` and are told
apart by `↓` against `■`.

**Held down is the hover pair the other way up**: the plate goes to `KT_SURFACE` and the mark takes
`KT_ACCENT`, so a chip under a finger reads as pushed in rather than as lit brighter, at the same
10.49:1 the hover pair measures because it is the same pair. It shows only while the pointer is
still on the chip, so a hand moved off shows a chip that will not fire — which is the answer to
"how do I take this back".

**And the mark is a shape the fill encloses.** `↓` for minimise, `■` for maximise and restore, `X`
for close, drawn dark on every bright fill because `KT_TEXT` is 1.10:1 on `KT_ACCENT` and 2.17:1 on
`KT_ERR`. Dark ink is the window body's own colour to the eye — the palette's dark slots are within
1.20:1 of each other, and on a focused chip `btn_slots` hands the mark `KT_SURFACE`, the very slot
`win_draw_all`
fills the frame with. So the only thing telling a mark from the ground below the title row is the
strip of plate between its ink and the cell's floor, and a thin strip reads as the chip ending
early. It is a threshold, not an absolute: `_` is 24 lit pixels of 512 on rows 27 and 28 of 32 in
the console's `ter-kdos32n`, leaving three rows at the floor, and it is the one shape this fill
cannot
hold. The three that ship are 68, 108 and 80 pixels on rows 6–25, 10–21 and 6–25, six clear rows
or more above and below each. `↓` and `■` come from `ktui_glyph`, so a terminal without UTF-8 gets
`v` and `#` rather than the blank a 512-glyph
console font draws for a codepoint it does not carry; `X` is ASCII. Minimise is the down arrow
because down is where the window goes — the taskbar row, which is the way back from it.

**The chip lights by the rule the grip lights by.** The router hands the window model the pointer's
cell on every cooked pointer event, the leave included — libkwl reports a leave as an off-grid
position, and a highlight nothing retracts stays lit for the rest of the session. It hands over an
off-grid position of its own whenever a lock, a saver, a mark, a pick, a payload drag, a window drag
or a guest holding the pointer owns the event, because a chip lit under the hand in any of those
promises a click that lands somewhere else. A chip lights only on the frame the hit test answers
with, so one on a frame behind another window is painted and then covered like the rest of that
frame. The screen is repainted when the chip under the pointer **changes** and not when the pointer
moves: nothing else on the desktop reads that position, and a repaint per cell would re-send the
whole grid for a hand crossing an empty desk.

**A chip is three cells and its glyph is one**, centred, with the chip's own fill either side. The
mark is read by the plate AROUND it — the palette's dark slots are one colour to the eye, so the
ink cannot be told from the window body by its colour — and an odd width is the only one that can
put plate on both sides of it. **All three cells answer a press**: a one-cell target is the failure
a person feels, and the miss lands on the title row underneath and arms a move.

**A press arms a chip; the release fires it.** A chip that acted on the press gave a person no way
to change their mind, and the one that destroys the window is the one they most need to. Only the
left button arms: middle and right are not a click on a control anywhere else on this desktop, and
a chip that answered all three put the window's close on the button a hand reaches for a context
menu with.

**A narrow frame drops buttons from the left, and never all of them.** Three cells each plus a rule
to their left is the whole of what a frame has to spend, so minimise goes first — the taskbar row does
the same job and is always there — and close goes last. A frame that kept its title and dropped
every button instead leaves the smallest window the desktop can make with no way to close it but the
keyboard.

**The title is cut to end before the run.** `ktui_draw_box` lays a title out from the third column
and closes it with a space, and the buttons are painted over the same row afterwards, so a title
long enough to reach them ends inside a chip, mid-character and in the chip's colours. The cut is
made in columns and not in bytes: a title is whatever the program set, so it is UTF-8, and half a
character is the `?` every unmapped codepoint becomes.

**A minimised window keeps its taskbar row**, because the row is how it comes back: it is drawn
nowhere, cycled past and not hit-testable on the desktop. **Three routes back exist without it** —
the window list on `Super+F2`, which holds it and marks it `↓`; `Super+Shift+n`, which brings back
the window that went away last and takes one press per window after that; and `Super+Alt+Shift+n`,
which brings back everything put away on this workspace at once. That is also why a window with no
row of its own — a dialog, a dock, a splash — cannot be minimised by itself, and why its frame is
drawn without the minimise button: it goes away with the window it belongs to and comes back on
that window's row.

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

**The table is the whole of what a window never receives, and the rest reaches it untouched.** That
is true of a terminal, a cell surface and a boxed graphical application alike: `Ctrl+W` closes a
browser tab, `Ctrl+T` opens one, `Ctrl+C` interrupts and `Alt+F4` reaches the application, because
none of them is bound here. **Three chords are not on Super and are the only three**: `Alt+Tab` and
`Alt+Shift+Tab` switch windows, which a lifetime of muscle memory earns and which an application
wanting Tab-with-Alt therefore does not get; `Ctrl+A` is the leader, and pressing it twice sends the
literal, which is what gives a shell back its start-of-line; and `Print` with its two shifted forms
captures, on a view with a real keyboard only. The media keys are bound bare because that is where a
keyboard puts them and nothing competes for them.

**A modifier is never taken from anything.** A modifier produces no character, so a view sends no
cooked message for one and no chord can consume it — an embedded guest sees `Ctrl`, `Alt`, `Shift`
and `Super` go down and come up whatever the session does with the letter that follows. The key a
chord *did* consume has its release swallowed with it, because a press the guest never saw must not
be followed by a release it did.

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
emits, and its `app_id` groups windows of a kind — every terminal window is `terminal`, and a guest
on a terminal of its own is `kdos-cage`, while an embedded guest carries the application's own name
— the basename of the program its generated launcher runs. The panel resolves an entry from that:
by file stem where the two agree, and by `StartupWMClass` where they do not, which is how GIMP's
`gimp.desktop` is found from `gimp-3.0`. A family of entries sharing one program — the LibreOffice
rows all run `libreoffice` — resolves to neither and keeps its own title. So the session records the
program a window was opened for once, at open, and matches on that. A terminal entry started from
the menu declares it through `kdos-term --app-id`.

**The workspace switch comes before the raise, never after.** Going to a workspace clears the focus
and cycles to whatever the ring lands on, so a raise before it is undone; and a minimised match is
un-minimised where it is rather than through the restore path, which *moves* a window to the
current workspace — the opposite of going to it.

The compositor binds the same seven, as `rc.xml` `ForEach` blocks whose `<query identifier>` is the
program's `app_id`. `W-grave` carries the [scratchpad](#the-scratchpad) on both, which is a
`ForEach` of the same shape over a marker rather than a program.

**`Super+p` configures a console screen.** `kdos-display` asks the session for its outputs, the
session asks the view that is driving them, and the answer is every connected connector with the
modes it published and which one is in force. Choosing one sends it back down the same path and the
fifteen-second countdown starts: the console has no second screen to fix an unreadable mode from,
so the mode reverts unless a person says to keep it. Off, scale and rotate are drawn disabled —
they are Wayland's verbs, and [Known gaps](../06-reference/known-gaps.md) says why.

**A KEPT mode survives the session, in `~/.local/state/kdos/con-modes`.** One
`<connector> <W>x<H>@<mHz>` line per screen, written by the **view** — the end that knows what a
connector is called and may be at the far end of an ssh link driving its own machine's monitors —
and written only after the mode took, so a mode the driver refused is never the one the next login
asks for. It is a geometry and never an index: the row a picker sends is a position in the list
that screen published this boot, and a cable, a firmware update or a different monitor publishes
them in another order. A geometry that is no longer on the list leaves the screen on the mode the
monitor prefers, and every other screen's line survives a keep on this one. The `keep` flag is what
distinguishes the two: an apply sends 0 and writes nothing, so a screen nobody can read is never
what the next login comes up on.

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
| `Super+b` | send the focused window to the **back**, and give the keyboard to what comes forward |
| `Super+Shift+s` | fold the **next** window of the ring into this one as a tab |
| `Super+]`, `Super+[` | the next tab, and the one before it |
| `Super+Alt+s` | take the stack apart; every tab back on the desk as a window |
| `Alt+Space` | the window menu, over the focused window |
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

### Tabbed windows

**Two windows in one rectangle, showing one of them at a time.** `Super+Shift+s` folds the next
window of the ring into the focused one; `Super+]` and `Super+[` walk the strip; `Super+Alt+]` and
`Super+Alt+[` carry the live tab along it; `Super+Alt+s` takes the group apart. It is Haiku's
stack, and **stacking is the whole of it** — nothing here puts two windows side by side and moves
them together.

**The pointer does both of the things the chords do.** A press on a tab brings that tab up — the
live tab is not a target, because a press on it is a hand reaching for the frame — and a title-bar
drag released over another window's title row folds this window into that window's stack. A drop
anywhere else is the edge snap it always was, and the join is asked first: a window dropped on a
title row against the top of the work area is one gesture and cannot be both.

**The carry is on Alt and not on Shift, and that is not a preference.** The chord table normalises
the CASE of a letter and nothing else, so `Shift+[` arrives as the `{` the layout produced and a
binding naming `[` with a Shift modifier would match nothing at all — the chord would fall through
to the focused window, which would type a brace into it. Every `Super+Shift` chord on this desktop
is on a letter for that reason.

**The tab on screen is an ordinary window and every other tab is hidden**, by the same flag the
scratchpad is put away with. That is what makes a stack one window to everything that counts
windows: **one taskbar row, one step of the cycle ring, one hit rectangle, one window-list row**,
and a guest behind a tab that is not showing sleeps exactly as one on another workspace does. None
of that is code the stack added.

**The strip replaces the title, not a row.** It runs from the frame's third column to the gap before
the `↓ ■ X` chips, which is the run the name occupies, so a stacked frame costs no cells at all —
and a window in no stack draws no strip, which is why every frame in this book looks the way it
always has. The window's number and its name go into the live tab, which is the only tab the ring
can reach.

**A tab is read by the plate around its name, exactly as a chip is.** The live one is `KT_SURFACE`
on `KT_ACCENT` — the pair a chip takes under the pointer — and a resting one is `KT_TEXT` on
`KT_DIM`, at 8.3:1. There is no third pair and none dims with the frame: **the strip says which tab
is up and the frame says which window has the keyboard**, and a strip that went flat on an unfocused
frame would leave a person unable to read what a stack will show when they click on it.

**When the tabs outnumber the columns the strip collapses to a counter.** Below `CON_TAB_MIN` columns
per tab the names are initials and the strip has stopped saying anything, so ` 3/8 ` is drawn
instead — which tab of how many, the one thing still worth a cell — and the rest of the run is left
as the rule the box drew.

**The strip is ordered by window id until something moves a tab, and never by the z-order.**
Bringing a tab up moves it to the front of the stacking list, so a strip drawn in list order would
reshuffle on every switch; ids only ever go up, so id order is the one order a person can point at
twice. A carry numbers the whole strip 1..n first and then swaps two of those numbers, so a strip
is **either entirely implicit or entirely explicit and never half of each** — and a window folded
into a strip nobody has reordered still arrives in id order, which is where a window with a lower
id than every tab belongs.

**A tab takes the stack's rectangle before it hides**, through the same call an ordinary resize goes
through — a terminal reflows and a guest is configured — because a tab carrying some other size
draws that size into the frame the moment it comes up. The tile state travels
with it, so a `Super+`arrow after a switch snaps from what the stack is in.

**The keyboard moves only when the tab that went away held it.** Stepping a stack while somebody is
typing in another window leaves the keyboard where it is.

**Closing the tab on screen promotes an heir**, the way an owner that goes does: the frontmost
survivor takes the rectangle and the rest re-point at it, and a stack with one tab left is not a
stack. A tab nothing pointed at would be a hidden window with no row, no ring step and no rectangle
— reachable by nothing on the desktop.

**Chrome, a guest on a terminal of its own and the scratchpad cannot be tabs.** The first two are
not things a person switches between; the scratchpad is put away with the very flag a stack hides
its members with, so folding it in would make two mechanisms disagree about whether it is on the
screen.

**A stack survives a saved session, as a group letter and not as a second file.** A row's flags
column carries `g<group>t<position>` and an `h` on the tab that was on screen; the group number is
this save's own, because a window id means nothing to the next session. On the way back the first
member of a group to appear becomes its anchor and every later one joins that anchor's stack, which
is what makes the order the rows arrive in — and an application's window does not exist until it
attaches — not matter. A file written before the column existed restores as windows in no stack.

**It is this desktop's alone.** `kdos-comp` has no stacking, so `rc.xml` binds nothing for these
six chords — and it binds nothing *else* to them either, so the day the compositor grows tabs the
chords are still free to mean the same thing.

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
is currently filling, and the tile itself comes back from the last field. **Fullscreen is orthogonal
to the tile**: it leaves the tile flag alone and writes that rectangle only from an untiled window,
and leaving fullscreen re-derives the tile from the flag rather than replaying a rectangle — so a
window fullscreened while tiled comes back to the tile the grid has, and its pre-tile rectangle
survives to the geometry table. **A remembered rectangle is fitted, not trusted**: it goes through
`kwm_fit()` into the work area, so one kept on a wide screen still comes back onto a narrow one. And **a second window of the same program does not land
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

**An embedded graphical application is a target like any other.** The same four verbs go out over
its cage's channel instead of over the surface socket — `KEMBED_DRAG_ENTER` and its three — and the
cage turns them into the `wl_data_device` events its guest understands. A guest that begins a drag
of its own is read out at once and the session takes it over, so it can be released on a terminal,
on another guest or on the trash. See [`kdos-cage`](kdos-cage.md) for the grab that makes the
guest's own acceptance of a type mean something.

**Whatever the drag was over is told when it ends, however it ended.** A target keeps state for the
length of one — a KDOS surface holds the `ENTER` it was sent, a cage holds a pointer grab and an
offer in the guest's hands — and one never told it ended keeps both for as long as it lives. The
drop is the exception and is ordered the other way: a leave sent after a drop would take back the
offer the drop just made.

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
left alone. **Whether the desktop is showing is asked of that set and not latched**: every other
way back — `Super+Shift+n`, `Super+Alt+Shift+n`, a window list row, a taskbar row — empties it
without the chord running, so a flag would answer "showing" to a full desk and spend the next press
bringing back nothing. A press that finds none of its set still away hides instead.

**The session's own window list is the fallback, not the desktop's.** Arrows and digits pick,
`Enter` raises the window or restores it when it is away, `Delete` closes, `m` puts one away and
brings it back, `Escape` leaves. It is what a session with no shell running has, and it is drawn
from the list the session already holds. `kdos-teams` shows the same windows through `libkdisp`,
which is the list every other surface reads.

**It holds what the taskbar holds, not what the ring steps through.** A minimised window is in it,
marked `↓` in its first column and drawn in `KT_DIM`, exactly as its taskbar row is — the list is
the bar for a session that has none, and a bar that dropped the rows a person is looking for is not
a fallback. One application is one row, a layer and a docked panel are out, and a hidden window's
chord is its way back rather than a row. **A window a modal stands over keeps its row**, which is
the second place the list parts from the ring: a row is not a step, so `Enter` on it lands on the
question and flashes it exactly as a click on its taskbar row does, and the question carries no row
of its own — a list that dropped the owner as well would answer "no windows" to an application with
a save dialog open.

**The number on a row is the row's own, and it is the digit that picks it.** It is not the ring
number the title bars and `Super+Alt+`*n* mean: the ring holds no minimised window, so every row
this list exists to offer would be numbered 0 — which is why a minimised taskbar row carries no
number either. The two run apart wherever the two sets do: a minimised window and a window a modal
stands over are rows the ring does not step to, and a dialog is a ring entry with no row.

**Tile, cascade and rearrange are the console's alone.** labwc has no tile-all or cascade action and
its `MoveResize` is not the same interaction, so binding the nearest thing there would make one
chord mean two different things on the two desktops — which is the one rule `keys.conf` and
`rc.xml` exist to keep.

**And so is `lower`, because the ring cannot stand in for it.** Every step of `Super+Tab` and
`Alt+Tab` *raises* what it lands on, so two windows of the same size fully overlapped stay in the
order they are in however many times they are stepped — the one underneath is unreachable. `Super+b`
moves the focused window to the tail of the stack, deepest-first so that a dialog goes down with the
window it belongs to and stays above it, and the keyboard goes to whatever comes forward: a focus
left on a window now covered by another is one whose keys land where nobody is looking. Lowering a
window a person is *not* typing in leaves their focus alone, which is the rule a minimise keeps for
the same reason. `Super+b` is free on both desktops unmodified, and labwc's own `Lower` action is
bound to nothing in `rc.xml`.

**`Alt+Space` is the window menu, and one of the few chords this session takes that is not on
`Super`.** It cannot be one: `Super+space` is the palette and `Super+Shift+space` the taskbar, so a
third form of that key would be three unrelated things a modifier apart — and `rc.xml` already opens
labwc's client menu with `Alt+Space`, so the two desktops answer it alike. The menu itself, what it
holds and how it is reached with a pointer or a finger, is in
[What the pointer does](#what-the-pointer-does).

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
- **The window menu names no workspace by letter.** Its `Send to workspace` pane is picked with the
  arrows, `Enter` or the pointer: `ktui_menu_accel_of()` reads a *letter* after the `&` and answers
  nothing for anything else, so a marked digit would be an underline advertising a key the menu does
  not answer. Each row still prints `Super+Shift+`N, which is the chord that does it, and every other
  row in the menu carries a letter.
- **A touch pointer moves a cell at a time.** A real pointer reaches a guest in pixels over the raw
  stream, and a real keyboard reaches it through the person's own layout; a touch event is
  synthesised from the gesture recogniser with no raw partner and so arrives at the grid's
  resolution.
- **No VT has ever been allocated.** The `--vt` path compiles and links and has never been run: it
  needs an ISO with `kdos-cage` in it and a machine with real terminals. Embedding, which is the
  default, has been run end to end.
- **No input method** — the candidate window is drawn and the engine is not running. Stated once,
  in [known-gaps](../06-reference/known-gaps.md).
- **A boxed application is rendered at scale 1 on every shipped font.** The number the session
  offers a guest is its view's cell height over a reference cell of eight by sixteen, stepped down
  until it divides the cell's width too, and `con.conf`'s `font = monospace:size=12` IS that
  reference cell. The mechanism is built at both ends and engages at a cell thirty-two pixels tall
  with an even width — `Super+=` stepped that far, or a `font =` written that large. Stated once, in
  [known-gaps](../06-reference/known-gaps.md).
- **One card.** `libkkms` takes the first `/dev/dri/card0..7` with a connected output and lights
  every connected connector on it; a second card's screens are not reachable, and `--card PATH`
  chooses which card that is.

## See also

- [The session](../03-architecture/session.md) — two sessions, one bring-up
- [Boot and init](../03-architecture/boot-and-init.md) — the tty1 chain
- [kdos-comp](kdos-comp.md) — the other desktop, and libkwm's other caller
- [Filesystem and IPC](../06-reference/filesystem-and-ipc.md) — both sockets and every verb
