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
# tesseract reads the page; pypdfium2 rasterises it and fpdf2 with uharfbuzz
# writes the invisible text layer over the original image, in place, and
# ghostscript remains the fallback rasteriser and the PDF/A converter.
#
# NO --no-deps HERE. The compiled dependencies — pillow, pikepdf, lxml,
# cryptography, pypdfium2, pydantic-core, uharfbuzz — and pluggy, packaging,
# pygments and fonttools are ports, already installed, and pip finds them;
# what is left is pure python that exists only to be imported (pydantic and
# its typing helpers, fpdf2, img2pdf, pdfminer.six, rich and what those pull).
# pydantic is pinned in `pypackages` to the release that names
# python3-pydantic-core's exact version; a newer one fails to resolve here.
mkdir -p vendor
tar -xf $PORT_SRC/$name-vendor-$version.tar.xz --strip-components=1 -C vendor
#
# BUILD ISOLATION STAYS ON, POINTED AT THE BUNDLE. Those pure-python packages
# build with backends the bundle carries beside them, so no port has to be
# installed for their sake; --no-index with --find-links is what makes an
# isolated environment offline.
pip3 install --no-index --find-links=vendor --root=$PKG --prefix=/usr .
