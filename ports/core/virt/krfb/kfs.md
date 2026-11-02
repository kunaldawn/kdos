## About

Krfb shares this desktop over VNC: it runs a VNC server from
[libvncserver](#port-libvncserver) on TCP port 5900, advertises it over DNS-SD with
[kdnssd](#port-kdnssd), and asks before each connection is let in. On Wayland it reads the screen
from a PipeWire stream through [kpipewire](#port-kpipewire), obtained from the desktop portal's
ScreenCast interface, and injects the remote pointer and keys through the portal's RemoteDesktop
interface; unattended-access passwords are kept in [kwallet](#port-kwallet).
`krfb-virtualmonitor` serves a second, virtual screen to a VNC viewer such as a tablet, through
KWin's screencast protocol.

## Flags

- `-D DISABLE_PIPEWIRE=OFF` — Build the PipeWire framebuffer and the portal input plugin, the only
  way Krfb can see and drive a Wayland desktop. Upstream's default; named so a changed default
  cannot drop them.
- `-D CMAKE_REQUIRE_FIND_PACKAGE_PlasmaWaylandProtocols=ON` — With
  `-D CMAKE_REQUIRE_FIND_PACKAGE_Qt6WaylandClient=ON`: the PipeWire framebuffer plugin compiles
  the client side of `zkde_screencast_unstable_v1` from
  [plasma-wayland-protocols](#port-plasma-wayland-protocols) with Qt's Wayland scanner from
  [qt6-qtwayland](#port-qt6-qtwayland). Upstream looks for both quietly and still adds the plugin,
  which then cannot build; required, a missing one stops the configure.
- `-D KF_SKIP_PO_PROCESSING=ON` — Skips building the interface translations, which the
  package-wide English-only rule would remove anyway (see
  [Size and language policy](../../../../docs/kdos/05-developer/writing-ports.md#size-and-language-policy)).

## Notes

`find_package(X11 REQUIRED)` and the XCB components make the X client libraries requirements
whatever the session. The XCB framebuffer and the XTest input plugin are built from them and can
capture and drive only an X server, which on KDOS is Xwayland: they share an X client's view, never
the Plasma desktop. The XTest plugin links [libXtst](#port-libXtst) unconditionally, so it is in
`depends`.

The ScreenCast and RemoteDesktop portals are answered by xdg-desktop-portal-kde in the Plasma
session; elsewhere Krfb starts and shares nothing. A machine reached over the network needs the
firewall's VNC service opened.
