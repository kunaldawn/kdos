# Input methods

This page covers typing languages a keyboard layout alone cannot type, Chinese, Japanese and Korean
among them, through [fcitx5]{.idx}: which engines ship, how the session starts them, the files they
are configured through, switching between them, and what to check when typing comes out as Latin.
It also covers the on-screen keyboard, which follows the same text-input signal. How the engine,
the compositor and the application are wired together, and why X11 and Qt 5 applications are left
out, is in [Input methods](../../kdos/03-architecture/session.md#input-methods).

## Installation

| Port | Provides |
|:--|:--|
| [fcitx5](#port-fcitx5) | The engine framework, `fcitx5-remote`, `fcitx5-diagnose`, and the English keyboard engine |
| [fcitx5-chinese-addons](#port-fcitx5-chinese-addons), over [libime](#port-libime) | `pinyin`, `shuangpin` and the table methods such as `wbx` (Wubi) and `cangjie` |
| [fcitx5-anthy](#port-fcitx5-anthy), over [anthy-unicode](#port-anthy-unicode) | `anthy`, for Japanese |
| [fcitx5-hangul](#port-fcitx5-hangul), over [libhangul](#port-libhangul) | `hangul`, for Korean |
| [kdos-shell](#port-kdos-shell) | [`kdos-ime`]{.idx}, the candidate window drawn in cells |
| [kdos-comp](#port-kdos-comp) | The relay between fcitx5 (`input-method-v2`) and applications (`text-input-v3`) |
| [wvkbd](#port-wvkbd) | The on-screen keyboard |

All of them are installed by `50_desktop`, except [`wvkbd`]{.idx}, which `42_graphics` installs, so every
image has them, live and installed. There is no service script. fcitx5 is a program of the
desktop session, started for each login by `kdos-desktop-start` once the compositor is up
(`fs/usr/local/bin/kdos-desktop-start`):

<!-- kfs:include fs/usr/local/bin/kdos-desktop-start lines=96-108 pin=74e73dc30bb8 -->

`kdos-ime` starts first, because fcitx5 hands its candidate window to whoever owns
`org.kde.impanel` when it appears. With fcitx5 not installed the block does nothing and prints
nothing. See [kdos-ime](../../kdos/04-programs/kdos-shell.md#kdos-ime).

## Configuration

### ~/.config/fcitx5/profile

Which engines a key can reach. Every new account gets the shipped copy,
`fs/etc/skel/.config/fcitx5/profile`: one group whose first entry is the keyboard layout and whose
others are the engines the image installs:

<!-- kfs:include fs/etc/skel/.config/fcitx5/profile -->

The first item is the *inactive* state, plain typing in the layout; the others are what turning the
[input method]{.idx} on can reach. fcitx5 writes this file itself when it saves its state, so edit it with
fcitx5 stopped (see [Adding an engine](#adding-an-engine)).

### ~/.config/fcitx5/config and conf/

fcitx5's global options, its hotkeys among them, live in `~/.config/fcitx5/config`, and each add-on's
options in `~/.config/fcitx5/conf/<addon>.conf`, such as `conf/pinyin.conf`. None of them ships;
fcitx5 uses its defaults until one exists. The configuration tool upstream ships for these files is
a Qt program that is not a port, so they are edited by hand.

### The build

fcitx5 is built for [Wayland]{.idx} only, with no X11 frontend and no autostart entry
(`ports/core/input/fcitx5/build.sh`); every flag is explained on its
[package page](#port-fcitx5):

<!-- kfs:include ports/core/input/fcitx5/build.sh lines=15-37 pin=fae713b9465c -->

The Chinese engines are built without cloud pinyin, which would send what you type to a remote
service, and without their Qt configuration windows:

<!-- kfs:include ports/core/input/fcitx5-chinese-addons/build.sh lines=20-22 pin=ac3f6438b8e8 -->

### Inside a box

A boxed application reaches fcitx5 through the compositor, never directly. `kdos-appbox` sets one
variable for it (`src/system/kdos-appbox/main.c`):

<!-- kfs:include src/system/kdos-appbox/main.c lines=341-354 pin=fe9f35096fbb -->

### The on-screen keyboard

`osk` in `~/.config/kdos/comp.conf` (`fs/etc/skel/.config/kdos/comp.conf`) decides whether the
compositor runs `wvkbd`, and whether a focused text field raises it:

<!-- kfs:include fs/etc/skel/.config/kdos/comp.conf lines=321-325 pin=32ce41c64116 -->

## Usage

### Typing in another language

1. Click into a text field of a GTK 3, GTK 4 or Qt 6 application, host or boxed.
2. Press `Ctrl+Space` to turn the input method on. fcitx5 switches from `keyboard-us` to the
   engine used last; the first time, that is the one after it in the profile, `pinyin`.
3. Type. The text being composed is shown in the field, and the numbered candidates appear
   in `kdos-ime`'s window, a bar drawn in cells above the panel; choose a candidate with its number
   key.
4. Press `Ctrl+Space` again to go back to plain typing.

To reach another engine, press `Ctrl+Space` and keep `Ctrl` held: each further `Space` steps to
the next engine in the group.

`Ctrl+Space` is fcitx5's own default, and the compositor does not bind it. While an engine is on,
the left `Shift` on its own switches to plain typing and back.

### Switching engine from a prompt

```bash
fcitx5-remote -n             # the engine in use
fcitx5-remote -s anthy       # switch to anthy
fcitx5-remote -s hangul
fcitx5-remote -s keyboard-us
fcitx5-remote                # 1 inactive, 2 active, 0 no text field has the focus
```

Each of these acts on the text field that has the focus, so run them in a terminal window of the
session, which is itself such a field; with nothing focused, `-n` prints an empty line and the
state is `0`. An engine that is not in the profile's group cannot be switched to; `-s` then
does nothing. A query is no test of whether fcitx5 runs: when it is stopped, any `fcitx5-remote`
call but `-e` starts it again through D-Bus activation
(`/usr/share/dbus-1/services/org.fcitx.Fcitx5.service`). Use `pgrep -a fcitx5` instead.

### Adding an engine

To make `shuangpin` or a table method such as `wbx` reachable:

1. Stop fcitx5, so it does not write its own copy of the profile over yours: `fcitx5-remote -e`.
   Run no other `fcitx5-remote` command until step 3: one would start fcitx5 again with the old
   profile, and that copy writes it back over your edit when it exits.
2. Edit `~/.config/fcitx5/profile` and add an item after the last one, numbered one higher:

   ```ini
   [Groups/0/Items/4]
   Name=shuangpin
   Layout=
   ```

3. Start it again: `fcitx5 -d`.
4. Check, from a terminal window: `fcitx5-remote -s shuangpin`, then `fcitx5-remote -n`, which
   prints `shuangpin`. fcitx5 rewrites the file the next time it saves, keeping the item and
   turning its empty `Layout=` into a comment.

To change an option of an engine or a hotkey, edit the file under `~/.config/fcitx5/` the same way
and run `fcitx5-remote -r` to reload, or restart fcitx5 as above.

### The on-screen keyboard

1. Set `osk = auto` in `~/.config/kdos/comp.conf` for a keyboard that appears whenever a text
   field has the focus, or `osk = manual` for one shown only by `Super+Alt+K`.
2. Log out and back in; `osk` is read once per session.

`auto` follows the text-input signal, which an application sends only while fcitx5 is running.

### Restarting the input method

After a crash, or to pick up a hand-edited file:

```bash
pkill -x kdos-ime; kdos-ime >/dev/null 2>&1 &
fcitx5 -rd
```

`-r` replaces a running fcitx5 and `-d` puts it in the background. Start `kdos-ime` first, as the
session does, or fcitx5 draws its own candidate window.

### The live medium and an installed system

The engines and the profile are the same on both. On the live medium the home directory is in
memory, so a changed profile lasts until power-off unless a persistence store exists
([kdos persist](../../kdos/04-programs/kdos-command.md#kdos-persist)).

## Tips

- `kdos doctor` says whether fcitx5 is running when it is installed.
- Never set `GTK_IM_MODULE`, and never set `QT_IM_MODULE=fcitx`. GTK on Wayland finds the route
  itself when the variable is unset, and the `fcitx` modules are the X11-era route, which is not
  there. `kdos explain GTK_IM_MODULE` prints the reason in full.
- fcitx5's default key for the next *group* of input methods is `Super+Space`, which the shipped
  `rc.xml` binds to the palette. The shipped profile has one group, so there is nothing to switch
  to.
- fcitx5 exits when the compositor goes away; the next session starts it again.
- Upstream also installs `fcitx5-configtool`, a script that looks for a Qt configuration program
  and, finding none, asks through `kdialog` or `zenity` whether to open the configuration
  directory instead. Run from a login shell on the live medium, the `zenity` question crashed with
  a segmentation fault and the script ended having done nothing.

## Troubleshooting

**`Ctrl+Space` does nothing and every key arrives as Latin.**

1. `pgrep -a fcitx5`. No line means fcitx5 is not running: start it as in
   [Restarting the input method](#restarting-the-input-method). `kdos doctor` says the same
   (`fcitx5 is running — Ctrl+Space switches input method`).
2. `fcitx5-remote -n` while the field has the focus. `keyboard-us` after `Ctrl+Space` means the
   group holds nothing else: check that `~/.config/fcitx5/profile` lists the engines, and copy
   [`/etc/skel/.config/fcitx5/profile`]{.idx} over it if not, with fcitx5 stopped.
3. Check the application. An X11 application under Xwayland, and a Qt 5 application, get no input
   method; see [Known gaps](#known-gaps). If it works in another GTK 3, GTK 4 or Qt 6 program and
   not in this one, the toolkit is the cause.

**Candidates appear in a window that is not drawn in cells.** fcitx5 drew its own candidate
window because `kdos-ime` was not running when it started. `pgrep -x kdos-ime`; restart both as above.

**`kdos-ime` refuses to start with `org.kde.impanel is already owned — something else is
drawing the candidate window`.** Another process owns `org.kde.impanel`, usually a `kdos-ime`
that is still running. `pkill -x kdos-ime`, then start it again.

**A GTK application takes no input method although others do.** Something in its environment
sets `GTK_IM_MODULE`, a wrapper script or a profile of your own. Unset it there.

**Running fcitx5 in the foreground for its messages.** Stop it with `fcitx5-remote -e` and run
`fcitx5 -r` in a terminal; it logs to the terminal until you press `Ctrl+C`. Start it again with
`fcitx5 -d` afterwards. `fcitx5-diagnose` prints upstream's report of the environment.

## Known gaps

- [X11 and Qt 5 applications get no input method](../../kdos/06-reference/known-gaps.md#x11-and-qt-5-applications-get-no-input-method)
- [There is no input-method configuration tool](../../kdos/06-reference/known-gaps.md#there-is-no-input-method-configuration-tool)
- [Every program speaks English](../../kdos/06-reference/known-gaps.md#every-program-speaks-english):
  input methods type other languages; no program's menus are translated
- Choosing a candidate with its number key has not been tried on a display; the candidate window
  itself has been photographed on the live medium. See
  [Status](../../kdos/06-reference/status.md#the-desktop)

## See also

- [Input methods](../../kdos/03-architecture/session.md#input-methods): the three parties and the
  two protocols
- [kdos-ime](../../kdos/04-programs/kdos-shell.md#kdos-ime): the candidate window
- [Input methods](../../kdos/02-user-guide/administration.md#input-methods) and
  [Language](../../kdos/02-user-guide/administration.md#language) in the user guide
- [The on-screen keyboard](../../kdos/04-programs/kdos-comp.md#the-on-screen-keyboard) and
  [The on-screen keyboard](../../kdos/02-user-guide/accessibility.md#the-on-screen-keyboard)
- [The environment a box receives](../../kdos/03-architecture/session.md#the-environment-a-box-receives)
- `displays-and-scaling.md`: [Displays and scaling](displays-and-scaling.md)

<!-- book-nav -->
---

*Part XVI. System administration, chapter 415.* Previous: [414. Xwayland and X11 applications](xwayland-and-x11-apps.md) · [Contents](../README.md) · Next: [416. Fonts and theming](fonts-and-theming.md)
