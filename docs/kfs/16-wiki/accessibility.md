# Accessibility

This page covers the accessibility aids a KDOS machine has and how to turn each one on: the
[screen reader]{.idx} Orca, which reads the whole Plasma desktop and its applications, [braille]{.idx}
and speech at a text console, KWin's zoom and [magnifier]{.idx}, the keyboard and pointer aids, the
on-screen keyboard, Dasher, colour correction and reduced motion. The
[Accessibility](../../kdos/02-user-guide/accessibility.md) chapter of the KDOS book describes each
aid in full; this page gives the files and the procedures.

## Installation

| Port | Provides |
|:--|:--|
| [orca](#port-orca) | The screen reader, for the desktop, the greeter and every GTK and Qt application |
| [at-spi2-core](#port-at-spi2-core) | The accessibility bus the toolkits and Plasma publish to |
| [kwin](#port-kwin) | Zoom and the magnifier, sticky, slow and bounce keys, mouse keys, dwell click, colour-blindness correction and colour inversion |
| [plasma-workspace](#port-plasma-workspace) | `kaccess`, which applies the keyboard aids and starts Orca on `Meta+Alt+S` |
| [plasma-desktop](#port-plasma-desktop) | System Settings → **Accessibility** |
| [plasma-keyboard](#port-plasma-keyboard) | The on-screen keyboard |
| [espeak-ng](#port-espeak-ng), [pcaudiolib](#port-pcaudiolib) | The speech synthesiser and its audio output |
| [speech-dispatcher](#port-speech-dispatcher) | `speech-dispatcher`, `spd-say` and [`spd-conf`]{.idx}, with `espeak-ng` as the voice Orca speaks with |
| [piper](#port-piper) | A neural text-to-speech program, `piper` |
| [brltty](#port-brltty) | The console screen reader, braille and speech, [`brltty-clip`]{.idx}, the Python `brlapi` module Orca uses for braille, the service script `65_brltty.sh` and the `brlapi` group |
| [liblouis](#port-liblouis) | Braille translation tables, contracted braille among them, under `/usr/share/liblouis/tables` |
| [dasher](#port-dasher) | Text entry by steering the pointer through a language model |
| [kdos-appbox](#port-kdos-appbox) | Gives a boxed application the accessibility bus whenever assistive technology is in use |

`65_brltty.sh` is the only service. It ships enabled, and it does nothing at boot until
[`/etc/brltty.conf`]{.idx} exists and is not empty, printing `[SKIP] brltty: no /etc/brltty.conf`.
Everything else runs inside the desktop session.

## Configuration

### /etc/init.d/65_brltty.sh

The [brltty](#port-brltty) port's `build.sh` writes the service script:

<!-- kfs:include ports/core/accessibility/brltty/build.sh lines=55-89 pin=658b4098a535 -->

`KDOS_ALSA_DEFAULT=kdos_card` sends the voice straight to the sound card, because no [PipeWire]{.idx} runs
at boot. [espeak-ng](#port-espeak-ng) opens the card for each utterance and closes it once the
utterance has played, so BRLTTY holds the card only while it speaks; see [Speech and the sound card](../../kdos/02-user-guide/accessibility.md#speech-and-the-sound-card).

### /etc/brltty.conf

Not shipped. You write it, naming a braille driver, a speech driver, or both. `brltty(1)` and
BRLTTY's own documentation list every driver code and directive; the drivers KDOS builds are in
[Turning BRLTTY on](../../kdos/02-user-guide/accessibility.md#turning-brltty-on).

### The brlapi group and its polkit rule

A program that talks to a running BRLTTY goes through BrlAPI, which asks polkit. The rule BRLTTY
installs, `/usr/share/polkit-1/rules.d/org.a11y.brlapi.rules`, grants the action to the `brlapi`
group. The port's install hook creates the group and adds the `kdos` account the first time:

<!-- kfs:include ports/core/accessibility/brltty/postinstall.sh lines=22-27 pin=7426fd1e7455 -->

### The screen reader's chord

`Meta+Alt+S` starts and stops Orca from the first login. Plasma's screen reader module would
switch to starting an `orca.service` unit if it ever saw systemd on the session bus, and KDOS has
no units, so the key that would record that switch is held immutable, from
`fs/usr/share/kdos/xdg/kaccessrc`:

<!-- kfs:include fs/usr/share/kdos/xdg/kaccessrc -->

### The aids' settings

Every aid on System Settings → **Accessibility** is saved in `~/.config/kaccessrc` (the keyboard
aids and the screen reader) or `~/.config/kwinrc` (zoom, the magnifier and colour correction).
Mouse keys and the pointer size are on the **Mouse** and **Cursors** pages.

### Boxed applications

A boxed application is given the accessibility bus only while assistive technology is in use:
the screen reader is on in System Settings, the accessibility registry says a tool is attached,
or `~/.config/kdos/a11y` exists (an empty file is enough). `KDOS_A11Y=1` or `0` decides one
launch. Otherwise the box is started with `NO_AT_BRIDGE=1` and `GTK_A11Y=none`. The bus carries
the text of every application on it, which is why it is not given out by default. A box's bus
policy is set when its keeper starts, so a screen reader turned on later reaches a box from its
next start.

## Usage

### Reading the desktop with Orca

1. Press `Meta+Alt+S`, or turn **Screen Reader** on under System Settings → **Accessibility**.
2. Orca speaks the focused window, the panel, Kickoff, KRunner, the notifications and the
   applications. `Orca+Space` (the `Insert` key, or `Caps Lock` in the laptop layout) opens its
   preferences; `orca -s` does the same from a terminal.
3. `Meta+Alt+S` again stops it.

The SDDM greeter has its own Orca, started by `Meta+Alt+S` at the login screen, but it is silent
there: the `sddm` account runs no PipeWire, so speech-dispatcher opens no audio device (see
[Known gaps](../../kdos/06-reference/known-gaps.md#the-screen-reader-at-the-login-screen-has-no-sound)).

### Braille or speech at a text console

1. Log in on `tty2` (`Ctrl+Alt+F2`).
2. Write `/etc/brltty.conf` as root. For a display found on USB, with speech:

   ```bash
   sudo tee /etc/brltty.conf <<'EOF'
   braille-driver auto
   braille-device usb:
   speech-driver  en
   EOF
   ```

   For contracted braille, add a liblouis table, such as `contraction-table louis:en-ueb-g2.ctb`.

3. Start the service. It starts at every boot after this, for as long as the file exists:

   ```bash
   sudo service start brltty
   service status brltty
   ```

4. Read what BRLTTY printed if nothing happens. The log is readable by root only:

   ```bash
   sudo cat /run/kdos-svc.brltty.log
   ```

Speech at the console takes the sound card for itself. While it speaks, the desktop has no sound;
while a session's PipeWire holds the card, the console voice cannot open it. Braille is not
affected. In the desktop, Orca drives the same braille display through BrlAPI.

On the live medium `/etc/brltty.conf` is lost at power-off unless the session has a persistence
store; see [Live persistence](live-persistence.md). On an installed system it stays.

### Letting another account use BrlAPI

```bash
sudo usermod -a -G brlapi <user>
```

The account takes the group at its next login.

### Speech from a terminal in the desktop

```bash
espeak-ng "The build has finished"
spd-say "The build has finished"
```

Both play through PipeWire inside the session. `spd-conf` writes a per-user Speech Dispatcher
configuration and runs its diagnostics.

### The magnifier

`Meta+=` and `Meta+-` zoom the whole screen in and out around the pointer, and `Meta+0` returns
to normal. System Settings → **Accessibility** → **Zoom and Magnifier** (or Desktop Effects)
chooses between whole-screen zoom and a magnifier window that follows the pointer, and how the
view tracks the focus.

### Keyboard aids

System Settings → **Accessibility** → **Modifier Keys** (sticky keys, locking keys) and
**Keyboard Filters** (slow keys and bounce keys). Each can also be turned on by a keyboard gesture
on the same page: five presses of `Shift` for sticky keys, `Shift` held for eight seconds for slow
keys.

### Pointing from the keyboard

System Settings → **Mouse** → **Keyboard Navigation** turns on mouse keys: the number pad moves
the pointer and `5` clicks.

### Dwell click

System Settings → **Accessibility** has dwell clicking: resting the pointer still clicks.

### The pointer size

System Settings → **Colors & Themes** → **Cursors** sets the size, up to 72 pixels. KWin's **Shake
cursor** effect enlarges the pointer for a moment when you shake the mouse.

### The on-screen keyboard

1. System Settings → **Keyboard** → **Virtual Keyboard**, and choose **Plasma Keyboard**. This
   replaces fcitx5 as the session's input method, so the input methods are unavailable while it
   is chosen; see [Input methods](input-methods.md).
2. The keyboard appears when a text field takes focus in a Wayland application. The
   system tray's virtual keyboard entry shows and hides it by hand.

### Writing with Dasher

Start **Dasher** from Kickoff. Steer the pointer towards the letters you want; the text it builds
can be copied into any application.

### Colour and contrast

- Every accent meets the same contrast floors, and `paper` is the one light accent:
  `kdos theme paper`. See [Fonts and theming](fonts-and-theming.md#changing-the-accent).
- System Settings → **Accessibility** → **Color Blindness Correction** filters the whole screen for
  protanopia, deuteranopia or tritanopia; Desktop Effects' **Invert** inverts the screen or a
  window.

### Larger text

System Settings → **Text and Fonts** → **Adjust All Fonts**, or a larger screen scale; see
[Displays and scaling](displays-and-scaling.md#the-size-of-the-desktops-text).

### Reducing motion

System Settings → **General Behavior** → **Animation speed** to **Instant**. KDOS ships it at half
the usual length.

### Letting a boxed application be read

A boxed application is read whenever Orca is on. To give boxes the accessibility bus for some
other tool, create the opt-in file and restart the box:

```bash
touch ~/.config/kdos/a11y
kdos-box stop <box>
```

## Tips

- The key card (`Meta+F1`) lists the accessibility chords under Accessibility.
- A keyboard that misbehaves for no visible reason may have an aid switched on by a gesture.
  System Settings → **Accessibility** shows which, and the gestures can be turned off there.
- Konsole changes its text size per window with `Ctrl++`, `Ctrl+-` and `Ctrl+0`.

## Troubleshooting

**`Meta+Alt+S` does nothing.** Check that Orca starts at all: `orca --version`, then `orca` from
Konsole and read what it prints. `grep -A2 ScreenReader ~/.config/kaccessrc` must not show
`SystemdManaged=true`.

**Orca is silent.** It speaks through Speech Dispatcher: `spd-say test` must be heard. If it is not,
`spd-conf -d` runs the diagnostics, and the session's sound must work (see [Audio](audio.md)).

**`brltty` never starts at boot.**
`/etc/brltty.conf` is missing or empty. Read `/run/kdos-init.brltty.log` for the `[SKIP]` line,
write the file, and run `sudo service start brltty`.

**BRLTTY runs, and there is no speech.**
A session holds the sound card. Speech at the console works while no session's PipeWire has the
card open. Drop the `speech-driver` line for braille only, or stop the service with
`sudo service stop brltty`.

**`service status brltty` answers `[FAIL] brltty  (exited with code 134; restarting)`.**
BRLTTY aborts and the supervisor starts it again every five seconds. Read its last words with
`sudo tail /run/kdos-svc.brltty.log`. `Assertion failed: !snd_interval_empty(i)` there means the
voice's sound device is a `plug` stacked on another `plug`: check that `kdos_card` in
`/etc/alsa/conf.d/99-kdos-pipewire.conf` is `type empty`, as shipped.

**A BrlAPI client such as `brltty-clip` is refused.**
The account is not in `brlapi`: check with `id`, then add it as above and log in again.

**Orca says nothing for a boxed application.** The box was started before the screen reader was
on. Stop the box (`kdos-box stop <box>`) and open the application again.

## Known gaps

- [Known gaps](../../kdos/06-reference/known-gaps.md): the desktop's open items
- [Features whose library has no port](../../kdos/06-reference/known-gaps.md#features-whose-library-has-no-port):
  BRLTTY's Libbraille driver

## See also

- [Accessibility](../../kdos/02-user-guide/accessibility.md): every aid, described in full
- [kdos-appbox](../../kdos/04-programs/kdos-appbox.md): the environment a box is given
- [Services](services.md), [Fonts and theming](fonts-and-theming.md),
  [Displays and scaling](displays-and-scaling.md), [Screen lock](screen-lock.md),
  [Input methods](input-methods.md), [Live persistence](live-persistence.md)

<!-- book-nav -->
---

*Part XVI. System administration, chapter 441.* Previous: [440. Fonts and theming](fonts-and-theming.md) · [Contents](../README.md) · Next: [442. Portals and screen sharing](portals-and-screen-sharing.md)
