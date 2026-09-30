# ██╗  ██╗██████╗  ██████╗ ███████╗
# ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
# █████╔╝ ██║  ██║██║   ██║███████╗
# ██╔═██╗ ██║  ██║██║   ██║╚════██║
# ██║  ██╗██████╔╝╚██████╔╝███████║
# ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
# ---------------------------------
#   KD's Homebrew Linux Distro
# ---------------------------------

# --no-build-isolation because every build dependency this needs is an
# installed port; pip's isolated environment would try to fetch them from PyPI
# and a build with no network fails there rather than at the compiler.
#
# Every optional codec is named: `enable` makes a missing header fail the
# build instead of producing a Pillow that silently lacks the format, and the
# probe would otherwise pick up whatever happens to be installed first. xcb
# stays off: it links the X client libraries for ImageGrab, and there is no X
# server to grab from.
pip3 install --no-deps --no-index --no-build-isolation --root=$PKG --prefix=/usr \
	-C tiff=enable \
	-C freetype=enable \
	-C raqm=enable \
	-C lcms=enable \
	-C webp=enable \
	-C jpeg2000=enable \
	-C imagequant=enable \
	-C avif=enable \
	-C xcb=disable \
	.
