# Accessibility

Making the console desktop readable: what says what, how to turn it on, and what the two routes
cost.

**This desktop is the one best placed to be read.** A graphical desktop reconstructs a tree of
accessible objects and hopes it matches what was drawn. The console session holds the literal text
of every cell, knows which window has the focus, and — because every widget announces itself —
knows which control within it. Reading the screen is a loop over a buffer that already exists.

## Two routes, and which to take

| Route | What it needs | What it costs |
|---|---|---|
| `a11y = yes` in `con.conf` | Nothing but `brltty` | The pixel half: no pictures on the screen, no font chords |
| `speak = yes` in `con.conf` | `espeak-ng`, which ships | Nothing; it runs beside the desktop |

They are independent. A braille display and a voice can both be on.

## The braille route

`kdos-view --kms` takes the terminal into graphics mode, and the kernel's text plane goes with it —
which is the plane `brltty` reads over `/dev/vcsa`. A `--tty` view on the same terminal leaves that
plane intact, so a braille display reads the console desktop with nothing else installed and
nothing else running.

```sh
# /etc/kdos/con.conf, or ~/.config/kdos-con/con.conf
a11y = yes
```

`kdos-con-start` then brings the desktop up on a `--tty` view. **It is a trade, not a free
setting**: the pixel half of the display is what pays for it.

## The voice

```sh
speak = yes
```

`kdos-a11y` starts with the session, connects to the reader's socket and says what each widget
announces: what the control is, its name, its value, and where it sits in its set — "check box
Night light, on", "tab Keys, 2 of 3", "list, 3 of 9".

**The position is a fact the widget states**, not a count somebody made from the screen. A list of
nine says nine because the list knows.

**A password field says that it is one and never what is in it.** The whole point of the field is
that what is typed into it is not on the screen; a reader that said it aloud would put it in the
room.

**It says a thing once.** A widget is right on every frame; dropping the repeat is the reader's job.

Run it by hand to hear what a desktop would say without a synthesiser:

```sh
kdos-a11y --print
```

## What a reader may do

**A reader may not type.** It arrives on a third socket — beside the surface socket and the view
socket, in the same private directory — whose clients are displays that cannot drive. That is the
socket's decision and not the client's: reaching it grants less, which is the whole reason it is a
separate path rather than a flag on the other one.

It is sent the composed grid like any display, plus one message per announcement. So a reader
written by somebody else has the same material: the text of the screen and what the desktop says
about it.

## What is not here

**No accessibility registry on the host, and no AT-SPI.** A screen reader running **inside** a box
reaches that box's own registry — `~/.config/kdos/a11y` opts boxed applications into it — and that
is a separate mechanism for a separate problem.

**Nothing reads the graphical desktop.** `kdos-comp` draws pixels and has no cell buffer to walk;
what is written here is the console session's.

## See also

- [Configuration](../06-reference/configuration.md#etckdosconconf) — the `a11y` and `speak` keys
- [kdos-con](../04-programs/kdos-con.md) — the session, its sockets and the reader's
- [C libraries](../05-developer/c-libraries.md#libktui) — `ktui_announce()`, and what a widget says
