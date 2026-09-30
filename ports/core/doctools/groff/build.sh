# ██╗  ██╗██████╗  ██████╗ ███████╗
# ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
# █████╔╝ ██║  ██║██║   ██║███████╗
# ██╔═██╗ ██║  ██║██║   ██║╚════██║
# ██║  ██╗██████╔╝╚██████╔╝███████║
# ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
# ---------------------------------
#   KD's Homebrew Linux Distro
# ---------------------------------


patch -p1 -i "$PORT_SRC/fix-neqn-wrapper.patch"

# Every probe is answered here rather than by what the chroot holds: uchardet
# is preconv's encoding guesser, and ghostscript is what grohtml renders
# equations and pictures with and what gropdf's font descriptions are built
# against. The URW font search stays off: it takes ghostscript's Resource/Font
# directory as the URW set, finds no AFM metrics there, and the install then
# stops on U-* font descriptions that were never generated. gropdf uses the
# base-14 PDF fonts instead. The X11 previewer gxditview is not built, and the
# compatibility wrappers exist for a system that also carries a vendor troff.
./configure --prefix=/usr --sysconfdir=/etc --libdir=/usr/lib \
	--disable-rpath \
	--without-x \
	--with-uchardet=yes \
	--with-gs=gs \
	--without-urw-fonts \
	--with-compatibility-wrappers=no
make
make DESTDIR=$PKG install

# mandoc owns soelim and roff(7). groff's soelim reads the same .so requests,
# so the man pipeline is unchanged, and two packages cannot own one path.
rm -f "$PKG/usr/bin/soelim" \
	"$PKG/usr/share/man/man1/soelim.1" \
	"$PKG/usr/share/man/man7/roff.7"
rm -f "$PKG/usr/lib/charset.alias"
