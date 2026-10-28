## About

Frotz is an interpreter for the Z-machine, the virtual machine Infocom's interactive fiction runs
on, and for the stories written for it since. KDOS builds its curses interface, so a story plays in
a terminal; the package carries no story of its own, and its desktop entry, `Exec=frotz %f` with
`Terminal=true`, opens one from the file manager.

frotz is the
[worked example of Writing ports](../../../../docs/kdos/05-developer/writing-ports.md#worked-example-frotz),
which goes through the whole recipe; the points below are the ones a reader of `build.sh` needs.

## Flags

- `curses` — The make target: build `frotz`, the curses interface, and neither `sfrotz` (SDL),
  which needs a window server, nor `dfrotz`, the dumb interface, which has no screen control and
  prints line by line.
- `SOUND_TYPE=none` — Build with no sound. The makefile's only other choice, and its default, is
  `ao`, which links libao, libsndfile, libvorbisfile, libmodplug and libsamplerate: an audio stack
  on every image for the few stories that use sound.
- `-Wno-error` — Cancel any `-Werror` on the compile line, so that a warning from this compiler
  stays a warning. frotz's makefile and the tree's flags add no `-Werror`, so on the build as it
  stands the switch changes nothing.
- `-Wl,--strip-debug` — Drop debug information at the link. The makefile appends `-g` to the
  `CFLAGS` it is given, so every object carries debug information whatever the tree's flags say,
  and the link is the first place it can be removed.

## Notes

The desktop entry claims `application/x-zmachine` and `application/x-blorb`, and
`shared-mime-info` defines neither, so `build.sh` installs
`/usr/share/mime/packages/kdos-zmachine.xml`, which defines both by file extension. Without it a
story file has no type, and the entry's `MimeType=` line matches nothing. Installing the package
rebuilds the MIME database on the target (see
[Shared indexes](../../../../docs/kdos/05-developer/writing-ports.md#shared-indexes)).
