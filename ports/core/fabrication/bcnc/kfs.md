## About

bCNC is a controller and G-code sender for CNC machines running GRBL firmware, written in Python
with a Tk interface: it streams G-code over a serial port and adds an editor, probing,
auto-levelling and simple CAM tools. Tk draws through X11, so the window runs under Xwayland.

## Flags

- `--no-deps` — With `--no-index`, `--find-links=vendor` and `--no-build-isolation`, in the first
  `pip3 install`: svgelements and shxparser are installed from the unpacked vendor bundle alone,
  with the tree's own setuptools and wheel as build backend, resolving nothing. The second
  `pip3 install` installs bCNC itself from `.` the same way, without `--find-links`, since every
  other module bCNC needs is a port.
- `--ignore-installed` — Installs into `$PKG` even when the build root already holds a copy, so
  the package always carries the vendored modules and bCNC.

## Patches

- `no-update-check.patch` — Ships `checkinterval = 0` in `bCNC.ini`, and moves the test for an interval of 0 ahead of the test for a first start in `need2Check()`. Upstream's update reminder asks `api.github.com` for the latest release, and on a first start it asks whatever the interval is; with the patch an interval of 0 means never.
- `wm-class.patch` — Creates the Tk application with the class name `bCNC` instead of upstream's name padded with two spaces on each side, which no desktop entry can name; unpadded, the window's class matches `StartupWMClass=bCNC`.

## Notes

svgelements and shxparser, which bCNC's `setup.py` requires and nothing else in the tree uses, are
vendored: they are named at their versions in `pypackages` in `kpkgbuild`, and `ports/fetch`
packs their source distributions into `bcnc-vendor-<version>.tar.xz`, pinned by its `sha256` (see
[Vendoring](../../../../docs/kdos/05-developer/writing-ports.md#vendoring)). numpy, Pillow,
pyserial and OpenCV are ports in `depends`.

`build.sh` removes every language directory under `bCNC/locales`, so the interface is English;
kpkg's English-only rule removes only the `.mo` catalogs in them and would leave the `.po` sources
(see [Size and language policy](../../../../docs/kdos/05-developer/writing-ports.md#size-and-language-policy)).

bCNC's only icon is 204 pixels square, which is no hicolor size, so `build.sh` scales it to 128
pixels with [python3-pillow](#port-python3-pillow). `/usr/bin/bcnc` is a symbolic link to
upstream's `bCNC` command.
