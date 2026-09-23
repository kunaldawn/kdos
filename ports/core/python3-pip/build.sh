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

mkdir -p vendor
tar -xf $PORT_SRC/$name-vendor-$version.tar.xz --strip-components=1 -C vendor
PYTHONPATH=src python3 -m pip install . --ignore-installed --no-deps --find-links=vendor --no-index --root="$PKG" --prefix=/usr

# distlib's Windows script launchers are prebuilt PE binaries; distlib reads
# them only when os.name is 'nt', so nothing on this system can load them.
find "$PKG/usr/lib" -path '*/pip/_vendor/distlib/*.exe' -delete
