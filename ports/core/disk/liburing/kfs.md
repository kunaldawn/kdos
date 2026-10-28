## About

liburing is the user-space library for [io_uring]{.idx}, the kernel's asynchronous I/O interface: a
program queues reads, writes, accepts and other operations into a ring of submission entries shared
with the kernel, and collects the results from a completion ring, with few or no system calls per
operation. It wraps the setup and the ring handling, and is what [qemu](#port-qemu),
[samba](#port-samba), [postgresql](#port-postgresql), [fuse](#port-fuse), [ubdsrv](#port-ubdsrv) and
the other ports that list it use to reach io_uring. The package installs `liburing.so`,
`liburing-ffi.so` (the same interface with the inline helpers compiled in, for callers that cannot
use C inline functions) and the section 2, 3 and 7 manual pages.

## Flags

- `--use-libc` — Builds liburing against the C library. Without it, configure builds a `nolibc`
  liburing on x86-64: compiled with `-nostdlib -ffreestanding -fno-builtin -fno-stack-protector`,
  making its system calls itself. Upstream's `--help` gives hardening as the reason to pass it; with
  it the library is compiled with the tree's own flags, the stack protector and fortified calls
  among them.
- `--mandir=/usr/share/man` — configure's default is `$prefix/man`, `/usr/man`.

## Notes

The recipe passes no `--prefix`: configure's defaults are already `/usr` and `/usr/lib`, and
`--prefix` is resolved with `realpath -s`, an option not every `realpath` accepts. The top-level
Makefile runs `./configure` itself, with no options at all, whenever `config-host.mak` is missing,
so `make` must run only after a configure that succeeded; `build.sh` runs under `set -e`, which stops
it at a failed configure first. The recipe runs `make library`, which builds only `src/`, not the
tests and examples `make all` builds.
