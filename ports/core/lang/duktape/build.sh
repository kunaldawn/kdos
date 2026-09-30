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

cd src
cc -shared -fPIC -O2 \
	-Wl,-soname,libduktape.so.207 \
	-o libduktape.so.207.0.0 \
	duktape.c -lm
cd ..

install -Dm755 src/libduktape.so.207.0.0 "$PKG/usr/lib/libduktape.so.207.0.0"
ln -sf libduktape.so.207.0.0 "$PKG/usr/lib/libduktape.so.207"
ln -sf libduktape.so.207.0.0 "$PKG/usr/lib/libduktape.so"
install -Dm644 src/duktape.h "$PKG/usr/include/duktape.h"
install -Dm644 src/duk_config.h "$PKG/usr/include/duk_config.h"

install -dm755 "$PKG/usr/lib/pkgconfig"
cat > "$PKG/usr/lib/pkgconfig/duktape.pc" <<EOF
prefix=/usr
exec_prefix=\${prefix}
libdir=\${prefix}/lib
includedir=\${prefix}/include

Name: duktape
Description: Embeddable JavaScript engine
Version: $version
Libs: -L\${libdir} -lduktape
Libs.private: -lm
Cflags: -I\${includedir}
EOF
