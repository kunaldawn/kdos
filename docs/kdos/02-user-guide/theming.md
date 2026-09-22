# Theming

This page covers changing how KDOS looks: the eight accents, the phosphor shader, the wallpaper,
fonts, and how applications inside containers get the same palette. One command does most of it,
and most of the result appears without restarting anything.

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
| `paper` | Ink on paper — the light one, and the only one whose ground is the light end |

An accent is not a single colour. It is a small palette: a primary, a dim variant of it, a
secondary, an urgent colour, a background, a text colour, a surface, and two more derived shades.
Everything drawn on this system takes its colours from named slots in that palette rather than
from literal values, which is why one word repaints the entire desktop. See
[the design language](../03-architecture/design-language.md).

Seven of the eight are dark and one is light, and what decides whether a palette works is its
plates rather than its ground. A focused plate has to stand off the bar it sits on *and* carry its
own label, and mixing toward the brighter end buys the first at the second's expense. A palette
that leaves no mix doing both is unusable whichever end its ground is, which is why the self-test
measures the plates themselves. Asking whether white beats black against the background is a proxy
that refuses a good light palette and accepts a bad dark one.

`paper`'s plate separates at the hover floor and carries its label at 7.51:1. `bone`'s reaches
6.21:1 and cannot do better: its separation floor binds first, and the best any mix of its own
colours reaches is 6.69:1. The solver aims at 7:1 and stops early when it gets there; 4.5:1, WCAG
AA for normal-size text, is the floor every accent is held to.

## Switching the accent

Press `Super+Ctrl+Shift+Space` to open `kdos-style`, the picker. It shows one row per accent, each
drawn in its own colours, with the desktop repainting live as the highlight moves. `Enter` keeps
the highlighted accent and `Esc` puts back the one you opened on. Settings' Appearance page and the
`style.theme` route open the same window; the accent is chosen there and nowhere else, because a
list of names is the worse of two ways to pick a colour.

From a prompt:

```sh
kdos theme              # what is in force
kdos theme list         # the eight names, with the current one marked
kdos theme amber        # switch
kdos theme next         # cycle forward
kdos theme prev         # cycle back
kdos theme --preview X  # the state file and the signal only
kdos theme --audit      # does what is installed match what this palette produces?
kdos theme style FILE   # apply a style file
```

The desktop retints live: the panel, the desktop icons, any notification on screen, the window
frames and the phosphor shader all change on one signal, with no restart and no logout. That works
because the desktop stores no theme *file* of its own. The compositor and the shell carry the
palette table compiled in and read exactly one word — the accent name — from a state file. A theme
switch writes that word and sends a signal.

A preview is half a theme, and it says so by what it leaves alone. Moving the highlight writes the
accent's state file and signals the session, so every KDOS surface repaints at once; it regenerates
nothing, because the GTK stylesheet, the icon theme, the cursors and the eight foreign
configuration files take seconds and are read by programs that are not running. So the desktop
moves under the highlight and a containerised application does not, and `Enter` — which runs the
real switch — is what makes the rest agree. Leaving the picker any other way puts the original
back, including on a kill.

The swatches in that picker are the one place on this desktop where a cell carries a literal
colour. Everything else draws in named slots so that one word repaints all of it; a swatch that
took the accent in force would show eight identical rows, which is the one thing that window exists
not to do. `preflight.sh` names the file as the exception rather than dropping the check.

`kdos-style` is the picker and `kdos-theme` is the generator. They are two programs, and the
generator is described under [How the theme is generated](#how-the-theme-is-generated).

## Choosing a font

The second page of the picker is the typeface. `Left` and `Right` move between Accent and Font, and
the `style.font` route opens straight onto the second page.

Its rows are the monospace families fontconfig knows, one row per family, and the list scrolls. A
cell grid drawn in a proportional face renders every column at a different advance, so a family
fontconfig does not call monospaced is never offered. The highlight opens on the face in force, or
on the first row where the name in force is an alias such as `monospace` that names no family of
its own. The arrows load the highlighted face into that window, live, so every cell you are reading
is a sample of it, and leaving any other way puts back the face the window opened on.

`Enter` on the font page writes a file, and that is why it lands at the next start. It writes the
chosen family into `chrome_font` and `panel_font` in `~/.config/kdos/comp.conf`, each keeping the
size it already carried, and every other line of that file — its comments, its blank lines and
every key the picker knows nothing about — survives byte for byte. Each surface on this desktop is
its own process with its own font and reads that file once as it starts, so the face is on in the
picker's own window at once and reaches every other surface as that surface is next started. The
accent half of the same window repaints the whole desktop on a signal; the font half cannot,
because no running surface can be handed a new font.

Where the list is empty the page says so in two lines, and it means one of two things: this machine
carries no monospace family at all, or the run is `--tty` inside a terminal that owns its own font.

## What changes when

Everything `kdos theme` writes beyond the state file exists for software that reads its colours
once, at start, and cannot be told to repaint — nearly all of it software that is not ours. The
timing differs by target:

| Artefact | Read by | Applies |
|---|---|---|
| The accent state file | The compositor, the panel, the desktop, notifications | Immediately, on signal |
| `~/.config/kdos-comp/themerc-override` | The compositor's frame theme | Immediately, same signal |
| `~/.cache/kdos/wallpaper.png` | The compositor | Immediately, same signal |
| `~/.config/tmux/themes/kdos.conf` | tmux | Immediately, if tmux is running |
| The palette block in `~/.config/starship.toml`, between its two markers | starship | On the next shell prompt |
| `~/.config/foot/themes/kdos` | foot | On the next terminal — foot cannot reload its configuration |
| `~/.config/btop/themes/kdos.theme` | btop | On the next start |
| `~/.config/kdos/term-colors.conf` | `kdos-term` | On the next terminal |
| `~/.config/kdos/fzf-colors` | fzf, through `$FZF_DEFAULT_OPTS` | On the next login shell |
| `~/.config/bat/themes/kdos.tmTheme` | bat | On the next start, once the cache is built |
| `~/.config/micro/colorschemes/kdos.micro` | micro | On the next start |
| `~/.config/helix/themes/kdos.toml` | helix, which is not on the image — a helix you install in a box | On the next start |
| `~/.config/nvim/colors/kdos.vim` | neovim | On the next start |
| `~/.config/git/kdos-delta` | delta, included from the shipped gitconfig | On the next diff |
| `~/.config/newsboat/kdos-colors` | newsboat, `include`d from its config | On the next start |
| `~/.config/aerc/stylesets/kdos` | aerc | On the next start |
| `~/.config/yazi/theme.toml` | yazi | On the next start |
| `~/.local/share/mc/skins/kdos.ini` | mc, pointed at it by `skin = kdos` under `[Midnight-Commander]` in `~/.config/mc/ini` | On the next start |
| `~/.config/kdos/ls-colors` | `ls`, through `$LS_COLORS`, sourced by `.bashrc` | On the next shell |
| `~/.themes/KDOS-<accent>/` | GTK3 applications in containers | At once, in the window already open |
| `~/.config/gtk-{3,4}.0/settings.ini` | GTK, where the settings portal cannot be reached | On the application's next launch |
| `~/.config/gtk-4.0/gtk.css` | libadwaita applications | On the application's next launch |
| `~/.icons/KDOS/` | Every toolkit, host and container | On the application's next launch |
| `~/.config/kdeglobals` | Qt applications under the KDE platform theme | On the application's next launch |
| `~/.local/share/color-schemes/KDOS.colors` | KDE's own appearance dialog, as a scheme you can pick | On the application's next launch |
| `~/.icons/KDOS-cursors/` | Cursor lookup inside containers | On the application's next launch |
| `/etc/kdos/accent` | `rcS`, which retints the running splash | On the next boot |
| `/boot/efi/limine.conf` | The bootloader | On the next boot |
| `/etc/vtrgb` | Every text console — the login prompt, `/etc/issue`, the banner | On the next login prompt |

A GTK3 application restyles while it is open, and the accent in the directory name is what does it.
GTK rebuilds its whole style cascade when the `gtk-theme-name` setting moves and at no other time,
so a stylesheet rewritten under one fixed name would reach the next launch and never the window on
the screen. `kdos theme` writes `~/.themes/KDOS-<accent>`, deletes the copies for every accent you
are not wearing, and points `~/.themes/KDOS` at it; the settings portal answers with that name and
signals the change, and every containerised GTK3 application repaints.

Everything else picks the switch up when you next start it. GTK re-reads neither its icons nor the
user stylesheet when those files change, and no toolkit offers a way around that from outside.

A generated file is never a file you edit. Each row above is written whole on every accent switch,
and most are *selected* by a second file that ships once and is then yours: `settings.json` for
micro, `init.vim` for neovim, an `[include]` for delta, an `include` line for newsboat,
`styleset-name` for aerc, `--theme="kdos"` for bat, `color_theme = "kdos"` for btop, `include=` in
`foot.ini`, a `source-file` line in `tmux.conf`. Change those freely; they are not rewritten. Your
own micro scheme may `include "kdos"` and your own helix theme may `inherits = "kdos"`, so you can
keep the accent and override one colour.

helix is the one row whose program is not on the image: the port exists, no `packages.txt` names
it, and nothing builds it. The theme is still written, because a box takes `home = shared` by
default and a helix installed in one reads it. Nothing selects it for you — write `theme = "kdos"` in
your own `~/.config/helix/config.toml`.

Two rows have no such second file. yazi reads `theme.toml` by that name and deserializes it over
its own preset, so the generated file is partial on purpose — only what this palette decides is in
it, and the preset supplies the rest. And mc's selector is not yours: `kdos theme` sets
`skin = kdos` in `~/.config/mc/ini` itself, rewriting that one key and leaving the rest of the file
alone.

Two programs cannot take a colour, and each is answered differently. newsboat reads `#` as a
comment, so a hex value truncates the line and the entry is refused outright — its colours are
therefore the nearest of the 256 terminal indices rather than the scheme's exact values. lazygit
has no include, no import and no separate theme file: its colours live inside the one `config.yml`
you edit, so `kdos theme` does not write them at all. Writing them would mean owning that file and
discarding whatever else you had put in it.

## The boot menu, the splash and the text consoles

These three belong to root, and `kdos theme` reaches them through `kdos-powerd`. The boot menu
lives in `limine.conf` on the ESP, the splash reads `/etc/kdos/accent`, and every text console
reads `/etc/vtrgb`. None is a file your account can write, and all three are administration of the
machine rather than of your session — the same argument that puts the timezone and the autologin
account on that daemon.

The console palette is the accent's sixteen colours, and it is where SGR reaches a scheme.
`kdos-getty` loads `/etc/vtrgb` onto each VT before it clears the screen, so the login prompt,
`/etc/issue` and the login banner draw in `\e[…;3Nm` and land on the accent, bold reaching slots
8–15 and plain 0–7. Blue, magenta and cyan are the exception: no accent names them, and a console
whose eight colours collapsed onto one would have nothing left to show `ls --color`, a diff or a
syntax highlight with. `kdos-bootctl palette [<accent>]` prints the table without writing it.

The boot menu and the console palette both apply on the next boot, and the splash applies partway
through it. The splash is started by the initramfs, before any root filesystem is mounted, so it
comes up in the accent compiled into it and `rcS` tells it the machine's real one as soon as `/etc`
can be read. On a machine whose accent has never been changed those are the same scheme and nothing
is visible at all.

A machine with no writable ESP skips the boot menu and says so. The live medium is read-only, and a
machine may have no `/boot/efi` at all. `kdos theme` reports `boot menu and splash unchanged` and
applies everything else; refusing the whole change over a bootloader would be the wrong trade,
since the desktop is the thing you are looking at.

What the bootloader cannot follow is the artwork. Limine cannot retint a picture, and a theme
change must not write a PNG to a FAT filesystem that may be half-done when the power goes, so the
backdrop behind the boot menu is achromatic by construction: one dimmed, hueless image that sits
under every accent.

## The phosphor pass

The compositor renders the whole desktop through a phosphor shader — scanlines, a horizontal
bleed, a vignette, optional barrel distortion, and a faint phosphor floor so black is never quite
black. It is on by default.

Set these in `~/.config/kdos/comp.conf`. All are percentages, and all apply on signal:

| Key | Default | What it does |
|---|---|---|
| `crt` | `55` | Overall strength. `0` is an honest off |
| `crt_scanlines` | `0` | Scanline depth, every third physical row |
| `crt_curve` | `0` | Barrel distortion. Normalised so no value crops the desktop |
| `crt_fullscreen` | `on` | Whether the pass also runs over a fullscreen window |

The pass declines in two situations. It declines on a software renderer, because a fullscreen
post-process there is a slideshow — so `make run` with plain virtio-vga has no shader while
`make run-hw` with virgl does. And it steps aside while the screen magnifier is on, since an
accessibility zoom read through scanlines is harder to read rather than easier.

A screenshot of an output captures the processed buffer, so a screenshot of a phosphor desktop
looks like the desktop. A per-window capture renders the window itself and is untinted. Both are
honest answers to different questions.

## Wallpaper

The wallpaper is drawn by the compositor rather than by a client, and it is retinted to follow the
accent: `kdos theme` remaps the *shipped* image into the current palette and writes
`$XDG_CACHE_HOME/kdos/wallpaper.png`.

That cache is what the compositor draws whenever it exists, and it beats `wallpaper =` in
`comp.conf` — so pointing that key at your own picture takes effect only while no cache is there,
and the next accent switch puts the shipped image back. Delete `~/.cache/kdos/wallpaper.png` after
naming your own, or accept that a theme change overrides it. `wallpaper = none` is the one answer
the cache never overrides: you said none, and none is what you get.

The shipped image carries no scanlines baked in — the shader draws those — because two sets of
scanlines beating against each other is moiré rather than identity.

## Style files

An accent plus a set of related settings can be bundled and shared:

```sh
kdos theme style retro.kdos
```

A style file is flat `key = value` and may carry `accent`, `crt`, `crt_scanlines`, `crt_curve`,
`crt_fullscreen`, `chrome_font` and `clock_format`, plus any *dotted* key, which is appended to the
compositor's generated frame theme so it wins over the generated block.

It rewrites only the keys it names. Every other line in your `comp.conf` survives verbatim, and a
key the style does not mention is left alone. A style with no dotted keys clears the ones a
previous style left, because a style is a whole look rather than a patch on the last one.

## Fonts

Three settings decide what this desktop is drawn in, and they are different objects in different
files. None of them is the others' fallback.

The console font is a bitmap PSF at 16x32 pixels, built from source and loaded by `kdos-getty` onto
each VT before it clears the screen. It carries 512 glyphs, which is why parts of this system
restrict themselves to a small glyph set — anything outside it renders as a blank on `tty1`.

`chrome_font` in `comp.conf` is what the desktop's own surfaces are drawn with, as a fontconfig
pattern (`Terminus:pixelsize=32`). `panel_font` (`Terminus:pixelsize=20`) is the bar's own and
reaches the bar rather than the popups it opens. Each is one pixel size for every screen, which is
right on a machine with one monitor and wrong on two of different densities.

Your editor is not the only writer of those two keys. The picker's Font page sets the family on
both and leaves each key's size where it was; Settings sets either pattern whole, `chrome_font` on
its Appearance page and `panel_font` on its Panel page; a style file may carry `chrome_font`.
Whichever of them wrote it, the value is read once by each surface as it starts, so the desktop
agrees at the next login and not on a signal.

The compositor's own chrome is a separate setting: `<theme><font>` in `rc.xml`, in *points*, with a
row for each of the five places it draws — `ActiveWindow`, `InactiveWindow`, `MenuHeader`,
`MenuItem` and `OnScreenDisplay`. These are the surfaces the compositor renders with pango rather
than as cells, which is why they need a font of their own and why it must name a *scalable* face:
pango has not drawn bitmap fonts since 1.44, so asking for the bitmap `Terminus` here resolves in
fontconfig and then falls through to a generic sans, silently.

The shipped configuration names `Terminus (TTF)` at 24 points on all five rows — the same face as
the cell grid, converted to TrueType, which is what the `terminus-ttf` port exists for. Twenty-four
points is 32 pixels at 96 dpi, so a title bar is exactly one cell tall and the chrome sits on the
same rhythm as everything drawn in cells.

## Theming applications inside containers

A box's `/usr` is Debian's, not this machine's, so `/usr/share/themes` and `/usr/share/icons` on the
host are invisible inside it. What the box does share is your home directory, at the same path on
both sides — so everything a containerised application reads is written into `$HOME`:

| Path | Read by |
|---|---|
| `~/.themes/KDOS-<accent>/` | GTK3 and non-libadwaita GTK4. `~/.themes/KDOS` is a symlink to it |
| `~/.config/gtk-{3,4}.0/settings.ini` | The theme name, where the portal cannot be reached |
| `~/.config/gtk-4.0/gtk.css` | libadwaita, which ignores themes entirely. GTK3 gets no such file: it is loaded once at startup and would outrank the theme for the life of the process |
| `~/.icons/KDOS/` | Every toolkit |
| `~/.icons/KDOS-cursors/` | Cursor lookup |
| `~/.config/kdeglobals` | Qt, under the KDE platform theme |

The GTK theme is a recoloured `adw-gtk3`, and choosing that specifically is what makes it work.
Stock GTK3's Adwaita is compiled from SASS with literal colour values in nearly every rule, so
redefining named colours reaches only a few widgets and hand-written overrides never cover enough.
`adw-gtk3` is the libadwaita stylesheet ported to GTK3 and is written against named colours end to
end, so rewriting the palette makes every widget follow — and GTK3 applications end up genuinely
identical to GTK4 ones.

Qt has two routes and the right one is chosen per pack. Where a pack's runtime provides KDE's
platform integration, Qt applications read `~/.config/kdeglobals` directly. Where it does not, the
GTK platform theme plus a style override is used instead. Neither may be exported on a guess: the
fallback style with no platform theme lands on Qt's built-in *light* palette, which is worse than
doing nothing. The runtime declares which it provides, so the answer travels with the packages
rather than being inferred.

`kdeglobals` is merged rather than overwritten. KDE applications write their own settings into that
file, so only the sections the theme owns are replaced and everything else is kept.

## How the theme is generated

`kdos-theme` is the generator, with four subcommands:

```sh
kdos-theme gtk     <out-dir> [accent] [--src DIR]
kdos-theme icons   <out-dir> [accent] [--src DIR] [--marks DIR]
kdos-theme cursors <out-dir> [accent] [--src DIR]
kdos-theme accents
```

The icon and cursor themes are vendored, pruned and recoloured rather than drawn. A maintenance
script prunes an upstream release into a committed source tree, and the generator recolours that
tree into the palette at build time and again on every accent switch.

Colours are mapped by hue *family* rather than flattened onto the accent — blues and greens become
the accent, warm hues become the secondary, reds stay urgent. The upstream icon set colour-codes
file types, and collapsing every hue onto one accent turns a folder of files into a wall of
identical lozenges. A PDF stays red and an audio file stays amber while folders and devices go
phosphor.

The vendored artwork carries no Applications context at all, so the generator builds one by
sweeping `/usr/share/icons/hicolor` — every size directory, not just `scalable/`, because a theme
that ships SVGs under numeric sizes and nowhere else would otherwise leave exactly the dock buttons
you look at unthemed. It takes **only** the `kdos.` prefix. Packaging installs the boxed
applications' own icons into that same directory, and a phosphor Firefox logo is vandalism, not
theming: somebody else's mark is left alone.

### Auditing what is installed

```sh
kdos theme --audit          # does what is installed match what this palette produces?
kdos theme --audit amber    # what would switching to amber change?
```

The audit does not try to recognise "palette colours" in the installed files. It runs the same
generators into a scratch directory and compares byte for byte, symlinks included, so anything that
differs, differs from what your palette produces right now. It writes nothing outside its scratch
directory and repairs nothing — an audit that fixed what it found would be a `kdos theme` under a
misleading name. Exit 0 is clean, 1 is drift, 2 means the audit could not run.

## See also

- [The design language](../03-architecture/design-language.md) — the palette slots and the contrast rules
- [kdos-comp](../04-programs/kdos-comp.md) — the phosphor pass and the frame theme
- [Configuration](../06-reference/configuration.md) — every key named here, with defaults
- [Applications](applications.md) — why containerised software is themed through `$HOME`
- [The kdos command](../04-programs/kdos-command.md) — `kdos theme` in full
