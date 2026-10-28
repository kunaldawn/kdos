## About

[SuperTuxKart]{.idx} is a 3D kart racer: story mode, grand prix, time trials, battle arenas and
soccer, against the computer or other people on the same machine or over the network. It renders
through its own fork of the Irrlicht engine, with an OpenGL renderer and a Vulkan one, and plays
sound through OpenAL. This port is the program and the engine's own data; the karts, tracks,
music, sounds, models and textures are [stk-assets](#port-stk-assets), built from the same
release tarball.

## Flags

- `-DSERVER_ONLY=OFF` — build the game with graphics and sound. `ON` builds a dedicated network
  server only. This is upstream's default, named.
- `-DCHECK_ASSETS=OFF` — skip upstream's configure-time check that the karts, tracks and the other
  asset directories are present. The release tarball carries them in `data/`, so the check would
  pass; with it off, the build does not depend on where the assets sit.
- `-DBUILD_RECORDER=OFF` — build without the in-game video recorder. It needs libopenglrecorder,
  which the tree does not carry, and in a release build its absence is a configure error rather
  than a warning.
- `-DUSE_WIIUSE=ON` — with `-DUSE_SYSTEM_WIIUSE=OFF`: support Wii remotes as controllers, through
  the copy of the wiiuse library in the source, which talks to them over Bluetooth with
  [bluez](#port-bluez)'s `libbluetooth`. The tree has no wiiuse port. Both are upstream's
  defaults, named.
- `-DUSE_IPV6=ON` — allow creating and joining network games over IPv6. It also makes the build use
  the copy of ENet in the source rather than the tree's [enet](#port-enet), because the IPv6
  support lives in that copy. Upstream's default, named.
- `-DUSE_SQLITE3=ON` — keep server statistics and the ban list in SQLite, for a network game this
  machine hosts. Upstream turns it off with a warning when SQLite is missing; [sqlite](#port-sqlite)
  is in `depends`.
- `-DUSE_CRYPTO_OPENSSL=ON` — encrypt network-game traffic between client and server with
  [openssl](#port-openssl) rather than mbed TLS. Upstream's default, named.
- `-DUSE_MOJOAL=OFF` — use the system OpenAL, [openal-soft](#port-openal-soft), rather than the
  copy of MojoAL in the source. Upstream's default, named.
- `-DUSE_DNS_C=OFF` — resolve server names through the C library's resolver rather than the DNS
  library bundled in the source. Upstream's default on Linux, named.
- `-DUSE_GLES2=OFF` — the desktop OpenGL renderer rather than OpenGL ES 2, upstream's default on
  x86-64, named.
- `-DNO_SHADERC=OFF` — build the Vulkan renderer, which compiles its shaders at run time with
  [shaderc](#port-shaderc). With shaderc missing and this off, configure stops with an error rather
  than leaving Vulkan out.
- `-DUSE_LIBBFD=OFF` — no libbfd in the crash reporter and leak checker, upstream's default, named.

## Patches

- `offline-default.patch` — In `src/config/user_config.hpp`, changes the default of the `enable_internet` setting from `0` ("the user was not asked") to `2` ("not allowed"). With `0` the game asks on its first start whether it may connect to the SuperTuxKart servers; with `2` it does not ask, and fetches no news and no add-on list until the player ticks "Connect to the Internet" under Options, General. Online multiplayer is behind the same setting.

## Notes

`cmake --install` installs two libraries built from copies in the source, wiiuse and AngelScript
(the scripting language tracks use), as static archives with their headers and AngelScript's CMake
package files. The game links both into its binary, so `build.sh` removes them, and the empty
`/usr/include` and `/usr/lib` directories after them. A change that makes upstream install any
other file under those directories makes the `rmdir` fail and the build stop.

Upstream installs the whole of `data/` from the release, assets included. `build.sh` deletes the
`karts`, `library`, `models`, `music`, `sfx`, `textures` and `tracks` directories again, because
[stk-assets](#port-stk-assets) installs those same seven, and kpkg's
[conflict scan](../../../../docs/kdos/03-architecture/packaging.md#who-owns-a-file) refuses a path
two packages claim. The list here and the list in `stk-assets/build.sh` have to stay the same. The
two ports share `group = supertuxkart`, so the version checker offers them only as one bump; both
name the same tarball by the same `sha256`, so the source cache holds it once.

The game reads its translations as `.po` files from `data/po` at run time. kpkg's
[language prune](../../../../docs/kdos/05-developer/writing-ports.md#size-and-language-policy)
removes only compiled catalogues, so `build.sh` deletes the `.po` files itself, and the game is
English only, like the rest of the system.
