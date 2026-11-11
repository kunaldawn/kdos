# Accessibility

This chapter is for anyone who relies on a screen reader, braille, speech or magnification, and
for anyone setting up a KDOS machine for such a person. It states what accessibility support KDOS
has, how to turn on each part, and what it does not reach. No earlier chapter is needed, but
[Getting started](getting-started.md) explains the login and the text consoles this chapter refers
to, and [The desktop](desktop.md) explains the desktop's keyboard routes.

The desktop is Plasma, so it carries Plasma's accessibility: the whole desktop is published to
screen readers, KWin magnifies and filters the keyboard and pointer, and System Settings →
Accessibility holds every switch. KDOS adds a few things: the screen reader works from the first
login and on the login screen, the Welcome tour asks about accessibility first, the eight accents
are held to contrast floors, and the text consoles have BRLTTY for braille and speech.

## What is available

| Need | What KDOS offers | Where |
|---|---|---|
| A screen reader | Orca reads the panel, menus, System Settings, KDOS's own pages and the applications, on `Meta+Alt+S`; on the login screen it starts and is silent | [The screen reader](#the-screen-reader) |
| Braille on the desktop | Orca drives a braille display through BRLTTY | [The screen reader](#the-screen-reader) |
| Braille or speech at a text console | BRLTTY, on `tty2`, the installer's text front end and any other console | [Braille and speech at a text console](#braille-and-speech-at-a-text-console) |
| Magnification | KWin's zoom, on `Meta+=`, and its magnifier | [The magnifier](#the-magnifier) |
| Larger text | The font size, the screen's scale, and a larger pointer | [Larger text](#larger-text) |
| Typing with limited movement | Sticky, slow and bounce keys | [Keyboard aids](#keyboard-aids) |
| Clicking with limited movement | Dwell click: resting the pointer clicks | [Dwell click](#dwell-click) |
| Pointing without a mouse | Mouse keys: the number pad moves the pointer and clicks | [Pointing from the keyboard](#pointing-from-the-keyboard) |
| Writing with a pointer or a switch | Dasher, which types letters chosen by steering | [Writing with Dasher](#writing-with-dasher) |
| Speaking typed text | KMouth, over Speech Dispatcher | [Writing with Dasher](#writing-with-dasher) |
| Typing without a keyboard | Plasma's on-screen keyboard | [The on-screen keyboard](#the-on-screen-keyboard) |
| Contrast and colour | Eight accents held to contrast floors, one of them light; colour-blindness correction and inverted colours | [Colour and contrast](#colour-and-contrast) |
| Less motion | One switch that stops every animation | [Reducing motion](#reducing-motion) |
| A boxed application read aloud | The box reaches the accessibility bus while a reader is in use | [Boxed applications](#boxed-applications) |

The Welcome tour, which opens on the first login, shows its Accessibility page straight after its
welcome page (and after the live page on the live medium), before anything else is asked. It
holds what a person needs first: switches for the screen reader, reduced motion, the four keyboard
and mouse aids and dwell click, a button to larger text, the two high-contrast accents, the zoom
chords, and a button to the rest. Every switch takes effect at once and is changed later in System
Settings → Accessibility; the page reads them again whenever it is shown, so a change made
elsewhere while the tour is open shows there too.

## The screen reader

A screen reader works from a tree of *accessible objects* that an application's toolkit publishes,
on Linux through the AT-SPI accessibility bus. Each object describes one control: what kind it is,
what it is called and what it is set to. Plasma, System Settings, KDOS's own settings pages,
plasmoids, Welcome pages and installer window, and every Qt, KDE and GTK application publish
theirs, and Orca reads them; `Alt+Tab` is announced by the window it switches to. Two parts of the
desktop publish nothing Orca can use. KWin's overview (`Meta+W`) is silent: Orca echoes the keys
and reads none of its windows, desktops or search results. The lock screen reads its password
field and its Show Password, Virtual Keyboard and user buttons, has one Tab stop that says nothing,
and never reaches its Sleep and Switch User buttons; see
[Known gaps](../06-reference/known-gaps.md#the-overview-is-silent-under-the-screen-reader-and-the-lock-screen-only-partly-read).

`Meta+Alt+S` starts and stops Orca, from the first login, as do the Screen Reader page of System
Settings → Accessibility and the Welcome tour's Accessibility page. The same chord starts Orca on
the login screen, but there it has no sound to speak through (see
[Known gaps](../06-reference/known-gaps.md#the-screen-reader-at-the-login-screen-has-no-sound)). Orca speaks through Speech Dispatcher and eSpeak NG, and
drives a braille display through BRLTTY's client interface, BrlAPI, when BRLTTY is running (see
[Programs that talk to BRLTTY](#programs-that-talk-to-brltty)). Orca's own settings, voices, key
echo and verbosity among them, are in its preferences dialog.

Plasma's screen-reader module starts Orca itself. It would switch for good to starting an `orca`
service unit if it ever saw systemd on the session bus, and KDOS has no units, so the shipped
`/usr/share/kdos/xdg/kaccessrc` makes that switch impossible and the chord keeps starting Orca.
Whether Orca is on stays your setting: the chord, the System Settings page and the Welcome switch
each write it to `[ScreenReader] Enabled` in your `~/.config/kaccessrc`, and the module starts Orca
at login when it says so.

## Braille and speech at a text console

Four packages provide braille and speech below the desktop.
[The ports catalogue](../06-reference/ports-catalogue.md) lists them with the rest of the ports.

| Port | Version | What it is |
|---|---|---|
| `brltty` | 6.9.1 | The console screen reader: drives a braille display, speaks, or both |
| `liblouis` | 3.39.0 | Braille translation tables, including contracted (grade 2) braille |
| `espeak-ng` | 1.52.0 | A speech synthesiser |
| `speech-dispatcher` | 0.12.1 | The common speech interface screen readers talk to, with `espeak-ng` as its voice |

BRLTTY works at a text console, where the screen is a grid of characters it can read directly. In
the graphical session the reader is Orca, which uses BRLTTY only to reach a braille display.

### Which screens BRLTTY reads

BRLTTY reads the Linux text console through `/dev/vcsa`, which always shows the virtual terminal
in the foreground. That covers:

- `tty2`, a plain login prompt on every KDOS system, which you reach from the desktop with
  `Ctrl+Alt+F2`;
- the installer's full-screen front end, `kinstall`, when it runs on a text console of its own (see
  [Running the installer](installation.md#running-the-installer));
- the password prompt `kdos-dm` puts on `tty1` when the login screen cannot start.

While the login screen or a desktop is in the foreground, `/dev/vcsa` holds nothing BRLTTY can use;
Orca is the reader there. Speech from BRLTTY shares the sound card with the desktop; see
[Speech and the sound card](#speech-and-the-sound-card).

The build selects three screen drivers: `lx`, the Linux console, which is the default; `em`, which
reads a terminal run under `brltty-pty`; and `tx`, which reads a `tmux` session. A different one is
chosen with `screen-driver` in `/etc/brltty.conf`.

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
eSpeak-NG is linked into `brltty` itself; the other speech drivers, `sd` among them, are modules
in the same directory. BRLTTY's own manual lists every driver code and every directive the file
accepts.

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
at a time. BRLTTY's voice opens the card for each utterance and closes it once the utterance has
played, so a BRLTTY that is not speaking leaves the card free. While a desktop session's PipeWire
holds the card, the console voice cannot open it; while the console voice is speaking, the desktop
has no sound, and Speech Dispatcher, Orca's voice, waits and speaks once the console voice has
finished. Braille output does not use the sound card and is not affected. On a machine that runs
the desktop, use Orca for speech and BRLTTY for braille.

In a terminal inside the desktop, speech plays through ALSA's default device, which in a session
is PipeWire: `espeak-ng "text"` speaks a line, and `spd-say "text"` does the same through Speech
Dispatcher. Speech Dispatcher is built with ALSA output and `espeak-ng` as its only synthesiser.
Its Python module, `speechd`, is installed with it, and so is `spd-conf`, which writes a per-user
configuration and runs Speech Dispatcher's own diagnostics.

`piper`, a neural speech synthesiser, is on the image as well and speaks through PipeWire. It
ships no voice: a voice is a model file passed with `-m`, and `kdos speech voice get` fetches one
into `~/.local/share/piper/voices` (see
[`kdos speech`](../04-programs/kdos-command.md#kdos-speech)). Speech Dispatcher and BRLTTY do not use
it (see [Local AI, speech and translation](applications.md#local-ai-speech-and-translation)).

### Contracted braille

BRLTTY is built with liblouis, which installs 323 translation tables (`.ctb`, `.utb` and `.tbl`
files) covering literary and computer braille for many languages. They are installed under
`/usr/share/liblouis/tables`. Name a liblouis table with a `louis:` prefix. For Unified English
Braille grade 2, put this line in `/etc/brltty.conf`:

```ini
contraction-table louis:en-ueb-g2.ctb
```

or pass `-c louis:en-ueb-g2.ctb` on BRLTTY's command line. A table name without the prefix names
one of BRLTTY's own, smaller set of contraction tables. A Python program reaches the same tables
through the `louis` module, which is installed with liblouis; Orca uses it for contracted braille
on the desktop.

### Programs that talk to BRLTTY

A program reaches a running BRLTTY through BrlAPI, BRLTTY's client interface. Orca is one such
client, through the `brlapi` Python module installed with BRLTTY; `brltty-clip`, which shares a
clipboard with the braille display, is another. BrlAPI admits a client through *polkit*, the system
service other daemons ask whether a user may perform an action. The action is
`org.a11y.brlapi.write-display`, and the rule installed with BRLTTY grants it to members of the
`brlapi` group and to nobody else. The rule tests group membership and not an active session, so it
covers a program in a console login or a background job as well as one in the desktop (see
[The security model](../03-architecture/security-model.md#polkit-and-the-authentication-agent)).

Installing BRLTTY creates the `brlapi` group and adds the desktop account, `kdos`, to it. That
happens only when the group is first created, so an account removed from the group stays out
across reinstalls. Any other account that needs braille, through Orca or otherwise, must be added
by hand:

```sh
sudo usermod -a -G brlapi <user>
```

There is no `/etc/brlapi.key`. A key file generated when the image is built would be the same
secret on every machine installed from it, so BrlAPI's key-file authorisation is left without a
key and polkit is what admits clients.

## The magnifier

KWin magnifies the screen around the pointer:

| Key | Action |
|---|---|
| `Meta+=` | Zoom in |
| `Meta+-` | Zoom out |
| `Meta+0` | Back to actual size |

The zoom follows the pointer and the text cursor, so what you type stays in view: KDOS turns on
following the text cursor, which works in every Wayland program that takes text through the input
method or the on-screen keyboard. It does not follow the keyboard focus, which KWin leaves off on
Wayland. System Settings → Accessibility, and Window Management → Desktop Effects, choose between
the full-screen zoom and a magnifying inset that follows the pointer, and set how the view tracks
the pointer and the text cursor.

While either is on, Phosphor mode's CRT pass stands aside, because scanlines and glow, once
magnified, make text harder to read (see [Phosphor mode](theming.md#phosphor-mode)).

## Larger text

| What | Where |
|---|---|
| Every font at once | System Settings → Text & Fonts: the interface, fixed-width, toolbar, menu and window-title sizes |
| Everything, text and controls alike | System Settings → Display & Monitor: the scale of each screen, which may be a fraction |
| One Konsole tab | `Ctrl++`, `Ctrl+-` and `Ctrl+0` |
| Konsole's default | The profile's font, under **Settings → Edit Current Profile → Appearance** |
| A web page | The browser's own zoom, `Ctrl++` |
| The text consoles | The console font is a fixed 16×32-pixel bitmap |

The Welcome tour's **Make Text Larger…** opens the font page. A boxed application takes the fonts
and the scale the session hands it; one that ignores them keeps its own toolkit's settings.

### The pointer size

System Settings → Mouse & Touchpad → Cursors sets the pointer's size, and KWin draws the pointer at
that size over every window. The pointer theme is Breeze's, at 24 pixels by default. KWin's shake
effect briefly enlarges the pointer when you shake it, to find it on a large screen.

## Keyboard aids

Three aids change how the keys of a physical keyboard are read, all off until you turn them on in
System Settings → Accessibility or on the Welcome tour's Accessibility page:

| Aid | What it does |
|---|---|
| Sticky keys | Press and release `Shift`, `Ctrl`, `Alt` or `Meta` on its own and it applies to the next key; pressed twice, it stays on until pressed again |
| Slow keys | A key counts only once it has been held down for a moment; a key brushed and released sooner is ignored |
| Bounce keys | A second press of the same key soon after the first is ignored, so a hand that shakes does not type a letter twice |

The delays, and whether a sound or a notification marks each aid's use, are on the same page. The
aids act on the desktop's shortcuts as well as on windows, because KWin applies them before a key
reaches anything else.

## Dwell click

Dwell click clicks wherever the pointer comes to rest, for anyone who can move a pointer but not
press a button reliably. Turn it on in System Settings → Accessibility, or on the Welcome tour's
Accessibility page; the rest time is set on the Accessibility page.

## Pointing from the keyboard

Mouse keys move the pointer with the number pad: the digits around `5` move it, `5` clicks, and the
keys above the pad choose the button. Turn it on in System Settings → Accessibility (Mouse
Navigation) or on the Welcome tour's Accessibility page; the speed and acceleration are set there.

## Writing with Dasher

Dasher writes text without a keyboard. Letters stream towards the pointer in boxes sized by how
likely each one is to come next, and steering into a box writes its letter, so a mouse, a head
pointer, an eye tracker that moves the pointer or a single switch is enough. It can speak what has
been written through Speech Dispatcher. Dasher runs under Xwayland, because it reads the pointer
through X11, and its direct mode, which types into another application, reaches only other X11
windows; for a Wayland window, write in Dasher's own text box and paste.

KMouth speaks what is typed into it, or a sentence picked from its phrase books, through Speech
Dispatcher, for someone who cannot speak. On its first start it offers a voice in the session's
language, an English one under the default `en_US.UTF-8`; another is chosen in its first-run
dialogue or later under Settings → Configure KMouth.

## The on-screen keyboard

Plasma's on-screen keyboard types into whichever window has the keyboard focus. KWin runs one
input method at a time, and KDOS starts fcitx5, the input method for Chinese, Japanese and Korean
(see [Input methods](administration.md#input-methods)). To use the on-screen keyboard instead,
choose Plasma Keyboard on System Settings → Keyboard → Virtual Keyboard. It then appears when a
text field is focused by touch or a tablet pen; to have a mouse click bring it up too, set **Show
Virtual Keyboard** on the same page to **With Touch, Tablet, and Mouse**. A program running under
Xwayland does not say when a text field has the focus, so the keyboard does not appear by itself
over one.

While fcitx5 is the input method, the **Virtual Keyboard** button on the lock screen does nothing:
fcitx5 draws no keyboard, and the button is shown whenever KWin has an input method at all.
Choose Plasma Keyboard first to type on the lock screen without a keyboard.

The login screen has Plasma Keyboard whatever the session's input method is: its own KWin starts
it. It appears when the password field is touched, and the **Virtual Keyboard** button at the
bottom left shows and hides it for a mouse. With more than one keyboard layout set, the button
beside it names the current layout and switches to the next.

## Colour and contrast

KDOS has eight colour schemes, called accents: `phosphor`, `amber`, `ice`, `bone` (the default),
`norton`, `borland`, `perfect` and `paper`. `paper` is the only light one, dark text on a pale
ground. Switch on System Settings' KDOS Look page, on the Welcome tour's Accessibility page (which offers the
highest-contrast dark accent and the light one), or from a terminal:

```sh
kdos theme paper
```

Every scheme is held to contrast floors when the image is built, and none can ship below them:
every text role at 4.5:1 or better, the WCAG AA level for normal-sized text, on both backgrounds it
is drawn on; focus and hover decorations at 3:1; and a selected row at 4.5:1, for its own text
and for the list's ordinary text over a translucent selection. See
[How the accents are kept readable](theming.md#how-the-accents-are-kept-readable). Kontrast checks
any pair of colours against the same guidelines.

KWin also corrects for colour blindness (protanopia, deuteranopia and tritanopia), and inverts the
colours of the whole screen or of one window; both are under System Settings → Accessibility and
Desktop Effects. Inversion is off until its effect is turned on there, and `Meta+Ctrl+I` (the
screen) and `Meta+Ctrl+U` (the window) work from then on. Night Light warms the whole screen
towards a lower colour temperature on the schedule set on its System Settings page;
`Meta+Ctrl+Shift+N` turns it on, warm at once and all day, or off.

## Reducing motion

One switch stops every animation: windows opening, closing and moving between desktops, the
overview, popups and notifications sliding in, and, with Phosphor mode on, the CRT's unfolding,
folding, degauss and power-down. Plasma, KWin and every KDE application follow it at once.

```sh
kdos toggle reduced-motion
```

The same switch is on the Welcome tour's Accessibility and Look pages, and on System Settings → General Behavior, where the
animation speed slider's fastest end is instant; KDOS ships animations at half their usual length.
`kdos toggle reduced-motion` writes `AnimationDurationFactor=0` into your own `~/.config/kdeglobals`,
and turning it off removes that key again, so the shipped speed applies. Phosphor mode's still
picture stays: only its motion stops. See [Animation speed](../04-programs/kwin.md#animation-speed)
for the setting KWin's effects follow, and [Motion](../03-architecture/design-language.md#motion)
for the rule every animation follows.

## Boxed applications

Inside a *box*, the container an application from the catalogue runs in (see the
[glossary](../06-reference/glossary.md#box)), the ordinary Linux accessibility stack applies. An
application's toolkit publishes its tree of accessible objects on the accessibility bus, and Orca
on the host reads it like any other window.

A box's bus filter lets it reach the accessibility bus whenever assistive technology is in use:
the screen reader is switched on in System Settings, a reader or another assistive program is
attached to the accessibility bus, or the file `~/.config/kdos/a11y` exists. Otherwise the box is
kept off the accessibility bus, and `kdos-appbox` puts `NO_AT_BRIDGE=1` and `GTK_A11Y=none` in its
environment, so that a GTK application does not spend its start-up looking for it.

To turn it on for every box whatever else is running, create an empty file:

```sh
touch ~/.config/kdos/a11y
```

To decide one launch, set `KDOS_A11Y` in front of the command:

```sh
KDOS_A11Y=1 hugin                        # the command on your PATH
KDOS_A11Y=1 kdos-appbox -b app.hugin run hugin
```

`KDOS_A11Y=0` turns it off for that launch whatever else is true; any other non-empty value turns
it on. A box's filter is set when the box starts, so a screen reader switched on later reaches a
running box from its next start. What a boxed application offers is whatever its own toolkit
publishes.

## The announcement record

`libktui`, the library KDOS's text-console programs (the installer's full-screen front end among
them) are built on, keeps a per-frame record of what each control would announce, filled in
through `ktui_announce()`: the control's kind, a name, a value and its position in its set, so a
reader built on it could say "check box Administrator, on" or "tab Keys, 2 of 3". A password field
announces that it is one, with the value `hidden`, and never its contents. Nothing on the system
reads the record except the library's own self-test; at a text console BRLTTY reads the screen
itself. The programming interface is described in
[The C libraries](../05-developer/c-libraries.md#announcements).

## See also

- [Configuration](../06-reference/configuration.md#configkdosa11y) — the `~/.config/kdos/a11y` file
- [KWin](../04-programs/kwin.md#accessibility) — zoom and the keyboard and pointer aids as KWin provides them
- [kdos-appbox](../04-programs/kdos-appbox.md#the-environment-a-box-gets) — how a box's environment is built
- [Administration](administration.md#services) — starting, stopping and disabling the `brltty` service
- [The desktop](desktop.md#accessibility) — the accessibility shortcuts
- [Theming](theming.md) — fonts, accents and Phosphor mode
- [The ports catalogue](../06-reference/ports-catalogue.md) — `orca`, `brltty`, `liblouis`, `espeak-ng` and `speech-dispatcher` among the other ports

<!-- book-nav -->
---

*Part II — Using KDOS, chapter 11.* Previous: [10. Administration](administration.md) · [Contents](../README.md) · Next: [12. Architecture overview](../03-architecture/overview.md)
