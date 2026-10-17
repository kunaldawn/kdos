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


patch -d "$SRC_ROOT/wagyu-$_wagyu" -p1 -i "$PORT_SRC/wagyu-execinfo.patch"

patch -p1 -i "$PORT_SRC/include-iomanip.patch"

_shim="$SRC_ROOT/shim"
mkdir -p "$_shim"
printf 'include("%s")\n' \
	"$SRC_ROOT/conan-ultimaker-index-$_index/recipes/standardprojectsettings/all/StandardProjectSettings.cmake" \
	> "$_shim/standardprojectsettings-config.cmake"

_iface() {
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

install -Dm755 build/CuraEngine "$PKG/usr/bin/CuraEngine"
