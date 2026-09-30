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

# --without-yaml: libyaml serves only lou_checkyaml, upstream's table test
# driver, so the tool is not built — and its manual page, which the install
# ships regardless, goes with it.
./configure --prefix=/usr --sysconfdir=/etc --libdir=/usr/lib --disable-static \
	--without-yaml
make
make DESTDIR=$PKG install
rm "$PKG/usr/share/man/man1/lou_checkyaml.1"

# THE louis PYTHON MODULE is how Orca translates contracted braille. It is a
# ctypes binding, a pyproject of its own that make install does not install;
# make writes its __init__.py with the soname of the library just built, so
# it is installed from python/ after the build and loads that library.
(cd python && pip3 install --no-deps --no-index --no-build-isolation \
	--root=$PKG --prefix=/usr .)
