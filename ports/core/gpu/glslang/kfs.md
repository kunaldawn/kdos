## About

glslang is Khronos's reference front end for the OpenGL and Vulkan shading languages: it parses
and validates GLSL and HLSL and compiles them to SPIR-V. It installs the `glslang` command (with
`glslangValidator`, its older name, as a link to it) and the libraries other compilers build on, among them
`libglslang` and `libSPIRV`. [shaderc](#port-shaderc), the compiler behind `glslc`, is built on it;
[ffmpeg](#port-ffmpeg) compiles its Vulkan shaders with the `glslang` command at build time, and
[libplacebo](#port-libplacebo) links the library to compile shaders at run time.

`build.sh` configures and builds the project twice from the same tree, once shared
(`build-shared`) and once static (`build-static`), and installs both into the one package.

## Flags

- `-include cstdint` — Compile every C++ file as though it began with `#include <cstdint>`, the
  header that declares the fixed-width integer types, for a source that uses them without
  including it (see `-include <header>` in the
  [flag glossary](../../../../docs/kfs/17-appendices/flag-glossary.md#the-compiler-and-linker-flags)).
- `-DCMAKE_INSTALL_LIBEXECDIR=lib` — Set CMake's libexec directory to `/usr/lib`. No install rule
  in glslang's CMake files names that directory, so on this release the switch changes nothing
  that is installed.
- `-DBUILD_SHARED_LIBS=ON` — In `build-shared`: build the libraries as shared objects, each
  with glslang's major version in its soname.
- `-DBUILD_SHARED_LIBS=OFF` — In `build-static`: build the same libraries as static archives
  (`libglslang.a` and the rest).
- `-DALLOW_EXTERNAL_SPIRV_TOOLS=ON` — Build against the installed [spirv-tools](#port-spirv-tools)
  through its CMake package. Without it glslang looks for SPIRV-Tools in its own `External/`
  directory, which a release archive does not populate (`update_glslang_sources.py` downloads it).
- `-DENABLE_OPT=ON` — Build in SPIR-V optimisation through SPIRV-Tools' optimiser library,
  `SPIRV-Tools-opt`. The installed `glslang-config.cmake` then requires
  `SPIRV-Tools-opt` of every CMake consumer.
- `-DGLSLANG_TESTS=OFF` — Build none of glslang's self-tests. The option defaults to on when
  glslang is the top-level CMake project, as it is here.

## Notes

The two installs write the same CMake package files under `/usr/lib/cmake/glslang`, and the static
one runs second, so the installed `glslang-targets.cmake` describes the static archives:
`glslang::glslang`, `glslang::SPIRV` and the rest are `STATIC IMPORTED` targets. A program that
finds glslang through CMake, as [shaderc](#port-shaderc) does, links it statically; one that links
`-lglslang` directly gets the shared library. The `glslang` command installed is the static
build's, for the same reason.

After the install, `build.sh` adds a `.so.0` link beside every `.so` in `/usr/lib`, pointing at
it: `libglslang.so.0` to `libglslang.so`, and so on for each library.
