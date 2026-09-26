# kdos-term

This page covers `kdos-term`, the KDOS terminal emulator: how to start it, the keys and mouse
gestures it takes, its configuration file, and what it tells the programs running inside it. It is
for anyone who uses the terminal, and for anyone writing a program that draws pictures, links or
notifications into one.

If you only want to use it, read [Synopsis](#synopsis), [Selection and the two
clipboards](#selection-and-the-two-clipboards), [The font, per window](#the-font-per-window) and
[Configuration](#configuration). The sections on the escape layer and on pictures are reference
for program authors.

## What it is, and where it sits

`kdos-term` runs in three ways:

| Mode | How | What you get |
|---|---|---|
| A window | `kdos-term` under `kdos-comp` | An `xdg-toplevel` window on the desktop |
| Inside another terminal | `kdos-term --tty` | The same terminal, drawn in the cells of the terminal you started it from |
| Offscreen | `kdos-term --dump WxH` | The child runs to completion and the final screen is printed as text |

It ships beside `foot` and replaces nothing. `foot` is the default terminal in the shipped
`rc.xml`, `menu.xml`, `favorites` and `tmux.conf` (all under `/etc/skel/.config/`), and the
session's own "open a terminal" helper names `foot`. `kdos-term` is the second terminal on the
menu, listed as **Terminal** by its desktop entry, `/usr/share/applications/kdos-term.desktop`.
`foot` is the default because it has been tested against every full-screen program in the
application [catalogue](../06-reference/glossary.md); `kdos-term` has not been yet. Use `kdos-term`
when you want pictures, links or a font size per window.

A desktop entry can ask for `kdos-term` by name with `X-KDOS-Term=kdos-term`, which is what an
application that draws pictures in the cell grid does. See
[kdos-appbox](kdos-appbox.md) for how launchers use that key.

The binary is `/usr/bin/kdos-term`, built from `src/desktop/kdos-term/`.

## Synopsis

```
kdos-term [--title TEXT] [--app-id NAME] [--font NAME] [-D DIR]
          [--size WxH] [--float] [--tty] [--dump WxH]
          [-e CMD [args...] | -- CMD [args...]]
```

| Option | Effect |
|---|---|
| `-e`, `--exec CMD [args...]` | Run `CMD` instead of the shell. Everything after it is the child's argument vector |
| `--` | The same as `-e`, for a caller that would rather not spell `-e` |
| `--title TEXT` | The window title, until the program sets its own. Defaults to `Terminal` |
| `--app-id NAME` | The identity the desktop files this window under. Defaults to `kdos-term` |
| `-D`, `--working-directory DIR` | Start the child in `DIR` |
| `--size WxH` | Open at `W` columns and `H` rows instead of the size `term.conf` asks for. Ignored below 4x2 |
| `--float` | Open unanchored, at that size. This is what a desktop entry's `X-KDOS-Float` asks for |
| `--font NAME` | A fontconfig name, overriding `term.conf` |
| `--tty` | Draw on the terminal this was started from |
| `--dump WxH` | Run the child to completion offscreen and print the cells. Needs at least 4x2 |
| `-h`, `--help` | Print usage |

Some examples:

```sh
kdos-term                              # your shell, in a window
kdos-term -D ~/src -e make -j8         # run a build in ~/src
kdos-term --title logs -e tail -f /var/log/messages
kdos-term --dump 80x24 -e ls --color   # print what the screen would show
```

`-e` takes everything after it, as it does in other terminals. No shell is involved at any point:
the argument vector is built by `libkxdg` and executed directly. That holds for the `shell` key and
`$SHELL` too, which are split the way a desktop entry's `Exec` line is, because both are strings
someone else wrote.

`--app-id` is the window's identity and is not the same as its title. A program can rewrite the
title at any moment with an escape sequence; the app id belongs to the desktop, and taskbar rows,
window rules and run-or-raise chords all match on it. When the shell starts a `Terminal=true`
desktop entry in `kdos-term`, it passes the program's name as both `--title` and `--app-id`, so the
window says what is running in it. `foot` is given the same name through its own `--app-id`.

`-D` is spelled the way `foot` spells it, so a caller can pass the same flags to either terminal.

**Exit status.** `kdos-term` exits with its child's status, so a terminal opened to run one command
reports that command's result. A command that cannot be found exits 127, as a shell reports it, and
the window shows that rather than closing blank. A `--dump` run exits 0. Errors of its own exit 1:
an unknown option, a bad `--dump` size, no display (the message suggests `--tty`), or no
pseudo-terminal.

## What it is built from

| Library | Contributes |
|---|---|
| [`libkvt`](../05-developer/c-libraries.md) | The VT100–VT520 state machine, the child on a pseudo-terminal, and the screen as cells |
| `libkdisp` | Which display server to use, decided once at startup |
| `libkwl` | The Wayland window: the only display implementation `kdos-term` links |
| `libktui` | The cell grid, the sprite table and its budget, dialogs |
| `libkcell` | The font painter, and the one scale-and-cut from a decoded picture into sprite tiles |
| `libkimg` | The one place untrusted image bytes become a picture |
| `libkxdg` | Splitting `shell` and `$SHELL` into an argument vector |
| `libkcolor` | The accent palettes |
| `libkbase` | Files, processes, notifications |

A `--tty` or `--dump` run opens no display at all.

The terminal draws no chrome beyond a single frame. Under the compositor the server-side decoration
is that frame, so the terminal draws none of its own. On a virtual terminal, inside another
terminal, and in a dump, nothing else draws one, so `kdos-term` draws a box with the title in it.
Inside the frame the grid runs edge to edge: a header or a button row would cost the terminal a row
of text.

## What the child sees

Every child starts with this environment:

| Variable | Value | Why |
|---|---|---|
| `TERM` | `xterm-256color` | What the state machine implements and what ncurses already ships an entry for. A private `TERM` breaks the first time somebody types `ssh` |
| `COLORTERM` | `truecolor` | The only way a program learns that 24-bit colour works. `libktui` reads it, so a KDOS program running inside `kdos-term` draws the theme exactly |
| `TERM_PROGRAM` | `kdos-term` | Picture programs read it as a last resort when the escape-sequence probes are unavailable, for example over a pipe |
| `TERM_PROGRAM_VERSION` | The package version | |
| `COLUMNS`, `LINES` | Removed | A stale size inherited from the parent would override the real one |

The shipped `/etc/bash.bashrc` reads `TERM_PROGRAM` to set `GNUTERM=sixelgd`, so gnuplot draws
sixel pictures in `kdos-term`.

## The escape layer

This section is reference for program authors: what `kdos-term` answers, and in what form.

### Keys go in as keysyms

A key enters as a keysym plus a modifier set, and `libkvt` decides what bytes it becomes.
Application cursor mode (`DECCKM`, which `less` sets on its first screen), keypad mode and the
modifier encoding all live there, so there is one implementation for every program built on the
state machine. A key press also ends a scrollback view and returns to the live screen, so you never
type into a line you cannot see.

### Mode queries are answered in the form they were asked

`DECRQM` has two spellings, and the reply carries the marker of the request:

| Request | Asks after | Reply | Modes answered |
|---|---|---|---|
| `CSI ? <mode> $ p` | A DEC private mode | `CSI ? <mode> ; <value> $ y` | Application cursor keys, auto wrap, the cursor, every mouse mode, focus reporting, the alternate screen, bracketed paste, synchronized output |
| `CSI <mode> $ p` | An ANSI mode | `CSI <mode> ; <value> $ y` | `2` KAM, `4` IRM, `12` SRM, `20` LNM |

Mode `4` means different things in the two spellings, so the marker matters. A mode nothing
implements is answered `0`, "not recognised". A program that probes is built to handle that; what it
cannot handle is silence, which makes it wait for a timeout and fall back to the most basic output.

### Window titles and colour queries

| Sequence | Effect |
|---|---|
| `OSC 0 ; text`, `OSC 2 ; text` | Set the window title. It reaches the compositor's frame, or the frame `kdos-term` draws itself |
| `OSC 4 ; n ; ?` | Report palette entry `n`. Setting the palette is not supported |
| `OSC 10 ; ?`, `OSC 11 ; ?` | Report the default foreground and background |

Replies end with the same terminator the query used (`BEL` or `ST`).

### Focus, and holding a frame

**Focus reporting** (`DECSET 1004`): when the child asks, `kdos-term` sends `CSI I` when the window
gains focus and `CSI O` when it loses it. An editor that is not told it lost focus does not reload a
file changed underneath it, and its next write goes over someone else's work. Nothing is sent until
a program asks, because unrequested `CSI I` would appear as two stray characters in programs that
never enabled it.

**Synchronized output** (`DECSET 2026`): the terminal holds the frame while the child draws it, so a
program that brackets its screen is never seen half-drawn. A 150 ms watchdog releases the hold,
because a child that set the mode and then died would otherwise freeze its window forever. The rule
lives in `libkvt`, so every renderer has the same timeout.

Every KDOS program brackets its own frames the same way when the terminal it runs in says it
supports this. `libktui` asks with `DECRQM` and sends nothing where the answer is "not recognised";
inside `kdos-term` the answer is yes.

### Picture capabilities

**Device attributes.** The primary device attributes reply is `CSI ? 60 ; 1 ; 4 ; 6 ; 9 ; 15 c`.
Parameter `4` means sixel: `chafa`, `img2sixel`, `lsix`, `timg` and `mpv --vo=sixel` all send
`CSI c` and look for it, and fall back to half blocks without it. The conformance level stays 60,
because a higher level would claim behaviour this state machine has not been measured against.

**Graphics limits** (`CSI ? Pi ; Pa ; Pv S`). `Pi` is 1 for the colour registers and 2 for the sixel
geometry. A read is answered `CSI ? Pi ; 0 ; <values> S`. The geometry reported is the one actually
enforced: `image_cells` in each direction, never more than the grid has, multiplied by the cell size
in pixels. It is recalculated whenever the window is resized. A request to *set* either value is
refused with a failure code (`Pa` 3 is answered `Ps=3`) rather than ignored, because a program that
believed it had negotiated a larger picture would send one and have it clipped. Anything the
terminal cannot do is answered with a failure code, never with silence.

`CSI Ps S` and `CSI ? … S` differ only by the `?`, and the first one scrolls the screen. A terminal
that misses the marker answers a capability probe by scrolling.

**Cell size** (`CSI 16t`). The reply gives one cell's size in pixels, height first. Only the program
drawing the glyphs knows it; a client that has to guess draws a correctly decoded picture at the
wrong scale. A terminal with no display, such as a dump, answers nothing rather than inventing a
number.

**Kitty query** (`APC G a=q`). A picture program sends a one-pixel transmission with `a=q` and waits
for `OK` or an error code. `kdos-term` answers with the program's id echoed, and a query stores no
image. With `images = no` it still answers, with `ENOTSUP`, so the program falls back at once instead
of waiting for its own timeout.

### Colour and attributes

**The sixteen named colours** are mapped to the theme's eight colour slots by nearest distance, so
`kdos theme` moves a terminal's red along with everything else on the screen. On `tty1` the kernel
already draws them from the palette the desktop installed.

**Everything above those sixteen** is kept exactly: the 216-colour cube, the grey ramp, and 24-bit
`SGR 38;2`. Mapping those to eight slots would ruin a picture, and no palette names them. The cell
also carries the slot mapping, so a renderer that draws only the eight slots still draws something
sensible.

**The default colours** are slots, not reductions. A terminal's default foreground is a light grey
and its default background is black, and mapping both by distance against eight shades of phosphor
green lands them on the same slot, which would draw text in the colour of the screen behind it. So
the default foreground and background are the theme's text and background slots (`KT_TEXT` and
`KT_BG`). A colour a program asked for is still mapped, including one that maps to the background,
because a program writing black on black meant to.

**Underline styles.** `SGR 4:0` to `4:5` select none, single, double, curly, dotted and dashed.
`SGR 58` gives the line its own colour and `SGR 59` removes it. A renderer that has one underline
shape draws the plain line. When a KDOS program forwards these to another terminal, it does so only
where 24-bit colour is available: an older terminal drops the colon, reads `4:3` as `SGR 43`, and
draws a green background.

**Text styles.** Six are drawn: bold (`SGR 1`), underline, inverse, italic (`SGR 3`), strikethrough
(`SGR 9`) and overline (`SGR 53`). Each is one bit in the cell's attribute field.

- Bold uses a bold companion face when the loaded font has one whose cell size matches, and
  otherwise strikes the upright glyph twice.
- Italic uses an italic companion face when there is one, and otherwise shears the upright glyph by
  7/32, a little over twelve degrees, about its vertical middle.
- `blink` (`SGR 5`) and `dim` (`SGR 2`) are parsed and not drawn. Drawing blink as bold would
  misstate which words are emphasised.

## Notifications from a running program

`make && notify-send done` works on the desktop itself. These escape sequences do the same from
places `notify-send` cannot reach, such as a shell on another machine over `ssh`, and from any
program that can print:

| Sequence | Origin | Carries |
|---|---|---|
| `OSC 9 ; text` | iTerm2, the most common | A summary |
| `OSC 777 ; notify ; summary ; body` | urxvt, and `tmux` forwards it | Summary and body |
| `OSC 99 ; params ; text` | kitty | A summary, after a parameter list |

For example, from any shell inside `kdos-term`:

```sh
make; printf '\033]9;build finished\007'
```

The desktop's notification daemon shows it like any other toast, with the application name
`kdos-term`. The toast is raised over the session bus with `gdbus`, in the background, so the
terminal never stops drawing to wait for it.

`OSC 99`'s parameter list is read past and ignored: this terminal has no notification ids,
replacement or progress, so honouring `d=0` would claim something that is not there.

`libkvt` raises nothing itself; the terminal registers a handler. A program built on `libkvt` that
sets no handler drops these sequences.

## Pasting, and the paste guard

**Every paste arrives as one line.** A paste from the clipboard or the primary selection passes
through `libktui`'s paste filter before `kdos-term` sees it, whether or not the program in the
terminal has bracketed paste on:

- Each newline becomes a space.
- Tab, carriage return, DEL and every other control byte are removed.
- A paste is capped at 4 KB (4095 bytes). Anything longer is cut at the last whole character, and
  the rest is lost without a message.

So a multi-line paste into a shell, `vim` or any other editor lands flattened onto one line, and
pasting a long file gives you only its first 4 KB.

The filter exists because of what an unbracketed paste would otherwise do. With bracketed paste on,
the child receives a paste marked as a paste and decides what to do with it. With bracketed paste
off, the bytes go straight to the pseudo-terminal, and a newline in them runs whatever came before
it: at a shell prompt, at an `ssh` password prompt, inside `read`. That is the one way a terminal can
be made to act as its user, and a paste that holds no newline cannot do it.

**The guard** is a second check behind the filter. If bracketed paste is off and the text still
contains a control byte other than tab, `kdos-term` holds it back and shows a dialog: "*N* lines
would run as if typed", with **Paste** and **Cancel**. The dialog owns the keyboard while it is up,
so the answer cannot be typed into the program the paste would run in. No paste made with the
keyboard or mouse reaches the guard with a control byte left in it, so in practice the dialog never
appears. `paste_guard = no` in `term.conf` turns the guard off; the filter stays on either way. The
key is also listed in [Configuration](../06-reference/configuration.md).

## Selection and the two clipboards

| Gesture | Does |
|---|---|
| Left drag | Selects, and puts the selection on the primary selection on release |
| Double click | Selects the word under the pointer; nothing on blank or unwritten space |
| Middle click | Pastes the primary selection |
| `Ctrl+Shift+C` | Copies the selection to the clipboard |
| `Ctrl+Shift+V` | Pastes the clipboard |
| `Shift+PgUp`, `Shift+PgDn` | Scroll back or forward half a screen |
| `Ctrl+Shift+Up`, `Ctrl+Shift+Down` | Jump to the previous or next prompt (see [Prompt marks](#prompt-marks)) |
| Wheel | Scroll three lines, or pass the wheel to the child when it has asked for the mouse |
| `Ctrl`+click on a link | Open the link (see [Hyperlinks](#hyperlinks)) |

A press and release in the same cell is a click, and a click selects nothing. Without that rule
every click would leave a one-character selection for the next middle click to paste.

A program that has asked for mouse reports gets the pointer, because it draws its own selection.
Hold `Shift` to take the pointer back and select text out of such a program, as in other terminals.

**Programs can copy too.** `OSC 52 ; <targets> ; <base64>` puts text on the clipboard, or on the
primary selection when the targets include `p`. The read form, `OSC 52 ; … ; ?`, is refused: it would
let anything that can write to the terminal read whatever was last copied anywhere on the desktop,
and there is no setting to allow it.

**Chords the terminal keeps.** Nine are claimed and no more: the six above behind `Shift` or
`Ctrl+Shift`, and the three font chords below behind a bare `Ctrl`. Every chord a terminal claims is
one no program inside it can use, which is why the prefix is `Ctrl+Shift`. The font chords can use a
bare `Ctrl` because `Ctrl+=`, `Ctrl+-` and `Ctrl+0` produce no control code, so a program could not
tell them from the plain key anyway. Shift must be up for those three: leaving `Ctrl+Shift` alone
keeps `Ctrl+_` reaching the child.

## The font, per window

Each `kdos-term` window is its own process, so each window has its own font size. Two terminals side
by side can sit at two sizes.

| Chord | Does |
|---|---|
| `Ctrl+=` | One step larger |
| `Ctrl+-` | One step smaller |
| `Ctrl+0` | Back to the size the window opened at |

A step changes the size in the fontconfig name by one:

| The name carries | Range |
|---|---|
| `:pixelsize=N` | 8 to 72 |
| `:size=N` | 5 to 48 |
| No size | `:size=11` is appended and the step counts from there, keeping the face you chose |

A scalable face changes on every step. A bitmap face answers with the nearest size it has, so a step
that lands between two of its sizes changes nothing on screen until a step reaches the next one.

A different cell size means a different number of columns and rows, so the grid is recut and the
child is told its new size, exactly as when the window is resized. A picture already on the screen
goes blank until the program sends it again, because every tile was scaled to the old cell.

In a `--tty` run these chords are passed to the child. The font there belongs to the outer terminal.

## Hyperlinks

A program marks text as a link with `OSC 8 ; params ; URI` and ends it with `OSC 8 ; ;`. Moving the
pointer over a link underlines the whole run of text; `Ctrl`+click opens it. A plain click does not,
so selecting a word inside a link never opens anything by accident.

A link opens through `kdos-appbox open`, the same route the portal's `OpenURI` takes, as an argument
vector and never a command line. The address resolves to the handler for its type, or for
`x-scheme-handler/<scheme>`.

The rules, each of which exists because anything that can write to a terminal can write an `OSC 8`
(a `cat` of a hostile file, a program on the far end of `ssh`):

- Only four schemes are accepted: `http`, `https`, `file` and `mailto`.
- Every byte must be printable ASCII, and the address must be shorter than 2048 bytes. A control
  byte would reach an argument vector, and a byte above 126 can make one address read two ways.
- A refused link is drawn as plain text, with nothing to click.
- The `id=` parameter is ignored. Links are keyed by address, so two runs of the same address are
  one link, and a program cannot reuse an id to make one run of text point somewhere else.
- The table holds 128 addresses and is kept for the life of the terminal, so a link in the
  scrollback always resolves. Past 128, new links are plain text.

The link id lives in the terminal's own buffer, not in the toolkit's cell type, so it costs nothing
for programs that are not terminals. The console session's terminal windows use the same library and
behave the same way. What a link opens on a bare virtual terminal is covered in
[known gaps](../06-reference/known-gaps.md).

## Prompt marks

A shell can report where each prompt starts with `OSC 133 ; A`, and the exit status of the last
command with `OSC 133 ; D ; <status>`. The shipped `/etc/bash.bashrc` emits both from
`PROMPT_COMMAND`, so the marks are there whether the plain prompt or `starship` draws it.

`Ctrl+Shift+Up` and `Ctrl+Shift+Down` jump between marked prompts, including ones that have scrolled
into the scrollback. That is the quick way back to the start of a long command's output. With no
marks, the chords do nothing.

The status is attached to the prompt the command was typed at, not the line where the shell reported
it. A prompt whose command has not finished carries no status, which is different from a status of
zero.

When `kdos-term` draws its own frame (on a virtual terminal, inside another terminal, in a dump), each
marked prompt gets a sign on the frame's left border:

| Sign | Meaning |
|---|---|
| Bullet in the error colour | The command failed |
| Bullet in the accent colour | The command succeeded |
| Dim dot | Nothing has finished at this prompt yet |

Under the compositor the frame is the compositor's, so there is no border column to draw on and the
signs are not shown; the chords still work.

`OSC 133 ; B` and `C` (end of the typed line, start of output) are accepted and ignored.

## Pictures

`kdos-term` shows pictures sent with three protocols: sixel, iTerm2's inline images and the kitty
graphics protocol.

| Sequence | What is supported |
|---|---|
| `DCS q … ST` | Sixel, at its natural size |
| `OSC 1337 ; File=… : <base64>` | `inline=1` only, with `width` and `height` in cells, in `px`, or in `%` |
| `APC G a=T`, `a=t`, `a=p`, `a=d` | Transmit, transmit and place, place, delete; the chunked form; `f=100` (any format `libkimg` reads), `f=24` and `f=32` |
| `APC G a=f`, `a=a` | Animation: a frame with its delay, optionally composed onto an earlier frame, and run, stop and loop-count controls |
| `APC G a=q` | The capability query: `OK` with the id echoed, or `ENOTSUP` where pictures are off |
| `U+10EEEE` cells | Unicode placeholders, which is how a picture reaches this terminal through `tmux` |

For example, `chafa photo.jpg`, `img2sixel photo.png` and `timg photo.webp` all draw in `kdos-term`.

Pictures are written into the screen's cells, not into an overlay. That is why a picture scrolls with
its output, disappears on `clear` and is kept in the scrollback.

The work is divided: `libkvt` finds where each picture sequence starts and ends and decodes nothing;
`kdos-term` removes the transport encoding (base64 and the `key=value` control blocks, both bounded),
decides how many cells the picture covers, and writes sprite cells into the screen; `libkimg` does
all image decoding; `libkcell` scales and cuts the picture into tiles, the same code `kdos-peek` uses.
Nothing in `kdos-term` parses an image format.

An animated GIF sent inline uses the same frame machinery as a kitty animation. `libkimg` returns
every frame, already composed, with its delay; the first is placed and the rest are stored. An
animation loops for as long as it is on the screen, because `libkimg` does not return the file's loop
count.

A sixel is handed to the decoder with its `DCS … q` introducer rebuilt, because `libkvt` consumes the
introducer and passes the parameters separately, and a sixel body without it decodes as a one-pixel
image with no error.

### Pictures through tmux

`tmux` does not pass the kitty protocol's `APC` sequences through. A program inside `tmux` instead
transmits the picture with `a=t` (or `a=T,U=1`), then writes a run of `U+10EEEE` cells where the
picture should appear, with the picture's id in each cell's foreground colour. `kdos-term` replaces
those cells with the picture's tiles after rendering the screen and before drawing it, so the picture
scrolls, clears and reaches the scrollback like any other.

Details a program author needs:

- Tiles are cut when the picture is transmitted, so an id that was never placed at a cursor still
  draws.
- `U=1` suppresses placement at the cursor, so `a=T,U=1` draws the picture once, where the
  placeholders are.
- Row and column come from the position of each cell in the run. The optional combining diacritics
  that state them explicitly are not read, which is what the specification says a terminal does
  when they are absent. A picture split across two places on one screen is therefore not supported.
- Only a 24-bit foreground (`38;2;r;g;b`) names an id. An indexed colour (`38;5;n`) is resolved to
  RGB and its index lost, so it cannot. A placeholder with no literal colour is drawn as a space.

### Animation

A frame is a picture with a delay. `a=f` transmits one, either whole or as a rectangle composed onto
an earlier frame at an offset, and `a=a` runs, stops or sets the loop count.

A new frame replaces the picture's pixels under the same sprite, so the cells on screen are never
rewritten: an animation costs no extra cells and no extra sprite slots however many frames it has,
and it scrolls, clears and reaches the scrollback like a still picture. The terminal tracks which
slots changed by a counter rather than by pointer, because the allocator often hands the next frame
the same memory as the last.

The frame budget is `kdos-term`'s own memory, not the sprite table. An animation that exceeds it
drops the frames that do not fit and plays the rest, rather than pushing the desktop's icons out of
the sprite table.

An idle terminal wakes ten times a second; one playing an animation wakes when its next frame is due.

### When pictures cannot be drawn

| Situation | What happens |
|---|---|
| A build without `libkimg` | The three protocols are off entirely |
| `images = no` | Picture sequences are still delimited, under a 4 KB cap, so the kitty query can be answered `ENOTSUP`. Nothing is decoded, and a larger body is dropped |
| No pixels to draw with: a virtual terminal, a `--dump` | Each cell of a picture shows a fallback shade, so the picture's shape is still visible |

## Configuration

`~/.config/kdos/term.conf` (or `$XDG_CONFIG_HOME/kdos/term.conf`) uses `key = value` lines, the same
shape as `panel.conf` and `res.conf`. Lines starting with `#` are comments. Every key has a working
default and the file need not exist. An unknown key is reported by name on standard error.

| Key | Default | Range | Does |
|---|---|---|---|
| `shell` | `$SHELL`, then `/bin/sh` | | What `kdos-term` runs when no command is given. Split like a desktop entry's `Exec`; no shell is involved |
| `font` | `Terminus:pixelsize=32` under the compositor | | A fontconfig name, and the size a window opens at. `Ctrl+=` steps from it and `Ctrl+0` returns to it |
| `columns` | `80` | 20–1000 | Columns asked for when the window opens |
| `rows` | `24` | 4–1000 | Rows asked for when the window opens |
| `scrollback` | `2000` | 0–200000 | Lines kept above the screen |
| `images` | `yes` | | Whether pictures are decoded at all |
| `image_max` | `1024` | 4–65536 | The largest single picture payload, in kilobytes |
| `image_cells` | `200` | 1–1000 | The widest and tallest a picture may be, in cells |
| `paste_guard` | `yes` | | The second check behind the paste filter: hold back an unbracketed paste that still contains a control byte. The filter removes every such byte first, so no keyboard or mouse paste reaches it; see [Pasting, and the paste guard](#pasting-and-the-paste-guard) |
| `opacity` | `100` | 20–100 | How much of the window's own background it keeps, in per cent |

A boolean is on for `yes`, `1`, `true` or `on`, and off for anything else. A number outside its range
is clamped to the nearest end.

A small example:

```
font = Terminus:pixelsize=24
scrollback = 10000
opacity = 90
```

Below 100, `opacity` lets the desktop show through the cells the terminal has not drawn on; text is
never blended. It applies only under a compositor.

`image_max` limits a payload before anything is allocated for it. That matters because a terminal
receives whatever is `cat`-ed into it, including a file someone sent you.

**Changing it while windows are open.** `kdos-settings` writes this file from its Desktop page and
sends `SIGHUP` to every `kdos-term`; `kdos theme` sends the same signal. On `SIGHUP` a window re-reads
the file and the accent:

- The accent, `paste_guard` and `image_cells` apply to the open window at once.
- `shell`, `font`, `columns`, `rows`, `opacity` and `scrollback` are read when a window opens.
- `images` and `image_max` are read when a window opens and again after a font step.

## A child that dies

When the child exits, the window draws one last frame showing how it finished, then closes.

A program killed before it could tidy up leaves its terminal modes set. So before that last frame,
`kvt_term_reset_modes()` turns bracketed paste, mouse reporting, focus reporting and synchronized
output off, and returns to the primary screen with the cursor where `DECRST 1049` would have put it.
Without that, a `vim` killed with `kill -9` would leave its own buffer on the alternate screen, with
the scrollback unreachable behind it.

The screen and the scrollback are left alone: the last thing the program printed is the reason the
final frame exists. There is no kitty keyboard mode to restore, because `kdos-term` does not
implement that protocol.

## Coverage

`testing/selftest.sh` builds `kdos-term` against a stand-in for `libkwl` and runs it with `--dump` on
any host, comparing four golden frames under `testing/goldens/`:

| Golden | Checks |
|---|---|
| `term-hello-44x8` | A command and its output |
| `term-ansi-44x10` | Colour, an attribute and cursor addressing |
| `term-sixel-44x10` | A sixel: the rows it took and where it left the cursor. Skipped on a host without the sixel decoder |
| `term-kitty-uniph-44x10` | A kitty picture drawn through Unicode placeholders. Skipped on a host without the PNG decoder |

Where the Wayland libraries are present, it also builds the real window binary. See
[Testing](../05-developer/testing.md).

## See also

- [kdos-comp](kdos-comp.md) — the compositor it is a window under
- [kdos-appbox](kdos-appbox.md) — launchers, `X-KDOS-Term`, and `kdos-appbox open`
- [The C libraries](../05-developer/c-libraries.md) — `libkvt`, `libkimg` and the sprite table
- [Configuration](../06-reference/configuration.md) — `term.conf` beside every other key file
- [Known gaps](../06-reference/known-gaps.md) — what a picture still cannot do
