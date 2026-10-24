## About

Amiberry is an Amiga emulator, from the A500 to the A4000 and the CD32, built on SDL 3 with an ImGui
interface. It boots WHDLoad game archives directly and installs AROS's free Kickstart replacement
ROMs, so it starts an Amiga with no ROM of the user's own. Rendering is desktop OpenGL through
libglvnd.

## Flags

- `-D_LARGEFILE64_SOURCE` — Added to `CFLAGS` and `CXXFLAGS`. Amiberry's `zfile.cpp` and the
  bundled CAPS image library call the `*64` file functions (`fopen64`, `fseeko64`, `ftello64`),
  which musl declares only under this macro.
- `-DUSE_OPENGL=ON` — With `-DUSE_GLES=OFF` and `-DUSE_VULKAN=OFF`: desktop OpenGL, through
  [libglvnd](#port-libglvnd) and [mesa](#port-mesa). These are upstream's defaults, named;
  upstream calls its Vulkan renderer experimental and exclusive of OpenGL.
- `-DUSE_JIT=ON` — The 68k JIT recompiler, for x86_64 and AArch64. Upstream's default, named.
- `-DUSE_PCEM=ON` — The PCem code that emulates the x86 bridgeboards. Upstream's default, named.
- `-DUSE_PPC=ON` — With `-DUSE_QEMU_PPC=ON`: support for the PowerPC accelerator boards and for
  the QEMU-UAE PowerPC plugin that runs their CPU. The plugin itself is not a KDOS port, so a
  PowerPC board is configurable but has no CPU to run.
- `-DUSE_LIBSERIALPORT=ON` — With `-DUSE_LIBENET=ON`, `-DUSE_PORTMIDI=ON`, `-DUSE_LIBMPEG2=ON`,
  `-DUSE_MPG123=ON`, `-DUSE_UAENET_PCAP=ON`, `-DUSE_UAENET_TAP=ON` and `-DUSE_ZSTD=ON`: serial
  port emulation through [libserialport](#port-libserialport), networking through
  [enet](#port-enet) and through [libpcap](#port-libpcap) or a TAP device, MIDI through
  [portmidi](#port-portmidi), CD32 full-motion video through [libmpeg2](#port-libmpeg2), MP3 CD
  tracks through [mpg123](#port-mpg123), and CHD compressed disk images through
  [zstd](#port-zstd). All are upstream's defaults, named. Configure stops when libpcap or mpg123
  is missing, but a missing libserialport, enet, portmidi or zstd costs only a `STATUS` message
  and the feature; every one of these libraries is in `depends`.
- `-DUSE_GPIOD=OFF` — No GPIO-driven LEDs, a feature for single-board computers. Upstream's
  default, named.
- `-DUSE_DBUS=OFF` — No D-Bus remote-control interface. Upstream's default, named;
  `-DUSE_IPC_SOCKET=ON`, its Unix-socket counterpart, stays on.
- `-DWITH_LTO=OFF` — With `-DWITH_OPTIMIZE=OFF`: no link-time optimisation and none of upstream's
  native-CPU tuning, which would tie the binary to the processor that built it. Upstream's
  defaults, named.
- `-DBUNDLE_SDL=OFF` — Links the system [sdl3](#port-sdl3) and does not copy SDL's shared
  libraries into the package, an option upstream offers for distributions without SDL 3.
  Upstream's default, named.

## Patches

- `musl-null.patch` — In `src/custom.cpp`, casts `0` instead of `NULL` to the `uaecptr` address type in two custom-chip writes. musl defines `NULL` in C++ as `nullptr`, which `static_cast` cannot turn into an integer.
- `no-update-check.patch` — Makes `get_update_method()` return `DISABLED` on Linux. Upstream returns `NOTIFY_ONLY` unless `FLATPAK_ID` is set, and at start-up queries GitHub's release API for a newer Amiberry when the update check is enabled. The WHDLoad-database download the user starts is untouched.
- `system-tinyxml2.patch` — Drops `src/tinyxml2.cpp` from the source list and links the `tinyxml2::tinyxml2` target of the [tinyxml2](#port-tinyxml2) port instead; `build.sh` deletes the bundled `tinyxml2.cpp` and `tinyxml2.h`, so the program cannot compile against its own copy by accident.

## Notes

The native file dialogs, nativefiledialog-extended, are a git submodule that the release archive
leaves empty; without them the build falls back to its own ImGui file dialog. The recipe fetches
a commit of it as a second source, `_nfd` in `kpkgbuild`, and `build.sh` moves it into
`external/nativefiledialog-extended`. Amiberry builds it with `NFD_PORTAL` forced on, so it opens
the file chooser of [xdg-desktop-portal](#port-xdg-desktop-portal) over D-Bus and links no GTK.
Its Wayland code is generated from the xdg-foreign protocol in another submodule,
`3ps/wayland-protocols`, empty in the commit archive too; `build.sh` replaces that directory with a symbolic link to
`/usr/share/wayland-protocols`, which is why [wayland-protocols](#port-wayland-protocols) is in
`depends`.

The AROS Kickstart ROMs and the WHDLoad boot binaries the package installs are 68k code for the
emulated machine, one of the payloads the tree does not compile (see
[What is built from source](https://github.com/kunaldawn/kdos/blob/main/docs/kdos/05-developer/writing-ports.md#what-is-built-from-source)).

Upstream's desktop entry is replaced by one without `application/x-cue`: a cue sheet is any CD
image, and its default player should not be an Amiga emulator.
