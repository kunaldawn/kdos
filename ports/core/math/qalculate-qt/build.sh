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


# qmake 6 reads no compiler flags from the environment, so they are handed to
# it as QMAKE_* assignments; without them the reproducibility flags never
# reach the binary. lrelease compiles every catalogue the project lists and
# the install copies all of them, so all but English are removed from the
# package: bundled data is English only. Plotting runs gnuplot at run time.
qmake6 PREFIX=/usr CONFIG+=release \
	QMAKE_CFLAGS_RELEASE="$CFLAGS" \
	QMAKE_CXXFLAGS_RELEASE="$CXXFLAGS" \
	QMAKE_LFLAGS_RELEASE="$LDFLAGS"
make
make INSTALL_ROOT=$PKG install

find "$PKG/usr/share/qalculate-qt/translations" -name '*.qm' ! -name '*_en.qm' -delete
