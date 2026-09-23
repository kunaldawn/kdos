# kdos-term

`kdos-term` is the KDOS terminal emulator. Under `kdos-comp` it is an `xdg-toplevel` window; with
`--tty` it draws inside somebody else's terminal, and with `--dump` it renders one frame offscreen
and prints the cells.

It ships beside `foot` and displaces nothing. `foot` remains the default terminal in `rc.xml`,
`menu.xml`, the `.desktop` files, `favorites` and `tmux.conf`, because `foot` is battle-tested and
this is not — the same trade [`kdos-res`](kdos-res.md) makes beside `btop`.

## Synopsis

```
kdos-term [--title TEXT] [--app-id NAME] [--font NAME] [-D DIR]
          [--size WxH] [--float] [--tty] [--dump WxH]
          [-e CMD [args...] | -- CMD [args...]]
```

| Option | Effect |
|---|---|
| `-e`, `--exec CMD` | Run `CMD` instead of the shell. Everything after it is the child's argument vector |
| `--` | The same, for a caller that would rather not spell `-e`: everything after it is the child's |
| `--title TEXT` | The window title, until the program sets one |
| `--app-id NAME` | The identity the desktop files this window under. Defaults to `kdos-term` |
| `-D`, `--working-directory DIR` | Enter `DIR` before the fork |
| `--size WxH` | Open at this many columns and rows, rather than at the size `term.conf` asks for |
| `--float` | Open unanchored, at that size — what a desktop entry's `X-KDOS-Float` asks for |
| `--font NAME` | A fontconfig name, overriding `term.conf` |
| `--tty` | Draw on the terminal this was started from |
| `--dump WxH` | Run the child to completion offscreen and write the cells |
| `-h`, `--help` | Usage |

`-e` takes everything after it as the child's argument vector, as it does in every terminal there
has ever been. There is no shell anywhere on the path: the vector is built by `libkxdg` and
executed directly, because `$SHELL` and `term.conf` are both strings somebody else wrote.

`--app-id` is the identity, and it is not the title. A title is the guest's to rewrite the moment
it emits an OSC; the app id is the desktop's, and a taskbar row, a window rule and a run-or-launch
chord all key on it. The default is right when the terminal is the application. A terminal running
somebody else's program is given that program's name, the same way `foot --app-id` is;
`sh_term_argv_in()` passes both to whichever emulator it named, so a `Terminal=true` desktop entry
opens a window that says what is running in it.

`-D` is spelled as `foot` spells it on purpose. Every caller in `kdos-shell` names the terminal
through one helper and passes the same flags whichever program that helper resolved.

`--dump` runs the child to completion, consumes everything it wrote, and prints the cells. That is
the whole terminal short of a display, which is why the self-test's golden frames are taken through
it.

The exit status is the child's. A terminal opened to run one command is a wrapper around it.

## What it is built from

| Library | Contributes |
|---|---|
| [`libkvt`](../05-developer/c-libraries.md) | The VT100–VT520 state machine, a child on a pty, and the screen as `KtuiCell`s |
| `libkdisp` | Which display server, decided once at startup |
| `libkimg` | The one place untrusted image bytes become a picture |
| `libktui` | The cell grid, the sprite table and its budget |
| `libkcell` | The one scale-and-cut from a decoded picture into sprite tiles |

Naming `kwl_impl` is what links the Wayland half in. `libkdisp` picks the first implementation
whose probe succeeds, and a `--tty` or `--dump` run opens no display at all.

The terminal draws no chrome of its own beyond one frame. Under the compositor the server-side
decoration is that frame, so the drawn one is suppressed; on a virtual terminal and in a dump
nothing else draws one, and then it is the only frame there is. Inside it is the terminal, edge to
edge. A terminal is a full grid of text, and a plate, a header band or a button row would each
cost it a row.

## The escape layer

### Keys go in as keysyms

A caller that writes `\x1b[A` for an arrow is wrong the moment a program sends DECCKM, which `less`
does on its first screen. So a key enters as a keysym and a modifier set, and `libkvt` decides what
bytes it becomes. Application cursor mode, keypad mode and the modifier encoding all live there,
and there is one implementation of them for every consumer of the state machine.

`TERM` is `xterm-256color`, because that is what the state machine implements and what ncurses
already ships an entry for; a private `TERM` breaks the first time somebody types `ssh`.
`COLORTERM` is `truecolor` beside it, which is the only way a program learns that the sixteen-colour
entry `TERM` names is an understatement. `libktui`'s own capability probe reads it, so without it a
KDOS surface running inside a KDOS terminal detects 256 colours and draws the theme approximately.

### Mode queries are answered in the form they were asked

`DECRQM` has two spellings and the reply carries the marker of the request. `CSI ? <mode> $ p` asks
after a DEC private mode and is answered `CSI ? <mode> ; <value> $ y` — for application cursor
keys, auto wrap, the cursor, every mouse mode, focus reporting, the alternate screen, bracketed
paste and synchronized output. `CSI <mode> $ p` asks after an ANSI mode and is answered without the
`?`, for the four that exist: `2` KAM, `4` IRM, `12` SRM, `20` LNM.

Mode `4` means two different things in the two spellings, so an unmarked reply to the second would
be read as an answer about the first. A mode nothing implements is answered `0`, meaning "not
recognised", which is what a probing program is built to handle. Silence is what it is not built to
handle: it waits, times out, and draws like a terminal from 1978. Answering is what makes every
later mode safe to add.

### Focus, and holding a frame

Focus is reported when the child asks for it (`DECSET 1004`, `CSI I` in and `CSI O` out). An editor
that is not told it lost the focus does not reload a file changed underneath it, so its next write
goes over somebody else's work — nearer to data loss than to polish. It is silent until asked,
because a terminal that wrote `CSI I` unasked would put two stray characters into every program
that never requested them. It is reported from a diff against the previous turn of the loop rather
than from an event, because the display server already holds the answer and does not deliver it as
one.

Synchronized output (`DECSET 2026`) holds the frame while the child draws it, so a program that
brackets its screen is never seen half-drawn — which on a slow link is the difference between a
frame and a tear. The hold is released by a 150 ms watchdog, because a child that sets the mode and
dies would otherwise freeze its window forever and the terminal cannot tell that from a program
taking its time. The rule lives in `libkvt` rather than in each renderer, since two copies of a
timeout is two timeouts.

Every KDOS surface brackets its own frames the same way when the terminal it is running in answers
the probe. `libktui` asks with DECRQM and emits nothing where the answer is "not recognised";
inside this terminal the answer is yes.

### Picture capabilities

The primary device attributes report sixel. `chafa`, `img2sixel`, `lsix`, `timg` and
`mpv --vo=sixel` all send `CSI c` and read parameter `4` as sixel support, so a reply without it
makes every one of them fall back to half blocks on a terminal that decodes sixel, OSC 1337 and the
kitty protocol. The conformance level stays 60: raising it would claim something this state machine
has not been measured against, which is a different promise.

`CSI ? Pi ; Pa ; Pv S` reports how big a picture may be. `Pi` is 1 for the colour registers and 2
for the sixel geometry; a read is answered `CSI ? Pi ; 0 ; <values> S`, and anything this terminal
cannot do is answered with a failure code rather than with silence. The geometry reported is the
one actually enforced — `image_cells` in each direction, never more than the grid has, multiplied
by the cell size — and it is re-stated whenever the window is resized, because half that bound is
the number of columns and rows.

A set is refused rather than ignored. The geometry is the configuration's and the register count is
the decoder's, so a program that believed it had negotiated a larger picture would send one and
have it clipped. `Pa` 3 is answered `Ps=3`.

`CSI 16t` reports what one cell is in pixels, height first. Only the program drawing the glyphs
knows it, and a client that is not told has to guess — a guessed cell is a correctly encoded
picture at the wrong scale, which looks like a broken decoder rather than a missing answer. A
terminal nobody has told answers nothing rather than inventing a number, because a client cannot
tell an invented answer from a real one.

`CSI Ps S` and `CSI ? … S` differ by the private marker alone, and the first scrolls the screen. A
terminal that misses the marker answers a capability probe by scrolling: the program learns
nothing, and what it sees is its own output moving.

`APC G a=q` is answered. A picture program sends a one-pixel transmission with `a=q` and waits —
`OK` means it may use the protocol, an error code means it must fall back, and nothing at all means
it waits for its own timeout and then draws as though this were a teletype. The id is echoed so a
program with several queries in flight knows which it is hearing about, and a query stores no
image. A terminal with `images = no` answers the query too, with `ENOTSUP`: silence there would
cost somebody who turned pictures off exactly what not implementing the protocol would.

### Colour and attributes

The sixteen named colours are reduced to the theme's eight slots by nearest distance, so
`kdos theme` moves a terminal's red along with everything else. On `tty1` the kernel is already
drawing them out of the colour map this desktop installed, and a terminal whose red stopped
following the theme would be the one window on the screen wearing somebody else's scheme.

Everything above those sixteen is a literal the program chose exactly: the 216-colour cube, the
greys, and a 24-bit `SGR 38;2`. Reducing those to eight slots is what loses a picture, and no
palette names them, so nothing is gained by it. The cell carries the reduction as well, so a
renderer that draws only the eight slots draws what it always drew.

The two default colours are not reduced; they are slots. A terminal's default foreground is a light
grey and its default background is black, and reducing both by distance against eight phosphor
greens lands them on the same slot — which draws every character in the colour of the screen behind
it. "Default" means whatever this desktop calls text and background, so it is answered with
`KT_TEXT` and `KT_BG` directly. A colour a program actually asked for is still reduced, including
one that reduces to its own background, because a program writing black on black meant to.

The underline's shape and colour travel with it. `SGR 4:0`–`4:5` select none, single, double,
curly, dotted and dashed; `SGR 58` gives the line its own colour and `59` takes it back. A renderer
with one shape draws the plain line, because the attribute means "this word is marked" and a shape
nobody drew is worse than the wrong shape. They are emitted onward only where 24-bit colour is:
`4:3` is a sub-parameter, and a terminal old enough to want indexed colour is old enough to drop
the colon and read the pair as `SGR 43` — a green background where a program asked for a wavy line.

Six styles are drawn and two are dropped. Bold (`SGR 1`), underline, inverse, italic (`SGR 3`),
strikethrough (`SGR 9`) and overline (`SGR 53`) each ride a bit of the cell's attribute byte, so
they cost no storage beyond the byte the cell already carries. Bold is drawn from a bold companion
face where the loaded font has one whose cell matches, and by striking the mask twice where it does
not; italic works the same way, from an italic companion where there is one and from the upright
mask sheared a little over twelve degrees where there is not. `blink` and `dim` are parsed and
reach no bit at all: a blink drawn as bold is a lie about the text, and a terminal that lies about
which words are emphasised is worse than one that shows them plainly.

## Notifications from a running program

`make && notify-send done` does not work on this image, because `libnotify` is not a port and
`notify-send` is not here. The escape sequences are, and they cost a parse rather than a
dependency, so a long job can say so from inside the terminal it is running in and the desktop's
own notification daemon shows it like any other toast.

| Sequence | Whose | Carries |
|---|---|---|
| `OSC 9 ; text` | iTerm2's, and the common one | A summary |
| `OSC 777 ; notify ; summary ; body` | urxvt's, which `tmux` forwards | Both |
| `OSC 99 ; params ; text` | kitty's | A summary, past a parameter list |

Three spellings, because none of them won and all three are what programs actually emit.

`libkvt` raises nothing itself. A toast means a session bus, and that library links nothing and
must not start one; the terminal registers a handler, and a terminal that sets none drops the
escape, which is right for one with no desktop behind it. The application name is the window's
title, so somebody with four builds running can tell which one spoke.

`OSC 99`'s parameter list is parsed and ignored. This terminal has no notification identity, no
replacement and no progress, so honouring `d=0` would be claiming a facility that is not here.

## The paste guard

With bracketed paste on, the child sees a paste as text and decides for itself what to do with it.
With bracketed paste off the bytes go straight to the pty, so a newline in them executes: at a
plain shell, at an `ssh` password prompt, inside `read`. That is the one place a terminal can be
made to act as the user, and it stopped being a thought experiment the moment a clipboard could
carry what a remote shell printed.

A payload containing a newline, a carriage return or any other control byte except tab is held
back, and `kdos-term` asks with a dialog naming how many lines would run. A tab is not held: it is
what a person pasting a table has, and it submits nothing.

`paste_guard = no` in `term.conf` turns the guard off, which is what somebody pasting all day into
a program that never enables bracketing will want.

## Selection and the two clipboards

| Gesture | Does |
|---|---|
| Left drag | Selects, and puts the selection on the primary clipboard on release |
| Double click | Selects the word under the pointer, and nothing at all on blank or unwritten space |
| Middle click | Pastes the primary selection |
| `Ctrl+Shift+C` | Copies the selection to the clipboard |
| `Ctrl+Shift+V` | Pastes the clipboard |
| `Shift+PgUp`, `Shift+PgDn` | Half a screen of scrollback |
| `Ctrl+Shift+Up`, `Ctrl+Shift+Down` | Jump to the previous or next prompt mark |
| Wheel | Scrollback, or the child's own scrolling when it has asked for the mouse |

A press and a release in the same cell is a click, and a click selects nothing. Without that rule
every click leaves a one-character selection on the primary clipboard, which is what the next
middle click pastes.

A program that has asked for mouse reports gets the pointer, because it is drawing its own idea of
what is selected and a second selection on top of it belongs to nobody. Shift is the override, as
it is in every terminal: it hands the pointer back so text can be taken out of a program that
captured it.

A paste arrives with newlines already turned into spaces, by the same filter that stops a paste
pressing Enter in a text field. In a shell that is the same protection, and it is why a multi-line
paste runs as one line rather than as a sequence of commands.

Nine chords are claimed and no more: the six above behind `Shift` or `Ctrl+Shift`, and the three
font chords below behind a bare `Ctrl`. Every chord a terminal eats is a chord no program running
inside it can use, which is what `Ctrl+Shift` is for. The three that do use a bare `Ctrl` are `=`,
`-` and `0`, and they can: the state machine has no `Ctrl` rule for any of them, so a child handed
the plain character cannot tell the chord from the key. Shift must be up for those three, since the
shifted spelling of `=` is `+` — a different character on every layout — and leaving `Ctrl+Shift`
alone keeps `Ctrl+_` reaching the child.

## The font, per window

`kdos-term` is one process per window and the face is a process-global in the cell painter, so the
size belongs to this window alone. Two terminals side by side under `kdos-comp` can sit at two
sizes.

| Chord | Does |
|---|---|
| `Ctrl+=` | One step larger |
| `Ctrl+-` | One step smaller |
| `Ctrl+0` | Back to the size `term.conf` asked for |

A step moves the size in the fontconfig name — `:pixelsize=N` or `:size=N`, whichever the name
carries — and is clamped at both ends, because fontconfig will return a two-pixel face and a window
of unreadable specks is not a step a chord can undo. A scalable face moves on every step. A bitmap
face answers with the nearest strike it carries, so a step that lands between two of them leaves
the cell exactly where it was and nothing on the screen changes until a step crosses into a size
the file holds.

A different cell is a different number of columns and rows, so the grid is recut and the child is
resized exactly as it is when the window is. A picture already on the screen goes blank until the
program sends it again: every tile was scaled to the old cell, and only the program that
transmitted it can say what it should look like at the new one.

In a `--tty` run the key is left alone and reaches the child. There the font belongs to the
terminal this one is running inside.

## Hyperlinks

A program marks a run of text with `OSC 8 ; params ; URI` and ends it with the empty form. The
address is interned and the cell keeps a 16-bit id, so the text can be scrolled back to and still
be the link it was. `KtuiCell` is not widened for it, because a link is a property of a terminal's
buffer and not of every surface the toolkit draws. Two runs of the same address are one link
whether or not the program said so: the `id=` parameter is ignored, so a program reusing an id for
a different address cannot make one run of text point at another's.

Hover underlines the whole run and `Ctrl`+click follows it. The run rather than the character,
because an address is one thing and underlining the letter somebody happens to be over says nothing
about where it ends. `Ctrl` rather than a plain click, because a link sitting in a screenful of
text must not be a trap for somebody selecting a word. It opens through `kdos-appbox open`, the
MIME route the portal's `OpenURI` takes, as an argument vector and never a command line.

Four schemes are accepted and nothing else — `http`, `https`, `file`, `mailto` — and every byte
must be printable ASCII. Anything that can write to a terminal can write an OSC: a `cat` of a
hostile file, a program on the other end of an `ssh`. A control byte would reach an argument
vector, and a byte above 126 makes the same address read two ways depending on who decodes it,
which is how an allowlist gets walked around. A refused link is text: the characters are drawn and
no link is offered, so there is nothing to be made to follow.

The table is capped at 128 addresses and is never freed while the terminal lives. Both halves of
that are one decision: an id in the scrollback can always be resolved, which is only affordable
because a child emitting a fresh URI per cell cannot grow the table forever. Past the cap the text
is still text.

The session's own terminal windows do the same thing through the same library, so a link works in
whichever terminal a person has. What a link resolves to on a bare virtual terminal is a separate
question, covered in [known gaps](../06-reference/known-gaps.md).

## Prompt marks

A shell says where its prompt starts with `OSC 133 ; A` and what the last command exited with using
`OSC 133 ; D ; <status>`. The shipped `/etc/bash.bashrc` emits both from `PROMPT_COMMAND`, outside
the `starship` branch: `starship` is a port and is installed, so a version embedded in `PS1` would
be dead on every real login here.

`Ctrl+Shift+Up` and `Ctrl+Shift+Down` jump between the marks. A screen of build output has one
prompt at each end of it, and scrolling by lines to find the last one is what this replaces. The
mark is kept on the line, so it survives into the scrollback — which is exactly when it is worth
having, because the interesting prompt has already gone off the top.

The status lands on the prompt the command was typed at, not on the line the shell reported it
from; those are several lines apart in any command with output. A prompt with nothing finished yet
carries no status, and that is not the same as zero — "it worked" and "nobody said" are different
things to draw.

The mark is drawn on the frame's left border where there is one. A terminal has no gutter: every
column belongs to the child, so the only column this program owns is its own border. An undecorated
window has one; a window the compositor decorates does not, and gets no dot, though the chords
still jump. The colour carries the meaning — a bullet in the error slot for a command that failed
and in the accent for one that did not, with a dot where nothing has finished.

`B` and `C`, the end of the typed line and the start of output, are parsed and not kept. Getting
them right needs a `DEBUG` trap that also fires for the prompt's own commands, which emits "output
started" while the shell is still waiting for a key; a mark at the wrong moment is worse than no
mark, and nothing here reads them.

## Pictures

Three protocols are supported: sixel (`DCS`), iTerm2's `OSC 1337` and kitty's `APC G`. `libkvt`
delimits all three and decodes none of them, handing the payload here. This program strips the
transport encoding, decides how many cells the picture occupies, and writes sprite cells into the
screen. The scale and the cut into tiles are `libkcell`'s, shared with `kdos-peek` — two
implementations of that would be two answers to how a photograph is resampled.

| Sequence | What is supported |
|---|---|
| `DCS q … ST` | Sixel, at its natural size |
| `OSC 1337;File=…:<base64>` | `inline=1` only, with `width`/`height` in cells, `px` or `%` |
| `APC G a=T/a=t/a=p/a=d` | Transmit, place, delete, the chunked form, `f=100` (any format `libkimg` reads), `f=24` and `f=32` |
| `APC G a=f/a=a` | Animation: a frame with its delay, composed onto an earlier one, and the run, stop and loop-count controls |
| `APC G a=q` | The capability query, answered `OK` with the id echoed, or `ENOTSUP` where pictures are off |
| `U+10EEEE` cells | The Unicode placeholders, which is how a picture reaches this terminal through `tmux` |

Pictures are written into the screen, not into an overlay beside it. That is what makes a picture
scroll with its output, disappear on `clear` and reach the scrollback — three behaviours an overlay
would have to reimplement against a screen already doing all three.

An animated GIF arrives whole and takes the same frame machinery the kitty protocol fills a frame
at a time. `libkimg` returns every frame with the delay after it; the first is placed and the rest
go into the store, so the timer, the eviction, the budget and the replace-the-pixels-under-the-same-key
trick are all the ones already there. The frames are taken, not composed: `libnsgif` has already
applied disposal and transparency, so each one is the complete canvas and compositing it over the
first would show the first through anything transparent in a later one. An animation loops for as
long as it is on the screen: `libkimg` does not return the file's loop count.

A sixel is handed to the decoder with its introducer put back on. `libkvt` consumes the DCS final
`q` as a state transition and passes the parameters separately, and a sixel decoder leaves its own
DCS state on `q` and on nothing else — so a body passed on alone is skipped to the terminator and
decodes as a one-pixel image with no error anywhere. The frame is rebuilt here because this is
where the parameters are; `libkimg` never sees them. A body that reaches this code without an
introducer is framed the same way, so a caller that has only the body is not required to know this.

Nothing here parses an image format. Base64 and the `key=value` control blocks are transport and
are bounded here; the moment a byte could be part of a picture it goes to `libkimg`.

### Pictures through tmux

`tmux` will not pass an APC through, and the Unicode placeholders are the way around it. A program
inside one transmits the picture with `a=t` — or `a=T,U=1` — and then writes a run of `U+10EEEE`
cells where it wants it drawn, carrying the picture's id in each cell's foreground colour. Those
cells are resolved to the picture's tiles after the frame is rendered and before it is drawn, so
the picture scrolls, clears and lands in the scrollback with the text around it, exactly as a
placed one does.

The tiles are cut on the transmit, not only on a placement. An id whose picture had never been
placed at a cursor would otherwise name a picture with no tiles, and every cell of the run would be
blank — the protocol would be a picture transmitted and never drawn. `U=1` also stops the placement
at the cursor: without reading it, `a=T,U=1` would stamp the picture where the cursor was and also
answer the client's own run, drawing it twice.

The row and column are read from the run. The protocol also allows combining diacritics after each
placeholder to state them outright; this reads the position instead, which is what the
specification says a terminal does when they are absent and is what the programs that use this
emit. A picture drawn out of order, or split across two places on one screen, would need the
diacritic table — three hundred codepoints for a case nothing here produces.

Only a truecolor foreground names an id. `38;2;r;g;b` reaches a cell intact; `38;5;<n>` does not,
because the state machine resolves an indexed colour to the xterm cube's RGB and throws the index
away, and what survives into a cell's slot is one of the theme's eight. Those would collide with
the small ids clients actually use and answer a default-coloured placeholder with somebody else's
picture. A placeholder with no literal colour is drawn as a space.

### Animation

A frame is a picture with a delay after it, which is how the protocol describes one and is all this
holds. `a=f` transmits one — whole, or a rectangle composed onto an earlier frame at an offset —
and `a=a` runs it, stops it, or sets how many times round.

A frame that changes no cell still has to reach the screen. The sprite table's put counter for a
slot is what moves, not the pointer: the evictor frees the previous frame at the moment the next is
registered and the allocator hands the same block straight back. A sprite cell encodes the slot,
not the picture, so a diff over the cells alone would drop every frame of an animation after the
first.

A frame replaces the picture under the same sprite key, so the screen is never rewritten. The cells
naming those slots go on naming them and only the pixels behind them change. An animation therefore
costs no damage in the cell grid at all, needs no extra slots however many frames it has, and
scrolls, clears and reaches the scrollback exactly as a still picture does.

The budget is on the frames, not on the slots. Frames are this program's own memory — the sprite
table only ever holds the one that is showing — so an animation past the cap drops the frames that
do not fit and plays the ones that do, rather than pushing the desktop's own icons out of the
table.

The wait is the picture's, not the loop's. A terminal with nothing moving in it wakes ten times a
second and no more; one with an animation playing wakes when its next frame is due.

### When pictures cannot be drawn

A build without `libkimg` leaves the three protocols off entirely rather than parsing them and
dropping the result. `images = no` is not the same thing: there the delimiter still runs, under a
4 KB cap, so that the kitty query can be refused. A refusal is instant and is what a program's
fallback path is written for, where silence costs it a timeout. Nothing on that path is decoded,
and a body over the cap is dropped before it reaches the screen.

Where pixels cannot be drawn at all — a virtual terminal, a build without `libkimg`, a `--dump` —
every cell of a picture carries a fallback shade. Something rather than nothing: a photograph that
rendered as blank cells is indistinguishable from output that never arrived.

## Configuration

`~/.config/kdos/term.conf` is `key = value`, the same shape as `panel.conf` and `res.conf`. An
unknown key is reported by name. Every key has a working default and the file need not exist.

| Key | Default | Does |
|---|---|---|
| `shell` | `$SHELL`, then `/bin/sh` | What an argument-less `kdos-term` runs |
| `font` | The toolkit's | A fontconfig name. The size the window opens at, which `Ctrl+=` steps from and `Ctrl+0` returns to |
| `columns` | `80` | Columns asked for on the first configure |
| `rows` | `24` | Rows asked for on the first configure |
| `scrollback` | `2000` | Lines kept above the screen |
| `images` | `yes` | Whether pictures are decoded at all |
| `image_max` | `1024` | The cap on one image payload, in kilobytes |
| `image_cells` | `200` | The widest and tallest a picture may be, in cells |
| `paste_guard` | `yes` | Hold back an unbracketed paste carrying a control byte |
| `opacity` | `100` | How much of the window's own background it keeps, per cent, 20 to 100 |

Below 100, `opacity` lets the desktop show through the cells the terminal has not drawn on, and the
ink is never mixed. It applies only under a compositor: on a `--tty` there is nothing behind the
window at all.

`image_max` bounds a payload before anything allocates for it, which matters because a terminal is
reachable by `cat` on a file somebody sent you.

`SIGHUP` re-reads the file and the accent, which is how `kdos theme` retints a running window.

## A child that dies

A program killed before it could tidy up leaves its modes set, and the window outlives it. This
terminal draws one more frame before it goes, showing how its program finished, so
`kvt_term_reset_modes()` runs the moment the death is seen: bracketed paste off, mouse reporting
off, focus reporting off, synchronized output off, and the primary screen back with its cursor
where `DECRST 1049` would have put it. A `vim` killed with `-9` would otherwise leave its own
buffer on the screen, on an alternate screen nothing can scroll back from, with every click still
going to a pipe that has nothing on the other end.

The screen and the scrollback are not touched. The last thing the program printed is the whole
reason the window is still there, so this is not `kvt_vte_reset()` — that one resets the screen as
well, and calling it here would take away what the frame exists to show. There is no kitty keyboard
mode to put back: this terminal does not implement that protocol, so a child cannot have pushed
one.

## Coverage

The self-test drives the terminal on every host: a command and its output, colour, an attribute,
cursor addressing, and — where the image decoders exist — a sixel, checked for the shape it left on
the grid and for where it put the cursor afterwards.

It has not been through a rig pass against the catalogue's full-screen programs. Until it has,
`foot` remains the default and this is the second terminal on the menu.

## See also

- [kdos-comp](kdos-comp.md) — the compositor it is a window under
- [The C libraries](../05-developer/c-libraries.md) — `libkvt`, `libkimg` and the sprite table
- [Configuration](../06-reference/configuration.md) — `term.conf` beside every other key file
- [Known gaps](../06-reference/known-gaps.md) — what a picture still cannot do
