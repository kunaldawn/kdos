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

./configure \
	--prefix=/usr \
	--enable-ipv6 \
	--enable-loadable-sqlite-extensions \
	--enable-optimizations \
	--enable-shared \
	--with-computed-gotos \
	--with-lto \
	--with-system-expat \
	--with-system-libmpdec \
	--with-readline=readline \
	--with-ensurepip=no \
	--with-wheel-pkg-dir=/usr/share/python-wheels \
	--with-tzpath=/usr/share/zoneinfo \
	--disable-test-modules \
	ac_cv_header_bluetooth_bluetooth_h=no \
	ac_cv_header_bluetooth_h=no \
	py_cv_module__tkinter=n/a \
	ac_cv_prog_cc_g=no \
	OPT="-O3 -Wall" \
	CFLAGS_ALIASING=-fno-strict-aliasing \
	CPPFLAGS="$CPPFLAGS -DOPENSSL_NO_SSL3 -DOPENSSL_NO_SSL3_METHOD -DOPENSSL_NO_TLS1_METHOD -DOPENSSL_NO_TLS1_1_METHOD -DOPENSSL_NO_TLS1_2_METHOD"
make EXTRA_CFLAGS="$CFLAGS"
make EXTRA_CFLAGS="$CFLAGS" DESTDIR=$PKG install maninstall

rm -rf $PKG/usr/lib/python${_version}/test
rm -rf $PKG/usr/lib/python${_version}/ensurepip/_bundled
rm -rf $PKG/usr/lib/python${_version}/{tkinter,idlelib,turtledemo,turtle.py}
rm -f $PKG/usr/bin/idle3 $PKG/usr/bin/idle${_version}
ln -s python3 $PKG/usr/bin/python
ln -s python3.1 $PKG/usr/share/man/man1/python.1

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
