## About
[Qt 5]{.idx} is the previous major release of the Qt toolkit, kept for the ports that build
against Qt 5 only. `qt5-qtbase` is its first module: QtCore, QtGui, QtWidgets, QtNetwork,
QtSql, QtDBus, QtOpenGL and QtPrintSupport, the build tools, and the X11 platform plugin. The
Wayland plugin is [qt5-qtwayland](#port-qt5-qtwayland). Qt 5 installs everything under `qt5`
names, `/usr/lib/qt5`, `/usr/include/qt5` and `/usr/share/qt5`, so it sits beside
[qt6-qtbase](#port-qt6-qtbase) without sharing a path.

Qt 5's QtNetwork does not compile against OpenSSL 4, the tree's `openssl`, so this port builds and
links against the OpenSSL 3 runtime that [openssl3](#port-openssl3) installs (see
[A second version beside the first](../../../../docs/kdos/05-developer/writing-ports.md#a-second-version-beside-the-first)).

## Flags
- `./config` — in the OpenSSL source unpacked from the recipe's second source, followed by
  `make build_generated`: generate OpenSSL 3's headers, `opensslv.h` and `configuration.h` among
  them, with the same configuration [openssl3](#port-openssl3) is built with, and build nothing
  else. `openssl3` installs no headers, and the tree's own headers are OpenSSL 4's.
- `CPATH` — set to that OpenSSL's `include` directory, so the compiler finds OpenSSL 3's headers
  ahead of the OpenSSL 4 headers in `/usr/include`.
- `OPENSSL_LIBS` — `/usr/lib/libssl.so.3 /usr/lib/libcrypto.so.3`, by path: link QtNetwork to the
  OpenSSL 3 runtime. `-lssl` would find OpenSSL 4's `libssl.so`. Nothing else a Qt 5 process loads
  may bring OpenSSL 4 into it, because musl's loader has one symbol namespace and the two
  `libcrypto` sonames would resolve each other's calls.
- `-openssl-linked` — with `-dtls` and `-ocsp`: link OpenSSL at build time rather than load it at
  run time, with DTLS and OCSP stapling in QtNetwork.
- `-no-feature-gssapi` — no SPNEGO or Kerberos authentication in QtNetwork: [krb5](#port-krb5)
  links OpenSSL 4.
- `-no-sql-psql` — no PostgreSQL driver: `libpq` links OpenSSL 4. With `-no-sql-mysql`,
  `-no-sql-odbc`, `-no-sql-tds`, `-no-sql-db2`, `-no-sql-ibase`, `-no-sql-oci` and
  `-no-sql-sqlite2`, SQLite (`-sql-sqlite -system-sqlite`) is the one SQL driver.
- `-confirm-license` — with `-opensource`: accept the open-source licence without the interactive
  question.
- `-optimized-qmake` — upstream's other name for `-optimized-tools`: build `qmake` and the other
  host tools optimised even in a debug build, which upstream does not do by default.
- `-archdatadir /usr/lib/qt5` — with `-bindir`, `-libexecdir`, `-plugindir`, `-importdir`,
  `-qmldir`, `-headerdir`, `-datadir`, `-translationdir`, `-docdir` and `-examplesdir`: every
  Qt 5 path under a `qt5` directory. `-sysconfdir /etc/xdg` is where `QSettings` looks for
  system-wide settings.
- `-no-rpath` — no `RPATH` in Qt's libraries and tools, and none added by `qmake` to what it
  links; every Qt library is in `/usr/lib`.
- `-no-pch` — no precompiled headers.
- `-no-reduce-relocations` — no `-Bsymbolic-functions` link of the Qt libraries. With it on, Qt's
  headers refuse to compile a program that is not built with `-fPIC`.
- `-no-separate-debug-info` — no split-off debug files. Upstream's default, named.
- `-no-mimetype-database` — leave out Qt's built-in copy of the freedesktop.org MIME database and
  read the system one, which [shared-mime-info](#port-shared-mime-info) installs.
- `-dbus-linked` — link QtDBus to libdbus at build time, rather than loading it at run time.
- `-system-doubleconversion` — with `-system-pcre`, `-system-zlib`, `-system-freetype`,
  `-system-harfbuzz`, `-system-libpng`, `-system-libjpeg` and `-system-sqlite`: the tree's
  libraries rather than the copies in Qt's source.
- `-qt-libmd4c` — the copy of md4c, the Markdown parser, in Qt's source, since md4c is not a port.
- `-no-journald` — with `-no-syslog`: Qt's log messages go to standard error, not to the systemd
  journal, which KDOS does not have, or to syslog.
- `-no-libproxy` — no proxy lookup through libproxy, which is not a port; `-system-proxies` keeps
  the proxy settings read from the environment, upstream's default.
- `-no-sctp` — no SCTP sockets. Upstream's default, named.
- `-opengl desktop` — with `-egl` and `-vulkan`: desktop OpenGL, EGL and Vulkan.
- `-xcb` — with `-xcb-xlib`, `-no-bundled-xcb-xinput`, `-xkbcommon`, `-feature-xkbcommon-x11`,
  `-feature-xcb-glx-plugin` and `-feature-xcb-egl-plugin`: the X11 platform plugin with the
  system's XInput, keyboard handling through xkbcommon, and both its GLX and EGL OpenGL
  integrations, each named so that a missing library stops the configure.
- `-no-feature-xcb-sm` — no X session management through libSM; libSM is not in `depends`.
- `-no-eglfs` — with `-no-gbm`, `-no-kms`, `-no-linuxfb`, `-no-directfb` and `-no-feature-vnc`:
  none of the platform plugins that draw without a window system or serve the screen over VNC, as
  for Qt 6 (see
  [How Qt 6 is built](../../../../docs/kdos/05-developer/writing-ports.md#how-qt-6-is-built)).
- `-libudev` — with `-evdev`, `-libinput` and `-mtdev`: input device support, named; `-no-tslib`
  leaves out tslib, which is not a port.
- `-feature-accessibility-atspi-bridge` — the AT-SPI bridge, without which no Qt 5 program is
  visible to a screen reader.
- `-no-gtk` — no GTK 3 platform theme. Qt 5 applications take the desktop's colours from the
  qt5ct files `kdos theme` writes.
- `EXTRA_LFLAGS="$LDFLAGS"` — on the `make` line: the tree's link flags for the bootstrap build of
  `qmake`, whose makefile reads this variable and leaves it undefined; without it `qmake` would
  be linked without `BIND_NOW`.

## Patches
- `qt5-base-cflags.patch` — makes the GCC mkspecs build their release compile and link flags from the environment's `CPPFLAGS`, `CFLAGS`, `CXXFLAGS` and `LDFLAGS`, and their debug flags from `DEBUG_CFLAGS` and `DEBUG_CXXFLAGS`, when those are set, falling back to upstream's optimisation flag and `-Wl,-O1` only when they are empty. Qt, and every `qmake` project built in the tree's environment, then compile with the tree's flags.
- `lfs64.patch` — makes Qt's POSIX platform header map its file calls to the plain names, `stat`, `open`, `lseek`, `mmap` and the rest, rather than their `*64` forms. musl's `off_t` is 64 bits already, and musl declares the `*64` names only under `_LARGEFILE64_SOURCE`, so code that includes the header would otherwise fail to compile.
- `egl-x11.patch` — replaces the body of the configure test for EGL on X11, which opens an X display and creates a window, with `return 0;`. The test then passes whenever `EGL/egl.h` and `X11/Xlib.h` compile and link together, and the X11 plugin is built with its EGL integration.
- `qt-musl-iconv-no-bom.patch` — makes QtCore's iconv codec treat a Linux system without glibc as it treats FreeBSD: name the byte order in the UTF-16 encoding, `UTF-16LE` here, rather than hand iconv a byte order mark to read it from.

## Notes
After the install, `build.sh` links every tool in `/usr/lib/qt5/bin` into `/usr/bin` with a
`-qt5` suffix, such as `moc-qt5` and `uic-qt5`, and links `qmake` itself under its plain name as
well, since Qt 6 installs only `qmake6`.

`qmake`'s bootstrap makefile puts `-g` after every compile flag and reads none of the tree's
`CFLAGS`, so `build.sh` runs `strip --strip-debug` on the installed `qmake` and fails the build if
it still carries `.debug_info`.
