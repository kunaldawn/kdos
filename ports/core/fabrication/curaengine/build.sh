#!/bin/bash
# ██╗  ██╗██████╗  ██████╗ ███████╗
# ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
# █████╔╝ ██║  ██║██║   ██║███████╗
# ██╔═██╗ ██║  ██║██║   ██║╚════██║
# ██║  ██╗██████╔╝╚██████╔╝███████║
# ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
# ---------------------------------
#   KD's Homebrew Linux Distro
# ---------------------------------


# musl has no execinfo.h; wagyu includes it only for a debug backtrace, and
# the patch from Ultimaker's own recipe includes it only where it exists.
patch -d "$SRC_ROOT/wagyu-$_wagyu" -p1 -i "$PORT_SRC/wagyu-execinfo.patch"

# OBJ.cpp calls std::setprecision without including <iomanip>, and none of the
# headers it does include brings it in here; the patch includes it.
patch -p1 -i "$PORT_SRC/include-iomanip.patch"

# CuraEngine's CMake is written for Conan: it loads its compiler settings as
# a package called standardprojectsettings, and links every dependency by the
# target name Conan's recipes generate (boost::boost, onetbb::onetbb,
# clipper::clipper, rapidjson, ...). The shim answers each name from this
# system's own packages, and nothing of it reaches the package. Five
# header-only libraries no other port uses are pinned beside the source at
# the versions Ultimaker's recipes name: Scripta (the slicing-debug hooks,
# compiled out), stb_image (density images), and mapbox's wagyu polygon
# repair with the geometry and variant headers it is written against.
_shim="$SRC_ROOT/shim"
mkdir -p "$_shim"
printf 'include("%s")\n' \
	"$SRC_ROOT/conan-ultimaker-index-$_index/recipes/standardprojectsettings/all/StandardProjectSettings.cmake" \
	> "$_shim/standardprojectsettings-config.cmake"

_iface() {
	# _iface <package> <target> <include dir>... : a header-only target
	_pkg=$1 _tgt=$2; shift 2
	{
		printf 'if(NOT TARGET %s)\n' "$_tgt"
		printf '\tadd_library(%s INTERFACE IMPORTED)\n' "$_tgt"
		printf '\tset_target_properties(%s PROPERTIES INTERFACE_INCLUDE_DIRECTORIES "%s")\n' \
			"$_tgt" "$*"
		printf 'endif()\n'
	} > "$_shim/$_pkg-config.cmake"
}
_iface scripta scripta::scripta "$SRC_ROOT/Scripta_public-$_scripta/include"
_iface stb stb::stb "$SRC_ROOT/stb-$_stb"
_iface mapbox-wagyu mapbox-wagyu::mapbox-wagyu \
	"$SRC_ROOT/wagyu-$_wagyu/include;$SRC_ROOT/geometry.hpp-$_geometry/include;$SRC_ROOT/variant-$_variant/include"

cat > "$_shim/clipper-config.cmake" <<'CMAKE'
find_package(PkgConfig REQUIRED)
pkg_check_modules(POLYCLIPPING REQUIRED IMPORTED_TARGET polyclipping)
if(NOT TARGET clipper::clipper)
	add_library(clipper::clipper INTERFACE IMPORTED)
	target_link_libraries(clipper::clipper INTERFACE PkgConfig::POLYCLIPPING)
endif()
CMAKE

# Boost, oneTBB and RapidJSON are real packages here, under other target
# names; the project include runs right after project() and adds Conan's
# names as aliases.
cat > "$_shim/aliases.cmake" <<'CMAKE'
find_package(Boost CONFIG REQUIRED)
add_library(boost::boost INTERFACE IMPORTED)
target_link_libraries(boost::boost INTERFACE Boost::headers)
find_package(TBB CONFIG REQUIRED)
add_library(onetbb::onetbb INTERFACE IMPORTED)
target_link_libraries(onetbb::onetbb INTERFACE TBB::tbb)
find_path(RAPIDJSON_SHIM_INCLUDE rapidjson/rapidjson.h REQUIRED)
add_library(rapidjson INTERFACE IMPORTED)
set_target_properties(rapidjson PROPERTIES INTERFACE_INCLUDE_DIRECTORIES "${RAPIDJSON_SHIM_INCLUDE}")
CMAKE

# Arcus is the socket Cura drives the engine through. The gRPC plugin system
# (ENABLE_PLUGINS) needs Ultimaker's plugin definitions and asio-grpc and is
# off; Sentry crash reporting is off. Conan's toolchain is what sets C++20.
cmake -S . -B build -G Ninja -Wno-dev \
	-DCMAKE_BUILD_TYPE=Release \
	-DCMAKE_CXX_STANDARD=20 \
	-DCMAKE_INSTALL_PREFIX=/usr \
	-DCURA_ENGINE_VERSION="$version" \
	-DENABLE_ARCUS=ON \
	-DENABLE_PLUGINS=OFF \
	-DENABLE_REMOTE_PLUGINS=OFF \
	-DENABLE_SENTRY=OFF \
	-DENABLE_TESTING=OFF \
	-DENABLE_BENCHMARKS=OFF \
	-DEXTENSIVE_WARNINGS=OFF \
	-DENABLE_MORE_COMPILER_OPTIMIZATION_FLAGS=OFF \
	-DENABLE_THREADING=ON \
	-Dprotobuf_MODULE_COMPATIBLE=ON \
	-Dstandardprojectsettings_DIR="$_shim" \
	-Dscripta_DIR="$_shim" \
	-Dstb_DIR="$_shim" \
	-Dmapbox-wagyu_DIR="$_shim" \
	-Dclipper_DIR="$_shim" \
	-DCMAKE_PROJECT_INCLUDE="$_shim/aliases.cmake"
ninja -C build CuraEngine

# The project has no install rules: Conan's recipe copies the one binary.
# Cura finds it at /usr/bin/CuraEngine. On its own, the engine reads machine
# definitions from the directories CURA_ENGINE_SEARCH_PATH names; the cura
# port installs them under /usr/share/cura/resources.
install -Dm755 build/CuraEngine "$PKG/usr/bin/CuraEngine"
