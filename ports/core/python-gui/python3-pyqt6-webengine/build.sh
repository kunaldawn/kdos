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

# The bindings are generated against the .sip files python3-pyqt6 installs
# under PyQt6/bindings, so the two must be the same release.
sip-build \
	--qmake /usr/lib/qt6/bin/qmake \
	--qmake-setting "QMAKE_CFLAGS_RELEASE = $CFLAGS" \
	--qmake-setting "QMAKE_CXXFLAGS_RELEASE = $CXXFLAGS" \
	--qmake-setting "QMAKE_LFLAGS_RELEASE = $LDFLAGS" \
	--build-dir build \
	--no-make \
	--verbose
make -C build

# The generated install rules race when run in parallel.
make -C build -j1 INSTALL_ROOT="$PKG" install

site=$(find "$PKG"/usr/lib -maxdepth 2 -type d -name site-packages)
for m in QtWebEngineCore QtWebEngineWidgets QtWebEngineQuick; do
	test -n "$(find "$site/PyQt6" -maxdepth 1 -name "$m.*.so")"
done
