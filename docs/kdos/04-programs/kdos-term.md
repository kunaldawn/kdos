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

Colours are reduced to the theme's eight slots by nearest distance — one rule for the ANSI sixteen,
the 256 and truecolor alike — so `kdos theme` moves a terminal's colours with everything else.

**The two DEFAULT colours are not reduced; they are slots.** A terminal's default foreground is a
light grey and its default background is black, and reducing both by distance against eight phosphor
greens lands them on the same slot — which draws every character in the colour of the screen behind
it. "Default" means whatever this desktop calls text and background, so it is answered with
`KT_TEXT` and `KT_BG` directly. A colour a program actually asked for is still reduced, including
one that reduces to its own background, because a program writing black on black meant to.

## Selection and the two clipboards

| Gesture | What it does |
|---|---|
| Left drag | Selects, and puts the selection on the **primary** clipboard on release |
| Double click | Selects the word under the pointer |
| Middle click | Pastes the primary selection |
| `Ctrl+Shift+C` | Copies the selection to the **clipboard** |
| `Ctrl+Shift+V` | Pastes the clipboard |
| `Shift+PgUp` / `Shift+PgDn` | Half a screen of scrollback |
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

Four chords are claimed and no more, all of them behind `Ctrl+Shift`. Every chord a terminal eats
is a chord no program running inside it can use.

## Pictures

The three protocols — sixel (`DCS`), iTerm2's `OSC 1337` and kitty's `APC G` — are delimited by
`libkvt`, which decodes none of them, and handed here. This program strips the transport encoding,
decides how many **cells** the picture occupies, scales it to them, and writes sprite cells into the
screen.

**Into the SCREEN, not into an overlay beside it.** That is what makes a picture scroll with its
output, disappear on `clear` and reach the scrollback — three behaviours an overlay would have to
reimplement against a screen already doing all three.

Nothing here parses an image format. Base64 and the `key=value` control blocks are transport and
are bounded here; the moment a byte could be part of a picture it goes to `libkimg`.

| Sequence | What is supported |
|---|---|
| `DCS q … ST` | Sixel, at its natural size |
| `OSC 1337;File=…:<base64>` | `inline=1` only, with `width`/`height` in cells, `px` or `%` |
| `APC G a=T/a=t/a=p/a=d` | Transmit, place, delete, the chunked form, `f=100` (any format `libkimg` reads), `f=24` and `f=32` |
| `APC G a=f/a=a` | Animation: a frame with its delay, composed onto an earlier one, and the run, stop and loop-count controls |

A build without `libkimg` leaves the three protocols **off entirely** rather than parsing them and
dropping the result: with no callback registered `libkvt` ignores a sixel dump exactly as it always
did. Parsing bytes nobody can use is a buffer somebody can fill.

Where pixels cannot be drawn — a tty, a view with no pixel library, a `--dump` — every cell of a
picture carries a fallback shade. Something rather than nothing: a photograph that rendered as blank
cells is indistinguishable from output that never arrived. A view that has `libkcell` and no screen
does better than the shade: it matches each cell of the picture to the character whose shape covers
the same part of a cell.

### Animation

A frame is a picture with a delay after it, which is how the protocol describes one and is all this
holds. `a=f` transmits one — whole, or a rectangle composed onto an earlier frame at an offset —
and `a=a` runs it, stops it, or sets how many times round.

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
kdos-term [--title TEXT] [--font NAME] [-D DIR] [--tty] [--dump WxH] [-e CMD [args...]]
```

`-e` takes everything after it as the child's argument vector, as it does in every terminal there
has ever been. There is **no shell**: the vector is built by `libkxdg` and executed directly,
because `$SHELL` and `term.conf` are both strings somebody else wrote.

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
