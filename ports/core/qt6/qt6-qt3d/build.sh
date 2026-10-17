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

mkdir build && cd build
cmake .. -G Ninja \
	-DCMAKE_BUILD_TYPE=Release \
	-DCMAKE_INSTALL_PREFIX=/usr \
	-DQT_BUILD_TESTS=OFF \
	-DQT_BUILD_EXAMPLES=OFF \
	-DINPUT_assimp=qt \
	-DFEATURE_qt3d_assimp=ON \
	-DFEATURE_qt3d_system_assimp=OFF \
	-DFEATURE_qt3d_render=ON \
	-DFEATURE_qt3d_input=ON \
	-DFEATURE_qt3d_logic=ON \
	-DFEATURE_qt3d_extras=ON \
	-DFEATURE_qt3d_animation=ON \
	-DFEATURE_qt3d_opengl_renderer=ON \
	-DFEATURE_qt3d_rhi_renderer=ON \
	-DFEATURE_qt3d_vulkan=ON \
	-DFEATURE_qt3d_fbxsdk=OFF
ninja
DESTDIR=$PKG ninja install
