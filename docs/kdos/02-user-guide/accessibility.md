# Accessibility

What is here, what is not, and why. **This page is short because the answer is short: nothing on
this image reads the desktop.** It is written so that nobody spends an afternoon looking for a
setting that does not exist.

## The desktop itself is not read

`kdos-comp` draws pixels. A screen reader on a pixel desktop works from a tree of accessible
objects the toolkit publishes — AT-SPI, in the world this borrows from — and **KDOS builds no such
tree**. Every KDOS surface composes its own grid of cells and hands it to the compositor as a
buffer; nothing in that path carries what a control *is*, what it is called or what it is set to.

`ktui_announce()` exists and every widget in `libktui` calls it: a control states its kind, its
name, its value and its position in its set — "check box Night light, on", "tab Keys, 2 of 3". That
record is composed per frame and **read by nothing**. It is the material a reader would need and
half the work of having one; what is missing is a client, and a way for a client to reach it.

There is no braille route and no voice. `brltty`, `espeak-ng` and `speech-dispatcher` are ports and
are on the image, because they are useful to somebody at a terminal — but no KDOS surface talks to
any of them.

## A boxed application can be read

**Inside a box, the ordinary Linux stack applies.** An application in a box runs against that box's
own accessibility registry, which is a complete AT-SPI world of its own: the toolkit publishes its
tree, and a reader installed in the same box walks it.

It is **off by default**, because the host runs no registry and the probe for one always times out —
so every boxed application paid a start-up delay for a service that was never there.

```sh
touch ~/.config/kdos/a11y      # opt every box in; an empty file is enough
KDOS_A11Y=1 kdos-appbox run gimp   # or just this launch
```

What that buys is what the application's own toolkit offers. It does not reach the panel, the Start
menu, the file chooser or anything else KDOS draws.

## What would have to change

Stated so the size of the job is clear rather than implied:

- **A reader needs something to read.** The announcement record would have to leave the process
  that composed it — a socket, a bus interface, or an AT-SPI bridge built on the record libktui
  already keeps.
- **And something to read it with.** A client, and a decision about what it may do: a reader that
  could type would be a keylogger with a friendly name, so whatever carries the announcements has
  to grant less than a client that places windows does.

Neither is built. See [Known gaps](../06-reference/known-gaps.md).

## See also

- [Configuration](../06-reference/configuration.md#configkdosa11y) — the `~/.config/kdos/a11y` file
- [C libraries](../05-developer/c-libraries.md#libktui) — `ktui_announce()`, and what a widget says
- [Known gaps](../06-reference/known-gaps.md) — this, stated as the gap it is
