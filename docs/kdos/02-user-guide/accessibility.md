# Accessibility

This page is for anyone who relies on a screen reader, braille, speech or magnification, and for
anyone setting up a KDOS machine for such a person. It states plainly what accessibility support
exists and what does not, so nobody spends an afternoon looking for a setting that is not there.

The short answer:

- **Nothing reads the KDOS desktop aloud or to a braille display.** The panel, the menus, the
  settings and every other KDOS window are invisible to a screen reader.
- **At a text console, BRLTTY works** — braille and speech for the login prompt, a shell and the
  installer. See [Braille and speech at a text console](#braille-and-speech-at-a-text-console).
- **The compositor has a screen magnifier**, but no key is bound to it until you add one. See
  [What does exist: the magnifier](#what-does-exist-the-magnifier).
- **Text size is adjustable** for everything KDOS draws.
- **A containerised application can use its own toolkit's accessibility support**, which is off by
  default and one file away. See [A containerised application can be read](#a-containerised-application-can-be-read).

## The desktop itself is not read

A screen reader works from a tree of *accessible objects* that an application's toolkit publishes
— on Linux, through the AT-SPI accessibility bus — describing each control: what it is, what it is
called and what it is set to. KDOS builds no such tree. Every KDOS window draws its own grid of
character cells and hands the compositor an ordinary picture of it, and the compositor's own window
decorations, root menu and window switcher are drawn the same way. Nothing in either path tells a
reader what a control is.

Half of what a reader would need does exist. The toolkit KDOS's own windows are built on,
`libktui`, keeps a record of what each control would announce, through `ktui_announce()`. Ten kinds
of control fill it in — buttons, check boxes, radio buttons, text inputs, lists, tables, tabs,
choices, sliders and text areas. Each states its kind, its name, its value and its position in its
set, for example "check box Night light, on" or "tab Keys, 2 of 3". It does so whether the control
was reached by keyboard or by mouse. Menus count only the rows the cursor can land on, so the
position a reader would hear matches the rows a person can actually reach.

That record is rebuilt at the start of every frame, and nothing on the system reads it except the
library's own self-test. What is missing is a program that reads it, and a way for that program to
reach it. See [What would have to change](#what-would-have-to-change).

## Braille and speech at a text console

`brltty`, `espeak-ng` and `speech-dispatcher` are installed on the image. No KDOS window talks to
any of them, but they work at a text console.

**Starting BRLTTY.** BRLTTY runs as a system service, `65_brltty`, which starts at boot once
`/etc/brltty.conf` exists and is not empty. Until you write that file — naming your braille
display's driver, or a speech driver — the service skips itself. BRLTTY reads the text console
through `/dev/vcsa`, so it covers `tty1`, `tty2` and the installer.

**Speech.** BRLTTY's voice is `espeak-ng`, playing straight to the sound card, because no audio
server runs at boot. The sound card accepts one user at a time: while a desktop session's audio
server holds it, the console voice cannot speak, and while the console voice is speaking, the
desktop has no sound. Braille output does not use the sound card and is not affected.

**Contracted braille.** BRLTTY is built with `liblouis`, so contraction tables for well over a
hundred languages are available from `/usr/share/liblouis/tables`. Name a liblouis table with a
`louis:` prefix — for Unified English Braille grade 2:

```
contraction-table louis:en-ueb-g2.ctb
```

in `/etc/brltty.conf`, or `-c louis:en-ueb-g2.ctb` on the command line. A table name without the
prefix is one of BRLTTY's own.

**Programs that talk to BRLTTY.** A terminal program can reach a running BRLTTY through BrlAPI —
`brltty-clip` is one — if its user is in the `brlapi` group. The desktop account is added to that
group when BRLTTY is installed. BrlAPI admits clients through a polkit rule that grants the
`brlapi` group directly. There is no `/etc/brlapi.key`: a key generated when the image is built
would be the same secret on every machine installed from it.

The application catalogue carries no screen reader, BRLTTY or espeak-ng of its own; the ones
installed on the host are the only copies.

## What does exist: the magnifier

The compositor, `kdos-comp`, can magnify the screen. Three actions drive it:

| Action | Effect |
|---|---|
| `ToggleMagnify` | Turn the magnifier on or off |
| `ZoomIn` | Magnify more |
| `ZoomOut` | Magnify less |

**No key is bound to any of them** in the shipped configuration, so on a fresh install the
magnifier exists but nothing reaches it. Add key bindings of your own to
`~/.config/kdos-comp/rc.xml`, inside its `<keyboard>` section:

```xml
<keybind key="W-equal"><action name="ToggleMagnify"/></keybind>
<keybind key="W-A-equal"><action name="ZoomIn"/></keybind>
<keybind key="W-A-minus"><action name="ZoomOut"/></keybind>
```

None of these three chords is used by the shipped file. If you pick others, check them against the
bindings the file already has and against [the desktop's keybindings](desktop.md). To load the
edited file, run `kdos-comp -r`, or choose **Reload Configuration** from the menu you get by right-clicking the
desktop.

A `<magnifier>` block in the same file sets how it behaves:

| Setting | Default | Meaning |
|---|---|---|
| `width` | `400` | Width of the magnified inset, in pixels. `-1` magnifies the whole screen |
| `height` | `400` | Height of the inset. `-1` magnifies the whole screen |
| `initScale` | `2.0` | Magnification when it is turned on; at least 1 |
| `increment` | `0.2` | How much `ZoomIn` and `ZoomOut` change it |
| `useFilter` | `yes` | Smooth the magnified pixels |

While the magnifier is on, the phosphor pass — the CRT-style scanline effect — is switched off for
the whole screen, because magnified scanlines are harder to read, not easier. See
[Theming](theming.md#the-phosphor-pass).

## Larger text

Everything KDOS draws is sized by its font, so a larger font is a larger desktop. Two keys in
`~/.config/kdos/comp.conf` set the pixel size:

| Key | Sets | Default |
|---|---|---|
| `chrome_font` | The panel (unless `panel_font` is set), the desktop icons, the dock-app column and notifications | `Terminus:pixelsize=32` |
| `panel_font` | The panel only; empty falls back to `chrome_font` | `Terminus:pixelsize=20` |

`Terminus` is a bitmap font, so choose a size it has: 12, 14, 16, 18, 20, 22, 24, 28 or 32. A size
it does not have comes back as the nearest one it does. For anything larger — a doubled cell on a
4K screen, for example — name the scalable version of the same typeface, which draws at any size:
`Terminus (TTF):pixelsize=64`. Both keys are read when the session starts, so log out and back in
after changing them.

These keys do not reach every KDOS window. The popups the panel opens, and programs you start
yourself such as `kdos-res`, draw at `Terminus:pixelsize=32` unless they are started with
`--font <name>`. `kdos-term` takes its font from `font` in its own `term.conf`, or from `--font`. The window title bars are set separately, by `<theme><font>` in
`~/.config/kdos-comp/rc.xml`, in points.

The Font page of `kdos-style` changes the typeface without touching either size, and the eight
accents include `paper`, a light scheme. See [Theming](theming.md#fonts).

## A containerised application can be read

Inside a box — the container a graphical application from the catalogue runs in — the ordinary
Linux accessibility stack applies. An application in a box has that box's own accessibility bus: its
toolkit publishes its tree of accessible objects there, and a screen reader running in the same box
can read it. The catalogue ships no screen reader, so you would install one into the box yourself
(`kdos-box enter <box>` gives you a shell inside it).

This is off by default. Nothing on the host answers on the accessibility bus, so with it on, every
containerised application would wait at start-up for a service that is never there. Off means two
variables in the box's environment: `NO_AT_BRIDGE=1` and `GTK_A11Y=none`.

To turn it on for every box, create an empty file:

```sh
touch ~/.config/kdos/a11y
```

To turn it on for one launch, set `KDOS_A11Y` in front of the command:

```sh
KDOS_A11Y=1 gimp                         # the command on your PATH
KDOS_A11Y=1 kdos-appbox -b app.gimp run gimp
```

`KDOS_A11Y=0` turns it off for that launch even when the file exists; any other non-empty value
turns it on.

What this gives you is whatever the application's own toolkit offers. It does not reach the panel,
the Start menu, the file chooser or anything else KDOS draws.

## What would have to change

For a screen reader to work on the KDOS desktop, two things would have to be built:

- **Something to read.** The announcement record `libktui` keeps would have to leave the program
  that made it — over a socket, a bus interface, or a bridge to AT-SPI built on that record.
- **Something to read it with**, and a decision about what that reader may do. A reader that could
  also type into other windows would be indistinguishable from a keylogger, so whatever carries the
  announcements has to grant less than the interface a window manager uses.

Neither is built. See [Known gaps](../06-reference/known-gaps.md).

## See also

- [Configuration](../06-reference/configuration.md#configkdosa11y) — the `~/.config/kdos/a11y` file
- [C libraries](../05-developer/c-libraries.md#libktui) — `ktui_announce()`, and what a widget says
- [Known gaps](../06-reference/known-gaps.md) — this, stated as the gap it is
- [The desktop](desktop.md) — the keyboard route to every surface
- [Theming](theming.md) — fonts, accents and the phosphor pass
