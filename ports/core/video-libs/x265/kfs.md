## About

x265 is the HEVC (H.265) encoder library and its `x265` command line; [ffmpeg](#port-ffmpeg) and
the GStreamer plugins encode HEVC through it. It is GPL-2.0-or-later, like x264, and what linking
it does to the licence of the shipped ffmpeg is recorded in the ffmpeg port's `LICENSE.notice`, which
the phase list names beside the two encoders.

This build makes one `libx265.so` that carries the 8-, 10- and 12-bit encoders. x265 compiles one
bit depth per configure, so `build.sh` configures three times, in `build-12`, `build-10` and
`build-8`, following upstream's `build/linux/multilib.sh`; the public API of the one library then
selects the encoder for the bit depth a caller asks for. Main10 is the depth of HDR video and of
most HEVC archives, and an 8-bit-only library makes ffmpeg's `libx265` encoder refuse every 10-bit
pixel format.

## Flags

- `-include cstdint` — Include `<cstdint>` at the top of every C++ file. The vendored json11 that
  the HDR10+ code is built on uses `uint8_t` without including the header, and none of the
  libstdc++ headers it does include brings it in, so every depth fails to compile without it.
- `-frandom-seed=x265` — Fix the names GCC gives each object's LTO sections, which are otherwise
  random, so the two static archives the 8-bit link reads come out the same on every build (see
  [Every port](../../../../docs/kdos/05-developer/writing-ports.md#every-port)).
- `-DCMAKE_INTERPROCEDURAL_OPTIMIZATION=ON` — Link-time optimisation, in all three builds. x265 is
  one of the media libraries the tree builds with LTO.
- `-DENABLE_ASSEMBLY=ON` — Use the hand-written assembly primitives, assembled with
  [nasm](#port-nasm). Upstream turns it on for x86-64 only when it finds a recent enough nasm;
  naming it keeps it on, so a missing assembler fails the build instead of making a slow encoder.
- `-DENABLE_HDR10_PLUS=ON` — Build the HDR10+ dynamic-metadata support and its `libhdr10plus.so`.
  Off by default upstream. It is on in all three builds: the metadata belongs to 10-bit video, and
  a depth's encoder can write it only when that depth is compiled with it.
- `-DENABLE_LIBNUMA=OFF` — Upstream's default is to link libnuma whenever it finds it, for thread
  pools placed by NUMA node on multi-socket machines. Off, the library links no libnuma whatever
  the build root holds.
- `-DENABLE_TESTS=OFF` — Leave out upstream's unit-test program.
- `-DHIGH_BIT_DEPTH=ON`, `-DMAIN12=ON` — Store samples in 16 bits, and with `MAIN12` encode 12-bit
  rather than 10-bit. The first build has both, the second only `HIGH_BIT_DEPTH`.
- `-DEXPORT_C_API=OFF` — In the 12- and 10-bit builds, leave out the public C API, whose symbols
  would collide with the 8-bit library's when all three are linked into one; those encoders are
  reached through the 8-bit library's API.
- `-DENABLE_SHARED=OFF`, `-DENABLE_CLI=OFF` — Build the 12- and 10-bit encoders as static archives
  only, with no `x265` program; they exist to be linked into the third build. The third sets
  `-DENABLE_SHARED=ON` and `-DENABLE_CLI=ON`.
- `-DEXTRA_LIB="x265_main10.a;x265_main12.a"` — Link the 10- and 12-bit archives into the 8-bit
  shared library and program. The two names are symbolic links `build.sh` makes in `build-8` to
  each build's `libx265.a`.
- `-DEXTRA_LINK_FLAGS=-L.` — Add the build directory to the library search path, where those links
  are.
- `-DLINKED_10BIT=ON`, `-DLINKED_12BIT=ON` — Tell the 8-bit build that the 10- and 12-bit encoders
  are linked in, so its API hands a request for either depth to them.

## Notes

The final link warns that `x265_analysis_distortion_data` breaks the one-definition rule: its
`ctuDistortion` member points at `sse_t`, which is 32 bits wide at 8-bit depth and 64 above. The
warning is expected. The structure's layout is the same at every depth, and the three encoders
reach one another only through the API table.

`make install` in `build-8` installs a `libx265.a` and a `libhdr10plus.a` as well, and `build.sh`
removes both. Upstream's script merges the three depths into one archive with `ar -M`; this build
does not, so the installed `libx265.a` would be the 8-bit encoder alone, referring to 10- and
12-bit encoders that no installed archive holds. Nothing in the tree links either library
statically.
