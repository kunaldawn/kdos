## About
`kio-library` makes the offline library a folder. Its KIO worker, `kio_library`, serves
`library:/`, whose five shelves list what is on this machine: Books (ZIM archives, the KDOS book
and KDOS from Scratch), Maps (Organic Maps' `.mwm` files), Models (llama.cpp's `.gguf` models,
whisper.cpp's `ggml-*.bin` and Piper's voices), Docsets (Zeal's `.docset` directories) and
Courses (Kolibri's channels). Each item shows its size and, in the Pack column, the data pack or
package it came from; opening it opens the file itself, with the program the menu would choose.

The worker writes nothing. It finds each item where its program keeps it: kiwix-manage's
`/var/lib/kiwix/library.xml` and Kiwix Desktop's own library, `/usr/share/kiwix` and
`~/.local/share/kiwix`; `/usr/share/organicmaps/data` and Organic Maps' `~/.local/share/OMaps`;
`/usr/share/llama.cpp/models`, `/usr/share/whisper.cpp/models` and `/usr/share/piper/voices`, and
the same under `~/.local/share`; Zeal's `~/.local/share/Zeal/Zeal/docsets` and
`/usr/share/zeal/docsets`; and Kolibri's `/var/lib/kolibri/content/databases`.

A data pack names its items with a launcher it installs: `Categories=X-KDOS-Library;`,
`X-KDOS-Library-Kind=` one of the five shelves, `X-KDOS-Library-Path=` the item's absolute path,
and `X-KDOS-Pack=` its id. The launcher's Name and Icon win over what the scan found, and the
launcher is the item's entry in the menu's Library category, which
`/etc/xdg/menus/applications-merged/kdos-library.menu` adds with its directory file
`kdos-library.directory`. A declared directory holding an `index.html` opens the index.

`kdos-library-place`, run once per login from `/etc/xdg/autostart`, adds Library to the Places
panel of Dolphin and every file dialog the first time, through KIO's own Places model.

## Flags
- `-S "$PORT_SRC"` — The sources are the port's own directory: a port of KDOS's own has no
  tarball, and the build tree is made in the empty `$SRC`.
- `-D KDE_INSTALL_USE_QT_SYS_PATHS=ON` — Install the worker into Qt's own plugin directory,
  `/usr/lib/qt6/plugins/kf6/kio`, where KIO looks for workers.
- `-D KDE_INSTALL_LIBEXECDIR=libexec` — Put `kdos-library-place` in `/usr/libexec/kdos`, the
  path its autostart entry names. ECM's default for this tree's layout is `lib/libexec`.
- `-D BUILD_TESTING=OFF` — Build no tests into the package. The QtTest suite in `tests/`, which
  builds a whole fake machine in a temporary directory, is run by the Qt regression net in the
  development container, not by the image build.
- `-D CMAKE_INSTALL_PREFIX=/usr`, `-D KDE_INSTALL_LIBDIR=lib`, `-D CMAKE_BUILD_TYPE=Release` —
  The tree's prefix and library directory, which ECM would otherwise put under `lib64`, and an
  optimised build. With the prefix `/usr`, ECM puts the menu and the autostart entry under
  `/etc`.

## Notes
An item's pack is, in order: the launcher's `X-KDOS-Pack`; the data pack whose graft, recorded in
`/var/lib/kdos/packs/pack-manifest`, the item lies under; and the kpkg package whose file list in
`/var/lib/kpkg/db` names it. Reading every package's list is the slow part, so what was found,
and what was looked for and not found, is kept in `~/.cache/kdos/library-owners` and trusted
until kpkg's database directory changes. An item under the home is the user's own and has no
pack.

KIO seeds a new Places list with its own defaults and has no hook a distribution can add one
through, so the Places entry is added by the user's session, once. A marker,
`~/.config/kdos/library-place`, records it, so a user who removes the entry has it stay removed.

An item's name in the URL is its file's own name; a second file of the same name on one shelf is
numbered, `a.zim~2`, as a file manager names a copy. A Piper voice is listed only with its
`.onnx.json` beside it, since one without the other does not speak. `KDOS_LIBRARY_ROOT`, when
set, is put in front of every system path, which is how the worker is pointed at a fixture tree.
