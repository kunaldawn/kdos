## About

libvpx is the reference VP8 and VP9 encoder and decoder from the WebM project. [ffmpeg](#port-ffmpeg),
the GStreamer plugins and the browsers' media stacks link it. It is BSD-licensed, so linking it
leaves ffmpeg's licence as it is, which the GPL encoders x264 and x265 do not.

## Flags

- `-flto=auto` — Link-time optimisation, in `CFLAGS`, `CXXFLAGS` and `LDFLAGS`. libvpx is one of
  the media libraries the tree builds with LTO, and its configure has no switch of its own for it
  (see [Every port](../../../../docs/kdos/05-developer/writing-ports.md#every-port)).
- `-frandom-seed=libvpx` — Fix the names GCC gives each object's LTO sections, which are otherwise
  random, so the build is reproducible.
- `--as=nasm` — Assemble with nasm. The default, `auto`, prefers yasm whenever it is installed, so
  the assembler would follow what earlier ports happened to install; [nasm](#port-nasm) is the one
  in `depends`.
- `--enable-shared` — Build `libvpx.so`. libvpx's configure builds only the static library unless
  asked.
- `--enable-pic` — Compile position-independent code, which a shared library on x86-64 needs.
- `--enable-vp8`, `--enable-vp9` — Both codecs, encoder and decoder. Upstream builds both by default;
  naming them keeps the set the same on every build.
- `--enable-postproc` — The decoder's post-processing filters (deblocking and noise reduction),
  which configure turns on by default only on x86 targets.
- `--enable-vp9-highbitdepth` — VP9 at 10 and 12 bits per sample, the profiles HDR VP9 uses. Off
  by default upstream.
- `--disable-examples` — Leave out the example encoder and decoder programs, such as `vpxenc`
  and `vpxdec`, which upstream builds and installs by default.
- `--disable-unit-tests` — Leave out the unit-test programs. Running them needs test vectors that
  the `testdata` target downloads.
- `--disable-docs` — Leave out the API reference, which needs doxygen.
