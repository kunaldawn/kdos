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


# The 1.1.0 headers do not compile under a current g++ without the first patch
# (GenericStringRef's assignment operator has no return). The second bounds
# GenericReader's recursion depth on malformed input (CVE-2024-38517).
patch -p1 -i "$PORT_SRC/gcc14.patch"
patch -p1 -i "$PORT_SRC/CVE-2024-38517.patch"

# Header-only: tests, examples and the Doxygen reference are not built.
mkdir build && cd build
cmake .. -G Ninja -DCMAKE_POLICY_VERSION_MINIMUM=3.5 -DCMAKE_BUILD_TYPE=Release \
	-DCMAKE_INSTALL_PREFIX=/usr \
	-DRAPIDJSON_BUILD_DOC=OFF \
	-DRAPIDJSON_BUILD_EXAMPLES=OFF \
	-DRAPIDJSON_BUILD_TESTS=OFF
ninja
DESTDIR=$PKG ninja install

# The install copies the example sources and the readme as documentation.
rm -rf "$PKG/usr/share/doc"
