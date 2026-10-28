## About
[GNU Radio]{.idx} is a library of signal-processing blocks for software-defined radio, with Python
bindings for every block and GNU Radio Companion (`gnuradio-companion`, GRC), a graphical editor
that wires blocks into a flow graph and generates the Python program that runs it. In KDOS it is
the base the other GNU Radio ports build on, [gr-osmosdr](#port-gr-osmosdr) among them, and it
reaches radio hardware through [SoapySDR](#port-soapysdr), [UHD](#port-uhd) and
[libiio](#port-libiio).

## Flags
- `-DENABLE_DEFAULT=OFF` — start with every component off, so that only the components the recipe
  names are built. Upstream's default turns on every component whose dependencies configure finds.
  A component named `ON` whose dependency is missing stops configure ("user force-enabled …"), so
  a missing dependency fails the build instead of producing a smaller GNU Radio.
- `-DENABLE_GR_QTGUI=ON` — and the other `ENABLE_GR_*` components after `ENABLE_GNURADIO_RUNTIME`:
  each block library by name. gr-qtgui is the Qt 5 widget set ([qt5-qtbase](#port-qt5-qtbase),
  [qwt-qt5](#port-qwt-qt5), [python3-pyqt5](#port-python3-pyqt5)); GRC itself is a GTK 3
  program. gr-uhd reaches Ettus radios, gr-iio the Analog Devices ones through
  [libad9361](#port-libad9361), gr-soapy everything SoapySDR drives, gr-zeromq sends streams and
  messages over ZeroMQ, and gr-vocoder builds the codec2, FreeDV and GSM 06.10 blocks.
- `-DENABLE_UHD_RFNOC=ON` — build gr-uhd's RFNoC blocks, which need UHD 4.0 or later.
- `-DENABLE_GR_VIDEO_SDL=OFF` — leave out gr-video-sdl, the video sink written to the SDL 1.2 API.
- `-DENABLE_GR_CTRLPORT=ON` — with `-DENABLE_CTRLPORT_THRIFT=ON`: ControlPort, the interface
  through which a running flow graph's parameters are read and set from outside it, carried over
  [thrift](#port-thrift). The ControlPort clients import Thrift's Python module.
- `-DENABLE_JSONYAML_BLOCKS=ON` — the JSON and YAML configuration blocks, which validate their
  input with [python3-jsonschema](#port-python3-jsonschema); configure turns the component on only
  when `import jsonschema` succeeds.
- `-DGRC_XTERM_EXE=/usr/bin/foot` — the terminal GRC runs a flow graph with no graphical
  interface in, written into `/etc/gnuradio/conf.d/grc.conf` as `xterm_executable`. Left to
  configure, it is the first of `x-terminal-emulator`, `gnome-terminal`, `konsole` and the rest
  found on the build's `PATH`, or the name `x-terminal-emulator` when none is, a command KDOS does
  not have. [foot](#port-foot) takes the `-e` GRC passes, and is in `depends`.
- `-DCMAKE_DISABLE_FIND_PACKAGE_JACK=ON` — with `-DCMAKE_DISABLE_FIND_PACKAGE_OSS=ON`: gr-audio
  never looks for JACK or OSS, so its backends are ALSA and PortAudio, both carried to
  [PipeWire](#port-pipewire), and do not change with whatever the build root holds.
- `-DENABLE_POSTINSTALL=OFF` — skip the install step that runs `xdg-utils` to register the
  desktop entry, icons and MIME type on the machine doing the build. `build.sh` installs those
  files itself.
- `-DENABLE_MANPAGES=ON` — with `-DENABLE_DOXYGEN=OFF`: the manual pages, which configure fills
  in from the `.1.in` templates in `docs/man`, and no Doxygen HTML reference.
- `-DENABLE_NATIVE=OFF` — no `-march=native`, which would tie the libraries to the build machine's
  processor. Upstream's default, named (see
  [`-march` measured per machine](../../../../docs/kdos/01-philosophy/decisions.md#-march-measured-per-machine-not-chosen-for-a-population)).
- `-DENABLE_COMMON_PCH=OFF` — build without the shared precompiled-header library the block
  libraries can be compiled against.
- `-DENABLE_EXAMPLES=ON` — install the example flow graphs, which GRC's `examples_path` points at.
  Upstream's default, named.
- `-DCMAKE_POLICY_DEFAULT_CMP0167=NEW` — make `find_package(Boost)` read [Boost](#port-boost)'s
  own CMake package. The old `FindBoost` module looks for a `libboost_system` library, and
  Boost.System is header-only.
- `-DPYTHON_INCLUDE_DIR` — with `-DPYTHON_LIBRARY`: Python's header directory and shared library,
  as `sysconfig` reports them for `/usr/bin/python3`. `GrPython.cmake` calls
  `find_package(PythonLibs … EXACT)` for the interpreter's version, and with both cache variables
  set CMake's `FindPythonLibs` takes these paths instead of searching for them.
- `-Dpybind11_DIR` — the CMake package directory of [python3-pybind11](#port-python3-pybind11),
  as `python3 -m pybind11 --cmakedir` prints it; the Python bindings are built with it.

## Notes
`docs/man` installs one fixed list of manual pages whichever components are built, so `build.sh`
removes each page whose command is not in the package.

With the post-install step off, the desktop entry, the `.grc` MIME type, the AppStream file and
the icons are copied from `grc/scripts/freedesktop` by `build.sh`, and kpkg rebuilds the MIME
database on the installed system. GRC's window's app_id is the script's name,
`gnuradio-companion`, which the entry's `StartupWMClass` names.

gr-qtgui's auto-correlator sink, ControlPort's plotter and the code GRC generates for a flow graph
with a Qt sink each run `import sip`, a top-level module. PyQt5 carries it only as `PyQt5.sip`, and no
port installs a top-level `sip` module, so `build.sh` writes a one-line `sip.py` into
site-packages that re-exports `PyQt5.sip`. Without it such a flow graph stops at its imports.

The last lines of `build.sh` stop the build when `gnuradio-companion`, the `foot` line in
`grc.conf`, or the compiled Python modules of `gnuradio.gr` and `gnuradio.qtgui` are missing from
the package.
