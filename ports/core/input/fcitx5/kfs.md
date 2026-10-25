## About

fcitx5 is an [input method]{.idx} framework: the engine that turns keystrokes into text a keyboard
cannot type directly, such as Chinese, Japanese or Korean. On KDOS it is the program on the far end
of the compositor's `input-method-v2` protocol; [kdos-comp](#port-kdos-comp) relays what it
produces to applications over `text-input-v3`, so a boxed application reaches it through the
compositor and never directly. The language engines are separate ports built on this one:
[fcitx5-chinese-addons](#port-fcitx5-chinese-addons) over [libime](#port-libime),
[fcitx5-anthy](#port-fcitx5-anthy) for Japanese and [fcitx5-hangul](#port-fcitx5-hangul) for
Korean. `kdos-desktop-start` starts it as `fcitx5 -d` once the compositor is up, and the candidate
window is drawn as cells by `kdos-ime`, which takes over through fcitx5's kimpanel module. The
whole arrangement is in
[Input methods](https://github.com/kunaldawn/kdos/blob/main/docs/kdos/03-architecture/session.md#input-methods).

The build is Wayland only: no X11 frontend, no systemd, no autostart entry, and every helper
archive the build would download already in place.

## Flags

- `-D ENABLE_X11=Off` — Builds no X11 support: no XIM frontend and no X11 parts of the classic
  user interface. With it on, fcitx5 needs `xcb-imdkit`, `cairo-xcb`, `xkbfile` and a set of xcb
  libraries (`xcb-icccm`, `xcb-ewmh`, `xcb-keysyms` and others) on the host. The cost is that an
  X11 application under Xwayland, on the host or in a box, has no input method, because XIM is the
  only route an X client has to an engine.
- `-D ENABLE_WAYLAND=On` — Builds the Wayland frontend, which speaks `input-method-v2` and
  `virtual-keyboard` to the compositor, and the Wayland half of the classic interface.
- `-D ENABLE_DBUS=On` — Builds the D-Bus modules: the kimpanel interface through which `kdos-ime`
  draws the candidate window, the virtual-keyboard interface, and the D-Bus, fcitx4 and IBus
  frontends. Without it there is no `kdos-ime` route and fcitx5 draws its own panel.
- `-D ENABLE_KEYBOARD=On` — Builds the XKB key translation and the keyboard engine that ordinary
  layouts use. Upstream's configure refuses Wayland support without it.
- `-D ENABLE_EMOJI=On` — Builds the emoji module, which needs zlib.
- `-D ENABLE_LIBUUID=On` — Takes UUIDs from libuuid, part of [util-linux](#port-util-linux),
  for the core and the IBus frontend.
- `-D ENABLE_SERVER=On` — Builds the `fcitx5` program itself, not only the libraries for
  embedding it.
- `-D USE_SYSTEM_YOGA=Off` — Builds the copy of the Yoga layout library in `third_party/`, which
  the classic interface lays out with. No yoga port exists.
- `-D ENABLE_ENCHANT=On` — Gives the spell module [enchant](#port-enchant) as a second word
  provider beside fcitx5's own English list. Upstream's check for it is `REQUIRED` once the
  option is on.
- `-D ENABLE_TESTING_ADDONS=On` — Builds the testing addons, a test frontend, a test engine
  and a test interface, although `ENABLE_TEST` is off. This is upstream's default. [fcitx5-anthy](#port-fcitx5-anthy) and
  [fcitx5-hangul](#port-fcitx5-hangul) ask for the `TestFrontend` module with an unconditional
  `REQUIRED` `find_package`, so without these addons neither engine configures.
- `-D ENABLE_XDGAUTOSTART=Off` — Installs no `.desktop` file under `/etc/xdg/autostart`. Nothing
  on KDOS reads that directory; `kdos-desktop-start` starts fcitx5 by name.
- `-D BUILD_SPELL_DICT=On` — Compiles `en_dict.fscd`, the English word list the spell module uses
  for hints, from the `en_dict` archive the recipe carries (see Notes).
- `-D EVENT_LOOP_BACKEND=libuv` — Runs fcitx5's event loop on [libuv](#port-libuv), and with
  it D-Bus through libdbus. The default, `auto`, tries sd-event first and takes libuv only when
  no libsystemd is found; naming the backend means a libsystemd in the build root could never be
  chosen. KDOS has no systemd.
- `-D USE_SYSTEMD=Off` — Takes sd-event out of the `auto` search. Upstream reads it only when the
  backend is `auto`, so beside `EVENT_LOOP_BACKEND=libuv` it changes nothing in this build.
- `-D USE_SYSTEM_PLASMA_WAYLAND_PROTOCOLS=On` — Generates the Plasma window-management protocol
  code from the XML [plasma-wayland-protocols](#port-plasma-wayland-protocols) installs, instead
  of the copy in fcitx5's own tree. This is upstream's default, and it is why that port is a
  dependency.

## Notes

kpkg unpacks only the archive types it recognises and copies any other source file, and
`.tar.zst` is not one it unpacks, so `build.sh` unpacks the fcitx5 archive itself, with
`--strip-components=1`, before configuring.

The spell module's CMake fetches the `en_dict` archive with `file(DOWNLOAD)` at build time,
and the build runs with no network. Upstream's download step does nothing when a file with the
expected sha256 already sits where it would write it, and it resolves that place against the
module's source directory, so the recipe declares the archive as a second source and copies it,
still packed, into `src/modules/spell/`. It must be the archive itself: the check hashes the
`.tar.gz`, not its contents. [libime](#port-libime) and
[fcitx5-chinese-addons](#port-fcitx5-chinese-addons) carry their data the same way.

The archive's name and sha256 are written twice, in `kpkgbuild` and in the spell module's
`CMakeLists.txt`. A release that moves to a newer word list changes the CMake side, and the
recipe has to follow it. Otherwise the build has no word list: upstream's step deletes a copy
whose hash does not match without fetching another, and tries the network for a file name it
does not find.
