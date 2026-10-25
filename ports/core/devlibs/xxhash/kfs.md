## About

xxHash is a family of very fast non-cryptographic hash functions (XXH32, XXH64 and the XXH3
variants), shipped as `libxxhash`, its headers and the `xxhsum` command. It is in `20_selfhost`
because [binutils](#port-binutils) is configured `--with-xxhash`; no phase list names it, and the
phase installs it as binutils' dependency. binutils' `ld` compiles the hash in from `xxhash.h`
(`XXH_INLINE_ALL`), so it needs the header only while it is built; [ccache](#port-ccache),
[gdb](#port-gdb) and other later ports link `libxxhash.so`.

The build is upstream's plain `Makefile`, with no configure step: the exported `CFLAGS` and
`LDFLAGS` reach it through the environment, and `PREFIX` and `DESTDIR` place the install.

## Flags

- `${CFLAGS/-O2/-O3}` — raise the optimisation level of the exported `CFLAGS` to `-O3`. The
  Makefile's own default is `-O3`, but only when `CFLAGS` is unset, and the tree always sets it. This
  is the "`-O3` for a hot port by pattern" case of
  [Every port](../../../../docs/kdos/05-developer/writing-ports.md#every-port).
- `DISPATCH` — build `xxhsum` with `xxh_x86dispatch.c`, which checks the processor when the program
  starts and runs the XXH3 code for the widest vector unit it has: SSE2, AVX2 or AVX-512. On
  x86-64, where SSE2 is always present, the dispatcher leaves the scalar code out.
  The Makefile already does this when the compiler targets x86; the variable states it.
- `LIBXXH_DISPATCH` — put the same dispatcher into `libxxhash` and install its header,
  `xxh_x86dispatch.h`, so programs that link the library get the same run-time choice. Upstream
  leaves this off.

Both are set from `uname -m`: `1` on x86 and x86-64, `0` elsewhere, because the dispatcher is x86
code and its source stops the build with an `#error` on any other architecture. Choosing the vector
code at run time is how the package uses AVX2 and AVX-512 while the tree compiles for the baseline
x86-64 (see [Every port](../../../../docs/kdos/05-developer/writing-ports.md#every-port), "No
machine-specific code").
