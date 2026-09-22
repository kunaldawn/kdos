# Accessibility

This page states what accessibility support exists on a KDOS machine and what does not, so that
nobody spends an afternoon looking for a setting that is not there. The short answer: nothing on
this image reads the desktop aloud. What does exist is a screen magnifier with no key bound to it,
and an opt-in that turns the ordinary Linux stack back on inside a box.

## The desktop itself is not read

`kdos-comp` draws pixels. A screen reader on a pixel desktop works from a tree of accessible
objects that the toolkit publishes — AT-SPI, in the world this borrows from — and KDOS builds no
such tree. Every KDOS surface composes its own grid of cells and hands it to the compositor as an
ordinary Wayland buffer; the compositor's own chrome — titlebars, the root menu, the
window-switcher OSD — is drawn with pango and handed to the screen the same way. Nothing in either
path carries what a control *is*, what it is called, or what it is set to.

Half the material for one exists. `ktui_announce()` is in `libktui` and every interactive widget
calls it — buttons, check boxes, radios, text inputs, lists, tables, tabs, choices, sliders and
static text, ten roles. A control states its kind, its name, its value and its position in its set:
"check box Night light, on", "tab Keys, 2 of 3". The record is rebuilt from scratch at the start of
every frame and read, today, only by the library's own self-test. What is missing is a client, and
a way for a client to reach it.

There is no braille route and no voice from any KDOS surface. `brltty`, `espeak-ng` and
`speech-dispatcher` are ports and are on the image, because they are useful to somebody at a
terminal, but no KDOS surface talks to any of them. The catalogue also carries an `app.a11y` pack
holding Debian's `brltty` and `espeak-ng`, reachable with `kdos-appbox -b app.a11y run espeak-ng`;
it is the same story one container further out.

## What does exist: the magnifier

`kdos-comp` magnifies. Three actions drive it — `ToggleMagnify`, `ZoomIn` and `ZoomOut` — and a
`<magnifier>` block in `~/.config/kdos-comp/rc.xml` sets its `width`, `height`, `initScale`,
`increment` and `useFilter`. The defaults are a 400x400 inset at 2x, stepping by 0.2, filtered;
setting `width` or `height` to `-1` magnifies the whole output instead.

**Nothing in the shipped `rc.xml` binds any of the three**, so on a fresh install the magnifier
exists and no key reaches it. Add a `keybind` of your own:

```xml
<keybind key="W-equal"><action name="ToggleMagnify"/></keybind>
```

The phosphor pass steps aside while the magnifier is on, whole-frame: the two cannot both process
the same buffer, and an accessibility zoom read through scanlines is harder to read rather than
easier. See [Theming](theming.md#the-phosphor-pass).

The other lever that exists is size. `chrome_font` and `panel_font` in `~/.config/kdos/comp.conf`
set the pixel size every KDOS surface draws at, and the Font page of `kdos-style` changes the face
without touching either size. A larger `chrome_font` is a larger desktop, in cells rather than in
scaling. See [Theming](theming.md#fonts).

## A containerised application can be read

Inside a box, the ordinary Linux accessibility stack applies. An application in a box runs against
that box's own accessibility registry, which is a complete AT-SPI world of its own: the toolkit
publishes its tree, and a reader installed in the same box walks it.

It is off by default, because nothing on the host owns `org.a11y.Bus` and the probe for it can only
time out — so every containerised application would pay a start-up delay for a service that is
never there. Off means two variables in the box's environment: `NO_AT_BRIDGE=1` and
`GTK_A11Y=none`.

To turn it on for every box, create an empty file:

```sh
touch ~/.config/kdos/a11y
```

To turn it on for one launch, set the variable in front of whatever you were going to type:

```sh
KDOS_A11Y=1 gimp                     # the shim on your PATH
KDOS_A11Y=1 kdos-appbox -b app.gimp run gimp
```

`KDOS_A11Y=0` is an explicit off and wins over the file.

What that buys you is what the application's own toolkit offers. It does not reach the panel, the
Start menu, the file chooser or anything else KDOS draws.

## What would have to change

Stated so that the size of the job is clear rather than implied:

- A reader needs something to read. The announcement record would have to leave the process that
  composed it — a socket, a bus interface, or an AT-SPI bridge built on the record `libktui`
  already keeps.
- And something to read it with. That means a client, and a decision about what it may do: a reader
  that could type would be a keylogger with a friendly name, so whatever carries the announcements
  has to grant less than a client that places windows does.

Neither is built. See [Known gaps](../06-reference/known-gaps.md).

## See also

- [Configuration](../06-reference/configuration.md#configkdosa11y) — the `~/.config/kdos/a11y` file
- [C libraries](../05-developer/c-libraries.md#libktui) — `ktui_announce()`, and what a widget says
- [Known gaps](../06-reference/known-gaps.md) — this, stated as the gap it is
- [The desktop](desktop.md) — the keyboard route to every surface
