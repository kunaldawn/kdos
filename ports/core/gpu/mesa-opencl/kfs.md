## About
mesa-opencl is rusticl, Mesa's OpenCL implementation, as an OpenCL installable client driver:
`libRusticlOpenCL.so.1` and its `/etc/OpenCL/vendors/rusticl.icd`, which
[ocl-icd](#port-ocl-icd)'s loader reads to offer rusticl's devices to every OpenCL program
(`clinfo`, darktable, ffmpeg's OpenCL filters). It is a second build of [mesa](#port-mesa)'s
source (see
[A second build of the same source](../../../../docs/kdos/05-developer/writing-ports.md#a-second-build-of-the-same-source)):
`mesa` is built without OpenCL, because rusticl links `libclang-cpp` and reads
[libclc](#port-libclc)'s kernels at run time, and [clang](#port-clang) is more than a gigabyte
the installation medium does not carry for the few programs that compute on a GPU. Installed
by name, `kdos update install mesa-opencl`, it brings clang, libclc and
[spirv-llvm-translator](#port-spirv-llvm-translator) with it.

## Flags
- `ln -sf "$SRC_ROOT/syn-2.0.87" subprojects/syn-2.0.87` — The first of the crate blocks, the
  same as [mesa](#port-mesa)'s: each Rust crate is linked where its wrap file says and given its
  `packagefiles` overlay, so meson fetches nothing.
- `LIBCLANG_PATH=/usr/lib/`, `LIBCLANG_STATIC_PATH=/usr/lib/` — Where `bindgen`, which generates
  rusticl's Rust bindings to the OpenCL and Mesa headers, finds libclang.
- `RUSTC="rustc -C target-feature=-crt-static"` — Link rusticl against the shared C library; the
  musl target's static default would put a private, uninitialised copy of musl into
  `libRusticlOpenCL.so`, for the reasons in [mesa](#port-mesa)'s notes. The recipe checks the
  installed library for that copy, as `mesa`'s does.
- `-D gallium-rusticl=true` — rusticl, the one thing this build is for.
- `-D gallium-rusticl-enable-drivers=radeonsi` — Offer AMD GPUs to OpenCL programs with nothing
  set. Upstream's `auto` enables only the ARM drivers (`asahi`, `freedreno`, `panfrost`), none of
  which this build has.
- `-D gallium-drivers=iris,radeonsi,nouveau,llvmpipe,zink` — The drivers rusticl runs on here,
  linked into `libRusticlOpenCL.so` itself: Intel from Gen 8 (`iris`), AMD from GCN (`radeonsi`),
  NVIDIA (`nouveau`), the processor (`llvmpipe`) and any Vulkan GPU through `zink`.
- `-D opengl=false`, `-D egl=disabled`, `-D gbm=disabled`, `-D gles1=disabled`,
  `-D gles2=disabled`, `-D glx=disabled`, `-D glvnd=disabled`, `-D platforms=`,
  `-D vulkan-drivers=`, `-D vulkan-layers=`, `-D gallium-va=disabled` — None of Mesa's other
  front ends: `mesa` installs them, and building them again would only be discarded.
- `-D llvm=enabled`, `-D shared-llvm=enabled` — Link [llvm](#port-llvm)'s component libraries
  shared, the ones `mesa` already loads.
- `-D spirv-tools=enabled` — Link [spirv-tools](#port-spirv-tools), which rusticl's OpenCL C
  path requires.
- `-D xmlconfig=enabled`, `-D expat=enabled`, `-D zstd=enabled` — The same driconf reading and
  shader-cache compression as `mesa`'s drivers.
- `-D lmsensors=disabled`, `-D libunwind=disabled`, `-D valgrind=disabled` — No HUD sensors (the
  HUD is an OpenGL overlay), no libunwind stack traces and no Valgrind client requests.

## Notes
The recipe names Mesa's tarball under Mesa's own file name and checksum,
`mesa-$version.tar.xz::<url>`, and the same Rust crates, so the source archive holds one copy of
each for both ports. `version` must equal `mesa`'s, and both recipes carry `group = mesa`, so a
bump moves the two together.

The install goes into a staging directory inside the source tree, and only the library, its
`.so.1` link and the ICD file are copied into the package: everything else the build installs is
`mesa`'s, and packaging it twice would give two packages the same path.

rusticl offers a device only for a driver its build enables or `RUSTICL_ENABLE` names. Every
login shell gets `RUSTICL_ENABLE=iris,radeonsi` from `/etc/profile.d/50-opencl.sh` (see
[Graphics drivers](../../../../docs/kfs/16-wiki/graphics-drivers.md#opencl)), which a value set
beforehand, empty included, overrides; NVIDIA, the processor and zink are offered once it names
`nouveau`, `llvmpipe` or `zink`.
