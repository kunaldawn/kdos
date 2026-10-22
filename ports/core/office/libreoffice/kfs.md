## About

[LibreOffice]{.idx} is the office suite: Writer, Calc, Impress, Draw, Math and Base, sharing one
runtime under `/usr/lib/libreoffice`. It is an application, so it is one of the programs the
desktop's no-toolkit rule leaves free to use one, and this build carries two front ends: the GTK 3
plugin and the Qt 6 plugin. It is built from three sources: its own, a separate help tarball,
and a vendor bundle of the libraries it builds itself. See
[Toolkits are for applications](https://github.com/kunaldawn/kdos/blob/main/docs/kdos/01-philosophy/principles.md#toolkits-are-for-applications-not-the-desktop).

## Flags

- `container=kdos-chroot` — LibreOffice's top-level Makefile refuses to run as root unless
  `$container` is set. The chroot build runs as root.
- `PYTHON_CFLAGS=$(pkg-config --cflags python3-embed)` — with `PYTHON_LIBS`, exported before
  configure: the compile and link flags for embedding Python in pyuno. Unless both are set,
  configure asks Python's `distutils` for them, which the Python standard library does not
  have.
- `--enable-release-build` — Mark the build as a release rather than a development build. Upstream
  notes that the choice is independent of symbols, debug information and optimisation.
- `--with-vendor=KDOS` — The vendor of the build.
- `--with-external-tar="$SRC_ROOT/externals"`, `--disable-fetch-external` — Find the bundled
  third-party tarballs in `externals/`, which is where the vendor bundle unpacks, and never
  download one: a file missing from the bundle is a build error.
- `--with-help=html` — Build the help as local HTML pages from the help tarball, which unpacks
  as `helpcontent2` inside the source tree, where configure looks for it.
- `--without-java`, `--without-junit` — No Java components, and so none of the parts written in
  Java: the HSQLDB embedded database, the report builder, and the BeanShell and Rhino script
  providers. `--without-junit` drops the JUnit tests, which need Java anyway.
- `--without-doxygen`, `--disable-odk` — No Office Development Kit, the SDK that extensions are
  built against, and no Doxygen run for its C and C++ documentation.
- `--without-lxml` — The `gla11y` accessibility check of the UI files runs without Python's lxml,
  so it reports only widget classes and ids and never builds a local copy of lxml.
- `--without-krb5`, `--without-gssapi` — No Kerberos 5 or GSSAPI authentication in the modules
  that support it. configure enables both when it finds a usable system library.
- `--disable-online-update`, `--disable-extension-update` — No update check for LibreOffice
  itself and no update of installed extensions. Software on KDOS is updated by `kpkg`.
- `--disable-breakpad` — No crash reporter.
- `--disable-libcmis` — No CMIS connectors to remote document stores. libcmis is not a port.
- `--disable-sdremote`, `--disable-sdremote-bluetooth`, `--disable-avahi` — No Impress remote
  control server, neither over the network nor over Bluetooth, and no Avahi advertisement of it.
- `--disable-dconf` — No dconf configuration backend; settings stay in LibreOffice's own registry
  in the user profile.
- `--disable-cpdb` — No Common Print Dialog Backends support; printing goes through CUPS.
  cpdb-libs is not a port.
- `--disable-skia` — Do not build Skia. The GTK 3 and Qt 6 plugins draw through cairo and Qt;
  only the plain X11 backend would use Skia.
- `--disable-eot` — No Embedded OpenType font support. Its library, libeot, is not a port.
- `--disable-coinmp`, `--enable-lpsolve` — Calc's Solver uses lp_solve, built from the bundle, and
  not the CoinMP solver.
- `--disable-firebird-sdbc`, `--disable-mariadb-sdbc`, `--enable-postgresql-sdbc` — Base has no
  embedded database: Firebird is not proven on musl and not a port, and with Java off there is no
  HSQLDB either. It connects to PostgreSQL, LDAP and file sources (dBase, CSV, spreadsheets);
  there is no MariaDB or MySQL driver.
- `--enable-ldap`, `--enable-cups`, `--enable-dbus`, `--enable-gio`, `--enable-randr`,
  `--enable-opengl`, `--enable-gstreamer-1-0` — LDAP address books and directory sources,
  printing through CUPS, the D-Bus features (screensaver control in a presentation, among them),
  GIO file access, RandR in the X11 code, OpenGL support, and media playback through GStreamer.
- `--enable-pdfimport`, `--enable-pdfium` — Open a PDF in Draw for editing, and build the PDFium
  copy from the bundle, which LibreOffice uses to render PDFs and to verify PDF signatures. Without
  PDFium, upstream warns, signature verification is insecure.
- `--enable-python=system` — Scripting and pyuno against the system [python3](#port-python3), not
  a Python built from the bundle.
- `--enable-gtk3`, `--enable-qt6`, `--enable-qt6-multimedia` — The two front ends (VCL plugins),
  with media playback inside the Qt 6 plugin through Qt Multimedia.
- `--disable-gtk4`, `--disable-qt5`, `--disable-kf5`, `--disable-kf6`, `--disable-gtk3-kde5` — No
  GTK 4 plugin, no Qt 5 plugin, no plugins integrating with KDE Frameworks 5 or 6, and no GTK 3
  plugin with KDE file dialogs.
- `--disable-introspection` — Generate no GObject introspection files.
- `--with-tls=nss` — [nss](#port-nss) for TLS and cryptography in LibreOffice's own code, which
  also restricts its use of OpenSSL. NSS is upstream's default.
- `--without-fonts` — Do not install the fonts LibreOffice bundles. The recipe depends on
  [font-liberation](#port-font-liberation), [font-carlito](#port-font-carlito) and
  [font-caladea](#port-font-caladea) instead, the metric-compatible faces documents most often ask
  for.
- `--without-myspell-dicts`, `--with-system-dicts` — Bundle no dictionaries and use the system's.
- `--with-external-dict-dir=/usr/share/hunspell`, `--with-external-hyph-dir=/usr/share/hyphen`,
  `--with-external-thes-dir=/usr/share/mythes` — Where those are: the spelling dictionaries of
  [hunspell-en](#port-hunspell-en), and the hyphenation patterns and thesauri directories of
  [hyphen](#port-hyphen) and [mythes](#port-mythes).
- `--with-system-zlib` — and every other `--with-system-*` flag: link the library the tree builds
  as a port, not the copy in the bundle. Each one is a port LibreOffice accepts at any release.
  `--with-system-zstd=yes` is the same switch with its value spelled out.
- `--without-system-boost` — and every other `--without-system-*` flag: build the library from the
  bundle. Most of these are not ports. Boost, abseil, poppler, mdds and orcus are, but
  LibreOffice pins their C++ APIs to the releases its `download.lst` names, so their bundled
  copies are used. Argon2, colamd and the ODBC headers also come from the bundle, although
  [argon2](#port-argon2), [suitesparse](#port-suitesparse) and [unixodbc](#port-unixodbc) are
  ports.
- `ENVCFLAGS=-D_LARGEFILE64_SOURCE` — with `ENVCFLAGSCXX`, exported after configure: gbuild adds
  both to every library it compiles itself. With `_LARGEFILE64_SOURCE`, musl's headers define
  `stat64`, `fstat64` and `lseek64` as the plain calls, which the Linux configuration of the
  bundled CLucene names; without it the search library of the help indexer does not compile. See
  [Build troubleshooting](https://github.com/kunaldawn/kdos/blob/main/docs/kdos/05-developer/build-troubleshooting.md).

## Patches

- `musl-libintl.patch` — adds `-lintl` to the libraries of `unotools`. The `libintl.h` of [gettext](#port-gettext) renames `bindtextdomain` to `libintl_bindtextdomain`, which musl does not define, so `unotools`, which calls it, does not link without the library.
- `musl-stacksize.patch` — replaces the platform test in `sal/osl/unx/thread.cxx` with `#if 1`, so `sal` sets the stack size of every thread it creates, 1 MiB in an optimised build. Upstream does that on Linux only in builds without runtime optimisations; otherwise a thread gets the C library's default, which on musl is 128 KiB, and the parser and layout threads overflow it.
- `sfinae-probe-cxxflags.patch` — corrects the `-Wsfinae-incomplete` probe in `configure`, which saves `$CFLAGS` and restores it as `$CXXFLAGS`. Without the patch every later C++ probe passes the tree's `-std=gnu11` to `g++` and, under `-Werror`, fails, and the C++20 probe stops configure.
- `externals-ldflags.patch` — adds gbuild's external-project link flags, which begin with `$(LDFLAGS)`, to the link of six bundled libraries: to the `LDFLAGS` that the build recipes of libetonyek, liblangtag, raptor, rasqal and redland set to little more than the `$ORIGIN` run path, and to the `CC` that lp_solve's recipe passes its build script. Without the patch the six are linked without the tree's link flags, among them `-z now`, whose absence the [package lint](https://github.com/kunaldawn/kdos/blob/main/docs/kdos/03-architecture/packaging.md#the-package-lint) reports.

## Notes

The vendor bundle, `libreoffice-vendor-<version>.tar.xz`, is every file LibreOffice's
`download.lst` names, at the name and sha256 that file gives it. `build.sh` unpacks it into the
source root, where `--with-external-tar` finds it. Which of its tarballs are built is decided by
the `--with-system-*` and `--without-system-*` flags, so moving a library from one list to the
other needs no new source. No tool writes this bundle: on a version bump it is rebuilt by hand from
the new `download.lst`, each `*_TARBALL` (or `*_JAR`, `*_TTF`) with its `*_SHA256SUM`, fetched from
`dev-www.libreoffice.org/src` or `/extern`, checked against its sum, placed under `externals/`
and packed with the vendor tar flags of `ports/fetch` plus `--mode=go-w`. See
[A bundle no tool writes](https://github.com/kunaldawn/kdos/blob/main/docs/kdos/05-developer/writing-ports.md#a-bundle-no-tool-writes).

`sfinae-probe-cxxflags.patch` changes the generated `configure` and not `configure.ac`. A
`configure.ac` newer than the configuration makes the build rerun `autogen.sh`, which would
regenerate `configure` without the fix.

`make build` is the product. Plain `make` would also run the unit tests.

`distro-pack-install` leaves its per-module file lists (`gid_Module_*`) as plain files at the top
of `DESTDIR`, for a distribution that splits the suite into several packages. One package owns
every path here, and a file at the top of `$PKG` would be installed in `/`, so `build.sh` deletes
them.

The desktop entries in `/usr/share/applications` are installed as absolute links into
`/usr/lib/libreoffice/share/xdg`. They resolve on the installed system and dangle anywhere else,
so the preflight checks of an entry's name, icon and command would skip every one. `build.sh`
replaces each link with a copy of its target, removing the link first: toybox's `cp` follows a
link at the destination, and this one leads nowhere inside `$PKG`.

`build.sh` writes `share/registry/kdos.xcd`, settings layered over the defaults and read like any
other `.xcd` in that directory: no donation or get-involved bars, no what's-new bar or dialog
after an upgrade, each of which opens a web page, and no crash-report consent, since no reporter is
built.
