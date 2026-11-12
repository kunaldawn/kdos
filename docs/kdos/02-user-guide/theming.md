# Theming

This chapter covers how KDOS looks and how to change it. The look is *KDOS Modern*: Plasma in KDOS
colours, with square, opaque panels and frames, Noto Sans for reading and JetBrains Mono for
everything fixed-width. It comes in eight colour schemes called *accents*, and *Phosphor mode*
turns it into a CRT, with a phosphor glow, optional scanlines and windows that unfold like a
picture tube warming up. The chapter describes the accents and how to switch them, what an accent
switch reaches and when, Konsole and the terminal programs, the boot menu, splash, login screen and
consoles, Phosphor mode, the wallpaper, sounds, fonts, and how applications on the host and in boxes
take the colours. The last sections describe the generators that write the theme files, for anyone
who wants to audit or rebuild them. [The design language](../03-architecture/design-language.md#colour)
explains the colour model underneath it.

To change the accent, open System Settings → Colors & Themes → KDOS Look and click a swatch, or type
`accent amber` in KRunner. Plasma, every KDE application, the open terminals, the wallpaper, the
lock and login screens, the boot menu and the text consoles all take the new accent, with no
logout. The rest of this chapter explains what that one click changes, when each change reaches
each program, and the settings around it.

## The eight accents

| Accent | Character |
|---|---|
| `phosphor` | Green on near-black |
| `amber` | Amber on warm black |
| `ice` | Cyan and pale blue on blue-black |
| `bone` | Warm off-white on near-black, the least saturated of the eight, and the default |
| `norton` | Yellow and cyan on deep blue, after the Norton Commander file manager |
| `borland` | Cyan and yellow on dark teal, after the Borland text-mode IDEs |
| `perfect` | White on blue, after the WordPerfect editing screen |
| `paper` | Blue ink on off-white paper, the one light accent |

The table is in the order KDOS Look, `kdos theme list` and `kdos theme next` use.

An accent is not a single colour. It is a palette of nine named colours, all defined in one table
in `libkcolor` (`KCOL_SCHEMES` in `src/libs/libkcolor/kcolor.h`):

| Colour | Role |
|---|---|
| `primary` | The accent itself: focus, links, the phosphor tint; the selection is it darkened on a dark accent and it on `paper` |
| `dim` | A muted version of the primary, used as a fill and never for text |
| `secondary` | A second hue for contrast: warnings, neutral messages |
| `urgent` | Errors and destructive actions |
| `deep` | The ground the palette's legibility is measured against: the window background, and the text console's |
| `text` | Body text, read on `deep` |
| `variant` | The surface colour of views, panels and popups |
| `pdark` | Visited links and secondary text |
| `backdrop` | The background behind windows |

Every colour KDOS hands to Plasma comes from this table, through one generator, `kdos-theme`, when
the image is built: the eight Plasma colour schemes, `KDOS<Title>.colors`; the eight global themes,
`org.kdos.<accent>.desktop`; the editor themes; the Konsole schemes and profiles; the Qt 5
palettes; the wallpapers; and the login screen's colours. The boot menu and the consoles take theirs from the same table. So
an accent is the same accent everywhere, from the firmware's menu to a terminal tab.

### How the accents are kept readable

Every accent is held to contrast floors, and the build refuses a scheme that falls below one.
Each Plasma colour scheme is two backgrounds per set (a list's normal and alternate rows, for
instance) with ten roles drawn on them, and every role is solved against *both* backgrounds rather
than picked by eye:

- Text roles, normal and inactive, links, and the positive, neutral and negative colours reach
  4.5:1, the WCAG AA level for normal-size text, on both backgrounds of their set.
- Focus and hover decorations reach 3:1.
- Window titles reach 4.5:1 on their own backgrounds.
- A selected row reaches 4.5:1 twice: its own text on the selection, and the list's ordinary text
  on the selection laid translucently over the list, which is how Dolphin and many other programs
  draw it. On a dark accent the selection is therefore the accent darkened until the ordinary text
  reads on it; on `paper` it is the accent with `deep` on it. A hovered selected row in a GTK
  program is the selection darkened further, so its text reads there too.
- The bevel shades Qt works out from the window background (LibreOffice's workspace is one of
  them) stay as quiet as that background: the window background of a dark accent is its `deep`
  moved towards grey just far enough, a move barely visible at that depth.

An accent-coloured role that misses its floor is moved towards the set's text colour one percent
at a time, so it keeps as much of its hue as the floor allows. The text console's sixteen colours
are held to their own floors by the library's self-test. `kdos theme --audit` measures the installed
schemes again on a running machine; see [Auditing what is installed](#auditing-what-is-installed),
and [The role mapping](../03-architecture/design-language.md#the-role-mapping) for every role.

## Switching the accent

There are four places to switch, and they all end in the same command:

| Where | What it does |
|---|---|
| System Settings → Colors & Themes → **KDOS Look** | One swatch per accent, each painted from its own colour scheme. A click runs `kdos theme <accent>`. Below the swatches the page compares the three places an accent is held (Plasma's colour scheme, the accent the terminals wear, and the machine's, which the screens before login wear), and **Repaint Everything** brings them back together when they differ. The page also holds the Phosphor mode switch |
| The Welcome tour's **Look** page | The same swatches, with light and dark, Phosphor mode, Night Light and reduced motion |
| KRunner | `accent amber`, `accent next` |
| A prompt | `kdos theme amber` |

Choosing a KDOS global theme on the Global Theme page, or a KDOS colour scheme on the Colors page,
works too. Those change only Plasma's colours, so a small KDOS module in the session, `kded-kdos`,
notices the new scheme and runs `kdos theme --from-plasma <accent>`, which carries it to the
terminals, the wallpaper, the lock and login screens, the boot menu and the consoles. A scheme that
is not KDOS's changes nothing below the desktop.

From a prompt, `kdos theme`:

```sh
kdos theme              # print the accent in force
kdos theme list         # the eight accents with their theme names; the current one is marked *
kdos theme amber        # switch to amber
kdos theme next         # the next accent in the list, wrapping at the end
kdos theme prev         # the previous accent
kdos theme --audit [X]  # compare installed theme files with the palette
kdos theme style FILE   # apply a style file
kdos update theme       # re-run the generators for the accent in force
```

A switch prints the accent and its theme name, such as `amber (KDOS-Amber)`. When the root power
service cannot be reached or refuses the request, a line follows saying the boot menu, splash and
login screen are unchanged, and why (see
[The boot menu, the splash and the text consoles](#the-boot-menu-the-splash-and-the-text-consoles)).
With no accent ever chosen, the accent in force is the one Plasma's colour scheme names when that is
a KDOS scheme, and otherwise `bone`.

Outside a Plasma session, at a text console or over SSH, `kdos theme` records the global theme for
the next login and says `Plasma takes it at the next login`; the terminal programs' files and the
machine's half are written at once.

**Light and dark.** `paper` is the one light accent. Quick Settings' light and dark switch, and the
Global Theme page's automatic switching, move between the dark accent in force and `paper`; KDOS
keeps that pairing pointed at the accent you chose, for as long as it names a KDOS theme. Automatic
switching by time of day is off; the Welcome tour offers it.

`kdos update theme` exists for package upgrades: it regenerates the terminal programs' files in
your home for the accent you already have, exactly as `kdos theme <current accent>` would.

### What repaints at once, and what waits

Nothing is generated during a switch except the terminal programs' files: every colour scheme,
global theme, Konsole profile, Qt 5 palette and wallpaper was written when the image was built, so
a switch chooses between files that exist. `kdos theme <accent>` runs eight steps, in order:

1. Writes the accent's name to `~/.config/kdos/accent`.
2. Applies the global theme `org.kdos.<accent>.desktop`, which sets the colours, the Plasma style,
   the icons, the cursor, the window decoration, the splash and the fonts, without touching your
   panels; and keeps the light-and-dark pairing on this accent.
3. Sets the wallpaper and the lock screen's wallpaper to the accent's, but only where the picture
   in force is a KDOS one; a picture you chose stays.
4. Moves Konsole's default profile, and every open Konsole tab on a KDOS profile, to the accent's,
   and QMLKonsole's colour scheme with them.
5. Writes the terminal programs' colour files, and tells a running tmux to read its configuration
   again.
6. Points the Qt 5 programs' palette at the accent's.
7. With Phosphor mode on, retints the CRT pass, with one degauss pulse.
8. Asks the root power service to recolour the boot menu, the splash, the text consoles and the
   login screen.

Plasma and every running KDE application repaint at once, as do the open Konsole tabs, the
wallpaper and the CRT tint. A running GTK 3 application repaints when Plasma's GTK bridge announces
the change. A running GTK 4 or libadwaita application does not: GTK 4 re-reads its colours only
when the dark or light preference changes, and that change reaches it before the bridge has
written the new colours, so it reloads the colours of the switch before. It takes the new accent
the next time it starts. A Qt 5 program, a terminal program that reads its colours once, and the boot menu,
splash, consoles and login screen take the accent the next time they start as well.

Night Light (`Meta+Ctrl+Shift+N`, or `kdos toggle night-light`) is KWin's: it warms the whole
picture towards a lower colour temperature, over every program, rather than changing the palette.

## What changes when

Besides the global theme, `kdos theme` writes a file for each program that does not read KDE's
colours, nearly all of them programs KDOS did not write. A few of them can be told to reload, but
most read their colours once, when they start. Each therefore takes effect at a different moment:

| What | Read by | Takes effect |
|---|---|---|
| The global theme `org.kdos.<accent>.desktop`, applied into `~/.config/kdeglobals` and its kin | Plasma, KWin and every KDE and Qt 6 application | At once |
| The wallpaper, in Plasma's desktop configuration and `~/.config/kscreenlockerrc` | The desktop and the lock screen | At once |
| `~/.config/konsolerc`, and the open tabs | Konsole, and the terminal panels of Dolphin, Kate and Yakuake | At once |
| `~/.config/qmlkonsolerc` | QMLKonsole | Next start |
| Nothing is written: the editor picks the `KDOS <Title>` editor theme whose background is the scheme's | The editor of Kate, KWrite and KDevelop | At once, unless an editor theme was chosen in the editor's own settings |
| `~/.config/tmux/themes/kdos.conf` | tmux | At once, if a tmux server is running (`kdos theme` re-sources `tmux.conf`) |
| The palette block in `~/.config/starship.toml`, between its two markers | starship | At the next shell prompt |
| `~/.config/btop/themes/kdos.theme` | btop | Next start |
| `~/.config/kdos/fzf-colors` | fzf, through `$FZF_DEFAULT_OPTS` (appended by `/etc/profile.d/30-kdos-colors.sh`) | At the next login shell |
| `~/.config/bat/themes/kdos.tmTheme` | bat | Next start; `kdos theme` rebuilds bat's cache itself |
| `~/.config/micro/colorschemes/kdos.micro` | micro | Next start |
| `~/.config/helix/themes/kdos.toml` | helix, on the host or in a box | Next start |
| `~/.config/nvim/colors/kdos.vim` | neovim | Next start |
| `~/.config/git/kdos-delta` | delta, included from the shipped git configuration | At the next diff |
| `~/.config/newsboat/kdos-colors` | newsboat, `include`d from its configuration | Next start |
| `~/.config/aerc/stylesets/kdos` | aerc | Next start |
| `~/.config/yazi/theme.toml` | yazi | Next start |
| `~/.local/share/mc/skins/kdos.ini` | mc, selected by `skin = kdos` under `[Midnight-Commander]` in `~/.config/mc/ini` | Next start |
| `~/.config/kdos/ls-colors` | `ls`, through `$LS_COLORS`, loaded by `.bashrc`, which follows `$XDG_CONFIG_HOME` as `kdos theme` does | In the next shell |
| `~/.config/qt5ct/qt5ct.conf` | Qt 5 applications, through qt5ct | The application's next launch |
| `/etc/kdos/accent` | The boot splash | The next boot |
| `/boot/efi/limine.conf` | The boot menu | The next boot |
| `/etc/vtrgb` | Every text console: the login prompt, `/etc/issue`, the login banner | The next login prompt |
| The `sddm` account's `~/.config` | The login screen | The next time it is shown |

The paths under `~/.config` and `~/.local/share` follow `$XDG_CONFIG_HOME` and `$XDG_DATA_HOME`
when those are set.

### Files you may edit, and files you may not

KDOS replaces only what is KDOS's. Every step first reads the value in force and moves it only while
it still names a KDOS file: a wallpaper, a Konsole profile, a light-and-dark pairing or a Qt 5
palette you chose is left exactly as you set it, so a KDOS accent switch never takes your own
choice with it. Plasma's own settings are yours in the usual way: anything you set in System
Settings lands in `~/.config` and outranks KDOS's defaults.

Never edit a generated file: every terminal program's file in the table above is rewritten on each
accent switch, and most of them carry a header saying so. Each is replaced whole, apart from the
files that are merged (see below). Most of them are *selected* by a second file that ships once and
then belongs to you. Change these freely; `kdos theme` does not touch them, apart from the starship
palette block noted in the table:

| Program | File | The line that selects the KDOS theme |
|---|---|---|
| micro | `~/.config/micro/settings.json` | `"colorscheme": "kdos"` |
| neovim | `~/.config/nvim/init.vim` | `colorscheme kdos` |
| delta | `~/.config/git/config` | an `[include]` of `kdos-delta` |
| newsboat | `~/.config/newsboat/config` | `include "~/.config/newsboat/kdos-colors"` |
| aerc | `~/.config/aerc/aerc.conf` | `styleset-name=kdos` |
| bat | `~/.config/bat/config` | `--theme="kdos"` |
| btop | `~/.config/btop/btop.conf` | `color_theme = "kdos"` |
| tmux | `~/.config/tmux/tmux.conf` | a `source-file` of `themes/kdos.conf` |
| starship | `~/.config/starship.toml` | `palette = "kdos"`; only the block between the markers is rewritten |

To keep the accent but change one colour, build on the generated theme: your own micro scheme may
`include "kdos"`, and your own helix theme may say `inherits = "kdos"`.

The helix theme is read by the host's helix and by a helix you install in a box, because a box
shares your home directory by default. Nothing selects it for you: add `theme = "kdos"` to your own
`~/.config/helix/config.toml`.

Two programs have no separate selector file:

- **yazi** reads `theme.toml` by that exact name and layers it over its own built-in preset. The
  generated file contains only the colours the palette decides, and yazi's preset supplies the
  rest.
- **mc**'s selector is not yours: `kdos theme` sets `skin = kdos` in `~/.config/mc/ini` itself,
  changing that one key and leaving the rest of the file alone.

Two programs cannot take an exact colour:

- **newsboat** treats `#` as the start of a comment, so a hex colour cuts the line short and the
  entry is rejected. Its colours are the nearest of the 256 terminal colours instead.
- **lazygit** has no include mechanism and no separate theme file; its colours live in the same
  `config.yml` you edit. `kdos theme` does not write lazygit colours at all, because doing so
  would mean taking over that file and discarding your other settings.

Three files are merged rather than replaced: `~/.config/starship.toml` (only the block between its
markers), `~/.config/mc/ini` (only the `skin` key) and `~/.config/qt5ct/qt5ct.conf` (only the
palette, style, icon theme and font keys, and only while the palette is a KDOS one; the rest is
what qt5ct saved for you).

## Konsole and the terminal programs

Konsole has one KDOS profile per accent, `KDOS <Title>`, each with a colour scheme of the same name
and JetBrains Mono one point larger than the interface, because a terminal is read for longer than
a text field. Ligatures are off, so `!=` and `->` stay two characters in the columns you typed them
in. The sixteen terminal colours are the accent's: plain and bold text, the accent and its
secondary, red from the urgent colour, and blue, magenta and cyan derived from the palette so that
`ls`, a diff and syntax highlighting keep three distinct hues in every accent. The same sixteen are
what `kdos theme` writes into tmux, btop and the other terminal programs, so a program's palette and
the terminal's agree.

`/etc/xdg/konsolerc` makes a new tab follow the desktop's light and dark state, opening in the dark
accent's profile or in `KDOS Paper`; `kdos theme` moves the default to the accent you chose, and
every open tab on a KDOS profile with it. A profile you made yourself is never moved. Dolphin's,
Kate's and Yakuake's terminal panels open in the same default. QMLKonsole has no profiles; it
names a colour scheme, `KDOS-Bone` by default, and `kdos theme` moves it to the accent's scheme
while it still names a KDOS one.

## The boot menu, the splash and the text consoles

Four things outside your session also follow the accent: the boot menu lives in `limine.conf` on
the EFI system partition, the boot splash reads `/etc/kdos/accent`, every text console reads
`/etc/vtrgb`, and the login screen reads its colours from the `sddm` account's home. They belong to
root, and they are settings for the machine rather than for your session, so `kdos theme` asks the
root power service, `kdos-powerd`, to update them (through `kdos-power accent <name>`). The daemon
accepts the request from root and from members of the `wheel` group, and asks polkit for anyone
else; it accepts only one of the eight accent names, so there is no path or free text for a caller
to supply. It writes `/etc/kdos/accent` itself, copies the accent's login-screen files into the
`sddm` account's home, and runs `kdos-bootctl theme <accent>` for the boot menu and the consoles.

The console palette is the accent's sixteen colours. `kdos-getty` loads `/etc/vtrgb` onto each
virtual terminal before it clears the screen, so the login prompt, `/etc/issue` and the login
banner are drawn in the accent: plain text uses slots 0–7 and bold text slots 8–15, and both halves
come from the accent. Blue, magenta and cyan are the exception. No accent defines them, so they are
fixed hues, the same in every accent and chosen to read on a dark or a light background; a console
whose eight colours all collapsed onto one accent would have nothing left for `ls --color`, a diff
or syntax highlighting. To see the table for an accent without writing it:

```sh
kdos-bootctl palette [<accent>]
```

The four change at different moments. The boot menu changes on the next boot, the console palette
at the next login prompt (log out of a console, or reboot), the login screen the next time it is
shown, and the splash partway through the next boot. The splash starts from the initramfs, before
the root filesystem is mounted, so it comes up in the accent built into the initramfs; the startup
script `/etc/init.d/rcS` then passes it the machine's accent from `/etc/kdos/accent` as soon as
`/etc` can be read. If you have never changed the accent, the two are the same and you see nothing.
The login screen never follows day and night, so `paper`, the light accent, shows its dark partner
there.

The splash, the login screen, KSplash (the splash between the login and the desktop) and the lock
screen are one look: the KDOS mark in the same place on the same ground, with the clock in the
monospace face (on the lock screen in the band above the mark, on the login screen where Breeze
puts it), so the start reads as one continuous picture from the firmware to the desktop.

`kdos-bootctl` rewrites only the lines of `limine.conf` that describe the menu's look and the
splash's motion, never the timeout or the boot entries, and writes the file through a temporary
copy so a power cut cannot leave it empty. Three situations leave the boot menu as it is:

- A machine with no bootloader configuration, such as the live medium or any machine with no
  `/boot/efi/limine.conf`, has no boot menu to recolour. The splash, the console palette and the
  login screen still follow the accent.
- When a `limine.conf` exists but cannot be rewritten, or `kdos-bootctl` is missing, the splash and
  the login screen are still updated (and the console palette too, unless `kdos-bootctl` is
  missing).
- When `kdos-powerd` does not answer (inside a box, where the daemon cannot be reached) or refuses
  the request, the boot menu, the splash, the console palette and the login screen are all left as
  they are, and `kdos theme` prints `boot menu, splash and login screen unchanged (…)` with the
  reason.

In each of these situations, everything in your session is still switched, and `kdos theme` exits
without an error.

The boot menu's background picture does not follow the accent. The bootloader cannot recolour a
picture, and writing a new image to a FAT filesystem on every theme change risks a half-written
file if the power fails, so the backdrop is a single dimmed, colourless image that suits every
accent.

## Phosphor mode

Phosphor mode makes the screen a CRT. It is off by default, and one switch turns all of it on or
off:

```sh
kdos phosphor on                    # the Subtle tuning
kdos phosphor on --preset retro     # scanlines and a curved glass
kdos phosphor on --terminus         # and Terminus as the fixed-width face, Konsole's included
kdos phosphor off                   # everything back as it was
kdos phosphor status                # on or off, the tuning, and what is refused
```

The same switch is on the KDOS Look page, on the Welcome tour's Look page and in KRunner (`phosphor
on`). It has no keyboard shortcut, so a stray chord cannot flash the whole screen. What it turns on:

| Part | What you see |
|---|---|
| The CRT pass | A full-screen effect in KWin over everything: a horizontal bleed, a vignette and a faint glow in the accent so black is never quite black, and with the Retro tuning scanlines and barrel curvature |
| The motion | Windows unfold from a line as they open and fold to one as they close; the desktop opens like a picture tube warming up after the login; at logout the windows fold away; the screen collapses to a dot as it locks; each accent change gives one degauss pulse |
| The accent | `bone` and `paper`, the two accents that do not read as a CRT, give way to `phosphor`; the other six are kept. Off puts the previous accent back |
| The boot splash | A CRT power-on, from the next boot, in place of the plain fade |
| The face (with `--terminus`) | Terminus (TTF) in Konsole and as the fixed-width font everywhere; the interface stays in Noto Sans |

`kdos phosphor off` puts back exactly what `on` found, key by key in your own configuration: before
it changes anything, `on` records the value of every key it is about to change, as your own file
held it, in `~/.config/kdos/phosphor`. A setting you never made stays unset afterwards.

The tuning is three numbers, edited on the effect's own settings in System Settings → Window
Management → Desktop Effects, or with `kdos phosphor preset subtle|retro`:

| Key in `kwinrc` `[Effect-kdos_crt]` | Subtle | Retro | What it does |
|---|---|---|---|
| `Strength` | 55 | 55 | Overall strength of the pass, in percent |
| `Scanlines` | 0 | 60 | Scanline depth, in percent |
| `Curve` | 0 | 30 | Barrel curvature, in percent, scaled so no value crops the desktop |
| `Fullscreen` | off | off | Whether the pass covers a fullscreen window too |

`kdos phosphor fullscreen on|off` sets the last key. A [style file](#style-files) carries the same
tuning with an accent. Retro's scanlines lower the effective contrast of the screen, which the
accents' measured floors do not cover; Subtle draws none.

Reduced motion stops all of the motion and keeps the still picture: with animations off
(`kdos toggle reduced-motion`, or the animation speed set to instant on System Settings' General
Behavior page), windows appear and close at once, the lock is instant, and no degauss pulse runs.

### What it costs

While the pass is on, every frame of every screen it covers goes through the GPU twice: KWin draws
the screen into an offscreen picture and then draws that through the CRT shader, and the pointer is
drawn into the picture so that it curves with it. The display's hardware cursor and direct scanout
are given up for those screens. With `Fullscreen` off, the default, a screen showing an active
fullscreen window gets direct scanout back, so a video or a game plays untouched and its frames go
straight to the display; that is also the lever on battery.

### When it switches itself off

The pass runs only where KWin renders on a GPU. On software rendering, such as a virtual machine
without 3D acceleration, the effect refuses to load, because a full-screen pass per frame would be
the processor's whole budget, and `kdos phosphor on` says so and writes nothing. It also steps
aside:

- while the zoom or the magnifier is on, since magnified scanlines make text harder to read;
- on an HDR screen, which it leaves plain;
- in captures: a screenshot or a screen recording, through Spectacle or the screen-sharing portal,
  and the colour picker get the plain picture, never the CRT one.

How KWin decides, frame by frame, is in
[When a frame goes through the pass](../04-programs/kwin.md#when-a-frame-goes-through-the-pass).
For developers: `make run` boots without 3D and Phosphor mode cannot be turned on there; `make
run-hw` boots with virgl and it can (see [Developing](../05-developer/developing.md)).

## Wallpaper

Each accent has its wallpaper, `KDOS-<Title>`, a Plasma wallpaper package under
`/usr/share/wallpapers`: the KDOS penguin and wordmark, with the glow in the accent, on the accent's
ground, in a dark and a light version and at four sizes. Plasma follows the light and dark switch
within the package by itself, and picks the size nearest the screen.

An accent switch sets the desktop's and the lock screen's wallpaper to the new accent's, but only
while the picture in force is a KDOS one. A picture you chose, in **Configure Desktop and
Wallpaper** or on the Screen Locking page, is never replaced. To go back to the accent's, choose
`KDOS-<Title>` on the same page.

## Style files

A style file bundles an accent with Phosphor mode's tuning, so you can share a whole look:

```sh
kdos theme style retro.kdos
```

A style file is plain `key = value` lines; `#` starts a comment. It may contain:

| Key | Effect |
|---|---|
| `accent` | The accent to switch to. Without it, the current accent is kept |
| `crt` | Phosphor mode's `Strength` |
| `crt_scanlines` | `Scanlines` |
| `crt_curve` | `Curve` |
| `crt_fullscreen` | `Fullscreen`, `true` or `false` |

For example:

```ini
# retro.kdos
accent = amber
crt = 70
crt_scanlines = 60
crt_curve = 30
```

The tuning is written whether Phosphor mode is on or not, and takes effect at once when it is on. A
key that is none of the above is reported and ignored, so a look that differs on a second machine
says so. An unknown accent stops the command before anything is written. After writing the tuning,
a style runs the same full switch as `kdos theme <accent>`.

## Sounds

The KDOS sound theme, `kdos`, gives the machine its own voices for the login and logout, a new
message, an error, a warning, a finished job and a device plugged in or removed: short square and
triangle tones, synthesised when the image is built and recorded nowhere. Every other event falls
through to Plasma's Ocean theme. `kdos notify` sounds the new-message voice. Choose another theme on
System Settings' System Sounds page, and turn event sounds off on the Notifications page.

## Fonts

| Text | Face |
|---|---|
| Everything you read in Plasma and KDE applications: menus, labels, window titles | Noto Sans, 10 points |
| Everything fixed-width: Konsole, Kate's editor, code, the panel clock, the splash's text, the login and lock screens' clocks, System Monitor's figures | JetBrains Mono, 10 points (11 in Konsole) |
| The text console | `ter-kdos32n`, a 16×32-pixel bitmap font built from Terminus |
| Phosphor mode with `--terminus` | Terminus (TTF) as the fixed-width face, 12 points |

JetBrains Mono is named in `kdeglobals` as the fixed face and first for `monospace` in fontconfig,
so every program that asks for "monospace" gets it. It is chosen for a tall x-height and for
telling `0` from `O` and `1` from `l` and `I` at small sizes; `testing/monofont.py` checks its
coverage of box drawing, blocks, braille, Latin, Greek and Cyrillic, and the width of its cell. Text
is drawn with greyscale antialiasing and slight hinting, which looks the same at every fractional
scale, in widgets and in Plasma's own Qt Quick labels alike.

### Choosing a font

System Settings → Text & Fonts sets the interface, fixed-width, small, toolbar, menu and window
title fonts and the antialiasing; a change there lands in your `~/.config/kdeglobals` and outranks
the global theme. Konsole's font is part of its profile, set under **Settings → Edit Current
Profile → Appearance**. `Ctrl++`, `Ctrl+-` and `Ctrl+0` resize one Konsole tab.

The console font is a bitmap font with 512 glyphs, loaded by `kdos-getty` onto each virtual
terminal before it clears the screen. Anything outside those glyphs shows as a blank on a text
console, which is why the tools that run there limit themselves to a small set of characters.

## Theming native applications

Applications on the host take the accent by one of three routes:

| Toolkit | Route |
|---|---|
| Qt 6, KDE Frameworks | KDE's platform theme (`QT_QPA_PLATFORMTHEME=kde`, set for every login) reads the colours, fonts and icons of the global theme from `kdeglobals`, and repaints a running application on a switch |
| GTK 3, GTK 4 and libadwaita | Plasma's GTK bridge sets the Breeze GTK theme, the icons, fonts and window buttons, and writes the scheme's colours under `~/.config/gtk-3.0` and `~/.config/gtk-4.0` from the first login; a running GTK 3 application repaints on a switch, a GTK 4 one at its next start. GTK 4 opens the KDE file dialog through the portal; a GTK 3 application does when it uses GTK's native file chooser, which `GTK_USE_PORTAL=1` sends to the portal, and keeps GTK's own dialog when it builds that itself, as AbiWord does |
| Qt 5 | qt5ct, under the same `kde` platform-theme name, reads `~/.config/qt5ct/qt5ct.conf`, which `kdos theme` points at the accent's palette, with the Fusion style and the KDOS icons |

KDE's platform theme is built for Qt 6 only, and there are no KDE Frameworks for Qt 5, so qt5ct is
the only way the accent reaches VLC, KeePassXC, LMMS and the other Qt 5 programs. qt5ct's own
settings program is hidden from the menu; a palette chosen in it is yours and stays. KeePassXC
replaces that palette with its own light or dark style unless its theme is `classic`, which a new
account's `~/.config/keepassxc/keepassxc.ini` sets; choosing another theme in KeePassXC's settings
brings its own style back.

Kate's, KWrite's and KDevelop's editor draws in a syntax-highlighting theme rather than the colour
scheme. With none chosen in the editor's settings it takes the installed theme whose background
is exactly the scheme's view background, so `kdos-theme` writes `KDOS <Title>` for every accent,
with the terminal's sixteen colours for the syntax, and the editor follows each switch.

Tk, FLTK and Java's Swing and AWT take none of these routes and draw their own light-grey
defaults; see
[Tk, FLTK and Java programs keep their own colours](../06-reference/known-gaps.md#tk-fltk-and-java-programs-keep-their-own-colours).

**Icons and the pointer.** The `KDOS` icon theme (`KDOS-Light` for `paper`) is Breeze's icons with
the KDOS penguin where Plasma shows the KDE logo, and it follows the colour scheme: an icon drawn to
take the scheme's colours takes the accent's. `KDOS` redraws Breeze's status and emblem marks
(the information, success, warning and error squares among them) so that their glyph is dark on a
light accent, and both themes fall back to Adwaita for the symbolic icons only GTK programs ask
for. GTK and other toolkits that do no such recolouring draw those icons in Breeze's own blue. The
pointer is Breeze's, the same in every accent.

## Theming applications inside boxes

A box has its own `/usr`, from its own distribution, so the host's themes are invisible inside it.
What a box does share by default is your home directory, at the same path on both sides, and the
session's settings portal:

| Toolkit in the box | Takes the accent from |
|---|---|
| Qt, where the box's runtime includes KDE's platform integration | `~/.config/kdeglobals`, which Plasma writes into the shared home, so Breeze paints in the session's colours with no bridge |
| Qt, without it | The GTK platform theme with the Fusion style, set by the runtime's own pack metadata |
| GTK | `GTK_THEME`, which every launch sets to the theme Plasma's GTK bridge names, and the colours the bridge writes under `~/.config/gtk-3.0` and `~/.config/gtk-4.0` |

The GTK row holds because the theme is named outright: every launch exports `GTK_THEME` as the
theme `~/.config/gtk-3.0/settings.ini` names, with `:dark` when that file prefers the dark variant,
and the variable wins over the theme name the settings portal hands a boxed GTK program and over the
box's GSettings, a key file that holds none. The theme is in the box because the `rt-gtk`
runtime every GTK application of the catalogue sits on carries Debian's Breeze GTK theme (light and
dark, for GTK 2, 3 and 4), and the session's colours come from the `colors.css` the bridge writes
beside it. A running boxed GTK program keeps the theme and the colours it started with: an accent
switch, or a switch between dark and light, reaches it at its next start (see
[known gaps](../06-reference/known-gaps.md#a-running-boxed-gtk-program-takes-a-new-accent-only-at-its-next-start)).
A GTK 3 program that builds GTK's own file dialog shows GTK's dialog in a box too (see
[known gaps](../06-reference/known-gaps.md#a-gtk-program-that-builds-its-own-file-dialog-keeps-gtks)).

A box created with `home = private` has a home of its own and sees none of these files; see
[Box profiles](../04-programs/kdos-appbox.md#box-profiles). The full list of what a box receives is
in [The environment a box gets](../04-programs/kdos-appbox.md#the-environment-a-box-gets).

## How the theme is generated

`kdos-theme` is the generator of KDOS's identity kit. It runs when the image is built, in the
`build.sh` of the ports that install what it writes, never in a session, and every colour it writes
comes from libkcolor's table and nothing else:

| Subcommand | Writes | Installed by |
|---|---|---|
| `kde-colors` | `KDOS<Title>.colors`, one Plasma colour scheme per accent, and `KDOSBox.colors` for a box with no accent | `kdos-plasma-look`, into `/usr/share/color-schemes` |
| `syntax-themes` | `KDOS-<Title>.theme`, the editor theme of Kate and every KTextEditor program per accent | `kdos-theme` itself, into `/usr/share/org.kde.syntax-highlighting/themes` |
| `konsole` | `KDOS-<Title>.colorscheme` and `KDOS-<Title>.profile` per accent | `kdos-plasma-look`, into `/usr/share/konsole` |
| `qt5ct-colors` | `KDOS-<Title>.conf`, the Qt 5 palette per accent | `kdos-plasma-look`, into `/usr/share/qt5ct/colors` |
| `wallpaper` | `KDOS-<Title>/`, a Plasma wallpaper package per accent, from the master artwork | `kdos-wallpapers`, into `/usr/share/wallpapers` |
| `greeter-globals` | The colour groups merged into a `kdeglobals`, and per accent the files the login screen reads | `kdos-plasma-look`, into `/usr/share/kdos/xdg` and `/usr/share/kdos/greeter/<accent>` |
| `contrast` | A table of every role against its floor; exit 1 when one is below | Every `build.sh` that installs a scheme, as a gate |
| `accents` | The accent names, for a build that makes one file per accent | |

`kdos-plasma-look` writes the rest of the kit from the generated schemes: the eight global themes
(each naming its scheme, the `KDOS` or `KDOS-Light` icons, Breeze's pointer, the square Breeze
decoration, the `kdos` Plasma style, the fonts, the KSplash and the wallpaper), the `kdos` Plasma
style itself, the three desks, and the previews the Global Theme page shows. Every colour a frame,
a preview or the splash needs is looked up in the generated scheme, so no colour is written
anywhere else.

The KDE role mapping, for each set of a scheme:

| Role | Colour |
|---|---|
| The backgrounds | Views on `variant`, windows on `deep` moved towards grey until the shades Qt derives from it are quiet, buttons, tooltips and headers on steps between them |
| Normal text | `text`; the selection's text on a selection |
| Inactive text | The background walked towards the text colour until it clears 4.5:1 on both backgrounds |
| Active text, links, positive | The accent; the selection's text on a selection |
| Visited links | `pdark` |
| Negative, neutral | `urgent`, `secondary` |
| Focus decoration | The accent, held to 3:1 |
| Hover decoration | Between the focus colour and `deep`, held to 3:1 |
| The selection | On a dark accent the accent darkened until `text` holds 4.5:1 on it, with `text` on it; on `paper` the accent, with `deep` on it |

A dark accent's selection carries `text` because KDE draws the list's ordinary text over the
selection as often as the selection's own: Dolphin lays the selection translucently over the view
and keeps the label's colour, and so do Haruna, KSudoku and every GTK program through Plasma's
bridge. The bright accent under `deep` would leave that text at under 2:1. Plasma's own
accent-colour setting is never written, because Plasma would then recolour the selection, links
and decorations itself with colours that are not the solved ones.

The wallpaper generator recolours only the light of the master artwork: the penguin is the same in
every accent, and its glow and the wordmark take the accent, solved against the ground they are
drawn on. The icon themes recolour nothing themselves: they inherit Breeze and set
`FollowsColorScheme`, so Plasma recolours Breeze's own icons into the scheme as it draws them, and
`KDOS`'s copies of Breeze Dark's status and emblem marks name a scheme colour for the glyph Breeze
fixes white.

What a switch writes into your home, the terminal programs' files, is generated by `kdos theme`
itself from the same table, because no package can ship a file into a home.

### Auditing what is installed

```sh
kdos theme --audit          # does this machine match what the palette makes for the accent in force?
kdos theme --audit amber    # the same, for amber
```

The audit asks three things. The installed colour schemes still meet the contrast floors, measured
again with the build's own gate, which catches a scheme edited or replaced after the image was
made. The accent's installed files are all there, found the way KDE finds them, along
`$XDG_DATA_DIRS`; a global theme whose colour scheme is missing would apply Breeze's colours under
the KDOS name. And the generated files in your home are what the palette makes: the audit runs the
same generators with `$HOME` and the XDG directories pointed at a scratch directory under `$TMPDIR`
(default `/tmp`), first copying in the files that are merged rather than written, and compares the
results byte for byte. Anything reported as different is different from what this machine's own
palette produces.

It writes nothing outside its scratch directory and repairs nothing; run `kdos theme <accent>` to
repair.

| Exit status | Meaning |
|---|---|
| `0` | Everything matches |
| `1` | Something differs |
| `2` | The audit could not run |

## See also

- [The design language](../03-architecture/design-language.md#colour): the palette, the role mapping and the contrast floors
- [KWin](../04-programs/kwin.md#phosphor-mode): the CRT effect and the motion of Phosphor mode
- [KDOS settings pages](../04-programs/kdos-settings.md#kdos-look): the KDOS Look page
- [The desktop](desktop.md): the desks, the panel and the fonts in use
- [Accessibility](accessibility.md): contrast, reduced motion and larger text
- [Applications](applications.md): why boxed software is themed through `$HOME`
- [The daemons](../04-programs/daemons.md#kdos-powerd): `kdos-powerd`, which writes the boot, console and login-screen files
- [The kdos command](../04-programs/kdos-command.md#kdos-theme): `kdos theme` and `kdos phosphor` in full
- [The ports catalogue](../06-reference/ports-catalogue.md): the theme, icon, wallpaper and font ports
- [How KDOS is built](../05-developer/how-kdos-is-built.md): where the default look is seeded

<!-- book-nav -->
---

*Part II — Using KDOS, chapter 9.* Previous: [8. Applications](applications.md) · [Contents](../README.md) · Next: [10. Administration](administration.md)
