## About
[libogg]{.idx} reads and writes the Ogg container, the bitstream that carries Vorbis, Opus, FLAC
and Theora streams in `.ogg`, `.oga`, `.opus` and `.ogv` files. It is the bottom of the audio
codec stack: [libvorbis](#port-libvorbis), [flac](#port-flac) (for Ogg FLAC),
[opusfile](#port-opusfile), [libsndfile](#port-libsndfile), [ffmpeg](#port-ffmpeg) and the
GStreamer plugin packages build on it, and with them most of the players, editors and games of the
tree.

Its build is the plain autotools shape, with nothing of its own to explain.
