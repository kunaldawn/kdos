## About

libvslvm reads the Linux Logical Volume Manager (LVM2) format from an image or device: the volume
group metadata on a physical volume and the logical volumes inside it. The package holds the
library, `vslvminfo`, which lists the volume group and its volumes, and `vslvmmount`, which
presents each logical volume as a raw file through FUSE. [sleuthkit](#port-sleuthkit) links it to
find and read the file systems on logical volumes inside a disk image. Upstream's README lists
what it does not read: more than one physical volume, more than one segment, non-linear stripes,
and snapshots. It is a libyal library, built the same way as [libewf](#port-libewf),
[libvhdi](#port-libvhdi) and [libvmdk](#port-libvmdk).

## Flags

- `--with-libbfio` — Link the [libbfio](#port-libbfio) port for file input and output, instead of
  the copy of libbfio the tarball carries. sleuthkit opens a volume group by building a
  `libbfio_pool_t` over its own image with libbfio and handing it to
  `libvslvm_handle_open_physical_volume_files_file_io_pool()`, so libvslvm and sleuthkit must use
  one and the same libbfio. The check after `configure` fails the build unless it was found.
- `--with-libcerror=no`, `--with-libcthreads=no`, `--with-libcdata=no`, `--with-libclocale=no`,
  `--with-libcnotify=no`, `--with-libcsplit=no`, `--with-libuna=no`, `--with-libcfile=no`,
  `--with-libcpath=no`, `--with-libfcache=no`, `--with-libfvalue=no`, `--with-libfdata=no` — For
  each of these libyal support libraries, "no" means "use the local version": the copy in the
  release tarball is compiled in. None of them is a port, and without the switch configure would
  link any copy it found installed instead.
- `--with-libfuse` — Build `vslvmmount` on [fuse](#port-fuse), which is libfuse 3; this release's
  mount tool supports libfuse 3 itself.
- `--enable-multi-threading-support`, `--with-pthread` — Build the thread-safe library on POSIX
  threads.
- `--disable-rpath` — Link the tools without an `RPATH` to `/usr/lib`.
- `--disable-python` — Build no `pyvslvm` module; upstream's default.

## Notes

The loop after `configure` reads `common/config.h` and stops the build unless multi-threading,
libbfio and libfuse 3 were all found. configure treats each as optional and would otherwise build
without it and say nothing.
