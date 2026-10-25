## About
[CuraEngine]{.idx} is the slicer behind Cura: it turns a 3D model and a printer's settings into
the G-code a 3D printer runs. [Cura](#port-cura) starts it as `/usr/bin/CuraEngine` and drives it
over Arcus, its socket protocol; it also runs on its own from the command line, reading machine
definitions from the directories `CURA_ENGINE_SEARCH_PATH` names (the cura port installs them
under `/usr/share/cura/resources`).

## Flags
- `-DCMAKE_CXX_STANDARD=20` — Compile as C++20. Upstream's build gets its standard from Conan's
  toolchain, which this build does not use.
- `-DCURA_ENGINE_VERSION="$version"` — The version compiled into the engine and printed by it;
  Conan's recipe passes it.
- `-DENABLE_ARCUS=ON` — Build the Arcus front end Cura talks to, on [libarcus](#port-libarcus)
  and [protobuf](#port-protobuf), which compiles `Cura.proto`.
- `-DENABLE_PLUGINS=OFF`, `-DENABLE_REMOTE_PLUGINS=OFF` — Build without the gRPC plugin system,
  which needs asio-grpc, gRPC and Ultimaker's plugin definitions, and so without remote plugins,
  which only exist with it.
- `-DENABLE_SENTRY=OFF` — Send no crash reports to Sentry.
- `-DENABLE_TESTING=OFF`, `-DENABLE_BENCHMARKS=OFF` — Build no unit tests and no benchmarks.
- `-DEXTENSIVE_WARNINGS=OFF` — Do not add the long warning list of `set_project_warnings()`
  from the project settings module.
- `-DENABLE_MORE_COMPILER_OPTIMIZATION_FLAGS=OFF` — Declared as an option in `CMakeLists.txt`
  and read nowhere else in this release, so it changes nothing.
- `-DENABLE_THREADING=ON` — Link the engine and its static library `_CuraEngine` against the
  thread library through the project settings' `use_threads()`. It is upstream's default,
  stated. [onetbb](#port-onetbb) is found and linked either way.
- `-Dprotobuf_MODULE_COMPATIBLE=ON` — Make protobuf's CMake package also define the variables
  and functions of CMake's old `FindProtobuf` module, `protobuf_generate_cpp()` among them, which
  CuraEngine calls to compile `Cura.proto`.
- `-Dstandardprojectsettings_DIR="$_shim"`, `-Dscripta_DIR="$_shim"`, `-Dstb_DIR="$_shim"`,
  `-Dmapbox-wagyu_DIR="$_shim"`, `-Dclipper_DIR="$_shim"`,
  `-DCMAKE_PROJECT_INCLUDE="$_shim/aliases.cmake"` — Point each of CuraEngine's
  `find_package()` calls that only Conan answers at the shim files `build.sh` writes; see Notes.

## Patches
- `wagyu-execinfo.patch` — Applied to the wagyu source, not CuraEngine's: in `ring.hpp` and `ring_util.hpp`, includes `<execinfo.h>` for the debug backtrace only when `DEBUG` is defined and the header exists (`__has_include`). musl has no `execinfo.h`. It is the patch Ultimaker's own Conan recipe for wagyu applies.
- `include-iomanip.patch` — Adds `#include <iomanip>` to `src/utils/OBJ.cpp`, which calls `std::setprecision` and includes no header that declares it here.

## Notes
- **The Conan shim.** CuraEngine's CMake is written for Conan: it loads its compiler settings
  as a package called `standardprojectsettings` and links every dependency by the target name
  Conan's recipes generate. `build.sh` answers each name from the system, in `$SRC_ROOT/shim`,
  which never reaches the package:
  - `standardprojectsettings` includes `StandardProjectSettings.cmake` from the pinned
    `conan-ultimaker-index` source.
  - `scripta`, `stb` and `mapbox-wagyu` are header-only interface targets made by the `_iface`
    function, pointing at the pinned Scripta (slicing-debug hooks, compiled out), stb (for
    `stb_image`, which reads density images) and wagyu (polygon repair, with mapbox's
    `geometry.hpp` and `variant` headers it is written against). No other port uses these
    header libraries, so each is a source of this recipe at the version Ultimaker's recipes name.
  - `clipper::clipper` wraps [polyclipping](#port-polyclipping)'s pkg-config file.
  - `aliases.cmake`, run right after `project()` through `CMAKE_PROJECT_INCLUDE`, adds Conan's
    names as aliases of real packages: `boost::boost` for Boost's headers, `onetbb::onetbb` for
    `TBB::tbb`, and `rapidjson` for the RapidJSON include directory.
- **Only the binary is installed.** The project has no install rules; upstream's `conanfile.py`
  copies the binary and the static `_CuraEngine` library by hand. `build.sh` builds only the
  `CuraEngine` target and installs the binary.
