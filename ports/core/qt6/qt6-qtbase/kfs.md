## About
[Qt 6]{.idx} is the C++ application toolkit that KDE's applications, and many others in the tree,
are written in. `qt6-qtbase` is its first module: QtCore, QtGui, QtWidgets,
QtNetwork, QtSql, QtDBus, QtOpenGL, QtPrintSupport and QtConcurrent, the build tools (`moc`,
`rcc`, `uic`, `qmake6`), and the platform plugins that put a window on a screen. Every other
`qt6-*` port builds on it and inherits its install layout. The desktop itself links no toolkit;
Qt is for applications (see
[Toolkits are for applications, not the desktop](../../../../docs/kdos/01-philosophy/principles.md#toolkits-are-for-applications-not-the-desktop)).

What this recipe decides for the whole Qt 6 set, and why, is in
[How Qt 6 is built](../../../../docs/kdos/05-developer/writing-ports.md#how-qt-6-is-built); the
bullets below are the switch-by-switch reading of it.

## Flags
- `-DINSTALL_BINDIR=lib/qt6/bin` — with the other `INSTALL_` paths: Qt 6's tools in
  `/usr/lib/qt6/bin`, its plugins, QML and mkspecs under `/usr/lib/qt6`, its headers in
  `/usr/include/qt6` and its data in `/usr/share/qt6`, so Qt 5's files sit beside them under
  `qt5` names. Every other Qt 6 module reads these paths from this one.
- `-DINSTALL_PUBLICBINDIR=bin` — link each user-facing tool into `/usr/bin` with a `6` suffix,
  such as `qmake6`, so it does not take a Qt 5 tool's name.
- `-DINSTALL_SYSCONFDIR=/etc/xdg` — where `QSettings` looks for system-wide settings files.
- `-DQT_DISABLE_RPATH=ON` — with `-DFEATURE_rpath=OFF`: no `RPATH` in Qt's own libraries and
  tools, and none added by `qmake6` to what it links. Every Qt library is in `/usr/lib`, so an
  entry would only add a lookup, and the package lint warns on each.
- `-DQT_QPA_PLATFORMS="wayland;xcb"` — the default platform plugin list: Wayland first, and the
  X11 plugin, which runs under Xwayland, only for a program that asks for it or when Wayland cannot
  start. The session sets the same list in `QT_QPA_PLATFORM`.
- `-DINPUT_opengl=desktop` — with `-DFEATURE_opengl_desktop=ON`: desktop OpenGL through
  [libglvnd](#port-libglvnd), rather than OpenGL ES 2.
- `-DINPUT_publicsuffix=qt` — use the copy of the Public Suffix List compiled into QtNetwork, which
  `QNetworkCookieJar` checks a cookie's domain against. Upstream's Linux default also looks for a
  system copy at run time, `publicsuffix/public_suffix_list.dafsa` under the XDG data
  directories, and no port installs one.
- `-DFEATURE_relocatable=OFF` — Qt finds its plugins and data at the fixed paths it was configured
  with, not relative to wherever `libQt6Core` is loaded from.
- `-DFEATURE_reduce_relocations=OFF` — no `-Bsymbolic-functions` link of the Qt libraries. With it
  on, Qt's headers stop the compile of every program that is not built with `-fPIC`, so a PIE
  executable built as `-fPIE` would fail.
- `-DFEATURE_wayland=ON` — with `xcb`, `xlib`, `xcb_xlib`, `xkbcommon`, `xkbcommon_x11`,
  `system_xcb_xinput`, `xcb_glx_plugin`, `xcb_egl_plugin`, `opengl`, `egl`, `egl_x11`, `gbm`, `kms`
  and `vulkan`: the Wayland and X11 platform plugins and their OpenGL, EGL and Vulkan integration,
  each named so that a missing library stops the configure instead of dropping the backend.
- `-DFEATURE_xcb_sm=OFF` — no X session management through libSM in the X11 plugin. Upstream
  builds it whenever libSM is found, and libSM is not in `depends`.
- `-DFEATURE_eglfs=OFF` — with `-DFEATURE_linuxfb=OFF`, `-DFEATURE_vnc=OFF`,
  `-DFEATURE_vkkhrdisplay=OFF` and `-DFEATURE_directfb=OFF`: none of the platform plugins that
  draw without a window system, straight to the card or the framebuffer, or serve the screen over
  VNC. A Qt program started outside the compositor would otherwise take the display and the input
  devices from it.
- `-DFEATURE_tslib=OFF` — no touchscreen input through tslib, which is not a port.
- `-DFEATURE_libinput=ON` — with `mtdev`, `evdev` and `libudev`: Qt's input device support through
  [libinput](#port-libinput), [mtdev](#port-mtdev) and [eudev](#port-eudev); named, so the
  configure stops on a missing library.
- `-DFEATURE_accessibility_atspi_bridge=ON` — the AT-SPI bridge through
  [at-spi2-core](#port-at-spi2-core), without which no Qt program is visible to a screen reader.
- `-DFEATURE_dbus_linked=ON` — link QtDBus to libdbus at build time, rather than loading it with
  `dlopen` at run time.
- `-DFEATURE_openssl_linked=ON` — link QtNetwork to [openssl](#port-openssl) at build time, rather
  than loading `libssl` by name at run time.
- `-DFEATURE_gssapi=ON` — SPNEGO and Kerberos authentication in QtNetwork, through
  [krb5](#port-krb5).
- `-DFEATURE_liburing=ON` — io_uring through [liburing](#port-liburing) for QtCore's asynchronous
  file I/O. Upstream's Linux default, named.
- `-DFEATURE_system_freetype=ON` — and the other `system_` switches set on: link the tree's
  FreeType, HarfBuzz, libpng, libjpeg-turbo, zlib, PCRE2, double-conversion and SQLite rather than
  the copies in Qt's source.
- `-DFEATURE_system_libb2=OFF` — with `-DFEATURE_system_textmarkdownreader=OFF`: the copies of
  BLAKE2 and md4c in Qt's source, since neither library is a port.
- `-DFEATURE_libproxy=OFF` — no proxy lookup through libproxy, which is not a port.
- `-DFEATURE_journald=OFF` — no logging to the systemd journal; KDOS has no systemd (see
  [No systemd](../../../../docs/kdos/01-philosophy/principles.md#no-systemd)).
- `-DFEATURE_gtk3=OFF` — no GTK 3 platform theme. The session themes Qt through
  `QT_QPA_PLATFORMTHEME=kde` instead.
- `-DFEATURE_sctp=OFF` — with `-DFEATURE_jemalloc=OFF` and `-DFEATURE_lttng=OFF`: no SCTP sockets,
  no jemalloc allocator and no LTTng tracepoints, each off by upstream's default.
- `-DFEATURE_sql_sqlite=ON` — with `-DFEATURE_sql_psql=ON`: the SQLite and PostgreSQL drivers of
  QtSql, through [sqlite](#port-sqlite) and [postgresql](#port-postgresql)'s `libpq`. The MySQL,
  ODBC, InterBase, Oracle, DB2 and Mimer drivers are switched off.

## Patches
- `musl-lfs64.patch` — makes Qt's POSIX platform header map its file calls to the plain names, `stat`, `open`, `lseek`, `mmap` and the rest, rather than their `*64` forms. musl's `off_t` is 64 bits already, and musl declares the `*64` names only under `_LARGEFILE64_SOURCE`, so code that includes the header would otherwise fail to compile.
- `musl-dns-resolver.patch` — makes QtNetwork's DNS lookups call `res_mkquery()` and `res_send()`, which musl has, in place of the `res_nmkquery()` and `res_nsend()` that take a resolver state, which it does not, and changes the configure test that decides whether `libresolv` is usable to match. It adds a small header, `resolv_compat.h`, giving `res_ninit()` and `res_nclose()` over musl's global resolver state.
- `wayland-cancel-read.patch` — in the Wayland plugin's event reader thread, when the display is shutting down after the thread has prepared a read, calls `wl_display_cancel_read()` before returning. A prepared read must be followed by a read or a cancel; without one, another thread reading the same display waits for this one forever and the program hangs on exit.
- `no-source-tree.patch` — writes an empty `QT_SOURCE_TREE` into the installed `QtBuildInternalsExtra.cmake`. Upstream records the path of the source tree there, which in a package is the build directory under `/var/cache/kpkg`.

## Notes
`build.sh` ends by checking that no file under `/usr/lib/cmake` in the package names
`/var/cache/kpkg`. A build path that reached the installed CMake files would be read by every Qt
module built afterwards, and would point at a directory that does not exist on the target.

Every other `qt6-*` module but [qt6-qtwebengine](#port-qt6-qtwebengine) is at this port's version
and builds against its private headers, which change from one Qt release to the next, so the set is
moved and rebuilt together.
