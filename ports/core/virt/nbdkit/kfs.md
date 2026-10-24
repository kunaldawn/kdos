## About

nbdkit is a Network Block Device server built from plugins, each a source of disk data (a file,
memory, a URL, an SSH or NFS path, a disk image built on the fly), and filters stacked over them.
In KDOS [libvirt](#port-libvirt) runs it to serve network disks to guests. configure builds most
plugins and filters whenever it finds their library, and only warns when it does not, even for a
switch given as `--with`. `build.sh` names nearly every switch: those turned off stay off whatever
the build root holds, and the libraries of those turned on are in `depends`.

## Flags

- `--disable-valgrind` — Do not compile in Valgrind's client request, with which the server checks
  whether it runs under Valgrind and then keeps plugins loaded at exit so Valgrind's reports can
  name their functions.
- `--disable-probes` — No USDT tracing probes. They need `sys/sdt.h` from systemtap, which is not a
  port; configure turns them on by default on Linux.
- `--disable-perl`, `--disable-python`, `--disable-ocaml`, `--disable-rust`, `--disable-tcl`,
  `--disable-lua`, `--disable-golang` — The language plugins, each of which embeds an interpreter
  or a runtime so that plugins can be written in that language. All of them are left out.
- `--disable-torrent` — The BitTorrent plugin, which links libtorrent-rasterbar. Off, so that
  library does not end up beneath libvirt.
- `--disable-vram` — The plugin that serves a disk from a graphics card's memory through OpenCL.
- `--disable-vddk` — The VMware VDDK plugin, which loads VMware's proprietary library at run time.
- `--disable-libguestfs-tests`, `--without-libguestfs` — libguestfs, which is not a port, for the
  test suite and for the guestfs plugin.
- `--enable-linuxdisk` — The plugin that builds an ext2 disk image from a directory, running
  `mke2fs -d` from [e2fsprogs](#port-e2fsprogs).
- `--with-gnutls` — TLS through GnuTLS.
- `--with-iconv` — Use iconv, which on musl is part of the C library. The `floppy` plugin
  converts file names with it when it writes the long names of a FAT image.
- `--with-curl`, `--with-ssh`, `--with-nfs`, `--with-iso` — The plugins that serve a disk from a
  URL that curl can fetch, an SSH server (libssh), an NFS export (libnfs), and an ISO image
  made from a directory, which runs `xorriso` from [libisoburn](#port-libisoburn).
- `--with-zlib`, `--with-bzip2`, `--with-liblzma` — The filters that serve a gzip-, bzip2-, xz- or
  lzip-compressed image, and zlib for the deflate clusters of the `qcow2dec` filter.
- `--with-libzstd` — `allocator=zstd`, with which the `memory` and `data` plugins keep their disk
  compressed in RAM, and the zstd clusters of `qcow2dec`.
- `--without-zlib-ng` — Decompress `qcow2dec`'s deflate clusters with zlib rather than zlib-ng.
- `--with-libnbd` — The `nbd` plugin, which proxies another NBD server through
  [libnbd](#port-libnbd).
- `--with-ext2` — The `ext2` filter, which serves one file from inside an ext2, ext3 or ext4
  image, through e2fsprogs' `libext2fs`.
- `--with-manpages` — Build the manual pages, from POD with perl.
- `--without-libvirt` — The libvirt plugin. libvirt's own nbdkit backend runs this server, and
  linking the two in both directions is a dependency cycle.
- `--without-libblkio`, `--without-selinux` — libblkio and SELinux, neither of which is a port.
- `--with-bash-completions` — Install the bash completions, into the directory
  [bash-completion](#port-bash-completion)'s pkg-config file names.
