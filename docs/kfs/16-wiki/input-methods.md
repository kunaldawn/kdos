# Input methods

This page covers typing languages a
[keyboard layout](locale-and-console.md#changing-the-keyboard-layout-of-the-consoles-and-the-desktop)
alone cannot type, Chinese, Japanese and Korean among them, through [fcitx5]{.idx}: which engines
ship, how KWin starts them, the files and the settings page they are configured through, switching
between them, and what to check when typing comes out as Latin. How the engine, KWin and the
application are wired together is in
[Input methods](../../kdos/03-architecture/session.md#input-methods).

## Installation

| Port | Provides |
|:--|:--|
| [fcitx5](#port-fcitx5) | The engine framework, its candidate window, `fcitx5-remote`, `fcitx5-diagnose`, and the English keyboard engine |
| [fcitx5-chinese-addons](#port-fcitx5-chinese-addons), over [libime](#port-libime) | `pinyin`, `shuangpin` and the table methods such as `wbx` (Wubi) and `cangjie` |
| [fcitx5-anthy](#port-fcitx5-anthy), over [anthy-unicode](#port-anthy-unicode) | `anthy`, for Japanese |
| [fcitx5-hangul](#port-fcitx5-hangul), over [libhangul](#port-libhangul) | `hangul`, for Korean |
| [fcitx5-qt](#port-fcitx5-qt) | fcitx5's Qt 6 module, for Qt 6 programs that do not use the Wayland text-input route |
| [fcitx5-gtk](#port-fcitx5-gtk) | fcitx5's GTK 3 and GTK 4 modules, which reach fcitx5 over D-Bus: the route of every GTK 3 program and of a GTK program under Xwayland |
| [fcitx5-configtool](#port-fcitx5-configtool) | System Settings → **Input Method**, and `fcitx5-config-qt` |
| [kwin](#port-kwin) | Starts fcitx5 for the session and relays it to applications over `text-input` |
| [plasma-keyboard](#port-plasma-keyboard) | The on-screen keyboard, the other input method KWin can start |

All of them are installed by `50_desktop`, so every image has them, live and installed. There is
no service script and no autostart entry: KWin starts fcitx5 as the session's input method and
restarts it if it dies, from `fs/usr/share/kdos/xdg/kwinrc`:

<!-- kfs:include fs/usr/share/kdos/xdg/kwinrc lines=45-49 pin=86dcb86db396 -->

The session wrapper clears `QT_IM_MODULE` and `GTK_IM_MODULE`, so each toolkit picks its route
itself: Qt 6 and GTK 4 reach fcitx5 through KWin's `text-input`, a GTK 3 program and a GTK
program under Xwayland through fcitx5-gtk's module over the session bus. Why each toolkit picks
what it does is in [Input methods](../../kdos/03-architecture/session.md#input-methods).

## Configuration

### ~/.config/fcitx5/profile

Which engines a key can reach. Every new account gets the shipped copy,
`fs/etc/skel/.config/fcitx5/profile`: one group whose first entry is the keyboard layout and whose
others are the engines the image installs:

<!-- kfs:include fs/etc/skel/.config/fcitx5/profile -->

The first item is the *inactive* state, plain typing in the layout; the others are what turning the
[input method]{.idx} on can reach. The Input Method page edits this file; fcitx5 writes it itself
when it saves its state.

### /etc/xdg/fcitx5/config

fcitx5's global options, its hotkeys among them. fcitx5 reads one file: `~/.config/fcitx5/config`
when it exists, else this one. KDOS moves group switching off `Meta+Space`, which opens KRunner,
from `fs/etc/xdg/fcitx5/config`:

<!-- kfs:include fs/etc/xdg/fcitx5/config lines=16-28 pin=d03efcf96f8d -->

Each add-on's options are in `~/.config/fcitx5/conf/<addon>.conf`, such as `conf/pinyin.conf`,
once the Input Method page has saved them.

### The build

fcitx5 is built for [Wayland]{.idx} only, with no X11 frontend and no autostart entry
(`ports/core/input/fcitx5/build.sh`); every flag is explained on its
[package page](#port-fcitx5):

<!-- kfs:include ports/core/input/fcitx5/build.sh lines=15-37 pin=fae713b9465c -->

The GTK modules are built for GTK 3 and GTK 4 alone, and the build writes the list GTK 3 loads a
module through, `/usr/lib/gtk-3.0/3.0.0/immodules.cache`, because GTK 3 ignores a module that list
does not name (`ports/core/input/fcitx5-gtk/build.sh`; the reasons are on its
[package page](#port-fcitx5-gtk)):

<!-- kfs:include ports/core/input/fcitx5-gtk/build.sh lines=14-29 pin=91194328c531 -->

The Chinese engines are built without cloud pinyin, which would send what you type to a remote
service:

<!-- kfs:include ports/core/input/fcitx5-chinese-addons/build.sh lines=20-22 pin=ac3f6438b8e8 -->

### Inside a box

A boxed application reaches fcitx5 through KWin, never directly. `kdos-appbox` sets one variable
for it (`src/system/kdos-appbox/main.c`):

<!-- kfs:include src/system/kdos-appbox/main.c lines=423-435 pin=afbe7df0b3c1 -->

## Usage

### Typing in another language

1. Click into a text field of an application, host or boxed.
2. Press `Ctrl+Space` to turn the input method on. fcitx5 switches from `keyboard-us` to the
   engine used last; the first time, that is the one after it in the profile, `pinyin`.
3. Type. The text being composed is shown in the field, and the numbered candidates appear in
   fcitx5's candidate window beside it; choose a candidate with its number key.
4. Press `Ctrl+Space` again to go back to plain typing.

To reach another engine, press `Ctrl+Space` and keep `Ctrl` held: each further `Space` steps to
the next engine in the group. While an engine is on, the left `Shift` on its own switches to plain
typing and back. The input method's tray icon shows which engine is in use.

### Adding an engine

1. System Settings → **Input Method**.
2. **Add Input Method**, pick the engine, for example **Shuangpin** or **Wubi**, and **Add**.
3. **Apply**. fcitx5 reloads, and the engine is in the group at once.

The same page sets the hotkeys and, through each engine's configure button, its options.

### Switching engine from a prompt

```bash
fcitx5-remote -n             # the engine in use
fcitx5-remote -s anthy       # switch to anthy
fcitx5-remote -s keyboard-us
fcitx5-remote                # 1 inactive, 2 active, 0 no text field has the focus
```

Each of these acts on the text field that has the focus, so run them in Konsole, which is itself
such a field. An engine that is not in the profile's group cannot be switched to.
`fcitx5-remote --check` exits with status 1 and starts nothing when fcitx5 is not running.

### The on-screen keyboard

System Settings → **Keyboard** → **Virtual Keyboard** chooses between fcitx5 and Plasma Keyboard;
only one input method runs at a time. See
[Accessibility](accessibility.md#the-on-screen-keyboard).

### The live medium and an installed system

The engines and the profile are the same on both. On the live medium the home directory is in
memory, so a changed profile lasts until power-off unless a persistence store exists
([kdos persist](../../kdos/04-programs/kdos-command.md#kdos-persist)).

## Tips

- `kdos doctor` says whether fcitx5 is running.
- Never set `GTK_IM_MODULE`, and never set `QT_IM_MODULE=fcitx` in a profile: the session wrapper
  clears both, and a value set later in `plasma-workspace/env` chooses the route for every program
  of that toolkit, boxed ones included, where the D-Bus route does not exist.
- `fcitx5-gtk3-immodule-probing` and `fcitx5-gtk4-immodule-probing`, run in Konsole, print the
  module a GTK 3 or GTK 4 program started there would use: `fcitx` for fcitx5-gtk's, `wayland` for
  KWin's `text-input`.

## Troubleshooting

**`Ctrl+Space` does nothing and every key arrives as Latin.**

1. `pgrep -a fcitx5`. No line means fcitx5 is not running: check that **Fcitx 5** is chosen on
   the Virtual Keyboard page, and `grep -A1 Wayland ~/.config/kwinrc` for an `InputMethod` of your
   own that names something else.
2. `fcitx5-remote -n` while the field has the focus. `keyboard-us` after `Ctrl+Space` means the
   group holds nothing else: add an engine as above.
3. Check the application in another program: if it works in another GTK or Qt program and not in
   this one, the toolkit is the cause.

**A GTK application takes no input method although others do.** Something in its environment
sets `GTK_IM_MODULE`, a wrapper script or a profile of your own. Unset it there. For a GTK 3
program, `grep fcitx5 /usr/lib/gtk-3.0/3.0.0/immodules.cache` must print the module's lines: without
them GTK 3 never loads fcitx5-gtk's module and falls back to plain typing.

**Running fcitx5 in the foreground for its messages.** `fcitx5-diagnose` prints upstream's report
of the environment. KWin restarts fcitx5 when it exits; its messages go to the session log,
`~/.local/share/sddm/wayland-session.log`.

## Known gaps

- [Every program speaks English](../../kdos/06-reference/known-gaps.md#every-program-speaks-english):
  input methods type other languages; no program's menus are translated

## See also

- [Input methods](../../kdos/03-architecture/session.md#input-methods): how KWin starts fcitx5 and
  relays it
- [The environment a box receives](../../kdos/03-architecture/session.md#the-environment-a-box-receives)
- [Accessibility](accessibility.md): the on-screen keyboard and the other aids
- [Locale and console](locale-and-console.md): the keyboard layout the session types in
- [Xwayland and X11 applications](xwayland-and-x11-apps.md)

<!-- book-nav -->
---

*Part XVI. System administration, chapter 439.* Previous: [438. Xwayland and X11 applications](xwayland-and-x11-apps.md) · [Contents](../README.md) · Next: [440. Fonts and theming](fonts-and-theming.md)
