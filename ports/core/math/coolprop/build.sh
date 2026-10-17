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

pip3 install --ignore-installed --no-deps --no-index --no-build-isolation --root=$PKG --prefix=/usr . \
	--config-settings=cmake.define.CPM_Eigen_SOURCE=/usr/include/eigen3 \
	--config-settings=cmake.define.CPM_fmt_SOURCE=/usr \
	--config-settings=cmake.define.CPM_nlohmann_json_SOURCE=/usr \
	--config-settings=cmake.define.CPM_msgpack-c_SOURCE=$SRC_ROOT/msgpack-c-$_msgpack \
	--config-settings=cmake.define.CPM_IF97_SOURCE=$SRC_ROOT/IF97-$_if97 \
	--config-settings=cmake.define.CPM_REFPROP_headers_SOURCE=$SRC_ROOT/REFPROP-headers-$_refprop \
	--config-settings=cmake.define.CPM_boost_headers_SOURCE=$SRC_ROOT/boost-headers-$_boost \
	--config-settings=cmake.define.CPM_multicomplex_SOURCE=$SRC_ROOT/multicomplex-$_multicomplex \
	--config-settings=cmake.define.CPM_valijson_SOURCE=$SRC_ROOT/valijson-$_valijson
