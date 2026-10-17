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

cmake -S . -B build -G Ninja \
	-DCMAKE_BUILD_TYPE=Release \
	-DCMAKE_CXX_STANDARD=20 \
	-DCMAKE_INSTALL_PREFIX=/usr \
	-DBUILD_SHARED_LIBS=ON \
	-DENABLE_TESTING=OFF \
	-Dstandardprojectsettings_DIR="$_shim"
ninja -C build

install -Dm755 build/libSavitar.so "$PKG/usr/lib/libSavitar.so"
install -d "$PKG/usr/include/Savitar"
install -m644 include/Savitar/*.h "$PKG/usr/include/Savitar/"
install -d "$PKG/usr/lib/cmake/savitar"
cat > "$PKG/usr/lib/cmake/savitar/savitar-config.cmake" <<'CMAKE'
include(CMakeFindDependencyMacro)
find_dependency(pugixml CONFIG)
get_filename_component(_savitar_prefix "${CMAKE_CURRENT_LIST_DIR}/../../.." ABSOLUTE)
if(NOT TARGET savitar::savitar)
	add_library(savitar::savitar SHARED IMPORTED)
	set_target_properties(savitar::savitar PROPERTIES
		IMPORTED_LOCATION "${_savitar_prefix}/lib/libSavitar.so"
		INTERFACE_INCLUDE_DIRECTORIES "${_savitar_prefix}/include"
		INTERFACE_LINK_LIBRARIES pugixml::pugixml)
endif()
set(savitar_FOUND TRUE)
CMAKE
