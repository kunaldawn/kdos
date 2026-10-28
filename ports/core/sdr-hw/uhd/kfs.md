## About

UHD, the USRP Hardware Driver, is Ettus Research's driver library for its USRP radios (the USB
B-series, the networked N- and X-series, and the embedded E-series). It installs `libuhd`, a C API
over it, the `uhd` Python module, and the `uhd_*` utilities, among them `uhd_find_devices` and
`uhd_usrp_probe`. [gnuradio](#port-gnuradio) builds its `gr-uhd` blocks on it, and
[gr-osmosdr](#port-gr-osmosdr), [sdrangel](#port-sdrangel), [sdrpp](#port-sdrpp),
[satdump](#port-satdump) and [urh](#port-urh) link it. The repository holds the FPGA and firmware
sources as well; `build.sh` configures only the host software, in `host/`.

## Flags

- `-DUHD_VERSION=$version` — The version UHD reports and writes into `uhd.pc`. Left alone, UHD
  derives it from git (`git describe`, the branch name), and a release tarball is no checkout, so
  the version would carry a commit count of `0` and a hash of `unknown`.
- `-DENABLE_STATIC_LIBS=OFF` — Build no static library; upstream's default.
- `-DENABLE_LIBUHD=ON`, `-DENABLE_C_API=ON`, `-DENABLE_UTILS=ON`, `-DENABLE_USB=ON` — Require the
  library, its C API, the utility programs and USB transport (for the B-series and the USRP1, on
  [libusb](#port-libusb)). Each is a UHD component, on by default whenever its dependencies are
  found. Upstream marks the library required, so configure stops without Boost and Mako even
  unnamed; the other three are left out with only a status line when theirs are missing. Named
  `ON`, a component whose dependencies are missing stops configure with "Dependencies for
  required component … not met".
- `-DENABLE_PYTHON_API=ON` — Build the `uhd` Python module with pybind11. The component needs
  [numpy](#port-numpy) and the Python headers, and named `ON` it fails configure without them. It
  compiles against [python3-pybind11](#port-python3-pybind11) when CMake finds it, and against the
  copy under `host/lib/deps/pybind11` when not. The last `test` line in `build.sh` fails the build
  if the module is not in the package.
- `-DENABLE_EXAMPLES=OFF`, `-DENABLE_TESTS=OFF` — Build neither the example programs nor the unit
  tests, which are on by default.
- `-DENABLE_PYMOD_UTILS=OFF` — Leave out the Python utilities module, a separate component from
  the Python API.
- `-DENABLE_SIM=OFF` — Leave out UHD's simulated USRP device, which is on by default whenever
  the Python API is built.
- `-DENABLE_DPDK=OFF` — No DPDK transport for the networked radios; DPDK is not a port.
- `-DENABLE_RFNOC_DEV=OFF` — Install none of the RFNoC FPGA development files; upstream's default.
- `-DENABLE_MAN_PAGES=OFF` — Let UHD install no manual pages itself. Its manual-page component
  installs the uncompressed pages by bare name from `host/docs/`, where they are not, and the
  install stops; with compression on, it installs gzipped copies of its whole list, among them
  pages for `rfnoc_modtool` and `rfnoc_image_builder`, which this build does not install. The loop
  after the install puts the uncompressed pages in place, below.
- `-DENABLE_MANUAL=OFF`, `-DENABLE_DOXYGEN=OFF` — Generate neither the HTML manual nor the API
  documentation with Doxygen.

## Patches

- `gcc-15.patch` — Adds `#include <cstdint>` to `host/lib/include/uhdlib/transport/link_if.hpp`. The header uses `int32_t` and includes nothing that declares it with the tree's libstdc++.

## Notes

The loop after the install puts the manual pages in place. For each program in
`$PKG/usr/bin` it installs `host/docs/manpages/<program>.1`, uncompressed, when one exists, so
no page describes a program that is not there. The
`test` lines at the end check the pkg-config file, `uhd_find_devices`, its manual page and the
Python module.

The package carries no FPGA or firmware images for the radios. `uhd_images_downloader` fetches
them from Ettus into `/usr/share/uhd/images/` when it is run, and a radio that loads its image from
the host finds none until then.

Access to the USB radios comes from the tree's own udev rules, which give the Ettus and National
Instruments ids to the `dialout` group (see
[The device groups and the udev rules](../../../../docs/kdos/02-user-guide/administration.md#the-device-groups-and-the-udev-rules));
upstream's `uhd-usrp.rules`, installed under `/usr/lib/uhd/utils`, is not used.

The recipe's `series` key keeps `ports/update` offering only releases on the line it names. The
next line, 4.11, moves the RPC transport of the MPM devices (N3xx, E3xx, X4xx) onto gRPC and
Protobuf, and its host build needs gRPC, which is not a port.
