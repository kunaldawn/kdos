## About

`kdos-plasma-look` is KDOS Modern for Plasma: the data that makes a Plasma session read as KDOS,
generated at build time from the one palette in libkcolor. It installs

- the eight colour schemes, `/usr/share/color-schemes/KDOS<Title>.colors`;
- the eight global themes, `/usr/share/plasma/look-and-feel/org.kdos.<accent>.desktop`, one per
  accent, each naming its scheme, the `KDOS` icon theme (`KDOS-Light` for the light accent),
  `breeze_cursors`, the square Breeze decoration, the `kdos` Plasma style, the fonts, its own
  KSplash and the `KDOS-<Title>` wallpaper;
- the Plasma style `kdos`, `/usr/share/plasma/desktoptheme/kdos`: square, opaque panel, popup,
  tooltip, button, task-manager and pager frames with a one-pixel accent rule, and the slider,
  progress bar, radio button, check marks and switch in the accent, falling back to Breeze for
  everything it does not draw;
- the three desks, `/usr/share/plasma/layout-templates/org.kdos.desk.{kdos,dock,tiling}`;
- the Konsole schemes and profiles `KDOS-<Title>` and the qt5ct palettes `KDOS-<Title>.conf`;
- the login screen's files for each accent, `/usr/share/kdos/greeter/<accent>/`, which
  `kdos-powerd` copies into the `sddm` account when the machine's accent changes.
- the distribution's `kdeglobals`, `/usr/share/kdos/xdg/kdeglobals`: the fonts, the terminal, the
  global theme and its light and dark pair, the animation speed, the icon and sound themes and the
  KNewStuff lock, with the default accent's colours merged in.

The colour schemes, the Konsole files, the qt5ct palettes and the greeter files are written by
[kdos-theme](#port-kdos-theme)'s generators; everything else by `genlook.py`, beside the recipe.
How the accents are switched is [`kdos theme`](../../../docs/kdos/04-programs/kdos-command.md#kdos-theme).

## Flags

- `kdos-theme kde-colors` — The colour schemes, all eight accents, into the package tree. The
  Konsole files and the qt5ct palettes are the `konsole` and `qt5ct-colors` generators beside it.
- `kdos-theme contrast -q` — Re-reads the installed schemes and fails the build when any text
  role falls below 4.5:1 on either background of its set, or a decoration below 3:1. `-q` prints
  only the failures, so the build log names what broke.
- `kdos-theme greeter-globals --sddm` — One directory per accent holding the `kdeglobals`,
  `plasmarc` and `kcminputrc` the SDDM greeter reads from the `sddm` account's home, and
  `background.png`, the accent's dark wallpaper dimmed until the greeter's text holds 4.5:1 on its
  brightest pixel. It reads the wallpapers [kdos-wallpapers](#port-kdos-wallpapers) installed,
  which is why that port is in `depends`. The greeter reads no light or dark setting, so the light
  accent's directory carries its dark partner's files.
- `kdos-theme greeter-globals "$LOOK/kdos/xdg/kdeglobals"` — Merges the default accent's
  `[Colors:*]` and `[ColorEffects:*]` groups, the colour keys of `[WM]` and `[General] ColorScheme`
  into the installed `kdeglobals`, leaving every other line as written. KColorScheme reads colours
  from those groups and never from the scheme file `ColorScheme` names, so without them a program
  started outside a Plasma session (pkexec, an early autostart) draws Breeze. With no accent named
  the generator takes libkcolor's default, so the file follows the table.
- `kdos-theme contrast -q "$LOOK/kdos/xdg/kdeglobals"` — The same contrast gate as the schemes',
  on the merged file.
- `--accents "${accents[@]}"` — The accent list, in libkcolor's table order, read from
  `kdos-theme accents`, the one program that holds the table. `genlook.py` fails on an accent with
  no scheme file, so a ninth accent in the table cannot ship without its global theme.
- `--splash "$PORT_SRC/../../art/kdos-splash"` — Where the penguin, the block wordmark and the
  mascot's own colours are read from: kdos-splash's `penguin.h`, and the wordmark, the two lines
  under it, `peng_pal[1]`, `C_GLOW` and the wordmark's lower-row mix in `kdos-splash.c`. The
  KSplash and the previews draw the picture the boot splash draws, from the same bytes.
- `--schemes` — The colour schemes `genlook.py` reads every colour from: the role a frame, a
  preview or a placeholder needs is looked up in the generated `.colors` file, and no colour is
  written anywhere else.

## Notes

**Fonts.** Every global theme sets Noto Sans 10 for the interface and JetBrains Mono 10 as the
fixed face. `KLookAndFeelManager` applies the `[kdeglobals][General]` font keys but decides
whether a package has fonts at all from `[kdeglobals][WM]`, so each package carries a WM `font`
key as well; without it the fonts are never applied.

**Light and dark.** An accent is light when KDE says so: `qGray()` of its Window background at 192
or above, the rule the Global Theme page uses. The light accent gets `KDOS-Light` icons.

**Previews.** `preview.png`, `fullscreenpreview.jpg` and `splash.png` are drawn by `genlook.py`
from the scheme's own roles: the desktop preview shows the mark on the ground, a window with a
selected row and the KDOS desk's panel; the splash preview is the KSplash's settled frame at its
third stage. They are generated rather than captured, so they are exact for every accent and need
no session. The two small ones are drawn at three times their size and reduced, so the one-pixel
rules come out as a tint rather than vanishing. `cjpeg` writes the JPEG, because the package
structure names that file `fullscreenpreview.jpg`; a global theme missing any of the three shows
Breeze's picture of another desktop on the Global Theme or Splash Screen page.

**KSplash.** `Splash.qml` is copied into every package, never linked: whether KPackage follows a
symbolic link out of a package is not something the theme should depend on. It repeats
kdos-splash's `layout()` arithmetic in device pixels, so at 1x and 2x the penguin and the wordmark
land on the pixels the boot splash left them on, and its colours are the running scheme's roles.
The three colours that are no scheme's (the mascot's body, the colour its rim is mixed towards,
and the black of a screen with nothing on it, which KSplash's transparent window would otherwise
show the starting desktop through) come from the generated `mark.js`, never from a literal in the
QML. Its bar carries on from the boot splash's: `genlook.py` reads `BAR_SEGS` and `draw_bar()`'s
hold (99% and one segment short, where an unfinished boot stops and where the bar stands when `rcS`
hands the screen over) out of `kdos-splash.c` into `mark.js`. KSplash's bar starts at the handover
value and never passes the hold, and its six stages share out whatever lies between; the two are
the same, so the bar stands at 99% with its leading segment pulsing until the desktop is up and
never moves backwards at the login; a `kdos-splash.c` whose hold or bar
`genlook.py` cannot read fails the build. Its text is set in `Terminus (TTF)`, the scalable face
of the Terminus font the boot splash's `splash.psf` is, sized so a column is the boot splash's
8-pixel glyph; without that face it falls back to the fixed-width font. The motion is KDOS Modern's, a fade out of black with the mark flat; in Phosphor mode the
CRT pass is KWin's and lies over the splash like everything else. The recipe hash covers this
port and `src/libs`, not `src/art/kdos-splash`, so a new penguin reaches the splash when this port
is next rebuilt.

**Desks.** A desk template must be in the `panel` containment category, because `loadTemplate()`
finds templates only through it, and must carry `"NoDisplay": true`, because Add Panel lists every
panel template without it. `genlook.py` refuses a desk that is missing either. Each global theme's
`layouts/org.kde.plasma.desktop-layout.js` lays out the desk named by `kdosrc [Desk] Layout`, so
the Global Theme page and `kdos desk` always agree, and records when it did in `[Desk] LaidOut`;
`kdos desk` records a digest of the panels it laid out in `[Desk] LaidOutPanels` and compares it
with the panels now to tell whether they were changed since. The digest covers each panel's place,
its applets in order and the settings every desk writes into them (Start's icon and label, the
pinned apps, each meter's face), and a desk whose Start has no icon, whose task manager has no
launchers or whose meter has no face reads as changed even before a digest is recorded: every desk
writes those keys, so their absence means the panels are not the ones it laid out. The clock
carries no font keys: the applet's own font fits the panel and follows the font in force, and a
manual one is sized against the screen's pixel density, which the display scale cancels. The
Tiling desk's pager shows the desktops' names without window outlines, which would be drawn over
the names on a bar that thin.
The pinned apps in the desks are the first words of `~/.config/kdos/favorites`, the list of boxes
warmed at login; `testing/lnfcheck.py` compares the two.

**The Plasma style.** It draws only the panel, popups, tooltips, buttons, the task manager, the
pager, the slider, the progress bar, the radio and check marks and the switch, with
`FallbackTheme=default`
for the rest, and ships no `colors` file, so it follows whichever
scheme is in force. Each of the panel, dialog and tooltip frames is installed twice, at its path
and under `solid/`: an opaque panel, a solid dialog and every PlasmaComponents tooltip and menu
ask for the `solid/` path by name, and without the copy they would get Breeze's. No `translucent/`
set is shipped: with KWin's blur active Plasma looks there first and, finding nothing in this
style, takes the opaque frame from the same style before any fallback. Blur, background contrast
and adaptive transparency are off in `plasmarc`. No `icons/` are shipped: Plasma 6.8 draws every
icon from the icon theme, and nothing reads a style's icon SVGs. The pager's cells carry no fill:
its desktop labels are always drawn in the text colour, which Breeze's filled current cell puts on
the light accent of a dark scheme, so the current desktop is an accent outline and bottom rule
over the panel's own ground.

The slider's fill, the progress bar's fill (the volume and brightness OSD's among them) and the
radio and check marks are the accent, `ColorScheme-ButtonFocus`, over a hollow track: Breeze's
fills them with `ColorScheme-Highlight`, the darkened selection, which on a dark accent barely
parts from its groove, and its fallback radio barely parts from the ground. The track is a faint
text-coloured outline around the ground itself, so the fill meets the ground at the focus colour's
3:1 floor. Plasma takes a style's file whole or not at all, so each of `slider.svgz`,
`bar_meter_horizontal.svgz`, `radiobutton.svgz`, `checkmarks.svgz` and `switch.svgz` holds every
element its component asks for; shipping `radiobutton.svgz` is also what moves PlasmaComponents'
radio button off the compatibility path that draws it from `actionbutton.svgz`.

The switch (`switch.svgz`) is a 36 by 18 pixel hollow groove, outlined in the radio's ring
(`ColorScheme-Text` at 60%, at least 4.4:1 against the ground on every accent), the accent filled
up to the handle when it is on, and a square handle in the button's ground. Breeze's switch parts
from a dark ground at about 1.5:1, which leaves an unchecked switch, the tray's "Raise maximum
volume" among them, all but invisible. Plasma's `SwitchIndicator` reads these elements, and
`genlook.py` stops the build when one is missing: the frames `inactive-*` and `active-*` (all
nine parts each), `hint-bar-size` (the groove's size), and `handle`, `handle-hover`,
`handle-pressed`, `handle-active`, `handle-focus` and `handle-shadow`. It shows one of the first
four at a time, so each is a whole handle, not an overlay: the ring outline at rest, the hover
colour under the pointer and with keyboard focus, a tinted handle outlined in the accent while
pressed, and the accent outline when on. With keyboard focus `handle-focus`, a two-pixel accent
outline, is drawn over the hover handle; `handle-shadow` is empty.

A popup's margins are 8 pixels and its insets 4. Kickoff spaces its header, and pads its header and
footer, by the dialog's margin less its inset, so equal values put the account picture against
the user's name.
