## About

Universal Radio Hacker (URH) records, demodulates, decodes and replays wireless protocols: it
captures a signal from a software-defined radio or a file, turns it into bits, and helps work out
the protocol inside them. It is a Python program on PyQt6 ([python3-pyqt6](#port-python3-pyqt6)),
with Cython extensions for its signal processing and one native extension per radio family, built
against that radio's own library: [airspy](#port-airspy), [bladerf](#port-bladerf),
[hackrf](#port-hackrf), [rtl-sdr](#port-rtl-sdr), [limesuite](#port-limesuite),
[libiio](#port-libiio) for the PlutoSDR and [uhd](#port-uhd) for USRP radios.

## Flags

- `--with-airspy` — and `--with-bladerf`, `--with-hackrf`, `--with-rtlsdr`, `--with-limesdr`,
  `--with-plutosdr`, `--with-usrp`: build the native extension for each of these radios
  unconditionally. Without a switch, `setup.py` (`ExtensionHelper.py`) compiles a small probe
  program against the radio's library and quietly skips the extension when the probe fails, so a
  library missing at build time would leave a radio out of the package. `ExtensionHelper.py` reads
  these switches from `setup.py`'s own command line and removes them before setuptools sees the
  rest, so they are given to `setup.py bdist_wheel` directly.
- `--without-sdrplay` — no SDRplay extension. It links SDRplay's `mirsdrapi-rsp` library, which is
  proprietary and is not a port.

## Patches

- `app-id.patch` — In `src/urh/main.py`, calls `app.setDesktopFileName("urh")` right after the `QApplication` is made. Qt's Wayland plugin takes the window's app id from the desktop file name and, when that is empty, from the base name of the running executable, which for a Python program is the interpreter, `python3`. With the patch the app id is `urh`, the name of the desktop entry the recipe writes, and the panel can match the window to it (see [`kdos appid`](../../../../docs/kdos/04-programs/kdos-command.md#kdos-appid)).

## Notes

The wheel is installed with `pip --no-deps --no-index --no-build-isolation`, so pip neither reaches
the network nor resolves anything: every Python dependency is a port in `depends`. The last lines
of `build.sh` check that each of the seven radio extensions (`airspy`, `bladerf`, `hackrf`,
`rtlsdr`, `limesdr`, `plutosdr`, `usrp`) landed under `urh/dev/native/lib` in site-packages, so the
build fails instead of packaging a URH that cannot open one of those radios.

Upstream installs no desktop entry or PNG icons. `build.sh` writes `urh.desktop` and rasterises
`data/icons/appicon.svg` to 48, 64, 128 and 256 pixel PNGs with `rsvg-convert` from
[librsvg](#port-librsvg), beside upstream's 512 pixel `appicon.png`.
