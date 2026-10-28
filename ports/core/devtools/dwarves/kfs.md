## About

dwarves is [pahole]{.idx} and the other tools that read DWARF debugging information: `pfunct`,
`pdwtags`, `codiff`, `ctracer`, `btfdiff` and the rest. KDOS builds it for the kernel. The
[linux](#port-linux) port's configuration turns on `CONFIG_DEBUG_INFO_BTF`, and with it
`CONFIG_DEBUG_INFO_BTF_MODULES`, which defaults on. kbuild then runs `pahole` over the kernel's
DWARF to produce BTF, the compact description of the kernel's own types that
[bpftrace](#port-bpftrace), `sched_ext` and CO-RE BPF programs read. `linux` is the only port
that depends on it, and `60_kernel` builds it just before the kernel.

Without `pahole` in the root, the kernel's `olddefconfig` drops BTF without a word. The kernel
recipe checks for that afterwards and stops the build; see
[The kernel configuration](../../../../docs/kdos/05-developer/how-kdos-is-built.md#the-kernel-configuration).

## Flags

- `-DLIBBPF_EMBEDDED=OFF` — Link the [libbpf](#port-libbpf) port, found through pkg-config,
  instead of compiling the copy of libbpf the tarball carries in `lib/bpf`, which is upstream's
  default. CMake then stops if pkg-config finds no `libbpf` new enough.
- `-DLIB_INSTALL_DIR=lib` — `CMakeLists.txt` installs `libdwarves`, `libdwarves_emit` and
  `libdwarves_reorganize` to `${LIB_INSTALL_DIR}`, a variable it never sets; `CMAKE_INSTALL_LIBDIR`
  does not reach those two install lines. This puts the libraries in `/usr/lib` with the rest.
- `-DCMAKE_DISABLE_FIND_PACKAGE_Python3=ON` — Stop CMake looking for Python. When it finds one,
  the build installs `ostra-cg`, a Python script that draws call graphs from a `ctracer` trace, and
  its module under `/usr/share/dwarves/runtime/python`. With the switch neither is installed, so
  what the package holds does not depend on whether `python3` happens to be in the root when it is
  built.

## Notes

- **musl has neither argp nor obstack**, the two glibc interfaces the tools are written against.
  [argp-standalone](#port-argp-standalone) and [musl-obstack](#port-musl-obstack) provide them as
  libraries. dwarves' own `Findargp.cmake` and `Findobstack.cmake` look for `libargp` and
  `libobstack`, and when one finds nothing it prints `Assuming argp is in libc` (or `obstack`) and
  carries on, so a root without the two ports passes configure and fails to compile at the first
  `#include <argp.h>`.
