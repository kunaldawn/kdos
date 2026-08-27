# ██╗  ██╗██████╗  ██████╗ ███████╗
# ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
# █████╔╝ ██║  ██║██║   ██║███████╗
# ██╔═██╗ ██║  ██║██║   ██║╚════██║
# ██║  ██╗██████╔╝╚██████╔╝███████║
# ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
# ---------------------------------
#   KD's Homebrew Linux Distro
# ---------------------------------

# AN ARCHIVE OF SCANS IS UNSEARCHABLE UNTIL SOMETHING PUTS TEXT IN IT.
# tesseract reads the page and ghostscript rewrites it; ocrmypdf is what makes
# the two into one command that leaves the original image alone and adds an
# invisible text layer over it, in place.
#
# NO --no-deps HERE, WHICH IS THE ONE PLACE IN THIS TREE THAT IS TRUE. The
# compiled dependencies — pillow, pikepdf, pi-heif, lxml, cryptography — are
# ports, already installed, and pip finds them; what is left is a handful of
# pure-python packages (deprecation, img2pdf, pdfminer.six, pluggy, rich and
# what rich pulls) that exist only to be imported. Six more recipes for six
# files of python would be six more things to bump.
mkdir -p vendor
tar -xf $PORT_SRC/$name-vendor-$version.tar.xz --strip-components=1 -C vendor
#
# BUILD ISOLATION STAYS ON, POINTED AT THE BUNDLE. Those pure-python packages
# build with hatchling and flit-core, which are not ports and are not worth
# being ones; --no-index with --find-links is what makes an isolated
# environment offline.
pip3 install --no-index --find-links=vendor --root=$PKG --prefix=/usr .
