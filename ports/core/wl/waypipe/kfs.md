## About

Waypipe is a proxy for Wayland clients: run as `waypipe ssh host program`, it starts the program
on the other machine and forwards its Wayland protocol, buffers and file descriptors over the
SSH connection, so the window appears on this desktop as a local one. It is the Wayland
counterpart of X11 forwarding. The port builds the Rust implementation; the older C one,
`waypipe-c`, is not built.

## Flags

- `--buildtype=release` — Meson's `release` is optimisation level 3 with no debug information, and
  waypipe's `meson.build` maps that onto the Cargo profile `meson-3-strip`, which it passes to
  `cargo build`: opt-level 3, LTO, one codegen unit, debug information stripped.
- `-Dwerror=false` — Override the `werror=true` that upstream's `meson.build` sets in its default
  options, under which meson treats every compiler warning as an error. It reaches only C that
  meson compiles itself, which with `-Dbuild_c=false` is none; Cargo's build is a custom target
  that meson's warning options do not touch.
- `-Dbuild_rs=true`, `-Dbuild_c=false` — Build the Rust `waypipe` and not `waypipe-c`. Meson only
  drives `cargo` here: it works out which Cargo features to ask for from the options below and
  passes them with `--no-default-features --features`.
- `-Dtests=false` — Only `waypipe-c`'s `meson.build` reads this option. The Rust build declares its
  two tests, `cargo test` and a shortened `waypipe bench` run, regardless; they run only under
  `meson test`, which the build does not call.
- `-Dman-pages=enabled` — Generate `waypipe.1` with [scdoc](#port-scdoc), failing if it is
  missing.
- `-Dwith_video=enabled` — The `--video` option, which sends the forwarded buffers as a video
  stream encoded and decoded through [ffmpeg](#port-ffmpeg). It also needs `glslc` from
  [shaderc](#port-shaderc), which compiles waypipe's Vulkan shaders at build time.
- `-Dwith_dmabuf=enabled` — Forward DMA-BUFs, the buffers OpenGL and Vulkan programs share with
  the compositor, handling them through Vulkan ([vulkan-loader](#port-vulkan-loader)).
- `-Dwith_gbm=enabled` — Use [mesa](#port-mesa)'s `libgbm` as a limited fallback for DMA-BUFs
  where Vulkan cannot handle them.
- `-Dwith_lz4=enabled`, `-Dwith_zstd=enabled` — LZ4 and Zstandard compression of the forwarded
  data, through the [lz4](#port-lz4) and [zstd](#port-zstd) ports.

Each `with_` option is `auto` upstream, and an `auto` feature is dropped when its library is
missing, or, for the features built on a wrapper crate, when `bindgen` is; `enabled` makes the
configure fail instead.

## Notes

The crates come from `waypipe-vendor-<version>.tar.xz`, made by `cargo vendor` when `ports/fetch`
sees `vendoring = rust`, and the build is offline (`CARGO_NET_OFFLINE=true`). The FFmpeg, LZ4,
Zstandard and GBM wrappers are crates inside the source tree whose bindings
[bindgen](#port-bindgen) generates against the installed headers at build time, which is why
`LIBCLANG_PATH` is set and [clang](#port-clang) is in `depends`; see
[rust](../../../../docs/kdos/05-developer/writing-ports.md#rust).

The machine at the other end needs waypipe too; [openssh](#port-openssh) is in `depends` because
`waypipe ssh` runs `ssh`.
