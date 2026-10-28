## About
libffi lets a program call a C function whose argument types it learns only at run time, and
create a C function pointer, a closure, that calls back into it. Interpreters and binding layers
use it to reach C libraries: [python3](#port-python3) (its `ctypes` module), [glib](#port-glib),
[gobject-introspection](#port-gobject-introspection), [wayland](#port-wayland), and the runtimes
of Ruby, Guile, Node.js and GHC all link it. It is built inside the opening run of
`30_foundation`, because python3 depends on it.

## Flags
- `--disable-multi-os-directory` — Install the library in `--libdir` as it stands. Without it,
  configure asks the compiler for `-print-multi-os-directory` and appends the answer; this tree's
  GCC answers `../lib`, so the library would be installed under `/usr/lib/../lib` and `libffi.pc`
  would hand every consumer `-L/usr/lib/../lib`. With it, the `.pc` file's `toolexeclibdir` is
  plain `${libdir}`.
- `--enable-exec-static-tramp` — Build closures from static trampolines: libffi maps a table of
  trampoline code from its own library file, which it finds through `/proc/<pid>/maps`, read-only and
  executable, beside a writable table of their parameters, so that making a closure needs no page
  that is both writable and executable. It is upstream's default on x86-64 Linux; the switch names
  it.
- `--enable-portable-binary` — When configure chooses its own optimisation flags, tune for the
  build machine's processor with `-mtune` rather than `-march`, so the library still runs on any
  x86-64. configure chooses them only when `CFLAGS` is unset; the chroot exports the tree's
  `CFLAGS`, so in this build libffi is compiled with the
  [release flags](../../../../docs/kdos/05-developer/writing-ports.md#the-release-flags)
  and the switch changes nothing in the result.
