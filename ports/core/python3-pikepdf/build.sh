# ██╗  ██╗██████╗  ██████╗ ███████╗
# ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
# █████╔╝ ██║  ██║██║   ██║███████╗
# ██╔═██╗ ██║  ██║██║   ██║╚════██║
# ██║  ██╗██████╔╝╚██████╔╝███████║
# ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
# ---------------------------------
#   KD's Homebrew Linux Distro
# ---------------------------------

# 9.9.0, AND BOTH HALVES OF THAT PIN ARE LOAD-BEARING. pikepdf 10 moved to
# scikit-build-core and nanobind, whose metadata asks for the PyPI `cmake` and
# `ninja` PACKAGES — CMake's own source tree, compiled from scratch — whatever
# is on PATH. And 9.10 onward requires pybind11 3, using py::smart_holder,
# while ports/core/python3-pybind11 is 2.13.6 and matplotlib builds against it.
# 9.9.0 is the last release that asks for 2.13.6, and ocrmypdf wants >= 8.10.1.
#
# --no-build-isolation because both backends are installed ports; pip's
# isolated environment would fetch them over a network this build lacks.
pip3 install --no-deps --no-index --no-build-isolation --root=$PKG --prefix=/usr .
