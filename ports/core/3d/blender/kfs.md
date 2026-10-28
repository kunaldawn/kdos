## About
[Blender]{.idx} is a 3D suite: modelling, sculpting, animation, simulation, rendering with Cycles
and EEVEE, compositing, motion tracking and video editing, scripted in Python. This port builds it
against the tree's libraries and the system Python, as an ordinary install under `/usr` rather
than upstream's self-contained portable directory. Cycles renders on the CPU; EEVEE and the
viewport draw with OpenGL or Vulkan.

## Flags
- `-DWITH_STRICT_BUILD_OPTIONS=ON` — when a feature named `ON` lacks its library, stop configure
  instead of switching the feature off, so every `ON` in the list is a feature the package has.
  Every feature that needs a library the tree does not carry is named `OFF`.
- `-DWITH_LIBS_PRECOMPILED=OFF` — with `-DWITH_STATIC_LIBS=OFF` and `-DWITH_INSTALL_PORTABLE=OFF`:
  use the installed libraries rather than Blender's precompiled set under `lib/linux_x64` in the
  source tree, link them shared, and install into `/usr` instead of a redistributable directory
  with its own runtime.
- `-DWITH_CPU_CHECK=OFF` — no start-up processor check. It is a library only a portable install
  ships, and with strict options configure stops when it is left on.
- `-DWITH_PYTHON_INSTALL=OFF` — with `-DWITH_PYTHON_INSTALL_NUMPY=OFF`,
  `-DWITH_PYTHON_INSTALL_REQUESTS=OFF` and `-DWITH_PYTHON_INSTALL_ZSTANDARD=OFF`: Blender runs on
  the system [Python](#port-python3) and bundles none of its modules. NumPy and zstandard are the
  ports' ([numpy](#port-numpy), [python3-zstandard](#port-python3-zstandard)); zstandard is how
  Python scripts such as `blend_render_info` read a zstd-compressed `.blend`, and Blender's own
  loader does not need it.
- `-DPYTHON_VERSION` — the Python version, read from the interpreter, so CMake looks for that
  version's headers and library.
- `-DWITH_PYTHON_SECURITY=ON` — scripts inside a `.blend` file do not run unless the user allows
  them. Upstream's default, named.
- `-DWITH_BUILDINFO=OFF` — keep the build date and git hash out of the binary.
- `-DWITH_DOC_MANPAGE=ON` — with `-DWITH_BLENDER_THUMBNAILER=ON`: the manual page, and
  `blender-thumbnailer`, which file managers run to extract the preview image from a `.blend`.
- `-DWITH_GHOST_WAYLAND=ON` — with `-DWITH_GHOST_X11=ON`, `-DWITH_GHOST_CSD=ON`,
  `-DWITH_GHOST_XDND=ON`, `-DWITH_X11_XINPUT=ON` and `-DWITH_X11_XFIXES=ON`: GHOST, Blender's
  windowing layer, is built for Wayland, with its own window decorations, and for X11, and tries
  Wayland first.
- `-DWITH_GHOST_WAYLAND_DYNLOAD=OFF` — with `-DWITH_PULSEAUDIO_DYNLOAD=OFF` and
  `-DWITH_PIPEWIRE_DYNLOAD=OFF`: link the Wayland, PulseAudio and PipeWire libraries instead of
  opening them at run time, so the ELF names what the package needs.
- `-DWITH_PIPEWIRE=ON` — with `-DWITH_PULSEAUDIO=ON` and `-DWITH_OPENAL=ON`: the audio outputs.
  `-DWITH_JACK=OFF` and `-DWITH_SDL_AUDIO=OFF` leave out the other two.
- `-DWITH_GHOST_SDL=OFF` — no SDL windowing in place of the native backends. Upstream's default,
  named.
- `-DWITH_INPUT_NDOF=ON` — 3D mice and other six-degree-of-freedom devices, through
  [libspnav](#port-libspnav).
- `-DWITH_SYSTEM_GLOG=OFF` — with `-DWITH_SYSTEM_GFLAGS=OFF`: use Blender's bundled glog and
  gflags (the defaults, named), although the tree has [glog](#port-glog) and
  [gflags](#port-gflags): libmv includes glog's headers with none of the definitions glog's CMake
  target supplies, and the bundled pair is the one Blender tests with.
- `-DWITH_SYSTEM_BULLET=OFF` — with `-DWITH_SYSTEM_AUDASPACE=OFF`: the bundled Bullet physics and
  Audaspace audio libraries. Upstream marks a system Bullet unsupported, a system Audaspace
  cannot be combined with `-DWITH_RUBBERBAND=ON`, and neither library is a port.
- `-DWITH_HARFBUZZ=OFF` — with `-DWITH_FRIBIDI=OFF`: text layout without HarfBuzz and FriBidi.
  Upstream's default, named.
- `-DWITH_INTERNATIONAL=OFF` — no translations of the interface.
- `-DWITH_CYCLES_EMBREE=ON` — with `-DWITH_OPENIMAGEDENOISE=ON`: Cycles traces rays with
  [Embree](#port-embree) and denoises with [OpenImageDenoise](#port-oidn).
- `-DWITH_DRACO=ON` — with `-DWITH_MESHOPTIMIZER=ON`: glTF mesh compression, through
  [draco](#port-draco) and [meshoptimizer](#port-meshoptimizer).
- `-DWITH_CYCLES_OSL=OFF` — no Open Shading Language in Cycles; OSL is not a port.
- `-DWITH_LLVM=OFF` — with `-DWITH_CLANG=OFF`: link neither LLVM nor Clang. Upstream's defaults,
  named.
- `-DWITH_CYCLES_PATH_GUIDING=OFF` — no path guiding, which needs OpenPGL, not a port.
- `-DWITH_CYCLES_DEVICE_CUDA=OFF` — and the OptiX, HIP, HIP RT and oneAPI devices, with
  `-DWITH_CYCLES_CUDA_BINARIES=OFF` and `-DWITH_CYCLES_HIP_BINARIES=OFF`: the Cycles GPU devices
  need vendor compilers the tree does not carry.
- `-DWITH_USD=OFF` — with `-DWITH_MATERIALX=OFF`, `-DWITH_HYDRA=OFF` and `-DWITH_XR_OPENXR=OFF`:
  no USD import and export, MaterialX, Hydra render delegates or OpenXR virtual reality; none of
  their libraries is a port.
- `-DWITH_TRACY=OFF` — with `-DWITH_RENDERDOC=OFF`: no Tracy profiler and no RenderDoc capture
  hooks.
- `-DWITH_TBB_MALLOC_PROXY=OFF` — do not replace the C library's `malloc` with oneTBB's allocator.
- `-DWITH_LINKER_LLD=OFF` — with `-DWITH_LINKER_MOLD=OFF` and `-DWITH_COMPILER_CCACHE=OFF`: the
  default linker, and no ccache.
- `-DWITH_GTESTS=OFF` — no test programs.

## Patches
- `ffmpeg9-audaspace.patch` — the bundled Audaspace's FFmpeg writer reads `AVCodec`'s `sample_fmts` and `supported_samplerates`, fields the tree's [FFmpeg](#port-ffmpeg) does not have. The patch asks `avcodec_get_supported_config` for both, as Blender's own movie writer does; a null list means the encoder accepts any format or rate.

## Notes
The three numbered patches are Alpine's musl fixes, each with its header: the bundled glog claims
`<execinfo.h>`, which musl does not have; one header uses `uint64_t` without including
`<cstdint>`; and `readfile.hh` poisons `off_t`, which musl's own headers use after it.

The panel reads PNG icons only, so `build.sh` rasterises the scalable icon to 256 by 256 with
`rsvg-convert` and removes the SVG. It replaces upstream's desktop entry, whose
`StartupWMClass=Blender` does not match `blender`, the app_id GHOST gives its Wayland windows, with
one that carries `StartupWMClass=blender` and starts `blender --offline-mode`. That switch
disallows internet access for the run whatever Blender's "Allow Online Access" preference holds,
and leaves the preference itself as it is, so the extensions platform offers nothing that needs
the network.
