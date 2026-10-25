## About
[PDFium]{.idx} is Chromium's PDF engine. KDOS builds it as a shared library, `libpdfium.so`,
with its C headers under `/usr/include/pdfium` and two pkg-config files, for the two ports that
use it: [python3-pypdfium2](#port-python3-pypdfium2), the Python binding that
[OCRmyPDF](#port-ocrmypdf) renders pages with, and [libvips](#port-libvips)'s PDF loader. The
port's version is the PDFium branch number, which is also the build number of the Chromium
release it belongs to (`_chromium` in the recipe).

## Flags
- `export CC=gcc CXX=g++ AR=ar NM=nm` — The `//build/toolchain/linux/unbundle:default`
  toolchain, named as both `custom_toolchain` and `host_toolchain`, takes its compiler,
  archiver and `nm`, and `CFLAGS`, `CPPFLAGS`, `CXXFLAGS` and `LDFLAGS`, from the environment.
  `is_clang = false` and `clang_use_chrome_plugins = false` go with it: the library is built with
  the tree's GCC.
- `-ffp-contract=off` — Do not fuse a multiply and an add into one floating-point operation.
  Chromium's own build, which is clang, passes the same flag in `build/config/compiler`; with
  `is_clang = false` that branch is not taken, so `build.sh` adds it through `CPPFLAGS`, which
  the toolchain passes to both C and C++ compiles.
- `--root-pattern=//:pdfium` — Keep gn to the library's own build graph, so the test targets,
  whose fonts are not in the tarball, are never loaded.
- `is_component_build = false` — A normal build, in which a gn `component` is a static library;
  `pdfium-shared-library.patch` is what makes `pdfium` a shared one.
- `treat_warnings_as_errors = false` — Do not compile with `-Werror`, which upstream's build
  turns on.
- `use_custom_libcxx = false`, `use_sysroot = false`, `use_glib = false`, `use_siso = false` —
  Link the system's libstdc++ instead of Chromium's bundled libc++, compile against the root
  instead of a downloaded Debian sysroot, use no GLib, and generate plain ninja files rather than
  ones for Chromium's `siso` build tool.
- `pdf_is_standalone = true` — Build PDFium on its own, outside a Chromium tree.
- `pdf_enable_v8 = false`, `pdf_enable_xfa = false` — Build without JavaScript in PDF forms,
  which needs V8, a second browser engine to build, and without XFA forms, which need V8 too.
- `pdf_use_skia = false` — Render with PDFium's own AGG rasteriser, not Skia.
- `pdf_use_partition_alloc = false` — Allocate with the system's `malloc` instead of Chromium's
  PartitionAlloc.
- `pdf_bundle_freetype = false`, `use_system_freetype = true`, `use_system_harfbuzz = true`,
  `use_system_lcms2 = true`, `use_system_libjpeg = true`, `use_system_libopenjpeg2 = true`,
  `use_system_libpng = true`, `use_system_libtiff = true`, `use_system_zlib = true` — Link every
  codec and font library from the tree instead of the copies under `third_party/`. All are ports
  in `depends` except libtiff, which only XFA reaches.

## Patches
- `pdfium-shared-library.patch` — Turns the `pdfium` target from a gn `component`, which outside a component build is a static library, into a `shared_library`; makes `FPDF_EXPORT` export the API unconditionally instead of only under `COMPONENT_BUILD`; and makes the headers in `public/cpp` include their siblings by relative path, so the installed headers resolve under `/usr/include/pdfium`.

## Notes
- **The source and the vendor bundle.** The tarball is LibreOffice's copy of the PDFium branch;
  it carries PDFium's own tree with its `third_party` copies, and of what gclient would fetch,
  fast_float and part of abseil. The rest of what gn needs is the vendor
  bundle, made by hand: Chromium's `//build` at the revision the branch's `DEPS` names as
  `build_revision`, `third_party/abseil-cpp` with its `BUILD.gn` files at `abseil_revision`, and
  `tools/generate_shim_headers/generate_shim_headers.py` from the matching Chromium tag. How to
  rebuild it on a version bump is in
  [A bundle no tool writes](../../../../docs/kdos/05-developer/writing-ports.md#a-bundle-no-tool-writes).
  `build.sh` removes the tarball's partial abseil and unpacks the bundle over the tree.
- **The pins are checked first.** The recipe's `_build_rev` and `_abseil_rev` must be the
  revisions `DEPS` names; the loop at the top of `build.sh` stops the build otherwise, because a
  `//build` from another branch fails in gn with errors that name neither.
- **Two pkg-config files, two versions.** `libpdfium.pc` carries the Chromium version
  (`_chromium`), which python3-pypdfium2's build reads and records in its bindings; without it
  the binding records the build number as NaN, and every `build >= N` check in its helpers is
  false, so the constants and error paths those checks guard are silently missing.
  `pdfium.pc` carries the branch number: libvips looks the library up as `pdfium` and requires
  version `>= 4200`, a branch number, which the Chromium version would fail. A symbolic link
  between the two would give both consumers the same `Version` line.
- **Two files gclient would write.** `build/config/gclient_args.gni`, which `//build` imports, is
  written by `build.sh` with `build_with_chromium = false`, since no gclient runs; and Chromium's
  ICU unbundling shim, `build/linux/unbundle/icu.gn`, is installed as `third_party/icu/BUILD.gn`,
  so ICU comes from the [icu](#port-icu) port.
