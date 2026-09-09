# kdos-term

The terminal. One binary that is an `xdg-toplevel` window under `kdos-comp` and a cell surface
under `kdos-con`, because both desktops are the same character grid.

It ships **beside** `foot` and changes nothing: `foot` is still the default terminal in `rc.xml`,
`menu.xml`, the `.desktop` files, `favorites` and `tmux.conf`. `foot` is battle-tested and this is
not, which is the trade [`kdos-res`](kdos-res.md) already made beside `btop`.

## What it is made of

| Library | What it contributes |
|---|---|
| [`libkvt`](../05-developer/c-libraries.md) | The VT100–VT520 state machine, a child on a pty, and the screen as `KtuiCell`s |
| `libkdisp` | Which display server, decided once at startup |
| `libkimg` | The one place untrusted image bytes become a picture |
| `libktui` | The cell grid, the sprite table and its budget |
| `libkcell` | The one scale-and-cut from a decoded picture into sprite tiles |

Naming `kwl_impl` is what links the Wayland half in, and naming `kcon_impl` is what makes the same
source a console surface. `libkdisp` picks the first whose probe succeeds, so the console is tried
before Wayland and a program started inside a session finds its own.

It draws **no chrome of its own beyond one frame**. Under the compositor the server-side decoration
is that frame, so the drawn one is suppressed; on a tty and in a dump nothing else draws one and
then it is the only frame there is. Inside it is the terminal, edge to edge — a terminal is a full
grid of text and a plate, a header band or a button row would each cost it a row.

## The escapes are the state machine's

A caller that writes `\x1b[A` for an arrow is wrong the moment a program sends DECCKM, which `less`
does on its first screen. So a key goes in as a **keysym and a modifier set** and `libkvt` decides
what bytes it becomes — application cursor mode, keypad mode and the modifier encoding all live
there, and there is one implementation of them for both this terminal and `kdos-con`'s own windows.

`TERM` is `xterm-256color`, because that is what the state machine implements and what ncurses
already ships an entry for. A private `TERM` breaks the first time somebody types `ssh`.
**`COLORTERM` is `truecolor` beside it**, which is the only way a program learns that the
sixteen-colour entry `TERM` names is an understatement — `libktui`'s own capability probe reads it,
so without it a KDOS surface running inside a KDOS terminal detected 256 colours and drew the theme
approximately.

**The modes a program probes are answered.** `DECRQM` (`CSI ? <mode> $ p`) reports whether a DEC
private mode is on, for application cursor keys, auto wrap, the cursor, every mouse mode, focus
reporting, the alternate screen, bracketed paste and synchronized output. **A mode nothing
implements is answered `0`** — "not recognised", which is what a probing program is built to handle.
Silence is what it is not built to handle: it waits, times out, and draws like a terminal from 1978.
Answering is what makes every later mode safe to add.

**Focus is reported when the child asks for it** (`DECSET 1004`, `CSI I` in and `CSI O` out). An
editor that is not told it lost the focus does not reload a file changed underneath it, so its next
write is over somebody else's work — nearer to data loss than to polish. It is silent until asked,
because a terminal that wrote `CSI I` unasked would put two stray characters into every program that
never requested them. Both desktops report it, from a diff against the previous turn of the loop
rather than an event, because each display server already holds the answer and neither delivers it
as one.

**Synchronized output** (`DECSET 2026`) holds the frame while the child draws it, so a program that
brackets its screen is never seen half-drawn — which on a slow link is the difference between a
frame and a tear. **Under a watchdog of 150 ms**, because a child that sets the mode and dies would
otherwise freeze its window forever and the terminal cannot tell that from a program taking its
time. The rule lives in `libkvt` rather than in each renderer, because two copies of a timeout is
two timeouts.

**Every KDOS surface brackets its own frames the same way**, when the terminal it is running in
answers the probe — `libktui` asks with DECRQM and emits nothing where the answer is "not
recognised". Inside this terminal the answer is yes, so a surface's frame is held here by the path
above; a surface in a window of the console session is bracketed and shown as it arrives, because
that session composes one grid for every window and cannot hold one of them.

**The primary device attributes report sixel.** `chafa`, `img2sixel`, `lsix`, `timg` and
`mpv --vo=sixel` all send `CSI c` and read parameter `4` as sixel support, so a reply without it
made every one of them fall back to half blocks on a terminal that decodes sixel, OSC 1337 *and* the
kitty protocol. The conformance level stays 60: raising it would claim something this state machine
has not been measured against, which is a different promise.

**`CSI ? Pi ; Pa ; Pv S` reports how big a picture may be.** `Pi` is 1 for the colour registers and
2 for the sixel geometry; a read is answered `CSI ? Pi ; 0 ; <values> S` and anything this terminal
cannot do is answered with a failure code rather than with silence. **The geometry reported is the
one actually enforced** — `image_cells` in each direction, never more than the grid has, multiplied
by the cell size — and it is re-stated whenever the window is resized, because half that bound is
the number of columns and rows.

**A set is refused, not ignored.** The geometry is the configuration's and the register count is the
decoder's, so a program that believed it had negotiated a larger picture would send one and have it
clipped. `Pa` 3 is answered `Ps=3`.

**`CSI 16t` reports what one cell is in pixels**, height first. Only the program drawing the glyphs
knows it, and a client that is not told has to guess — a guessed cell is a correctly encoded picture
at the wrong scale, which looks like a broken decoder rather than a missing answer. A terminal
nobody has told answers nothing rather than inventing a number, because a client cannot tell an
invented answer from a real one.

**`CSI Ps S` and `CSI ? … S` differ by the private marker alone**, and the first scrolls the screen.
A terminal that misses the marker answers a capability probe by scrolling: the program learns
nothing, and what it sees is its own output moving.

**`APC G a=q` is answered.** A picture program sends a one-pixel transmission with `a=q` and waits —
`OK` means it may use the protocol, an error code means it must fall back, and **nothing** means it
waits for its own timeout and then draws as though this were a teletype. The id is echoed so a
program with several in flight knows which it is hearing about, and a query stores no image.
**A terminal with `images = no` answers the query too, with `ENOTSUP`**: silence there costs a person
who turned pictures off exactly what not implementing the protocol would.

**The sixteen named colours are reduced to the theme's eight slots by nearest distance**, so
`kdos theme` moves a terminal's red with everything else — on `tty1` the kernel is drawing them out
of the colour map this desktop installed, and a terminal whose red stopped following the theme would
be the one window on the screen wearing somebody else's scheme.

**Everything above them is a literal the program chose exactly**: the 216-colour cube, the greys and
a 24-bit `SGR 38;2`. Reducing those to eight slots is what loses a picture, and no palette names
them, so nothing is lost by keeping them. The cell carries the reduction as well, so a display that
was never sent the literals — a view over a slow link that declined them — draws what it always
drew.

**The underline's shape and colour travel with it.** `SGR 4:0`–`4:5` select none, single, double,
curly, dotted and dashed; `SGR 58` gives the line its own colour and `59` takes it back. A renderer
with one shape draws the plain line, because the attribute means "this word is marked" and a shape
nobody drew is worse than the wrong shape. **They are emitted onward only where 24-bit colour is**:
`4:3` is a sub-parameter, and a terminal old enough to want indexed colour is old enough to drop the
colon and read the pair as `SGR 43` — a green background where a program asked for a wavy line.

**Five styles are drawn and two are dropped.** Bold, underline, inverse, italic (`SGR 3`),
strikethrough (`SGR 9`) and overline (`SGR 53`) each ride a bit of the cell's attribute byte, so
they cost nothing on the wire between a session and a view. `blink` and `dim` are parsed and reach
no bit at all — a blink drawn as bold is a lie about the text, and a terminal that lies about which
words are emphasised is worse than one that shows them plainly.

**The two DEFAULT colours are not reduced; they are slots.** A terminal's default foreground is a
light grey and its default background is black, and reducing both by distance against eight phosphor
greens lands them on the same slot — which draws every character in the colour of the screen behind
it. "Default" means whatever this desktop calls text and background, so it is answered with
`KT_TEXT` and `KT_BG` directly. A colour a program actually asked for is still reduced, including
one that reduces to its own background, because a program writing black on black meant to.

## A program saying it finished

`make && notify-send done` does not work on this image: `libnotify` is not a port and
`notify-send` is not here. **The escape sequences are**, and they cost a parse rather than a
dependency — so a long job says so from inside the terminal it is running in, and the desktop's own
notification daemon shows it like any other toast.

| Sequence | Whose | Carries |
|---|---|---|
| `OSC 9 ; text` | iTerm2's, and the common one | a summary |
| `OSC 777 ; notify ; summary ; body` | urxvt's, which `tmux` forwards | both |
| `OSC 99 ; params ; text` | kitty's | a summary, past a parameter list |

Three spellings because none of them won, and all three are what programs actually emit.

**`libkvt` raises nothing itself.** A toast means a session bus, and that library links nothing and
must not start; the terminal registers a handler and a terminal that sets none drops the escape,
which is right for one with no desktop behind it. **The application name is the window's title**,
so somebody with four builds running can tell which one spoke.

**`OSC 99`'s parameter list is parsed and ignored.** This terminal has no notification identity, no
replacement and no progress, so honouring `d=0` would be claiming a facility that is not here.

## The paste guard

**With bracketed paste on, the child sees a paste as text and decides for itself.** With it off the
bytes go straight to the pty, so a newline in them **executes**: at a plain shell, at an `ssh`
password prompt, inside `read`. That is the one place a terminal can be made to act as the user, and
it stopped being a thought experiment the moment a clipboard could arrive from a forwarded view.

A payload with a newline, a carriage return or any other control byte except tab is held back. A tab
is not held: it is what a person pasting a table has, and it submits nothing.

`kdos-term` asks with a dialog naming how many lines would run. **The console session cannot** — it
does not own the toolkit of the window the paste is going into — so there the first attempt is
refused, the taskbar says why, and pressing the chord again within five seconds means it. A
confirmation nobody can see would be a refusal with no way past it.

`paste_guard = no` in `term.conf` or `con.conf` turns it off, which is what somebody pasting all day
into a program that never enables bracketing will want.

## Selection and the two clipboards

| Gesture | What it does |
|---|---|
| Left drag | Selects, and puts the selection on the **primary** clipboard on release |
| Double click | Selects the word under the pointer |
| Middle click | Pastes the primary selection |
| `Ctrl+Shift+C` | Copies the selection to the **clipboard** |
| `Ctrl+Shift+V` | Pastes the clipboard |
| `Shift+PgUp` / `Shift+PgDn` | Half a screen of scrollback |
| `Ctrl+Shift+Up` / `Ctrl+Shift+Down` | Jumps to the previous or next **prompt** |
| Wheel | Scrollback, or the child's own scrolling when it has asked for the mouse |

A press and a release in the same cell is a **click, and a click selects nothing** — without that
rule every click leaves a one-character selection on the primary clipboard, which is what the next
middle click pastes.

A program that has asked for mouse reports gets the pointer, because it is drawing its own idea of
what is selected and a second selection on top of it belongs to nobody. **Shift is the override**,
as it is in every terminal: it hands the pointer back so text can be taken out of a program that
captured it.

A paste arrives with **newlines already turned into spaces**, by the same filter that stops a paste
pressing Enter in a text field. In a shell that is the same protection, and it is why a multi-line
paste runs as one line rather than as a sequence of commands.

Six chords are claimed and no more, all of them behind `Shift` or `Ctrl+Shift`. Every chord a
terminal eats is a chord no program running inside it can use.

## Hyperlinks

A program marks a run of text with `OSC 8 ; params ; URI` and ends it with the empty form. **The
address is interned and the cell keeps a 16-bit id**, so the text can be scrolled back to and still
be the link it was; `KtuiCell` is not widened for it, because a link is a property of a terminal's
buffer and not of every surface the toolkit draws. Two runs of the same address are **one** link
whether or not the program said so — the `id=` parameter is ignored, so a program reusing an id for
a different address cannot make one run of text point at another's.

**Hover underlines the whole run and `Ctrl`+click follows it.** The run rather than the character:
an address is one thing, and underlining the letter somebody happens to be over says nothing about
where it ends. `Ctrl` rather than a plain click, because a link sitting in a screenful of text must
not be a trap for somebody selecting a word. It opens through `kdos-appbox open`, the MIME route the
portal's `OpenURI` takes, as an argument vector and never a command line.

**Four schemes and nothing else** — `http`, `https`, `file`, `mailto` — and **every byte must be
printable ASCII**. Anything that can write to a terminal can write an OSC: a `cat` of a hostile
file, a program on the other end of an `ssh`. A control byte would reach an argument vector, and a
byte above 126 makes the same address read two ways depending on who decodes it, which is how a
whitelist gets walked around. A refused link is **text**: the characters are drawn and no link is
offered, so there is nothing to be made to follow.

**The table is capped at 128 addresses and is never freed while the terminal lives.** Both halves of
that are the same decision: an id in the scrollback can always be resolved, which is only affordable
because a child emitting a fresh URI per cell cannot grow the table for ever. Past the cap the text
is still text.

The session's own terminal windows do the same thing, through the same library, so a link works in
whichever terminal a person has. What a link **resolves to** on a bare virtual terminal is a
separate question, and it is in [known gaps](../06-reference/known-gaps.md).

## Prompt marks

A shell says where its prompt starts with `OSC 133 ; A` and what the last command exited with using
`OSC 133 ; D ; <status>`. The shipped `/etc/bash.bashrc` emits both from `PROMPT_COMMAND`, **outside
the `starship` branch**: `starship` is a port and is installed, so a version embedded in `PS1` would
be dead on every real login here.

**`Ctrl+Shift+Up` and `Ctrl+Shift+Down` jump between them.** A screen of build output has one prompt
at each end of it, and scrolling by lines to find the last one is what this replaces. The mark is
kept **on the line**, so it survives into the scrollback — which is exactly when it is worth having,
because the interesting prompt has already gone off the top.

**The status lands on the prompt the command was typed at**, not on the line the shell reported it
from; those are several lines apart in any command with output. A prompt with nothing finished yet
carries no status, and that is not the same as zero — "it worked" and "nobody said" are different
things to draw.

**It is drawn on the frame's left border, where there is one.** A terminal has no gutter: every
column belongs to the child, so the only column this program owns is its own border. An undecorated
window and every window in the console session have one; a window the compositor decorates does
not, and gets no dot — the chords still jump. The colour carries the meaning, a bullet in the error
slot for a command that failed and in the accent for one that did not, with a dot where nothing has
finished.

`B` and `C` — the end of the typed line and the start of output — are parsed and **not** kept.
Getting them right needs a `DEBUG` trap that also fires for the prompt's own commands, which emits
"output started" while the shell is still waiting for a key; a mark at the wrong moment is worse
than no mark, and nothing here reads them.

## Pictures

The three protocols — sixel (`DCS`), iTerm2's `OSC 1337` and kitty's `APC G` — are delimited by
`libkvt`, which decodes none of them, and handed here. This program strips the transport encoding,
decides how many **cells** the picture occupies, and writes sprite cells into the screen. The scale
and the cut into tiles are `libkcell`'s, shared with `kdos-peek` — two implementations of it would
be two answers to how a photograph is resampled.

**An animated GIF arrives whole and takes the same frame machinery** the kitty protocol fills a
frame at a time. `libkimg` returns every frame with the delay after it; the first is placed and the
rest go into the store, so the timer, the eviction, the budget and the replace-the-pixels-under-the
-same-key trick are all the ones that were already there. The frames are *taken*, not composed:
`libnsgif` has already applied disposal and transparency, so each one is the complete canvas and
compositing it over the first would show the first through anything transparent in a later one.

**Into the SCREEN, not into an overlay beside it.** That is what makes a picture scroll with its
output, disappear on `clear` and reach the scrollback — three behaviours an overlay would have to
reimplement against a screen already doing all three.

**A sixel is handed to the decoder with its introducer put back on.** `libkvt` consumes the DCS
final `q` as a state transition and passes the parameters separately, and a sixel decoder leaves its
own DCS state on `q` and on nothing else — so a body passed on alone is skipped to the terminator
and decodes as a one-pixel image with no error anywhere. The frame is rebuilt here rather than in
`libkimg`, whose fixtures carry their own introducer and would get two.

Nothing here parses an image format. Base64 and the `key=value` control blocks are transport and
are bounded here; the moment a byte could be part of a picture it goes to `libkimg`.

| Sequence | What is supported |
|---|---|
| `DCS q … ST` | Sixel, at its natural size |
| `OSC 1337;File=…:<base64>` | `inline=1` only, with `width`/`height` in cells, `px` or `%` |
| `APC G a=T/a=t/a=p/a=d` | Transmit, place, delete, the chunked form, `f=100` (any format `libkimg` reads), `f=24` and `f=32` |
| `APC G a=f/a=a` | Animation: a frame with its delay, composed onto an earlier one, and the run, stop and loop-count controls |
| `APC G a=q` | The capability query, answered `OK` with the id echoed — or `ENOTSUP` where pictures are off |

A build without `libkimg` leaves the three protocols **off entirely** rather than parsing them and
dropping the result. `images = no` is not the same thing: there the delimiter still runs, under a
4 KB cap, so that the kitty query can be *refused* — a refusal is instant and is what a program's
fallback path is written for, where silence costs it a timeout. Nothing on that path is decoded, and
a body over the cap is dropped before it reaches the screen.

Where pixels cannot be drawn — a tty, a view with no pixel library, a `--dump` — every cell of a
picture carries a fallback shade. Something rather than nothing: a photograph that rendered as blank
cells is indistinguishable from output that never arrived. A view that has `libkcell` and no screen
does better than the shade: it matches each cell of the picture to the character whose shape covers
the same part of a cell.

### Animation

A frame is a picture with a delay after it, which is how the protocol describes one and is all this
holds. `a=f` transmits one — whole, or a rectangle composed onto an earlier frame at an offset —
and `a=a` runs it, stops it, or sets how many times round.

**A frame that changes no cell still has to reach the display.** Over the console protocol the
client sends a sprite when the table's put counter for that slot moved — not when the pointer did,
because the evictor frees the previous frame at the moment the next is registered and the allocator
hands the same block straight back. On the display side the cells are byte-identical for the same
reason, so the view forces that frame rather than letting its diff drop it. Both are the same fact
seen from two ends: a sprite cell encodes the slot, not the picture.

**A FRAME REPLACES THE PICTURE UNDER THE SAME SPRITE KEY**, so the screen is never rewritten. The
cells naming those slots go on naming them and only the pixels behind them change. An animation
therefore costs no damage in the cell grid at all, needs no extra slots however many frames it has,
and scrolls, clears and reaches the scrollback exactly as a still picture does.

**The budget is on the frames, not on the slots.** Frames are this program's own memory — the sprite
table only ever holds the one that is showing — so an animation past the cap drops the frames that
do not fit and plays the ones that do, rather than pushing the desktop's own icons out of the table.

**The wait is the picture's, not the loop's.** A terminal with nothing moving in it wakes ten times
a second and no more; one with an animation playing wakes when its next frame is due.

**Over a forwarded socket, every frame is a picture on the wire.** A view is sent a sprite's pixels
when they change, and every frame changes them — so an animation is exactly as expensive as
re-sending the picture at its frame rate. It is worth knowing before pointing a large one down an
`ssh` link.

## Configuration

`~/.config/kdos/term.conf`, `key = value`, the `panel.conf` shape. An unknown key is reported by
name. Every key has a working default and the file need not exist.

| Key | Default | What it does |
|---|---|---|
| `shell` | `$SHELL`, then `/bin/sh` | What an argument-less `kdos-term` runs |
| `font` | the toolkit's | fontconfig name |
| `columns`, `rows` | 80, 24 | The size asked for on the first configure |
| `scrollback` | 2000 | Lines kept above the screen |
| `images` | `yes` | Decode pictures at all |
| `image_max` | 1024 | The cap on one image payload, in kilobytes |
| `image_cells` | 200 | The widest and tallest a picture may be, in cells |

`SIGHUP` re-reads the file and the accent, which is how `kdos theme` retints a running window —
something `foot` cannot do at all.

## Options

```
kdos-term [--title TEXT] [--app-id NAME] [--font NAME] [-D DIR] [--tty]
          [--dump WxH] [-e CMD [args...]]
```

`-e` takes everything after it as the child's argument vector, as it does in every terminal there
has ever been. There is **no shell**: the vector is built by `libkxdg` and executed directly,
because `$SHELL` and `term.conf` are both strings somebody else wrote.

**`--app-id` is the identity, and it is not the title.** A title is the guest's to rewrite the
moment it emits an OSC; the app id is the desktop's, and it is what a taskbar row, a window rule
and a run-or-launch chord all key on. It defaults to `kdos-term`, which is right when the terminal
*is* the application; a terminal running somebody else's program is given that program's name, the
same way `foot --app-id` is. `sh_term_argv_in()` passes both to whichever emulator it named, so a
`Terminal=true` entry opens a window that says what is running in it on either desktop.

`-D`/`--working-directory` enters `DIR` before the fork, so the child and everything it starts
begin there. It is spelled as `foot` spells it on purpose: every caller in `kdos-shell` names the
terminal through one helper and passes the same flags to whichever of the two it named.

`--dump` runs the child to completion, consumes everything it wrote and prints the cells. That is
the whole terminal short of a display, which is why the self-test's goldens are taken through it.

The exit status is the **child's**: a terminal opened to run one command is a wrapper round it.

## What it has been run against

The self-test drives it on every host: a command and its output, colour, an attribute, cursor
addressing, and — where the image decoders exist — a sixel, checked for the shape it left on the
grid and where it put the cursor afterwards.

It has **not** been through a rig pass against the catalogue's full-screen programs. Until it has,
`foot` remains the default and this is the second terminal on the menu.

## See also

- [kdos-con](kdos-con.md) — the console desktop it is a surface on
- [kdos-comp](kdos-comp.md) — the compositor it is a window under
- [The C libraries](../05-developer/c-libraries.md) — `libkvt`, `libkimg` and the sprite table
- [Known gaps](../06-reference/known-gaps.md) — what a picture still cannot do
