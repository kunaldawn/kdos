## About

[DSDA-Doom]{.idx} is a Doom source port from the PrBoom+ line, aimed at speedrunners: it records
and plays back demos exactly, and supports the Boom, MBF and MBF21 extensions and UMAPINFO, with
a software renderer and an OpenGL one. With [freedoom](#port-freedoom) in `depends`, the Freedoom
IWADs in `/usr/share/games/doom`, where the engine looks for WAD files and where it installs its
own `dsda-doom.wad`, give it a game to play.

## Flags

- `-DENABLE_PACKAGING=OFF` — do not set up upstream's CPack packaging targets, which are on by
  default when the project is built on its own.
- `-DSTRICT_FIND=ON` — make every optional dependency required: each `find_package` for an
  optional library is given `REQUIRED`, so a missing library fails the configure instead of
  dropping a music or image format without a word.
- `-DWITH_FLUIDSYNTH=ON` — with `-DWITH_IMAGE=ON`, `-DWITH_MAD=ON`, `-DWITH_PORTMIDI=ON`,
  `-DWITH_VORBISFILE=ON` and `-DWITH_XMP=ON`: MIDI music through [fluidsynth](#port-fluidsynth),
  PNG and other image formats through [sdl2-image](#port-sdl2-image), MP3 music through
  [libmad](#port-libmad), MIDI out to a device through [portmidi](#port-portmidi), Ogg Vorbis
  music through [libvorbis](#port-libvorbis), and tracker music through
  [libxmp](#port-libxmp). Each is on by default and means "use it if available"; with
  `STRICT_FIND` each is required.

## Notes

The CMake project is in the `prboom2` directory of the source, hence `-S prboom2`.

Upstream installs a desktop entry and a scalable SVG icon. The desktop's icon lookup reads PNGs
only (see
[Desktop entries belong to the port](../../../../docs/kdos/05-developer/writing-ports.md#desktop-entries-belong-to-the-port)),
so `build.sh` renders a 256-pixel PNG from the SVG with `rsvg-convert` from
[librsvg](#port-librsvg), and writes its own `dsda-doom.desktop` over upstream's, adding
`StartupWMClass=dsda-doom`, the window's Wayland app id, which is the program's name. The entry
keeps upstream's claim on `application/x-doom-wad`, the shared MIME database's type for `*.wad`,
so a WAD file opens in the game.
