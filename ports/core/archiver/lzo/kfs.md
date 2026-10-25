## About
LZO is a real-time compression library: it favours speed over compression ratio, and
decompression speed over compression speed. It is a plain C library with headers under
`/usr/include/lzo` and one library, `liblzo2`.

KDOS builds it so that other programs can read and write LZO data: [libarchive](#port-libarchive)
links it for `lzop` archives, [squashfs-tools](#port-squashfs-tools) and
[btrfs-progs](#port-btrfs-progs) for file systems compressed with it, and [qemu](#port-qemu) and
[openvpn](#port-openvpn) link it as well. Because libarchive depends on it and
[cmake](#port-cmake) links libarchive, LZO is built early in `30_foundation`, well before its own
shelf's place in the list.

## Flags
- `--enable-shared` — Build `liblzo2.so` as well as the static `liblzo2.a`. LZO's configure builds
  only the static library unless this is given (its `INSTALL` says so), and every consumer here
  links the shared one. `--disable-static` is not passed, so the static archive is installed too.
