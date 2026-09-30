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

mkdir -p build && cd build
# THE TWO AUTOLOAD FLAGS ARE THE POINT. By default duckdb DOWNLOADS an
# extension from duckdb.org the first time a query needs one — reading a
# Parquet file on a machine with no network then fails at run time with a
# message about an extension rather than about the network. Off, the extensions
# compiled in are all there are, and that is knowable — so icu (time zones,
# TIMESTAMPTZ, collations, from its vendored ICU) and the shell's tab
# completion are compiled in, or those queries fail with that same message.
cmake .. -G Ninja -DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
	-DCMAKE_BUILD_TYPE=Release \
	-DCMAKE_INSTALL_PREFIX=/usr -DCMAKE_INSTALL_LIBDIR=lib \
	-DBUILD_SHELL=ON -DBUILD_UNITTESTS=OFF \
	-DENABLE_EXTENSION_AUTOLOADING=0 -DENABLE_EXTENSION_AUTOINSTALL=0 \
	-DBUILD_EXTENSIONS="parquet;json;icu;autocomplete"
ninja
DESTDIR=$PKG ninja install
