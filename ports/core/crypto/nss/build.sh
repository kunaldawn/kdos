# ██╗  ██╗██████╗  ██████╗ ███████╗
# ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
# █████╔╝ ██║  ██║██║   ██║███████╗
# ██╔═██╗ ██║  ██║██║   ██║╚════██║
# ██║  ██╗██████╔╝╚██████╔╝███████║
# ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
# ---------------------------------
#   KD's Homebrew Linux Distro
# ---------------------------------


# Upstream's make build writes neither nss.pc nor nss-config; the patch adds a
# config/ step that generates both into dist/, and every consumer's configure
# looks for one of them.
patch -p1 -i "$PORT_SRC/nss-standalone.patch"

cd nss
# NSS_DISABLE_GTESTS: the gtest suites double the build and nothing runs them.
# NSS_ENABLE_WERROR=0: a newer compiler's new warning would otherwise stop the
# build. USE_64=1 selects the x86_64 freebl; without it the 32-bit one is built.
make BUILD_OPT=1 USE_64=1 \
	NSPR_INCLUDE_DIR=/usr/include/nspr \
	USE_SYSTEM_ZLIB=1 ZLIB_LIBS=-lz \
	NSS_USE_SYSTEM_SQLITE=1 \
	NSS_ENABLE_WERROR=0 \
	NSS_DISABLE_GTESTS=1 \
	XCFLAGS="$CFLAGS"

cd ../dist
install -d "$PKG/usr/lib/pkgconfig" "$PKG/usr/bin" "$PKG/usr/include/nss"
install -m755 Linux*/lib/*.so "$PKG/usr/lib/"
# The .chk files are freebl's and softokn's self-test signatures; FIPS mode
# refuses to load a library whose .chk is missing.
install -m644 Linux*/lib/*.chk "$PKG/usr/lib/"
cp -RL public/nss/* private/nss/* "$PKG/usr/include/nss/"
chmod 644 "$PKG"/usr/include/nss/*
install -m755 Linux*/bin/nss-config "$PKG/usr/bin/"
install -m755 Linux*/bin/{certutil,cmsutil,crlutil,modutil,pk12util,signtool,signver,ssltap} \
	"$PKG/usr/bin/"
install -m644 Linux*/lib/pkgconfig/nss.pc "$PKG/usr/lib/pkgconfig/"
