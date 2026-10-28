# Accessibility

This page covers the accessibility aids a KDOS machine has and how to turn each one on: [braille]{.idx} and
speech at a text console, the compositor's [magnifier]{.idx}, the keyboard and pointer aids, Dasher, the
on-screen keyboard, reduced motion, and the [screen reader]{.idx} for native applications. It also states plainly
what is missing: the desktop's own windows are not read by any screen reader. The
[Accessibility](../../kdos/02-user-guide/accessibility.md) chapter of the KDOS book describes each
aid in full; this page gives the files and the procedures.

## Installation

| Port | Provides |
|:--|:--|
| [brltty](#port-brltty) | The console screen reader, braille and speech, [`brltty-clip`]{.idx}, the Python `brlapi` module, the service script `65_brltty.sh` and the `brlapi` group |
| [liblouis](#port-liblouis) | Braille translation tables, contracted braille among them, under `/usr/share/liblouis/tables` |
| [espeak-ng](#port-espeak-ng), [pcaudiolib](#port-pcaudiolib) | The speech synthesiser and its audio output |
| [speech-dispatcher](#port-speech-dispatcher) | `speech-dispatcher`, `spd-say` and [`spd-conf`]{.idx}, with `espeak-ng` as the only voice |
| [piper](#port-piper) | A neural text-to-speech program, `piper` |
| [at-spi2-core](#port-at-spi2-core) | The accessibility bus the toolkits publish to |
| [orca](#port-orca) | The screen reader for native GTK and Qt applications |
| [dasher](#port-dasher) | Text entry by steering the pointer through a language model |
| [wvkbd](#port-wvkbd) | The on-screen keyboard, `wvkbd-deskintl` |
| [wl-kbptr](#port-wl-kbptr) | **Keyboard Pointer**, moving and clicking the pointer from the keyboard |
| [kdos-comp](#port-kdos-comp) | The magnifier, the keyboard aids, dwell click, the pointer size and the keyboard monitor |
| [kdos-appbox](#port-kdos-appbox) | The switch that lets a boxed application publish to an accessibility bus |

Every one of them is named in a phase list and installed on the image. `orca --version` prints
`Orca version 51.0, AT-SPI2 version: 2.62.0, Session: wayland`.

`65_brltty.sh` is the only service. It ships enabled, and it does nothing at boot until
[`/etc/brltty.conf`]{.idx} exists and is not empty, printing `[SKIP] brltty: no /etc/brltty.conf`. The
rest run inside the desktop session, started by the compositor or by you.

## Configuration

### /etc/init.d/65_brltty.sh

The [brltty](#port-brltty) port's `build.sh` writes the service script:

<!-- kfs:include ports/core/accessibility/brltty/build.sh lines=55-88 pin=b28c20d781c2 -->

`KDOS_ALSA_DEFAULT=kdos_card` sends the voice straight to the sound card, because no [PipeWire]{.idx} runs
at boot; see [Speech and the sound card](../../kdos/02-user-guide/accessibility.md#speech-and-the-sound-card).

### /etc/brltty.conf

Not shipped. You write it, naming a braille driver, a speech driver, or both. `brltty(1)` and
BRLTTY's own documentation list every driver code and directive; the drivers KDOS builds are in
[Turning BRLTTY on](../../kdos/02-user-guide/accessibility.md#turning-brltty-on), and its screen
drivers in [Which screens BRLTTY reads](../../kdos/02-user-guide/accessibility.md#which-screens-brltty-reads).

### The brlapi group and its polkit rule

A program that talks to a running BRLTTY goes through BrlAPI, which asks polkit. The rule BRLTTY
installs, `/usr/share/polkit-1/rules.d/org.a11y.brlapi.rules`, grants the action to the `brlapi`
group. The port's install hook creates the group and adds the `kdos` account the first time:

<!-- kfs:include ports/core/accessibility/brltty/postinstall.sh lines=22-27 pin=7426fd1e7455 -->

### ~/.config/kdos/comp.conf

Every aid the compositor provides starts off, and is set per account, from
`fs/etc/skel/.config/kdos/comp.conf`:

<!-- kfs:include fs/etc/skel/.config/kdos/comp.conf lines=281-327 pin=84053e7a4e1f -->

The reduce-motion switch is in the same file:

<!-- kfs:include fs/etc/skel/.config/kdos/comp.conf lines=234-265 pin=0d97438eca26 -->

Ranges and when each key applies are in
[Keys applied on reload](../../kdos/04-programs/kdos-comp.md#keys-applied-on-reload). `osk` is read
at login.

### ~/.config/kdos-comp/rc.xml

The keys that switch each aid for the session, from `fs/etc/skel/.config/kdos-comp/rc.xml`:

<!-- kfs:include fs/etc/skel/.config/kdos-comp/rc.xml lines=447-462 pin=636a1a99602f -->

The shipped file has no `<magnifier>` block, so the magnifier runs with its defaults. The block and
its five settings are in [The magnifier](../../kdos/02-user-guide/accessibility.md#the-magnifier).

### ~/.config/kdos/a11y

An empty file whose existence lets every boxed application publish to an accessibility bus.
`kdos-appbox` reads it, and `KDOS_A11Y` overrides it for one launch. From
`src/system/kdos-appbox/main.c`:

<!-- kfs:include src/system/kdos-appbox/main.c lines=133-155 pin=4bbfad7df825 -->

### Orca's launcher

The [orca](#port-orca) port writes its own desktop entry, with a **Preferences** action:

<!-- kfs:include ports/core/accessibility/orca/build.sh lines=24-42 pin=73def9cfde24 -->

## Usage

No test harness runs these aids. On the live medium, in a virtual machine with no braille display
and no microphone, BRLTTY's start, the speech commands, the magnifier, every keyboard and pointer
switch and Orca's start have been carried out; braille output, a voice actually heard, dwell
clicking and the on-screen keyboard have not.

### Braille or speech at a text console

1. Log in on `tty2` (Ctrl+Alt+F2), or work at the login prompt of an installed machine before the
   desktop starts. Once the desktop runs on `tty1`, BRLTTY has nothing there to read.
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

BRLTTY's log starts with `BRLTTY 6.9.1`, the screen driver (`lx [Linux]`) and the speech driver
(`en [eSpeak-NG]`). Speech at the console takes the sound card for itself. While it speaks, the
desktop has no sound; while the desktop's PipeWire holds the card, the console voice cannot open
it. Braille is not affected.

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

1. Press Super+= to turn it on or off. With the defaults it draws a square lens around the
   pointer, which follows it and is clipped at the screen's edges.
2. Press Super+Alt+= to magnify more and Super+Alt+- to magnify less.

The phosphor pass and direct scanout stay off while it is on. To change its size or step, add a
`<magnifier>` block to `~/.config/kdos-comp/rc.xml` and run `kdos-comp -r`.

### Keyboard aids, dwell click and the large pointer

1. For this session only, press the switch; a notification says which way it went:

   | Keys | Aid |
   |:--|:--|
   | Super+Alt+S | Sticky keys |
   | Super+Alt+L | Slow keys |
   | Super+Alt+B | Bounce keys |
   | Super+Alt+D | Dwell click |
   | Super+Alt+C | Large pointer |

2. For every session, set the key in `~/.config/kdos/comp.conf`, for example:

   ```text
   sticky_keys       = yes
   dwell_click       = yes
   dwell_click_delay = 900
   large_cursor      = yes
   ```

3. Apply the file with `kdos-comp -r` from a terminal inside the session. A reload takes a switch
   from the file only when its line has changed, so it does not undo one made by key.

What each aid does, and its limits, are in
[Keyboard aids](../../kdos/02-user-guide/accessibility.md#keyboard-aids),
[Dwell click](../../kdos/02-user-guide/accessibility.md#dwell-click) and
[The pointer size](../../kdos/02-user-guide/accessibility.md#the-pointer-size).

### The on-screen keyboard

1. Set `osk = manual` or `osk = auto` in `~/.config/kdos/comp.conf`.
2. Log out and back in; `osk` is read at login.
3. With `manual`, press Super+Alt+K to show and hide it. With `auto`, it also appears whenever a
   text field of a [Wayland]{.idx} application has the focus.

With the shipped `osk = off`, the compositor logs `wvkbd-deskintl: off in comp.conf` at login and
does not start the keyboard, and Super+Alt+K posts a notification that says so instead.

`auto` needs [`fcitx5`]{.idx}, which the session starts when it is installed. Applications under [Xwayland]{.idx}
and Qt 5 applications do not raise it; use Super+Alt+K.

### Pointing from the keyboard

Open **Keyboard Pointer** from the Start menu, or run `wl-kbptr`, then type the label of the
region to move to. `wl-kbptr -o modes=floating,click -o mode_floating.source=detect` finds the
clickable regions itself. Its example configuration is in
`/usr/share/doc/wl-kbptr/config.example`. The keys that narrow the choice and click are in
[Pointing from the keyboard](../../kdos/02-user-guide/accessibility.md#pointing-from-the-keyboard).

### Writing with Dasher

Open **Dasher** from the Start menu, or run `dasher`, and steer the pointer into the letters as
they stream towards it. Dasher always runs under Xwayland, and its direct mode types only into
other X11 windows; for a Wayland window, write in Dasher's own text box and paste. See
[Writing with Dasher](../../kdos/02-user-guide/accessibility.md#writing-with-dasher).

### Reducing motion

1. Set `motion = no` in `~/.config/kdos/comp.conf`.
2. Run `kdos-comp -r`. The desktop's own programs read the line at each animation, so they follow
   it without the reload.

### Reading native applications with Orca

1. Run `orca` from a terminal, or open **Orca Screen Reader** from the Start menu. Nothing starts
   it at login.
2. Open its preferences with `orca --setup`.

Orca reads the windows of native GTK and Qt applications. It does not read the panel, the menus,
Settings or any other window KDOS draws. `orca -l` lists the applications on the accessibility bus
that Orca can see, which on a bare live session is `xdg-desktop-portal-gtk` and Orca itself.
Orca starts on the live medium; it has not been heard reading an application.

### Letting a boxed application be read

1. For every box:

   ```bash
   touch ~/.config/kdos/a11y
   ```

2. Or for one launch:

   ```bash
   KDOS_A11Y=1 kdos-appbox run hugin
   ```

3. Install a screen reader inside the box if you want one there; `kdos-box enter <box>` opens a
   shell in it. The catalogue ships none.

`KDOS_A11Y=0` turns it off for one launch even when the file exists. The setting is read when the
application starts, so one already running keeps what it started with.

## Tips

- Every accent meets the same contrast floors, and `paper` is the one light accent:
  `kdos theme paper`. See [Changing the accent](fonts-and-theming.md#changing-the-accent) and
  [Colour and contrast](../../kdos/02-user-guide/accessibility.md#colour-and-contrast).
- Larger text and a larger pointer are separate settings: `chrome_font` and `panel_font` in
  `comp.conf` set the size of the desktop's text, `cursor_size` and `large_cursor_size` the
  pointer's. See [The size of the desktop's text](displays-and-scaling.md#the-size-of-the-desktops-text).
- `foot` and `kdos-term` change their text size per window with Ctrl+=, Ctrl+- and Ctrl+0.
- A keyboard that misbehaves for no visible reason may have an aid switched on by accident. The
  same Super+Alt key switches it off again. Slow keys leaves Super and Alt alone, but with it on
  the letter of that chord has to be held for `slow_keys_delay` (300 ms by default) before it
  counts, so a quick press does nothing.
  Each switch logs `a11y: Sticky keys on` and the like in `$XDG_RUNTIME_DIR/kdos-comp.log`.
- The desktop's keyboard shortcuts are listed in
  [Keyboard shortcuts](../../kdos/02-user-guide/desktop.md#keyboard-shortcuts).

## Troubleshooting

**`brltty` never starts at boot.**
`/etc/brltty.conf` is missing or empty. Read `/run/kdos-init.brltty.log` for the `[SKIP]` line,
write the file, and run `sudo service start brltty`.

**BRLTTY runs, and there is no speech.**
The desktop holds the sound card. Speech at the console works while no graphical session's
PipeWire has the card open, for instance before the desktop starts or after logging out of it.
Drop the `speech-driver` line for braille only, or stop the service with
`sudo service stop brltty`.

**`service status brltty` answers `[FAIL] brltty  (exited with code 134; restarting)`.**
BRLTTY aborts and the supervisor starts it again every five seconds; after the second failed start
in a row the line reads `[FAIL] brltty  (exited with code 134, 2 failed starts in a row;
restarting)`, counting up. Read its last words with
`sudo tail /run/kdos-svc.brltty.log`. `Assertion failed: !snd_interval_empty(i)` there means the
voice's sound device is a `plug` stacked on another `plug`: check that `kdos_card` in
`/etc/alsa/conf.d/99-kdos-pipewire.conf` is `type empty`, as shipped, and that
`KDOS_ALSA_DEFAULT=kdos_card aplay /usr/share/sounds/alsa/Front_Center.wav` plays as root.

**A BrlAPI client such as `brltty-clip` is refused.**
The account is not in `brlapi`: check with `id`, then add it as above and log in again.

**The magnifier keys do nothing.**
The account's `rc.xml` lacks the bindings: it is copied from `/etc/skel` once, when the account is
made, and never updated. Add the three `<keybind>` lines after `<default />` in the `<keyboard>` section, check the
file with `xmllint --noout ~/.config/kdos-comp/rc.xml`, and run `kdos-comp -r`.

**`kdos-comp -r` prints `LABWC_PID not set`.**
It was run from a shell the compositor did not start, such as `tty2` or SSH. Run it in a terminal
inside the session, or send the compositor the signal directly:

```bash
pkill -HUP -x kdos-comp
```

**The on-screen keyboard never appears in `auto`.**
Check that `fcitx5` is running: `kdos doctor` reports it in its Desktop section. The application
must be a Wayland one; under Xwayland use Super+Alt+K.

**Orca says nothing for a boxed application.**
The box was started without `~/.config/kdos/a11y` or `KDOS_A11Y=1`, so it was given
`NO_AT_BRIDGE=1` and `GTK_A11Y=none`. Whether the host's Orca reaches a box's own bus has not been
tried.

## Known gaps

- [A screen reader reads the applications, not the desktop's own windows](../../kdos/06-reference/known-gaps.md#a-screen-reader-reads-the-applications-not-the-desktops-own-windows)
- [There is no graphical login screen (greeter)](../../kdos/06-reference/known-gaps.md#there-is-no-graphical-login-screen-greeter):
  the login is the text prompt, which BRLTTY can read
- [X11 and Qt 5 applications get no input method](../../kdos/06-reference/known-gaps.md#x11-and-qt-5-applications-get-no-input-method):
  nor the on-screen keyboard's automatic raise
- [The control centre has no pointer page](../../kdos/06-reference/known-gaps.md#the-control-centre-has-no-pointer-page)
- [Every program speaks English](../../kdos/06-reference/known-gaps.md#every-program-speaks-english)
- [Features whose library has no port](../../kdos/06-reference/known-gaps.md#features-whose-library-has-no-port):
  BRLTTY's Libbraille driver

## See also

- [Accessibility](../../kdos/02-user-guide/accessibility.md): every aid, described in full
- [Accessibility](../../kdos/04-programs/kdos-comp.md#accessibility) in the compositor's chapter:
  how the aids and the keyboard monitor are built
- [Motion](../../kdos/04-programs/kdos-comp.md#motion): what `motion = no` stops
- [kdos-appbox](../../kdos/04-programs/kdos-appbox.md): the environment a box is given
- [polkit, and why the desktop has no authentication agent](../../kdos/03-architecture/security-model.md#polkit-and-why-the-desktop-has-no-authentication-agent)
- [Services](services.md), [Fonts and theming](fonts-and-theming.md),
  [Displays and scaling](displays-and-scaling.md), [Screen lock](screen-lock.md),
  [Live persistence](live-persistence.md)

<!-- book-nav -->
---

*Part XVI. System administration, chapter 417.* Previous: [416. Fonts and theming](fonts-and-theming.md) · [Contents](../README.md) · Next: [418. Portals and screen sharing](portals-and-screen-sharing.md)
