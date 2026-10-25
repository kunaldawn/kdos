## About

Sideband is a messaging, voice-call, telemetry and map client for Reticulum, the mesh networking
stack that runs over LoRa radios, packet radio, serial links and ordinary IP. It speaks LXMF, the
messaging protocol built on Reticulum, and carries voice through LXST with the Codec2 and Opus
codecs. It is a Kivy program, drawn through SDL. Reticulum, LXMF, Kivy, NumPy, Pillow, qrcode and
the rest of its dependencies are ports of their own, found in `site-packages` through `depends`.

## Flags

- `--prefix=$_home` — Installs Sideband and the four Python packages only it imports (LXST, the
  voice transport, and pycodec2, mistune and audioop-lts) under their own prefix,
  `/usr/lib/sideband`, where they cannot collide with a copy another port puts in `site-packages`.
- `--no-build-isolation` — Builds each package with the build backends already installed (the
  Cython, NumPy and setuptools ports) instead of an isolated environment pip would fill from the
  network. pycodec2 binds codec2's C library and builds against NumPy's headers with Cython.
- `--ignore-installed` — Keeps the prefix whole when a copy of a package is already in the build
  root; without it pip would uninstall that copy from the root instead of writing the new one into
  `$PKG` (see [The three Python
  keys](https://github.com/kunaldawn/kdos/blob/main/docs/kdos/05-developer/writing-ports.md#the-three-python-keys)).
- `--no-deps` — Installs exactly the named packages and resolves nothing. Of Sideband's declared
  requirements, materialyoucolor and beautifulsoup4 are imported by no module it ships, and are not
  installed.

## Notes

The vendor bundle, `sideband-vendor-<version>.tar.xz`, is a Python bundle that `ports/fetch` makes
from the recipe's `pypackages` line, `pycodec2 mistune audioop-lts`, as source distributions with
their build requirements, and pins by its `sha256` (see
[Vendoring](https://github.com/kunaldawn/kdos/blob/main/docs/kdos/05-developer/writing-ports.md#vendoring)).
LXST is the recipe's second `source =`, a GitHub tag archive installed from its unpacked tree.
audioop-lts is the `audioop` module Python 3.13 removed from the standard library, which LXST's
pydub imports.

LXST builds its filter library from `LXST/Filters.c`, loaded at run time through cffi. Its source
also carries prebuilt Windows and macOS libraries for its bundled pyogg, which `setup.py` installs
as package data; they are code for other systems and `build.sh` removes them. pyogg then loads the
system's `libopus` by name at run time; without [opus](#port-opus), LXST's Opus codec cannot load
and calls are left with Codec2 and raw audio.

The `platlib` directory for that removal is resolved with both `base` and `platbase` set to the
prefix. With `base` alone it resolves to the build root's own `site-packages`, and the `rm` would
reach outside `$PKG`.

`/usr/bin/sideband` is a launcher written by `build.sh`. It puts the prefix's site directories,
resolved through `sysconfig` so that it names no Python version, ahead of any `PYTHONPATH` the
caller set, and runs the entry point pip wrote. It also sets `SDL_APP_ID`, the Wayland app_id SDL
gives the window; unset, the app_id is the Python interpreter's name, which no desktop entry can
match. `setup.py`'s data files land under the prefix and are deleted; the icon and the desktop
entry are installed into `/usr/share` by `build.sh`.
