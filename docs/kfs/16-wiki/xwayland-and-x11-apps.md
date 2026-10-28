# Xwayland and X11 applications

KDOS has no Xorg server. Programs that can only speak the X11 protocol run on
[Xwayland]{.idx}, an X server that is itself a client of the compositor and draws each X11 window
as a window of the desktop. This page covers how Xwayland is started, which settings decide
whether a program uses it, how to run an X11 program on purpose from a terminal, from another
console or inside a box, how to check that X11 and its OpenGL work, and what to look at when an
X11 program does not open. Why X is allowed only in this form is explained in
[No Xorg server, and one carve-out](../../kdos/01-philosophy/principles.md#no-xorg-server-and-one-carve-out),
and how the compositor runs it in [kdos-comp](../../kdos/04-programs/kdos-comp.md#xwayland).

## Installation

Xwayland is part of every image that has the desktop, and it is not a service:

- [xwayland](#port-xwayland) installs `/usr/bin/Xwayland`. Its recipe builds no other X server.
- [wlroots](#port-wlroots) is built with `-Dxwayland=enabled`, and [kdos-comp](#port-kdos-comp)
  with the same switch; the compositor starts Xwayland through wlroots.
- [mesa](#port-mesa) is built with the X11 platform and GLX behind [libglvnd](#port-libglvnd), so
  an X11 client gets OpenGL.
- [xkbcomp](#port-xkbcomp) compiles the keymaps Xwayland asks for, from
  [xkeyboard-config](#port-xkeyboard-config)'s data.
- [font-misc-misc](#port-font-misc-misc), [font-cursor-misc](#port-font-cursor-misc) and
  [font-adobe-75dpi](#port-font-adobe-75dpi) are the core X fonts, the ones a Motif or Xt program
  asks the server for by name.
- The X client libraries, `libX11`, `libxcb` and the rest, are ports on the `x11` shelf and are
  linked by any application that needs them.

The compositor creates the X display when it starts and sets `DISPLAY` for the programs it starts.
Xwayland itself starts when the first X11 client connects. wlroots starts it with `-terminate 10`,
so it exits ten seconds after the last client disconnects; the display keeps its name, and the next
client starts it again. Nothing is enabled by hand, and `fs/etc/X11/` does not exist.

The live medium and an installed system behave the same. Every procedure below except the OpenGL
check has been carried out on the live medium; that one needs a box built from the network.

## Configuration

### The Xwayland build

<!-- kfs:include ports/core/x11/xwayland/build.sh lines=12-30 pin=6112a6b18978 -->

What each switch does is on the [xwayland](#port-xwayland) package page. The keymaps it compiles
at run time are written to `/var/lib/xkb`.

### The compositor

<!-- kfs:include src/desktop/kdos-comp/build.sh lines=21-23 pin=872fbb2551ab -->

`rc.xml` sets nothing about Xwayland, so the compositor's defaults apply: started on demand,
stopped when idle. labwc's `<core><xwaylandPersistence>yes</xwaylandPersistence></core>` keeps it
running for the whole session. A script at `~/.config/kdos-comp/xinitrc`, which the image does not
ship, is run with `sh` each time Xwayland becomes ready.

### Which toolkits choose X11

Every login shell exports the toolkits' back ends, and [Wayland]{.idx} comes first in each that has one:

<!-- kfs:include fs/etc/profile.d/10-wayland.sh lines=78-91 pin=7ff578216c09 -->

- **GTK** tries Wayland first on its own; `GDK_BACKEND` is left unset, so a program that sets
  `GDK_BACKEND=x11` for itself gets Xwayland.
- **Qt** takes the list `wayland;xcb` and uses X11 only when its Wayland plugin cannot start.
- **SDL** is built with its Wayland driver alone, so `SDL_VIDEODRIVER` names no fallback.
- **Clutter** and **Firefox** are told to use Wayland.
- **Tk** and **Motif** have only an X11 back end, so every Tk or Motif program runs on Xwayland.
- **Java** Swing and AWT run on Xwayland; `_JAVA_AWT_WM_NONREPARENTING=1` is what makes them draw.
  The compositor sets the same default for the programs it starts.

A port whose program misbehaves on Wayland can force X11 in its own [desktop entry]{.idx}. Brasero's
recipe does:

<!-- kfs:include ports/core/optical/brasero/build.sh lines=40-40 pin=e5ebc99b52b4 -->

### Boxed applications

A box receives no profile environment. [`kdos-appbox`]{.idx} gives it `DISPLAY` itself, from its own
environment or from the first socket in `/tmp/.X11-unix`, which the box shares with the host:

<!-- kfs:include src/system/kdos-appbox/main.c lines=427-441 pin=5b518a0f07ce -->

When `kdos-appbox` writes launchers for a boxed application, it removes from the entry's `Exec`
line the assignments that would force X11, so an application whose distribution entry asks for
X11 runs on Wayland:

<!-- kfs:include src/system/kdos-appbox/launchers.c lines=264-268 pin=9b7f3a80babb -->

## Usage

### Checking that Xwayland is there

1. In a terminal the compositor started (`Super+Return`):

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

3. `kdos doctor` checks for the socket in its Desktop section, and from a shell the compositor did
   not start says so: `Xwayland on :0 (not in this shell's env — the compositor exports DISPLAY
   only to what it spawned)`.

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

A shell the compositor did not start has no `DISPLAY`.

1. Log in as the same account as the session.
2. Read the display number off the socket and use it:

   ```bash
   ls /tmp/.X11-unix/
   DISPLAY=:0 wish
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
  does, is answered by the compositor, so the screen does not dim during playback; see
  [Idle, dim, lock and lid](../../kdos/04-programs/kdos-comp.md#idle-dim-lock-and-lid).
- A file dialog opened through the [portal]{.idx} by an X11 window is not placed over that window: the
  portal drops a parent handle of the `x11:` kind.
- An X11 window from a box still wears its box's chip; the compositor finds its box from the
  client's process rather than from a Wayland security context. See
  [Box identity](../../kdos/04-programs/kdos-comp.md#box-identity).
- The on-screen keyboard in `osk = auto` mode never rises for an X11 window, which does not speak
  the text-input protocol; show it with `Super+Alt+K`. The mode is set in
  `~/.config/kdos/comp.conf`; see [The on-screen keyboard](accessibility.md#the-on-screen-keyboard).
- `kdos why /usr/bin/Xwayland` names the package and recipe it came from.

## Troubleshooting

**An X11 program exits saying it cannot open or connect to the display.** Its shell has no
`DISPLAY`. Inside the session, start it from a terminal the compositor started, or set `DISPLAY`
from the socket name as in
[Running an X11 program from tty2 or SSH](#running-an-x11-program-from-tty2-or-ssh). In a box,
check that `/tmp/.X11-unix/` holds a socket; `kdos-appbox` sets nothing when there is none.

**There is no socket in `/tmp/.X11-unix/`.** The compositor could not create the X display.
Look in its log:

```bash
grep -i xwayland "$XDG_RUNTIME_DIR/kdos-comp.log"
```

The first session after a boot creates `/tmp/.X11-unix` and logs
`Created /tmp/.X11-unix ourselves -- other users will be unable to create X11 UNIX sockets of their own`
at start-up; a later session finds the directory there and says nothing about it.
`failed to create xwayland server, continuing without` means the session runs with no X11 at
all; when it follows `Cannot find Xwayland binary "/usr/bin/Xwayland"`, the package is missing.
Check that it is installed with `kpkg info xwayland`; the display is created when the compositor
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

**Typing in an X11 program ignores the input method.** X11 programs have no input-method route;
see [Known gaps](#known-gaps).

**A program that has a Wayland back end runs on X11 anyway.** Check its environment for a forcing
variable (`GDK_BACKEND=x11`, `QT_QPA_PLATFORM=xcb`, `SDL_VIDEODRIVER=x11`) and its desktop entry's
`Exec` line for an `env` prefix:

```bash
grep -l 'GDK_BACKEND=x11\|QT_QPA_PLATFORM=xcb' /usr/share/applications/*.desktop ~/.local/share/applications/*.desktop
```

## Known gaps

- [X11 and Qt 5 applications get no input method](../../kdos/06-reference/known-gaps.md#x11-and-qt-5-applications-get-no-input-method)
- [The native applications have not been run](../../kdos/06-reference/known-gaps.md#the-native-applications-have-not-been-run)
- [Drag and drop carries text and files, and nothing else](../../kdos/06-reference/known-gaps.md#drag-and-drop-carries-text-and-files-and-nothing-else)
- [A screen reader reads the applications, not the desktop's own windows](../../kdos/06-reference/known-gaps.md#a-screen-reader-reads-the-applications-not-the-desktops-own-windows): Orca's mouse review needs Wnck, which is X11-only and not a port

## See also

- [No Xorg server, and one carve-out](../../kdos/01-philosophy/principles.md#no-xorg-server-and-one-carve-out): what the carve-out allows and what it keeps out
- [Xwayland](../../kdos/04-programs/kdos-comp.md#xwayland): how the compositor runs it
- [Input methods](../../kdos/03-architecture/session.md#input-methods): why an X11 window gets none
- [The environment a box receives](../../kdos/03-architecture/session.md#the-environment-a-box-receives): `DISPLAY` and the rest inside a box
- [Desktop GL and GLX](../../kdos/05-developer/testing.md#desktop-gl-and-glx): what an X11 client draws with
- [Session and environment](session-and-environment.md), [Input methods](input-methods.md), [Boxes](boxes.md),
  [Graphics drivers](graphics-drivers.md), [Virtual machines](virtual-machines.md)

<!-- book-nav -->
---

*Part XVI. System administration, chapter 414.* Previous: [413. The panel and the launcher](panel-and-launcher.md) · [Contents](../README.md) · Next: [415. Input methods](input-methods.md)
