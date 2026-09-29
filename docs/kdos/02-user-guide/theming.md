# Theming

This chapter covers how KDOS looks and how to change it: the colour scheme (called an *accent*),
the fonts, the phosphor shader that gives the screen its CRT look, the wallpaper, and how the same
colours reach the boot menu, the text consoles and the applications: those ported natively to the
host, and those running in boxes (rootless containers, each with its own distribution but sharing
your home directory; see [Applications](applications.md)). It is written for
anyone using the desktop; the last sections describe the generators that write the theme files, for
anyone who wants to audit or rebuild them. [The desktop](desktop.md) introduces the panel and the
menus this chapter refers to, and [The design language](../03-architecture/design-language.md)
explains the colour model underneath it.

To change the accent, press `Super+Ctrl+Shift+Space`, move the highlight to an accent and press
`Enter`. Every KDOS [surface](../06-reference/glossary.md) (a window or popup KDOS itself draws)
repaints at once, with no restart and no logout. The rest of this chapter explains what that one key
changes, when each change reaches each program, and the settings around it.

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

The table is in the order the picker, `kdos theme list` and `kdos theme next` use.

An accent is not a single colour. It is a palette of nine named colours, all defined in one table
in `libkcolor` (`KCOL_SCHEMES` in `src/libs/libkcolor/kcolor.h`):

| Colour | Role |
|---|---|
| `primary` | The accent itself: frames, highlights, the phosphor tint |
| `dim` | A dark version of the primary, used as a fill |
| `secondary` | A second hue for contrast (warnings, the second colour of a two-colour element) |
| `urgent` | Errors and destructive actions |
| `deep` | The background everything is read against |
| `text` | What is read on `deep` |
| `variant` | The surface colour of panels and popups |
| `pdark` | The dark end of the highlighted-row range |
| `backdrop` | The colour behind unfocused or inactive areas |

Everything KDOS draws takes its colours from named roles derived from this palette (called
*slots*; see the [glossary](../06-reference/glossary.md)) rather than from fixed colour values.
That is why changing one word repaints the entire desktop: every program already has all eight
palettes compiled in and only needs to be told which one is in force.

### How the accents are kept readable

Every accent is held to contrast rules by the library self-test, so none of them produces text you
cannot read:

- Text (`text`) against the background (`deep`) must reach 7:1.
- The accent (`primary`) against the background must reach 4.5:1, the WCAG AA level for
  normal-size text.
- The highlighted row in a menu (the *plate*) must both stand out from the bar behind it and keep
  its own label readable. The label is aimed at 7:1 and must reach at least 4.5:1.

Whether an accent's background is dark or light does not decide whether it is usable; the plate
does. `paper`, the light accent, carries its highlighted label at about 7.1:1, like every accent
except `bone`. `bone`, the most muted, is the lowest of the eight at about 6.2:1, because the
separation the plate needs from the bar limits how far its colour can move. How the plate colours are chosen is in
[The design language](../03-architecture/design-language.md).

## Switching the accent

`Super+Ctrl+Shift+Space` opens `kdos-style`, the picker. It is a small window titled **Style** with
two pages, **Accent** and **Font**; this section is about the first, and the second is under
[Choosing a font](#choosing-a-font). The Accent page shows one row per accent: the accent's name,
its theme name (for example `KDOS-Amber`) and a swatch of eight blocks in that accent's own colours.
The highlight starts on the accent in force.

| Key or action | Effect |
|---|---|
| `Up`, `Down`, `Home`, `End` | Move the highlight and preview that accent on the whole desktop |
| `Enter` | Keep the highlighted accent: run the full switch |
| `Esc` | Close the picker and put back the accent it opened on |
| `Left`, `Right` | Switch between the Accent and Font pages |
| Click a row | Preview it, as an arrow key would |
| Click the highlighted row again | Keep it, as `Enter` would |

Settings opens the same picker from the **Accent…** row on its Appearance page, and so does the
`style.theme` route in the menu (`kdos menu summon style.theme`).

The swatches are the one place on the desktop drawn in fixed colours rather than slots: a swatch
drawn in the current accent's slots would show eight identical rows.

From a prompt, use `kdos theme`:

```sh
kdos theme              # print the accent in force (also: kdos theme show, kdos theme current)
kdos theme list         # the eight accents with their theme names; the current one is marked *
kdos theme amber        # switch to amber
kdos theme next         # the next accent in the list, wrapping at the end
kdos theme prev         # the previous accent
kdos theme --preview X  # repaint the desktop only; write nothing else (see below)
kdos theme --audit [X]  # compare installed theme files with the palette (or: kdos theme audit)
kdos theme style FILE   # apply a style file
kdos update theme       # re-run the generators for the accent in force
```

A switch prints the accent and its theme name, such as `amber (KDOS-Amber)`. If the root power
service cannot be reached or refuses the request, a line saying the boot menu and splash are
unchanged is printed first (see
[The boot menu, the splash and the text consoles](#the-boot-menu-the-splash-and-the-text-consoles)).
With no accent ever chosen, or with an unreadable accent file (`~/.cache/kdos/theme`, described
below), the accent in force is `bone`.

`kdos update theme` exists for package upgrades. The icon, cursor and GTK packages install new
source artwork, but the copies in your home directory were generated from the old artwork and stay
as they are until something regenerates them. `kdos update theme` does that for the accent you
already have, exactly as `kdos theme <current accent>` would.

`kdos-style` is the picker and `kdos-theme` is the generator that writes the GTK, icon and cursor
themes. They are two separate programs; the picker never switches an accent itself, it runs
`kdos theme`. The generator is described under
[How the theme is generated](#how-the-theme-is-generated).

### What repaints at once, and what waits

The desktop keeps no theme file of its own. Every KDOS program links `libkcolor`, so it carries all
eight palettes, and reads a single word, the accent name, from `~/.cache/kdos/theme` (under
`$XDG_CACHE_HOME` if you set it). A switch writes that word and then signals the session: it sends
`SIGHUP` to the compositor (`kdos-comp`), the panel, the desktop icons (`kdos-desk`), the
notification daemon, the dock-app column (`kdos-slit`), the resource monitor (`kdos-res`) and the
terminal (`kdos-term`). Every other KDOS window, such as an open menu or Settings, notices the
changed file on its own. So the panel, the desktop icons, notifications on screen, window frames,
open popups and the phosphor shader all change together. While the phosphor pass is on, the
compositor marks the change with a brief distortion of the whole picture, like the degaussing pulse
of an old CRT monitor, unless `motion = no` is set in `comp.conf` (see
[Accessibility](accessibility.md#reducing-motion)).

A *preview* does only that half. Moving the highlight in the picker (or running
`kdos theme --preview <accent>`) writes the accent name and signals the session, so every KDOS
surface repaints, but it regenerates nothing else. The GTK stylesheet, the icon theme, the cursors
and the configuration files for other programs take seconds to write and are read only when those
programs start, so during a preview the desktop changes and a boxed application does not. The
settings portal announces an accent change to running applications only once the GTK theme for that
accent exists on disk, so a running boxed application keeps its colours through a preview. An
application started during a preview asks the portal and is given the previewed theme name,
`KDOS-<accent>`, which does not exist until the switch is kept. Pressing `Enter` runs the full
switch, which brings everything else into line. Leaving the picker any other way, including
stopping it with `SIGTERM` or `SIGINT`, puts the original accent back.

Night light (`Super+Ctrl+Shift+N`, or `kdos toggle night-light`) warms the palette itself rather
than adding a filter, so it reaches every surface on the same signal as an accent switch.

## What changes when

Besides the accent name, `kdos theme` writes a file for each program that does not link
`libkcolor`, and nearly all of those are programs KDOS did not write. A few of them can be told to
reload, but most read their colours once, when they start. Each file therefore takes effect at a
different moment:

| File | Read by | Takes effect |
|---|---|---|
| `~/.cache/kdos/theme` (the accent name) | Every KDOS surface, the compositor, the settings portal | Immediately, on the signal |
| `~/.config/kdos-comp/themerc-override` | The compositor's window frames | Immediately, same signal |
| `~/.cache/kdos/wallpaper.png` | The compositor | Immediately, same signal |
| `~/.config/tmux/themes/kdos.conf` | tmux | Immediately, if a tmux server is running (`kdos theme` re-sources `tmux.conf`) |
| The palette block in `~/.config/starship.toml`, between its two markers | starship | At the next shell prompt |
| `~/.config/foot/themes/kdos` | foot | In the next terminal; foot cannot reload its configuration |
| `~/.config/kdos/term-colors.conf` | `kdos-term`'s sixteen terminal colours | In the next terminal; the window's own frame follows the accent at once |
| `~/.config/btop/themes/kdos.theme` | btop | Next start |
| `~/.config/kdos/fzf-colors` | fzf, through `$FZF_DEFAULT_OPTS` (appended by `/etc/profile.d/30-kdos-colors.sh`) | At the next login shell |
| `~/.config/bat/themes/kdos.tmTheme` | bat | Next start; `kdos theme` rebuilds bat's cache itself |
| `~/.config/micro/colorschemes/kdos.micro` | micro | Next start |
| `~/.config/helix/themes/kdos.toml` | helix, if you install it in a box (it is not on the host) | Next start |
| `~/.config/nvim/colors/kdos.vim` | neovim | Next start |
| `~/.config/git/kdos-delta` | delta, included from the shipped git configuration | At the next diff |
| `~/.config/newsboat/kdos-colors` | newsboat, `include`d from its configuration | Next start |
| `~/.config/aerc/stylesets/kdos` | aerc | Next start |
| `~/.config/yazi/theme.toml` | yazi | Next start |
| `~/.local/share/mc/skins/kdos.ini` | mc, selected by `skin = kdos` under `[Midnight-Commander]` in `~/.config/mc/ini` | Next start |
| `~/.config/kdos/ls-colors` | `ls`, through `$LS_COLORS`, loaded by `.bashrc` | In the next shell |
| `~/.themes/KDOS-<accent>/` | GTK3 applications, native and in boxes | At once, in windows already open |
| `~/.config/gtk-{3,4}.0/settings.ini` | GTK, when the settings portal cannot be reached | The application's next launch |
| `~/.config/gtk-4.0/gtk.css` | libadwaita applications | The application's next launch |
| `~/.icons/KDOS/` | Every toolkit, on the host and in boxes | The application's next launch |
| `~/.icons/KDOS-cursors/` | Cursors inside boxes | The application's next launch |
| `~/.config/kdeglobals` | Qt applications using the KDE platform theme | The application's next launch |
| `~/.local/share/color-schemes/KDOS.colors` | KDE's own appearance settings, as a scheme you can choose | The application's next launch |
| `~/.config/qt5ct/qt5ct.conf`, `~/.config/qt5ct/colors/KDOS.conf` | Qt 5 applications, under the `qt5ct` platform theme or the session's `kde` value, which qt5ct also answers | The application's next launch |
| `~/.config/qt6ct/qt6ct.conf`, `~/.config/qt6ct/colors/KDOS.conf` | Qt 6 applications under the `qt6ct` platform theme | The application's next launch |
| `/etc/kdos/accent` | The boot splash, retinted during startup | The next boot |
| `/boot/efi/limine.conf` | The boot menu | The next boot |
| `/etc/vtrgb` | Every text console: the login prompt, `/etc/issue`, the login banner | The next login prompt |

The paths under `~/.config` and `~/.local/share` follow `$XDG_CONFIG_HOME` and `$XDG_DATA_HOME`
when those are set.

**Why GTK3 applications repaint while open.** GTK rebuilds its styles only when the name of the
theme changes, so rewriting a stylesheet under the same name would reach only the next launch.
`kdos theme` therefore writes the theme into a directory named after the accent,
`~/.themes/KDOS-<accent>`, deletes the directories for every other accent, and points the symlink
`~/.themes/KDOS` at it. The settings portal announces the new theme name (see
[Theming applications inside boxes](#theming-applications-inside-boxes)), and every running GTK3
application that reads the portal repaints, on the host and in a box.

Everything else picks up the change when you next start it. GTK does not re-read icons or a user
stylesheet when those files change, and no toolkit offers a way to force it from outside.

### Files you may edit, and files you may not

Never edit a generated file: every file in the table above is rewritten on each accent switch, and
most of them carry a header saying so. Each is replaced whole, apart from the files that are merged
(see below) and `limine.conf`, of which only the lines describing the menu's look change. Most of
the files in your home directory are *selected* by a second file that ships once and then belongs
to you. Change these freely; `kdos theme` does not touch them, apart from the starship palette
block noted in the table:

| Program | File | The line that selects the KDOS theme |
|---|---|---|
| micro | `~/.config/micro/settings.json` | `"colorscheme": "kdos"` |
| neovim | `~/.config/nvim/init.vim` | `colorscheme kdos` |
| delta | `~/.config/git/config` | an `[include]` of `kdos-delta` |
| newsboat | `~/.config/newsboat/config` | `include "~/.config/newsboat/kdos-colors"` |
| aerc | `~/.config/aerc/aerc.conf` | `styleset-name=kdos` |
| bat | `~/.config/bat/config` | `--theme="kdos"` |
| btop | `~/.config/btop/btop.conf` | `color_theme = "kdos"` |
| foot | `~/.config/foot/foot.ini` | `include=~/.config/foot/themes/kdos` |
| tmux | `~/.config/tmux/tmux.conf` | a `source-file` of `themes/kdos.conf` |
| starship | `~/.config/starship.toml` | `palette = "kdos"`; only the block between the markers is rewritten |

To keep the accent but change one colour, build on the generated theme: your own micro scheme may
`include "kdos"`, and your own helix theme may say `inherits = "kdos"`.

helix is not installed on the host. The theme is still written, because a box shares your home
directory by default, and a helix you install in a box reads it. Nothing selects it for you: add
`theme = "kdos"` to your own `~/.config/helix/config.toml`.

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

Five files are merged rather than replaced: `~/.config/starship.toml` (only the block between its
markers), `~/.config/kdeglobals` (see
[Theming applications inside boxes](#theming-applications-inside-boxes)), `~/.config/mc/ini`
(only the `skin` key), and `~/.config/qt5ct/qt5ct.conf` and `~/.config/qt6ct/qt6ct.conf` (only
the palette, icon theme, style, dialog and font keys; the rest is what the qt5ct and qt6ct
programs saved for you).

One file of yours is removed rather than kept: `~/.config/gtk-3.0/gtk.css` is deleted on every
accent switch, because GTK3 loads it once and it would hold the old accent for the life of every
GTK3 program. A GTK3 stylesheet of your own does not survive an accent switch; change GTK3's look
through the theme's accent instead.

## The boot menu, the splash and the text consoles

Three things outside your session also follow the accent: the boot menu lives in `limine.conf` on
the EFI system partition, the boot splash reads `/etc/kdos/accent`, and every text console reads
`/etc/vtrgb`. They belong to root, and they are settings for the machine rather than for your
session, so `kdos theme` asks the root power service, `kdos-powerd`, to update them (through
`kdos-power accent <name>`). The daemon accepts the request from root and from members of the
`wheel` group, and accepts only one of the eight accent names, so there is no path or free text for
a caller to supply. It writes `/etc/kdos/accent` itself and runs `kdos-bootctl theme <accent>` for
the other two.

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

The three change at different moments. The boot menu changes on the next boot, the console palette
at the next login prompt (log out of a console, or reboot), and the splash partway through the next
boot. The splash starts from the initramfs, before the root filesystem is mounted, so it comes up
in the accent built into the initramfs; the startup script `/etc/init.d/rcS` then passes it the
machine's accent from `/etc/kdos/accent` as soon as `/etc` can be read. If you have never changed
the accent, the two are the same and you see nothing.

`kdos-bootctl` rewrites only the lines of `limine.conf` that describe the menu's look, never the
timeout or the boot entries, and writes the file through a temporary copy so a power cut cannot
leave it empty. Three situations leave the boot menu as it is:

- A machine with no bootloader configuration, such as the live medium or any machine with no
  `/boot/efi/limine.conf`, has no boot menu to recolour. The splash and the console palette still
  follow the accent.
- When a `limine.conf` exists but cannot be rewritten, or `kdos-bootctl` is missing, the splash is
  still updated (and the console palette too, unless `kdos-bootctl` is missing). The power service
  replies `ok <accent> (boot menu unchanged)`, which `kdos theme` does not display.
- When `kdos-powerd` does not answer (inside a box, where the daemon cannot be reached) or refuses
  the request (for an account outside `wheel`), the boot menu, the splash and the console palette
  are all left as they are, and `kdos theme` prints
  `boot menu and splash unchanged (kdos-powerd did not answer)`, preceded by `kdos-power`'s own
  one-line reason on standard error. If `kdos-power` is not installed at all, nothing is printed
  and the three are left as they are.

In each of these situations, every other file in the table in [What changes
when](#what-changes-when) is still written, and `kdos theme` exits without an error.

The boot menu's background picture does not follow the accent. The bootloader cannot recolour a
picture, and writing a new image to a FAT filesystem on every theme change risks a half-written
file if the power fails, so the backdrop is a single dimmed, colourless image that suits every
accent.

## The phosphor pass

The compositor draws the whole desktop through a phosphor shader: optional scanlines, a horizontal
bleed, a vignette, optional barrel distortion, and a faint glow so black is never quite black. The
glow takes the accent's primary colour. The pass is on by default.

Set these in `~/.config/kdos/comp.conf` or on the Appearance page of Settings:

| Key | Default | Range | What it does |
|---|---|---|---|
| `crt` | `55` | `0`–`100` | Overall strength, in percent. `0` switches the pass off completely |
| `crt_scanlines` | `0` | `0`–`100` | Scanline depth, in percent: a dark line on every third physical row. `60` is the strength the rest of the pass is designed around |
| `crt_curve` | `0` | `0`–`100` | Barrel distortion, in percent. Scaled so that no value crops the desktop |
| `crt_fullscreen` | `on` | `on`, `off` | `off` skips the pass on a screen whose topmost window on the current workspace (minimised windows aside) is fullscreen, focused or not |

Settings applies a change at once. After editing the file by hand, apply it with
`kdos-comp --reconfigure`, or log in again. A value outside its range is reported in the
compositor's log and ignored.

Scanlines are off by default because the desktop is a grid of 16×32-pixel cells and a line on
every third row does not line up with it, so text comes out striped.

### What it costs

While the pass is on, every frame goes through the GPU twice: there is a second set of frame
buffers per screen (a single 4K frame buffer is about 33 MB), and the shader runs over the part of
the screen that changed, so a blinking cursor or a clock tick costs a strip rather than the whole
screen. Barrel distortion moves every pixel, so with `crt_curve` above `0` the shader runs over the
whole screen on every frame. Direct scanout, where a fullscreen video's frames go straight
to the display without being composited, never happens on a frame the pass draws. On battery,
`crt = 0` or `crt_fullscreen = off` is the lever, and either takes effect at once: the frame goes
out without the shader, and a fullscreen window's frames can then go straight to the display with
no composite at all. `crt_fullscreen = off` is also what lets a game that asks to tear do so, and
what keeps the rest of the desktop in the pass. See
[kdos-comp](../04-programs/kdos-comp.md#fullscreen-scanout-variable-refresh-and-tearing) for the
cases the display still refuses.

### Changes that need a new session

Two parts of the pass are decided when the compositor starts:

- A session that started with `crt = 0` has no pass to tune. Raising `crt` later is noted in the
  log and takes effect at the next login.
- Barrel distortion needs the pointer drawn in software, because the hardware cursor plane is laid
  over the picture after the shader and would not follow the curve. The compositor makes that
  choice at startup when `crt_curve` is above `0`, so curvature switched on mid-session leaves the
  pointer drifting away from where it points, towards the edges, until the next login.

### When it switches itself off

The pass runs only on the GPU (GLES2) renderer. Where there is no GPU acceleration, such as a
virtual machine without 3D (virgl) enabled, it is off, because a full-screen effect on software
rendering drops to a few frames a second. If the pass fails on one
screen while running, that screen falls back to the plain picture for a while and retries later.
It also steps aside while the screen magnifier is on, since zoomed text is harder to read through
the effect. Each of these is recorded in the compositor's log, `$XDG_RUNTIME_DIR/kdos-comp.log`,
as a line beginning `crt:`. (For developers: `make run` boots without 3D and shows no pass,
`make run-hw` boots with virgl and does; see [Developing](../05-developer/developing.md).)

### Screenshots

A screenshot of a whole screen captures the shaded image, so it looks like what you see. A screenshot of a single window captures the window itself, without the shader.

## Wallpaper

The compositor draws the wallpaper, and it follows the accent: `kdos theme` recolours the shipped
image, `/usr/share/backgrounds/kdos/default-wallpaper.png`, into the current palette and writes the
result to `~/.cache/kdos/wallpaper.png` (under `$XDG_CACHE_HOME` if you set it). The file is
written beside the old one and renamed over it, so the compositor never reads a half-written
image.

The compositor chooses what to draw like this:

1. `wallpaper = none` in `comp.conf`: no wallpaper; the background is the accent's background
   colour. The cache is never used.
2. Otherwise, if `~/.cache/kdos/wallpaper.png` exists, it is drawn.
3. Otherwise, the file named by `wallpaper =` is drawn (default
   `/usr/share/backgrounds/kdos/default-wallpaper.png`). If it cannot be read, the background is
   the accent's background colour.

So to use your own picture:

1. Set `wallpaper = /path/to/picture.png` in `~/.config/kdos/comp.conf`, or on Settings'
   Appearance page (the desktop menu's **Change Wallpaper** opens it). The path may begin with `~/`
   or `$HOME/`.
2. Delete `~/.cache/kdos/wallpaper.png`.

The next accent switch writes the cache again and your picture is replaced by the recoloured
shipped one; delete the cache again afterwards. The wallpaper must be a PNG. It is scaled to cover
the screen and centred, so a picture of a different shape is cropped rather than stretched.

The shipped image has no scanlines drawn into it, because the shader adds them and two sets of
scanlines would interfere with each other.

## Style files

A style file bundles an accent with related settings so you can share a whole look:

```sh
kdos theme style retro.kdos
```

A style file is plain `key = value` lines; `#` starts a comment. It may contain:

| Key | Effect |
|---|---|
| `accent` | The accent to switch to. Without it, the current accent is kept |
| `crt`, `crt_scanlines`, `crt_curve`, `crt_fullscreen` | Written to `comp.conf`; applied on the same signal as the accent |
| `chrome_font`, `clock_format` | Written to `comp.conf`; they reach the desktop at the next login |
| Any key containing a dot, such as `osd.bg.color: #202020` | A window-frame theme setting (the `themerc` vocabulary of labwc, the compositor `kdos-comp` is built on). Kept in `~/.config/kdos/style-themerc` and added after the generated frame theme, so it wins |

Either `=` or `:` may separate a key from its value; whichever comes first on the line is used,
so `chrome_font = Terminus (TTF):pixelsize=64` keeps its colon. A line that is neither `key = value` nor
`key: value`, and a key that is none of the above, is reported and ignored. An unknown accent
stops the command before anything is written.

For example:

```ini
# retro.kdos
accent = amber
crt = 70
crt_scanlines = 60
chrome_font = Terminus:pixelsize=32
```

A style changes only the `comp.conf` keys it names; a key the file does not have yet is added at
the end, and every other line is kept exactly as it was. The dotted keys behave differently: a
style replaces the whole of `style-themerc`, and a style with no dotted keys deletes it, because a
style is a whole look rather than a patch on the previous one. The saved lines survive later plain
accent switches. After writing its settings, a style runs the same full switch as
`kdos theme <accent>`.

## Fonts

Three separate settings decide what the desktop is drawn in. They live in different files and
none of them falls back to another.

**The console font** is a 16×32-pixel bitmap font, `ter-kdos32n`, built from Terminus by the
`terminus-font` [port](../06-reference/glossary.md) and loaded by `kdos-getty` onto each virtual
terminal before it clears the screen. It has 512 glyphs, which is why parts of KDOS limit themselves
to a small set of characters: anything outside it shows as a blank on `tty1`.

**`chrome_font` and `panel_font`** in `~/.config/kdos/comp.conf` are the desktop's own fonts, as
fontconfig patterns with a size in pixels:

| Key | Default | Used by |
|---|---|---|
| `chrome_font` | `Terminus:pixelsize=32` | The surfaces the compositor starts and supervises: the desktop icons, the dock-app column, notifications and the Wi-Fi passphrase prompt |
| `panel_font` | `Terminus:pixelsize=20` | The panel bar only, not the popups it opens |

The compositor passes these fonts to the surfaces it starts. Windows opened from the panel, from
the menu or from a key binding (menus, Settings, the picker itself) receive no font from it and
draw in the built-in default, `Terminus:pixelsize=32`, whatever `chrome_font` says. An empty value
(`panel_font =`) is ignored with a note in the log, and the default applies.

The panel's font sets its height: a cell is half as wide as the font is tall, so the default
20-pixel font gives a 10×20 cell and a two-row panel 40 pixels high. Terminus is a bitmap font, so
name a size it has (12, 14, 16, 18, 20, 22, 24, 28 or 32); a size between them, or from 10 up to
35, is rounded to the nearest one it does have. 64, 96 and every further multiple of 32 are the 32
size with every pixel doubled, tripled and so on. Any other size (8, 9, and 36 upwards) is drawn
from `Terminus (TTF)`, the scalable version of the same typeface, in a cell exactly as tall as the
size and half as wide, rounded up at an odd size.

Each key is a single pixel size for every screen. That is right on a machine with one monitor and
wrong on two of different densities. On a 4K screen left at scale 1, `chrome_font =
Terminus:pixelsize=64` doubles the cell for the surfaces that read it. A screen given a scale in
`displays.conf` scales every surface by itself, so the default is already right there: scale 2
doubles it, and a fraction such as 1.5 draws it from `Terminus (TTF)` at 48 pixels rather than
stretching the 32 size. Each surface follows the scale of the screen it is on, which is how two
monitors of different densities are served.

Several things write these two keys: the picker's Font page (see [Choosing a
font](#choosing-a-font)) sets the family on both and keeps each size; Settings sets either one whole
(`chrome_font` on its Appearance page, `panel_font` on its Panel page); a style file may set
`chrome_font`. Whichever wrote it, each surface reads the value once as it starts, so the desktop
agrees at the next login.

**The compositor's own chrome**, meaning title bars, its menus and the window-switcher display, is
set separately, in `~/.config/kdos-comp/rc.xml`, in *points*. There is one `<font>` line for each
of the five places it draws: `ActiveWindow`, `InactiveWindow`, `MenuHeader`, `MenuItem` and
`OnScreenDisplay`.

These surfaces are drawn with pango, which cannot draw bitmap fonts, so this font must be a
*scalable* one. Asking for the bitmap `Terminus` here silently falls back to a generic sans-serif
font. The shipped file names `Terminus (TTF)`, the same design converted to TrueType (the
`terminus-ttf` port), at 24 points on all five rows:

```xml
<font place="ActiveWindow"><name>Terminus (TTF)</name><size>24</size></font>
```

24 points is 32 pixels at 96 dpi, so a title bar is exactly one cell tall.

### Choosing a font

The picker's second page (see [Switching the accent](#switching-the-accent) for the picker itself)
chooses the typeface of `chrome_font` and `panel_font`. `Left` and `Right` move between the Accent
and Font pages, and the `style.font` route (`kdos-style --page font`) opens straight onto the Font
page.

The list shows the monospace font families fontconfig knows, one row per family, sorted by name
and limited to the first 64. Proportional fonts are not offered, because the desktop is a grid of
equal-width cells and a proportional face would draw every column at a different width. Each row
shows the family name, cut to 28 characters, beside the sample `AaBbGg 0O1lI {}[]()`: an ascender,
a descender, the digits and letters most often confused, and the brackets a terminal user reads.

The highlight starts on the font in use, or on the first row if the font in use is an alias such
as `monospace`, which fontconfig resolves but never lists. Moving the highlight switches the
picker's own window to that font, so the whole window is the sample. A font that fails to load is
not applied. Leaving any way other than `Enter`, or a click on the highlighted row, puts the
original font back.

`Enter` on the Font page writes the chosen family into both `chrome_font` and `panel_font` in
`~/.config/kdos/comp.conf`, keeping the pixel size each already had. A key the file does not set
yet is added with its default size (32 pixels for `chrome_font`, 20 for `panel_font`). A commented
line such as `#chrome_font = …` is left as a comment, and every other line in the file (comments,
blank lines and keys the picker does not know) is kept exactly as it was.

The new font appears in the picker at once but reaches other surfaces only when each next starts,
in practice at your next login. Each surface loads its font once, as it starts, and a running
surface cannot switch fonts. (The accent is different: it repaints everything on a signal.) Which
surfaces read these two keys is in the table above.

If the list is empty, the page says so and names the two keys. That means either the machine has
no monospace font installed, or the picker is running on a display that does not offer fonts,
such as a terminal that controls its own font.

## Theming native applications

The applications ported natively run on the host and read the same files in your home directory
as boxed ones, through two variables that `/etc/profile.d/10-wayland.sh` exports for every login:

| Variable | Effect |
|---|---|
| `GTK_USE_PORTAL=1` | GTK reads its theme, icon, cursor and font names from the settings portal, and opens the portal's file chooser instead of its own dialog, so a running GTK3 application restyles on an accent switch |
| `QT_QPA_PLATFORMTHEME=kde` | A Qt 6 application loads KDE's platform theme (the `plasma-integration` port) and reads the palette, fonts and icons from `~/.config/kdeglobals` |

The variable names one platform theme, not a list, and each Qt major version searches only its own
plugin directory. KDE's platform theme is built for Qt 6 only; the `qt5ct` port's plugin also
answers the name `kde`, so a native Qt 5 application loads qt5ct under the same value and reads the
generated `qt5ct` files, which are described under
[Theming applications inside boxes](#theming-applications-inside-boxes).

## Theming applications inside boxes

A box has its own `/usr`, from its own distribution, so the host's `/usr/share/themes` and
`/usr/share/icons` are invisible inside it. What a box does share by default is your home
directory, at the same path on both sides. So everything a boxed application reads for its theme
is written into `$HOME`:

| Path | Read by |
|---|---|
| `~/.themes/KDOS-<accent>/` | GTK3, and GTK4 applications that do not use libadwaita. `~/.themes/KDOS` is a symlink to it |
| `~/.config/gtk-{3,4}.0/settings.ini` | The theme, icon and cursor names, when the settings portal cannot be reached |
| `~/.config/gtk-4.0/gtk.css` | libadwaita, which ignores themes and reads the palette from here. GTK3 has no equivalent (see [Files you may edit, and files you may not](#files-you-may-edit-and-files-you-may-not)) |
| `~/.icons/KDOS/` | Every toolkit |
| `~/.icons/KDOS-cursors/` | Cursors |
| `~/.config/kdeglobals` | Qt, under the KDE platform theme |
| `~/.config/qt5ct/`, `~/.config/qt6ct/` | Qt, under the qt5ct or qt6ct platform theme |

A box created with `home = private` has a home of its own and sees none of these files; see
[kdos-appbox](../04-programs/kdos-appbox.md).

The generated `settings.ini` also fixes the pointer size for GTK: it names the `KDOS-cursors` theme
with `gtk-cursor-theme-size=24`, the same 24 pixels that `/etc/profile.d/10-wayland.sh` exports as
`XCURSOR_SIZE`. `kdos theme` rewrites both `settings.ini` files whole on every accent switch, so a
different size typed into either is lost at the next switch.

**The settings portal.** `xdg-desktop-portal-kdos` answers the settings requests toolkits make: the
GTK theme name (`KDOS-<accent>`), the icon theme (`KDOS`), the cursor theme (`KDOS-cursors`) and
the accent colour, all read from the same accent file the desktop reads. It watches that file and
announces each change, which is what lets a running GTK3 application restyle (see
[What changes when](#what-changes-when)). It reports a preference for a dark colour scheme for
every accent, including `paper`, and so does the generated `settings.ini`. It also names the
fonts, `Noto Sans 10` and `Noto Sans Mono 10`, the faces fontconfig already uses for `sans-serif`
and `monospace`, and answers `contrast` with "no preference". The full list of keys is in
[The session](../03-architecture/session.md#the-kdos-backend).

A GTK application on the host started without `GTK_USE_PORTAL` reads GSettings instead, and the
image ships `/usr/share/glib-2.0/schemas/90_kdos.gschema.override`, which makes the same theme,
icon, cursor, colour-scheme and font answers the GSettings defaults. It names the theme `KDOS`, the
link that always points at the current accent, so such an application wears the accent it started
in and changes at its next launch. A value you set yourself with `gsettings set` wins over the file.

The GTK theme is a recoloured `adw-gtk3`. GTK3's own Adwaita theme has literal colour values in
nearly every rule, so redefining its named colours reaches only a few widgets. `adw-gtk3` is the
libadwaita stylesheet ported to GTK3 and uses named colours throughout, so rewriting the palette
recolours every widget, and GTK3 applications look the same as GTK4 ones.

**Qt** has two routes, and which one an application takes depends on what its
[runtime](../06-reference/glossary.md) (a layer shared by many applications that holds a toolkit)
provides. Where the runtime includes KDE's platform integration, `QT_QPA_PLATFORMTHEME=kde` makes
Qt applications read `~/.config/kdeglobals` directly. Where it does not, the GTK platform theme
with the Fusion style is used instead. The runtime declares the variable in the `env =` lines of its
own pack metadata, so the value travels with the packages that make it work instead of being
guessed; a wrong guess would leave the application on Qt's built-in light palette.

`kdos-appbox` exports those lines only for a box built from a pack: one whose base is `pack:`, or
one named after an installed pack. A box the store builds with `kdos app install` is named after
an application no installed pack carries, so it gets no `QT_QPA_PLATFORMTHEME`, and its Qt
applications fall back to Qt's defaults instead of the accent. The catalogue's own `env` rows for
`rt-qt` and `rt-kde` are not exported into it. The full list of what a box receives is in
[The environment a box gets](../04-programs/kdos-appbox.md#the-environment-a-box-gets).

**`kdeglobals` is merged, not overwritten.** KDE applications save their own settings in that
file, so only what the theme owns is replaced and everything else is kept. The theme owns the
`[Colors:*]` and `[WM]` sections, and six keys elsewhere: `ColorScheme` and `Name`, the fonts
`font` (`Noto Sans,10`) and `fixed` (`Noto Sans Mono,10`) under `[General]`, `widgetStyle=Breeze`
under `[KDE]`, and `Theme=KDOS` under `[Icons]`.

**Qt without the KDE platform theme.** The KDE platform theme is built for Qt 6 only, so a Qt 5
application cannot read `kdeglobals`. `kdos theme` also writes the files of qt5ct and qt6ct, the
platform themes an application loads when `QT_QPA_PLATFORMTHEME` is `qt5ct` or `qt6ct`, and which a
native Qt 5 application on the host loads under `kde` as well: in each
tool's directory, `colors/KDOS.conf` holds the palette, with a selected row in the same colours as
GTK's, and `qt5ct.conf` or `qt6ct.conf` selects it with `custom_palette=true`, sets the `KDOS` icon
theme, the portal's file chooser and the two Noto fonts, and names the style: `Fusion` for Qt 5,
because the Breeze style is built for Qt 6 only, and `Breeze` for Qt 6. The palette path is
written as `~/.config/...`, so a copy seeded from `/etc/skel` points into each user's own home.

**Icons the KDOS set does not have.** The generated `~/.icons/KDOS/index.theme` inherits from
`breeze-dark`, `Adwaita` and `hicolor` in that order, or from `breeze` instead of `breeze-dark`
under `paper`, the one light accent: breeze's monochrome icons are drawn for one background, and
the other one leaves them invisible. A KDE or GNOME application that asks for an icon the KDOS set
lacks gets the Breeze or Adwaita one before the application's own. A parent theme that is not
installed, on the host or in a box's own `/usr`, is skipped.

## How the theme is generated

The artwork comes from three data packages, each copied into the repository from an upstream project
and recoloured:

| Package | Upstream | Source artwork installed to | Generated system copy |
|---|---|---|---|
| `kdos-icons` | Papirus icons | `/usr/share/kdos/icons/` (`art/`, `marks/`, and the desktop's pixel-icon atlas `atlas.kia`) | `/usr/share/icons/KDOS` |
| `kdos-cursors` | Bibata cursors | `/usr/share/kdos/cursors/art` | `/usr/share/icons/KDOS-cursors` |
| `kdos-gtk-theme` | adw-gtk3 | `/usr/share/kdos/gtk-theme/theme` | `/usr/share/themes/KDOS` |

`kdos-theme` is the generator that recolours them. The packages run it once at build time to make
the system copies, always in `phosphor`. Those copies never follow the accent, and nothing in a
session or a box reads them in preference to the copies in your home directory. The four ports
(`kdos-theme` and the three above) are listed in
[The ports catalogue](../06-reference/ports-catalogue.md#colour-management-and-codecs). `kdos theme` runs
the generator on every accent switch to write the copies in `$HOME`. The image build runs
`kdos theme` once more against `/etc/skel`, with the default accent, so a new account starts
themed; [How KDOS is built](../05-developer/how-kdos-is-built.md#packaging-06_packaging) places that
step among the other packaging steps.

```sh
kdos-theme gtk     <out-dir> [accent] [--src DIR]
kdos-theme icons   <out-dir> [accent] [--src DIR] [--marks DIR]
kdos-theme cursors <out-dir> [accent] [--src DIR]
kdos-theme accents
```

| Argument | Default |
|---|---|
| `accent` | `bone`, the default accent |
| `--src` for `gtk` | `$KDOS_GTK_SRC`, else `/usr/share/kdos/gtk-theme/theme` |
| `--src` for `icons` | `$KDOS_ICON_ART`, else `/usr/share/kdos/icons/art` |
| `--marks` for `icons` | `$KDOS_ICON_MARKS`, else `/usr/share/kdos/icons/marks` |
| `--src` for `cursors` | `$KDOS_CURSOR_ART`, else `/usr/share/kdos/cursors/art` |

`kdos-theme accents` prints the eight accent names.

The icons and cursors are recoloured rather than redrawn. A maintenance script (`vendor.py`, beside
each package's recipe under `src/packages/`) prunes an upstream release into a source tree kept in
the repository, and the generator recolours that tree into the palette. Each colour keeps its own
lightness, and usually its saturation, and takes a hue from the palette.

Colours are mapped by hue family, not all flattened onto the accent: blues, greens and purples
become the accent (cyans at reduced saturation, so they read as a shade of it), yellows, oranges
and browns become the secondary colour, reds become the urgent colour, and near-greys are tinted
faintly. Papirus colour-codes file types, and turning every hue into the accent would make a folder
of mixed files look like a wall of identical shapes. So a PDF stays red (the urgent colour is a red
in every accent), an audio file takes the secondary colour, and folders and devices take the
accent.

**Application icons.** The copied icon set has no application icons, so the generator builds them
by scanning every size directory of `/usr/share/icons/hicolor`, not only `scalable/`. It takes only
icons whose names start with `kdos.`. Applications, native and boxed, install their own icons
into the same directory, and those belong to their projects: a Firefox logo recoloured into
the accent would stop being Firefox's own mark, so it is left alone.

### Auditing what is installed

```sh
kdos theme --audit          # do the installed files match what the current accent produces?
kdos theme --audit amber    # what would switching to amber change?
```

The audit runs the same generators, with `$HOME` and the XDG directories pointed at a scratch
directory under `$TMPDIR` (default `/tmp`), and compares the results byte for byte with what is
installed, symlinks included. It first copies in the files that are merged rather than written
(`starship.toml`, `kdeglobals`, `mc/ini`, `qt5ct/qt5ct.conf`, `qt6ct/qt6ct.conf`) and any saved
style lines, so your own settings in them are not reported as differences. For each generated
file or tree it prints `matches the palette`, `not generated on this machine`, or `DRIFTED` with
counts of files that differ, are missing, or are present but not generated. Anything reported as different is different from what your palette
produces.

The audit covers the files in your home directory. It does not check the wallpaper cache, the
accent state file, the root-owned boot and console files, or the phosphor-coloured system copies
under `/usr/share`. It writes nothing outside its scratch directory and repairs nothing; run
`kdos theme <accent>` to repair.

| Exit status | Meaning |
|---|---|
| `0` | Everything matches |
| `1` | Something differs |
| `2` | The audit could not run |

## See also

- [The design language](../03-architecture/design-language.md): the palette slots and the contrast rules
- [kdos-comp](../04-programs/kdos-comp.md): the phosphor pass, the wallpaper and the frame theme
- [kdos-shell](../04-programs/kdos-shell.md#kdos-style): the picker, `kdos-style`, among the other surfaces
- [The desktop](desktop.md): the picker's shortcut, the panel and its fonts
- [Accessibility](accessibility.md): the magnifier and the font size for large screens
- [Configuration](../06-reference/configuration.md): every key named here, with defaults
- [Applications](applications.md): why boxed software is themed through `$HOME`
- [The daemons](../04-programs/daemons.md#kdos-powerd): `kdos-powerd`, which writes the boot and console files
- [The kdos command](../04-programs/kdos-command.md): `kdos theme` in full
- [The ports catalogue](../06-reference/ports-catalogue.md): the theme, icon, cursor and font ports
- [How KDOS is built](../05-developer/how-kdos-is-built.md): where the default theme is seeded into `/etc/skel`

<!-- book-nav -->
---

*Part II — Using KDOS, chapter 9.* Previous: [8. Applications](applications.md) · [Contents](../README.md) · Next: [10. Administration](administration.md)
