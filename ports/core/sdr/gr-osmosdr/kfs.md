## About
[gr-osmosdr]{.idx} is one GNU Radio source block and one sink block that front many kinds of radio
hardware behind a single device string, such as `rtl=0` or `hackrf=0`. A program written against
it opens whichever radio is plugged in without code of its own for each. In KDOS it is how
[gqrx](#port-gqrx) reaches a radio, and it installs `osmocom_fft` and the other `osmocom_*`
tools.

## Flags
- `-DENABLE_DEFAULT=OFF` — start from no device support, then name each wanted one `ON`. The
  build uses GNU Radio's component machinery, so a device named `ON` whose library is missing is a
  configure error instead of a device the string parser silently does not know.
- `-DENABLE_RTL=ON` — and the other devices named `ON`: RTL-SDR dongles and an `rtl_tcp` server,
  HackRF, bladeRF, Airspy and Airspy HF+, MiriSDR, RFSPACE and Red Pitaya receivers, anything
  SoapySDR drives, IQ files, Ettus radios through GNU Radio's gr-uhd, the FUNcube Dongle through
  [gr-funcube](#port-gr-funcube), and I/Q imbalance correction through
  [gr-iqbal](#port-gr-iqbal). Each library is a port in `depends`.
- `-DENABLE_FREESRP=OFF` — with `-DENABLE_XTRX=OFF`: no FreeSRP or XTRX support. Their libraries,
  libfreesrp and libxtrx, are not ports.
- `-DENABLE_NONFREE=OFF` — never look for the SDRplay API, a closed library and the one component
  upstream marks nonfree. Upstream's default, named.
- `-DCMAKE_POLICY_DEFAULT_CMP0167=NEW` — make `find_package(Boost)` read [Boost](#port-boost)'s
  own CMake package, which knows Boost.System is header-only; the old `FindBoost` module looks for
  a `libboost_system` library.
- `-DPYTHON_INCLUDE_DIR` — with `-DPYTHON_LIBRARY`: Python's header directory and shared library,
  as `sysconfig` reports them for `/usr/bin/python3`. CMake's `FindPythonLibs` runs twice: through
  `GrPython.cmake`, which [GNU Radio](#port-gnuradio)'s CMake package includes and which asks for
  the interpreter's exact version, and through the configure's own `find_package(PythonLibs 3)`.
  With both cache variables set, each takes these paths instead of searching for them.
- `-Dpybind11_DIR` — the CMake package directory of [python3-pybind11](#port-python3-pybind11),
  for the `osmosdr` Python module.

## Notes
The last two lines of `build.sh` stop the build when `libgnuradio-osmosdr.so` or `osmocom_fft` is
missing from the package.
