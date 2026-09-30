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

# The sdist carries the sip-generated C++ for the wxWidgets release it names,
# which is the release the wxwidgets port builds, so the bindings compile as
# shipped and no sip is run. wxWidgets on musl has no wxStackWalker (there is
# no backtrace()), so the one binding that calls it returns an empty trace.
patch -p1 -i "$PORT_SRC/no-stacktrace.patch"

# build.py fetches waf unless bin/waf-<version> is present with the hash it
# pins. The sdist ships that file, so the build opens no connection; a release
# whose bin/ lacks it makes build.py try the network and fail.

# --use_syswx links the installed wxWidgets through wx-config instead of
# building the copy under ext/, so every wxPython program shares the one wx
# the C++ programs use.
python3 build.py build_py --use_syswx --gtk3 --release --jobs="$(nproc)"
python3 build.py install --use_syswx --gtk3 --release --destdir="$PKG" \
	--extra_setup="--prefix=/usr"

test -n "$(find "$PKG"/usr/lib/python3*/site-packages/wx -maxdepth 1 -name '_core.*.so')"
test -n "$(find "$PKG"/usr/lib/python3*/site-packages/wx/svg -maxdepth 1 -name '_nanosvg.*.so')"
