## About
[Mumble]{.idx} is low-latency group voice chat: the `mumble` client and `mumble-server`, the
server it connects to (Murmur, in upstream's build options), which a machine on a LAN can host for
its own group. Both are built from the one source. The client is a Qt 5 application, which
decides how the port links OpenSSL.

## Flags
- `-DOPENSSL_INCLUDE_DIR="$_ossl/include"` — with `-DOPENSSL_SSL_LIBRARY=/usr/lib/libssl.so.3` and
  `-DOPENSSL_CRYPTO_LIBRARY=/usr/lib/libcrypto.so.3`, hand CMake's FindOpenSSL the OpenSSL 3
  headers generated in the second source (see Notes) and the [openssl3](#port-openssl3) slot's two
  libraries, by path.
- `-DCMAKE_IGNORE_PATH="/usr/lib/cmake/OpenSSL;/usr/lib64/cmake/OpenSSL;/lib/cmake/OpenSSL;/lib64/cmake/OpenSSL"` — hide OpenSSL 4's CMake
  package. Mumble's dependency search tries a CMake configuration package before FindOpenSSL, and
  a package it finds there wins over the `OPENSSL_*` paths above.
- `-DCMAKE_INSTALL_SYSCONFDIR=/etc` — install `mumble-server.ini` as `/etc/mumble/mumble-server.ini`.
- `-DCMAKE_CXX_STANDARD=17` — compile as C++17 instead of Mumble's default of C++14. The
  [protobuf](#port-protobuf) package Mumble links requires C++17 of its consumers
  (`cxx_std_17`), and the Abseil headers it includes stop with an error under anything older.
- `-DBUILD_NUMBER="${version##*.}"` — the last part of the version. Mumble's version is
  `1.5.<build number>`, and without the switch upstream sets the number to 0, so the program would
  report the wrong version.
- `-Dclient=ON` — build the client and, with `-Dserver=ON`, the server; `-Dplugins=ON` builds the
  positional-audio plugins and `-Dmanual-plugin=ON` the built-in manual one. All are upstream's
  defaults, named.
- `-Dtests=OFF` — no test suite and, with `-Dbenchmarks=OFF`, no benchmarks.
- `-Dwarnings-as-errors=OFF` — upstream's default is on, so any warning the tree's compiler raises
  that upstream's did not would stop the build.
- `-Duse-timestamps=OFF` — keep the build's clock out of the output. With the switch on, upstream
  compiles the current year into the client's copyright line and writes the day of the build as
  the release date in the generated AppStream metadata, so two builds of one recipe on different
  days differ. Upstream's default is on.
- `-Dupdate=OFF` — the client does not check for a newer version by default.
  Upstream's default is on.
- `-Dcrash-report=OFF` — leave out the crash reporter that sends reports to the Mumble developers.
  Upstream's default is on.
- `-Dtranslations=OFF` — only the English interface, with `-Dbundle-qt-translations=OFF` adding no
  copy of Qt's translations. The tree's
  [language policy](../../../../docs/kdos/05-developer/writing-ports.md#size-and-language-policy)
  would remove the catalogues in any case.
- `-Dbundled-speex=OFF` — link the [speex](#port-speex) and [speexdsp](#port-speexdsp) ports, and
  with `-Dbundled-json=OFF` the [nlohmann-json](#port-nlohmann-json) port, instead of the copies
  under `3rdparty/`. Upstream's default for both is the bundled copy.
- `-Dbundled-gsl=ON` — use the copy of Microsoft's Guidelines Support Library under `3rdparty/gsl`,
  upstream's default; it is not a port (the tree's `gsl` is the GNU Scientific Library).
- `-Drenamenoise=ON` — machine-learning noise suppression with ReNameNoise, and with
  `-Dbundled-renamenoise=ON` the copy under `3rdparty/`, which is not a port. Both are upstream's
  defaults.
- `-Dalsa=ON` — the ALSA, `-Dpipewire=ON` PipeWire and `-Dpulseaudio=ON` PulseAudio audio
  backends, upstream's defaults on Linux, named.
- `-Djackaudio=OFF` — no JACK backend. The JACK library on the system is PipeWire's own, so a JACK
  switch left to detection would follow build order, and every port that can link JACK names it;
  see [JACK programs on the host](../../../../docs/kdos/03-architecture/session.md#jack-programs-on-the-host).
  Upstream's default is on.
- `-Dportaudio=OFF` — no PortAudio backend, and with `-Doss=OFF` no OSS one. Both are on by default
  upstream.
- `-Dspeechd=ON` — text-to-speech of chat events through [speech-dispatcher](#port-speech-dispatcher),
  upstream's default on Linux; `-Dqtspeech=OFF`, also the default, keeps that rather than Qt
  Speech.
- `-Dxinput2=ON` — link the X Input 2 extension (`X11::Xi`), which the client's global shortcuts
  use under X11; upstream's default on Linux. `-Dg15=OFF`, the default, leaves out the Logitech
  G15 keyboard display.
- `-Dzeroconf=ON` — find servers on the local network over mDNS through [avahi](#port-avahi),
  and announce `mumble-server` there; upstream's default.
- `-Dqssldiffiehellmanparameters=ON` — let the server use custom Diffie-Hellman parameters,
  upstream's default.
- `-Doverlay=OFF` — no in-game overlay, which upstream builds with the client by default; with
  `-Doverlay-xcompile=OFF` no 32-bit build of it either.
- `-Dice=OFF` — no remote control of the server over ZeroC Ice RPC, which the tree does not carry.
  Upstream's default is on.

## Patches
- `EVIO-int.patch` — casts the `EVIOCGVERSION`, `EVIOCGNAME`, `EVIOCGBIT` and `EVIOCGRAB` request codes to `int` in the `ioctl()` calls of `src/mumble/GlobalShortcut_unix.cpp`, which reads keyboards from `/dev/input` for global shortcuts. musl declares `ioctl()` with an `int` request, and the read requests' codes are larger than `INT_MAX`, so without the cast the compiler stops with an overflow-in-conversion error.
- `server-ini.patch` — sets `database=` in the shipped `mumble-server.ini` to `/var/lib/mumble-server/mumble-server.sqlite`, the state directory the package creates and the path the init script checks. Upstream leaves it empty, which makes the server search its default locations for the file and create it there if none is found.

## Notes
The tree's `openssl` is version 4, and Qt 5 cannot be built against it, so a Qt 5 program links
the `openssl3` slot, which installs `libssl.so.3` and `libcrypto.so.3` and no headers (see
[A second version beside the first](../../../../docs/kdos/05-developer/writing-ports.md#a-second-version-beside-the-first)).
The recipe's second source is that OpenSSL release, pinned in `_openssl`, and `build.sh` runs its
`./config` with the slot's own configuration line, word for word, then `make build_generated`,
which writes the generated headers and sources without compiling the library. The configuration
has to match, because the generated `include/openssl/configuration.h` records which algorithms the
library was built with. Nothing from that tree is installed.

After the install, `build.sh` reads the dynamic section of `mumble` and `mumble-server` and fails
the build unless each needs `libssl.so.3` and neither needs an OpenSSL 4 library. musl's loader
has one symbol namespace, so two `libcrypto` sonames in one process would resolve each other's
calls.

KDOS has no systemd, so the unit file, the `sysusers.d` and `tmpfiles.d` entries and the
per-user `mumble-server-user-wrapper` script upstream installs with them are deleted, and the
port ships its own init script, `/etc/init.d/75_mumble-server.sh`. It runs the server under
`supervise` as the `mumble-server` account, through `setpriv`, and skips with a message while
there is no account or no database yet; a database is made by running `mumble-server -supw` as
that account. `postinstall.sh` creates the account and group in the root being installed into,
`PKG_ROOT`, and gives the account `/var/lib/mumble-server` (mode 0750) and the `.ini`, which only
its group may read. It reads `/etc/passwd` and `/etc/group` under `PKG_ROOT` directly, because
`getent` answers for the running system and not the tree being installed. See
[postinstall.sh](../../../../docs/kdos/05-developer/writing-ports.md#postinstallsh).
