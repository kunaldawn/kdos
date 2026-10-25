## About
[libvorbis]{.idx} is the reference Vorbis audio codec: `libvorbis`, with the `libvorbisenc`
encoder and the `libvorbisfile` reader on top, over [libogg](#port-libogg). It is the Vorbis
encoder and decoder of [ffmpeg](#port-ffmpeg), [libsndfile](#port-libsndfile), the GStreamer
plugins, [sdl2-mixer](#port-sdl2-mixer) and [sdl3-mixer](#port-sdl3-mixer), and through them of
most of the players and games of the tree. The `oggenc` and `oggdec` commands are in
[vorbis-tools](#port-vorbis-tools).

## Flags
- `${CFLAGS/-O2/-O3}` — raise the exported `CFLAGS` and `CXXFLAGS` from `-O2` to `-O3`. libvorbis's
  `configure` sets `-O3` itself for GCC on Linux, with `-ffast-math` among its flags, but puts
  the caller's `CFLAGS` after its own, so the tree's `-O2` would win. Raised, the codec builds at
  the level upstream chose; this is the "`-O3` for a hot port by pattern" case of
  [Every port](../../../../docs/kdos/05-developer/writing-ports.md#every-port).
