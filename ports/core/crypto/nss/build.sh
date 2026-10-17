# ██╗  ██╗██████╗  ██████╗ ███████╗
# ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
# █████╔╝ ██║  ██║██║   ██║███████╗
# ██╔═██╗ ██║  ██║██║   ██║╚════██║
# ██║  ██╗██████╔╝╚██████╔╝███████║
# ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
# ---------------------------------
#   KD's Homebrew Linux Distro
# ---------------------------------

patch -p1 -i "$PORT_SRC/nss-standalone.patch"

cd nss
make BUILD_OPT=1 USE_64=1 \
	NSPR_INCLUDE_DIR=/usr/include/nspr \
	USE_SYSTEM_ZLIB=1 ZLIB_LIBS=-lz \
	NSS_USE_SYSTEM_SQLITE=1 \
	NSS_ENABLE_WERROR=0 \
	NSS_DISABLE_GTESTS=1 \
	DSO_LDOPTS="-shared -m64 -Wl,--gc-sections -Wl,-z,defs $LDFLAGS" \
	XCFLAGS="$CFLAGS"

cd ../dist
install -d "$PKG/usr/lib/pkgconfig" "$PKG/usr/bin" "$PKG/usr/include/nss"
install -m755 Linux*/lib/*.so "$PKG/usr/lib/"
install -m644 Linux*/lib/*.chk "$PKG/usr/lib/"
cp -RL public/nss/* private/nss/* "$PKG/usr/include/nss/"
chmod 644 "$PKG"/usr/include/nss/*
install -m755 Linux*/bin/nss-config "$PKG/usr/bin/"
install -m755 Linux*/bin/{certutil,cmsutil,crlutil,modutil,pk12util,signtool,signver,ssltap} \
	"$PKG/usr/bin/"
install -m644 Linux*/lib/pkgconfig/nss.pc "$PKG/usr/lib/pkgconfig/"
rm -f "$PKG/usr/lib/libnssckbi.so" "$PKG/usr/lib/libnssckbi-testlib.so"
ln -sf pkcs11/p11-kit-trust.so "$PKG/usr/lib/libnssckbi.so"
