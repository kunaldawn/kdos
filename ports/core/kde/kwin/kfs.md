## About

KWin is the Plasma desktop's compositor and window manager. On KDOS it runs only as
`kwin_wayland`: the Wayland compositor that takes the DRM device and the input devices, places,
decorates and draws every window, runs the effects (Overview, the window switcher, Night Light,
zoom, the screen edges), serves global shortcuts through [kglobalacceld](#port-kglobalacceld), shows
the lock screen of [kscreenlocker](#port-kscreenlocker), and starts [xwayland](#port-xwayland) the
first time an X11 client connects. `startplasma-wayland` runs it under `kwin_wayland_wrapper`,
which holds the Wayland socket and starts KWin again after any exit but a clean one, up to ten
crashes in a row, so clients reconnect to the new compositor; the SDDM greeter runs a second copy
of it for the login screen. Its settings pages (window rules, decorations, effects, virtual
desktops, the task switcher, screen edges, Xwayland) are KCMs that System Settings loads.

KWin implements `wp_security_context_v1`. A client that connects through a socket tagged with it
counts as sandboxed. Upstream refuses it seven globals: window management
(`org_kde_plasma_window_management`), fake input, screencasting, activation feedback, the
lock-screen overlay, data control, and the security-context manager itself; KDOS's
`sandbox-policy.patch` refuses it the desktop-shell globals as well, unless the box's profile
grants them back. Its windows carry the tag's app id in place of their own, and their title bar
shows a box before the caption.

The port also builds `kdos_crt`, the effect behind Phosphor mode, into KWin.

## Flags

- `-D KF_SKIP_PO_PROCESSING=ON` — Compiles none of the translation catalogues. kpkg removes
  every non-English catalogue from the package anyway (see
  [Size and language policy](../../../../docs/kdos/05-developer/writing-ports.md#size-and-language-policy)),
  so this only saves build time.
- `-D KWIN_BUILD_X11=ON` — Builds the X11 window management KWin needs to run Xwayland and manage
  its windows, so an application with no Wayland path, and every Tk window, still opens. It is
  upstream's default, named so that it cannot change unnoticed. It does not build an X11 session:
  there is no `kwin_x11` in this release and no X server but Xwayland (see
  [No Xorg server](../../../../docs/kdos/01-philosophy/principles.md#no-xorg-server-and-one-carve-out)).
  It makes [libxcb](#port-libxcb), [xcb-util-cursor](#port-xcb-util-cursor),
  [xcb-util-keysyms](#port-xcb-util-keysyms), [xcb-util-wm](#port-xcb-util-wm),
  [libX11](#port-libX11) and the X11 half of [libxkbcommon](#port-libxkbcommon) requirements.
- `-D CMAKE_REQUIRE_FIND_PACKAGE_X11_XCB=ON` — Makes `libX11-xcb`, from [libX11](#port-libX11), a
  requirement of the configure. Upstream builds the X11 windowed backend,
  `kwin_wayland --x11-display`, which runs KWin nested in a window of an X server, only when the
  library is found, but `kwin_wayland` refers to that backend whenever `KWIN_BUILD_X11` is on, so
  without the library the configure passes and the link of `kwin_wayland` fails.
- `-D CMAKE_REQUIRE_FIND_PACKAGE_PlasmaActivities=ON` — Activities, through
  [plasma-activities](#port-plasma-activities): window rules and the window switcher can then
  follow the activity a window belongs to. Upstream drops the feature without an error when the
  library is missing.
- `-D CMAKE_REQUIRE_FIND_PACKAGE_Libeis-1.0=ON` — Emulated input through [libei](#port-libei).
  The `eis` plugin serves `org.kde.KWin.EIS.RemoteDesktop` and the input-capture interface, which
  the desktop portal's RemoteDesktop and InputCapture portals, remote-desktop servers and KDE
  Connect use to send pointer and key events. Without the library the plugin is not built and
  those portals have no input.
- `-D CMAKE_REQUIRE_FIND_PACKAGE_QAccessibilityClient6=ON` — Builds the zoom effect's AT-SPI
  trackers of the keyboard focus and of an X11 window's text cursor, through
  [libqaccessibilityclient](#port-libqaccessibilityclient). On Wayland KWin runs them only when
  `KWIN_WAYLAND_ZOOM_FORCE_LEGACY_FOCUS_TRACKING` or
  `KWIN_WAYLAND_ZOOM_FORCE_LEGACY_TEXT_CARET_TRACKING` is set, because their blocking D-Bus calls
  can stall the compositor; the text cursor of a Wayland window, which the shipped `kwinrc` has the
  zoom follow, comes from the input method and needs no library.
- `-D CMAKE_REQUIRE_FIND_PACKAGE_KF6DocTools=ON` — The handbook pages of the KWin settings pages,
  through [kdoctools](#port-kdoctools), which upstream looks for as an optional component.
- `-D CMAKE_REQUIRE_FIND_PACKAGE_Python3=ON` — Makes [python3](#port-python3) a requirement of the
  configure. Every built-in effect's `metadata.json` is stripped at build time by
  `strip-effect-metadata.py`, a Python script run through its `#!/usr/bin/env python3` line.
  Upstream looks for the interpreter only to report it, so without it the configure passes and the
  build stops at the first effect.
- `-Wno-dev` — Silences CMake's warnings meant for the project's own developers.

## Patches

`sandbox-policy.patch` is KDOS's boundary for boxed applications at the compositor. Upstream
refuses a client of a `wp_security_context_v1` socket seven globals and offers it every other one;
the patch also refuses the layer shell, output management and external brightness, DRM leases,
DPMS, the Plasma shell surface, screen edges, the keyboard state and virtual-desktop management,
and lets a box open named sets of them again through the `grant` key of its profile,
`~/.config/kdos/boxes/<box>.conf`: `window-list`, `fake-input`, `screencast`, `data-control`,
`layer-shell`, `output-management`, `dpms` and `plasma-shell` (`input-method` is accepted and opens
nothing, because KWin keeps the input-method globals for the input method it starts). The box is
the context's instance id, which `kdos-boxsock` sets; only the engine `io.kdos.appbox` is read, a
profile is read once per client, and `wp_security_context_manager_v1` and
`kde_lockscreen_overlay_v1` are never granted. The path is `$HOME/.config`, not
`$XDG_CONFIG_HOME`, because `kdos-appbox` writes it there and the two must never disagree. It keeps
the context's sandbox engine and instance id on the client connection, which a grant source needs
and which upstream drops; that half is proposed upstream, the KDOS profile reader is not.

`sandbox-badge.patch` gives a window three constant properties, `sandboxEngine`, `sandboxAppId`
and `sandboxInstanceId`, read from its client's security context, and passes them to the
decoration through KDecoration3's `DecoratedWindowPrivateV5`, so Breeze can draw a box glyph before
a confined window's caption. An X11 window reads as unsandboxed: its surface is Xwayland's. A KDOS
box's engine carries which boundary held, `io.kdos.appbox+private` or `io.kdos.appbox+tagged`, which
is how the glyph tells a confined box from one that could reach past its tag. It is applied after
`sandbox-policy.patch`, whose engine and instance id it reads, and needs the
[kdecoration](#port-kdecoration) and [breeze](#port-breeze) patches of the same name: against an
unpatched kdecoration KWin does not compile. It is proposed upstream with them (it serves Flatpak's
`org.flatpak` engine unchanged).

`kdos-crt.patch` adds `kdos_crt`, the effect behind Phosphor mode, as a built-in effect: it is
compiled into KWin instead of shipped as a plugin, because the loader refuses an out-of-tree effect
built against any other KWin release, point releases included, and a built-in one is a static
plugin it never checks. It paints each output into an offscreen texture and draws it back through
a CRT shader: a three-tap horizontal bleed, scanlines on every third row, a vignette, barrel
curvature, a faint floor in the accent, a 400 ms degauss when the colour scheme's accent changes,
and a collapse to a line and a dot when the session locks or quits. It is off by default, refuses a
software renderer and HDR outputs, gives screenshots, screencasts and the colour picker the plain
picture, stands aside while zoom or the magnifier runs, and with `Fullscreen=false` gives an active
fullscreen window's output back to direct scanout. It is KDOS's own and not upstream; KDOS proposes
instead a generic "custom screen shader" effect that loads a GLSL file and its uniforms, after
which this shader ships as data and the patch is dropped. The effect interface moves between Plasma
minor releases, so the patch is rebased at each one. It shares no file with the two sandbox
patches.

## Notes

The effect's settings are `kwinrc [Effect-kdos_crt]`: `Strength` (0–100, 55), `Scanlines` (0–100,
0), `Curve` (0–100, 0) and `Fullscreen` (false). `kdos phosphor` turns it on with
`[Plugins] kdos_crtEnabled=true` and `org.kde.KWin /Effects loadEffect kdos_crt`. While it runs
the scene is composited on every frame of every output it draws on, the hardware cursor plane and
direct scanout are given up and the pointer is drawn into the picture, so it curves with it. The
power-down is also exported as `io.kdos.Crt1` (`Collapse`, `Unfold`, `Degauss`, property `Phase`)
at `/io/kdos/Crt1` on `org.kde.KWin`, and `qdbus org.kde.KWin /Effects debug kdos_crt` prints its
state and each output's verdict. `KWIN_KDOS_CRT_DUMP=<prefix>` in KWin's environment writes the
pass's input and output once as `<prefix>-in.png` and `<prefix>-out.png`
(`KWIN_KDOS_CRT_DUMP_FRAME=<n>` waits for the n-th passed frame); it is read only from KWin's own
environment, so no client can use it to capture the screen. `KWIN_EFFECTS_FORCE_ANIMATIONS=1`
lets it load on a software renderer, for a test only. The settings page,
`kwin_kdos_crt_config`, is built because `KWIN_BUILD_KCMS` is on, upstream's default.

Two features hang on a `pkg_check_modules` probe, which no CMake switch can make required:
screencasting (the `screencast` plugin that streams outputs and windows to the portal over
[pipewire](#port-pipewire)) and game controllers (the `gamecontroller` plugin, off until it is
enabled in the settings, which reads controllers through [libevdev](#port-libevdev), counts their
use as user activity so the screen does not idle, and can drive the pointer and keys with one).
After the configure, `build.sh` reads `PipeWire_FOUND` and `libevdev_FOUND` from `CMakeCache.txt`
and stops the build if either probe failed.

A third probe, for a `libsystemd.pc`, has no switch either. When one is found, KWin links it for a
watchdog that answers only a systemd service manager (`WATCHDOG_USEC`) and for a check that counts
a process in a Flatpak or Snap unit under `app.slice` as sandboxed. [elogind](#port-elogind)
installs its library under its own name, `libelogind.pc`, so on KDOS both are compiled out, the
package links no session library, and only the security-context tag marks a client as sandboxed.
An elogind that also installs `libsystemd.pc` makes KWin link `libelogind`, and elogind then
belongs in `depends`. The tag filters only what reaches the tagged socket: a box's boundary is its
private runtime directory, which holds no `wayland-0`, and its filtered bus. A box granted
`fake-input` or `window-list` can act on the whole desktop.

The X11 windowed backend reads pointer motion through the X Input extension when CMake's `FindX11`
finds [libXi](#port-libXi) at configure time, and polls the pointer otherwise. The build reads only
the headers from it, so it is in `depends` for the build alone.

The install runs `setcap CAP_SYS_NICE=+ep` on `/usr/bin/kwin_wayland`, and `find_package(Libcap)`
is required for that, so [libcap](#port-libcap), which provides `setcap`, is in `depends`. kpkg's
package tar records no file capabilities, so the installed binary has none. KWin asks for
`SCHED_RR` at the lowest real-time priority for its main thread, the DRM commit thread and the
input thread, which needs either the capability or an `RLIMIT_RTPRIO` of at least 1 for the user.
With no capability, that limit is the one `pam_limits` sets at login from
`/etc/security/limits.d`; without it KWin logs `Failed to gain real time thread priority` and
composites at normal priority.

The install puts a systemd user unit, `plasma-kwin_wayland.service`, under `/usr/lib/systemd`;
`build.sh` removes the directory, since KDOS has no systemd (see
[No systemd](../../../../docs/kdos/01-philosophy/principles.md#no-systemd-init-units-or-service-manager)).
`KF_SKIP_PO_PROCESSING` stops only `ki18n_install()`: `kdoctools_install()` still builds the
handbook in every translated language, and kpkg's language rule removes
`/usr/share/doc/HTML/<lang>/` from the package.

CMake records several packages as run-time only and builds the same without them; each is in
`depends` because the session needs it. KWin loads the configured decoration, then
`org.kde.breeze` from [breeze](#port-breeze) when that fails, then `org.kde.kwin.aurorae` from
[aurorae](#port-aurorae), so a window keeps a frame while any of the three loads.
[xwayland](#port-xwayland) is found on `PATH` when an X11 client connects, and
[hwdata](#port-hwdata)'s `pnp.ids` names a monitor's manufacturer. The QML of the effects and
pages imports [kirigami](#port-kirigami), [libplasma](#port-libplasma) and [ksvg](#port-ksvg),
`Qt5Compat.GraphicalEffects` from [qt6-qt5compat](#port-qt6-qt5compat) in Overview, the window
heap and the tile editor, `org.kde.milou` from [milou](#port-milou) for Overview's search,
`org.kde.kitemmodels` from [kitemmodels](#port-kitemmodels) in the tile editor and the window
rules page, and `org.kde.quickcharts` from [kquickcharts](#port-kquickcharts) in the Show FPS
effect.

When Xwayland starts, KWin runs every file in `/etc/xdg/Xwayland-session.d` with `DISPLAY` set to
it, in name order, and then plasma-workspace's `/usr/lib/libexec/plasma-setup-xwayland`; it takes
the X11 window-manager selections only once all have finished.

The decoration, effects and scripts pages carry "Get New…" buttons from
[knewstuff](#port-knewstuff), which is required whenever the settings pages are built. The `ghns`
Kiosk restriction in the session's `kdeglobals` disables them, since nothing is downloaded at run
time.

None of the patches is for musl: nothing in this release needs one. `SCHED_RESET_ON_FORK`, which the real-time request
is compiled behind, is defined in musl's `<sched.h>` without `_GNU_SOURCE`; `memfd_create`, which
KWin's shared-memory buffers use, is in musl; and the `mallopt(M_TRIM_THRESHOLD)` tuning is
compiled only where the C library defines `M_TRIM_THRESHOLD`, which musl does not.
