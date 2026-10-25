## About

PPSSPP is a PlayStation Portable emulator. The port builds its SDL front end, `PPSSPPSDL`, on
[sdl2-compat](#port-sdl2-compat), drawing with desktop OpenGL through libglvnd or with Vulkan
presented through the Wayland WSI. Its interface is its own, drawn in the game window, so no
toolkit is linked.

## Flags

- `-DUSING_QT_UI=OFF` — The SDL front end, not upstream's Qt wrapper around it.
- `-DUSING_GLES2=OFF` — With `-DUSING_EGL=OFF` and `-DUSING_FBDEV=OFF`: desktop OpenGL, with the
  GL context made by SDL, rather than OpenGL ES on EGL or a framebuffer device as on embedded
  boards.
- `-DUSING_X11_VULKAN=OFF` — No Xlib surface for Vulkan; upstream's default is on.
  [sdl2-compat](#port-sdl2-compat) is built with `-DSDL2COMPAT_X11=OFF`, so there is no X11
  window to hand it.
- `-DUSE_WAYLAND_WSI=ON` — Vulkan presents through its Wayland surface. Upstream's default, named.
- `-DUSE_VULKAN_DISPLAY_KHR=OFF` — No Vulkan full-screen output straight to a display, outside
  any compositor.
- `-DUSE_FFMPEG=ON` — With `-DUSE_SYSTEM_FFMPEG=ON`: the decoding behind the PSP's video
  playback (`MediaEngine`), through the [ffmpeg](#port-ffmpeg) port. Without `USE_SYSTEM_FFMPEG`,
  upstream links the libraries under its `ffmpeg/<platform>/lib`, which its own scripts in `ffmpeg/` build from an FFmpeg copy in the
  archive and which the release archive leaves empty.
- `-DUSE_SYSTEM_LIBSDL2=ON` — With `-DUSE_SYSTEM_LIBPNG=ON`, `-DUSE_SYSTEM_LIBZIP=ON`,
  `-DUSE_SYSTEM_ZSTD=ON`, `-DUSE_SYSTEM_RAPIDJSON=ON` and `-DUSE_SYSTEM_FREETYPE=ON`: links the
  tree's SDL, libpng, libzip, zstd, RapidJSON and FreeType. All but SDL have a copy under `ext/`
  that is compiled otherwise; the system SDL is upstream's default everywhere but macOS, named.
- `-DUSE_MINIUPNPC=ON` — With `-DUSE_SYSTEM_MINIUPNPC=ON`: UPnP port forwarding for ad hoc
  network play, through the [miniupnpc](#port-miniupnpc) port; PPSSPP uses it only when the user
  turns it on.
- `-DUSE_SYSTEM_SNAPPY=OFF` — With `-DUSE_SYSTEM_LIBCHDR=OFF`: snappy and libchdr, the CHD disc
  image reader, are compiled from the copies in `ext/`. libchdr is not a KDOS port.
- `-DUSE_DISCORD=OFF` — No Discord rich presence, which upstream builds by default from
  `ext/discord-rpc`.
- `-DUSE_CCACHE=OFF` — Does not wrap the compiler in ccache when one is found.
- `-DHEADLESS=OFF` — With `-DUNITTEST=OFF` and `-DATLAS_TOOL=OFF`: none of the developer targets,
  the `PPSSPPHeadless` test runner, the unit tests, or the tool that generates the font atlases.
- `-DGOLD=OFF` — Upstream's default, named. `GOLD` builds the paid "PPSSPP Gold" edition, whose
  binary is `PPSSPPGold`; off, the binary is `PPSSPPSDL`, the name the desktop entry runs.

## Patches

- `no-version-check.patch` — Takes Linux out of the platforms for which `Config::SupportsUpgradeCheck()` is true. Upstream fetches `http://www.ppsspp.org/version.json` on every fifth start while the default-on "check for new version" setting stays on.

## Notes

`test ! -e "$PKG/usr/include"` stops the build if the install puts anything under `/usr/include`,
so no header of a library compiled from `ext/` can reach the package beside the program.

`build.sh` deletes every interface translation in `/usr/share/ppsspp/assets/lang` but `en_US.ini`.
These `.ini` files are not message catalogs, so the package-wide English-only rule (see
[Size and language policy](https://github.com/kunaldawn/kdos/blob/main/docs/kdos/05-developer/writing-ports.md#size-and-language-policy))
does not reach them; PPSSPP falls back to `en_US` for every string.

Upstream's desktop entry is replaced. Its `StartupWMClass` names an identifier the SDL front end
never sets, where SDL's Wayland app_id is the executable's name, and its `MimeType` claims every CD
image and every ZIP file; this entry claims only `application/x-compressed-iso`, which upstream's
`assets/mime/ppsspp.xml` defines.
