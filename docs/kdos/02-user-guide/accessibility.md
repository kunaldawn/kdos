# Accessibility

This chapter is for anyone who relies on a screen reader, braille, speech or magnification, and
for anyone setting up a KDOS machine for such a person. It states what accessibility support KDOS
has and what it lacks, and how to turn on each part that exists. No earlier chapter is needed, but
[Getting started](getting-started.md) explains the text consoles and the login this chapter refers
to, and [The desktop](desktop.md) explains the desktop's keyboard routes.

## What is available

| Need | What KDOS offers | Where |
|---|---|---|
| A screen reader on the desktop | Nothing. The panel, menus, settings and every other KDOS window are invisible to a screen reader | [The desktop itself is not read](#the-desktop-itself-is-not-read) |
| Braille or speech | BRLTTY, at a text console only: the login prompt, a shell on `tty2`, the installer | [Braille and speech at a text console](#braille-and-speech-at-a-text-console) |
| Magnification | The compositor's magnifier, which ships with no key bound to it | [The magnifier](#the-magnifier) |
| Larger text | A font size for the desktop's chrome, per-window sizes in the terminals, a larger pointer | [Larger text](#larger-text) |
| Contrast | Eight colour schemes held to fixed contrast floors, one of them light | [Colour and contrast](#colour-and-contrast) |
| An application read aloud | A boxed application's own toolkit support, off by default and turned on by `~/.config/kdos/a11y` | [A containerised application can be read](#a-containerised-application-can-be-read) |

## The desktop itself is not read

A screen reader works from a tree of *accessible objects* that an application's toolkit publishes,
on Linux through the AT-SPI accessibility bus. Each object describes one control: what kind it is,
what it is called and what it is set to. KDOS builds no such tree. Every KDOS window draws its own
grid of character cells and hands the compositor, `kdos-comp`, an ordinary picture of it. The
compositor's own window decorations, root menu and window switcher are drawn as pixels in the same
way. Nothing on either path tells a reader what a control is, and the host runs no AT-SPI registry
for a reader to ask. The host carries no GTK or Qt, whose toolkits would publish such a tree; see
[How KDOS differs](../01-philosophy/how-kdos-differs.md#toolkits-on-the-host).

`libktui`, the toolkit KDOS's own windows are built on, records what each control would announce,
but nothing reads that record; see [What would have to change](#what-would-have-to-change).

## Braille and speech at a text console

Four packages on the image provide braille and speech.
[The ports catalogue](../06-reference/ports-catalogue.md) lists them with the rest of the ports.

| Port | Version | What it is |
|---|---|---|
| `brltty` | 6.9.1 | The console screen reader: drives a braille display, speaks, or both |
| `liblouis` | 3.39.0 | Braille translation tables, including contracted (grade 2) braille |
| `espeak-ng` | 1.52.0 | A speech synthesiser |
| `speech-dispatcher` | 0.12.1 | The common speech interface screen readers talk to, with `espeak-ng` as its voice |

No KDOS window talks to any of them. They work at a text console, where the screen is a grid of
characters BRLTTY can read directly.

### Which screens BRLTTY reads

BRLTTY reads the Linux text console through `/dev/vcsa`, which always shows the virtual terminal
in the foreground. That covers:

- the login prompt on `tty1` of an installed system, before the desktop starts;
- `tty2`, a plain login prompt on every KDOS system, which you reach from the desktop with
  `Ctrl+Alt+F2` and leave with `Alt+F1`;
- the installer, `kinstall`, when it runs on a text console (see [Installation](installation.md)).

Once the desktop is running on `tty1`, `/dev/vcsa` holds nothing BRLTTY can use. For a spoken or
braille shell on a machine that runs the desktop, log in on `tty2`. Speech there shares the sound
card with the desktop; see [Speech and the sound card](#speech-and-the-sound-card).

The build selects three screen drivers: `lx`, the Linux console, which is the default; `em`, which
reads a terminal run under `brltty-pty`; and `tx`, which reads a `tmux` session. A different one is
chosen with `screen-driver` in `/etc/brltty.conf`. None of them reads anything the compositor
draws.

### Turning BRLTTY on

BRLTTY runs as a system service, `65_brltty`, because `/dev/vcsa` belongs to root and the `tty`
group. The service starts at boot only once `/etc/brltty.conf` exists and is not empty; until
then it reports `[SKIP] brltty: no /etc/brltty.conf` and does nothing. Nothing on the image
installs that file, because an attached serial or USB device is no evidence of a braille display.

Write the file, naming a braille driver, a speech driver, or both. For example, a display found
automatically on USB, with speech from `espeak-ng`:

```ini
braille-driver auto
braille-device usb:
speech-driver  en
```

Driver codes are two letters: `bm` is Baum, `ht` HandyTech, `fs` Freedom Scientific, `hw`
HumanWare and so on, `auto` probes for a display, `en` is eSpeak-NG and `sd` is
Speech Dispatcher. Every braille driver BRLTTY registers is built, as a loadable module under
`/usr/lib/brltty`, and BRLTTY loads only the driver the file names, or the one `auto` finds.
BRLTTY's own manual lists every driver code and every directive the file accepts.

Then start the service, or reboot:

```sh
sudo service start brltty
```

`service` is described under [Services](administration.md#services). On the live image the file is
lost at power-off unless the session has a persistence store (see
[Getting started](getting-started.md#keep-what-you-change)).

### Speech and the sound card

At boot no audio server is running, so the BRLTTY service sets `KDOS_ALSA_DEFAULT=kdos_card` and
its voice plays straight to the sound card rather than through PipeWire. The card accepts one user
at a time. While a desktop session's PipeWire holds the card, the console voice cannot open it;
while the console voice is speaking, the desktop has no sound. Braille output does not use the
sound card and is not affected.

In a terminal inside the desktop, the synthesiser is reachable directly through PipeWire:
`espeak-ng "text"` speaks a line, and `spd-say "text"` does the same through Speech Dispatcher.
Speech Dispatcher is built with ALSA output and `espeak-ng` as its only synthesiser.

### Contracted braille

BRLTTY is built with liblouis, which installs 323 translation tables (the `.ctb`, `.utb` and `.tbl`
files that its `tables/Makefile.am` installs) covering literary and computer braille for many
languages. They are installed under `/usr/share/liblouis/tables`. Name a liblouis table with a
`louis:` prefix. For Unified English Braille grade 2, put this line in `/etc/brltty.conf`:

```ini
contraction-table louis:en-ueb-g2.ctb
```

or pass `-c louis:en-ueb-g2.ctb` on BRLTTY's command line. A table name without the prefix names
one of BRLTTY's own, smaller set of contraction tables.

### Programs that talk to BRLTTY

A program reaches a running BRLTTY through BrlAPI, BRLTTY's client interface. `brltty-clip`, which
shares a clipboard with the braille display, is one such client. BrlAPI admits a client through
*polkit*, the system service other daemons ask whether a user may perform an action. The action is
`org.a11y.brlapi.write-display`, and the rule installed with BRLTTY grants it to members of the
`brlapi` group and to nobody else. The rule tests group membership and not an active session:
KDOS runs no session manager, so polkit never sees a session as active and a rule that asked for
one would never grant anything (see
[The security model](../03-architecture/security-model.md#polkit-and-why-the-desktop-has-no-authentication-agent)).

Installing BRLTTY creates the `brlapi` group and adds the desktop account, `kdos`, to it. That
happens only when the group is first created, so an account removed from the group stays out
across reinstalls. Any other account that needs BrlAPI must be added by hand:

```sh
sudo usermod -a -G brlapi <user>
```

There is no `/etc/brlapi.key`. A key file generated when the image is built would be the same
secret on every machine installed from it, so BrlAPI's key-file authorisation is left without a
key and polkit is what admits clients.

## The magnifier

The compositor can magnify part of the screen, or all of it, around the pointer. Three actions
drive it:

| Action | Effect |
|---|---|
| `ToggleMagnify` | Turn the magnifier on or off. It comes back at the magnification it last had; the first time, at `initScale` |
| `ZoomIn` | Magnify more by one `increment`. If the magnifier is off, turn it on at one step above no magnification |
| `ZoomOut` | Magnify less by one `increment`. A step that would reach no magnification turns it off |

No key is bound to any of them in the shipped configuration, so on a fresh install the magnifier
exists but nothing reaches it. Add bindings of your own to `~/.config/kdos-comp/rc.xml`, inside its
`<keyboard>` section and after the `<default />` line:

```xml
<keybind key="W-equal"><action name="ToggleMagnify"/></keybind>
<keybind key="W-A-equal"><action name="ZoomIn"/></keybind>
<keybind key="W-A-minus"><action name="ZoomOut"/></keybind>
```

That gives `Super+=` to toggle, and `Super+Alt+=` and `Super+Alt+-` to zoom. None of the 118
bindings in the shipped file uses these chords, and neither do the compositor's built-in defaults.
If you choose others, check them against the file and against
[the desktop's keyboard shortcuts](desktop.md#keyboard-shortcuts); the rules for editing the file
are in [Changing the bindings](desktop.md#changing-the-bindings). To load the edited file, run
`kdos-comp -r` (`--reconfigure`). The desktop's right-click menu has no reload entry; the
compositor's root menu has one, and it answers the wallpaper only with `desktop_icons = no` (see
[Pointer bindings](desktop.md#pointer-bindings)).

A `<magnifier>` block in the same file sets how it behaves:

```xml
<magnifier>
  <width>400</width>
  <height>400</height>
  <initScale>2.0</initScale>
  <increment>0.2</increment>
  <useFilter>yes</useFilter>
</magnifier>
```

| Setting | Default | Meaning |
|---|---|---|
| `width` | `400` | Width of the magnified inset, in pixels, centred on the pointer. `-1` magnifies the whole screen |
| `height` | `400` | Height of the inset. `-1` magnifies the whole screen |
| `initScale` | `2.0` | Magnification the first time it is turned on; values below 1 are raised to 1 |
| `increment` | `0.2` | How much `ZoomIn` and `ZoomOut` change it; negative values are raised to 0 |
| `useFilter` | `yes` | Smooth the magnified pixels. `no` keeps them square, which suits a bitmap font |

The inset has a border, one pixel wide and red by default. Two theme keys change it, in
`~/.config/kdos-comp/themerc-override`:

```ini
magnifier.border.width: 2
magnifier.border.color: #ffb000
```

The magnifier draws on the output nearest the pointer. While it is on, the phosphor pass, the
CRT-style effect KDOS draws over the desktop, is switched off for the whole screen, because
the effect's scanlines and glow, once magnified, make text harder to read. *Direct scanout*, in
which a fullscreen window's buffer goes to the display without being composited, is also off while
the magnifier is on, so a fullscreen window is composited like any other. See
[Theming](theming.md#the-phosphor-pass).

## Larger text

Everything KDOS draws is sized by its font: a cell is half as wide as the font is tall, and every
control is measured in cells. A larger font is therefore a larger desktop. Two keys in
`~/.config/kdos/comp.conf` set the font of the chrome the compositor starts:

| Key | Sets | Default |
|---|---|---|
| `chrome_font` | The desktop icons, the dock-app column, notifications and the network passphrase prompt. Not the panel, and not the menus and popups the panel opens | `Terminus:pixelsize=32` |
| `panel_font` | The panel only; an empty value is ignored and the default applies | `Terminus:pixelsize=20` |

Each value is a fontconfig name with a size in pixels. `Terminus` is a bitmap font, so choose a size
it has: 12, 14, 16, 18, 20, 22, 24, 28 or 32. A size it lacks comes back as the nearest one it has,
silently. For anything larger, such as a doubled cell on a 4K screen, name the scalable version of
the same typeface, which draws at any size:

```ini
chrome_font = Terminus (TTF):pixelsize=64
```

Both keys are read when the session starts, so log out and back in after changing them. The
Appearance page of Settings (`Super+I`) edits `chrome_font` and its Panel page edits `panel_font`;
both write the same file.

These keys do not reach every window:

| What | Where its size comes from |
|---|---|
| Popups the panel opens, and programs you start yourself such as `kdos-res` | `Terminus:pixelsize=32`, unless started with `--font <name>` |
| `kdos-term` | `font` in `~/.config/kdos/term.conf`, or `--font`. `Ctrl+=`, `Ctrl+-` and `Ctrl+0` change the size of one window; see [kdos-term](../04-programs/kdos-term.md#the-font-per-window) |
| `foot`, the terminal on `Super+Return` | `font` in `~/.config/foot/foot.ini`, 16 pixels as shipped. `Ctrl+=`, `Ctrl+-` and `Ctrl+0` change it per window |
| Window title bars and the compositor's menus | `<theme><font>` in `~/.config/kdos-comp/rc.xml`, in points; the shipped size is 24 |
| Boxed applications | Their own toolkit's settings |

The Font page of `kdos-style` changes the typeface of `chrome_font` and `panel_font` together
without touching either size. See [Theming](theming.md#fonts).

### The pointer size

The compositor draws the pointer from the `KDOS-cursors` theme at the size in `XCURSOR_SIZE`,
which it reads when it starts; `/etc/profile.d/10-wayland.sh` sets it to 24. The theme holds each
shape at 24, 32, 48, 64 and 96 pixels. To choose a larger one, put the variable in the
compositor's environment file, `~/.config/kdos-comp/environment`, which it reads at start-up and
which overrides the value from the profile:

```ini
XCURSOR_SIZE=48
```

Log out and back in for it to take effect. KDOS's own windows ask the compositor for a named
pointer shape rather than drawing one, so they follow this size. A boxed GTK application draws
its own pointer at `gtk-cursor-theme-size` in `~/.config/gtk-3.0/settings.ini` and
`~/.config/gtk-4.0/settings.ini`, which `kdos theme` writes as 24 each time it runs and overwrites
any edit, so boxed GTK applications keep a 24-pixel pointer.

## Colour and contrast

KDOS has eight colour schemes, called accents: `phosphor`, `amber`, `ice`, `bone` (the default),
`norton`, `borland`, `perfect` and `paper`. `paper` is the only light one, dark text on a pale
ground. Switch with the picker, `kdos-style`, or from a terminal:

```sh
kdos theme paper
```

The library self-test checks every scheme against three floors, so none of them can ship with
text that fails them:

- body text on its background at 7:1 or better;
- the accent colour on the same background at 4.5:1 or better, the WCAG AA level for
  normal-sized text;
- the label of a highlighted row at 4.5:1 or better on its highlight, with the highlight still
  distinguishable from the bar behind it.

Night light (`Super+Ctrl+Shift+N`) warms the colours KDOS's own surfaces draw in, lowering green
and blue towards a white of about 3400 K and leaving red untouched. See
[Theming](theming.md#how-the-accents-are-kept-readable) for the schemes and
[The desktop](desktop.md#notifications-and-session-switches) for the switch.

## A containerised application can be read

Inside a *box*, the container a graphical application from the catalogue runs in (see the
[glossary](../06-reference/glossary.md)), the ordinary Linux accessibility stack applies. An
application's toolkit publishes its tree of accessible objects on the box's own accessibility bus,
and a screen reader running in the same box can read it. The catalogue ships no screen reader,
BRLTTY or `espeak-ng`, so you install a reader into the box yourself; `kdos-box enter <box>` opens
a shell inside it.

This is off by default. Nothing on the host answers on the accessibility bus, so with it on, every
boxed GTK application would probe at start-up for a service that is not there and wait for the
probe to time out. Off means `kdos-appbox` puts two variables in the box's environment:
`NO_AT_BRIDGE=1` and `GTK_A11Y=none`.

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
turns it on. The setting is read when the application starts, so an application already running
keeps the environment it started with.

What this gives you is whatever the application's own toolkit offers. It does not reach the panel,
the Start menu, the file chooser or anything else KDOS draws.

## What would have to change

`libktui` keeps a per-frame record of what each control would announce, filled in through
`ktui_announce()`. Ten kinds of control write to it: buttons, check boxes, radio buttons, text
inputs, lists, tables, tabs, choices (drop-down lists), sliders and text areas. Each entry carries
the control's kind, a name, a value and the item's position in its set, so a reader built on it
could say "check box Night light, on" or "tab Keys, 2 of 3". Some details of the record matter to
anyone who would build on it:

- A control announces itself whether it was reached by keyboard or by mouse. Controls whose
  selection moves inside an event handler (tables, drop-down lists, text areas, row lists, menus)
  announce from every handler, the pointer and the wheel as well as the key.
- A menu counts only the rows the cursor can land on. Separators and hidden rows are left out of
  both the ordinal and the total, so "3 of 4" matches what a person can reach by counting.
- A password field announces that it is one, with the value `hidden`, and never its contents.
- A list or a table takes its rows from the calling program, so it states the position and leaves
  the name to the program that has it.
- The record holds at most 16 entries (`KTUI_A11Y_MAX`) and is cleared at the start of every
  frame. A control that says nothing leaves nothing behind, so a reader is never handed the name
  of a control from an earlier frame.

Nothing on the system reads that record except the library's own self-test
(`src/libs/selftest.c`). The programming interface is described in
[The C libraries](../05-developer/c-libraries.md#libktui).

For a screen reader to work on the KDOS desktop, two things would have to be built. First, the
record would have to leave the program that made it, over a socket, a bus interface or a bridge to
AT-SPI; it has none of these. Second, a reader program would have to exist, with a defined limit
on what it may do. A reader that could also type into other windows would be indistinguishable
from a keylogger, so whatever carries the announcements has to grant less than the interface a
window manager uses.

Neither is built. The gap is recorded in [Known gaps](../06-reference/known-gaps.md).

## See also

- [Configuration](../06-reference/configuration.md#configkdosa11y) — the `~/.config/kdos/a11y` file
- [kdos-appbox](../04-programs/kdos-appbox.md) — how a box's environment is built
- [The C libraries](../05-developer/c-libraries.md#libktui) — `ktui_announce()`, and what a widget says
- [Administration](administration.md#services) — starting, stopping and disabling `65_brltty`
- [Known gaps](../06-reference/known-gaps.md) — the missing desktop screen reader, stated as a gap
- [The desktop](desktop.md) — the keyboard route to every surface
- [Theming](theming.md) — fonts, accents and the phosphor pass
- [The ports catalogue](../06-reference/ports-catalogue.md) — `brltty`, `liblouis`, `espeak-ng` and `speech-dispatcher` among the other ports

<!-- book-nav -->
---

*Part II — Using KDOS, chapter 11.* Previous: [10. Administration](administration.md) · [Contents](../README.md) · Next: [12. Architecture overview](../03-architecture/overview.md)
