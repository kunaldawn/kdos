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
- **Exact-cell screenshots.** `kdos-view --dump` is a view like any other.

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

**Only the shell's own panel replaces the built-in taskbar.** `kdos-con` draws its taskbar until a
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

A view says in its hello how many pixels one of its cells is and whether it can put a sprite's bytes
on a screen. The session uses the first for sizing an embedded guest — the primary view's, because
that is the display the person is looking at — and the second to decide how often it may send a
frame: at the session's own redraw rate when something can show pixels, and once every 250 ms when
nothing can, because a window of pixels at a compositor's frame rate down an `ssh` link is a link
that does nothing else.

**Every view is sent the same thing.** A view that cannot show pixels turns each cell of the picture
into the character whose shape covers the same part of a cell — the matcher behind `kdos-ascii` —
and colours it with the nearest palette slot to that cell's average. That happens **in the view**,
which is the only end that knows whether this build has a font at all; the session holds no font and
no pixel code and must not gain one.

So an embedded application over `ssh` is characters, by shape, and it falls out of the negotiation
rather than being a special case.

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

**Every dragged rectangle goes through `kwm_fit`**, exactly as a snap does. A dragged window and a
snapped one obeying different work-area rules would be two answers to one question, and the panel's
exclusive zone is in that answer.

**The panel is not draggable and neither is a fullscreen window.** The panel's rectangle *is* the
exclusive zone, so moving it would move the work area out from under every other window.

## The chords, and the leader that reaches them

Every window-management chord is on Super, so none of them can collide with what a program inside a
window wants. A view with a screen of its own reads Super from `libinput` and they all work.

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
- **The screenshot is cells rather than an image.** `kdos-shot` writes the grid as text; rendering
  cells to a picture is `libkcell`'s, which needs fcft and pixman, and `kdos-tools` is on every
  image and links neither. A *recording* is a picture — `kdos-view --cast` rasterises through
  exactly that library — so a still image is the thing that is missing and not the moving one.
- **No input method.** fcitx5 is a Wayland client.
- **Single output.** `libkkms` takes the first card with a connected output and its preferred mode.

## See also

- [The session](../03-architecture/session.md) — two sessions, one bring-up
- [Boot and init](../03-architecture/boot-and-init.md) — the tty1 chain
- [kdos-comp](kdos-comp.md) — the other desktop, and libkwm's other caller
- [Filesystem and IPC](../06-reference/filesystem-and-ipc.md) — both sockets and every verb
