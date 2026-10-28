# Locale and console

This page covers the language settings every login gets, the keyboard layout of the text consoles
and of the desktop, and the [font]{.idx} and colours of the consoles. KDOS speaks English only, so a
[locale]{.idx} here decides character handling and a few formats rather than the language of the
programs. The page shows where each setting lives, how to change the keyboard layout, and what to
check when a console comes up with the wrong font or layout. How the consoles are started, and why
the font is loaded where it is, is in the KDOS book's
[The console](../../kdos/03-architecture/boot-and-init.md#the-console) section.

## Installation

| What | Port |
|---|---|
| The C library and its built-in `C.UTF-8` locale | [musl](#port-musl) |
| The `locale` command and musl's locale files | [musl-locales](#port-musl-locales) |
| `loadkeys`, `setfont`, `showconsolefont`, `setvtrgb` and the console [keymaps]{.idx term="keymap"} | [kbd](#port-kbd) |
| The [console font]{.idx}, `ter-kdos32n` | [terminus-font](#port-terminus-font) |
| [`kdos-getty`]{.idx}, which loads the font, the colours and the keymap on each console | [kdos-tools](#port-kdos-tools) |
| `kdos-power accent`, which recolours the console | [kdos-powerd](#port-kdos-powerd) |

All of them are on the image. `kdos-getty` is started by [`/etc/inittab`]{.idx}, not by a service script,
so there is nothing to enable. Typing Chinese, Japanese and Korean is the input method's job; see
[Input methods](../../kdos/02-user-guide/administration.md#input-methods).

## Configuration

**`/etc/profile`**, from `fs/etc/profile`, sets the locale of every login shell:

<!-- kfs:include fs/etc/profile lines=33-33 pin=d682fd279e6d -->

**[`/etc/profile.d/00locale.sh`]{.idx}** is installed by musl-locales. Its build names the option that
installs the file, `LOCALE_PROFILE`, in `ports/core/toolchain/musl-locales/build.sh`:

<!-- kfs:include ports/core/toolchain/musl-locales/build.sh lines=12-17 pin=747de6c05196 -->

It sets `MUSL_LOCPATH` to `/usr/share/i18n/locales/musl`, the directory musl reads locale files
from and `locale -a` lists.

**[`/etc/keymap`]{.idx}** holds the console keymap's name on one line. The installer writes it from its
Keyboard page; the image does not ship it, so the live medium uses the kernel's built-in US map.
`kdos-getty` loads it on every console, in `src/system/kdos-tools/getty.c`:

<!-- kfs:include src/system/kdos-tools/getty.c lines=357-367 pin=1d8052890c4d -->

The desktop takes its layout from the same file when the session starts, translating the console
map's name into an XKB layout name, in `fs/usr/local/lib/kdos/session-common.sh`:

<!-- kfs:include fs/usr/local/lib/kdos/session-common.sh lines=43-82 pin=cd7b9d8e5b23 -->

**`~/.config/kdos-comp/environment`**, not shipped, overrides that for one user: a
`XKB_DEFAULT_LAYOUT=` line there replaces the value the login exported, for the compositor and
everything it starts; see
[`~/.config/kdos-comp/environment`](../../kdos/06-reference/configuration.md#configkdos-compenvironment).

**The console font** is compiled into `kdos-getty` and has no setting:

<!-- kfs:include src/system/kdos-tools/getty.c lines=54-55 pin=e884f1c51cdf -->

**[`/etc/vtrgb`]{.idx}**, shipped from `fs/etc/vtrgb`, holds the sixteen console colours as three lines of
red, green and blue values. `kdos-getty` loads it with `setvtrgb`, and setting the accent rewrites
it; see [accent](../../kdos/04-programs/daemons.md#accent).

<!-- kfs:include fs/etc/vtrgb -->

**`/etc/inittab`**, from `fs/etc/inittab`, puts `kdos-getty` in front of the login on `tty1` and
`tty2`:

<!-- kfs:include fs/etc/inittab lines=34-40 pin=527998a111ea -->

## Usage

### Seeing the locale

1. Show the locale every category is in:

   ```bash
   locale
   ```

2. List the locales with files on this machine:

   ```bash
   locale -a
   ```

### Using another locale for one account

Every program's menus and messages stay in English whatever is set
([Known gaps](#known-gaps)). musl-locales' files translate the C library's own messages and its
date and time strings.

1. Add the setting to the account's `~/.bash_profile`, above the block that starts the desktop:

   ```bash
   export LC_TIME=de_DE.UTF-8
   ```

   The name must be one that `locale -a` lists: `C`, `C.UTF-8` and seventeen musl-locales
   files, `cs_CZ.UTF-8` to `sv_SE.UTF-8`. With this one `date` prints `Mi Okt 21 19:14:39 UTC 2026`.

2. Log out and in again. The desktop inherits the login shell's environment.

### Changing the keyboard layout of the consoles and the desktop

1. Find the map's name. Each file under [`/usr/share/keymaps`]{.idx} is a map, named without its
   `.map.gz` suffix:

   ```bash
   find /usr/share/keymaps -name '*.map.gz' | sed 's|.*/||; s|\.map\.gz$||' | sort | less
   ```

2. Try it on the console you are on; it applies to every console at once and lasts until the next
   map is loaded:

   ```bash
   sudo loadkeys de-latin1
   ```

3. Keep it:

   ```bash
   echo de-latin1 | sudo tee /etc/keymap
   ```

   Every console loads it from the next time its login prompt starts.

4. Log out of the desktop and in again. The session reads `/etc/keymap` as it starts and gives the
   compositor the matching layout, here `de`.

On the live medium, `/etc/keymap` lasts until power-off unless the medium has a persistence store
([kdos persist](../../kdos/04-programs/kdos-command.md#kdos-persist)).

### Giving the desktop a layout of its own

When the desktop needs a layout or variant that the console map does not translate to, set it for
one user:

1. Create `~/.config/kdos-comp/environment` with, for example:

   ```text
   XKB_DEFAULT_LAYOUT=de
   XKB_DEFAULT_VARIANT=nodeadkeys
   ```

   The names are XKB's: the layouts are the files in `/usr/share/X11/xkb/symbols`.

2. Log out and in again.

The consoles keep `/etc/keymap`.

### Reading the console font

```bash
showconsolefont -i
```

prints the size of the font loaded on the console you are on, as width, height and glyph count:
`16x32x512` for `ter-kdos32n`, which `kdos-getty` loads and checks for the `16x32` at the start.
From another console or as root, name the console with `-C`: `showconsolefont -i -C /dev/tty2`.
On `tty1` while the desktop holds it the answer is
`showconsolefont: ERROR kdfontop.c:38 is_kd_text_mode: Console is not in text mode`, because the
compositor has switched that console to graphics.

`setfont` loads another font for the current console. The fonts in `/usr/share/consolefonts` are
the Terminus faces from terminus-font; the kbd port removes kbd's own. The console keeps the font
until its login prompt starts again, when `kdos-getty` loads `ter-kdos32n` once more:

```bash
setfont ter-124n
```

## Tips

- `C.UTF-8` is built into musl and needs no file under `MUSL_LOCPATH`.
- `Ctrl+Alt+F2` from the desktop reaches `tty2`, the recovery console, and `Alt+F1` there comes
  back; see [The console](../../kdos/03-architecture/boot-and-init.md#the-console).
- The console font has no emoji and no Nerd Font icons, so a program that prints them on `tty1`
  shows blank cells there; the same program in a desktop terminal shows them.

## Troubleshooting

**A console comes up with the small kernel font.** Read the log `kdos-getty` writes for that
console, then the size actually loaded:

```bash
cat /run/kdos-getty.tty1.log
showconsolefont -i
```

The log is appended to each time the console's getty starts. It holds `kdos-getty`'s own lines,
such as `kdos-getty: tty1 winsize -> <columns>x<rows>` when it corrected the window size, and whatever the
getty it then runs writes to its error output. `setfont` and `loadkeys` run with their output
thrown away, so a font that did not load shows only as the size `showconsolefont -i` reports.
Without `setfont`, from the kbd package, `kdos-getty` loads no font, no colours and no keymap at
all; a missing `/usr/share/consolefonts/ter-kdos32n.psf.gz`, from terminus-font, leaves the
kernel's font.

**The console keyboard has the wrong layout.** Check the file and load it by hand to see the error
`kdos-getty` did not show:

```bash
cat /etc/keymap
sudo loadkeys "$(cat /etc/keymap)"
```

A name that `loadkeys` cannot find is not under `/usr/share/keymaps`, and `loadkeys` says so:
`loadkeys: Unable to open file: <name>: No such file or directory`.

**The consoles have the right layout and the desktop is US.** The session exports a layout only
when XKB has a file by the translated name: the name from the `case` table in `session-common.sh`
above, or else the console map's first two letters. Check that the file is there, here for
`de-latin1`, whose name is `de`:

```bash
ls /usr/share/X11/xkb/symbols/de
```

When there is no such file `kdos-desktop` prints
`kdos-desktop: keymap '<map>' has no XKB layout '<name>' — leaving the default` on `tty1` and
leaves the desktop at its default. Set the layout in `~/.config/kdos-comp/environment` as
[above](#giving-the-desktop-a-layout-of-its-own).

**The console colours are wrong or half black.** `/etc/vtrgb` must be readable and hold three lines
of sixteen numbers each. Setting the accent again rewrites it; see
[accent](../../kdos/04-programs/daemons.md#accent).

**`LANG` or `LC_MESSAGES` set to another language changes nothing.** That is expected; see
[Known gaps](#known-gaps).

## Known gaps

- [Every program speaks English](../../kdos/06-reference/known-gaps.md#every-program-speaks-english)
- [Emoji draw everywhere except the text console on `tty1`](../../kdos/06-reference/known-gaps.md#emoji-draw-everywhere-except-the-text-console-on-tty1)
- [Nerd Font icons are blank on `tty1`, and the shipped configurations turn them off everywhere](../../kdos/06-reference/known-gaps.md#nerd-font-icons-are-blank-on-tty1-and-the-shipped-configurations-turn-them-off-everywhere)
- [There is no graphical login screen (greeter)](../../kdos/06-reference/known-gaps.md#there-is-no-graphical-login-screen-greeter)
- [X11 and Qt 5 applications get no input method](../../kdos/06-reference/known-gaps.md#x11-and-qt-5-applications-get-no-input-method)
- [There is no input-method configuration tool](../../kdos/06-reference/known-gaps.md#there-is-no-input-method-configuration-tool)

## See also

- [The console](../../kdos/03-architecture/boot-and-init.md#the-console): what `kdos-getty` does, in order, and why
- [Language](../../kdos/02-user-guide/administration.md#language): the English-only rule
- [Size and language policy](../../kdos/05-developer/writing-ports.md#size-and-language-policy): why no translations ship
- [2. Keyboard](../../kdos/02-user-guide/installation.md#2-keyboard): the installer's Keyboard page and the console-to-XKB names
- [`/etc/keymap`](../../kdos/06-reference/configuration.md#etckeymap): the file's reference entry
- [Input methods](../../kdos/02-user-guide/administration.md#input-methods): typing Chinese, Japanese and Korean
- [Time and NTP](time-and-ntp.md): the time zone, the other setting the profile carries
- [Login and autologin](login-and-autologin.md): what runs on `tty1` and `tty2`
- [Live persistence](live-persistence.md): keeping the keymap on the live medium

<!-- book-nav -->
---

*Part XVI. System administration, chapter 367.* Previous: [366. Time and NTP](time-and-ntp.md) · [Contents](../README.md) · Next: [368. Users and groups](users-and-groups.md)
