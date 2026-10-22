## About
Mesa is the graphics driver stack: the OpenGL, OpenGL ES, EGL, GBM and Vulkan drivers for every
GPU family the image supports, the software renderers that stand in when there is none, a VA-API
video driver and an OpenCL implementation. On KDOS the port that leans on it hardest is
[kdos-comp](#port-kdos-comp): the compositor draws through EGL and GLES2 from here, and on a
machine with only software rendering, such as a virtual machine with plain graphics, it turns its
phosphor pass off
([The phosphor pass](https://github.com/kunaldawn/kdos/blob/main/docs/kdos/04-programs/kdos-comp.md#the-phosphor-pass)).

Mesa is built behind [libglvnd](#port-libglvnd), which owns `libGL`, `libEGL` and `libGLX` and
dispatches to Mesa's `libEGL_mesa` and `libGLX_mesa`. It is built for Wayland and for X11 alike:
there is no X server on the login path, but X11 clients run under Xwayland and draw through
Mesa's GLX; see
[No Xorg server, and one carve-out](https://github.com/kunaldawn/kdos/blob/main/docs/kdos/01-philosophy/principles.md#no-xorg-server-and-one-carve-out)
and [Desktop GL and GLX](https://github.com/kunaldawn/kdos/blob/main/docs/kdos/05-developer/testing.md#desktop-gl-and-glx).

Every probed dependency that has a switch is named on or off, so the set of drivers and features
does not depend on what happens to be installed when Mesa builds. Two probes have no switch:
`libudev` and `xcb-keysyms` are linked whenever they are found, which is why
[eudev](#port-eudev) and [xcb-util-keysyms](#port-xcb-util-keysyms) are in `depends`.

## Flags
- `ln -sf "$SRC_ROOT/syn-2.0.87" subprojects/syn-2.0.87` — The first of the blocks, one per Rust
  crate (`syn`, `unicode-ident`, `quote`, `proc-macro2`, `paste`, `rustc-hash`). Mesa's meson
  wraps would download each crate from crates.io; the recipe fetches the crates as sources of its
  own, links each unpacked crate where its wrap file says the directory goes, and copies the
  wrap's `packagefiles` overlay (the crate's `meson.build`) into it, so meson finds the subproject
  present and fetches nothing. The crates are for the Rust parts of Mesa, NVK's compiler among
  them.
- `LIBCLANG_PATH=/usr/lib/`, `LIBCLANG_STATIC_PATH=/usr/lib/` — Where `bindgen`, which generates
  rusticl's Rust bindings to the OpenCL and Mesa headers, finds libclang.
- `RUST_BACKTRACE=1` — Print a backtrace when a Rust build step panics, so a failure in the log
  says where.
- `RUSTFLAGS="-C prefer-dynamic"` — Link the Rust standard library dynamically into Mesa's Rust
  code rather than statically.
- `-D b_ndebug=true` — Compile with `NDEBUG`, so Mesa's assertions are off in the release build.
- `-D opengl=true`, `-D gles1=disabled`, `-D gles2=enabled` — Desktop OpenGL and OpenGL ES 2 and
  3; no OpenGL ES 1.
- `-D egl=enabled`, `-D gbm=enabled` — EGL, which every Wayland client and the compositor draw
  through, and GBM, the buffer allocator the compositor and Xwayland use on a DRM device.
- `-D glx=dri` — The GLX that X11 clients under Xwayland draw with, as `libGLX_mesa` behind
  libglvnd. It needs [libXxf86vm](#port-libXxf86vm) for direct rendering.
- `-D glvnd=enabled` — Install the drivers as libglvnd vendor libraries, `libEGL_mesa` and
  `libGLX_mesa`, and leave `libGL`, `libEGL` and `libGLX` to [libglvnd](#port-libglvnd).
- `-D platforms=wayland,x11` — The window systems EGL and Vulkan support. Wayland is the desktop;
  X11 is for clients under Xwayland, and its EGL and Vulkan code links `libxcb` and `libX11`
  whether or not any X11 client runs.
- `-D xlib-lease=enabled` — Vulkan's `VK_EXT_acquire_xlib_display`, which needs
  [libXrandr](#port-libXrandr). It is named because on `auto` it turns itself on with the X11
  platform.
- `-D llvm=enabled`, `-D shared-llvm=enabled` — Use [llvm](#port-llvm), which llvmpipe, lavapipe
  and rusticl need, and link it as the shared `libLLVM` rather than statically into each driver.
- `-D gallium-drivers=zink,crocus,iris,nouveau,r300,r600,radeonsi,svga,llvmpipe,softpipe,virgl,i915`
  — The OpenGL drivers: Intel (`i915`, `crocus`, `iris`, oldest to newest), NVIDIA (`nouveau`),
  AMD (`r300`, `r600`, `radeonsi`), VMware's virtual GPU (`svga`), QEMU's virtio GPU (`virgl`),
  OpenGL on top of a Vulkan driver (`zink`), and the two software renderers, `llvmpipe` and
  `softpipe`. It is the set upstream's `auto` picks on x86-64, written out.
- `-D vulkan-drivers=amd,intel,intel_hasvk,nouveau,swrast,virtio` — The Vulkan drivers: RADV for
  AMD, ANV and the older-generation `intel_hasvk` for Intel, NVK for NVIDIA, lavapipe on the CPU
  (`swrast`), and Venus for a virtual machine's virtio GPU. It is upstream's `auto` set on x86-64
  with Venus added.
- `-D vulkan-layers=device-select,intel-nullhw,overlay,anti-lag` — The Vulkan layers: GPU
  selection on a machine with more than one, Intel's null-hardware layer for testing, the
  statistics overlay, and Mesa's implementation of AMD's `VK_AMD_anti_lag` latency extension.
- `-D gallium-extra-hud=true` — Add the disk, network and CPU-frequency sources to the Gallium HUD,
  the overlay `GALLIUM_HUD` draws over any GL client; see
  [The overlay, which is already on every machine](https://github.com/kunaldawn/kdos/blob/main/docs/kdos/05-developer/testing.md#the-overlay-which-is-already-on-every-machine).
- `-D lmsensors=enabled` — Let the HUD show temperatures and other readings from
  [lm-sensors](#port-lm-sensors).
- `-D xmlconfig=enabled`, `-D expat=enabled` — Read driconf files (`/usr/share/drirc.d`,
  `/etc/drirc`, `~/.drirc`) with [expat](#port-expat), so options such as `vblank_mode` can be
  set per application. Disabled, Mesa compiles its default driconf in.
- `-D gallium-rusticl=true` — rusticl, Mesa's OpenCL implementation. It installs
  `/etc/OpenCL/vendors/rusticl.icd`, which only [ocl-icd](#port-ocl-icd)'s loader reads, and its
  OpenCL C compiler needs [libclc](#port-libclc), [spirv-llvm-translator](#port-spirv-llvm-translator)
  and [clang](#port-clang).
- `-D spirv-tools=enabled` — Link [spirv-tools](#port-spirv-tools), which the OpenCL C path
  requires and which also dumps SPIR-V for debugging.
- `-D video-codecs=all` — Build hardware video decoding and encoding for every codec Mesa knows,
  including the patent-encumbered H.264, H.265 and VC-1 that upstream's default, `all_free`,
  leaves out. It applies to both VA-API and Vulkan Video.
- `-D gallium-va=enabled` — The VA-API driver for the Gallium GPUs upstream supports it on
  (`radeonsi`, `r600`, `nouveau`, `virgl`), loaded through [libva](#port-libva).
- `-D display-info=enabled` — Parse a display's EDID with [libdisplay-info](#port-libdisplay-info).
- `-D zstd=enabled` — Use [zstd](#port-zstd) instead of zlib where Mesa compresses, the on-disk
  shader cache among them.
- `-D intel-rt=enabled` — Vulkan ray tracing on Intel GPUs that support it.
- `-D libunwind=disabled` — No libunwind, which Mesa uses only for the stack traces it prints
  when debugging.
- `-D valgrind=disabled` — No Valgrind client requests compiled into the drivers.

## Notes
The crate blocks have to match the wrap files of the Mesa version being built. A bump that
moves a crate's version changes the wrap's `directory`, and a block left at the old name leaves
meson wanting a download the build cannot make; the crate's `source` and `sha256` lines in the
recipe move with it.
