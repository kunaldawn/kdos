## About

Printrun is host software for 3D printers that take G-code over a serial line, as most Marlin, Prusa
and RepRap firmware does. The port installs its console half: `pronsole`, an interactive console
that connects to the printer, sends a sliced job and reports progress and temperatures, and
`printcore`, the library and command under it, which streams a G-code file to a printer with no
interface at all. With a slicer it is what sends the job down the USB cable. The printer's port is a
USB serial device, which the `dialout` group opens; the `kdos` user is in it through `fs/etc/group`.

## Flags

- `--no-build-isolation` — pip builds the package with the `setuptools` and `cython` already
  installed instead of fetching them into an isolated environment; both are ports in `depends`
  ([python3-setuptools](#port-python3-setuptools), [python3-cython](#port-python3-cython)), and
  Cython compiles the `printrun.gcoder_line` extension, the G-code line parser.
- `--no-deps`, `--no-index` — Install Printrun alone, with nothing resolved or downloaded. Its
  `requirements.txt` names the graphical front end's dependencies as well (wxPython, pyglet, numpy,
  lxml, puremagic, dbus-python); the console path imports only `serial`, `platformdirs` and
  `psutil`, which are ports in `depends`.

## Patches

- `no-wx-on-console.patch` — `printrun/settings.py` imports `wx` at module scope, and `pronsole` imports its settings from there, so the console client would refuse to start without wxPython, which this port does not depend on. The patch wraps the `wx` import, and the import of `get_space` from the GUI widgets, in a `try`; without wxPython, `wx` is `None` and `get_space` raises an `ImportError` only if something builds a settings widget. Every use of `wx` in the file is inside such a method, so a console session never reaches one.

## Notes

- **The graphical front ends are removed.** `pronterface.py` and `plater.py`, with their compiled
  caches, desktop entries and AppStream files, are deleted from the package. Their 3D and projector
  views import pyglet and puremagic, which the tree does not carry, so they would be commands on the
  `PATH`, and entries in Start, that fail on an import.
- **`pronsole` and `printcore` without the suffix.** Upstream installs its programs as the scripts
  `setup.py` lists, `pronsole.py` and `printcore.py`; the recipe adds `pronsole` and `printcore` as
  links to them, so both run by their bare names. `pronsole.desktop` stays installed, and runs
  `/usr/bin/pronsole.py` in a terminal.
- **The sleep inhibit is optional.** During a print `pronsole` asks `org.freedesktop.ScreenSaver`
  over D-Bus to hold off the idle lock, inside a `try` that goes on without it. dbus-python is not
  in this port's `depends`, and without it `pronsole` prints with no inhibit. psutil is used to
  raise the process's priority while it prints, as far as the `RLIMIT_NICE` limit allows.
