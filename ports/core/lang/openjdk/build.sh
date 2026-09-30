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


# The seed is Adoptium's musl build of the same feature release, used as the
# boot JDK and then discarded: nothing it built is in $PKG. OpenJDK accepts a
# boot JDK of this feature release or the one before it
# (DEFAULT_ACCEPTABLE_BOOT_VERSIONS), so bumping $version past a new feature
# release bumps the seed with it. Its NEEDED entry is Alpine's
# libc.musl-x86_64.so.1, which musl's loader resolves to itself.
_seedjdk="$SRC_ROOT/jdk-$_seed+$_seedbuild"
_jhome=/usr/lib/jvm/java-$_feature-openjdk

# configure ignores CFLAGS, CXXFLAGS and LDFLAGS from the environment; the
# phase's flags go in through the --with-extra-* switches instead, which keeps
# the -ffile-prefix-map and build-id settings. _LARGEFILE64_SOURCE: musl
# declares the *64 file functions the class library calls only under it.
_cflags="$CFLAGS -D_LARGEFILE64_SOURCE"
_cxxflags="$CXXFLAGS -D_LARGEFILE64_SOURCE"
_ldflags="$LDFLAGS"
unset CFLAGS CXXFLAGS LDFLAGS

# The build takes its parallelism from --with-jobs and refuses a -j in
# MAKEFLAGS.
unset MAKEFLAGS

# Every image library is the system's, including FreeType and HarfBuzz. AWT
# is built for X11 and runs under Xwayland; CUPS, ALSA and fontconfig are
# found in /usr, and configure refuses a build without any of them. The build
# reads SOURCE_DATE_EPOCH, so the class files, jmods and version strings carry
# the tree's pinned date. pandoc renders the manual pages from their Markdown
# sources; without it the image has none.
bash ./configure \
	--prefix="$_jhome" \
	--with-boot-jdk="$_seedjdk" \
	--with-extra-cflags="$_cflags" \
	--with-extra-cxxflags="$_cxxflags" \
	--with-extra-ldflags="$_ldflags" \
	--with-jobs="$(nproc)" \
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

# The manual pages move to where man looks; the image's copy is the only one.
install -dm755 "$PKG/usr/share/man/man1"
mv "$PKG$_jhome"/man/man1/*.1 "$PKG/usr/share/man/man1/"
rm -rf "$PKG$_jhome/man"

# /usr/lib/jvm/default-jvm is the JAVA_HOME consumers name, and every
# launcher is on PATH through /usr/bin.
ln -s "java-$_feature-openjdk" "$PKG/usr/lib/jvm/default-jvm"
install -dm755 "$PKG/usr/bin"
for f in "$PKG$_jhome"/bin/*; do
	ln -s "../lib/jvm/java-$_feature-openjdk/bin/${f##*/}" "$PKG/usr/bin/${f##*/}"
done
