# Fonts and theming

KDOS Modern is the look of KDOS on Plasma: eight accents, each a colour scheme and a global theme,
the `kdos` Plasma style, the KDOS icons, the square Breeze title bar, Noto Sans for the interface
and JetBrains Mono for fixed-width text. This page covers changing the [accent]{.idx} and what it
reaches, Phosphor mode, the fonts and how fontconfig chooses them, and how GTK, Qt 5 and boxed
applications follow the theme. Why the look is built this way is in
[Theming](../../kdos/02-user-guide/theming.md) and [The design language](../../kdos/03-architecture/design-language.md).

## Installation

| Port | Provides |
|---|---|
| [kdos-plasma-look](#port-kdos-plasma-look) | The eight colour schemes `KDOS<Title>.colors`, the eight global themes `org.kdos.<accent>.desktop`, the `kdos` Plasma style, the desks, the Konsole profiles and qt5ct palettes, and the distribution's `kdeglobals` |
| [kdos-theme](#port-kdos-theme) | The generators those files are made with, from the one palette in libkcolor |
| [kdos-icons](#port-kdos-icons) | The `KDOS` and `KDOS-Light` icon themes |
| [kdos-wallpapers](#port-kdos-wallpapers) | The `KDOS-<Title>` wallpapers, one per accent |
| [kcm-kdos-look](#port-kcm-kdos-look) | System Settings → Colors & Themes → **KDOS Look**: the accent everywhere at once, and Phosphor mode |
| [kdos-kwin-motion](#port-kdos-kwin-motion) | Phosphor mode's window, login and logout motion |
| [kdos-tools](#port-kdos-tools) | `kdos theme` and `kdos phosphor` |
| [noto-fonts](#port-noto-fonts), [ttf-jetbrains-mono](#port-ttf-jetbrains-mono), [terminus-ttf](#port-terminus-ttf) | The interface face, the fixed-width face and the fallback for braille and box glyphs |
| [kde-gtk-config](#port-kde-gtk-config) | Carries the colours, fonts and cursor to GTK programs |
| [qt5ct](#port-qt5ct) | Carries the KDOS palette to Qt 5 programs |

The eight accents are `phosphor`, `amber`, `ice`, `bone`, `norton`, `borland`, `perfect` and
`paper`. `bone` is the default and `paper` the one light accent.

## Configuration

### The distribution's defaults

[`/usr/share/kdos/xdg/kdeglobals`]{.idx term="/usr/share/kdos/xdg/kdeglobals"}, installed by
kdos-plasma-look, holds the fonts, the terminal, the global theme and its light and dark pair,
the animation speed, and the icon and sound themes. From
`src/desktop/kdos-plasma-look/xdg/kdeglobals`:

<!-- kfs:include src/desktop/kdos-plasma-look/xdg/kdeglobals lines=31-53 pin=38e91eea0168 -->

The global theme applies the same fonts, the colour scheme, icons and widget style into
`~/.config/kdedefaults` at login, and a person's choice on any Appearance page lands in
`~/.config/kdeglobals` above both. The machine's own accent, the one a new account starts in, is
`LookAndFeelPackage` in `/etc/xdg/kdeglobals`, which the installer writes.

The same file makes KNewStuff unavailable on every machine, so no **Get New…** button appears:
the KDOS add-ons ship as the [kdos-addons](#port-kdos-addons) package instead.

### The fixed-width face

`monospace` is a fontconfig list, and the first face in it with a glyph draws that glyph. KDOS
puts JetBrains Mono first, Noto Sans Mono second for the Latin, Greek and Cyrillic it lacks, and
Terminus third for braille patterns and box drawing, from `fs/etc/fonts/conf.d/45-kdos-mono.conf`:

<!-- kfs:include fs/etc/fonts/conf.d/45-kdos-mono.conf lines=45-57 pin=32b406aa9868 -->

### The accent outside the session

The screens before a login, which are root's, wear the machine's accent from
`/etc/kdos/accent`: the boot menu, the splash, the text consoles' palette `/etc/vtrgb`, and the
SDDM greeter, whose files `kdos-powerd` copies into the `sddm` account. `kdos theme` asks
`kdos-powerd` to change them, which it does for root and members of `wheel`, and for the person at
the machine when polkit allows it.

## Usage

### Changing the accent

1. System Settings → Colors & Themes → **KDOS Look**, and pick a swatch; or

   ```bash
   kdos theme amber
   kdos theme list          # the eight, and which is in force
   kdos theme next          # or prev
   ```

2. Plasma switches to the accent's global theme and repaints. `kdos theme` also writes the
   terminal programs' own colour files in your home (`mc`, `tmux`, `yazi`, `newsboat`, git's
   delta, `fzf`, `ls` and qt5ct), switches Konsole's profile, moves the wallpaper and the lock screen's wallpaper to the accent's own
   while they still name a KDOS one, and repaints the boot menu, splash, consoles and greeter.

Picking a KDOS global theme on the stock Global Theme page does the same: `kded-kdos` notices and
runs `kdos theme --from-plasma`. A wallpaper, a Konsole profile or a colour scheme you chose
yourself is left as it is.

### Light and dark

`paper` is the light accent. System Settings → **Global Theme** has the light and dark pair,
`paper` and `bone` by default, and the automatic switch at sunset, off by default.

### Phosphor mode

Phosphor mode adds a CRT pass over the whole screen and phosphor motion to windows, the login and
the logout, and switches to the `phosphor` accent:

```bash
kdos phosphor on                    # --preset retro, --terminus for the Terminus face
kdos phosphor status
kdos phosphor off                   # puts back every key it changed
```

or the switch on the KDOS Look page. The CRT pass needs GPU rendering: on software rendering,
which a plain virtual display gives, `on` refuses before it changes anything.

### Changing the fonts

System Settings → **Text and Fonts** → **Fonts** sets the general, fixed-width, small, toolbar,
menu and title-bar fonts, antialiasing and hinting. **Adjust All Fonts** scales them together.
The change applies to KDE programs at once and to GTK programs through kde-gtk-config.

A font of your own goes in `~/.local/share/fonts/`; `fc-cache -f` makes it known at once.

### GTK, Qt 5 and boxed applications

- **GTK** programs take the colours, fonts, icons and cursor from kde-gtk-config; GTK 4 ones read
  them through the portal's Settings interface, served by xdg-desktop-portal-gtk, and GTK 3 ones
  do too under `GTK_USE_PORTAL=1`. A running GTK 3 program repaints on an accent switch, through
  the `colorreload-gtk-module` kde-gtk-config's `org.gtk.Settings` names, which
  [gtk3](#port-gtk3) is patched to read beside the portal; a GTK 4 one at its next start. File dialogs come from the
  KDE portal for GTK 4 and for GTK 3's native chooser (`GTK_USE_PORTAL=1`).
- **Qt 5** programs load the qt5ct platform theme under the name `kde` and read the KDOS palette
  `kdos theme` writes into `~/.config/qt5ct/`.
- **Boxed** GTK applications are started with `GTK_THEME` set to the theme kde-gtk-config names,
  `:dark` added for the dark variant, and read its `colors.css` from the shared home, so a running
  one takes a new accent at its next start; Qt ones read `kdeglobals` or the GTK platform theme.
  See [Theming applications inside boxes](../../kdos/02-user-guide/theming.md#theming-applications-inside-boxes).

## Tips

- `kdos theme --audit` compares the files in force with what the palette makes and lists any
  that differ.
- Reduced motion: System Settings → **General Behavior** → **Animation speed** to **Instant**,
  which also stops Phosphor mode's degauss and collapse.
- `fc-match monospace` and `fc-match -s monospace | head` show the fallback chain in force.

## Troubleshooting

**`kdos theme` prints `boot menu and splash unchanged (kdos-powerd did not answer)`.** The account
is not in `wheel` and polkit did not allow it, `kdos-powerd` is not running
(`service status powerd`), or the command ran inside a box. Plasma's part was still applied.

**A program draws in Breeze's colours.** It started outside the session's environment, with no
`/usr/share/kdos/xdg` in `XDG_CONFIG_DIRS`; see
[Session and environment](session-and-environment.md#troubleshooting). A Qt 5 program needs
`QT_QPA_PLATFORMTHEME=kde`, which every login shell sets.

**Box-drawing or braille characters look wrong in Konsole.** `fc-match -s monospace` must list
Terminus before any proportional face; check that `45-kdos-mono.conf` is in
`/etc/fonts/conf.d/` and run `fc-cache -f`.

**`kdos phosphor on` refuses.** KWin is on software rendering: `qdbus6 org.kde.KWin /KWin
supportInformation | grep -i 'OpenGL renderer'` names llvmpipe. See
[Graphics drivers](graphics-drivers.md).

## Known gaps

- [Known gaps](../../kdos/06-reference/known-gaps.md): the desktop's open items

## See also

- [Theming](../../kdos/02-user-guide/theming.md): the accents, Phosphor mode, wallpapers and fonts
- [The design language](../../kdos/03-architecture/design-language.md): the palette and the rules every surface follows
- [kdos theme](../../kdos/04-programs/kdos-command.md#kdos-theme): every option
- [Displays and scaling](displays-and-scaling.md), [Accessibility](accessibility.md),
  [Locale and console](locale-and-console.md)

<!-- book-nav -->
---

*Part XVI. System administration, chapter 440.* Previous: [439. Input methods](input-methods.md) · [Contents](../README.md) · Next: [441. Accessibility](accessibility.md)
