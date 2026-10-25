## About
[Nheko]{.idx} is a desktop client for Matrix, written in C++ with a Qt 6 Quick interface: rooms
and direct chats, end-to-end encryption with device verification, and voice and video calls. The
Matrix protocol itself is [mtxclient](#port-mtxclient), the library from the same project, and
the encryption is [olm](#port-olm).

## Flags
- `-Wl,-z,stack-size=1048576`, added to `LDFLAGS` — ask for a 1 MiB stack in the program's
  `PT_GNU_STACK` header. musl gives each new thread 128 KiB of stack unless that header asks for
  more, and nheko's worker threads need more than that.
- `-D CMAKE_SKIP_RPATH=ON` — write no RPATH into the program.
- `-D HUNTER_ENABLED=OFF` — do not use Hunter, the CMake package manager upstream can fetch every
  dependency with. It is upstream's default, and most `USE_BUNDLED_…` switches take
  their default from it, so with it off spdlog, fmt, olm, cmark, nlohmann-json,
  mtxclient, lmdb, lmdbxx, qtkeychain, coeurl, libevent, curl, re2 and KDSingleApplication all
  come from their ports.
- `-D USE_BUNDLED_CPPHTTPLIB=ON` — compile the copy of cpp-httplib under `third_party/`, and with
  `-D USE_BUNDLED_BLURHASH=ON` the one of blurhash. Both are upstream's defaults, and neither
  library is a port.
- `-D VOIP=ON` — voice and video calls through GStreamer's WebRTC library
  (`gstreamer-webrtc-1.0`), upstream's default on Linux, named.
- `-D SCREENSHARE_XDP=ON` — share the screen in a call through the desktop's ScreenCast
  [portal](../../../../docs/kdos/03-architecture/session.md#portals). It is upstream's default
  when `VOIP` is on.
- `-D X11=OFF` — leave out the X11-only code: screen sharing by grabbing X11 windows and setting
  X11 window roles. Upstream gives the switch an empty default, which CMake reads as off.
- `-D MAN=ON` — build the manual page from its AsciiDoc source with
  [asciidoctor](#port-asciidoctor), upstream's default on Linux.
- `-D CI_BUILD=OFF` — upstream's switch for its own CI, which adds `-Werror`; off by default.

## Notes
Both patches are upstream commits and carry their own descriptions. One makes the CMake build find
the private Qt modules nheko links, which Qt from 6.10 on has to be asked for by name
(`GuiPrivate`, `QmlPrivate`); the other repairs how replies are drawn in the timeline under
recent Qt releases.
