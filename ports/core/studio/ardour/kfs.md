## About

Ardour is a digital audio workstation: it records, edits and mixes multitrack audio and MIDI,
hosts LV2 plugins, and automates a mix. Its interface is drawn with YTK, an in-tree fork of GTK 2
whose only Linux back end is X11, so Ardour runs as an Xwayland window. It is built with waf, through
`python3 ./waf`. The installed program and its files carry the major version in their names
(`ardour9`).

## Flags

- `--configdir=/etc` — waf's name for the system configuration directory, the counterpart of
  `--sysconfdir`.
- `--optimize` — an optimized build. Without it Ardour's waf builds a debug build, and a debug
  build that is not given `--keepflags` empties the exported `CFLAGS` and `CXXFLAGS`, so the tree's
  compiler flags would not reach the compile.
- `--freedesktop` — build the freedesktop.org MIME type and desktop files. With this switch the
  configure runs `itstool --version` and stops unless it finds itstool 2 or later; itstool is not
  in `depends` but is installed by the `30_foundation` phase.
- `--no-phone-home` — do not contact ardour.org at start-up for announcements.
- `--no-nls` — no translations; the package is English only under the
  [size and language policy](../../../../docs/kdos/05-developer/writing-ports.md#size-and-language-policy).
- `--with-backends=jack,alsa,pulseaudio,dummy` — the audio back ends: JACK, ALSA, PulseAudio and
  the dummy back end that needs no device. PipeWire serves all three real ones on KDOS (see
  [JACK programs on the host](../../../../docs/kdos/03-architecture/session.md#jack-programs-on-the-host)).
- `--libjack=link` — link the JACK back end against `libjack` directly, which the
  [pipewire](#port-pipewire) port provides, rather than loading it at run time (`weak`). `link` is
  also what `auto` picks on Linux, and the configure's `jack_port_rename()` check runs only in this
  mode.
- `--cxx17` — compile as C++17 (`-std=c++17`). GCC 16 compiles C++ as C++20 by default, and C++20
  types a `u8"..."` literal as `const char8_t[]`, which converts to neither `std::string` nor
  `const char *`; Ardour's sources assign such literals to both (see
  [`u8` literals under C++20](../../../../docs/kdos/05-developer/build-troubleshooting.md#u8-literals-under-c20)).
- `--no-execstack` — do not link with `-z execstack`. Ardour's waf adds it on Linux by default,
  and the package lint refuses an executable stack (see
  [what the package lint refuses](../../../../docs/kdos/05-developer/writing-ports.md#what-the-package-lint-refuses)).
  Upstream's help text warns that this may break some plugins.
- `--noconfirm` — ask no questions during the build.

## Patches

- `musl.patch` — In `libs/tk/ytk/gtksearchenginesimple.c`, the file search of YTK's file chooser, changes the three `#ifdef HAVE_GNU_FTW` tests to also require `FTW_ACTIONRETVAL`. YTK's hard-coded `config.h` defines `HAVE_GNU_FTW` on every system but macOS and NetBSD, but musl's `nftw()` has none of glibc's `FTW_ACTIONRETVAL`, `FTW_STOP` and `FTW_SKIP_SUBTREE`; with the patch the search uses the plain `nftw()` return values there.

## Notes

`build.sh` installs upstream's `ardour.1` as `ardour9.1`, matching the program's installed name,
and writes its own `ardour9.desktop` with `StartupWMClass=Ardour`: Ardour's windows set their
X11 window class to its program name, `Ardour`, each with an instance name of its own
(`ardour_mixer`, `ardour_plugin_editor` and so on).
