## About
[Remmina]{.idx} is a GTK 3 remote desktop client. Each protocol is a plugin: RDP through
[FreeRDP](#port-freerdp) 3, VNC through [libvncserver](#port-libvncserver)'s client library,
SPICE through [spice-gtk](#port-spice-gtk), and SSH and SFTP through [libssh](#port-libssh)
with a [VTE](#port-vte3) terminal. Saved passwords go through libsecret.

## Flags
- `-lintl` — Added to `LDFLAGS`. Remmina calls gettext, which musl leaves to libintl, and its
  link lines never name the library.
- `-D WITH_FREERDP3=ON` — Build the RDP plugin against FreeRDP 3, the major version of the port.
  Upstream's default is FreeRDP 2.
- `-D WITH_NEWS=OFF` — With `-D WITH_STATS=OFF` and `-D WITH_TIP=OFF`: no news check, no usage
  statistics sent and no tip of the day, each of which reaches remmina.org.
- `-D WITH_WWW=OFF` — No WWW plugin, which opens a web page as a connection through WebKitGTK.
  Upstream's default is on.
- `-D WITH_KF5WALLET=OFF` — With `-D WITH_GVNC=OFF`, `-D WITH_X2GO=OFF`, `-D WITH_NX=OFF`,
  `-D WITH_XDMCP=OFF`, `-D WITH_ST=OFF` and `-D WITH_PYTHONLIBS=OFF`: none of the optional
  plugins for KWallet, the gtk-vnc VNC client, X2Go, NX, XDMCP, the Simple Terminal, or Python
  plugins. The libvncserver plugin is the VNC client here.
- `-D WITH_VTE=ON` — With `-D WITH_MANPAGES=ON`: the VTE terminal for SSH connections, and the
  manual page. Upstream's defaults, named.
- `-D WITH_TRANSLATIONS=OFF` — Build no message catalogues.
- `-D WITH_KIOSK_SESSION=OFF` — Install no kiosk session files: an X session entry under
  `xsessions` and a GNOME session that run Remmina alone. Upstream's default, named; no display
  manager or GNOME session runs here.
- `-D WITH_ICON_CACHE=OFF` — With `-D WITH_UPDATE_DESKTOP_DB=OFF`: do not write the icon cache and
  the desktop database into the package; kpkg regenerates both on install.
- `-D HAVE_LIBAPPINDICATOR=ON` — The tray icon, an Ayatana AppIndicator through
  [libayatana-appindicator](#port-libayatana-appindicator), which the panel's tray hosts as a
  StatusNotifierItem. Upstream's default, named; Remmina searches for the library as required.
- `-D WITH_AVAHI=OFF` — No host discovery. It needs `avahi-ui-gtk3`, which the
  [avahi](#port-avahi) port does not build. This switch, not `CMAKE_DISABLE_FIND_PACKAGE`, is what
  turns it off, because Remmina searches for every suggested package as required.
- `-D CMAKE_REQUIRE_FIND_PACKAGE_WinPR=ON` — and the same for `FreeRDP`, `FreeRDP-Client`,
  `SharedMimeInfo`, `LIBSSH`, `LIBVNCSERVER`, `Spice`, `Libsecret`, `VTE`, `GCRYPT` and `Cups`:
  turn each `find_package` into a required one. The RDP plugin's three packages are searched
  without `REQUIRED` and the plugin is dropped silently when FreeRDP is missing; the list makes
  every plugin and feature this recipe expects a configure error when its library is absent.
- `-D VERSION=$version` — With `-D prefix=/usr`, `-D exec_prefix=/usr`, `-D libdir=/usr/lib` and
  `-D includedir=/usr/include`: the values `data/remmina.pc.in` substitutes. Remmina sets none of
  these variables itself, so without them `remmina.pc` comes out with empty paths and no
  version; the `grep` after the install stops the build if `includedir` is not `/usr/include`.

## Patches
- `vnc-link-gnutls.patch` — Adds `gnutls` to the libraries the VNC plugin, `remmina-plugin-vnc`, links in `plugins/vnc/CMakeLists.txt`.
