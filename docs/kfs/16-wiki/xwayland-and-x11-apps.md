# Xwayland and X11 applications

KDOS has no Xorg server. Programs that can only speak the X11 protocol run on
[Xwayland]{.idx}, an X server that is itself a client of KWin and draws each X11 window as a window
of the desktop. This page covers how Xwayland is started, which settings decide
whether a program uses it, how to run an X11 program on purpose from a terminal, from another
console or inside a box, how to check that X11 and its OpenGL work, and what to look at when an
X11 program does not open. Why X is allowed only in this form is explained in
[No Xorg server, and one carve-out](../../kdos/01-philosophy/principles.md#no-xorg-server-and-one-carve-out),
and how KWin runs it in [KWin](../../kdos/04-programs/kwin.md).

## Installation

Xwayland is part of every image that has the desktop, and it is not a service:

- [xwayland](#port-xwayland) installs `/usr/bin/Xwayland`. Its recipe builds no other X server.
- [kwin](#port-kwin) is built with its X11 window management, which it needs to run Xwayland and
  manage X11 windows, and starts Xwayland itself.
- [mesa](#port-mesa) is built with the X11 platform and GLX behind [libglvnd](#port-libglvnd), so
  an X11 client gets OpenGL.
- [xkbcomp](#port-xkbcomp) compiles the keymaps Xwayland asks for, from
  [xkeyboard-config](#port-xkeyboard-config)'s data.
- [font-misc-misc](#port-font-misc-misc), [font-cursor-misc](#port-font-cursor-misc) and
  [font-adobe-75dpi](#port-font-adobe-75dpi) are the core X fonts, the ones a Motif or Xt program
  asks the server for by name.
- The X client libraries, `libX11`, `libxcb` and the rest, are ports on the `x11` shelf and are
  linked by any application that needs them.

The session's KWin creates the X display when it starts and puts `DISPLAY` and `XAUTHORITY` into
the session's environment, so every program the session starts has them. Xwayland itself starts
when the first X11 client connects; KWin then runs plasma-workspace's
`/usr/lib/libexec/plasma-setup-xwayland` with `DISPLAY` set, and takes the X11 window-manager
selections. The greeter's KWin starts no X server at all. Nothing is enabled by hand, and
`fs/etc/X11/` does not exist.

The live medium and an installed system behave the same.

## Configuration

### The Xwayland build

<!-- kfs:include ports/core/x11/xwayland/build.sh lines=12-30 pin=6112a6b18978 -->

What each switch does is on the [xwayland](#port-xwayland) package page. The keymaps it compiles
at run time are written to `/var/lib/xkb`.

### KWin's settings for X11 programs

Two pages of System Settings change how KWin treats X11 programs:

- **Display Configuration** → **Legacy Applications (X11)**: whether X11 programs are scaled by
  KWin, which blurs them on a scaled screen, or apply the scaling themselves.
- **Application Permissions** → **Legacy X11 App Support**: whether an X11 program may read the
  keys typed into Wayland windows, never, only with a modifier, or always. Programs that rely on
  global key grabs, such as some push-to-talk tools, need it.

### Which toolkits choose X11

Every login shell exports the toolkits' back ends, and [Wayland]{.idx} comes first in each that has one:

<!-- kfs:include fs/etc/profile.d/10-wayland.sh lines=78-89 pin=a3a54207979f -->

- **GTK** tries Wayland first on its own; `GDK_BACKEND` is left unset, so a program that sets
  `GDK_BACKEND=x11` for itself gets Xwayland.
- **Qt** takes the list `wayland;xcb` and uses X11 only when its Wayland plugin cannot start.
- **SDL** is built with its Wayland driver alone, so `SDL_VIDEODRIVER` names no fallback.
- **Clutter** and **Firefox** are told to use Wayland.
- **Tk** and **Motif** have only an X11 back end, so every Tk or Motif program runs on Xwayland.
- **Java** Swing and AWT run on Xwayland.

A port whose program misbehaves on Wayland can force X11 in its own [desktop entry]{.idx}. Brasero's
recipe does:

<!-- kfs:include ports/core/optical/brasero/build.sh lines=40-40 pin=e5ebc99b52b4 -->

### Boxed applications

A box receives no profile environment. Xwayland is one client to KWin, so an X11 window in a box
is drawn untagged and nothing it binds is filtered; a box is therefore handed the X11 display only
when its profile's `x11` key says so. `x11 = auto`, the default, hands it to an application whose
catalogue entry or runtime forces an X11 backend, and to a box distrobox made from somebody else's
image; `yes` and `no` decide outright. [`kdos-appbox`]{.idx} then sets `DISPLAY` and the cookie
itself (`src/system/kdos-appbox/main.c`):

<!-- kfs:include src/system/kdos-appbox/main.c lines=503-511 pin=72bbf4f777a6 -->

Every other box is told there is no display.

When `kdos-appbox` writes launchers for a boxed application, it removes from the entry's `Exec`
line the assignments that would force X11, so an application whose distribution entry asks for
X11 runs on Wayland:

<!-- kfs:include src/system/kdos-appbox/launchers.c lines=281-285 pin=9b7f3a80babb -->

## Usage

### Checking that Xwayland is there

1. In Konsole (`Meta+Return`):

   ```bash
   echo "$DISPLAY"
   ls /tmp/.X11-unix/
   ```

   `DISPLAY` names the display, such as `:0`, and `/tmp/.X11-unix/` holds its socket, `X0` for `:0`.

2. See whether the server is running at this moment; with no X11 client it is not:

   ```bash
   pgrep -a Xwayland
   ```

   While a client is connected it prints a line such as
   `Xwayland :0 -rootless -core -terminate 10 -listenfd 20 -listenfd 21 -displayfd 76 -wm 67`.
   Ten seconds after the last client leaves, the process is gone and `X0` is still there.

3. `kdos doctor` checks for the socket in its Desktop section.

### Running a program on X11

1. To run a GTK program on X11 instead of Wayland, for one start:

   ```bash
   GDK_BACKEND=x11 <program>
   ```

2. For a Qt program:

   ```bash
   QT_QPA_PLATFORM=xcb <program>
   ```

3. A Tk program needs nothing; Tk speaks only X11. A test window, which closes when its button is
   pressed:

   ```bash
   echo 'pack [button .b -text "X11 works" -command exit]' | wish
   ```

To make the choice for every start of one program, write your own copy of its desktop entry into
`~/.local/share/applications/` with `env GDK_BACKEND=x11` (or `QT_QPA_PLATFORM=xcb`) at the front
of its `Exec` line; see [The panel and the launcher](panel-and-launcher.md#adding-an-application-to-the-menus).

### Running an X11 program from tty2 or SSH

A shell outside the session has no `DISPLAY`, and Xwayland requires the session's cookie.

1. Log in as the same account as the session.
2. Read the display number off the socket, and the cookie's path from KWin's own environment:

   ```bash
   ls /tmp/.X11-unix/
   tr '\0' '\n' < /proc/$(pgrep -u "$USER" -x kwin_wayland)/environ | grep XAUTHORITY
   DISPLAY=:0 XAUTHORITY=<that path> wish
   ```

### Checking OpenGL through Xwayland

The host's `mesa-demos` port is built for Wayland alone, so `glxinfo` and `glxgears` come from a
box; the host has `es2gears_wayland` and none of the three below. The `app.mesa-utils` catalogue
entry builds Debian's `mesa-utils` and puts the three X11 programs on the host's path. This needs
a network for the first install; without one it stops at `app.mesa-utils: base did not build`:

```bash
kdos app install app.mesa-utils
glxinfo | head -20
glxgears
es2gears_x11
```

`glxinfo` shows the GLX extension, which Xwayland on the host serves, and the renderer, which is
the box's own Mesa: an X11 client in a box draws with its box's libraries. Comparing
`es2gears_x11` with the host's `es2gears_wayland` shows what the X11 path costs; see
[Desktop GL and GLX](../../kdos/05-developer/testing.md#desktop-gl-and-glx).

## Tips

- An X11 video player that blocks the screensaver through `org.freedesktop.ScreenSaver`, as VLC
  does, is answered by PowerDevil, so the screen does not dim during playback.
- An X11 window from a box carries no box tag, so it has no box colour on its title bar and none
  of the box's restrictions at KWin. That is why a box is handed the display only when it needs it.
- The on-screen keyboard never rises by itself for an X11 window, which does not speak the
  text-input protocol; show it from the system tray.
- `kdos why /usr/bin/Xwayland` names the package and recipe it came from.

## Troubleshooting

**An X11 program exits saying it cannot open or connect to the display.** Its shell has no
`DISPLAY` or no cookie. Inside the session, start it from Konsole, or set both as in
[Running an X11 program from tty2 or SSH](#running-an-x11-program-from-tty2-or-ssh). In a box,
the profile's `x11` key decides: `kdos-box profile <box>` shows it, and `x11 = yes` hands the
display over at the box's next start.

**There is no socket in `/tmp/.X11-unix/`.** KWin could not create the X display. Read the session
log for KWin's Xwayland lines:

```bash
grep -i xwayland ~/.local/share/sddm/wayland-session.log
```

Check that the package is installed with `kpkg info xwayland`; the display is created when KWin
starts, so log in again after installing it.

**An X11 program starts and exits at once with a GLX error, or says no GLX extension is
present.** Run `glxinfo` from the box above. When it lists no `GLX` extension, the X server is the
cause: Xwayland serves GLX only when it is built with `-Dglx=true`, as the
[xwayland](#port-xwayland) page explains. When the extension is listed and a host program still
fails, check that the host's OpenGL libraries are installed; see
[Desktop GL and GLX](../../kdos/05-developer/testing.md#desktop-gl-and-glx).

**A Java program draws a blank or misplaced window.** `_JAVA_AWT_WM_NONREPARENTING` is not `1`
in its environment, which happens when it was started through `sudo`, `pkexec` or a shell that
did not read the profile. Set it on the command line:

```bash
_JAVA_AWT_WM_NONREPARENTING=1 <program>
```

**Typing in an X11 program ignores the input method.** An X11 program has an input-method route
only when it is a GTK 3 or GTK 4 program on the host, through fcitx5-gtk's module
([Input methods](input-methods.md)); every other X11 program has none. See
[X11 programs other than GTK ones get no input method](../../kdos/06-reference/known-gaps.md#x11-programs-other-than-gtk-ones-get-no-input-method).

**A program that has a Wayland back end runs on X11 anyway.** Check its environment for a forcing
variable (`GDK_BACKEND=x11`, `QT_QPA_PLATFORM=xcb`, `SDL_VIDEODRIVER=x11`) and its desktop entry's
`Exec` line for an `env` prefix:

```bash
grep -l 'GDK_BACKEND=x11\|QT_QPA_PLATFORM=xcb' /usr/share/applications/*.desktop ~/.local/share/applications/*.desktop
```

## Known gaps

- [The native applications have not been run](../../kdos/06-reference/known-gaps.md#the-native-applications-have-not-been-run)

## See also

- [No Xorg server, and one carve-out](../../kdos/01-philosophy/principles.md#no-xorg-server-and-one-carve-out): what the carve-out allows and what it keeps out
- [Xwayland](../../kdos/04-programs/kwin.md#xwayland): how KWin runs it
- [Input methods](../../kdos/03-architecture/session.md#input-methods): which X11 windows get one
- [The environment a box receives](../../kdos/03-architecture/session.md#the-environment-a-box-receives): `DISPLAY` and the rest inside a box
- [Desktop GL and GLX](../../kdos/05-developer/testing.md#desktop-gl-and-glx): what an X11 client draws with
- [Session and environment](session-and-environment.md), [Input methods](input-methods.md), [Boxes](boxes.md),
  [Graphics drivers](graphics-drivers.md), [Virtual machines](virtual-machines.md)

<!-- book-nav -->
---

*Part XVI. System administration, chapter 438.* Previous: [437. The panel and the launcher](panel-and-launcher.md) · [Contents](../README.md) · Next: [439. Input methods](input-methods.md)
