## About

libewf reads and writes the Expert Witness Compression Format, the `.E01` and `.Ex01` (and SMART
`.s01`) evidence files that forensic acquisition tools produce: a disk image split into segments,
compressed, and carrying hashes of the acquired data. It reads the logical evidence files, `.L01`
and `.Lx01`, as well. The package holds the library and the `ewf`
tools: `ewfacquire` and `ewfacquirestream` to make an image, `ewfinfo`, `ewfverify`, `ewfexport`
and `ewfrecover`, and `ewfmount`, which presents an image as a raw file through FUSE.
[sleuthkit](#port-sleuthkit) and [testdisk](#port-testdisk) open evidence files through it. It is
one of the libyal libraries, as are [libbfio](#port-libbfio), [libvhdi](#port-libvhdi),
[libvmdk](#port-libvmdk) and [libvslvm](#port-libvslvm).

## Flags

- `--with-libcerror=no`, `--with-libcthreads=no`, `--with-libcdata=no`, `--with-libcdatetime=no`,
  `--with-libclocale=no`, `--with-libcnotify=no`, `--with-libcsplit=no`, `--with-libuna=no`,
  `--with-libcfile=no`, `--with-libcpath=no`, `--with-libbfio=no`, `--with-libfcache=no`,
  `--with-libfdata=no`, `--with-libfdatetime=no`, `--with-libfguid=no`, `--with-libfvalue=no`,
  `--with-libhmac=no`, `--with-libcaes=no`, `--with-libodraw=no`, `--with-libsmdev=no`,
  `--with-libsmraw=no` — For each of these libyal support libraries, "no" means "use the local
  version": the copy in the release tarball is compiled into `libewf` and the tools. None of them
  but libbfio is a port, and without the switch configure would link any copy it found installed
  instead. libbfio has a port, but libewf keeps its own: its consumers open an image by file name
  (`libewf_handle_open()` in sleuthkit), so no libbfio object crosses into libewf from outside.
- `--enable-multi-threading-support`, `--with-pthread` — Build the thread-safe library on POSIX
  threads. Both are upstream's defaults when pthreads is found.
- `--enable-openssl-evp-md`, `--with-openssl` — Compute the MD5, SHA-1 and SHA-256 hashes of the
  image data with OpenSSL's EVP digests instead of the bundled libhmac's own code.
- `--enable-openssl-evp-cipher` — Do AES with OpenSSL's EVP ciphers in the bundled libcaes.
- `--with-zlib`, `--with-adler32=zlib` — Compress and decompress with [zlib](#port-zlib), and take
  the Adler-32 checksums from zlib rather than from libewf's own implementation.
- `--with-bzip2` — Build libewf's bzip2 chunk compression on [bzip2](#port-bzip2), for `Ex01` images
  whose chunks are bzip2-compressed. Upstream's README lists bzip2 compression of `Ex01` as not supported, a work in progress.
- `--with-libuuid=/usr` — Generate image GUIDs with util-linux's `libuuid`; `ewfacquire` writes one
  into each image it makes. The switch names a directory rather than `yes` because the pkg-config
  branch of this release's `libuuid.m4` links nothing.
- `--with-libfuse` — Build `ewfmount` on libfuse. The tree's [fuse](#port-fuse) is libfuse 3, which
  configure finds, and which `ewfmount` can use only with the patch below.
- `--disable-rpath` — Link the tools without an `RPATH` to `/usr/lib`.
- `--disable-python` — Build no `pyewf` module; upstream's default.

## Patches

- `fuse3.patch` — Ports `ewfmount` to the libfuse 3 API. configure detects libfuse 3 and defines `HAVE_LIBFUSE3`, but the sources test only `HAVE_LIBFUSE` and `HAVE_LIBOSXFUSE`, so without the patch `ewfmount` builds with no FUSE support at all. The patch adds `HAVE_LIBFUSE3` to each of those guards, sets `FUSE_USE_VERSION` to 30 for it, creates the session with `fuse_new()` and then `fuse_mount()` instead of libfuse 2's channel calls, unmounts with `fuse_unmount()` before `fuse_destroy()`, and adds the extra arguments libfuse 3 passes to `getattr`, `readdir` and the directory filler.

## Notes

The loop after `configure` reads `common/config.h` and stops the build unless pthreads, zlib,
bzip2, libcrypto, `uuid_generate_random` and libfuse 3 were all found. configure treats every one
of them as optional and would otherwise build a smaller libewf without saying so.

`rm -f $PKG/usr/lib/*.la` removes the libtool archive the install leaves behind.
