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

export LDFLAGS="$LDFLAGS -Wl,--strip-debug"
patch -p1 -i "$PORT_SRC/no-stacktrace.patch"
python3 build.py build_py --use_syswx --gtk3 --release --jobs="$KDOS_JOBS"
python3 build.py install --use_syswx --gtk3 --release --destdir="$PKG" \
	--extra_setup="--prefix=/usr"
test -n "$(find "$PKG"/usr/lib/python3*/site-packages/wx -maxdepth 1 -name '_core.*.so')"
test -n "$(find "$PKG"/usr/lib/python3*/site-packages/wx/svg -maxdepth 1 -name '_nanosvg.*.so')"
test -z "$(readelf -S "$(find "$PKG"/usr/lib/python3*/site-packages/wx -maxdepth 1 -name '_core.*.so')" | grep '\.debug_info')"
