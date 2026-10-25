## About

shairplay is a free implementation of the [AirPlay]{.idx} audio receiver protocol, RAOP, as a
library: `libshairplay` accepts a stream from an Apple device or iTunes, decrypts it and decodes
its ALAC audio, and hands the samples to the program that links it. In KDOS, `kodi` links it as
the target of its AirTunes receiver. The recipe builds a pinned commit of upstream's repository,
named by `_commit` and saved under the port's own version-stamped file name.

## Notes

- **Only the library and its headers are built.** `build.sh` runs `make` in `include` and
  `src/lib` alone. The `shairplay` program, built when `configure` finds libao, reads
  `airport.key` from its working directory, so as an installed command it cannot start; it is
  not built, and the last two lines of `build.sh` check that the library and `raop.h` are in the
  package.
- **The patch is Kodi's.** Upstream's `configure.ac` requires `dns_sd.h`, the Bonjour header,
  whenever the system has `dlopen`; the patch requires it only where there is none. The library
  opens `libdns_sd.so` at run time for its own service advertising, so building it needs no
  Bonjour header, and Kodi advertises AirPlay through its own Avahi client and never calls that
  path. The patch edits `configure.ac`, so `autoreconf -fi` regenerates `configure` after it.
