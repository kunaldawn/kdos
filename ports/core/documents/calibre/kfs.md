## About
[calibre]{.idx} manages a library of e-books: it catalogues them, converts between formats such as
EPUB, AZW3, MOBI and PDF, fetches news and metadata, and sends books to readers. The port installs
its three windows, the library (`calibre`), the `ebook-viewer` and the `ebook-edit` editor, and its
command-line tools (`ebook-convert`, `calibredb` and the rest). It is Python on PyQt6 with C and
C++ extensions, and the Python modules it needs that the tree carries as no port of their own come
from a vendor bundle.

## Flags
- `XXHASH_LINK_SO=1` — read by the bundled `xxhash` module's `setup.py`: link the system
  [xxhash](#port-xxhash) library instead of compiling the copy of its C source the module carries.
- `CALIBRE_CONFIG_DIRECTORY="$SRC_ROOT/calibre-config"` — the build imports calibre, which creates
  its configuration directory on import; this keeps that directory inside the work tree rather than
  under the builder's home.
- `QT_QPA_PLATFORM=offscreen` — run Qt with no display during the build.
- `CFLAGS="${CFLAGS/-std=gnu11/}"` — remove the tree's C standard from `CFLAGS` for
  `setup.py install`. calibre's `setup/build.py` adds the environment's `CFLAGS` to the flags of
  every extension it compiles, the C++ ones included, where `-std=gnu11` is not a valid standard;
  see
  [C flags handed to the C++ compiler](../../../../docs/kdos/05-developer/build-troubleshooting.md#c-flags-handed-to-the-c-compiler).
- `--staging-root="$PKG/usr"` — write the files under the package, laid out for `--prefix=/usr`.
  Without it `setup/install.py` takes the prefix itself as the staging root and writes into
  `/usr` of the build root.
- `--no-postinstall` — skip upstream's post-install step, which writes shell completions, installs
  desktop entries, icons and MIME types through xdg-utils, and writes an uninstaller script;
  `build.sh` installs the entries, icons and MIME types into the package itself.

## Patches
- `0001-calibre-no-update.patch` — empties the loop of the background thread in `src/calibre/gui2/update.py` that checks for a new calibre release and for plugin updates, so neither check is ever made; the thread still runs and only waits.
- `0002-calibre-use-make.patch` — in `setup/build.py`, adds `-GUnix Makefiles` to the CMake command that configures calibre's headless Qt platform plugin. The step runs `make` straight after it, and without the switch the generator would follow `CMAKE_GENERATOR` from the environment.
- `0003-calibre-disable-piper.patch` — removes the `piper` extension, calibre's built-in neural text-to-speech, from `setup/extensions.json` and from the list of required extensions in `src/calibre/constants.py`, and skips it in the import test. It calls `espeak_TextToPhonemesWithTerminator()`, which the tree's [espeak-ng](#port-espeak-ng), a release, does not have, and its voices are downloaded on first use; with it gone, reading aloud goes through Qt Speech and [speech-dispatcher](#port-speech-dispatcher).
- `netifaces-msghdr.patch` — applied to the bundled `netifaces` module, not to calibre: initialises a `struct msghdr` in `netifaces.c` by field name instead of by position. musl's `msghdr` has padding members glibc's has not, so the positional form puts the values in the wrong fields and does not compile.

## Notes
The bundle, `calibre-vendor-<version>.tar.xz`, holds source distributions only. The recipe sets
`vendoring = python` with `pyrequirements = no`, `pyruntime = no` and an explicit `pypackages`
list, so `ports/fetch` downloads the modules named there without resolving their runtime
dependencies (see [The three Python keys](../../../../docs/kdos/05-developer/writing-ports.md#the-three-python-keys));
it also adds the build backends those source distributions declare. The modules more than one
program uses (lxml, Pillow, psutil, Pygments, dateutil, six, html5lib, fontTools, PyQt6 and the
rest in `depends`) are ports instead, and so are the build backends: pip runs with
`--no-build-isolation` and builds with the installed ones, not the copies in the bundle. html5-parser is a source of its own, pinned in the
recipe's `_html5parser`.

Two modules are unpacked and changed before they are installed. apsw's `setup.apsw` is deleted:
it tells apsw's build to download the matching SQLite source and compile it in, and without it
apsw links the [sqlite](#port-sqlite) port. netifaces gets the patch above.

The bundled modules are installed with `--target` into a directory of the build's own, then put on
`PYTHONPATH` for `setup.py install`, because its `gui` step imports calibre and calibre imports
them. That install empties calibre's library directory, `/usr/lib/calibre`, before filling it, so
the modules are copied into it afterwards. calibre's launchers put that directory first on
`sys.path`, so none of these modules is a path in `site-packages` that another package could also
own. Bytecode is compiled once at the end, with `compileall -s "$PKG" -p /`, so each `.pyc` records
the path it is installed at.

The release tarball already carries the generated resources the install needs (the compiled viewer
and editor scripts, MathJax, the hyphenation dictionaries, the iso-codes tables and fonts), and
`setup.py install` runs only its `build` and `gui` steps, so nothing reaches the network.

`build.sh` writes the three desktop entries. Each program sets its Wayland app id from its entry's
name, which the entries' `StartupWMClass` lines match. Only the viewer claims the e-book MIME types,
so a book opens in the viewer and PDF stays with the document viewer.
