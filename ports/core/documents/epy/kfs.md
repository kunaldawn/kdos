## About

epy is an ebook reader for the terminal: it opens EPUB, MOBI, AZW3 and FictionBook files, shows the
reading progress, keeps bookmarks, and offers search, a table of contents and lookups in an external
dictionary program (`sdcv`, `dict` or `wkdict`). It is pure Python, built here from upstream's
source with pip. The port also installs `epy.desktop`, kept beside the recipe, so an ebook opened
from the desktop can open in epy in a terminal.

## Patches

- `python313-imghdr.patch` — The bundled KindleUnpack code in `src/epy_reader/tools/KindleUnpack/mobi_cover.py` asks the standard library's `imghdr` module which image format a book's cover is. `imghdr` is not in the standard library from Python 3.13 on, so importing the module fails. The patch drops the import and adds `_sniff_image()`, which recognises PNG, JPEG, GIF, BMP, TIFF and WebP by their first bytes and returns the same names `imghdr.what()` returned, which the callers compare against; `get_image_type()` reads the first 32 bytes of the file, or of the data it is given, and passes them to it.

## Notes

- **The vendor bundle is the build backend alone.** epy names no run-time dependency for Linux, and
  the bundle `ports/fetch` made from its metadata holds only `poetry-core`, the PEP 517 backend its
  `pyproject.toml` asks for. `build.sh` unpacks it into `vendor/`, and pip builds the wheel in an
  isolated environment that takes `poetry-core` from there (`--no-index --find-links=vendor`), so
  nothing is downloaded and nothing is installed into the build root. `--no-deps` installs epy and
  nothing else into the package; why every `pip install --root` also passes `--ignore-installed` is
  in [The three Python
  keys](https://github.com/kunaldawn/kdos/blob/main/docs/kdos/05-developer/writing-ports.md#the-three-python-keys).
- **The desktop entry is a handler.** `epy.desktop` claims the EPUB, MOBI and FictionBook types,
  runs `epy %f` with `Terminal=true`, and is `NoDisplay=true`, so it is not listed in Start or the
  launcher; see [Desktop entries belong to the
  port](https://github.com/kunaldawn/kdos/blob/main/docs/kdos/05-developer/writing-ports.md#desktop-entries-belong-to-the-port).
