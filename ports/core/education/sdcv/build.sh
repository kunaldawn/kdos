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

# THE DICTIONARIES ARE NOT SHIPPED AND THE FORMAT IS WHY THIS IS THE ROW.
# StarDict is the one dictionary format with a large body of freely
# redistributable data already converted into it — Wiktionary dumps, WordNet,
# GCIDE, dozens of bilingual pairs — so somebody with a stick and no network
# can drop a directory into /usr/share/stardict/dic and have a dictionary.
# Shipping one would be picking a language for everybody.
# stardict_lib.cpp assigns g_utf8_next_char()'s const result to a gchar*, which
# a C++ compiler has rejected since it started enforcing const-correctness on
# that macro. It is upstream's bug and there is no flag that fixes it — only
# -fpermissive, which is the one the compiler itself names, and which leaves
# every other diagnostic an error.
export CXXFLAGS="$CXXFLAGS -fpermissive"

mkdir -p build && cd build
cmake .. -G Ninja \
	-DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
	-DCMAKE_BUILD_TYPE=Release \
	-DCMAKE_INSTALL_PREFIX=/usr \
	-DENABLE_NLS=ON \
	-DWITH_READLINE=ON
ninja
# `lang` IS NOT IN THE DEFAULT TARGET and the install rule does not know that:
# FindGettextTools adds it with ADD_CUSTOM_TARGET and no ALL, while the
# install unconditionally copies the directory it produces. Build it by name,
# or the install dies on a missing build/locale after everything compiled.
ninja lang
DESTDIR=$PKG ninja install
install -dm755 $PKG/usr/share/stardict/dic
