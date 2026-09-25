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
# --with-wheel-pkg-dir. With no wheel there, venv leaves an environment with no
# pip and no activate scripts and exits 1; `--without-pip` works, and adds the
# system pip with `--system-site-packages`.
#
# The rest pins what configure would otherwise decide from whatever happens to
# be installed. libmpdec is not a port, so _decimal uses the copy CPython
# carries. Bluetooth sockets need bluez's headers, and bluez is built long
# after this bootstrap. _tkinter needs Tk, which is X11.
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
	ac_cv_header_bluetooth_bluetooth_h=no \
	ac_cv_header_bluetooth_h=no \
	py_cv_module__tkinter=n/a \
	CPPFLAGS="$CPPFLAGS -DOPENSSL_NO_SSL3 -DOPENSSL_NO_SSL3_METHOD -DOPENSSL_NO_TLS1_METHOD -DOPENSSL_NO_TLS1_1_METHOD -DOPENSSL_NO_TLS1_2_METHOD"
make EXTRA_CFLAGS="$CFLAGS"
make EXTRA_CFLAGS="$CFLAGS" DESTDIR=$PKG install maninstall

rm -rf $PKG/usr/lib/python${_version}/test
rm -rf $PKG/usr/lib/python${_version}/ensurepip/_bundled
