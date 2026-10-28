## About
[MKVToolNix]{.idx} creates, splits, edits and inspects Matroska and WebM files: `mkvmerge`,
`mkvextract`, `mkvinfo` and `mkvpropedit` on the command line, and `mkvtoolnix-gui`, the Qt 6
application that drives them. It reads and writes Matroska through
[libebml](#port-libebml) and [libmatroska](#port-libmatroska).

## Flags
- `ac_cv_path_INKSCAPE=` — Record Inkscape as not found. The Rakefile renders the Windows icon
  set whenever configure finds both Inkscape and ImageMagick, and Inkscape crashes in the chroot;
  the empty answer keeps that Windows-only step out whatever the tree holds
  ([An optional tool found in the tree](../../../../docs/kdos/05-developer/build-troubleshooting.md#an-optional-tool-found-in-the-tree)).
- `--enable-gui` — Build the Qt 6 GUI. Upstream's default, named.
- `--enable-dbus` — Build the GUI with Qt's D-Bus module. Upstream's default is `auto`.
- `--disable-update-check` — Compile out the GUI's daily download of the release list from the
  MKVToolNix home page.
- `--disable-precompiled-headers` — Build without precompiled headers, which configure otherwise
  turns on for any compiler at version 4.1.0 or later (`ac/precompiled_headers.m4`).
- `--without-gettext` — Build the programs in English, with no message catalogues.
- `--with-flac` — With `--with-dvdread`: FLAC audio, and reading chapters from DVDs through
  [libdvdread](#port-libdvdread). Both are upstream's defaults, named; neither makes a missing
  library an error, and configure reports what it found.

## Notes
- **The build tool.** The build runs on `drake`, a parallel version of rake that the tarball
  carries under `rake.d/vendor`, so [ruby](#port-ruby) is all it needs. `V=1` prints each
  command in full, and `-j"$KDOS_JOBS"` gives it the tree's job count.
- **Manual pages** come from DocBook through `xsltproc`, with [docbook-xsl](#port-docbook-xsl).
  The translated pages need po4a, which is not a port, and are not built.
- **The menu icon.** Upstream installs SVGs, and the panel reads hicolor PNGs, so `build.sh`
  renders 48, 64, 128 and 256 pixel PNGs of `mkvtoolnix-gui.svg`.
- **The menu entry is replaced** to add `StartupWMClass`, which upstream's lacks: Qt builds the
  Wayland app_id from the organisation domain and the program, `org.bunkus.mkvtoolnix-gui`. It
  claims only the GUI's own settings type; the Matroska and WebM types belong to the players.
