## About

`kdos-plasma-look` is KDOS Modern for Plasma: the data that makes a Plasma session read as KDOS,
generated at build time from the one palette in libkcolor. It installs

- the eight colour schemes, `/usr/share/color-schemes/KDOS<Title>.colors`;
- the eight global themes, `/usr/share/plasma/look-and-feel/org.kdos.<accent>.desktop`, one per
  accent, each naming its scheme, the `KDOS` icon theme (`KDOS-Light` for the light accent),
  `breeze_cursors`, the square Breeze decoration, the `kdos` Plasma style, the fonts, its own
  KSplash and the `KDOS-<Title>` wallpaper;
- the Plasma style `kdos`, `/usr/share/plasma/desktoptheme/kdos`: square, opaque panel, popup,
  tooltip, button, task-manager and pager frames with a one-pixel accent rule, falling back to
  Breeze for everything it does not draw;
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
  `plasmarc` and `kcminputrc` the SDDM greeter reads from the `sddm` account's home, and a
  `background.png` link to the accent's dark wallpaper. The greeter reads no light or dark setting,
  so the light accent's directory carries its dark partner's files.
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
selected row and the KDOS desk's panel; the splash preview is the KSplash's settled frame at half
its stages. They are generated rather than captured, so they are exact for every accent and need
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
QML. The motion is KDOS Modern's, a fade out of black with the mark flat; in Phosphor mode the
CRT pass is KWin's and lies over the splash like everything else. The recipe hash covers this
port and `src/libs`, not `src/art/kdos-splash`, so a new penguin reaches the splash when this port
is next rebuilt.

**Desks.** A desk template must be in the `panel` containment category, because `loadTemplate()`
finds templates only through it, and must carry `"NoDisplay": true`, because Add Panel lists every
panel template without it. `genlook.py` refuses a desk that is missing either. Each global theme's
`layouts/org.kde.plasma.desktop-layout.js` lays out the desk named by `kdosrc [Desk] Layout`, so
the Global Theme page and `kdos desk` always agree, and records when it did in `[Desk] LaidOut`;
`kdos desk` records a digest of the panels it laid out in `[Desk] LaidOutPanels` and compares it
with the panels now to tell whether they were changed since.
The pinned apps in the desks are the first words of `~/.config/kdos/favorites`, the list of boxes
warmed at login; `testing/lnfcheck.py` compares the two.

**The Plasma style.** It draws only the panel, popups, tooltips, buttons, the task manager and
the pager, with `FallbackTheme=default` for the rest, and ships no `colors` file, so it follows whichever
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
