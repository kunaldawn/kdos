# ██╗  ██╗██████╗  ██████╗ ███████╗
# ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
# █████╔╝ ██║  ██║██║   ██║███████╗
# ██╔═██╗ ██║  ██║██║   ██║╚════██║
# ██║  ██╗██████╔╝╚██████╔╝███████║
# ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
# ---------------------------------
#   KD's Homebrew Linux Distro
# ---------------------------------

export CFLAGS="$CFLAGS -flto=auto"
export AR=gcc-ar NM=gcc-nm RANLIB=gcc-ranlib
pip3 install --ignore-installed --no-deps --no-index --no-build-isolation --root=$PKG --prefix=/usr \
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
