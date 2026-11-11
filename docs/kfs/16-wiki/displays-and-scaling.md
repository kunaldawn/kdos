# Displays and scaling

KWin drives every screen, and KScreen is how a person arranges them: resolution, refresh rate,
position, rotation, scale and which screen is primary. This page covers arranging the screens
from System Settings and from a prompt, the size of the desktop's text, Night Light, the
brightness of a laptop panel and of an external monitor, and what to check when a screen is
missing or blurry. How KWin draws the screens is in [KWin](../../kdos/04-programs/kwin.md).

## Installation

| Port | Provides |
|---|---|
| [kwin](#port-kwin) | Output management: modes, scale, rotation, HDR and colour profiles, and Night Light |
| [kscreen](#port-kscreen) | The **Display Configuration** page, the screen-layout OSD on `Meta+P`, the KDE daemon module that applies a saved layout when screens change, and `kscreen-doctor` |
| [knighttime](#port-knighttime) | The day and night schedule Night Light follows |
| [powerdevil](#port-powerdevil) | Screen brightness, a laptop screen's own and an external monitor's over DDC/CI |
| [ddcutil](#port-ddcutil) | The DDC/CI library PowerDevil uses for an external monitor |
| [kdos-tools](#port-kdos-tools) | `kdos toggle night-light` |

Nothing is enabled by hand; KWin finds every connected screen at login, and the greeter's KWin
does the same before it.

## Configuration

### Where a layout is kept

KWin keeps every screen arrangement it has seen in `~/.config/kwinoutputconfig.json`, keyed by the
set of connected screens, and applies the matching one when that set comes back. The greeter's
KWin keeps its own in `/var/lib/sddm/.config/`. There is no system-wide layout file.

### The size of the desktop's text

Two settings make text larger. The **scale** of a screen, on Display Configuration, scales
everything on it, in steps of 5 % from 50 % to 300 %; KWin renders a fractional scale directly for
programs that support it and scales the rest. The **font size**, on System Settings → **Text and
Fonts** → **Fonts**, makes text alone larger and leaves the rest of each window as it was.

### Night light

System Settings → **Night Light** sets when the screen warms and how much: always on, from sunset
to sunrise at the machine's location or at fixed times. KNightTime works the schedule out and KWin
follows it. `Meta+Ctrl+Shift+N`, a chord KDOS adds (see
[Shortcuts and window rules](shortcuts-and-window-rules.md)), runs `kdos toggle night-light`,
which writes `[NightColor]` `Active` in `~/.config/kwinrc`, and on the way on `Mode=Constant`:
the screen warms at once and all day, in place of the schedule, until it is turned off or a
schedule is chosen on the page again.

### The DDC/CI line of an external monitor

PowerDevil changes an external monitor's brightness over its DDC/CI line, through libddcutil,
which opens the monitor's `/dev/i2c-N` node. KDOS grants that node to the `video` group and to the
user of the active session, and to nothing else, from
`fs/etc/udev/rules.d/70-kdos-i2c.rules`:

<!-- kfs:include fs/etc/udev/rules.d/70-kdos-i2c.rules lines=39-39 pin=74ce37d36ea2 -->

The comment above the rule explains the class match and the `uaccess` tag, which elogind turns
into an access-control entry for whoever's session is in front.

## Usage

### Arranging the screens

1. System Settings → **Display Configuration**, or `Meta+P` for the quick layouts (extend,
   mirror, only one screen).
2. Drag the screens into place; pick each one's resolution, refresh rate, rotation and scale; set
   the primary screen, which holds the panel.
3. **Apply**. KWin asks whether to keep the change and reverts it after 15 seconds without an
   answer, so a mode the screen cannot show undoes itself.

### Listing and changing the screens from a prompt

```bash
kscreen-doctor --outputs                       # every output, its modes, scale and position
kscreen-doctor output.HDMI-A-1.mode.1920x1080@60
kscreen-doctor output.eDP-1.scale.1.5
kscreen-doctor output.HDMI-A-1.position.1920,0
kscreen-doctor output.HDMI-A-1.disable
```

The command works from Konsole, and from a console or SSH login of the same user, against the
running session. The output names are KWin's, as `--outputs` prints them.

### Larger text on a high-density screen without scaling

Leave the scale at 100 % and raise the font size under **Text and Fonts**, or set **Force fonts
DPI** there. Windows keep their pixel sizes and only the text grows; a program that draws its own
text at fixed sizes is not affected.

### Night light

Turn it on and choose the schedule on System Settings → **Night Light**. From a prompt, or with
`Meta+Ctrl+Shift+N`, it is on, warm all day, or off:

```bash
kdos toggle night-light
```

### The brightness of an external monitor

Use the brightness slider in the system tray's Brightness and Color applet, or the keyboard's
brightness keys: each screen that answers on DDC/CI has its own slider. A monitor that does not
answer has DDC/CI turned off in its own menu, or is connected through a dock or adapter that does
not pass it on.

```bash
ddcutil detect          # which monitors answer on DDC/CI
```

### The live medium and an installed system

Both find the screens the same way. The live medium starts every screen at its preferred mode and
the scale KWin chooses for its size; an arrangement made there lasts until power-off unless the
medium has a persistence store.

## Tips

- A virtual machine's screen is resized from the viewer when the guest has the virtio or QXL
  display: KWin follows the new size. See [Virtual machines](virtual-machines.md).
- `qdbus6 org.kde.KWin /KWin supportInformation` prints, among much else, every output with its
  mode, scale and the renderer KWin uses.
- The login screen has its own arrangement. **Apply Plasma Settings** on System Settings →
  **Login Screen (SDDM)** copies yours to it.

## Troubleshooting

**A screen is black or missing.** Check that KWin sees it: `kscreen-doctor --outputs`. A connected
output listed as disabled is turned on with `kscreen-doctor output.<name>.enable`. One not listed
is not seen by the kernel either: check `cat /sys/class/drm/*/status` and the cable, and see
[Graphics drivers](graphics-drivers.md).

**Text is blurry on a scaled screen.** The program is an X11 one running under Xwayland, which
draws at 100 % and is scaled up. System Settings → **Display Configuration** → **Legacy
Applications (X11)** → **Apply scaling themselves** lets such programs draw at the real size; see
[Xwayland and X11 applications](xwayland-and-x11-apps.md).

**The external monitor has no brightness slider.** `ddcutil detect` as your own user. "No
monitor detected" with `sudo ddcutil detect` finding one means the node's permission: `ls -l
/dev/i2c-*` must show group `video` and an ACL (`getfacl /dev/i2c-N`) for the active user.

**The layout is wrong after plugging a dock in.** KWin applies the last layout it saw for that set
of screens. Arrange it once and **Apply**; it is kept for that set.

## Known gaps

- [Known gaps](../../kdos/06-reference/known-gaps.md): the desktop's open items

## See also

- [KWin](../../kdos/04-programs/kwin.md): outputs, scaling and the renderer
- [Graphics drivers](graphics-drivers.md), [Laptops](laptops.md),
  [Power management](power-management.md), [Fonts and theming](fonts-and-theming.md)

<!-- book-nav -->
---

*Part XVI. System administration, chapter 423.* Previous: [422. Audio](audio.md) · [Contents](../README.md) · Next: [424. Graphics drivers](graphics-drivers.md)
