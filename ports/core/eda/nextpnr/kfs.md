## About

nextpnr is the place-and-route tool of the open FPGA flow: it takes the netlist that
[yosys](#port-yosys) synthesises and fits it onto the cells and wires of one chip, writing the
configuration that [icestorm](#port-icestorm) or [prjtrellis](#port-prjtrellis) then packs into a
bitstream. Each chip family is its own program, `nextpnr-ice40`, `nextpnr-ecp5`,
`nextpnr-machxo2` and `nextpnr-generic`, and each carries its family's chip database compiled into
the binary.

## Flags

- `-Wl,--strip-debug` — Drop debug sections at the link. nextpnr's `CMakeLists.txt` sets its own
  `CMAKE_CXX_FLAGS_RELEASE` to `-O3 -g`, so a release build compiles every object with DWARF;
  no port ships it (see
  [What kpkg does around build.sh](https://github.com/kunaldawn/kdos/blob/main/docs/kdos/05-developer/writing-ports.md#what-kpkg-does-around-buildsh)).
- `-DCMAKE_POLICY_DEFAULT_CMP0167=NEW` — Make `find_package(Boost)` read the `BoostConfig.cmake`
  package that [boost](#port-boost) installs instead of CMake's old FindBoost module. FindBoost
  looks for a `libboost_system` library file; the boost port installs Boost.System as headers
  only, so the module reports the component missing.
- `-DCURRENT_GIT_VERSION="nextpnr-$version"` — The version string the programs print. Unset,
  `common/CMakeLists.txt` runs `git describe` in the source tree, which is no checkout here.
- `-DARCH="ice40;ecp5;machxo2;generic"` — The families to build. The cache variable is empty upstream, and
  configure stops until it names at least one.
  `ice40` reads icestorm's database and `ecp5` and `machxo2` read prjtrellis's. nexus, mistral and
  himbaechel are left out: their databases come from prjoxide, the mistral source tree and
  apicula, none of which is a port.
- `-DMACHXO2_DEVICES="1200;6900"` — The MachXO2 and MachXO3 parts whose chip database is compiled
  in, which is upstream's default pair. A design for any other part of that family finds no
  database.
- `-DBUILD_GUI=ON` — Build the Qt 6 floorplan viewer, `nextpnr-<arch> --gui`: the design drawn over
  the fabric as it is packed, placed and routed. Off by default upstream. It needs
  `BUILD_PYTHON`, and configure stops without it.
- `-DBUILD_PYTHON=ON` — Embed Python, through pybind11, for the `--pre-pack`, `--post-route` and
  `--run` scripts. It is upstream's default, and the GUI requires it.
- `-Dpybind11_DIR="$(python3 -m pybind11 --cmakedir)"` — Point `find_package(pybind11 CONFIG)` at
  the [python3-pybind11](#port-python3-pybind11) port, which keeps its CMake package inside
  `site-packages`. Without it the search finds nothing and configure falls back to the copy in
  `3rdparty/pybind11` with only a status line.
- `-DBUILD_RUST=OFF` — No Rust integration. It is upstream's default.
- `-DUSE_OPENMP=ON` — Parallelise the analytic placer with OpenMP (gcc's libgomp). Off by default
  upstream.
- `-DICESTORM_INSTALL_PREFIX=/usr` — Where icestorm's chip database is read from at build time.
  The default is the install prefix, which is also `/usr`, unless an `ICESTORM_INSTALL_PREFIX`
  variable is in the environment; naming it makes the environment irrelevant.
- `-DTRELLIS_INSTALL_PREFIX=/usr` — The same for prjtrellis's database and its Python library.

## Notes

The chip databases are read when nextpnr is built, not when it runs, so
[icestorm](#port-icestorm) and [prjtrellis](#port-prjtrellis) are in `depends` of a program that
links neither. A database that changes after nextpnr is built reaches it only through a rebuild of
nextpnr.

On a C library other than glibc, configure stops when libunwind is missing, GUI or not. The
[libunwind-nongnu](#port-libunwind-nongnu) port is in `depends` for that.

nextpnr installs no menu entries, so `build.sh` writes two, for ice40 and ecp5, each running
`nextpnr-<arch> --gui`. machxo2 and generic have none: machxo2 needs a `--device`, and generic has
no device until a Python script describes one.
