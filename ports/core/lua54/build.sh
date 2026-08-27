# ██╗  ██╗██████╗  ██████╗ ███████╗
# ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
# █████╔╝ ██║  ██║██║   ██║███████╗
# ██╔═██╗ ██║  ██║██║   ██║╚════██║
# ██║  ██╗██████╔╝╚██████╔╝███████║
# ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
# ---------------------------------
#   KD's Homebrew Linux Distro
# ---------------------------------

# A PARALLEL INSTALL, NOT A REPLACEMENT. ports/core/lua is 5.5.0 and stays the
# system lua; Prosody 13 supports 5.2 through 5.4 and will not load on 5.5, so
# this exists for it and for anything else in the same position.
#
# EVERY INSTALLED PATH IS VERSIONED, which is the whole of what keeps the two
# apart: the interpreter is `lua5.4`, the headers live in include/lua5.4, the
# library is liblua5.4, and the pkg-config file is lua5.4.pc. Nothing here
# writes a path that ports/core/lua also writes, so kpkg reports no conflict
# and neither package can shadow the other.
sed -i '/#define LUA_ROOT/s:/usr/local/:/usr/:' src/luaconf.h
make CC=cc MYCFLAGS="-DLUA_COMPAT_5_3 -fPIC" linux

cd src
cc -shared -ldl -Wl,-soname,liblua${_majorver}.so.${_majorver} \
   -o liblua${_majorver}.so.$version \
   -Wl,-whole-archive liblua.a -Wl,-no-whole-archive
cd ..

install -Dm755 src/lua  $PKG/usr/bin/lua$_majorver
install -Dm755 src/luac $PKG/usr/bin/luac$_majorver
install -d $PKG/usr/include/lua$_majorver
install -m644 src/lua.h src/luaconf.h src/lualib.h src/lauxlib.h src/lua.hpp \
        $PKG/usr/include/lua$_majorver/
install -Dm755 src/liblua${_majorver}.so.$version \
        $PKG/usr/lib/liblua${_majorver}.so.$version
ln -s liblua${_majorver}.so.$version $PKG/usr/lib/liblua${_majorver}.so.${_majorver}
ln -s liblua${_majorver}.so.$version $PKG/usr/lib/liblua${_majorver}.so
install -Dm644 src/liblua.a $PKG/usr/lib/liblua${_majorver}.a

install -d $PKG/usr/lib/lua/$_majorver $PKG/usr/share/lua/$_majorver

# TWO HEREDOCS, AND THE SPLIT IS THE POINT — the same one ports/core/lua makes.
# Only V and R are ours to expand; every ${prefix}, ${libdir} and ${V} below is
# a pkg-config VARIABLE and must reach the file literally.
install -d $PKG/usr/lib/pkgconfig
cat > $PKG/usr/lib/pkgconfig/lua$_majorver.pc << EOF
V=$_majorver
R=$version
EOF
cat >> $PKG/usr/lib/pkgconfig/lua5.4.pc << "EOF"

prefix=/usr
INSTALL_BIN=${prefix}/bin
INSTALL_INC=${prefix}/include/lua${V}
INSTALL_LIB=${prefix}/lib
INSTALL_MAN=${prefix}/share/man/man1
INSTALL_LMOD=${prefix}/share/lua/${V}
INSTALL_CMOD=${prefix}/lib/lua/${V}
exec_prefix=${prefix}
libdir=${exec_prefix}/lib
includedir=${prefix}/include/lua${V}

Name: Lua
Description: An Extensible Extension Language
Version: ${R}
Requires:
Libs: -L${libdir} -llua${V} -lm -ldl
Cflags: -I${includedir}
EOF
