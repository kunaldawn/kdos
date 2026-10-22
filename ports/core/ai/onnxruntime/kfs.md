## About
[ONNX Runtime]{.idx} runs neural-network models saved in the ONNX format. This port builds its CPU
execution provider only, as the C and C++ library `libonnxruntime` and the Python module
`onnxruntime`. The Python module is what [piper](#port-piper), the text-to-speech engine, imports
to run its voice models; it is built against [numpy](#port-numpy).

Every third-party library ONNX Runtime uses is compiled into `libonnxruntime` at the version this
release pins: abseil, protobuf, re2, flatbuffers, Eigen, the ONNX definitions and the rest. The
installed library needs nothing but the C and C++ runtime libraries. Each of those sources is a
further `source =` line of the recipe, named `ort-<name>-<version>`.

## Flags
- `-Wa,--noexecstack` — in `ASMFLAGS`, and `-Wl,-z,noexecstack` in `LDFLAGS`: mark every object
  assembled from the project's assembly sources, and every library linked, as needing no
  executable stack. The package lint refuses an ELF object that asks for one (see
  [What the package lint refuses](../../../../docs/kdos/05-developer/writing-ports.md#what-the-package-lint-refuses)),
  and `build.sh` checks the Python module's `GNU_STACK` header itself after the install.
- `-DFETCHCONTENT_TRY_FIND_PACKAGE_MODE=NEVER` — make every dependency CMake's `FetchContent`
  declares build from its pinned source, never from an installed package that `find_package`
  would find first. The tree has its own [abseil-cpp](#port-abseil-cpp),
  [protobuf](#port-protobuf), [re2](#port-re2), [eigen](#port-eigen) and
  [nlohmann-json](#port-nlohmann-json); a system protobuf built against another abseil would put
  two abseils in one process.
- `-Donnxruntime_CMAKE_DEPS_MIRROR_DIR` — the directory ONNX Runtime looks in before downloading
  a dependency. Each dependency is named in `cmake/deps.txt` by URL and SHA-1; a file at
  `<mirror>/<URL without https://>` is used in its place, still checked against that SHA-1 and
  still given upstream's patches from `cmake/patches`. `build.sh` builds that layout from the
  `ort-*` sources as symbolic links.
- `-Donnxruntime_BUILD_SHARED_LIB=ON` — build `libonnxruntime.so`, the shared library of the C
  and C++ API. Off by upstream's default.
- `-Donnxruntime_ENABLE_PYTHON=ON` — build the Python module's extension, `onnxruntime_pybind11_state.so`,
  with the pybind11 the release pins. Off by upstream's default.
- `-Donnxruntime_BUILD_UNIT_TESTS=OFF` — skip the unit tests, which upstream builds by default.
  They need googletest, which is not among the recipe's sources.
- `-Donnxruntime_USE_TELEMETRY=OFF` — no telemetry. It is upstream's default, named so that it is
  the recipe's own setting.
- `-Donnxruntime_BUILD_FOR_NATIVE_MACHINE=OFF` — no `-march=native -mtune=native`, which would
  tie the library to the processor of the machine that built it. Upstream's default, named: the
  tree builds for the x86-64 baseline and leaves the feature level to each machine (see
  [`-march` measured per machine](../../../../docs/kdos/01-philosophy/decisions.md#-march-measured-per-machine-not-chosen-for-a-population)).
- `-Donnxruntime_USE_XNNPACK=OFF` — with `onnxruntime_USE_MIMALLOC` and
  `onnxruntime_BUILD_BENCHMARKS`: no XNNPACK execution provider, no mimalloc allocator (which
  upstream offers on Windows only) and no micro-benchmarks, each upstream's default, so none of
  their sources is fetched.
- `-Donnxruntime_ENABLE_LTO=OFF` — no link-time optimisation, upstream's default, named. LTO is
  kept to the short list of hot libraries the tree names (see
  [Every port](../../../../docs/kdos/05-developer/writing-ports.md#every-port)).

## Patches
- `gcc16-cstdint.patch` — adds `#include <cstdint>` to `include/onnxruntime/core/framework/tensor_shape.h`, which uses the fixed-width integer types without including their header; GCC 16's C++ headers do not bring it in on the way to their own, so the file does not compile without it (see [A standard header the source never includes](../../../../docs/kdos/05-developer/build-troubleshooting.md#a-standard-header-the-source-never-includes)).

## Notes
[patch](#port-patch) is in `depends` because CMake applies upstream's patches from
`cmake/patches` to several fetched dependencies, protobuf, GSL and flatbuffers among them, with
`patch`.

The mirror loop in `build.sh` looks for each `ort-*` file in `$SRC`, `$PORT_SRC` and
`$SOURCE_DIR`, in that order, and stops the build naming the first one that is in none of them.
The `case` that picks the files reads the recipe's `source` lines, so a dependency added to the
recipe needs no change to `build.sh`, but its name must keep the `ort-` prefix.

The Python module is built as a wheel by `setup.py`, run from the build directory, where the
module and the two shared libraries were built, and installed with `pip3`. The wheel carries its
own copies of `libonnxruntime.so.<version>` and `libonnxruntime_providers_shared.so`;
`build.sh` replaces each with a link to the file already installed in `/usr/lib`, so the package
holds one copy of each library.
