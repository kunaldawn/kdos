# libstdc++

[libstdc++]{.idx} is GCC's C++ standard library. The cross GCC of [Part II](../02-cross/gcc.md) was built
without it, because the library needs a C library and there was none. With musl in `build/fs`, this
step builds it from the same GCC source, so that the C++ programs of the bootstrap, the native GCC
among them, can be compiled and linked. This page shows the commands of
`script/phases/10_bootstrap/030_libstdc++.sh` and explains each switch, including the one that
puts its headers outside the root.

**Source:** the archive of the [gcc](#port-gcc) port\
**Installs into:** `build/fs/usr/lib`, and its headers into `build/cross`\
**Approximate build time:** 1.4 SBU

## Installation of libstdc++

<!-- kfs:include script/phases/10_bootstrap/030_libstdc++.sh lines=24-43 pin=e988f112532c -->

`get_port_version` reads the [`gcc`]{.idx} recipe's version, which names GCC's versioned header directory.
The configure script run is the one in `libstdc++-v3/`, the library's own directory inside the GCC
source: only the library is built, not the compiler. `make install` builds what it installs, so no
separate `make` comes first.

### The meaning of the configure options

`--host=$KDOS_TARGET`
: The library runs on KDOS, so it is compiled by the cross compiler,
  `x86_64-kdos-linux-musl-g++`, against musl.

`--build=$(../config.guess)`
: The machine the build runs on, the container, as GCC's own `config.guess` names it.

`--prefix=/usr`
: Install the library under `/usr`.

`--disable-multilib`, `--disable-nls`
: One ABI and no message catalogues, as for the [cross compiler](../02-cross/gcc.md#the-meaning-of-the-configure-options).

`--disable-libstdcxx-pch`
: Do not build precompiled headers. They are large, and they only make C++ compiles faster.

`--with-gxx-include-dir=/../cross/$KDOS_TARGET/include/c++/$GCC_VER`
: Where the C++ headers go. Under `DESTDIR=$SYSROOT` the path becomes
  `build/fs/../cross/x86_64-kdos-linux-musl/include/c++/<version>`, which is
  `build/cross/x86_64-kdos-linux-musl/include/c++/<version>`: the directory in which the cross
  `g++` looks for its standard headers. The headers are for that compiler, so they go with it, and
  the root gets its own copy from the native GCC in step 100. The path works because
  `script/env/host.env` puts `build/cross` beside `build/fs`.

## Removing the libtool archives

The last command deletes the libtool archives, `libstdc++.la`, `libstdc++exp.la`,
`libstdc++fs.la` and `libsupc++.la`. Each records paths from this build, which libtool would copy
into every library later linked through it, and no package will ever own them, so nothing else
would remove them (see [Part III](intro.md#what-the-bootstrap-leaves-behind)).

The path is `/usr/lib64`. Without the `gcc` port's `t-linux64-lib.patch`, which only the native
builds apply, GCC's library directory on x86-64 is `lib64`. The `fs/` copy of step 000 made
`/usr/lib64` a link to `lib`, so the files are in `/usr/lib` all the same. `rm -v` has no `-f`, so
the step fails if any of the four is missing, which it would be only if the install had gone
somewhere else.

## Contents of libstdc++

**Installed libraries:** [`libstdc++.so`]{.idx} and [`libstdc++.a`]{.idx}, the C++ standard library; [`libsupc++.a`]{.idx},
the language support half alone (exceptions, RTTI, `new` and `delete`); [`libstdc++exp.a`]{.idx} and
[`libstdc++fs.a`]{.idx}, the experimental and filesystem extensions.

**Installed headers:** `build/cross/x86_64-kdos-linux-musl/include/c++/<version>`, outside the root.

## Built again later

libstdc++ has no port of its own. It is built again as part of GCC, twice:

- The [native GCC](100-gcc.md) of step 100 builds it with the compiler and installs it into
  `build/fs`, with its headers in `/usr/include/c++/<version>` in the root.
- The [gcc](#port-gcc) port, in `20_selfhost`, does the same inside the chroot. It applies
  `t-linux64-lib.patch`, so its libraries go to `/usr/lib` directly, and
  `libstdcxx-generic-locale-names.patch`, so `std::locale("")` accepts any locale name musl
  accepts instead of only `C` and `POSIX`, and it moves the
  `libstdc++.so.6.*-gdb.py` pretty-printer hook to `/usr/share/gdb/auto-load/usr/lib`, where gdb
  will load it. kpkg adopts the files this step installed in `/usr/lib` as the `gcc` package's.

## See also

- [Part III](intro.md): the bootstrap
- [Cross GCC, pass 1](../02-cross/gcc.md): the compiler built without this library
- [Native GCC](100-gcc.md): the compiler built with it
- [Syncing fs/](../../kdos/05-developer/build-system.md#syncing-fs): why `/usr/lib64` is a link to `lib`

<!-- book-nav -->
---

*Part III. The bootstrap, chapter 11.* Previous: [10. musl libc](020-musl-libc.md) · [Contents](../README.md) · Next: [12. Ncurses](040-ncurses.md)
