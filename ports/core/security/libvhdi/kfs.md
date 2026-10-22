## About

libvhdi reads Microsoft's Virtual Hard Disk images, both VHD and VHDX, in their fixed, dynamic
(sparse) and differencing forms. The package holds the library, `vhdiinfo`, which prints an
image's metadata, and `vhdimount`, which presents the disk inside an image as a raw file through
FUSE. [sleuthkit](#port-sleuthkit) links it to open `.vhd` and `.vhdx` images directly. It is a
libyal library, built the same way as [libewf](#port-libewf), [libvmdk](#port-libvmdk) and
[libvslvm](#port-libvslvm).

## Flags

- `--with-libbfio` — Link the [libbfio](#port-libbfio) port for file input and output, instead of
  the copy of libbfio the tarball carries; the check after `configure` fails the build unless it
  was found.
- `--with-libcerror=no`, `--with-libcthreads=no`, `--with-libcdata=no`, `--with-libclocale=no`,
  `--with-libcnotify=no`, `--with-libcsplit=no`, `--with-libuna=no`, `--with-libcfile=no`,
  `--with-libcpath=no`, `--with-libfcache=no`, `--with-libfguid=no`, `--with-libfdata=no` — For
  each of these libyal support libraries, "no" means "use the local version": the copy in the
  release tarball is compiled in. None of them is a port, and without the switch configure would
  link any copy it found installed instead.
- `--with-libfuse` — Build `vhdimount` on [fuse](#port-fuse), which is libfuse 3; this release's
  mount tool supports libfuse 3 itself.
- `--enable-multi-threading-support`, `--with-pthread` — Build the thread-safe library on POSIX
  threads.
- `--disable-rpath` — Link the tools without an `RPATH` to `/usr/lib`.
- `--disable-python` — Build no `pyvhdi` module; upstream's default.

## Notes

The loop after `configure` reads `common/config.h` and stops the build unless multi-threading,
libbfio and libfuse 3 were all found. configure treats each as optional and would otherwise build
without it and say nothing.
