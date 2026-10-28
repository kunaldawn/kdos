## About

translateLocally is a Qt 6 program that translates text between languages on the CPU, with
neural models from the Bergamot project, so the text never leaves the machine. It downloads a
language pair's model on request (on the command line, `--download-model`); the package carries
none. Under it are bergamot-translator, the Marian inference engine (`marian-dev`, the Bergamot
fork), intgemm for 8- and 16-bit integer matrix multiplication, a fork of SentencePiece for
tokenising, and ssplit-cpp for splitting sentences, all compiled into the program.

## Flags

- `-DBUILD_ARCH=x86-64` — The `-march=` value translateLocally and Marian put on every compile
  line. Upstream's default is `native`, which compiles for the build machine's processor, and on
  that value Marian also probes the machine and adds `-mavx2`, `-mavx512f` and the rest. On any
  other value Marian adds `-msse4.1` on x86-64 instead, so the program needs a processor with
  SSE4.1. intgemm reads CPUID when it runs and picks its SSE2, SSSE3, AVX2, AVX-512BW or AVX-512
  VNNI kernels then.
- `-DMARIAN_GIT_REVISION="${_marian:0:7}"` — The Marian revision compiled into its version
  string, here the first seven characters of the pinned commit. The option exists only with
  `marian-no-git.patch`.
- `-DCOMPILE_CUDA=OFF` — No GPU build; CUDA is not a port.
- `-DUSE_MKL=OFF` — Marian's default is to look for Intel MKL first, which is not a port. Off,
  it takes the OpenBLAS branch: `BLAS_VENDOR` is `OpenBLAS` and it looks for a CBLAS in
  [openblas](#port-openblas).
- `-DUSE_STATIC_LIBS=OFF` — translateLocally and bergamot-translator set this `ON`, which makes
  Marian link libgcc and libstdc++ statically and search for `.a` files only. Off, the program
  links its libraries shared, found the way CMake finds them by default.
- `-DUSE_DOXYGEN=OFF`, `-DUSE_CCACHE=OFF` — No API documentation from Doxygen (on by default in
  Marian) and no compiler cache.
- `-DCOMPILE_SERVER=OFF`, `-DCOMPILE_PYTHON=OFF`, `-DCOMPILE_TESTS=OFF` — Build neither
  `marian-server`, nor bergamot-translator's Python bindings, nor its tests. All three are off
  upstream; they are named so a change of default cannot add them.
- `-DBUILD_EXTERNAL_LIBARCHIVE=OFF` — Link the system [libarchive](#port-libarchive), which
  unpacks downloaded models, instead of building a copy as a CMake external project. This is
  upstream's default, named so it cannot change.
- `-DSSPLIT_USE_INTERNAL_PCRE2=OFF` — ssplit-cpp links the system [pcre2](#port-pcre2) rather
  than building its own. Also ssplit-cpp's default, named so it cannot change.
- `-DSSPLIT_PREFER_STATIC_COMPILE=OFF` — Upstream's default is `ON`, which puts `.a` ahead of
  `.so` in CMake's library search. The tree installs no static PCRE2, so the search finds the
  shared library either way; off, it is not asked to prefer one that is not there.

## Patches

All six patch the vendored Marian under `3rd_party/bergamot-translator/3rd_party/marian-dev/`.

- `marian-musl-no-execinfo.patch` — In `src/3rd_party/ExceptionWithCallStack.cpp`, includes `<execinfo.h>` and walks the stack with `backtrace()` only when `__GLIBC__` is defined. musl has neither the header nor the function, so on musl the call-stack dump prints one line saying the C library has no `backtrace()`.
- `marian-musl-strerror-r.patch` — In `src/3rd_party/zstr/strict_fstream.hpp`, takes the XSI `strerror_r()` branch (it returns an `int`) whenever the C library is not glibc. The source picks the GNU branch, which expects a `char *` back, whenever `_GNU_SOURCE` is defined, and musl's `strerror_r` is XSI only.
- `marian-cstdint.patch` — Adds `#include <cstdint>` to `src/3rd_party/faiss/utils/misc.h` and `src/microsoft/quicksand.h`, which use the fixed-width integer types without including the header that declares them.
- `no-werror.patch` — Removes `-Werror` from Marian's `ALL_WARNINGS` list and from intgemm's compile options, so a warning the tree's compiler raises does not stop the build.
- `marian-honour-flags.patch` — Marian sets `CMAKE_CXX_FLAGS` and `CMAKE_C_FLAGS` outright, which drops the exported `CXXFLAGS` and `CFLAGS`. The patch starts each from its existing value, so the tree's flags reach every compile line, with Marian's own after them.
- `marian-no-git.patch` — Replaces the custom command in `src/CMakeLists.txt` that writes `common/git_revision.h` by running `git log` (and depends on `.git/logs/HEAD`) with a cache variable, `MARIAN_GIT_REVISION`, written into that header with `file(CONFIGURE)`. A source archive has no `.git`, so the command's dependency on `.git/logs/HEAD` cannot be met and `git log` has no repository to read.

## Notes

GitHub's archive of a commit does not carry its git submodules, so the recipe fetches each of the
five the build compiles as its own source, pinned by the commit in its helper (`_bergamot`,
`_marian`, `_intgemm`, `_spm`, `_ssplit`), and `build.sh` copies each into the directory the
submodule occupies before any patch is applied. The other submodules stay empty, because this
x86-64 configuration compiles none of them. Bumping translateLocally means reading the submodule
commits of the new revision and moving every helper with it; a submodule left behind builds a
mismatched engine.

After the configure, `build.sh` fails the build unless `CMakeCache.txt` records
`cblas_openblas_WORKS:INTERNAL=1`. Marian's BLAS search does not stop the configure when it finds no
working CBLAS; it only leaves `BLAS_FOUND` undefined.

Upstream installs its logo as `/usr/share/icons/translateLocally_logo.png` and `.svg`, and its
desktop entry names `Icon=translateLocally_logo`. Neither file is in hicolor's `apps/` directory,
where the desktop looks for an application's icon (see
[desktop entries](../../../../docs/kdos/05-developer/writing-ports.md#desktop-entries-belong-to-the-port)),
so `build.sh` deletes all three and rasterises the SVG with `rsvg-convert` to 48, 128 and 256
pixel PNGs under `hicolor/<size>/apps/translateLocally.png`, then writes an entry that names that
icon.
