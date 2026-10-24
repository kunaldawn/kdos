## About
[libunwind]{.idx} (the nongnu.org project) determines a program's call chain: `libunwind` walks
the stack of the calling process, `libunwind-ptrace` that of another process through `ptrace`,
and `libunwind-coredump` that of a core file. [htop](#port-htop)'s backtrace screen,
[gstreamer](#port-gstreamer)'s and [libcamera](#port-libcamera)'s traces and
[samba](#port-samba)'s crash report use it.

It is distinct from LLVM's libunwind, the C++ unwinder of the same name, which the tree keeps
under a private prefix; `/usr/include/libunwind.h`, `libunwind.so` and the `libunwind*.pc` files
are this port's. See
[A second version beside the first](https://github.com/kunaldawn/kdos/blob/main/docs/kdos/05-developer/writing-ports.md#a-second-version-beside-the-first).

## Flags
- `--disable-unwind-header` — Do not install libunwind's `unwind.h`. GCC and clang each
  provide a header of that name in their own include directories, for the C++ exception
  runtime.
- `--enable-ptrace`, `--enable-coredump`, `--enable-setjmp` — Build `libunwind-ptrace`,
  `libunwind-coredump` and `libunwind-setjmp`. Each is autodetected by default; named, each is
  built, and `--enable-ptrace` makes configure stop if the target has no `sys/ptrace.h` instead
  of leaving the library out.
- `--enable-minidebuginfo` — Read symbol tables from the LZMA-compressed `.gnu_debugdata`
  section (MiniDebugInfo), with [xz](#port-xz)'s liblzma; configure stops if liblzma is missing.
- `--enable-zlibdebuginfo` — Read symbol tables from zlib-compressed sections, with
  [zlib](#port-zlib);
  configure stops if it is missing.

## Notes
The manual pages are generated from LaTeX sources by latex2man, which is not a port, and
configure then skips the documentation directory with a warning. The source carries them
already rendered as `doc/*.man`, and the loop at the end installs each as a section 3 page.
