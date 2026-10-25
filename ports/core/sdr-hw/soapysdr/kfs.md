## About

SoapySDR is a vendor-neutral C and C++ API for software-defined radio hardware. A program written
against it, such as [gnuradio](#port-gnuradio), [gr-osmosdr](#port-gr-osmosdr),
[sdrangel](#port-sdrangel), [sdrpp](#port-sdrpp), [cubicsdr](#port-cubicsdr) or
[rtl-433](#port-rtl-433), drives any radio that has a SoapySDR module. The modules are separate
ports ([soapyrtlsdr](#port-soapyrtlsdr), [soapyhackrf](#port-soapyhackrf),
[soapyairspy](#port-soapyairspy), [soapybladerf](#port-soapybladerf), and the LimeSDR one from
[limesuite](#port-limesuite)); each installs a shared object into
`/usr/lib/SoapySDR/modules<ABI version>/`, which the library loads when a program asks for a
device. `SoapySDRUtil --find` and `--probe` list what it sees.

## Flags

- `-DENABLE_PYTHON=OFF`, `-DENABLE_PYTHON3=OFF` — Build neither of the two Python binding
  directories: `python/`, for whichever interpreter CMake's `FindPythonInterp` finds, and
  `python3/`, for Python 3. Both are generated with SWIG and are on by default whenever SWIG and
  a Python interpreter with its headers are found.
- `-DENABLE_TESTS=OFF` — Build no unit tests.
- `-DENABLE_DOCS=OFF` — Generate no Doxygen documentation, which upstream builds whenever doxygen
  is found.
