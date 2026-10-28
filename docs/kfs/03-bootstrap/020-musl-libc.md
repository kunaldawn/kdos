# musl libc

[musl]{.idx} is KDOS's C library: the standard C functions, the POSIX interfaces, the thread library and
the [dynamic linker]{.idx}, all in one `libc.so`. With it in `build/fs`, the cross compiler can link a
complete program for the first time, and every later step depends on that. This page shows the
commands of `script/phases/10_bootstrap/020_musl_libc.sh`, explains its switches, and lists what it
installs.

**Source:** the archive of the [musl](#port-musl) port\
**Installs into:** `build/fs/usr` and the dynamic linker's link in `build/fs/lib`\
**Approximate build time:** 0.1 SBU

## Installation of musl

<!-- kfs:include script/phases/10_bootstrap/020_musl_libc.sh lines=23-31 pin=b9246aeec644 -->

musl's `configure` is its own short script, not autoconf, and takes its options in its own way.

### The meaning of the configure options

`CROSS_COMPILE=$KDOS_TARGET-`
: The prefix of the tools to build with. musl runs `x86_64-kdos-linux-musl-gcc`,
  `x86_64-kdos-linux-musl-ar` and `x86_64-kdos-linux-musl-ranlib`, the cross toolchain of
  [Part II](../02-cross/intro.md). musl has no `--host`; this variable is how it is cross-compiled.

`--prefix=/usr`
: Install the library in `/usr/lib` and its headers in `/usr/include`.

`--syslibdir=/lib`
: Where the dynamic linker goes. The path, `/lib/ld-musl-x86_64.so.1`, is written into every
  dynamically linked program as its interpreter, so it must never move. `/lib` is musl's default,
  so this states it. On KDOS `/lib` is a link to `/usr/lib` (see [The file system](000-file-system.md#merged-usr)),
  and the dynamic linker is a link to `libc.so` itself.

The `CFLAGS` of the environment, `-O2 -pipe -std=gnu99` from `script/env/host.env`, reach musl's
configure, and because they name an optimisation level, musl uses them as they are and compiles
none of its components at `-O3`. They come after musl's own flags on every compile line, so
`-std=gnu99` replaces the `-std=c99` musl asks for. See below for the port, which differs here.

`make DESTDIR=$SYSROOT install` puts the files under `build/fs` while the paths compiled into the
library stay `/usr` and `/lib`.

## Contents of musl

**Installed libraries:** [`libc.so`]{.idx}, which is also the dynamic linker, and [`libc.a`]{.idx}, in
`/usr/lib`; [`ld-musl-x86_64.so.1`]{.idx} in `/lib`, a link to `libc.so`. musl puts all of its functions in
`libc`, and installs [`libcrypt.a`]{.idx}, [`libdl.a`]{.idx}, [`libm.a`]{.idx}, [`libpthread.a`]{.idx}, [`libresolv.a`]{.idx}, [`librt.a`]{.idx},
[`libutil.a`]{.idx} and [`libxnet.a`]{.idx} as empty archives, so that a link line naming `-lm` or `-lpthread`
still succeeds.

**Installed start files:** [`crt1.o`]{.idx}, `Scrt1.o`, [`rcrt1.o`]{.idx}, [`crti.o`]{.idx} and [`crtn.o`]{.idx} in `/usr/lib`,
the code the linker places around `main`: `crt1.o` for a program that is not position-independent,
`Scrt1.o` for a [PIE]{.idx}, `rcrt1.o` for a static PIE.

**Installed headers:** the C library's headers in `/usr/include`, beside the kernel's.

## Built again later

The [musl](#port-musl) port, in `20_selfhost`, rebuilds musl inside the chroot with the bootstrap's
native compiler, and kpkg adopts the files this step installed. It differs in three ways:

- It gives no `CROSS_COMPILE` and no `--syslibdir`. It builds natively, and the dynamic linker's
  place is the same default.
- It passes `--enable-optimize`, so musl compiles its internal code, `malloc` and its string
  functions at `-O3`, which the plain configure above does not.
- It installs `/usr/sbin/ldconfig`, a script that does nothing and exits 0. musl's dynamic linker
  keeps no cache to rebuild, and the script answers the makefiles and install scripts that run
  `ldconfig` after installing a library.

## See also

- [Part III](intro.md): the bootstrap
- [Linux API headers](010-linux-headers.md): the headers musl is compiled against
- [libstdc++](030-libstdcxx.md): the next library, built on this one
- [How KDOS differs](../../kdos/01-philosophy/how-kdos-differs.md): what building on musl means for the system

<!-- book-nav -->
---

*Part III. The bootstrap, chapter 10.* Previous: [9. Linux API headers](010-linux-headers.md) · [Contents](../README.md) · Next: [11. libstdc++](030-libstdcxx.md)
