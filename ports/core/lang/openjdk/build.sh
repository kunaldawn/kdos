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

patch -p1 -i "$PORT_SRC/posix-grep.patch"

_seedjdk="$SRC_ROOT/jdk-$_seed+$_seedbuild"
_jhome=/usr/lib/jvm/java-$_feature-openjdk

_cflags="$CFLAGS -D_LARGEFILE64_SOURCE"
_cxxflags="$CXXFLAGS -D_LARGEFILE64_SOURCE"
_ldflags="$LDFLAGS"
unset CFLAGS CXXFLAGS LDFLAGS

unset MAKEFLAGS GNUMAKEFLAGS KDOS_JOBSERVER CARGO_MAKEFLAGS

bash ./configure \
	--prefix="$_jhome" \
	--with-boot-jdk="$_seedjdk" \
	--with-extra-cflags="$_cflags" \
	--with-extra-cxxflags="$_cxxflags" \
	--with-extra-ldflags="$_ldflags" \
	--with-jobs="$KDOS_JOBS" \
	--with-jvm-variants=server \
	--with-debug-level=release \
	--with-native-debug-symbols=none \
	--disable-warnings-as-errors \
	--disable-precompiled-headers \
	--disable-ccache \
	--disable-headless-only \
	--with-freetype=system \
	--with-harfbuzz=system \
	--with-zlib=system \
	--with-libjpeg=system \
	--with-giflib=system \
	--with-libpng=system \
	--with-lcms=system \
	--with-jtreg=no \
	--with-gtest=no \
	--disable-dtrace \
	--with-version-pre= \
	--with-version-opt=kdos \
	--with-version-build="$_build" \
	--with-vendor-name=KDOS
make jdk-image

mkdir -p "$PKG$_jhome"
cp -a build/linux-x86_64-server-release/images/jdk/. "$PKG$_jhome/"

install -dm755 "$PKG/usr/share/man/man1"
mv "$PKG$_jhome"/man/man1/*.1 "$PKG/usr/share/man/man1/"
rm -rf "$PKG$_jhome/man"

ln -s "java-$_feature-openjdk" "$PKG/usr/lib/jvm/default-jvm"
install -dm755 "$PKG/usr/bin"
for f in "$PKG$_jhome"/bin/*; do
	ln -s "../lib/jvm/java-$_feature-openjdk/bin/${f##*/}" "$PKG/usr/bin/${f##*/}"
done
