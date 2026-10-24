## About
[HandBrake]{.idx} converts video files and unencrypted DVDs and Blu-rays to MP4, MKV and WebM,
with presets for devices. The port installs both programs: `ghb`, the GTK 4 application, and
`HandBrakeCLI`. Both are built on `libhb`, HandBrake's library, which this build links statically
with the copies of FFmpeg, x265 (8, 10 and 12 bit), SVT-AV1, dav1d, zimg, libdvdread, libdvdnav
and libbluray that HandBrake pins.

## Flags
- `--disable-df-fetch` — Make a contrib archive missing from `download/` a build error instead of
  a download. Each archive is a source of this recipe, copied into `download/` under the name its
  `contrib/*/module.defs` fetches; HandBrake checks each against the sha256 in that file as it
  extracts it.
- `--enable-gtk` — Build the GTK application, `ghb`, as well as `HandBrakeCLI`.
- `--enable-x265` — With `--enable-numa`: the x265 HEVC encoder, built at 8, 10 and 12 bit, with
  its NUMA support through [numactl](#port-numactl). Both are upstream's Linux defaults, named.
- `--disable-fdk-aac` — No FDK AAC encoder. Enabling it builds HandBrake's FFmpeg with
  `--enable-nonfree`, a binary that cannot be redistributed; FFmpeg's own AAC encoder is used.
- `--disable-nvenc` — With `--disable-nvdec`, `--disable-qsv` and `--disable-vce`: no NVIDIA,
  Intel or AMD hardware encoder or decoder. Each needs a vendor SDK that is not a port.
- `--enable-libdovi` — Carry Dolby Vision metadata through a transcode, with
  [libdovi](#port-libdovi). Upstream turns it on when it finds `cargo` and `cargo-c`, and then
  builds its own libdovi with `cargo fetch`, which needs the network; with
  `system-libdovi.patch` the libdovi port is linked instead.
- `CFLAGS="${CFLAGS/-std=gnu11/}"` — Remove the tree's C standard from `CFLAGS`. HandBrake's make
  system adds `CFLAGS` to every C and C++ compile, its own and each bundled library's, and each of
  those names its own standard
  ([C flags handed to the C++ compiler](../../../../docs/kdos/05-developer/build-troubleshooting.md#c-flags-handed-to-the-c-compiler)).

## Patches
- `system-libdovi.patch` — Drops `contrib/libdovi` from the modules `make/include/main.defs` builds when the libdovi feature is on, so no libdovi is fetched or compiled, and adds the pkg-config module `dovi` to the libraries `HandBrakeCLI` links (`test/module.defs`). The GTK build already takes `dovi` from pkg-config in `gtk/meson.build`.

## Notes
- **Why the bundled libraries.** HandBrake's FFmpeg carries patches of its own (MP4 and MOV
  metadata, DVD and PGS subtitles, Dolby Vision RPU side data among them) that the
  [ffmpeg](#port-ffmpeg) port does not have, and `libhb` is written against that one release.
  The other contribs are pinned the same way; the system's copies of them are not used.
- **The menu icon.** Upstream installs only the scalable icon and keeps the raster sizes in its
  tree; `build.sh` installs the 32, 128 and 256 pixel PNGs, which the panel reads.
- **The menu entry is replaced** to add `StartupWMClass=fr.handbrake.ghb`, the application's
  Wayland app_id, which upstream's entry lacks. It claims no MIME type: a transcoder is opened on
  purpose, and the video types belong to the players.
