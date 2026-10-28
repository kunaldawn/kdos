## About

shaderc is Google's shader compiler built on [glslang](#port-glslang) and
[spirv-tools](#port-spirv-tools): `glslc`, a command-line compiler from GLSL and HLSL to SPIR-V
with GCC-like options, and `libshaderc`, the same as a library. `glslc` is the compiler Vulkan
projects call by name at build time: [libggml](#port-libggml) compiles the shaders of ggml's
Vulkan backend with it, the GPU path of whisper.cpp and llama.cpp, and [gtk4](#port-gtk4),
[blender](#port-blender), [waypipe](#port-waypipe) and the other ports that name it in `depends`
build with it too.

Each shaderc release names, in its `DEPS` file, the glslang and SPIRV-Tools revisions it was cut
against, and the recipe's version is chosen so that those match the versions of the glslang and
spirv-tools ports; a bump of either moves this one too.

## Flags

- `SHADERC_SYSTEM_DEPS=ON` — The switch `shaderc-system-deps.patch` adds: build against the
  installed glslang and SPIRV-Tools through their CMake packages, and the SPIRV-Headers in
  `/usr/include`, instead of the copies upstream expects in `third_party/`. Off, the build is
  upstream's.
- `SHADERC_SKIP_TESTS=ON` — Build none of the tests, which link googletest's gmock, another of
  the `third_party/` checkouts the release archive does not carry.
- `SHADERC_SKIP_EXAMPLES=ON` — Build none of the example programs.
- `SHADERC_SKIP_COPYRIGHT_CHECK=ON` — Do not run `utils/add_copyright.py` over the sources as part
  of every build, a check of the license headers in upstream's own tree.
- `SHADERC_ENABLE_WERROR_COMPILE=OFF` — Do not compile with `-Werror`, which upstream enables by
  default, so that a warning from this compiler stays a warning.

## Patches

- `shaderc-system-deps.patch` — Adds the `SHADERC_SYSTEM_DEPS` option to the top `CMakeLists.txt`. On, it replaces `add_subdirectory(third_party)` with `find_package` calls for `SPIRV-Tools`, `SPIRV-Tools-opt` and `glslang`, aliases the imported targets to the names shaderc's own targets link (`SPIRV-Tools`, `glslang`, `SPIRV`), and points `glslang_SOURCE_DIR` at the installed `/usr/include/glslang`, since the sources include glslang's headers relative to its source root. It also writes `build-version.inc` directly, with shaderc's version from its own `CHANGES`, SPIRV-Tools' from its pkg-config file and glslang's from its CMake package, because upstream's `update_build_version.py` reads the `CHANGES` files of source trees that are not there.

## Notes

The release archive carries `third_party/` with only its `CMakeLists.txt` and licence files: the
sources it expects there are fetched from git by upstream's `utils/git-sync-deps`, which a build
with no network cannot run. That is what the patch is for.

Only the shared library ships. `build.sh` removes `libshaderc.a`, `libshaderc_combined.a` and
their pkg-config files `shaderc_static.pc` and `shaderc_combined.pc`. `libshaderc_combined.a` is
meant to carry glslang and SPIRV-Tools inside it, and built against the installed packages it
carries neither; `shaderc_static.pc` names `-lshaderc_util`, a static library the install does not
ship. A program that linked either archive by its pkg-config file would not link.
