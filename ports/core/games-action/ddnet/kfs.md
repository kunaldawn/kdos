## About

[DDNet]{.idx} (DDraceNetwork) is a cooperative online racing game built on Teeworlds: teams of
players work through maps together using the hook, hammer and weapons. This port builds the
client `DDNet`, the server `DDNet-Server` and upstream's map and demo tools, which install under
`/usr/lib/ddnet`. Part of the engine is written in Rust, so CMake drives a cargo build from a
vendored crate bundle alongside the C++.

## Flags

- `-DPREFER_BUNDLED_LIBS=OFF` — search the system's libraries before the prebuilt copies upstream
  keeps in `ddnet-libs`, a directory the release tarball carries empty. With it on, every find
  module searches `ddnet-libs` first. Upstream's default on Linux, named.
- `-DDOWNLOAD_GTEST=OFF` — do not download GoogleTest at configure time for the unit tests. A build
  with no network cannot, and upstream's default on Linux is off too.
- `-DAUTOUPDATE=OFF` — with `-DINFORM_UPDATE=OFF`: no self-updater and no notice of a new version.
  Upstream's default for `INFORM_UPDATE` is on, which shows a newer version in the start menu with
  a button that opens the download page on `ddnet.org`.
- `-DSTEAM=OFF` — with `-DDISCORD=OFF` and `-DDISCORD_DYNAMIC=OFF`: no Steam release build and no
  Discord rich presence, which links Discord's prebuilt `discord_game_sdk` library. All three are
  upstream's defaults, named.
- `-DMYSQL=OFF` — the server keeps its ranks in SQLite only, without the MySQL backend.
  Upstream's default, named.
- `-DWEBSOCKETS=ON` — let the server and client speak `ws://` addresses as well as UDP, through
  [libwebsockets](#port-libwebsockets). Upstream's default is off; with it on, a missing
  libwebsockets is a configure error.
- `-DANTIBOT=OFF` — no support in the server for loading a dynamic anti-cheat library, which
  upstream does not provide. Upstream's default, named.
- `-DUPNP=ON` — let a server the player hosts open its port on the router through UPnP, with
  [miniupnpc](#port-miniupnpc). Upstream's default is off; with it on, a missing miniupnpc is a
  configure error.
- `-DVIDEORECORDER=ON` — record demos to video through [ffmpeg](#port-ffmpeg). Upstream's default,
  named.
- `-DVULKAN=ON` — build the Vulkan renderer beside the OpenGL one. Its shaders are compiled at build
  time with `glslangValidator` from [glslang](#port-glslang) and optimised with `spirv-opt` from
  [spirv-tools](#port-spirv-tools); a missing `glslangValidator` or `spirv-opt` stops the
  configure. Upstream's default on Linux, named.
- `-DCLIENT=ON` — with `-DSERVER=ON` and `-DTOOLS=ON`: build the client, the server and the tools.
  All three are upstream's defaults, named.
- `-DIPO=OFF` — no interprocedural (link-time) optimisation. Upstream's default, named.
- `-DDEV=OFF` — a packaging build: upstream generates what packaging needs, strips absolute paths
  from debug information, and leaves out the extra assertions of a development build. Upstream's
  default, named.
- `-DPRECOMPILE_HEADERS=ON` — precompile the commonly used headers, which only shortens the build.
  Upstream's default, named.
- `-DSECURITY_COMPILER_FLAGS=ON` — let upstream add its hardening flags, such as
  `-fstack-protector-strong` and `-D_FORTIFY_SOURCE=2`, when the compiler supports them. Upstream's
  default, named.

## Notes

The crates are vendored (`vendoring = rust`): `ddnet-vendor-<version>.tar.xz` holds `vendor/`,
`Cargo.lock` and a `.cargo/config.toml` that replaces crates.io with the `vendor` directory. It is
made by `ports/fetch` and held to its `sha256 =` line like any other source (see
[Vendoring](../../../../docs/kdos/05-developer/writing-ports.md#vendoring)). CMake runs
`cargo build --locked` with the `build` directory as its working directory, and cargo finds its
configuration by walking up from there, so `build.sh` unpacks the bundle at the top of the source
tree, one level above `build`
([Where the bundle goes](../../../../docs/kdos/05-developer/writing-ports.md#where-the-bundle-goes)).
The `CARGO_HOME`, `CARGO_NET_OFFLINE` and `RUSTFLAGS` lines are the
[rust build shape](../../../../docs/kdos/05-developer/writing-ports.md#rust)'s; without
`-crt-static` the Rust half would link statically against musl.

`build.sh` removes upstream's `ddnet.desktop` and writes its own, which adds `StartupWMClass=DDNet`:
the window's Wayland app id is SDL's default, the executable's name, `DDNet`. Both entries claim the
`ddnet:` URL scheme, a link to a server address that the client connects to.
