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

sed -i '/#define LUA_ROOT/s:/usr/local/:/usr/:' src/luaconf.h
make CC=cc MYCFLAGS="-DLUA_COMPAT_5_2 -DLUA_COMPAT_5_1 -fPIC" linux

cd src
cc -shared -ldl -Wl,-soname,liblua.so.${_majorver} -o liblua.so.$version -Wl,-whole-archive liblua.a -Wl,-no-whole-archive
ln -s liblua.so.$version liblua.so.${_majorver}
ln -s liblua.so.$version liblua.so
cd ..

make INSTALL_TOP=$PKG/usr \
     TO_LIB="liblua.so liblua.so.$_majorver liblua.so.$version" \
     INSTALL_DATA="cp -d" \
     INSTALL_MAN=$PKG/usr/share/man/man1 install

install -d $PKG/usr/lib/pkgconfig
# TWO HEREDOCS, AND THE SPLIT IS THE POINT. Only V and R are ours to expand;
# every ${prefix}, ${libdir} and ${V} below is a pkg-config VARIABLE and must
# reach the file literally. One quoted heredoc froze V and R too, so
# `Version:` read as the literal ${R} and every consumer asking for a version
# — tio's `dependency('lua', version: '>=5.1')` — was told lua is absent while
# it sat installed with this file in place.
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
