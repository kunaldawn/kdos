## About
[Surfer]{.idx} is a waveform viewer for the traces digital simulations write, VCD, FST and GHW,
drawn with egui. The port builds two programs from the Cargo workspace: `surfer`, the viewer, and
`surver`, the waveform server a remote machine runs so that `surfer <url>` can browse a trace
that stays where it was simulated. The window is eframe on OpenGL through glow, built with its
Wayland and X11 backends.

## Notes
- **Two submodules are sources of their own.** The release archive carries the `f128` and
  `instruction-decoder` submodules empty, and both are path dependencies of the workspace, so
  cargo cannot resolve it without them. The recipe pins each to a commit (`_f128`, `_idecoder`),
  and `build.sh` copies their trees into the empty directories before anything else.
- **The vendor bundle is made by hand.** `ports/fetch` vendors from the first source alone, where
  those path dependencies are missing and `cargo vendor` fails, so the bundle is `cargo vendor`
  run by the fetch container's cargo over the tag tree with both submodules unpacked in place,
  packed the way `ports/fetch` packs one; its `sha256 =` line is its identity. How to rebuild it
  is in
  [A bundle no tool writes](../../../../docs/kdos/05-developer/writing-ports.md#a-bundle-no-tool-writes).
  The build itself is the canonical [Rust shape](../../../../docs/kdos/05-developer/writing-ports.md#rust),
  with `-p surfer -p surver` selecting the two binaries.
- **zstd is the system's.** The tree's build environment sets `ZSTD_SYS_USE_PKG_CONFIG=1` in every
  phase after the compilers, so `zstd-sys` links [zstd](#port-zstd) instead of compiling its own copy. The `readelf`
  line after `cargo build` makes that a condition of the build: `surfer` must NEED
  `libzstd.so.1`.
- **The menu entry is KDOS's own.** Surfer sets its window's app_id to
  `org.surfer-project.surfer` in `main.rs`, which the entry names as `StartupWMClass`. Upstream's
  entry claims `application/vnd.gtkwave-vcd`, `-fst` and `-ghw`, GTKWave's own MIME types, which
  no MIME database here defines, so KDOS's entry claims none. The icon is upstream's PNG.
