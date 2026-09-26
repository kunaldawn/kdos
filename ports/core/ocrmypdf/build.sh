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
# --no-deps, AND THE PART OF THE CLOSURE NO OTHER PROGRAM IMPORTS NAMED ONE BY
# ONE. The compiled dependencies — pillow, pillow-heif, pikepdf, lxml,
# cryptography, pypdfium2, pydantic-core, uharfbuzz — and pluggy, packaging,
# pygments, fonttools, rich, typing-extensions and charset-normalizer are ports
# in `depends`. What is left is pure python that exists only to be imported:
# pydantic and its typing helpers, fpdf2, img2pdf and pdfminer.six. A resolving
# install would leave out whichever of those another package had already put in
# the build root, so what this package owns would follow build order. pydantic
# is pinned in `pypackages` to the release that names python3-pydantic-core's
# exact version.
mkdir -p vendor
tar -xf $PORT_SRC/$name-vendor-$version.tar.xz --strip-components=1 -C vendor
#
# OCCULTA IS BUILT HERE, NOT SHIPPED. The glyphless font the text layer is set
# in comes from upstream's own generator, run with the fontTools port; the
# prebuilt copy is removed first so a generator that fails cannot leave it in
# place. The generator stamps the font with time.time(), which is pinned to
# SOURCE_DATE_EPOCH or the package differs on every build. Each codepoint's
# advance comes from this python's unicodedata, so a python3 bump that moves
# the Unicode version changes the font. sRGB.icc stays as upstream ships it:
# the sdist carries no generator for it.
rm -f src/ocrmypdf/data/Occulta.ttf
python3 - <<'PY'
import os, sys, time
epoch = float(os.environ["SOURCE_DATE_EPOCH"])
time.time = lambda: epoch
sys.path.insert(0, "scripts")
import generate_glyphless_font
generate_glyphless_font.main()
PY
test -s src/ocrmypdf/data/Occulta.ttf
#
# BUILD ISOLATION STAYS ON, POINTED AT THE BUNDLE. Those pure-python packages
# build with backends the bundle carries beside them, so no port has to be
# installed for their sake; --no-index with --find-links is what makes an
# isolated environment offline.
pip3 install --no-deps --no-index --find-links=vendor --root=$PKG --prefix=/usr \
	annotated-types defusedxml fpdf2 img2pdf pdfminer.six pydantic typing-inspection .
