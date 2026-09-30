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


# Ultimaker's CMake files are written for Conan: they load their compiler
# settings as a package called standardprojectsettings, which only Conan's
# index carries. The shim answers that name from the index at the pinned
# commit, and nothing of it reaches the package.
_shim="$SRC_ROOT/shim"
mkdir -p "$_shim"
printf 'include("%s")\n' \
	"$SRC_ROOT/conan-ultimaker-index-$_index/recipes/standardprojectsettings/all/StandardProjectSettings.cmake" \
	> "$_shim/standardprojectsettings-config.cmake"

# Conan's toolchain is what sets the C++ standard the project is written in;
# without it the compiler's default applies.
cmake -S . -B build -G Ninja \
	-DCMAKE_BUILD_TYPE=Release \
	-DCMAKE_CXX_STANDARD=20 \
	-DCMAKE_INSTALL_PREFIX=/usr \
	-DBUILD_SHARED_LIBS=ON \
	-DENABLE_SENTRY=OFF \
	-Dstandardprojectsettings_DIR="$_shim"
ninja -C build

# The project has no install rules: Conan's recipe copies the headers and the
# library, and generates the arcus::arcus target CuraEngine links. The same
# three things are laid out here, the target as a CMake package file.
install -Dm755 build/libArcus.so "$PKG/usr/lib/libArcus.so"
install -d "$PKG/usr/include/Arcus"
install -m644 include/Arcus/*.h "$PKG/usr/include/Arcus/"
install -d "$PKG/usr/lib/cmake/arcus"
cat > "$PKG/usr/lib/cmake/arcus/arcus-config.cmake" <<'CMAKE'
include(CMakeFindDependencyMacro)
find_dependency(protobuf CONFIG)
get_filename_component(_arcus_prefix "${CMAKE_CURRENT_LIST_DIR}/../../.." ABSOLUTE)
if(NOT TARGET arcus::arcus)
	add_library(arcus::arcus SHARED IMPORTED)
	set_target_properties(arcus::arcus PROPERTIES
		IMPORTED_LOCATION "${_arcus_prefix}/lib/libArcus.so"
		INTERFACE_INCLUDE_DIRECTORIES "${_arcus_prefix}/include"
		INTERFACE_LINK_LIBRARIES protobuf::libprotobuf)
endif()
set(arcus_FOUND TRUE)
CMAKE
