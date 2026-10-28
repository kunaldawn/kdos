## About

[Warzone 2100]{.idx} is a real-time strategy game set after a nuclear war: the player builds a
base, researches a large technology tree and designs their own units, in campaigns, skirmishes
against the computer or multiplayer. It renders through OpenGL or Vulkan on SDL3, and its
texture packages are encoded during the build from the PNGs in the source.

## Flags

- `-Wl,--strip-debug` — appended to `LDFLAGS`. On GCC and Clang upstream appends `-g` to
  `CMAKE_CXX_FLAGS_RELEASE`, after the tree's flags, so every object carries debug information;
  the linker drops it from the program (see the
  [rule on debug information](../../../../docs/kdos/05-developer/writing-ports.md#what-kpkg-does-around-buildsh)).
- `-DWZ_SKIP_ELF_SEPARATE_DEBUG=ON` — skip upstream's post-build step that copies the debug
  information into a separate `.debug` file and links it to the program with a debug link.
- `-DWZ_DISTRIBUTOR=KDOS` — the distributor name compiled in and reported with the build's
  properties. Upstream's default is `UNKNOWN`.
- `-DWZ_ENABLE_WARNINGS=OFF` — with `-DWZ_ENABLE_WARNINGS_AS_ERRORS=OFF`: no additional warnings,
  and warnings are not errors. Upstream's default for the second is `ON`, so a warning a newer
  compiler adds would otherwise stop the build.
- `-DWZ_ENABLE_BASIS_UNIVERSAL=ON` — with `-DWZ_INCLUDE_TERRAIN_HIGH=ON` and
  `-DWZ_DOWNLOAD_PREBUILT_PACKAGES=OFF`: support Basis Universal compressed textures, include the
  high-quality terrain texture pack, and encode that pack here from its PNGs in `data/`, as every
  other texture package is, rather than downloading upstream's prebuilt `high.wz`. The encoder is
  `basisu`, which configure builds from `3rdparty/` when none is installed. The encode is slow,
  and it needs no network.
- `-DWZ_DISABLE_FETCHCONTENT_GIT_CLONE=ON` — never clone the Vulkan headers from git when the
  system's are missing or too old. With
  `-DCMAKE_REQUIRE_FIND_PACKAGE_VulkanHeaders=ON`, the [vulkan-headers](#port-vulkan-headers)
  lookup is required instead, so a missing one fails the configure.
- `-DWZ_ENABLE_BACKEND_VULKAN=ON` — build the Vulkan renderer beside the OpenGL one; its shaders
  are compiled with [shaderc](#port-shaderc). Upstream's default on Linux, named.
- `-DWZ_USE_SYSTEM_LIBJPEG_TURBO=ON` — link the tree's [libjpeg-turbo](#port-libjpeg-turbo).
  Upstream defaults to its bundled copy, because some distributions' CMake files for the library
  do not work.
- `-DWZ_FORCE_MINIMAL_OPUSFILE=ON` — a minimal build of the bundled Opusfile without HTTP stream
  support, which upstream says the game does not need. Upstream's default, named.
- `-DWZ_DEBUG_GFX_API_LEAKS=OFF` — no debugging of graphics-API object leaks, which upstream turns
  on by default.
- `-DENABLE_GNS_NETWORK_BACKEND=ON` — build the GameNetworkingSockets network backend from
  `3rdparty/`, on [protobuf](#port-protobuf). Upstream's default, named.
- `-DENABLE_DISCORD=OFF` — with `-DWZ_BUILD_SENTRY=OFF`: no Discord presence and no Sentry crash
  uploader. Both are upstream's defaults, named.
- `-DENABLE_NLS=OFF` — compile no translations, so the game is English only, like the rest of the
  system. Upstream's default is `ON`.
- `-DENABLE_DOCS=ON` — generate the manual with [asciidoctor](#port-asciidoctor). Upstream's
  default; without Asciidoctor or `a2x` it skips the documentation with only a warning.
- `-DCMAKE_REQUIRE_FIND_PACKAGE_Fribidi=ON` — with the same switch for `Miniupnpc`: make each
  lookup required. Upstream finds [fribidi](#port-fribidi), for right-to-left text, and
  [miniupnpc](#port-miniupnpc), for opening a hosted game's port on the router, as optional. A
  missing fribidi builds the game without it, and a missing miniupnpc builds the copy in
  `lib/netplay/3rdparty/` into the program instead.

## Patches

- `no-update-check.patch` — In `src/updatemanager.cpp`, removes the two threads that `WzInfoManager::initialize()` starts to fetch update information and driver compatibility data from `data.wz2100.net` (with a GitHub mirror as fallback), and in their place sets the compatibility check's result to unsuccessful, the same result the game sets when that fetch fails, so whatever waits on that result is released.
- `vulkan-colorspace.patch` — In `lib/ivis_opengl/gfx_api_vk.cpp`, names the sRGB colour space `vk::ColorSpaceKHR::eSrgbNonlinear` instead of `eVkColorspaceSrgbNonlinear`, an alias that the tree's Vulkan-Hpp headers do not carry.

## Notes

`build.sh` removes `/usr/share/doc/warzone2100/hosting/linux_scripts`, the sample scripts for
running a dedicated host, which include a systemd unit, `wz2100host_sample.service`; KDOS has no
systemd.

Upstream installs its icon loose in `/usr/share/icons`, which no icon lookup reads, so `build.sh`
removes it, installs `icons/warzone2100.large.png` into hicolor instead, and writes its own desktop
entry over upstream's with `StartupWMClass=warzone2100`, the window's Wayland app id, which is the
program's name.
