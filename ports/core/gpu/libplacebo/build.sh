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

meson setup build --prefix=/usr --libdir=lib --buildtype=release \
	-Dvulkan=enabled -Dvk-proc-addr=enabled \
	-Dvulkan-registry=/usr/share/vulkan/registry/vk.xml \
	-Dglslang=enabled -Dshaderc=disabled \
	-Dopengl=enabled -Dgl-proc-addr=enabled -Dd3d11=disabled \
	-Ddovi=enabled -Dlibdovi=disabled -Dunwind=disabled \
	-Dlcms=enabled -Dxxhash=enabled \
	-Ddemos=false -Dtests=false -Dbench=false -Dfuzz=false
meson compile -C build
DESTDIR=$PKG meson install --no-rebuild -C build
