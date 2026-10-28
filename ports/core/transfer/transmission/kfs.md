## About

Transmission is a BitTorrent client in several front ends over one engine, libtransmission. The
port builds `transmission-daemon` with its web interface, `transmission-remote`, which drives a
daemon, `transmission-cli`, which downloads on its own, the `transmission-create`, `-edit` and
`-show` tools for torrent files, and `transmission-qt`, the Qt 6 desktop client.

## Flags

- `-D BUILD_SHARED_LIBS=OFF` — CMake's default library type, static. libtransmission is declared
  static in any case, and with `-D INSTALL_LIB=OFF` neither it nor its headers are installed:
  every program links it in.
- `-D ENABLE_DAEMON=ON` — and `ENABLE_CLI`, `ENABLE_UTILS`, `ENABLE_QT`: the daemon,
  `transmission-cli` (off by default upstream), the utilities (`transmission-create`, `-edit`,
  `-remote` and `-show`) and the Qt client.
  `-D USE_QT_VERSION=6` builds that client against Qt 6 rather than whichever Qt configure finds
  first. Upstream's defaults are on for the daemon and the utilities and `AUTO` for the Qt
  client.
- `-D ENABLE_GTK=OFF` — and `-D ENABLE_MAC=OFF`: no GTK client and no macOS client; the Qt client
  is the desktop one. Both are `AUTO` upstream, which would build the GTK client whenever its
  libraries are installed.
- `-D REBUILD_WEB=OFF` — do not rebuild the web interface's generated assets, which runs npm and
  needs network access; this is upstream's default. The release tarball carries the assets built
  under `web/public_html`, and `-D INSTALL_WEB=ON` installs them.
- `-D INSTALL_DOC=ON` — install the manual pages and documentation.
- `-D ENABLE_NLS=ON` — native language support. `build.sh` then deletes every Qt translation
  catalogue but the English ones; kpkg's
  [size and language policy](../../../../docs/kdos/05-developer/writing-ports.md#size-and-language-policy)
  removes the same catalogues from every package.
- `-D ENABLE_TESTS=OFF` — no unit tests, which upstream builds by default.
- `-D ENABLE_UTP=ON` — µTP, BitTorrent's UDP-based transport.
- `-D ENABLE_WERROR=OFF` — and `-D RUN_CLANG_TIDY=OFF`: warnings do not stop the build, and
  clang-tidy is not run (`AUTO` upstream, which runs it when found).
- `-D WITH_CRYPTO=openssl` — the crypto library, [openssl](#port-openssl), rather than the first of
  ccrypto, OpenSSL, wolfSSL and mbedTLS, in that order, that configure finds.
- `-D WITH_SYSTEMD=OFF` — no systemd start-up notification in the daemon; KDOS has no systemd (see
  [No systemd](../../../../docs/kdos/01-philosophy/principles.md#no-systemd)).
- `-D WITH_INOTIFY=ON` — watch directories for new `.torrent` files with inotify.
- `-D WITH_APPINDICATOR=OFF` — no AppIndicator tray icon, which only the GTK 3 client uses.
- `-D USE_SYSTEM_EVENT2=ON` — and `USE_SYSTEM_DEFLATE`, `USE_SYSTEM_PSL`, `USE_SYSTEM_MINIUPNPC`:
  link [libevent](#port-libevent), [libdeflate](#port-libdeflate), [libpsl](#port-libpsl) and
  [miniupnpc](#port-miniupnpc) from their ports instead of the copies under `third-party/`.
- `-D USE_SYSTEM_DHT=OFF` — and `USE_SYSTEM_UTP`, `USE_SYSTEM_B64`, `USE_SYSTEM_NATPMP`: build the
  DHT, libutp, libb64 and libnatpmp copies under `third-party/`, none of which is a port.

## Notes

Upstream installs the logo as SVG only. The recipe rasterises
`icons/hicolor_apps_scalable_transmission.svg` to PNGs from 48 to 256 pixels named
`transmission-qt`, because the panel draws PNG icons only, and writes its own
`transmission-qt.desktop`: the window's app id is the binary's name, `transmission-qt`, which the
entry repeats in `StartupWMClass`. The entry claims no MIME type;
[qBittorrent](#port-qbittorrent) is the client that claims `.torrent` files and `magnet:` links.
