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


# Header-only, as Cura consumes it: HEADER_ONLY is upstream's default and the
# library target is an interface. Its CMake has no install rules (Conan's
# recipe copies the headers), so the headers are copied the same way. A
# consumer defines LIBNEST2D_GEOMETRIES_clipper, LIBNEST2D_OPTIMIZER_nlopt and
# LIBNEST2D_THREADING_std, the backends this port's depends provide.
install -d "$PKG/usr/include"
cp -r include/libnest2d "$PKG/usr/include/"
find "$PKG/usr/include/libnest2d" -type f -exec chmod 644 {} +

test -f "$PKG/usr/include/libnest2d/libnest2d.hpp"
test -f "$PKG/usr/include/libnest2d/backends/clipper/geometries.hpp"
