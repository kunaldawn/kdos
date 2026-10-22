## About
Brotli is Google's general-purpose compression format, the `br` content encoding of HTTP and the
compression inside WOFF2 web fonts. The port installs the three shared libraries
(`libbrotlicommon`, `libbrotlidec`, `libbrotlienc`), the `brotli` command, and the Python module
`brotli`.

The libraries are what [curl](#port-curl) decodes `br` responses with and what
[freetype2](#port-freetype2) reads WOFF2 fonts through, and [qt6-qtbase](#port-qt6-qtbase),
[webkitgtk](#port-webkitgtk), [chromium](#port-chromium) and [nodejs](#port-nodejs) depend on
them too. The Python module is there for Python programs that ask for
it: [fonttools](#port-fonttools), [calibre](#port-calibre) and [yt-dlp](#port-yt-dlp) all name
`brotli` in their `depends`.

## Flags
- `-D BUILD_SHARED_LIBS=True` — Build the three libraries shared. It is upstream's default, stated
  here; set false, CMake would build static archives only.
- `--optimize=1` — Have setuptools write optimised byte code, `brotli.cpython-*.opt-1.pyc`, beside
  the plain `.pyc` when it installs the module, so Python run with `-O` finds it ready.

## Notes
`build.sh` builds the source twice. CMake builds and installs the C libraries and the `brotli`
command; `setup.py` then builds the Python extension `_brotli` from the same tree. With
`USE_SYSTEM_BROTLI` unset, as here, `setup.py` compiles the C sources into the extension itself
rather than linking the libraries just installed, so `_brotli.cpython-*.so` needs nothing but the C
library. Setting `USE_SYSTEM_BROTLI=1` would make it link them instead, and would need the `pkgconfig` Python module, which this build does not
depend on.
