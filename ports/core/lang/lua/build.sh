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

patch -p1 -i $PORT_SRC/lua-root-usr.patch
make CC=cc MYCFLAGS="$CFLAGS -DLUA_COMPAT_5_2 -DLUA_COMPAT_5_1 -fPIC" MYLDFLAGS="$LDFLAGS" \
	linux-readline

cd src
cc -shared $LDFLAGS -ldl -Wl,-soname,liblua.so.${_majorver} -o liblua.so.$version -Wl,-whole-archive liblua.a -Wl,-no-whole-archive
ln -s liblua.so.$version liblua.so.${_majorver}
ln -s liblua.so.$version liblua.so
cd ..

make INSTALL_TOP=$PKG/usr \
     TO_LIB="liblua.so liblua.so.$_majorver liblua.so.$version" \
     INSTALL_DATA="cp -d" \
     INSTALL_MAN=$PKG/usr/share/man/man1 install

install -d $PKG/usr/lib/pkgconfig
cat > $PKG/usr/lib/pkgconfig/lua.pc << EOF
V=$_majorver
R=$version
EOF
cat >> $PKG/usr/lib/pkgconfig/lua.pc << "EOF"

prefix=/usr
INSTALL_BIN=${prefix}/bin
INSTALL_INC=${prefix}/include
INSTALL_LIB=${prefix}/lib
INSTALL_MAN=${prefix}/share/man/man1
INSTALL_LMOD=${prefix}/share/lua/${V}
INSTALL_CMOD=${prefix}/lib/lua/${V}
exec_prefix=${prefix}
libdir=${exec_prefix}/lib
includedir=${prefix}/include

Name: Lua
Description: An Extensible Extension Language
Version: ${R}
Requires:
Libs: -L${libdir} -llua -lm -ldl
Cflags: -I${includedir}
EOF

ln -s lua.pc $PKG/usr/lib/pkgconfig/lua$_majorver.pc
