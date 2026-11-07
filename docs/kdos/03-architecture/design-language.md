# The design language

This chapter is the specification of how KDOS looks: the identity called **KDOS Modern** that it
paints over KDE Plasma, from the boot menu to the lock screen, and the opt-in **Phosphor mode**
layered on top of it. It covers the eight accents and the colour roles they become, the type, the
pieces Plasma loads to wear the look (colour schemes, global themes, a Plasma style, the window
decoration, icons, cursor, wallpapers, sounds), how the screens before and after a login are made
to read as one, motion, Phosphor mode, and the smaller rule set of KDOS's text-mode tools. It is
written for people adding to the look or writing KDOS code that draws, and for anyone who wants to
know why the desktop looks the way it does.

How a person switches accents and what each control changes is in
[Theming](../02-user-guide/theming.md); the code side of a settings page, widget or effect is in
[Writing desktop software](../05-developer/writing-desktop-software.md). Read
[The architecture overview](overview.md) first for where Plasma and KDOS's own components sit.

## The identity is data Plasma already loads

KDOS does not fork Plasma's look. Every piece of KDOS Modern is a file in a format Plasma already
reads, installed where Plasma already looks, so the stock settings pages (Global Theme, Colours,
Plasma Style, Window Decorations, Icons, Cursors, Splash Screen, Wallpaper, System Sounds) show
KDOS's choices beside KDE's and can change any of them. Four rules hold the kit together:

- **One palette, compiled once.** The eight accents are one table in `libkcolor`
  (`KCOL_SCHEMES` in `src/libs/libkcolor/kcolor.h`). Every colour scheme, Konsole scheme, Qt 5
  palette, wallpaper, login-screen file, boot-menu palette and console palette is generated from
  it at build time by `kdos-theme`, so no second copy of a colour ships anywhere. A colour in a
  shipped KDE file that no generator wrote is a defect.
- **Derive, never fork.** Where KDOS needs a screen KDE draws, it starts from KDE's own file and
  carries a patch with a `Subject:` line: the login screen is the Breeze SDDM theme with one patch,
  the window decoration is Breeze configured, and the lock and logout screens are Breeze's own. An
  upstream change then breaks the build, never the screen.
- **One accent is one name.** Each accent is a separately named colour scheme, `KDOS<Title>`, and
  a separately named global theme, `org.kdos.<accent>.desktop`. Plasma applies nothing when told
  to apply the scheme it already has, so an accent that changed under an unchanged name would never
  reach a running session.
- **A person's choice is never overwritten.** Distribution defaults reach an account through
  Plasma's own defaults layer, under the person's keys. When `kdos theme` writes into `~/.config`
  it moves a value only while that value still names a KDOS file (a `KDOS*` colour scheme, a
  `KDOS-*` wallpaper package, a `KDOS *` Konsole profile), so a wallpaper or profile somebody chose
  survives every accent switch.

The ports that carry it:

| Port | Installs |
|---|---|
| `kdos-theme` (`src/art`) | Nothing at run time: the build-time generator of every colour file below |
| `kdos-plasma-look` (`src/desktop`) | The eight colour schemes and global themes, the `kdos` Plasma style, the three desk layouts, the Konsole schemes and profiles, the Qt 5 palettes, the login screen's files per accent, and the distribution's `kdeglobals` |
| `kdos-icons` (`src/art`) | The icon themes `KDOS` and `KDOS-Light`, the KDOS marks, and the default cursor theme for programs that name none |
| `kdos-wallpapers` (`src/art`) | The wallpaper `KDOS-<Title>` in each accent |
| `kdos-sounds` (`src/art`) | The sound theme `kdos` |
| `kdos-sddm-theme` (`src/art`) | The login screen, `/usr/share/sddm/themes/kdos` |
| `kdos-splash` (`src/art`) | The boot splash |
| `kdos-kwin-motion` (`src/desktop`) and the `kdos_crt` effect built into `kwin` | Phosphor mode |
| `breeze`, `breeze-gtk`, `kde-gtk-config`, `ocean-sound-theme`, `ttf-jetbrains-mono` (`ports/core`) | The upstream pieces the look is built over: the decoration, the cursors, the GTK theme and its bridge, the sounds KDOS inherits, the fixed-width face |

## Colour

There are eight accents, and every colour KDOS shows comes from one of them. Each row of the table
holds nine named colours: `primary` (the accent), `dim`, `secondary`, `urgent`, `deep` (the ground
at the far end from the text: the darkest colour of a dark scheme, the lightest of a light one),
`text`, `variant`, `pdark` and `backdrop`.

| Accent | Title | Light or dark | Colour scheme | Global theme |
|---|---|---|---|---|
| `phosphor` | Phosphor | dark | `KDOSPhosphor` | `org.kdos.phosphor.desktop` |
| `amber` | Amber | dark | `KDOSAmber` | `org.kdos.amber.desktop` |
| `ice` | Ice | dark | `KDOSIce` | `org.kdos.ice.desktop` |
| `bone` | Bone | dark | `KDOSBone` | `org.kdos.bone.desktop` |
| `norton` | Norton | dark | `KDOSNorton` | `org.kdos.norton.desktop` |
| `borland` | Borland | dark | `KDOSBorland` | `org.kdos.borland.desktop` |
| `perfect` | Perfect | dark | `KDOSPerfect` | `org.kdos.perfect.desktop` |
| `paper` | Paper | light | `KDOSPaper` | `org.kdos.paper.desktop` |

`bone` is the default, named in the table by `KCOL_DEFAULT_ID` rather than taken from a position,
so reordering the table never changes what an unconfigured machine starts in. An accent is light
when its ground is (`deep` at half luma or above), the same rule Plasma's Global Theme page uses;
`paper` is the one light accent. Plasma's light and dark switch flips between the account's dark
accent and `paper`: the distribution's `kdeglobals` names `org.kdos.bone.desktop` as the dark look
and feel and `org.kdos.paper.desktop` as the light one, with automatic switching off, and `kdos
theme` moves the dark half to whichever dark accent is chosen. The Konsole scheme and profile,
the Qt 5 palette and the wallpaper of an accent are all named `KDOS-<Title>`.

### The role mapping

A KColorScheme file is eight colour sets (View, Window, Button, Selection, Tooltip,
Complementary, Header and the inactive Header), each two backgrounds and ten roles drawn on them.
`kdos-theme kde-colors` writes one file per accent, and every role is **solved** against both
backgrounds of its set rather than picked. The palette's dark end is compressed (`deep`,
`backdrop` and `variant` are within 1.19:1 of each other), so a mapping judged by eye passes on one
background and fails on the other, and the two backgrounds of a set are exactly where KDE draws
alternating rows.

| Role | Taken from |
|---|---|
| Backgrounds | View: `variant`, and `variant` 5% towards the text; Window: `deep` and `variant`; Button: 9% and 11% of the text over `deep`; Tooltip 13%; Header and Complementary 7%; the inactive Header on `deep` |
| `ForegroundNormal` | `text`; `deep` on a Selection |
| `ForegroundInactive` | The set's background walked towards its text from 56% until it clears 4.5:1 on both backgrounds |
| `ForegroundActive`, `ForegroundLink`, `ForegroundPositive` | The accent; `deep` on a Selection |
| `ForegroundVisited` | `pdark`; `deep` 30% into the accent on a Selection |
| `ForegroundNegative`, `ForegroundNeutral` | `urgent` and `secondary` |
| `DecorationFocus` | The accent |
| `DecorationHover` | 35% of the way from the focus colour to `deep` (to the accent on a Selection), backed off towards the focus colour until it holds 3:1 |
| Selection background | The accent, and for the alternate row the accent 10% towards `deep` |

**The selection is the accent under `deep`.** A selected row is the one row a person is reading,
so it is the last place a scheme may fall below the floor. `deep` on the accent measures 6.02:1 at
its worst, on `paper`; the darker accent under the plain text would measure 2.42:1 on `bone`.

Every accent-coloured role that misses its floor is walked towards the set's text one percent at a
time, so it keeps as much of its hue as the floor allows and never more. `[General] AccentColor` is
never written: Plasma's colour applicator recolours the Selection, Link and Decoration roles
whenever an accent colour is set, and what it would write are not the solved colours. For the same
reason "accent colour from wallpaper" stays off.

### Contrast is measured at build time

The floors are WCAG 2 contrast ratios: 4.5:1 for every text role, 3:1 for every decoration, each
against both backgrounds of its set, and 4.5:1 for the window titles. `kdos-theme` measures the
palette before it writes a byte and dies when a role misses its floor, and the build of
`kdos-plasma-look` runs `kdos-theme contrast` over the files as installed, so a scheme that fails is
a failed build, never an installed scheme. `testing/selftest.sh` runs the same measurement.

A process that runs outside a Plasma session (the login screen, a `pkexec` program, an early
autostart entry) reads colours only from the configuration directories, never from Plasma's
per-account defaults, and KColorScheme reads the `[Colors:*]` groups of `kdeglobals`, never the
scheme file `ColorScheme` names. So the distribution's `kdeglobals` carries the default accent's
colour groups merged in, and the `sddm` account's own `kdeglobals` carries the machine accent's;
without them those processes draw Breeze blue.

### Colour in KDOS's own code

KDOS's settings pages, widgets, effects, the welcome pages and the login screen take every colour
from a role of the colour scheme in force: `Kirigami.Theme` in QML, `KColorScheme` or the
application palette in C++, uniforms in a shader. `testing/preflight.sh` refuses a hex colour, a
`Qt.rgba()`, a named colour or a `QColor` built from numbers anywhere under `src/desktop`. The few
colours that are no scheme's are generated rather than written: the mascot's body and the colour its
rim is mixed towards, read out of `kdos-splash` at build time into the splash's `mark.js`, and the
black of a screen with nothing on it, which every handover of the power-on passes through and which
no accent may own.

## Type

| Use | Face | Where it is set |
|---|---|---|
| Interface, menus, toolbars, window titles | Noto Sans 10 | `kdeglobals` `font`, `menuFont`, `toolBarFont`, `[WM] activeFont` |
| The smallest readable text | Noto Sans 8 | `smallestReadableFont` |
| Fixed width: Konsole, Kate's editor, code | JetBrains Mono 10 (11 in the Konsole profiles) | `fixed`, and first for `monospace` in fontconfig |
| Phosphor mode's option | Terminus (TTF) | Set only by `kdos phosphor terminus on` |

Text is antialiased in greyscale with slight hinting (`XftAntialias=true`,
`XftHintStyle=hintslight`, `XftSubPixel=none`), which renders the same at every fractional scale.
The fixed face is named explicitly, because Plasma's built-in default for an unset `fixed` key is
Hack, which KDOS does not ship. `/etc/fonts/conf.d/45-kdos-mono.conf` puts JetBrains Mono first for
`monospace`, Noto Sans Mono second for the Latin Extended, Greek and Cyrillic it lacks, and Terminus
(TTF) third, the one fixed-width face on the image with every braille pattern that terminal plots
draw with, so a missing glyph costs a different fixed-width face and never a proportional one.
`testing/monofont.py` measures each block against that chain. The Konsole profiles turn ligatures
off: a terminal has to show what was typed, and JetBrains Mono would join `!=` or `->` into one
glyph.

Every global theme sets the same fonts, and carries the fonts in `[kdeglobals][WM]` as well as
`[kdeglobals][General]`: Plasma applies a theme's fonts from the General group but decides whether
the theme has fonts at all from the WM group, so a theme without the WM key never applies them.

The fixed face is KDOS's typographic signature, used where a monospace means something: the
terminal, the editor, the clock on the login screen, the text of the splash, and the KDOS Phosphor
clock widget. Window titles and the rest of the interface stay in Noto Sans.

## The pieces of the look

A **global theme** (a Plasma look-and-feel package) is what binds the pieces together, one per
accent under `/usr/share/plasma/look-and-feel/org.kdos.<accent>.desktop`, generated by
`genlook.py` in `kdos-plasma-look`. Its `contents/defaults` names:

| Piece | Value |
|---|---|
| Colour scheme | `KDOS<Title>` |
| Application style | Breeze |
| Plasma style | `kdos` |
| Icons | `KDOS`, or `KDOS-Light` for the light accent |
| Cursor | `breeze_cursors` |
| Window decoration | Breeze (`org.kde.breeze`), normal borders |
| Window placement | Centred |
| Task switcher | Thumbnail grid |
| Splash screen | The theme's own KSplash |
| Wallpaper | `KDOS-<Title>` |

Every package also carries its own splash, its desk layout script, a Kickoff setup script that
gives the launcher the KDOS mark and the label "Start", and three previews. Plasma fills any
missing file from Breeze's package, so a theme without its splash would show Breeze's; the build
refuses a package that lacks one. The previews are drawn by the generator from the scheme's roles,
not captured, so they are exact for every accent with no session to photograph.

### The Plasma style

`/usr/share/plasma/desktoptheme/kdos` draws the panel, popups, tooltips, buttons and the task
manager: square frames, opaque, with a one-pixel rule in the accent. It falls back to Breeze for
everything else and ships no colour file of its own, so it follows whichever scheme is in force.
Blur, background contrast and adaptive transparency are off. Each frame is installed at its own path
and under `solid/`, because an opaque panel, a solid dialog and every tooltip and menu ask for the
`solid/` path by name.

### The window decoration

Window frames are Breeze's decoration, configured rather than replaced: square corners, a
one-pixel outline, no gradient, titles aligned left (`/usr/share/kdos/xdg/breezerc`). The title bar
takes its colours from the scheme's window-manager colours, so it follows the accent live. A boxed
application's window carries a box glyph before its caption, from the sandbox patches KDOS carries
against KWin, KDecoration and Breeze, and its title bar is drawn in the colour scheme of its box's
accent through a KWin window rule per box; see
[Window rules KDOS writes](window-model.md#window-rules-kdos-writes). A client that draws its own
title bar, as a GTK application with a header bar does, draws neither.

### Icons

`KDOS` and `KDOS-Light` are thin themes over Breeze: each holds the KDOS penguin under the names
Plasma draws the KDE logo by (`start-here-kde`, `start-here-kde-symbolic` and the Plasma variants),
and inherits every other icon from `breeze-dark` or `breeze`. The marks themselves are in `hicolor`:
`start-here-kdos` for the launcher, `distributor-logo-kdos` and `kdos` for the About page and
`/etc/os-release`. Both themes set `FollowsColorScheme=true`, which is what makes Breeze's
monochrome icons take the accent: KIconLoader recolours an inherited icon only when the top theme
asks for it. GTK and other non-KDE toolkits do no such recolouring and draw those icons in Breeze's
own blue. The marks are raster, the one part of the icon themes that does not follow the accent.

### The cursor

One neutral pointer, `breeze_cursors`, for every accent; `/usr/share/icons/default/index.theme`
names it for programs that name none, Xwayland's among them. KWin renders a cursor theme's SVGs as
they are, so a pointer per accent would be eight cursor themes and a cursor switch per accent. No
shipped profile script exports `XCURSOR_THEME` or `XCURSOR_SIZE`: KWin prefers those variables to
the Cursors page whenever both are set, and the page would stop working.

### Wallpapers

`KDOS-<Title>` is a Plasma wallpaper package per accent: the KDOS mark on the accent's own ground
in `images_dark/`, and on the light ground in `images/`, at four sizes, so the wallpaper follows
Plasma's light and dark switch with no KDOS code. The pictures are generated from the master
artwork by `kdos-theme wallpaper`. A global theme's wallpaper is a default only for a desktop with
none set, and Plasma reads it once per desktop, so an accent applied in a running session would leave
the old wallpaper up; `kdos theme` therefore sets the wallpaper itself, on each desktop whose image
is still a `KDOS-*` package, and the lock screen's the same way. `kdos-addons` adds four more
wallpaper packages that follow light and dark, chosen from the Wallpaper page like any other.

### Sounds

The sound theme `kdos` holds the machine's own voices (the login chime, the notification blip, the
error buzz, and the device, warning and completion sounds) under the freedesktop names, synthesised
at build time by `kdos-sfx render`, and inherits Plasma's Ocean theme and then the freedesktop
theme for every name it has no voice for. `[Sounds] Theme=kdos` selects it, and the login and
logout sounds are on.

### Applications outside KDE

A GTK application wears the Breeze GTK theme, which `kde-gtk-config` keeps in the scheme's colours
by writing each account's GTK settings; KDOS writes nothing under `~/.config/gtk-*` itself. A Qt 5
application has no Plasma integration to read the scheme (there is no KDE Frameworks 5), so it is
themed by qt5ct: `kdos theme` points qt5ct at the accent's generated palette. A boxed application
gets the colour scheme and the light or dark preference through the portal's Settings interface,
and the KDE runtime of the box catalogue installs Plasma's platform theme for the Qt applications in
it.

## Where the defaults live

KDOS's defaults for KDE's settings are files in `/usr/share/kdos/xdg`, which the session puts after
`/etc/xdg` in `XDG_CONFIG_DIRS`. A program therefore reads, from strongest to weakest:

1. the account's own file in `~/.config`;
2. the account's Plasma defaults in `~/.config/kdedefaults`, written from the global theme, inside a
   Plasma session only;
3. the administrator's `/etc/xdg`;
4. KDOS's `/usr/share/kdos/xdg`.

`/etc/xdg` stays the administrator's, so an edit there outranks KDOS's default for every account
and a package update never collides with it. The installer is the one program that writes there:
the accent chosen at installation as `/etc/xdg/kdeglobals` `LookAndFeelPackage`, the desk as
`/etc/xdg/kdosrc`, and for the Tiling desk `/etc/xdg/kwinrc [Plugins] krohnkiteEnabled=true`, because
KWin loads its scripts before the first login's layout script runs. `/etc/skel` ships no panel layout or Plasma defaults file, because one
copied into a new home would bypass the global theme and be rewritten at the first login anyway;
its `kdeglobals` holds only the `DefaultDarkLookAndFeel` pairing that `kdos theme` writes. A virtual machine gets one more directory in front, `/usr/share/kdos/xdg-vm`, which
turns screen locking and display power-off off, and the live medium gets `/usr/share/kdos/xdg-live`.
The full list of files is in [Configuration](../06-reference/configuration.md).

## One look from power-on to the lock screen

The screens a person sees before the desktop are drawn by six different programs, and they are
made to read as one sequence: the same ground, the same mark in the same place, the same accent,
and full black between each and the next, so a change of display mode always falls inside a black
frame.

| Stage | Drawn by | What it shows |
|---|---|---|
| Boot menu | Limine | The menu in the accent's palette, over the KDOS artwork |
| Boot progress | `kdos-splash`, on the framebuffer | The mark fading in out of black, then the boot stages under a segmented bar; at the end of `rcS` it fades to black and lets go of the screen |
| Login screen | SDDM, with the `kdos` theme on its own KWin | Breeze's login layout with the KDOS mark, the clock in the fixed face, the footer "KDOS · Plasma by KDE", over the accent's dark wallpaper, fading in out of black and back to black on a login |
| Session start | KSplash, the global theme's own | `kdos-splash`'s last frame again, at the same pixels: the mark and the wordmark, then a segmented bar driven by the session's start-up stages |
| Desktop | Plasma | The KDOS desk in the accent |
| Lock and logout | `kscreenlocker` and Plasma's logout screen, stock Breeze | Breeze's own layout in the accent's colours, wallpaper and fonts |

The login screen and the lock screen are drawn by the same Breeze components, and the patch that
makes the Breeze SDDM theme the KDOS one changes only the mark, the clock's face, the footer and the
background, never the layout, so the two screens a person types a password into look the same. A
global theme cannot change the lock or logout screen (Plasma loads both from the shell package),
which is why they are Breeze's in KDOS's colours rather than KDOS's own.

The login screen reads no light, dark or time-of-day setting, so it always shows a dark accent:
the machine's accent, or for `paper` its dark partner. The accent reaches the screens before a
login through `kdos-powerd`, because they are root's files: `/etc/kdos/accent` for the splash,
`/etc/vtrgb` for the text console, the boot menu's palette, and the `sddm` account's
`kdeglobals`, `plasmarc` and `kcminputrc`, copied from `/usr/share/kdos/greeter/<accent>/` together
with that set's `background.png` as `/var/lib/sddm/kdos-background.png`. How `kdos theme` reaches
each of them is in [kdos theme](../04-programs/kdos-command.md#kdos-theme); the splash and the
handover are in [The splash](boot-and-init.md#the-splash).

## Motion

KDOS motion is short. `kdeglobals` sets `AnimationDurationFactor=0.5`, half of KDE's own, which
every Plasma and KWin animation scales by, and `0` turns every one of them off. That one key is the
switch for reduced motion: the Welcome tour's accessibility and look pages offer it, System
Settings' animation speed slider sets it, and every KDOS motion reads it too.

KDOS Modern adds no motion of its own past that. KWin's own effects run as KDE ships them, and the
login screen and KSplash fade in out of black rather than move. Phosphor mode adds the motion of a
CRT, every duration below scaled by the factor:

| What moves | Effect | Duration |
|---|---|---|
| A window opening out of a hairline across its middle | `kdos_unfold` | 160 ms |
| A window closing: folded to a line, the line pinched to a dot | `kdos_unfold` | 140 ms |
| The desktop opening out of a line across the splash when KSplash closes | `kdos_login` | 900 ms |
| Each window folding away while the session quits | `kdos_quit` | 450 ms |
| The whole screen collapsing to a line and a dot at lock and logout | `kdos_crt` | 350 ms, then 100 ms |
| A degauss when the colour scheme's accent changes | `kdos_crt` | 400 ms, one pulse |

Two rules hold every motion here. **Nothing waits for one**: keys and the pointer act on the end
state at once, and a motion never shows anything the end state does not. **The end state is the
picture wherever nothing moves**: with the factor at `0`, on a software renderer, and in a
screenshot, a window is where it ends up and the screen is the screen. The degauss is one pulse,
under the three flashes a second that a flashing screen may not exceed.

## Phosphor mode

Phosphor mode is the CRT layer, off by default and off for every new account, switched by one
command, `kdos phosphor on|off`, by the switch on the Welcome tour's look page, and by the
`kdos_crt` entry on System Settings' Desktop Effects page. It has no keyboard shortcut, so a stray
chord cannot flash the whole screen.

`kdos phosphor on`:

1. asks KWin whether `kdos_crt` is supported and refuses, changing nothing, when it is not: the
   effect needs GPU rendering, and a software-rendered session (a plain virtual machine, `make run`)
   prints `phosphor mode needs GPU rendering (this session renders in software)`;
2. records every key it is about to change, as the account's own file had it, in
   `~/.config/kdos/phosphor`, so `off` puts each back exactly;
3. loads `kdos_crt` and the three motion effects, and unloads KWin's own `login` and
   `sessionquit` effects, which two of them replace;
4. switches `bone` or `paper`, the two accents that do not read as a CRT, to `phosphor`, and keeps
   any other accent as chosen; `off` restores the previous one;
5. with the Terminus option, sets the fixed face and a Konsole profile to Terminus (TTF); the
   interface stays in Noto Sans, because Terminus covers Latin, Greek and Cyrillic only;
6. sets the boot splash's motion to the CRT power-on.

The effect is built into KWin by a patch KDOS carries (`kdos-crt.patch` in the `kwin` port),
because KWin refuses an effect built outside it against any other KWin release, point releases
included. It paints each output into an offscreen picture and draws it back through a CRT shader:
a horizontal bleed, scanlines, a vignette, barrel curvature and a faint floor in the accent. Two
presets set its three values in `kwinrc [Effect-kdos_crt]`:

| Preset | `Strength` | `Scanlines` | `Curve` |
|---|---|---|---|
| Subtle (the default) | 55 | 0 | 0 |
| Retro | 55 | 60 | 30 |

What it costs, and when it stands aside:

- The scene is composited on every frame of every output it draws, the hardware cursor plane and
  direct scanout are given up, and the pointer is drawn into the picture, so it curves with it.
- With `Fullscreen=false`, the default, an output showing an active fullscreen window gets direct
  scanout back, so a game or a video plays untouched.
- It refuses HDR outputs, and stands aside while zoom or the magnifier runs.
- Screenshots, screencasts and the colour picker get the plain picture.
- Retro's scanlines lower the effective contrast of the screen, which the scheme's measured floors
  do not cover; Subtle draws none.

The effect's state and its power-down are exported on KWin's bus name as `io.kdos.Crt1` at
`/io/kdos/Crt1`, which `kdos phosphor collapse|unfold|degauss` calls. How to switch it, and its
effect on accessibility, are in [Theming](../02-user-guide/theming.md).

## Desks

How the panels are laid out is part of the look, and KDOS ships three layouts, the **desks**, as
Plasma layout templates: KDOS (one bottom panel, the default), Dock (a top bar and a centred
floating dock) and Tiling (a top bar, with automatic tiling on). Every global theme's layout script
lays out the desk the account picked (`kdosrc [Desk] Layout`), never a fixed one, so applying any
accent's layout gives back the same panels. The first Welcome page offers the three, `kdos desk`
switches between them, and switching replaces the panels after copying the old arrangement aside.
The desks themselves are in [The desktop](../02-user-guide/desktop.md); what the Tiling desk does to
windows is in [The window model](window-model.md#auto-tiling).

## The text tools

A few KDOS programs draw in a terminal rather than on Plasma: the installer's text front end
(`kinstall`, for serial and unattended installs) and the build's own front end `kdosbuild` among
them. They draw with `libktui`, KDOS's cell toolkit, and follow the same palette through
`libkcolor`'s eight **slots**:

| Slot | Role | Scheme colour |
|---|---|---|
| `KT_ACCENT` | The accent | `primary` |
| `KT_ERR` | Urgent | `urgent` |
| `KT_WARN` | A caution, not a failure | `secondary` |
| `KT_TEXT` | Body text | `text` |
| `KT_MID` | Labels, secondary text, rules | `pdark` |
| `KT_DIM` | A fill | `dim` |
| `KT_SURFACE` | Raised chrome background | `variant` |
| `KT_BG` | The background | `backdrop` |

There are eight slots and no more because the console font has 512 glyphs: the Linux console uses
the foreground intensity bit as the ninth glyph bit, so colours 8 to 15 cannot be a foreground.
Designing for eight means a text console and a terminal window show the same picture. Three rules
follow from the measured contrasts:

- **`KT_DIM` is a fill, never a label.** As a foreground it measures 1.42:1 to 2.12:1 against the
  surface and background slots; `KT_MID` measures 3.40:1 to 6.30:1. A label is `KT_MID`, and
  `testing/preflight.sh` refuses any text draw whose foreground can be `KT_DIM`. Text that must be
  muted and still readable takes `kcol_muted()`, which clears 4.5:1 on `deep` in every accent.
- **A selected row is `KT_DIM` under `KT_TEXT`, and the accent is one cell.** In a focused pane the
  row is filled `KT_DIM`, its label stays `KT_TEXT` (8.30:1 at worst), and the marker `►` takes the
  accent wherever the accent clears 4.5:1 on the fill, `KT_TEXT` where it does not; an unfocused
  pane keeps the marker and loses the fill. `ktui_sel_row()` draws it, and no program works it out
  for itself.
- **Emphasis is a fill with swapped slots, never the reverse attribute**, which inverts only the
  cells a character covers and leaves a hole at every space.

## The glyph tiers

What a text tool can draw depends on the terminal. `libktui` picks a tier from its capabilities:

| Tier | When | Box drawing and symbols | Bars and charts |
|---|---|---|---|
| rich | UTF-8, not a Linux console | `glyph_utf8` | Eighth blocks, 8 levels |
| vt | UTF-8 on a Linux console | `glyph_utf8` | `░ ▒ █`, 3 levels |
| ascii | No UTF-8 | `glyph_ascii` | `. : #`, 3 levels |

The box-drawing and symbol table, `glyph_utf8` in `src/libs/libktui/ktui_draw.c`, is the same for
the rich and vt tiers, so it stays inside what the console font `ter-kdos32n` can draw: a character
the font lacks is a blank on the console, and nothing reports it. The font has `░ ▒ █`, the single
box-drawing set, the double rules `═ ║`, the double corners `╔ ╗ ╚ ╝` and `╬`, and `· • ■ … ° ↑ ↓
◀ ▶`; it has no eighth or half blocks, no `▓`, no braille, and no double tees. `testing/preflight.sh`
reads the built font and refuses a `glyph_utf8` entry it cannot draw. A frame's title is bracketed
`[ like this ]` in ASCII, because the brackets read the same at all three tiers. Display width is
measured by the toolkit's own table, `ktui_wcwidth`, and a wide character on the console, whose font
has none, is written as one `?`.

## The checklist

KDOS code that draws on the desktop (a settings page, a widget, an effect, a welcome page, a
theme) is not finished until every line is answered:

1. Every colour from a role of the scheme in force (`Kirigami.Theme`, `KColorScheme`, a shader
   uniform), and none written as a literal; the preflight check passes.
2. Readable in all eight accents, `paper` included, and at the 4.5:1 text floor in each.
3. Text in the interface face; the fixed face only where a monospace means something.
4. Every motion through `animationTime()` or Kirigami's durations, so `AnimationDurationFactor`
   scales it and `0` stops it, with the end state complete on its own.
5. An accessible name on every control, so the screen reader can say what it is.
6. Square where KDOS draws a frame; Breeze's own components wherever Plasma provides one.
7. Nothing written into a person's settings unless the value there is still KDOS's own.

A text tool answers a shorter list: colour from slots, `KT_MID` for labels, fills for emphasis,
the selection drawn by `ktui_sel_row()`, and a look at the vt tier before believing it works on the
console.

## See also

- [Theming](../02-user-guide/theming.md) — choosing an accent, Phosphor mode and the fonts, as a user
- [The desktop](../02-user-guide/desktop.md) — the desks, the panel and the shortcuts
- [The window model](window-model.md) — where windows go, and the rules that colour a box's title bar
- [KWin](../04-programs/kwin.md#phosphor-mode) — the CRT pass and the window motion as KWin runs them
- [The Plasma shell](../04-programs/plasma-shell.md#the-desks) — the desks and the panels they lay out
- [Boot and init](boot-and-init.md) — the splash and the handover to the login screen
- [Writing desktop software](../05-developer/writing-desktop-software.md) — writing a settings page, widget or effect that follows this chapter
- [The C libraries](../05-developer/c-libraries.md) — `libkcolor` and `libktui`
- [The kdos command](../04-programs/kdos-command.md) — `kdos theme`, `kdos phosphor` and `kdos desk`
- [Accessibility](../02-user-guide/accessibility.md) — reduced motion, contrast and the screen reader
- [Configuration](../06-reference/configuration.md) — every settings file KDOS ships
- [Glossary](../06-reference/glossary.md) — accent, global theme, Phosphor mode and the other terms used here

<!-- book-nav -->
---

*Part III — Architecture, chapter 18.* Previous: [17. The security model](security-model.md) · [Contents](../README.md) · Next: [19. The window model](window-model.md)
