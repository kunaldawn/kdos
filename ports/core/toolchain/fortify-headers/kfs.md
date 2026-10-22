## About

fortify-headers is a [_FORTIFY_SOURCE]{.idx} implementation that needs no help from the C library.
glibc carries its own; musl has none. These headers sit in front of musl's: each one, such as
`string.h`, first includes musl's header of the same name with `#include_next`, then wraps the
functions it checks (`memcpy`, `strcpy`, `read`, `snprintf` and the rest) in inline versions that
compare the length against the size of the destination the compiler can see. A call that would overrun such an
object runs a trap instruction and the program dies with `SIGILL` instead of writing past the end.

KDOS turns it on for every port: from `20_selfhost` on, `script/env/chroot.env` compiles with
`-D_FORTIFY_SOURCE=2 -isystem /usr/include/fortify`. That is why this port is the first one
`20_selfhost` installs, ahead even of musl: a port compiled before the headers exist is compiled
unchecked. What the checks cover, what they cost, and which builds escape them is in
[The release flags](../../../../docs/kdos/05-developer/writing-ports.md#the-release-flags).

The port compiles nothing. `build.sh` applies the two patches and copies `include/*.h` and
`include/sys/*.h` into `/usr/include/fortify`, the same files upstream's `make install` would put
under its own prefix.

## Patches

- `late-feature-macro.patch` — adds a plain prototype in front of each wrapper whose function a feature-test macro gates: `realpath` in `stdlib.h`, `mempcpy`, `strlcat` and `strlcpy` in `string.h`, `getdomainname` and `swab` in `unistd.h`. The wrapper then never defines a function that musl's header, read under different feature macros, left undeclared, and a source that defines `_GNU_SOURCE` after its first system header still compiles.
- `lto-no-always-inline.patch` — drops `__always_inline__` from the GCC definition of `_FORTIFY_INLINE`, the attribute set every wrapper is declared with, and keeps `__gnu_inline__` and `__artificial__`; the clang definitions are untouched. When a program takes the address of a fortified function and link-time optimisation later turns that into a direct call, GCC cannot inline an `always_inline` body there and stops the link. Without the attribute that one call goes to musl's function unchecked, and every direct call in an optimised build is still inlined and checked. The case and the example, libstdc++'s `std::to_string` passing `vsnprintf`, are in [The release flags](../../../../docs/kdos/05-developer/writing-ports.md#the-release-flags).

## Notes

The checks work only in optimised code, since they read object sizes the optimiser works out. A
makefile that replaces `CFLAGS` with a line of its own drops `-isystem /usr/include/fortify` with
it and builds unchecked, without an error.

The programs `10_bootstrap` compiles are built before this port is installed and carry no
fortification. Later phases replace most of them with their ports; `kpkg`, which no port builds,
stays the copy `10_bootstrap` compiled.
