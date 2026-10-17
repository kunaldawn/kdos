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

patch -p1 -i "$PORT_SRC/musl-close-range.patch"

export VERSION="$version"

install -d "$SRC_ROOT/fmt-shim"
printf '#!/bin/sh\nexec cat\n' > "$SRC_ROOT/fmt-shim/fmt"
chmod 755 "$SRC_ROOT/fmt-shim/fmt"
export PATH="$SRC_ROOT/fmt-shim:$PATH"

make

make DESTDIR="$PKG" prefix=/usr install
