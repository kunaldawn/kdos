## About
[opusfile]{.idx} decodes and seeks `.opus` files: it reads the Ogg pages, the Opus headers and the
comment tags, and hands back PCM through [opus](#port-opus). It is the read half of the Opus file
format, as [libopusenc](#port-libopusenc) is the write half: `opusdec` and `opusinfo` in
[opus-tools](#port-opus-tools) link it, and so do [sdl2-mixer](#port-sdl2-mixer),
[sdl3-mixer](#port-sdl3-mixer), [sox](#port-sox), [cmus](#port-cmus) and
[audacity](#port-audacity).

## Flags
- `--disable-http` — build `libopusurl`, opusfile's URL reader, for `file:` URLs only. Without the
  switch it also reads `http://` and `https://` streams, and configure then requires
  [openssl](#port-openssl), which is not in `depends`. `libopusurl` and `opusurl.pc` are installed
  either way.
- `--disable-doc` — with `--disable-examples`: no Doxygen API reference and no example programs.
