# Theming

Changing how KDOS looks: the four accents, the phosphor shader, the wallpaper, fonts, and how
applications inside boxes get the same palette. One command does almost all of it, and most of
the result appears without restarting anything.

## The four accents

| Accent | Character |
|---|---|
| `phosphor` | Green on near-black. The default, and the project's own colour |
| `amber` | Amber on warm black |
| `ice` | Cyan and blue on deep blue-black |
| `bone` | Warm off-white on near-black — the least saturated of the four |

An accent is not a colour, it is a small palette: a primary, a dim variant of it, a secondary, an
urgent colour, a background, a text colour, a surface, and two more derived shades. Everything
drawn on this system takes its colours from named slots in that palette rather than from literal
values, which is why one word repaints the entire desktop. See
[the design language](../03-architecture/design-language.md).

## Switching

```sh
kdos theme amber        # switch
kdos theme list         # the four names
kdos theme next         # cycle forward
kdos theme prev         # cycle back
```

**The desktop retints live.** The panel, the desktop icons, any notification on screen, the window
frames and the CRT shader all change in one signal, with no restart and no logout.

That works because the desktop stores no theme *file* of its own. The compositor and the shell
carry the palette table compiled in, and read exactly one word — the accent name — from a state
file. A theme switch writes that word and sends a signal.

## What changes when

Everything else `kdos theme` writes exists for software that is **not ours** and cannot be told to
repaint. The timing differs by target, and this is the table worth knowing:

| Artefact | Read by | Applies |
|---|---|---|
| The accent state file | The compositor, the panel, the desktop, notifications | **Immediately**, on signal |
| Window frame theme | The compositor | **Immediately**, same signal |
| The wallpaper cache | The compositor | **Immediately**, same signal |
| `~/.config/tmux/themes/kdos` | tmux | Immediately, if tmux is running |
| The starship palette block | starship | On the next shell prompt |
| `~/.config/foot/themes/kdos` | foot | On the next terminal — foot cannot reload its configuration at all |
| `~/.config/btop/themes/kdos.theme` | btop | On the next start |
| The `mc` skin and `LS_COLORS` | mc, ls | On the next start |
| `~/.themes/KDOS/` | GTK3 applications in boxes | On the application's next launch |
| `~/.config/gtk-3.0/gtk.css`, `gtk-4.0/gtk.css` | libadwaita applications | On the application's next launch |
| `~/.icons/KDOS/` | Every toolkit, host and box | On the application's next launch |
| `~/.config/kdeglobals` | Qt applications under the KDE platform theme | On the application's next launch |
| `~/.icons/KDOS-cursors/` | Cursor lookup inside boxes | On the application's next launch |

GTK re-reads neither its theme nor its icons when the files change, so boxed applications pick up
an accent switch when you next start them. There is no way around that from outside the toolkit.

## The CRT pass

The compositor renders the whole desktop through a phosphor shader — scanlines, a horizontal
bleed, a vignette, optional barrel distortion, and a faint phosphor floor so black is never quite
black. It is **on by default**.

Set these in `~/.config/kdos/comp.conf`; all are percentages and all apply on signal:

| Key | Default | What it does |
|---|---|---|
| `crt` | `55` | Overall strength. `0` is an honest off |
| `crt_scanlines` | `0` | Scanline depth, every third physical row |
| `crt_curve` | `0` | Barrel distortion. Normalised so no value crops the desktop |
| `crt_fullscreen` | `on` | Whether the pass also runs over a fullscreen window |

Two things it will not do. **It declines on a software renderer**, because a fullscreen
post-process there is a slideshow — so `make run` with plain virtio-vga has no shader while
`make run-hw` with virgl does. And it steps aside while the screen magnifier is on, since an
accessibility zoom read through scanlines is harder to read rather than easier.

A screenshot of an output captures the *processed* buffer, so a screenshot of a phosphor desktop
looks like the desktop. A per-window capture renders the window itself and is untinted. Both are
honest answers to different questions.

## Wallpaper

The wallpaper is drawn by the compositor rather than by a client, and it is **retinted to follow
the accent**: `kdos theme` remaps the shipped image into the current palette and caches it, and
the compositor prefers that cache. `wallpaper = none` in `comp.conf` is an honest off.

The image ships with no scanlines baked in — the shader draws those — because two sets of
scanlines beating against each other is moiré rather than identity.

## Style files

An accent plus a set of related settings can be bundled and shared:

```sh
kdos theme style retro.kdos
```

A style file is flat `key = value` and may carry `accent`, `crt`, `crt_scanlines`, `crt_curve`,
`crt_fullscreen`, `chrome_font` and `clock_format`, plus any **dotted** key, which is appended to
the compositor's generated frame theme so it wins over the generated block.

It rewrites only the keys it names. Every other line in your `comp.conf` survives verbatim, and a
key the style does not mention is left alone.

## Fonts

Two fonts matter, and they are different objects:

- **The console font** is a bitmap PSF at 16x32, built from source and loaded by `kdos-getty` on
  every terminal. It carries 512 glyphs, which is why parts of this system restrict themselves to
  a small glyph set — anything outside it renders as a blank on `tty1`.
- **`chrome_font`** in `comp.conf` is what the desktop's own surfaces are drawn with, as a
  fontconfig pattern (`Terminus:pixelsize=32`). It is one pixel size for every screen, which is
  right on a machine with one monitor and wrong on two of different densities.

The compositor's own title bars are a separate setting — `<theme><font>` in `rc.xml`, in **points**
— and they must name a **scalable** face. Pango has not rendered bitmap fonts for many releases,
so naming the bitmap Terminus there resolves and then silently falls back to a generic sans. The
shipped configuration names the TrueType Terminus at 24 points, which is 32 pixels at 96 dpi, so
a title bar is exactly one cell tall.

## Theming applications inside boxes

A box shares your **home directory** and nothing else, so `/usr/share/themes` and
`/usr/share/icons` on the host are invisible inside it. Everything a boxed application reads is
therefore written into `$HOME`:

| Path | Read by |
|---|---|
| `~/.themes/KDOS/` | GTK3 and non-libadwaita GTK4 |
| `~/.config/gtk-3.0/gtk.css`, `~/.config/gtk-4.0/gtk.css` | libadwaita, which ignores themes entirely |
| `~/.icons/KDOS/` | Every toolkit |
| `~/.icons/KDOS-cursors/` | Cursor lookup |
| `~/.config/kdeglobals` | Qt, under the KDE platform theme |

**The GTK theme is a recoloured `adw-gtk3`**, and choosing that specifically is what makes it
work. Stock GTK3's Adwaita is compiled from SASS with literal colour values in nearly every rule,
so redefining named colours reaches only a few widgets and hand-written overrides never cover
enough. `adw-gtk3` is the libadwaita stylesheet ported to GTK3 and is written against named
colours end to end, so rewriting the palette makes every widget follow — and GTK3 applications end
up genuinely identical to GTK4 ones.

**Qt has two routes and the right one is chosen per pack.** Where a pack's runtime provides KDE's
platform integration, Qt applications read `~/.config/kdeglobals` directly. Where it does not, the
GTK platform theme plus a style override is used instead. Neither may be exported on a guess:
the fallback style with no platform theme lands on Qt's built-in *light* palette, which is worse
than doing nothing. The runtime declares which it provides, so the answer travels with the
packages rather than being inferred.

`kdeglobals` is **merged, never overwritten** — KDE applications write their own settings into
that file, so only the sections the theme owns are replaced and everything else is kept.

## How the theme is generated

`kdos-theme` is the generator, with three subcommands:

```sh
kdos-theme gtk     <out> [accent]
kdos-theme icons   <out> [accent]
kdos-theme cursors <out> [accent]
kdos-theme accents
```

The icon and cursor themes are **vendored, pruned and recoloured** rather than drawn: a
maintenance script prunes an upstream release into a committed source tree, and the generator
recolours that tree into the palette at build time and again on every accent switch. Colours are
mapped by hue **family** rather than flattened onto the accent — blues and greens become the
accent, warm hues become the secondary, reds stay urgent — because the upstream icon set
colour-codes file types, and collapsing every hue onto one accent turns a folder of files into a
wall of identical lozenges. A PDF stays red and an audio file stays amber while folders and
devices go phosphor.

Application icons are **never** recoloured. A phosphor Firefox logo is vandalism, not theming.

### Auditing

```sh
kdos theme --audit          # does what is installed match what this palette produces?
kdos theme --audit amber    # what would switching to amber change?
```

The audit does not try to recognise "palette colours" in the installed files. It runs the same
generators into a scratch directory and compares byte for byte, symlinks included. Anything that
differs, differs from what your palette produces right now. It writes nothing outside its scratch
directory and repairs nothing — an audit that fixed what it found would be a `kdos theme` under a
misleading name. Exit 0 is clean, 1 is drift, 2 means the audit could not run.

## See also

- [The design language](../03-architecture/design-language.md) — the palette slots and the contrast rules
- [kdos-comp](../04-programs/kdos-comp.md) — the CRT pass and the frame theme
- [Configuration](../06-reference/configuration.md) — every key named here, with defaults
- [Applications](applications.md) — why boxed software is themed through `$HOME`
- [The kdos command](../04-programs/kdos-command.md) — `kdos theme` in full
