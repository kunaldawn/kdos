# ██╗  ██╗██████╗  ██████╗ ███████╗
# ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
# █████╔╝ ██║  ██║██║   ██║███████╗
# ██╔═██╗ ██║  ██║██║   ██║╚════██║
# ██║  ██╗██████╔╝╚██████╔╝███████║
# ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
# ---------------------------------
#   KD's Homebrew Linux Distro
# ---------------------------------

mkdir -p vendor
tar -xf $PORT_SRC/$name-vendor-$version.tar.xz --strip-components=1 -C vendor
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
pip3 install --ignore-installed --no-deps --no-index --find-links=vendor --root=$PKG --prefix=/usr \
	annotated-types fpdf2 img2pdf pdfminer.six pydantic typing-inspection .
