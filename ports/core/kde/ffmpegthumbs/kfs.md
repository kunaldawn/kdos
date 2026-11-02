## About

ffmpegthumbs is the KIO thumbnailer for video files: it decodes a frame from a little way into the
video with [ffmpeg](#port-ffmpeg) and draws a film-strip border on it. Dolphin and the file dialog
show it as the file's preview.

## Flags

- `-D BUILD_FUZZERS=OFF` — No fuzzing targets. Upstream's default, named.

## Notes

The tarball has no translations, so `build.sh` passes no translation switch.
