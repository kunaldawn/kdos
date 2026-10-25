## About

libvmdk reads VMware's virtual disk images: flat (raw) and sparse VMDK extents, COWD sparse
extents, delta links between a disk and its parent, and compressed grains. The package holds the
library, `vmdkinfo`, which prints an image's descriptor and extents, and `vmdkmount`, which
presents the disk as a raw file through FUSE. [sleuthkit](#port-sleuthkit) links it to open
`.vmdk` images directly. It is a libyal library, built the same way as [libewf](#port-libewf),
[libvhdi](#port-libvhdi) and [libvslvm](#port-libvslvm).

## Flags

- `--with-libbfio` — Link the [libbfio](#port-libbfio) port for file input and output, instead of
  the copy of libbfio the tarball carries; the check after `configure` fails the build unless it
  was found.
- `--with-libcerror=no`, `--with-libcthreads=no`, `--with-libcdata=no`, `--with-libclocale=no`,
  `--with-libcnotify=no`, `--with-libcsplit=no`, `--with-libuna=no`, `--with-libcfile=no`,
  `--with-libcpath=no`, `--with-libfcache=no`, `--with-libfvalue=no`, `--with-libfdata=no` — For
  each of these libyal support libraries, "no" means "use the local version": the copy in the
  release tarball is compiled in. None of them is a port, and without the switch configure would
  link any copy it found installed instead.
- `--with-zlib` — Read the deflate-compressed grains of a VMDK with
  [zlib](#port-zlib).
- `--with-libfuse` — Build `vmdkmount` on [fuse](#port-fuse), which is libfuse 3; this release's
  mount tool supports libfuse 3 itself.
- `--enable-multi-threading-support`, `--with-pthread` — Build the thread-safe library on POSIX
  threads.
- `--disable-rpath` — Link the tools without an `RPATH` to `/usr/lib`.
- `--disable-python` — Build no `pyvmdk` module; upstream's default.

## Notes

The loop after `configure` reads `common/config.h` and stops the build unless multi-threading,
libbfio, libfuse 3 and zlib were all found. configure treats each as optional and would otherwise
build without it and say nothing.
