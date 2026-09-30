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

# define-stl makes the installed header always select the std::string API,
# so a consumer and the library cannot disagree about the class layout.
for p in define-stl entity CVE-2021-42260 CVE-2023-34194; do
	patch -p1 -i "$PORT_SRC/$p.patch"
done

# Upstream's makefile builds only the test program and no library at all, so
# the shared library is compiled here directly, under the soname other
# distributions give it.
for f in tinyxml tinystr tinyxmlerror tinyxmlparser; do
	$CXX $CXXFLAGS -fPIC -DTIXML_USE_STL -c "$f.cpp" -o "$f.o"
done
$CXX $CXXFLAGS $LDFLAGS -shared -Wl,-soname,libtinyxml.so.0 \
	-o libtinyxml.so.0.$version \
	tinyxml.o tinystr.o tinyxmlerror.o tinyxmlparser.o

install -Dm755 libtinyxml.so.0.$version "$PKG/usr/lib/libtinyxml.so.0.$version"
ln -s libtinyxml.so.0.$version "$PKG/usr/lib/libtinyxml.so.0"
ln -s libtinyxml.so.0.$version "$PKG/usr/lib/libtinyxml.so"
install -Dm644 tinyxml.h "$PKG/usr/include/tinyxml.h"
install -Dm644 tinystr.h "$PKG/usr/include/tinystr.h"

# Consumers such as Kodi look for the library through pkg-config first.
install -d "$PKG/usr/lib/pkgconfig"
cat > "$PKG/usr/lib/pkgconfig/tinyxml.pc" <<EOF
prefix=/usr
exec_prefix=\${prefix}
libdir=\${exec_prefix}/lib
includedir=\${prefix}/include

Name: TinyXML
Description: Small C++ XML parser
Version: $version
Libs: -L\${libdir} -ltinyxml
Cflags: -I\${includedir}
EOF
