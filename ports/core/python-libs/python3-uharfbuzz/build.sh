# ██╗  ██╗██████╗  ██████╗ ███████╗
# ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
# █████╔╝ ██║  ██║██║   ██║███████╗
# ██╔═██╗ ██║  ██║██║   ██║╚════██║
# ██║  ██╗██████╔╝╚██████╔╝███████║
# ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
# ---------------------------------
#   KD's Homebrew Linux Distro
# ---------------------------------

# USE_SYSTEM_LIBS links the harfbuzz port rather than compiling the copy the
# sdist carries. It asks pkg-config for harfbuzz-subset and harfbuzz-raster,
# so the harfbuzz port must build both — its defaults do.
export USE_SYSTEM_LIBS=1
pip3 install --no-deps --no-index --no-build-isolation --root=$PKG --prefix=/usr .
