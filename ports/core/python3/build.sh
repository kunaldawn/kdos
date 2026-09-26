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

# pip is the python3-pip port, built from its source; --with-ensurepip=no
# keeps `make install` from unpacking the prebuilt wheel CPython carries into
# the same paths, and the wheel itself is deleted below. ensurepip, and so a
# plain `python3 -m venv`, installs pip only from a pip-*.whl in
# --with-wheel-pkg-dir, and python3-pip builds that wheel from the same source
# it installs.
#
# The rest pins what configure would otherwise decide from whatever happens to
# be installed. libmpdec is not a port, so _decimal uses the copy CPython
# carries. Bluetooth sockets need bluez's headers, and bluez is built long
# after this bootstrap. _tkinter needs Tk, which is X11, so the Tk-only
# stdlib — tkinter, turtle, idlelib, turtledemo and the idle3 launchers — is
# removed below: every one of them fails on import. --disable-test-modules
# drops the _test* and xx* extension modules, which only the test suite this
# package does not ship imports; the PGO training run skips what needs them.
#
# OpenSSL 4 has no SSLv3 and no per-version method constructors, and declares
# no OPENSSL_NO_* guard for either; _ssl declares and calls them unless the
# guards are set, and fails to import on the unresolved symbols.
./configure \
	--prefix=/usr \
	--enable-ipv6 \
	--enable-loadable-sqlite-extensions \
	--enable-optimizations \
	--enable-shared \
	--with-computed-gotos \
	--with-lto \
	--with-system-expat \
	--without-system-libmpdec \
	--with-readline=readline \
	--with-ensurepip=no \
	--with-wheel-pkg-dir=/usr/share/python-wheels \
	--with-tzpath=/usr/share/zoneinfo \
	--disable-test-modules \
	ac_cv_header_bluetooth_bluetooth_h=no \
	ac_cv_header_bluetooth_h=no \
	py_cv_module__tkinter=n/a \
	CPPFLAGS="$CPPFLAGS -DOPENSSL_NO_SSL3 -DOPENSSL_NO_SSL3_METHOD -DOPENSSL_NO_TLS1_METHOD -DOPENSSL_NO_TLS1_1_METHOD -DOPENSSL_NO_TLS1_2_METHOD"
make EXTRA_CFLAGS="$CFLAGS"
make EXTRA_CFLAGS="$CFLAGS" DESTDIR=$PKG install maninstall

rm -rf $PKG/usr/lib/python${_version}/test
rm -rf $PKG/usr/lib/python${_version}/ensurepip/_bundled
rm -rf $PKG/usr/lib/python${_version}/{tkinter,idlelib,turtledemo,turtle.py}
rm -f $PKG/usr/bin/idle3 $PKG/usr/bin/idle${_version}

# THE SYSTEM site-packages BELONGS TO kpkg (PEP 668). Every module under it is
# a port's file, and `sudo pip install` over one replaces it with no warning;
# the next upgrade or removal of that port then deletes or conflicts with what
# pip put there. pip refuses to install outside a virtual environment while
# this file exists. A port's own `pip install --root=$PKG` is unaffected: pip
# does not consult the marker for an install into --root, --prefix or
# --target.
cat > "$PKG/usr/lib/python${_version}/EXTERNALLY-MANAGED" <<'EOF'
[externally-managed]
Error=The system Python belongs to kpkg: every package in its
 site-packages is a KDOS port, and pip installing over one leaves
 files the next kpkg upgrade or removal deletes or conflicts with.
 
 Install into a virtual environment instead:
 
 python3 -m venv ~/.venvs/NAME
 ~/.venvs/NAME/bin/pip install PACKAGE
 
 A Python package the system itself should carry is a port.
EOF
