# ██╗  ██╗██████╗  ██████╗ ███████╗
# ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
# █████╔╝ ██║  ██║██║   ██║███████╗
# ██╔═██╗ ██║  ██║██║   ██║╚════██║
# ██║  ██╗██████╔╝╚██████╔╝███████║
# ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
# ---------------------------------
#   KD's Homebrew Linux Distro
# ---------------------------------

# The sdist carries no libheif: setup.py compiles one extension, _pillow_heif,
# against the libheif port, which it finds through pkg-config. The extension
# refuses a libheif older than 1.23.4 with an #error, and upstream compiles it
# with -Werror, so a libheif header that grows a deprecation warning fails this
# build rather than degrading it.
#
# --no-build-isolation because every build dependency this needs is an
# installed port; pip's isolated environment would try to fetch them from PyPI
# and a build with no network fails there rather than at the compiler.
pip3 install --no-deps --no-index --no-build-isolation --root=$PKG --prefix=/usr .
