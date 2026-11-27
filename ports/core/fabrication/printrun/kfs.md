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

## Notes

- **The console imports no wx.** `pronsole` imports its settings from `printrun/settings.py`,
  which upstream opens with `import wx` and whose `ComboSetting` takes its default width from the
  GUI's `get_space` as a default argument, evaluated when the class is defined.
  `no-wx-on-console.patch` makes the `wx` import optional and works the width out when the widget
  is built, so the module imports, and `pronsole` starts, with no wxPython installed; a call that
  builds a settings widget is the one place that still needs it.
- **The graphical front ends are removed.** `pronterface.py` and `plater.py`, with their compiled
  caches, desktop entries and AppStream files, are deleted from the package. Their 3D and projector
  views import pyglet and puremagic, which the tree does not carry, so they would be commands on the
  `PATH`, and entries in Start, that fail on an import.
- **`pronsole` and `printcore` without the suffix.** Upstream installs its programs as the scripts
  `setup.py` lists, `pronsole.py` and `printcore.py`; the recipe adds `pronsole` and `printcore` as
  links to them, so both run by their bare names. `pronsole.desktop` is the recipe's own, written
  in place of upstream's, and runs `pronsole` in a terminal. Upstream's is named "Printer
  console", which the launcher's fuzzy search ranks first for "Printers", ahead of the Printers
  settings page; KDOS's says "3D Printing Console" and keeps "printer" out of every searched field.
- **The sleep inhibit is optional.** During a print `pronsole` asks `org.freedesktop.ScreenSaver`
  over D-Bus to hold off the idle lock, inside a `try` that goes on without it. dbus-python is not
  in this port's `depends`, and without it `pronsole` prints with no inhibit. psutil is used to
  raise the process's priority while it prints, as far as the `RLIMIT_NICE` limit allows.
