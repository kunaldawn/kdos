# ██╗  ██╗██████╗  ██████╗ ███████╗
# ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
# █████╔╝ ██║  ██║██║   ██║███████╗
# ██╔═██╗ ██║  ██║██║   ██║╚════██║
# ██║  ██╗██████╔╝╚██████╔╝███████║
# ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
# ---------------------------------
#   KD's Homebrew Linux Distro
# ---------------------------------

# A BUILD-TIME IMPORT THAT HAPPENS BEFORE ITS OWN FALLBACK. pandas ships
# `_version_meson.py` with the version already in it and generate_version.py
# prefers that — but it imports versioneer at MODULE scope, above the try, so
# an sdist build stops on the import rather than reaching the file that would
# have answered. Nothing here reads git; the module just has to exist.
pip3 install --no-deps --no-index --no-build-isolation --root=$PKG --prefix=/usr .
