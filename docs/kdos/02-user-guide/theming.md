# Theming

This page is for anyone who wants to change how KDOS looks: the colour scheme (called an
*accent*), the phosphor shader that gives the screen its CRT look, the wallpaper, the fonts, and
how boxed applications pick up the same colours. It also describes the tools that generate the
theme files, for anyone who wants to audit or rebuild them.

The short version: press `Super+Ctrl+Shift+Space`, pick an accent, press `Enter`. The whole desktop
repaints at once, with no restart and no logout. The rest of this page explains what that one key
changes, when each change reaches each program, and the settings around it.

## The eight accents

| Accent | Character |
|---|---|
| `phosphor` | Green on near-black. The project's own colour |
| `amber` | Amber on warm black |
| `ice` | Cyan and blue on deep blue-black |
| `bone` | Warm off-white on near-black, the least saturated of the eight. **The default** |
| `norton` | Yellow and cyan on deep blue. The two-pane file manager's own colours |
| `borland` | Cyan and yellow on dark teal. The blue-box IDEs of the late eighties |
| `perfect` | White on blue, and almost nothing else. The word processor that showed a blank screen |
| `paper` | Ink on paper — the one light accent |

An accent is not a single colour. It is a small palette of nine named colours: a primary, a dim
version of it, a secondary, an urgent colour, a background, a text colour, a surface colour, a
darker primary and a backdrop. Everything KDOS draws takes its colours from named roles in that
palette (called *slots*, see the [glossary](../06-reference/glossary.md)) rather than from fixed
colour values, which is why changing one word repaints the entire desktop.
[The design language](../03-architecture/design-language.md) describes the slots.

### How the accents are kept readable

Every accent is held to contrast rules by an automatic test, so none of them produces text you
cannot read:

- Text against the background must reach 7:1.
- The accent colour against the background must reach 4.5:1, the WCAG AA level for normal-size
  text.
- The highlighted row in a menu (the *plate*) must both stand out from the bar behind it and keep
  its own label readable. The label is aimed at 7:1 and must reach at least 4.5:1.

Every accent, light or dark, keeps a highlighted menu row readable at 4.5:1 or better; `bone`,
the most muted, is the lowest at about 6.2:1. How the plate colours are chosen is in
[Design language](../03-architecture/design-language.md).

## Switching the accent

Press `Super+Ctrl+Shift+Space` to open `kdos-style`, the picker. It shows one row per accent, each
drawn in its own colours, and the desktop repaints live as you move the highlight. `Enter` keeps
the highlighted accent; `Esc` puts back the one you started with. Settings' Appearance page and the
`style.theme` route open the same picker.

From a prompt, use `kdos theme`:

```sh
kdos theme              # print the accent in force (also: kdos theme show, kdos theme current)
kdos theme list         # the eight accents, with the current one marked
kdos theme amber        # switch to amber
kdos theme next         # the next accent in the list
kdos theme prev         # the previous accent
kdos theme --preview X  # repaint the desktop only; write nothing else (see below)
kdos theme --audit [X]  # check the installed theme files (also: kdos theme audit)
kdos theme style FILE   # apply a style file
```

### What repaints at once, and what waits

The desktop repaints immediately: the panel, the desktop icons, any notification on screen, the
window frames and the phosphor shader all change on one signal. This works because the desktop
keeps no theme file of its own. The compositor and the shell have every palette built in, and read
a single word — the accent name — from `~/.cache/kdos/theme`. A switch writes that word and signals
the session.

A *preview* does only that half. Moving the highlight in the picker (or running
`kdos theme --preview`) writes the accent name and signals the session, so every KDOS surface
repaints, but it regenerates nothing else: the GTK stylesheet, the icon theme, the cursors and the
configuration files for other programs take seconds to write and are only read when those programs
start. So during a preview the desktop changes and a boxed application does not. Pressing `Enter`
runs the full switch, which brings everything else into line. Leaving the picker any other way —
including if it is killed — puts the original accent back.

The swatches in the picker are the one place on the desktop that uses fixed colours: a swatch
drawn in the current accent's slots would show eight identical rows.

`kdos-style` is the picker and `kdos-theme` is the generator that writes the GTK, icon and cursor
themes. They are two separate programs; the generator is described under
[How the theme is generated](#how-the-theme-is-generated).

## Choosing a font

The picker's second page is the typeface. `Left` and `Right` switch between the Accent and Font
pages, and the `style.font` route (or `kdos-style --page font`) opens straight onto the Font page.

The list shows every monospace font family fontconfig knows, one row each. Proportional fonts are
not offered, because the desktop is a grid of equal-width cells. The highlight starts on the font in
use, or on the first row if the font in use is an alias such as `monospace`. As you move the
highlight, the picker's own window switches to that font, so what you are reading is a sample.
Leaving any way other than `Enter` puts the original font back.

`Enter` on the Font page writes the chosen family into both `chrome_font` and `panel_font` in
`~/.config/kdos/comp.conf`, keeping the pixel size each already had. Every other line in that file
— comments, blank lines and keys the picker does not know — is kept exactly as it was.

The new font appears in the picker at once, but reaches every other surface only when that surface
next starts — in practice, at your next login. Each surface loads its font once, as it starts, and
a running surface cannot switch fonts. (The accent is different: it repaints everything on a
signal.)

If the list is empty, the page says so. That means either the machine has no monospace font
installed, or the picker is running with `--tty` inside a terminal that controls its own font.

## What changes when

Apart from the accent name, everything `kdos theme` writes is for programs that read their colours
once, when they start, and cannot be told to repaint — nearly all of them programs KDOS did not
write. Each file therefore takes effect at a different moment:

| File | Read by | Takes effect |
|---|---|---|
| `~/.cache/kdos/theme` (the accent name) | The compositor, the panel, the desktop, notifications | Immediately, on the signal |
| `~/.config/kdos-comp/themerc-override` | The compositor's window frames | Immediately, same signal |
| `~/.cache/kdos/wallpaper.png` | The compositor | Immediately, same signal |
| `~/.config/tmux/themes/kdos.conf` | tmux | Immediately, if tmux is running |
| The palette block in `~/.config/starship.toml`, between its two markers | starship | At the next shell prompt |
| `~/.config/foot/themes/kdos` | foot | In the next terminal — foot cannot reload its configuration |
| `~/.config/btop/themes/kdos.theme` | btop | Next start |
| `~/.config/kdos/term-colors.conf` | `kdos-term` | In the next terminal |
| `~/.config/kdos/fzf-colors` | fzf, through `$FZF_DEFAULT_OPTS` | At the next login shell |
| `~/.config/bat/themes/kdos.tmTheme` | bat | Next start, once its cache is rebuilt |
| `~/.config/micro/colorschemes/kdos.micro` | micro | Next start |
| `~/.config/helix/themes/kdos.toml` | helix, if you install it in a box (it is not on the host) | Next start |
| `~/.config/nvim/colors/kdos.vim` | neovim | Next start |
| `~/.config/git/kdos-delta` | delta, included from the shipped git configuration | At the next diff |
| `~/.config/newsboat/kdos-colors` | newsboat, `include`d from its configuration | Next start |
| `~/.config/aerc/stylesets/kdos` | aerc | Next start |
| `~/.config/yazi/theme.toml` | yazi | Next start |
| `~/.local/share/mc/skins/kdos.ini` | mc, selected by `skin = kdos` under `[Midnight-Commander]` in `~/.config/mc/ini` | Next start |
| `~/.config/kdos/ls-colors` | `ls`, through `$LS_COLORS`, loaded by `.bashrc` | In the next shell |
| `~/.themes/KDOS-<accent>/` | GTK3 applications in boxes | At once, in windows already open |
| `~/.config/gtk-{3,4}.0/settings.ini` | GTK, when the settings portal cannot be reached | The application's next launch |
| `~/.config/gtk-4.0/gtk.css` | libadwaita applications | The application's next launch |
| `~/.icons/KDOS/` | Every toolkit, on the host and in boxes | The application's next launch |
| `~/.config/kdeglobals` | Qt applications using the KDE platform theme | The application's next launch |
| `~/.local/share/color-schemes/KDOS.colors` | KDE's own appearance settings, as a scheme you can choose | The application's next launch |
| `~/.icons/KDOS-cursors/` | Cursors inside boxes | The application's next launch |
| `/etc/kdos/accent` | The boot splash, retinted during startup | The next boot |
| `/boot/efi/limine.conf` | The boot menu | The next boot |
| `/etc/vtrgb` | Every text console — the login prompt, `/etc/issue`, the login banner | The next login prompt |

**Why GTK3 applications repaint while open.** GTK rebuilds its styles only when the name of the
theme changes, so rewriting a stylesheet under the same name would only reach the next launch.
`kdos theme` therefore writes the theme into a directory named after the accent,
`~/.themes/KDOS-<accent>`, deletes the directories for every other accent, and points the symlink
`~/.themes/KDOS` at it. The settings portal reports the new name, and every running GTK3
application in a box repaints.

Everything else picks up the change when you next start it. GTK does not re-read icons or a user
stylesheet when those files change, and no toolkit offers a way to force it from outside.

### Files you may edit, and files you may not

Never edit a generated file: every file in the table above is rewritten in full on each accent
switch. Most of them are *selected* by a second file that ships once and then belongs to you.
Change these freely; `kdos theme` does not touch them:

| Program | The line that selects the KDOS theme |
|---|---|
| micro | `settings.json` |
| neovim | `init.vim` |
| delta | an `[include]` in the git configuration |
| newsboat | an `include` line |
| aerc | `styleset-name` |
| bat | `--theme="kdos"` |
| btop | `color_theme = "kdos"` |
| foot | `include=` in `foot.ini` |
| tmux | a `source-file` line in `tmux.conf` |

To keep the accent but change one colour, build on the generated theme: your own micro scheme may
`include "kdos"`, and your own helix theme may say `inherits = "kdos"`.

**helix** is not installed on the host. The theme is still written, because a box shares your home
directory by default (`home = shared` in its profile), and a helix you install in a box reads it.
Nothing selects it for you: add `theme = "kdos"` to your own `~/.config/helix/config.toml`.

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

One file of yours is removed rather than kept: `~/.config/gtk-3.0/gtk.css` is deleted on every
accent switch, because GTK3 loads it once and it would hold the old accent for the life of every
GTK3 program. A GTK3 stylesheet of your own does not survive an accent switch; change GTK3's look
through the theme's accent instead.

## The boot menu, the splash and the text consoles

These three belong to root, so `kdos theme` asks the root power service, `kdos-powerd`, to update
them. The boot menu lives in `limine.conf` on the EFI system partition, the splash reads
`/etc/kdos/accent`, and every text console reads `/etc/vtrgb`. Your account cannot write any of
them, and all three are settings for the machine rather than for your session.

**The console palette** is the accent's sixteen colours. `kdos-getty` loads `/etc/vtrgb` onto each
virtual terminal before it clears the screen, so the login prompt, `/etc/issue` and the login
banner are drawn in the accent: bold text uses slots 8–15 and plain text slots 0–7. Blue, magenta
and cyan keep distinct colours, because no accent defines them and a console whose eight colours
all collapsed onto one would have nothing left for `ls --color`, a diff or syntax highlighting. To
see the table without writing it:

```sh
kdos-bootctl palette [<accent>]
```

**Timing.** The boot menu changes on the next boot, the console palette at the next login prompt
(log out of a console, or reboot), and the splash partway through the next boot. The splash starts from the initramfs, before the root filesystem is mounted,
so it comes up in the accent built into it; the startup script `rcS` then tells it the machine's
real accent as soon as `/etc` can be read. If you have never changed the accent, the two are the
same and you see nothing.

**A machine with no writable EFI partition** — the read-only live medium, or a machine with no
`/boot/efi` — keeps its boot menu as it is. The splash and the console palette still follow the
accent. When there is a boot menu that cannot be rewritten, the output ends
`ok <accent> (boot menu unchanged)`.

**When `kdos-powerd` does not answer** — inside a box, where the daemon cannot be reached, or for an
account it does not authorise — the boot menu, the splash and the console palette are all left as
they are, and `kdos theme` prints `boot menu and splash unchanged (kdos-powerd did not answer)`.

In both cases everything else in the table above is still applied.

**The boot menu's background picture** does not follow the accent. The bootloader cannot recolour
a picture, and writing a new image to a FAT filesystem on every theme change risks a half-written
file if the power fails, so the backdrop is a single dimmed, colourless image that suits every
accent.

## The phosphor pass

The compositor draws the whole desktop through a phosphor shader: scanlines, a horizontal bleed, a
vignette, optional barrel distortion, and a faint glow so black is never quite black. It is on by
default.

Set these in `~/.config/kdos/comp.conf` (or on Settings' Appearance page). They take effect
immediately:

| Key | Default | What it does |
|---|---|---|
| `crt` | `55` | Overall strength, in percent. `0` switches the pass off completely |
| `crt_scanlines` | `0` | Scanline depth, in percent — a dark line on every third physical row. `60` is the strength the rest of the pass is designed around |
| `crt_curve` | `0` | Barrel distortion, in percent. Scaled so that no value crops the desktop |
| `crt_fullscreen` | `on` | `off` skips the pass on a screen while its focused window is fullscreen, so a film or game gets the whole GPU back |

Scanlines are off by default because the desktop is a grid of 16×32-pixel cells and a line on
every third row does not line up with it, so text comes out striped.

**What it costs.** While the pass is on, every frame goes through the GPU: there is a second
frame buffer per screen (about 100 MB of video memory on a 4K screen), the shader runs over the
whole frame however little changed, and a fullscreen video cannot be sent straight to the display.
On battery, `crt = 0` or `crt_fullscreen = off` is the lever.

**When it switches itself off.** The pass does not run where there is no GPU acceleration, such
as a virtual machine without 3D (virgl) enabled, because a full-screen effect there drops to a few
frames a second. It also steps aside while the screen magnifier is on, since zoomed text is harder
to read through scanlines. When it is off for lack of acceleration, the compositor's log,
`$XDG_RUNTIME_DIR/kdos-comp.log`, says so. (For developers: `make run` boots without 3D and shows
no pass, `make run-hw` boots with virgl and does; see
[Developing](../05-developer/developing.md).)

**Screenshots.** A screenshot of a whole screen captures the shaded image, so it looks like what you
see. A screenshot of a single window captures the window itself, without the shader.

**Night light** (`Super+Ctrl+Shift+N`, or `kdos toggle night-light`) warms the palette itself
rather than adding a filter, so it applies to every surface the same way an accent does.

## Wallpaper

The compositor draws the wallpaper, and it follows the accent: `kdos theme` recolours the shipped
image, `/usr/share/backgrounds/kdos/default-wallpaper.png`, into the current palette and writes the
result to `~/.cache/kdos/wallpaper.png` (under `$XDG_CACHE_HOME` if you set it).

The compositor chooses what to draw like this:

1. `wallpaper = none` in `comp.conf`: no wallpaper; the background is the accent's background
   colour. The cache is never used.
2. Otherwise, if `~/.cache/kdos/wallpaper.png` exists, it is drawn.
3. Otherwise, the file named by `wallpaper =` is drawn.

So to use your own picture:

1. Set `wallpaper = /path/to/picture.png` in `~/.config/kdos/comp.conf`, or on Settings'
   Appearance page (the desktop menu's **Change Wallpaper** opens it).
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
| `crt`, `crt_scanlines`, `crt_curve`, `crt_fullscreen` | Written to `comp.conf` |
| `chrome_font`, `clock_format` | Written to `comp.conf`; they reach the desktop at the next login |
| Any key containing a dot, such as `osd.bg.color: #202020` | A window-frame theme setting. Kept in `~/.config/kdos/style-themerc` and added after the generated frame theme, so it wins |

Either `=` or `:` may separate a key from its value; whichever comes first is used. A key that is
none of the above is reported and ignored.

For example:

```ini
# retro.kdos
accent = amber
crt = 70
crt_scanlines = 60
chrome_font = Terminus:pixelsize=32
```

A style changes only the keys it names. Every other line in your `comp.conf` is kept exactly as it
was. A style with no dotted keys clears the ones an earlier style left, because a style is a whole
look rather than a patch on the previous one.

## Fonts

Three separate settings decide what the desktop is drawn in. They live in different files and
none of them falls back to another.

**The console font** is a 16×32-pixel bitmap font, `ter-kdos32n`, built from Terminus and loaded
by `kdos-getty` onto each virtual terminal before it clears the screen. It has 512 glyphs, which is
why parts of KDOS limit themselves to a small set of characters: anything outside it shows as a
blank on `tty1`.

**`chrome_font` and `panel_font`** in `~/.config/kdos/comp.conf` are the desktop's own fonts, as
fontconfig patterns with a size in pixels:

| Key | Default | Used by |
|---|---|---|
| `chrome_font` | `Terminus:pixelsize=32` | Every popup, menu and window the desktop draws, the desktop icons and notifications |
| `panel_font` | `Terminus:pixelsize=20` | The panel bar only, not the popups it opens. Empty means use `chrome_font` |

Each is a single pixel size for every screen. That is right on a machine with one monitor and wrong
on two of different densities. On a 4K screen, `chrome_font = Terminus:pixelsize=64` doubles the
cell.

Several things write these two keys: the picker's Font page sets the family on both and keeps each
size; Settings sets either one whole (`chrome_font` on its Appearance page, `panel_font` on its
Panel page); a style file may set `chrome_font`. Whichever wrote it, each surface reads the value
once as it starts, so the desktop agrees at the next login.

**The compositor's own chrome** — title bars, its menus and the window-switcher display — is set
separately, in `~/.config/kdos-comp/rc.xml`, in *points*. There is one `<font>` line for each of
the five places it draws: `ActiveWindow`, `InactiveWindow`, `MenuHeader`, `MenuItem` and
`OnScreenDisplay`.

These surfaces are drawn with pango, which cannot draw bitmap fonts, so this font must be a
*scalable* one. Asking for the bitmap `Terminus` here silently falls back to a generic sans-serif
font. The shipped file names `Terminus (TTF)`, the same design converted to TrueType (the
`terminus-ttf` package), at 24 points on all five rows:

```xml
<font place="ActiveWindow"><name>Terminus (TTF)</name><size>24</size></font>
```

24 points is 32 pixels at 96 dpi, so a title bar is exactly one cell tall.

## Theming applications inside boxes

A box has its own `/usr`, from its own distribution, so the host's `/usr/share/themes` and
`/usr/share/icons` are invisible inside it. What a box does share is your home directory, at the
same path on both sides. So everything a boxed application reads for its theme is written into
`$HOME`:

| Path | Read by |
|---|---|
| `~/.themes/KDOS-<accent>/` | GTK3, and GTK4 applications that do not use libadwaita. `~/.themes/KDOS` is a symlink to it |
| `~/.config/gtk-{3,4}.0/settings.ini` | The theme name, when the settings portal cannot be reached |
| `~/.config/gtk-4.0/gtk.css` | libadwaita, which ignores themes. There is no GTK3 equivalent, because GTK3 loads that file once and it would override the theme for the life of the program |
| `~/.icons/KDOS/` | Every toolkit |
| `~/.icons/KDOS-cursors/` | Cursors |
| `~/.config/kdeglobals` | Qt, under the KDE platform theme |

**The GTK theme** is a recoloured `adw-gtk3`, and that choice is what makes it work. GTK3's own
Adwaita theme has literal colour values in nearly every rule, so redefining its named colours
reaches only a few widgets. `adw-gtk3` is the libadwaita stylesheet ported to GTK3 and uses named
colours throughout, so rewriting the palette recolours every widget — and GTK3 applications end up
looking the same as GTK4 ones.

**Qt** has two routes, and the right one is chosen for each application from what its runtime
provides. Where the runtime includes KDE's platform integration, Qt applications read
`~/.config/kdeglobals` directly. Where it does not, they use the GTK platform theme with the Fusion
style. The runtime declares which it provides, so the choice travels with the packages instead of
being guessed — a wrong guess would leave the application on Qt's built-in light palette.

**`kdeglobals` is merged, not overwritten.** KDE applications save their own settings in that
file, so only the sections the theme owns are replaced and everything else is kept.

## How the theme is generated

The artwork comes from three data packages, each vendored from an upstream project and recoloured:

| Package | Upstream | Installed to |
|---|---|---|
| `kdos-icons` | Papirus icons | `/usr/share/kdos/icons/` |
| `kdos-cursors` | Bibata cursors | `/usr/share/kdos/cursors/` |
| `kdos-gtk-theme` | adw-gtk3 | `/usr/share/kdos/gtk-theme/` |

`kdos-theme` is the generator that recolours them. `kdos theme` runs it on every accent switch.
The image build runs it too: once for the system-wide copies each package installs, and once
through `kdos theme` to seed the default accent into `/etc/skel`, so a new account starts themed.

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

The icons and cursors are recoloured rather than redrawn. A maintenance script prunes an upstream
release into a source tree kept in the repository, and the generator recolours that tree into the
palette.

**Colours are mapped by hue family**, not all flattened onto the accent: blues and greens become the
accent, warm colours become the secondary, and reds stay the urgent colour. Papirus colour-codes
file types, and turning every hue into the accent would make a folder of mixed files look like a
wall of identical shapes. So a PDF stays red and an audio file stays amber while folders and
devices take the accent.

**Application icons.** The vendored icon set has no application icons, so the generator builds
them by scanning every size directory of `/usr/share/icons/hicolor`, not only `scalable/`. It takes
**only** icons whose names start with `kdos.`. The image build installs the boxed applications' own
icons into the same directory, and those belong to their projects: a Firefox logo recoloured into
the accent would stop being Firefox's own mark, so it is left alone.

### Auditing what is installed

```sh
kdos theme --audit          # do the installed files match what the current accent produces?
kdos theme --audit amber    # what would switching to amber change?
```

The audit runs the same generators into a scratch directory and compares the results byte for
byte with what is installed, symlinks included. Anything reported as different is different from
what your palette produces right now. The audit writes nothing outside its scratch directory and
repairs nothing; run `kdos theme <accent>` to repair.

| Exit status | Meaning |
|---|---|
| `0` | Everything matches |
| `1` | Something differs |
| `2` | The audit could not run |

## See also

- [The design language](../03-architecture/design-language.md) — the palette slots and the contrast rules
- [kdos-comp](../04-programs/kdos-comp.md) — the phosphor pass and the frame theme
- [The desktop](desktop.md) — the picker's shortcut, the panel and its fonts
- [Accessibility](accessibility.md) — the magnifier and the font size for large screens
- [Configuration](../06-reference/configuration.md) — every key named here, with defaults
- [Applications](applications.md) — why boxed software is themed through `$HOME`
- [The kdos command](../04-programs/kdos-command.md) — `kdos theme` in full
