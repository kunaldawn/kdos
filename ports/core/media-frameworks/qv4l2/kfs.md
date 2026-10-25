## About
`qv4l2` and `qvidcap` are the two Qt programs of v4l-utils, the Video4Linux tools. `qv4l2` is a
test bench for a capture or output device: it shows every control, input and format the driver
offers and streams from it, through OpenGL or plain Qt. `qvidcap` is a viewer for a capture
stream. The libraries and command-line tools come from the [v4l-utils](#port-v4l-utils) port,
which is built in `41_system`, before any toolkit; this port builds the same source again in
`44_apps` and keeps only the two Qt programs.

## Flags
- `-D_LARGEFILE64_SOURCE` — added to `CFLAGS` and `CXXFLAGS`, as in the v4l-utils recipe. musl
  declares `open64` and `mmap64` only under this macro. In v4l-utils the code that calls them is
  `v4l2-tracer`, which this build leaves out, and upstream's `meson.build` already passes the
  macro to the preload wrappers.
- `-Dqv4l2=enabled` — with `-Dqvidcap=enabled`: build both programs, so a missing Qt stops the
  configure. With the options at `auto`, meson skips them without an error.
- `-Dbpf=disabled` — with `-Dlibdvbv5=disabled` and `-Dv4l2-tracer=disabled`: skip the parts of
  the tree this port does not install (`ir-keytable`'s BPF decoders, the DVB library and tools,
  and the call tracer). v4l-utils builds and ships them.
- `-Dv4l-plugins=true` — and the `-Dv4l-*` and `-Dv4l2-*` `true` switches after it, `-Dgconv`,
  `-Djpeg` and `-Dudevdir`: the same values as the v4l-utils recipe, so the libraries the two Qt
  programs are linked against here are configured as the shipped ones are.

## Notes
`meson install` goes into a staging directory, `stage/`, not into `$PKG`. `build.sh` then copies
only the two programs, their manual pages, their desktop entries and their icons into the package.
Every other file of the tree is in the v4l-utils package, and kpkg's conflict scan refuses a
package that claims a path another installed package owns (see
[Who owns a file](../../../../docs/kdos/03-architecture/packaging.md#who-owns-a-file)).

meson takes Qt 6 when it finds Qt 6 with `Core5Compat`, and otherwise falls back to Qt 5. The last
line of `build.sh` checks that `qv4l2` names `libQt6Widgets.so.6`, so a build that fell back to
Qt 5 fails instead of shipping. That is why [qt6-qt5compat](#port-qt6-qt5compat) is in `depends`.
