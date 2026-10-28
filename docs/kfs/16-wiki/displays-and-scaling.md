# Displays and scaling

This page covers the screens of a KDOS desktop: arranging one or more monitors, choosing a mode,
a rotation and a [scale]{.idx}, keeping that layout across logins and hotplugs, making the desktop's
text larger without scaling, warming the colours at night, and setting the brightness of an
external monitor. The output configuration itself belongs to the compositor; how it picks a mode
and when a new screen is configured is explained in
[Frame pacing and the output mode](../../kdos/04-programs/kdos-comp.md#frame-pacing-and-the-output-mode),
and why every KDOS surface is sized by its font is in
[Larger text](../../kdos/02-user-guide/accessibility.md#larger-text).

## Installation

| Port | Provides |
|:--|:--|
| [kdos-comp](#port-kdos-comp) | The compositor. It takes screen configuration over `wlr-output-management`, and runs `kdos-display --apply` whenever a screen appears |
| [kdos-shell](#port-kdos-shell) | [`kdos-display`]{.idx}, the screen arranger, and [`kdos-osd`]{.idx}, the volume and brightness overlay on the media keys, which also sets a laptop panel's brightness. Both are names of the `kdos-shell` binary |
| [wlroots](#port-wlroots), [libdisplay-info](#port-libdisplay-info) | The display back end under the compositor, and the EDID parser that reads what each monitor says it can do |
| [ddcutil](#port-ddcutil) | Brightness, contrast and input selection on an external monitor, over its DDC/CI line |
| [wl-mirror](#port-wl-mirror) | One screen mirrored into a window, for a projector |

All of them are on the image, live and installed. None has a service script: screen
configuration happens inside the desktop session, and is per account. `kdos-display` and
`kdos-osd` are among the links the `kdos-shell` recipe makes to its binary:

<!-- kfs:include src/desktop/kdos-shell/build.sh lines=133-135 pin=9486651ccac5 -->

## Configuration

### ~/.config/kdos/displays.conf

The saved layout. No file ships: `kdos-display` writes it when you keep a layout, and
`kdos-display --apply` replays it. The compositor runs that replay at login and each time a screen
appears, one second after the last one, so a dock bringing up several screens triggers one replay:

<!-- kfs:include src/desktop/kdos-comp/src/kdos-child.c lines=363-369 pin=e83210160ec5 -->

The writer, one line per screen, refresh in millihertz and scale in thousandths:

<!-- kfs:include src/desktop/kdos-shell/display.c lines=586-607 pin=2d09d2ee3ccb -->

Screens are placed edge to edge from the left, in list order (`x += lw` above), each at `y` 0. A
line that does not parse is skipped, and a screen the file does not name is left as the compositor
brought it up. The format is in
[`~/.config/kdos/displays.conf`](../../kdos/06-reference/configuration.md#configkdosdisplaysconf).
The file is always looked up under `$HOME/.config`, whatever `$XDG_CONFIG_HOME` says.

### The keys that open it

`kdos-display` is bound to `Super+P` and to a laptop's display key in the shipped
`~/.config/kdos-comp/rc.xml` (`fs/etc/skel/.config/kdos-comp/rc.xml`):

<!-- kfs:include fs/etc/skel/.config/kdos-comp/rc.xml lines=261-262 pin=0f368a5ba24b -->

To move it to another chord, see
[Changing a shipped binding](compositor-keybindings.md#changing-a-shipped-binding).

### The size of the desktop's text

KDOS draws its chrome in cells, so the font size is the size of the desktop. Two keys in
`~/.config/kdos/comp.conf` (`fs/etc/skel/.config/kdos/comp.conf`) set it, for the panel and for
the rest of the chrome:

<!-- kfs:include fs/etc/skel/.config/kdos/comp.conf lines=44-57 pin=7dfac4ae70f0 -->

<!-- kfs:include fs/etc/skel/.config/kdos/comp.conf lines=103-121 pin=83f1ef1455a0 -->

The window title bars and the compositor's menus take their font from `<theme>` in `rc.xml`, in
points:

<!-- kfs:include fs/etc/skel/.config/kdos-comp/rc.xml lines=83-95 pin=f41f19e048ae -->

The pointer has its own size:

<!-- kfs:include fs/etc/skel/.config/kdos/comp.conf lines=316-321 pin=a7d60aef2093 -->

### Night light

Night light is a state file, `~/.local/state/kdos/toggles/night-light`, whose presence means on.
There is nothing to configure; what it does to the palette is in
[`~/.local/state/kdos/toggles/`](../../kdos/06-reference/configuration.md#localstatekdostoggles).
The shipped chord:

<!-- kfs:include fs/etc/skel/.config/kdos-comp/rc.xml lines=320-320 pin=4dbe4e0f699c -->

### The DDC/CI line of an external monitor

[`ddcutil`]{.idx} reaches a monitor through `/dev/i2c-*`, which exists only when `i2c-dev` is loaded. The
image loads it at boot from `fs/etc/modules-load.d/kdos-i2c.conf`:

<!-- kfs:include fs/etc/modules-load.d/kdos-i2c.conf -->

and `fs/etc/udev/rules.d/70-kdos-i2c.rules` gives the display controllers' buses, and only those,
to the `video` group:

<!-- kfs:include fs/etc/udev/rules.d/70-kdos-i2c.rules lines=30-30 pin=bdee605a26e7 -->

The recipe removes ddcutil's own, broader rule and its module list:

<!-- kfs:include ports/core/hardware/ddcutil/build.sh lines=25-29 pin=027e10ae720c -->

The live account `kdos` is in `video` (`fs/etc/group`). The installer carries the live account's
groups over to the account it makes, and takes out only `wheel`, and only for an account that is
not an administrator; see
[The device groups and the udev rules](../../kdos/02-user-guide/administration.md#the-device-groups-and-the-udev-rules).

## Usage

### Arranging the screens

1. Press `Super+P`, or the display key, or run `kdos-display`. It lists every screen with its
   mode, scale and state.
2. Select a screen with `↑` and `↓`. Every key from here edits a plan, and nothing is applied yet.
3. Press `m` and choose a mode from the list the monitor published. `Space` switches the screen
   on or off, `s` cycles the scale through 1, 1.5 and 2, `t` cycles the rotation and its mirrored
   forms, and `[` and `]` move the screen left or right in the order.
4. Press `Enter`. The plan is applied and a fifteen-second countdown starts; the footer then reads
   `K keep | R revert | Esc revert`.
5. Press `K` to keep it. That writes `~/.config/kdos/displays.conf`, for a single screen kept at
   scale 1.5 in a virtual machine:

   ```text
   output Virtual-1 mode 1280x800@74994 scale 1500 transform 0 pos 0 0
   ```

   Press `R` or `Esc`, or let the countdown run out, to go back to the arrangement from before
   `Enter`.

The keys are listed in [Displays](../../kdos/02-user-guide/desktop.md#displays). Settings opens
the same program from the *Displays…* row of its Desktop page.

### Listing the screens from a prompt

```bash
kdos-display --list      # name, mode, on/off, scale, transform, position, logical size, description
kdos hey outputs         # the compositor's own view: name, size, scale and position
```

`kdos-display --list` prints one tab-separated line per screen; in a virtual machine:

```text
Virtual-1	1280x800@75*	on	scale=1.00	transform=normal	at=0,0	logical=1280x800	Red Hat, Inc. QEMU Monitor (Virtual-1)
```

The second column is the mode in use, its refresh rounded to whole hertz; a `*` after it means
that mode is the one the monitor itself prefers. `kdos hey outputs --json` prints the
compositor's reply as it arrived, `{"ok":true,"outputs":[{"name":"Virtual-1","w":1280,"h":800,"scale":1.000,…}]}`; see
[kdos hey](../../kdos/04-programs/kdos-command.md#kdos-hey).

### A scale the arranger does not offer

`s` offers 1, 1.5 and 2. The file takes any scale in thousandths, so a scale such as 1.25 is set
by editing it:

1. Arrange and keep the screens once with `kdos-display`, so the file exists.
2. Edit `~/.config/kdos/displays.conf` and change the screen's `scale` field, for example to
   `scale 1250`.
3. Run `kdos-display --apply`. It prints nothing on success.

The look of KDOS's own surfaces at a fractional scale has not been seen on a screen; see
[Known gaps](#known-gaps).

### Larger text on a high-density screen without scaling

1. Edit `~/.config/kdos/comp.conf` and set, for example, `chrome_font = Terminus:pixelsize=64`
   and `panel_font = Terminus:pixelsize=32`. Choose sizes Terminus draws exactly, as the
   comments above list them: a bitmap size up to 32, or 64 and 96, which are its 32 size doubled
   and tripled.
2. For the title bars, raise the `<size>` of each `<font>` in `~/.config/kdos-comp/rc.xml`.
3. Log out and back in. Both [`comp.conf`]{.idx} keys are read once, at the start of the session.

The Appearance and Panel pages of Settings (`Super+I`) write the same two keys. Which windows they
do not reach is in [Larger text](../../kdos/02-user-guide/accessibility.md#larger-text).

### Night light

1. Press `Super+Ctrl+Shift+N`, or run `kdos toggle night-light`.
2. Run `kdos toggle` to see whether it is on. It lists every toggle with its state:

   ```text
   stay-awake     off  never save, lock or blank on idle
   night-light    on   warm the palette
   dnd            off  hold notifications back
   ```

   `kdos toggle night-light off` turns it off whatever its state.

### The brightness of an external monitor

A laptop panel's brightness is on the brightness keys; see [Laptops](laptops.md#brightness). An
external monitor is not in `/sys/class/backlight`, and its brightness is a DDC/CI setting:

1. `ddcutil detect` lists the monitors that answer, with the I2C bus of each.
2. `ddcutil getvcp 10` reads the brightness (VCP feature `10`).
3. `ddcutil setvcp 10 60` sets it to 60. Step 2 prints the monitor's maximum, which is the scale
   the value is on. With more than one monitor, add `--bus N` from step 1.

### Mirroring a screen into a window

`wl-mirror eDP-1` opens a window showing the output `eDP-1`, which can then be dragged to a
projector's screen; `wl-mirror --fullscreen-output HDMI-A-1 eDP-1` shows it full screen on
`HDMI-A-1`. Output names are the ones `kdos-display --list` prints. `wl-mirror` has not been run
against `kdos-comp`.

### The live medium and an installed system

`displays.conf` is in your home directory. On the live medium the home directory is in memory, so
a kept layout lasts until power-off unless a persistence store exists
([Live persistence](live-persistence.md),
[kdos persist](../../kdos/04-programs/kdos-command.md#kdos-persist)). On an installed system it is
on disk. In a virtual machine the outputs are the emulator's, and their modes are whatever the
virtual display offers.

## Tips

- A mode that a monitor lists may still show nothing. The countdown after `Enter` is the way back:
  wait it out with your hands off the keyboard.
- `kdos-display --apply` is safe to run at any time. With no file it does nothing.
- A screen that is switched off and kept has an `off` line of its own, so it stays off at the next
  login.
- Each screen gets its own panel and desktop icons; the workspaces are shared between screens.
- On a 4K screen at scale 1, `chrome_font = Terminus:pixelsize=64` gives the same cell as scale 2
  with the default font, drawn from the doubled bitmap rather than scaled.
- `crt = 0` in `comp.conf` turns off the phosphor pass, which costs a second swapchain per output;
  see [What it costs](../../kdos/04-programs/kdos-comp.md#what-it-costs).

## Troubleshooting

**The screen went black after `Enter`.** The monitor accepted a mode it cannot show. Do nothing:
when the countdown ends the earlier arrangement comes back. Then choose another mode.

**The saved layout is not applied at login or on plugging in.**

1. Check the file exists and names the screen: `cat ~/.config/kdos/displays.conf`, and compare the
   names with `kdos-display --list`. A monitor on another connector has another name.
2. Run `kdos-display --apply` in a terminal and read what it prints. `kdos-display: no compositor`,
   with exit status 1, means it found no Wayland display; run it from inside the session. A layout
   the compositor turned down prints `kdos-display: the compositor refused that configuration`,
   also with exit status 1, and nothing on the screens changes.
3. A line that does not match the format is skipped without a word; compare it with the writer in
   [Configuration](#configkdosdisplaysconf).

**A change to `chrome_font` or `panel_font` did nothing.** Both are read once per session: log out
and in. A line the compositor refused is written to `$XDG_RUNTIME_DIR/kdos-comp.log` with its file
name and line number:

```bash
grep comp.conf "$XDG_RUNTIME_DIR/kdos-comp.log"
```

**`ddcutil` says it cannot open `/dev/i2c-N`, or finds no monitor.**

1. `lsmod | grep i2c_dev`. If it is missing, `sudo modprobe i2c-dev` and check
   `/etc/modules-load.d/kdos-i2c.conf`.
2. `ls -l /dev/i2c-*`. The display controller's buses are group `video`; the chipset's SMBus is
   left to root on purpose.
3. `id`. Without `video` in the list, add the account: `sudo usermod -aG video <user>`, then log in
   again; see [Adding an account to a group](udev-and-device-groups.md#adding-an-account-to-a-group).
4. Some monitors turn DDC/CI off in their own on-screen menu.

`ddcutil detect` saying `No /sys/bus/i2c buses that might be used for DDC/CI communication found.`
means the display controller published no I2C bus at all, which is what a virtual machine's
emulated GPU does: there is no monitor line to talk to.

**Night light did not change a window.** Every KDOS cell surface applies it with the accent: a
long-lived one on the reload signal `kdos toggle` sends, a `kdos-shell` dialog on its next pass, the
lock screen each time it locks. The compositor's window frames and wallpaper, and every boxed or
native application, draw their own colours and do not follow it.

## Known gaps

- [KDOS's own surfaces have not been seen at a fractional scale](../../kdos/06-reference/known-gaps.md#kdoss-own-surfaces-have-not-been-seen-at-a-fractional-scale):
  the panel, the menus and the other KDOS surfaces are drawn at the device size on a scale such
  as 1.5, but none has been photographed that way, so the result is unconfirmed
- [The phosphor shader is not in any rig photograph](../../kdos/06-reference/known-gaps.md#the-phosphor-shader-is-not-in-any-rig-photograph)
- [Broad hardware support is not a goal](../../kdos/06-reference/known-gaps.md#broad-hardware-support-is-not-a-goal)

## See also

- [Displays](../../kdos/02-user-guide/desktop.md#displays): the arranger's keys
- [Frame pacing and the output mode](../../kdos/04-programs/kdos-comp.md#frame-pacing-and-the-output-mode)
  and [When an output appears](../../kdos/04-programs/kdos-comp.md#when-an-output-appears)
- [The fractional scale](../../kdos/05-developer/c-libraries.md#the-fractional-scale): how a
  surface draws at 1.5
- [Larger text](../../kdos/02-user-guide/accessibility.md#larger-text) and
  [The pointer size](../../kdos/02-user-guide/accessibility.md#the-pointer-size)
- [The system surfaces](../../kdos/04-programs/kdos-shell.md#the-system-surfaces): `kdos-display`
- [Configuration](../../kdos/06-reference/configuration.md#configkdosdisplaysconf): `displays.conf`
- [Graphics drivers](graphics-drivers.md): which driver drives a GPU, and checking that the
  desktop is accelerated
- [Laptops](laptops.md): the lid, panel brightness and docks over Thunderbolt

<!-- book-nav -->
---

*Part XVI. System administration, chapter 399.* Previous: [398. Audio](audio.md) · [Contents](../README.md) · Next: [400. Graphics drivers](graphics-drivers.md)
