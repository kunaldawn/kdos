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

_shim="$SRC_ROOT/shim"
mkdir -p "$_shim"
printf 'include("%s")\n' \
	"$SRC_ROOT/conan-ultimaker-index-$_index/recipes/standardprojectsettings/all/StandardProjectSettings.cmake" \
	> "$_shim/standardprojectsettings-config.cmake"
cat > "$_shim/clipper-config.cmake" <<'CMAKE'
find_package(PkgConfig REQUIRED)
pkg_check_modules(POLYCLIPPING REQUIRED IMPORTED_TARGET polyclipping)
if(NOT TARGET clipper::clipper)
	add_library(clipper::clipper INTERFACE IMPORTED)
	target_link_libraries(clipper::clipper INTERFACE PkgConfig::POLYCLIPPING)
endif()
CMAKE

cmake -S . -B build -G Ninja \
	-DCMAKE_BUILD_TYPE=Release \
	-DCMAKE_CXX_STANDARD=20 \
	-DCMAKE_INSTALL_PREFIX=/usr \
	-DCMAKE_POSITION_INDEPENDENT_CODE=ON \
	-DUVULA_VERSION="$version" \
	-DPYUVULA_VERSION="$version" \
	-DWITH_PYTHON_BINDINGS=ON \
	-DWITH_JS_BINDINGS=OFF \
	-DWITH_CLI=OFF \
	-DEXTENSIVE_WARNINGS=OFF \
	-Dstandardprojectsettings_DIR="$_shim" \
	-Dclipper_DIR="$_shim" \
	-Dpybind11_DIR="$(python3 -m pybind11 --cmakedir)"
ninja -C build pyUvula

_site=$(python3 -c 'import sysconfig; print(sysconfig.get_path("platlib", vars={"platbase": "/usr", "base": "/usr"}))')
install -d "$PKG$_site"
install -m755 build/pyUvula/pyUvula*.so "$PKG$_site/"
