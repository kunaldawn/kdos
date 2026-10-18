# ██╗  ██╗██████╗  ██████╗ ███████╗
# ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
# █████╔╝ ██║  ██║██║   ██║███████╗
# ██╔═██╗ ██║  ██║██║   ██║╚════██║
# ██║  ██╗██████╔╝╚██████╔╝███████║
# ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
# ---------------------------------
#   KD's Homebrew Linux Distro
# ---------------------------------

./configure --prefix=/usr \
	--enable-pic14-port --enable-pic16-port \
	--disable-doc
make
make DESTDIR=$PKG install
rm -f "$PKG"/usr/bin/c++filt "$PKG"/usr/share/info/ctf-spec.info "$PKG"/usr/share/info/sframe-spec.info "$PKG"/usr/share/info/dir \
	"$PKG"/usr/include/ctf-api.h "$PKG"/usr/include/ctf.h "$PKG"/usr/include/dis-asm.h \
	"$PKG"/usr/include/sframe-api.h "$PKG"/usr/include/sframe.h \
	"$PKG"/usr/lib/libctf.a "$PKG"/usr/lib/libctf-nobfd.a "$PKG"/usr/lib/libopcodes.a "$PKG"/usr/lib/libsframe.a
rmdir --ignore-fail-on-non-empty "$PKG"/usr/share/info "$PKG"/usr/include
test -d "$PKG"/usr/share/sdcc/lib/pic14
test -d "$PKG"/usr/share/sdcc/lib/pic16
