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

for triple in amdgcn-amd-amdhsa-llvm nvptx64-nvidia-cuda \
		spirv32-unknown-unknown spirv64-unknown-unknown; do
	cmake -S libclc -B build-$triple -G Ninja \
		-D CMAKE_INSTALL_PREFIX=/usr \
		-D CMAKE_BUILD_TYPE=Release \
		-D CMAKE_C_FLAGS_RELEASE="$CFLAGS" \
		-D CMAKE_CXX_FLAGS_RELEASE="$CXXFLAGS" \
		-D CMAKE_CLC_COMPILER=/usr/bin/clang \
		-D LLVM_DIR=/usr/lib/cmake/llvm \
		-D LLVM_DEFAULT_TARGET_TRIPLE=$triple \
		-D LIBCLC_USE_SPIRV_BACKEND=ON \
		-D LLVM_INCLUDE_TESTS=OFF

	cmake --build build-$triple
	DESTDIR=$PKG cmake --install build-$triple
done

# Mesa's rusticl loads spirv-mesa3d-.spv and spirv64-mesa3d-.spv from the
# libexecdir of libclc.pc; this libclc installs one directory per triple and
# no pkg-config file, and without both Mesa stops at setup.
ln -s spirv32-unknown-unknown/libclc.spv "$PKG/usr/share/clc/spirv-mesa3d-.spv"
ln -s spirv64-unknown-unknown/libclc.spv "$PKG/usr/share/clc/spirv64-mesa3d-.spv"
install -d "$PKG/usr/share/pkgconfig"
cat > "$PKG/usr/share/pkgconfig/libclc.pc" <<PC
prefix=/usr
datarootdir=\${prefix}/share
datadir=\${datarootdir}
pkgdatadir=\${datadir}/clc
libexecdir=\${pkgdatadir}

Name: libclc
Description: Library requirements of the OpenCL C programming language
Version: $version
PC
chmod 644 "$PKG/usr/share/pkgconfig/libclc.pc"
