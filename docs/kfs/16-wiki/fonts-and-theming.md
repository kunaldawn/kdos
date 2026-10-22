# Fonts and theming

This page covers the look of a KDOS machine: the [fonts]{.idx term="font"} fontconfig offers and how to add one, the
fonts the desktop itself draws in, the colour scheme KDOS calls an [accent]{.idx}, the phosphor
pass, the wallpaper, and how the same colours reach the [boot menu]{.idx}, the text consoles and the
applications. What each generated file is and when each program reads it is explained in
[Theming](../../kdos/02-user-guide/theming.md); this page gives the procedures and the files.

## Installation

The fonts are ordinary ports on the `fonts` shelf. Every one below is named in a phase list, so all
of them are on the image:

| Port | Installs |
|:--|:--|
| [fontconfig](#port-fontconfig) | The font library, `fc-list`, `fc-match`, `fc-cache`, and [`/etc/fonts/fonts.conf`]{.idx} with its stock rules in [`/etc/fonts/conf.d`]{.idx} |
| [noto-fonts](#port-noto-fonts) | Noto Sans and Noto Sans Mono, and the rule that puts them first for `sans-serif` and `monospace` |
| [noto-fonts-extra](#port-noto-fonts-extra) | Noto Sans for Arabic, Hebrew, the Indic scripts, Sinhala, Thai, Lao, Khmer, Myanmar, Ethiopic, Armenian and Georgian, with a rule that adds them to `sans-serif` |
| [noto-cjk](#port-noto-cjk) | Noto Sans CJK, chosen by language for Japanese, Chinese and Korean text |
| [noto-emoji](#port-noto-emoji) | Noto Color Emoji, as the `emoji` family |
| [terminus-font](#port-terminus-font) | The bitmap Terminus faces, and the console fonts under `/usr/share/consolefonts`, among them `ter-kdos32n`, which `kdos-getty` loads on every console |
| [terminus-ttf](#port-terminus-ttf) | `Terminus (TTF)`, the same design as a scalable face, for the programs that cannot draw a bitmap face, such as the window title bars |
| [nerd-fonts-symbols](#port-nerd-fonts-symbols) | Symbols Nerd Font, as a fallback for icon characters |
| [ttf-dejavu](#port-ttf-dejavu), [font-liberation](#port-font-liberation), [font-carlito](#port-font-carlito), [font-caladea](#port-font-caladea) | DejaVu, and the metric-compatible faces office documents ask for |
| [font-manager](#port-font-manager) | Font Manager, a GTK 4 application for browsing and previewing fonts |

The [theme]{.idx} is built from four ports of KDOS's own under `src/art/`:
[kdos-theme](#port-kdos-theme), the generator, and the artwork it recolours,
[kdos-icons](#port-kdos-icons), [kdos-cursors](#port-kdos-cursors) and
[kdos-gtk-theme](#port-kdos-gtk-theme). The `kdos theme` command is part of
[kdos-tools](#port-kdos-tools), the picker `kdos-style` is a name of
[kdos-shell](#port-kdos-shell), and the phosphor pass and the wallpaper belong to the compositor,
[kdos-comp](#port-kdos-comp). Qt applications reach the palette through
[plasma-integration](#port-plasma-integration), [qt5ct](#port-qt5ct) and [qt6ct](#port-qt6ct);
boxed applications through the settings [portal]{.idx} of
[xdg-desktop-portal-kdos](#port-xdg-desktop-portal-kdos).

No service is involved. Two image steps finish the job at build time:
`70_image/085_fontcache.sh` writes the system font cache into `/usr/lib/fontconfig/cache`, and
`70_image/050_theme.sh` runs `kdos theme` once against `/etc/skel` with the default accent, so every
new home, the live session's included, starts themed. After that, every `kpkg install` or removal
that touches [`/usr/share/fonts`]{.idx} or [`/etc/fonts`]{.idx} rebuilds the font cache through kpkg's triggers.

## Configuration

### /etc/fonts/conf.d

fontconfig reads `/etc/fonts/fonts.conf`, which takes every rule in `/etc/fonts/conf.d` in the
order of the file names. Beside fontconfig's own rules, the font ports add theirs. The one that
decides the desktop's default faces comes from [noto-fonts](#port-noto-fonts):

<!-- kfs:include ports/core/fonts/noto-fonts/build.sh lines=27-49 pin=f6b2c8e8cd13 -->

The icon fallback from [nerd-fonts-symbols](#port-nerd-fonts-symbols) accepts its faces behind
`monospace`, `Terminus`, `Terminus (TTF)`, `sans-serif` and `serif`:

<!-- kfs:include ports/core/fonts/nerd-fonts-symbols/build.sh lines=20-46 pin=40f0ba96bb2d -->

The emoji rule from [noto-emoji](#port-noto-emoji):

<!-- kfs:include ports/core/fonts/noto-emoji/build.sh lines=22-39 pin=efaf85ff7676 -->

fontconfig's stock set includes `70-no-bitmaps-except-emoji.conf`, which rejects every bitmap face;
[terminus-font](#port-terminus-font) installs upstream's `75-yes-terminus.conf`, which accepts the
bitmap Terminus again:

<!-- kfs:include ports/core/fonts/terminus-font/build.sh lines=29-29 pin=2b764e41c5fb -->

A rule of your own for every account goes in [`/etc/fonts/local.conf`]{.idx}, which the stock
`51-local.conf` reads; a rule for one account goes in `~/.config/fontconfig/fonts.conf`.

### ~/.config/kdos/comp.conf

The desktop's own fonts, the phosphor pass and the wallpaper are compositor keys. Every account's
copy starts as `/etc/skel/.config/kdos/comp.conf`, from `fs/etc/skel/.config/kdos/comp.conf`, with
each key commented out at its default. The panel's font:

<!-- kfs:include fs/etc/skel/.config/kdos/comp.conf lines=44-57 pin=436c1ca34021 -->

`chrome_font`, the font of the surfaces the compositor supervises other than the bar and the
on-screen keyboard, which `panel_font` falls back to when it is empty:

<!-- kfs:include fs/etc/skel/.config/kdos/comp.conf lines=103-121 pin=83f1ef1455a0 -->

The phosphor pass:

<!-- kfs:include fs/etc/skel/.config/kdos/comp.conf lines=174-203 pin=e71568e1b370 -->

The wallpaper:

<!-- kfs:include fs/etc/skel/.config/kdos/comp.conf lines=22-25 pin=79ff2287a7dd -->

Which of these apply on a reload and which wait for the next login is in
[Keys applied on reload](../../kdos/04-programs/kdos-comp.md#keys-applied-on-reload) and
[Keys applied at the next login](../../kdos/04-programs/kdos-comp.md#keys-applied-at-the-next-login).

### ~/.config/kdos-comp/rc.xml

Title bars, the compositor's menus and the window switcher are drawn with pango, which draws no
bitmap face, so they name the scalable `Terminus (TTF)`, in points. From
`fs/etc/skel/.config/kdos-comp/rc.xml`:

<!-- kfs:include fs/etc/skel/.config/kdos-comp/rc.xml lines=91-95 pin=235701dafef5 -->

### ~/.config/foot/foot.ini

The terminal on Super+Return is foot, and its font is its own, from
`fs/etc/skel/.config/foot/foot.ini`:

<!-- kfs:include fs/etc/skel/.config/foot/foot.ini lines=25-33 pin=1b16beda4e43 -->

### The accent and the files generated from it

The accent in force is one word in `~/.cache/kdos/theme`. `kdos theme` writes it and then rewrites
a file for every program that does not read it directly; the full table, with the moment each one
takes effect, is in [What changes when](../../kdos/02-user-guide/theming.md#what-changes-when).
Never edit a generated file. The files that select a generated theme, and belong to you, are listed
in [Files you may edit, and files you may not](../../kdos/02-user-guide/theming.md#files-you-may-edit-and-files-you-may-not).

Three root-owned files follow the accent as well: [`/etc/kdos/accent`]{.idx} (the splash), `/etc/vtrgb`
(the console palette) and [`/boot/efi/limine.conf`]{.idx} (the boot menu). `kdos theme` asks
`kdos-powerd` to write them, which it does for root and for members of `wheel`.

### /etc/profile.d/10-wayland.sh

The login shell exports what sends native GTK and Qt applications to the portal and to the KDOS
palette, from `fs/etc/profile.d/10-wayland.sh`:

<!-- kfs:include fs/etc/profile.d/10-wayland.sh lines=85-95 pin=6e24c8e47783 -->

A GTK application that does not use the portal reads GSettings, whose defaults KDOS sets in
`/usr/share/glib-2.0/schemas/90_kdos.gschema.override`, from the same path under `fs/`:

<!-- kfs:include fs/usr/share/glib-2.0/schemas/90_kdos.gschema.override lines=24-31 pin=26ccfd26c727 -->

## Usage

### Seeing which font a name resolves to

1. List the families fontconfig knows:

   ```bash
   fc-list : family | sort -u
   ```

2. Ask what a generic name, or a name of your own, resolves to:

   ```bash
   fc-match sans-serif
   fc-match monospace
   fc-match emoji
   fc-match 'Terminus:pixelsize=32'
   ```

   On the image these answer `"Noto Sans" "Regular"`, `"Noto Sans Mono" "Regular"`,
   `"Noto Color Emoji" "Regular"` and the bitmap file `ter-x32n.pcf.gz`.

3. See the whole fallback chain for a name, which is where a missing glyph is looked for:

   ```bash
   fc-match -s monospace | head
   ```

### Adding a font for one account

1. Copy the font files into `~/.local/share/fonts`, in a directory of their own if you like:

   ```bash
   mkdir -p ~/.local/share/fonts
   cp ~/Downloads/SomeFont/*.ttf ~/.local/share/fonts/
   ```

2. Rebuild your cache and check that the family is listed:

   ```bash
   fc-cache -f ~/.local/share/fonts
   fc-list | grep -i somefont
   ```

   `fc-list` prints the file's path and its family, such as
   `/home/kdos/.local/share/fonts/SomeFont.ttf: Some Font:style=Regular`.

3. Start the program that should use it again. A running program keeps the font list it read.

The files are in your home directory, which a box shares unless its profile says
`home = private`, so a boxed application's own fontconfig can find them there too.

On the live medium the copy lives in memory and is gone at power-off, unless the session has a
[persistence]{.idx term="persistence store"} store; see [Live persistence](live-persistence.md).

### Adding a font for every account

Package it. A font port is a few lines of `build.sh` that install the files under
`/usr/share/fonts/<name>`, and kpkg rebuilds the system cache when the package is installed or
removed; [Writing ports](../../kdos/05-developer/writing-ports.md) has the recipe format, and the
ports above are worked examples. Files copied into `/usr/share/fonts` by hand are owned by no
package, so `kdos why` answers
`package  (none — provided by fs/ or 10_bootstrap, or not installed)` and `sudo kpkg check --all` lists each as `unowned ./usr/share/fonts/<dir>/<file>`, and they are not
cached until you run `sudo fc-cache -f`. The full check reads every installed file and takes about
ten minutes on the live medium.

### Changing the accent

1. Press Super+Ctrl+Shift+Space. The picker, **Style**, opens on its **Accent** page.
2. Move the highlight with the arrow keys. The whole desktop previews each accent as the
   highlight reaches it.
3. Press Enter to keep it, or Esc to go back to the accent the picker opened on.

From a prompt:

```bash
kdos theme list
kdos theme amber
kdos theme next
```

`list` marks the accent in force with `*`; a new home, the live session's included, starts on
`bone`. Each switch prints the accent and its theme name, such as `amber (KDOS-Amber)`.
Every KDOS surface repaints at once. Other programs take the change at different moments, many
at their next start; see [What repaints at once, and what waits](../../kdos/02-user-guide/theming.md#what-repaints-at-once-and-what-waits).

On the live medium there is no `/boot/efi/limine.conf`, so the boot menu is not recoloured;
`/etc/kdos/accent` is absent until the first switch and is written by it. The root-owned files
and your home are in memory unless a persistence store exists. On an installed
system the boot menu changes on the next boot and the console palette at the next login prompt.

### Changing the desktop's font

1. Open the picker's **Font** page: press Super+Ctrl+Shift+Space and then Right, or run
   `kdos-style --page font`. Only monospace families are offered.
2. Move the highlight; the picker's own window shows each face. Press Enter.
3. Log out and back in. The picker writes the family into `chrome_font` and `panel_font` in
   `~/.config/kdos/comp.conf` and keeps the size each key already had. The panel and the other
   supervised surfaces are given their font when the session starts; a menu or window opened after
   the change reads the file itself and shows the new family at once.

To change a size, edit the value instead, for example `chrome_font = Terminus:pixelsize=64`. The
sizes Terminus draws exactly are listed in the file's own comment, above. Making text larger on a
high-density screen is covered in [Displays and scaling](displays-and-scaling.md).

The title bars are separate: change the five `<font>` lines in `~/.config/kdos-comp/rc.xml`, name
a scalable face, then run `kdos-comp -r` from a terminal inside the session.

### Tuning or switching off the phosphor pass

1. Open **Settings** (Super+I) and its **Appearance** page, which applies a change at once; or
   edit the `crt` keys in `~/.config/kdos/comp.conf`.
2. After a hand edit, reload the compositor with `kdos-comp -r`.

`crt = 0` switches the pass off at once. In a session that started with `crt = 0` there is no pass
to tune, so raising `crt` waits for the next login; turning on `crt_curve` mid-session also needs a
new login before the pointer follows the curve. Both reasons are in
[Changes that need a new session](../../kdos/02-user-guide/theming.md#changes-that-need-a-new-session).

### Using your own wallpaper

1. Set the path in `~/.config/kdos/comp.conf`, or on the Settings Appearance page:

   ```text
   wallpaper = ~/Pictures/beach.png
   ```

2. Delete the recoloured copy that wins over the key:

   ```bash
   rm ~/.cache/kdos/wallpaper.png
   ```

3. Reload with `kdos-comp -r`.

The picture must be a PNG. The next accent switch writes `~/.cache/kdos/wallpaper.png` again, so
repeat step 2 after one. The order the compositor applies is in
[Wallpaper](../../kdos/02-user-guide/theming.md#wallpaper).

### Applying a style file

A style file sets an accent and a few compositor keys in one go:

```ini
# retro.kdos
accent = amber
crt = 70
crt_scanlines = 60
```

```bash
kdos theme style retro.kdos
```

It prints `amber (KDOS-Amber) — style applied`, and the compositor keys it sets are appended to
`~/.config/kdos/comp.conf`. Its keys are listed in
[Style files](../../kdos/02-user-guide/theming.md#style-files).

### Checking that the generated files match the accent

```bash
kdos theme --audit
kdos theme --audit paper
```

The first compares what is in your home with what the accent in force produces; the second shows
what switching to `paper` would change. The audit works in a scratch directory and changes nothing
in your home. Exit status 0 means everything matches and 1 that something differs. To repair a
drifted file, run `kdos theme <accent>` again, or `kdos update theme` after a package upgrade has
brought new artwork.

## Tips

- `kdos-bootctl palette amber` prints the console palette an accent would load, without writing
  anything.
- `paper` is the one light accent. The settings portal reports a dark colour-scheme preference
  under every accent, `paper` included, so a libadwaita application draws dark on a `paper`
  desktop.
- A helix or micro theme of your own can build on the generated one: `inherits = "kdos"` in helix,
  `include "kdos"` in micro.
- `~/.config/gtk-3.0/gtk.css` is deleted by every accent switch. Change GTK 3's look through the
  accent rather than with a stylesheet of your own.
- `kdos why /etc/fonts/conf.d/56-noto-preferred.conf` names the package that owns a rule file and
  prints any recorded reason about it.

## Troubleshooting

**`kdos doctor` warns `no accent applied` or `foot theme missing`.**
The accent file or a generated theme is absent from your home. Run the command the warning names,
or `kdos theme <accent>` for the accent you want.

**A font change in `comp.conf` did nothing.**
The panel and the supervised surfaces are given their font when the session starts: log out and
back in. If it still did nothing, check that fontconfig resolves the name to the face you meant:

```bash
fc-match 'Terminus:pixelsize=64'
```

Then read the compositor's log, which names every `comp.conf` line it could not use, with its line
number:

```bash
grep comp.conf "$XDG_RUNTIME_DIR/kdos-comp.log"
```

**The title bars are in a generic sans-serif face.**
`rc.xml` names a bitmap face, such as `Terminus`. Pango cannot draw one and falls back without a
word. Name `Terminus (TTF)` or another scalable face, then `kdos-comp -r`.

**A character shows as a box or a blank.**
Ask fontconfig which face it would use, with the character's code point:

```bash
fc-match 'monospace:charset=1f600'
```

If no installed face covers it, install one that does. On `tty1` the console font holds 512 glyphs,
and nothing outside them can be drawn there at all.

**There is no phosphor glow.**
The pass runs only on the GPU renderer. Look for the `crt:` lines in
`$XDG_RUNTIME_DIR/kdos-comp.log`: they say whether it is off because this is not the GLES2
renderer (a virtual machine without virgl, for instance), because `crt` was `0` at login, or because
the shader failed. On the software renderer the session logs
`crt: off — this is not the GLES2 renderer, and a fullscreen shader on a software renderer is a slideshow`
at start-up and `crt: on in comp.conf, but this session could not have it` at every reload. The pass also stands aside while the [magnifier]{.idx} is on, and that writes no log
line: press Super+= to turn the magnifier off. See
[When it switches itself off](../../kdos/02-user-guide/theming.md#when-it-switches-itself-off).

**`kdos theme` prints `boot menu and splash unchanged (kdos-powerd did not answer)`.**
The account is not in `wheel`, `kdos-powerd` is not running, or the command ran inside a box.
`service status powerd` shows the daemon; `id` shows the groups. Everything in your home was still
written.

**A boxed Qt application ignores the accent.**
A box the store built with `kdos app install` is given no `QT_QPA_PLATFORMTHEME`, so its Qt
applications draw Qt's defaults; see
[Theming applications inside boxes](../../kdos/02-user-guide/theming.md#theming-applications-inside-boxes).

**A running libadwaita or KDE application keeps the old colours.**
It reads its palette when it starts. Restart it.

## Known gaps

- [Changing the accent colour reaches running GTK3 applications, not running libadwaita or KDE ones](../../kdos/06-reference/known-gaps.md#changing-the-accent-colour-reaches-running-gtk3-applications-not-running-libadwaita-or-kde-ones)
- [Emoji draw everywhere except the text console on tty1](../../kdos/06-reference/known-gaps.md#emoji-draw-everywhere-except-the-text-console-on-tty1)
- [Nerd Font icons are blank on tty1, and the shipped configurations turn them off everywhere](../../kdos/06-reference/known-gaps.md#nerd-font-icons-are-blank-on-tty1-and-the-shipped-configurations-turn-them-off-everywhere)
- [KDOS's own surfaces have not been seen at a fractional scale](../../kdos/06-reference/known-gaps.md#kdoss-own-surfaces-have-not-been-seen-at-a-fractional-scale)
- [The phosphor shader is not in any rig photograph](../../kdos/06-reference/known-gaps.md#the-phosphor-shader-is-not-in-any-rig-photograph)

## See also

- [Theming](../../kdos/02-user-guide/theming.md): the accents, every generated file, and the generators
- [The design language](../../kdos/03-architecture/design-language.md): the palette slots and the contrast rules
- [kdos theme](../../kdos/04-programs/kdos-command.md#kdos-theme): the command in full
- [The phosphor pass](../../kdos/04-programs/kdos-comp.md#the-phosphor-pass) and
  [The wallpaper](../../kdos/04-programs/kdos-comp.md#the-wallpaper): the compositor's side
- [~/.config/kdos/comp.conf](../../kdos/06-reference/configuration.md#configkdoscompconf): every key
- [Displays and scaling](displays-and-scaling.md), [Accessibility](accessibility.md),
  [Locale and console](locale-and-console.md), [Portals and screen sharing](portals-and-screen-sharing.md)

<!-- book-nav -->
---

*Part XVI. System administration, chapter 416.* Previous: [415. Input methods](input-methods.md) · [Contents](../README.md) · Next: [417. Accessibility](accessibility.md)
