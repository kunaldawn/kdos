# ██╗  ██╗██████╗  ██████╗ ███████╗
# ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
# █████╔╝ ██║  ██║██║   ██║███████╗
# ██╔═██╗ ██║  ██║██║   ██║╚════██║
# ██║  ██╗██████╔╝╚██████╔╝███████║
# ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
# ---------------------------------
#   KD's Homebrew Linux Distro
# ---------------------------------

./configure --prefix=/usr --libdir=/usr/lib \
	--enable-shared --disable-static --enable-libpng \
	--with-dsoflags="$LDFLAGS"
make OPTIM=
make OPTIM= DESTDIR=$PKG install

rm -f "$PKG"/usr/share/doc/pdfio/examples/*.ttf \
	"$PKG"/usr/share/doc/pdfio/examples/*-LICENSE.txt
