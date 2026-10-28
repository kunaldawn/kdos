## About

The Sleuth Kit is a library and a set of command-line tools for file-system forensics: `mmls`
lists the partitions in a disk image, `fls` lists the files of a file system, including deleted
ones, `icat` copies a file out by its metadata address, and `tsk_recover`, `sigfind`, `fiwalk` and
the rest build on the same library. It never asks the kernel to mount anything. It reads the
metadata straight out of the image, so a file system too damaged for `mount` to accept still gives
up its files; [testdisk](#port-testdisk), by contrast, repairs a partition table so that the
kernel can mount it.

Images are opened through the image-format libraries the tree carries: raw and split raw images
and devices directly, E01 through [libewf](#port-libewf), AFF through [afflib](#port-afflib), VHD
and VHDX through [libvhdi](#port-libvhdi), VMDK through [libvmdk](#port-libvmdk), and an LVM
volume group inside an image through [libvslvm](#port-libvslvm) and [libbfio](#port-libbfio).

## Flags

- `export CFLAGS="$CFLAGS -Wno-error"`, `export CXXFLAGS="$CXXFLAGS -Wno-error"` — Turn upstream's
  `-Werror` (in `tsk/util` and `tsk/pool`) back into warnings. `tsk/fs/tsk_fs_i.h` includes
  `<sys/fcntl.h>`, which on musl is a shim whose whole body is a `#warning` and a redirect to
  `<fcntl.h>`, and with `-Werror` that warning stops the build. Every warning is still printed.
  `CFLAGS` and `CXXFLAGS` come after automake's `AM_CXXFLAGS` on the compile line, so the
  `-Wno-error` wins.
- `export CPPFLAGS="$CPPFLAGS -Dlibewf_handle_read_random=libewf_handle_read_buffer_at_offset"` —
  sleuthkit's `tsk/img/ewf.cpp` calls `libewf_handle_read_random()`, which the
  [libewf](#port-libewf) port does not export; it has `libewf_handle_read_buffer_at_offset()`,
  which takes the same arguments. The define renames the one call at compile time; without it
  `ewf.cpp` fails on an undeclared function.
- `--disable-java` — Build no Java bindings or jar file, the JNI layer Java front ends such as
  Autopsy use; there is no Autopsy port. The bindings' README, `bindings/java/README.txt`, says
  they build with Ant and download the jars `ivy.xml` lists, which an offline build cannot do.
- `--disable-cppunit` — Build without the cppunit tests.
- `--with-afflib`, `--with-libbfio`, `--with-libvhdi`, `--with-libvmdk`, `--with-libvslvm`,
  `--with-libewf`, `--with-zlib` — Ask for each image-format library and zlib. configure still
  leaves one out quietly when it does not link, which is why the `config.h` check below exists.
  libvslvm is only probed when libbfio is found, because sleuthkit hands libvslvm a libbfio pool
  over its own image.
- `LDFLAGS="$LDFLAGS"`, `sigfind_LDFLAGS=` — Given on `make`'s command line, for the build and the
  install alike. `tools/autotools/Makefile.am` appends `-static` to `LDFLAGS`, and
  `tools/srchtools/Makefile.am` sets `sigfind_LDFLAGS = -static`; a variable on the command line
  overrides the makefile's own assignment, so every tool is linked dynamically, with the tree's
  linker flags, against the shared `libtsk` and the shared image libraries.

## Patches

- `lvm-header.patch` — Adds `tsk/pool/tsk_lvm.hpp`, the declaration of the `LVMPool` class, which `tsk/pool/lvm_pool.cpp` and `lvm_pool_compat.hpp` include. The release tarball does not carry the header, which no `Makefile.am` lists, and without it the LVM support, built because libvslvm is found, does not compile.

## Notes

`autoreconf -f -i` regenerates `configure` and every `Makefile.in` with the tree's autotools
before `configure` runs.

The loop after `configure` reads `tsk/tsk_config.h` and stops the build unless each of
`HAVE_LIBEWF`, `HAVE_LIBAFFLIB`, `HAVE_LIBBFIO`, `HAVE_LIBVHDI`, `HAVE_LIBVMDK`, `HAVE_LIBVSLVM`,
`HAVE_LIBZ` and `HAVE_LIBSQLITE3` is defined. sqlite has no switch at all: without a system
[sqlite](#port-sqlite) configure compiles the copy sleuthkit bundles
(`tsk/auto/sqlite3.c`), and the check holds it to the
port.

The last loop runs `readelf -d` over every installed file and fails the build if any names a
directory under `/var/cache`, the build tree, which the package lint also refuses (see
[What the package lint refuses](../../../../docs/kdos/05-developer/writing-ports.md#what-the-package-lint-refuses)).
